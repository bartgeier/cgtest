/* clexer.c - see clexer.h */
#include "clexer.h"

#include <stdlib.h>
#include <string.h>

#define CLEXER_TEST_PREFIX "test_"

/* ------------------------------------------------------------------ */
/* Character stream, with transparent phase-2 line splicing.           */
/* ------------------------------------------------------------------ */

/* Length, in bytes, of a backslash-newline (or backslash-CR-LF) splice
 * starting exactly at "pos", or 0 if there is none there. */
static size_t clexer_splice_len(const CLexer *lexer, size_t pos)
{
    if (pos + 1 < lexer->length && lexer->src[pos] == '\\') {
        if (lexer->src[pos + 1] == '\n') {
            return 2;
        }
        if (pos + 2 < lexer->length && lexer->src[pos + 1] == '\r' && lexer->src[pos + 2] == '\n') {
            return 3;
        }
    }
    return 0;
}

/* Advances "pos" past any run of consecutive splices starting there. */
static size_t clexer_skip_splices(const CLexer *lexer, size_t pos)
{
    size_t splice;
    while ((splice = clexer_splice_len(lexer, pos)) != 0) {
        pos += splice;
    }
    return pos;
}

static int clexer_current(const CLexer *lexer)
{
    size_t pos = clexer_skip_splices(lexer, lexer->pos);
    if (pos >= lexer->length) {
        return -1;
    }
    return (unsigned char)lexer->src[pos];
}

/* Logical lookahead: the character "n" positions after the current one,
 * with splices transparently skipped both before and between them. */
static int clexer_peek(const CLexer *lexer, size_t n)
{
    size_t pos = clexer_skip_splices(lexer, lexer->pos);
    size_t i;

    for (i = 0; i < n; i++) {
        if (pos >= lexer->length) {
            return -1;
        }
        pos = clexer_skip_splices(lexer, pos + 1);
    }

    if (pos >= lexer->length) {
        return -1;
    }
    return (unsigned char)lexer->src[pos];
}

/* Consumes exactly one logical character (skipping any splice that
 * precedes it), keeping lexer->pos always at a spliced position.
 *
 * Each spliced backslash-newline still corresponds to one physical
 * source line, so it must bump lexer->line too - otherwise every line
 * number reported after a backslash-continued line (e.g. inside a
 * multi-line #define) would undercount by the number of splices. */
static void clexer_advance(CLexer *lexer)
{
    size_t pos = lexer->pos;
    size_t splice;

    while ((splice = clexer_splice_len(lexer, pos)) != 0) {
        pos += splice;
        lexer->line++;
    }

    if (pos >= lexer->length) {
        lexer->pos = lexer->length;
        return;
    }
    if (lexer->src[pos] == '\n') {
        lexer->line++;
    }
    lexer->pos = pos + 1;
}

void clexer_init(CLexer *lexer, const char *source, size_t length)
{
    lexer->src = source;
    lexer->length = length;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->started = 0;
}

/* ------------------------------------------------------------------ */
/* Trivia: whitespace and both comment styles.                         */
/* ------------------------------------------------------------------ */

/* Skips whitespace and comments. Reports whether anything was skipped
 * (leading_space) and whether a real newline was crossed outside of any
 * comment (had_newline) - a newline hidden inside a slash-star comment
 * does NOT count, matching the standard's comment-first-then-directives
 * translation order. */
static int clexer_skip_trivia(CLexer *lexer, int *out_had_newline)
{
    int had_space = 0;
    int had_newline = 0;

    for (;;) {
        int c = clexer_current(lexer);

        if (c < 0) {
            break;
        }

        if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
            clexer_advance(lexer);
            had_space = 1;
            continue;
        }

        if (c == '\n') {
            clexer_advance(lexer);
            had_space = 1;
            had_newline = 1;
            continue;
        }

        if (c == '/' && clexer_peek(lexer, 1) == '/') {
            while (clexer_current(lexer) >= 0 && clexer_current(lexer) != '\n') {
                clexer_advance(lexer);
            }
            had_space = 1;
            continue;
        }

        if (c == '/' && clexer_peek(lexer, 1) == '*') {
            clexer_advance(lexer);
            clexer_advance(lexer);
            while (clexer_current(lexer) >= 0 &&
                   !(clexer_current(lexer) == '*' && clexer_peek(lexer, 1) == '/')) {
                clexer_advance(lexer);
            }
            if (clexer_current(lexer) >= 0) {
                clexer_advance(lexer); /* '*' */
                clexer_advance(lexer); /* '/' */
            }
            had_space = 1;
            continue;
        }

        break;
    }

    if (out_had_newline != NULL) {
        *out_had_newline = had_newline;
    }
    return had_space;
}

/* ------------------------------------------------------------------ */
/* Identifiers, including universal-character-names and (permissively)  */
/* raw extended (UTF-8) identifier bytes.                                */
/* ------------------------------------------------------------------ */

static int clexer_is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int clexer_is_hex_digit(int c)
{
    return clexer_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int clexer_is_ascii_ident_start(int c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int clexer_is_ascii_ident_continue(int c)
{
    return clexer_is_ascii_ident_start(c) || clexer_is_digit(c);
}

/* C23 permits identifier characters outside the basic character set
 * (6.4.2, Annex D). Rather than encoding the exact Unicode ranges, any
 * UTF-8 lead/continuation byte is accepted; see clexer.h's scope notes. */
static int clexer_is_extended_ident_byte(int c)
{
    return c >= 0x80 && c <= 0xff;
}

/* Length, in source characters, of a valid universal-character-name
 * (\uXXXX or \UXXXXXXXX) starting at the current position, or 0. */
static size_t clexer_ucn_len(const CLexer *lexer)
{
    int second;
    size_t hex_count;
    size_t i;

    if (clexer_current(lexer) != '\\') {
        return 0;
    }
    second = clexer_peek(lexer, 1);
    if (second == 'u') {
        hex_count = 4;
    } else if (second == 'U') {
        hex_count = 8;
    } else {
        return 0;
    }
    for (i = 0; i < hex_count; i++) {
        if (!clexer_is_hex_digit(clexer_peek(lexer, 2 + i))) {
            return 0;
        }
    }
    return 2 + hex_count;
}

static int clexer_is_ident_start_here(const CLexer *lexer)
{
    int c = clexer_current(lexer);
    return clexer_is_ascii_ident_start(c) ||
           clexer_is_extended_ident_byte(c) ||
           clexer_ucn_len(lexer) > 0;
}

static void clexer_scan_identifier(CLexer *lexer)
{
    for (;;) {
        int c = clexer_current(lexer);
        size_t ucn;

        if (clexer_is_ascii_ident_continue(c) || clexer_is_extended_ident_byte(c)) {
            clexer_advance(lexer);
            continue;
        }
        ucn = clexer_ucn_len(lexer);
        if (ucn > 0) {
            size_t i;
            for (i = 0; i < ucn; i++) {
                clexer_advance(lexer);
            }
            continue;
        }
        break;
    }
}

typedef struct {
    const char *text;
    CKeyword    keyword;
} CKeywordEntry;

static const CKeywordEntry clexer_keywords[] = {
    /* C89/C90 */
    { "auto", CKW_AUTO }, { "break", CKW_BREAK }, { "case", CKW_CASE },
    { "char", CKW_CHAR }, { "const", CKW_CONST }, { "continue", CKW_CONTINUE },
    { "default", CKW_DEFAULT }, { "do", CKW_DO }, { "double", CKW_DOUBLE },
    { "else", CKW_ELSE }, { "enum", CKW_ENUM }, { "extern", CKW_EXTERN },
    { "float", CKW_FLOAT }, { "for", CKW_FOR }, { "goto", CKW_GOTO },
    { "if", CKW_IF }, { "int", CKW_INT }, { "long", CKW_LONG },
    { "register", CKW_REGISTER }, { "return", CKW_RETURN }, { "short", CKW_SHORT },
    { "signed", CKW_SIGNED }, { "sizeof", CKW_SIZEOF }, { "static", CKW_STATIC },
    { "struct", CKW_STRUCT }, { "switch", CKW_SWITCH }, { "typedef", CKW_TYPEDEF },
    { "union", CKW_UNION }, { "unsigned", CKW_UNSIGNED }, { "void", CKW_VOID },
    { "volatile", CKW_VOLATILE }, { "while", CKW_WHILE },
    /* C99 */
    { "inline", CKW_INLINE }, { "restrict", CKW_RESTRICT },
    { "_Bool", CKW_U_BOOL }, { "_Complex", CKW_U_COMPLEX }, { "_Imaginary", CKW_U_IMAGINARY },
    /* C11 */
    { "_Alignas", CKW_U_ALIGNAS }, { "_Alignof", CKW_U_ALIGNOF },
    { "_Atomic", CKW_U_ATOMIC }, { "_Generic", CKW_U_GENERIC },
    { "_Noreturn", CKW_U_NORETURN }, { "_Static_assert", CKW_U_STATIC_ASSERT },
    { "_Thread_local", CKW_U_THREAD_LOCAL },
    /* C23 */
    { "alignas", CKW_ALIGNAS }, { "alignof", CKW_ALIGNOF }, { "bool", CKW_BOOL },
    { "constexpr", CKW_CONSTEXPR }, { "false", CKW_FALSE }, { "nullptr", CKW_NULLPTR },
    { "static_assert", CKW_STATIC_ASSERT }, { "thread_local", CKW_THREAD_LOCAL },
    { "true", CKW_TRUE }, { "typeof", CKW_TYPEOF }, { "typeof_unqual", CKW_TYPEOF_UNQUAL },
    { "_BitInt", CKW_U_BITINT }, { "_Decimal32", CKW_U_DECIMAL32 },
    { "_Decimal64", CKW_U_DECIMAL64 }, { "_Decimal128", CKW_U_DECIMAL128 },
    { NULL, CKW_NONE }
};

static CKeyword clexer_lookup_keyword(const char *start, size_t length)
{
    size_t i;
    for (i = 0; clexer_keywords[i].text != NULL; i++) {
        size_t klen = strlen(clexer_keywords[i].text);
        if (klen == length && memcmp(clexer_keywords[i].text, start, length) == 0) {
            return clexer_keywords[i].keyword;
        }
    }
    return CKW_NONE;
}

const char *clexer_keyword_name(CKeyword keyword)
{
    size_t i;
    for (i = 0; clexer_keywords[i].text != NULL; i++) {
        if (clexer_keywords[i].keyword == keyword) {
            return clexer_keywords[i].text;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* pp-number (C23 6.4.8) - a superset of integer/floating constants,    */
/* deliberately permissive; semantic validation is not this layer's job.*/
/* ------------------------------------------------------------------ */

static void clexer_scan_pp_number(CLexer *lexer)
{
    clexer_advance(lexer); /* the leading digit, or '.' followed by a digit */

    for (;;) {
        int c = clexer_current(lexer);
        size_t ucn;

        if (clexer_is_digit(c) || c == '.') {
            clexer_advance(lexer);
            continue;
        }

        if (c == 'e' || c == 'E' || c == 'p' || c == 'P') {
            int next = clexer_peek(lexer, 1);
            if (next == '+' || next == '-') {
                clexer_advance(lexer);
                clexer_advance(lexer);
                continue;
            }
            /* no sign: 'e'/'p' itself still qualifies below as identifier-nondigit */
        }

        if (c == '\'') {
            /* C23 digit separator: pp-number ' digit | pp-number ' nondigit */
            int next = clexer_peek(lexer, 1);
            if (clexer_is_digit(next) || clexer_is_ascii_ident_start(next)) {
                clexer_advance(lexer);
                clexer_advance(lexer);
                continue;
            }
            break;
        }

        if (clexer_is_ascii_ident_start(c) || clexer_is_extended_ident_byte(c)) {
            clexer_advance(lexer);
            continue;
        }

        ucn = clexer_ucn_len(lexer);
        if (ucn > 0) {
            size_t i;
            for (i = 0; i < ucn; i++) {
                clexer_advance(lexer);
            }
            continue;
        }

        break;
    }
}

/* ------------------------------------------------------------------ */
/* Character constants and string literals (with encoding prefixes).    */
/* ------------------------------------------------------------------ */

static void clexer_scan_quoted(CLexer *lexer, int quote)
{
    clexer_advance(lexer); /* opening quote */
    while (clexer_current(lexer) >= 0 && clexer_current(lexer) != quote && clexer_current(lexer) != '\n') {
        if (clexer_current(lexer) == '\\' && clexer_peek(lexer, 1) >= 0) {
            clexer_advance(lexer);
        }
        clexer_advance(lexer);
    }
    if (clexer_current(lexer) == quote) {
        clexer_advance(lexer); /* closing quote */
    }
}

/* Does "text" (of length "len") appear at the current position, directly
 * followed by a quote character? Used to recognize u8/u/U/L literal
 * prefixes without misclassifying identifiers like "U8Value". */
static int clexer_prefix_matches(const CLexer *lexer, const char *text, size_t len)
{
    size_t i;
    int after;

    for (i = 0; i < len; i++) {
        if (clexer_peek(lexer, i) != (unsigned char)text[i]) {
            return 0;
        }
    }
    after = clexer_peek(lexer, len);
    return after == '"' || after == '\'';
}

/* ------------------------------------------------------------------ */
/* Punctuators, including digraphs and the C23 "::" addition.           */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *spelling;
    CPunct      canonical;
    int         is_digraph;
} CPunctSpelling;

/* Ordered longest-spelling-first so the first match is the maximal munch
 * the standard requires (6.4p4). */
static const CPunctSpelling clexer_punct_table[] = {
    { "%:%:", CPUNCT_HASHHASH,   1 },
    { "...",  CPUNCT_ELLIPSIS,   0 },
    { "<<=",  CPUNCT_SHL_ASSIGN, 0 },
    { ">>=",  CPUNCT_SHR_ASSIGN, 0 },
    { "->",   CPUNCT_ARROW,      0 },
    { "++",   CPUNCT_PLUSPLUS,   0 },
    { "--",   CPUNCT_MINUSMINUS, 0 },
    { "<<",   CPUNCT_SHL,        0 },
    { ">>",   CPUNCT_SHR,        0 },
    { "<=",   CPUNCT_LE,         0 },
    { ">=",   CPUNCT_GE,         0 },
    { "==",   CPUNCT_EQ,         0 },
    { "!=",   CPUNCT_NE,         0 },
    { "&&",   CPUNCT_AMPAMP,     0 },
    { "||",   CPUNCT_PIPEPIPE,   0 },
    { "*=",   CPUNCT_STAR_ASSIGN,    0 },
    { "/=",   CPUNCT_SLASH_ASSIGN,   0 },
    { "%=",   CPUNCT_PERCENT_ASSIGN, 0 },
    { "+=",   CPUNCT_PLUS_ASSIGN,    0 },
    { "-=",   CPUNCT_MINUS_ASSIGN,   0 },
    { "&=",   CPUNCT_AMP_ASSIGN,     0 },
    { "^=",   CPUNCT_CARET_ASSIGN,   0 },
    { "|=",   CPUNCT_PIPE_ASSIGN,    0 },
    { "::",   CPUNCT_COLONCOLON, 0 },
    { "<:",   CPUNCT_LBRACKET,   1 },
    { ":>",   CPUNCT_RBRACKET,   1 },
    { "<%",   CPUNCT_LBRACE,     1 },
    { "%>",   CPUNCT_RBRACE,     1 },
    { "%:",   CPUNCT_HASH,       1 },
    { "##",   CPUNCT_HASHHASH,   0 },
    { "[",    CPUNCT_LBRACKET,   0 },
    { "]",    CPUNCT_RBRACKET,   0 },
    { "(",    CPUNCT_LPAREN,     0 },
    { ")",    CPUNCT_RPAREN,     0 },
    { "{",    CPUNCT_LBRACE,     0 },
    { "}",    CPUNCT_RBRACE,     0 },
    { ".",    CPUNCT_DOT,        0 },
    { "&",    CPUNCT_AMP,        0 },
    { "*",    CPUNCT_STAR,       0 },
    { "+",    CPUNCT_PLUS,       0 },
    { "-",    CPUNCT_MINUS,      0 },
    { "~",    CPUNCT_TILDE,      0 },
    { "!",    CPUNCT_BANG,       0 },
    { "/",    CPUNCT_SLASH,      0 },
    { "%",    CPUNCT_PERCENT,    0 },
    { "<",    CPUNCT_LT,         0 },
    { ">",    CPUNCT_GT,         0 },
    { "^",    CPUNCT_CARET,      0 },
    { "|",    CPUNCT_PIPE,       0 },
    { "?",    CPUNCT_QUESTION,   0 },
    { ":",    CPUNCT_COLON,      0 },
    { ";",    CPUNCT_SEMI,       0 },
    { "=",    CPUNCT_ASSIGN,     0 },
    { ",",    CPUNCT_COMMA,      0 },
    { "#",    CPUNCT_HASH,       0 },
    { NULL,   CPUNCT_NONE,       0 }
};

static const CPunctSpelling *clexer_match_punct(const CLexer *lexer)
{
    size_t i;
    for (i = 0; clexer_punct_table[i].spelling != NULL; i++) {
        size_t len = strlen(clexer_punct_table[i].spelling);
        size_t j;
        int matched = 1;

        for (j = 0; j < len; j++) {
            if (clexer_peek(lexer, j) != (unsigned char)clexer_punct_table[i].spelling[j]) {
                matched = 0;
                break;
            }
        }
        if (matched) {
            return &clexer_punct_table[i];
        }
    }
    return NULL;
}

const char *clexer_punct_name(CPunct punct)
{
    size_t i;
    for (i = 0; clexer_punct_table[i].spelling != NULL; i++) {
        if (clexer_punct_table[i].canonical == punct && !clexer_punct_table[i].is_digraph) {
            return clexer_punct_table[i].spelling;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Token dispatch.                                                      */
/* ------------------------------------------------------------------ */

static CToken clexer_make_common(const CLexer *lexer, int leading_space, int at_line_start)
{
    CToken token;

    token.start = lexer->src + lexer->pos;
    token.line = lexer->line;
    token.leading_space = leading_space;
    token.at_line_start = at_line_start;
    token.keyword = CKW_NONE;
    token.encoding = CENC_NONE;
    token.punct = CPUNCT_NONE;
    token.is_digraph = 0;
    token.type = CTOK_EOF;
    token.length = 0;
    return token;
}

static CToken clexer_lex_token(CLexer *lexer, int leading_space, int at_line_start)
{
    CToken token = clexer_make_common(lexer, leading_space, at_line_start);
    int c = clexer_current(lexer);

    if (c < 0) {
        return token;
    }

    {
        CEncodingPrefix enc = CENC_NONE;
        size_t plen = 0;

        if (clexer_prefix_matches(lexer, "u8", 2)) {
            enc = CENC_U8;
            plen = 2;
        } else if (clexer_prefix_matches(lexer, "u", 1)) {
            enc = CENC_u;
            plen = 1;
        } else if (clexer_prefix_matches(lexer, "U", 1)) {
            enc = CENC_U;
            plen = 1;
        } else if (clexer_prefix_matches(lexer, "L", 1)) {
            enc = CENC_L;
            plen = 1;
        }

        if (plen > 0) {
            size_t i;
            int quote;

            for (i = 0; i < plen; i++) {
                clexer_advance(lexer);
            }
            quote = clexer_current(lexer);
            clexer_scan_quoted(lexer, quote);
            token.type = (quote == '\'') ? CTOK_CHAR_CONST : CTOK_STRING_LITERAL;
            token.encoding = enc;
            token.length = (size_t)((lexer->src + lexer->pos) - token.start);
            return token;
        }
    }

    if (c == '"') {
        clexer_scan_quoted(lexer, '"');
        token.type = CTOK_STRING_LITERAL;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        return token;
    }

    if (c == '\'') {
        clexer_scan_quoted(lexer, '\'');
        token.type = CTOK_CHAR_CONST;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        return token;
    }

    if (clexer_is_ident_start_here(lexer)) {
        clexer_scan_identifier(lexer);
        token.type = CTOK_IDENTIFIER;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        token.keyword = clexer_lookup_keyword(token.start, token.length);
        return token;
    }

    if (clexer_is_digit(c) || (c == '.' && clexer_is_digit(clexer_peek(lexer, 1)))) {
        clexer_scan_pp_number(lexer);
        token.type = CTOK_PP_NUMBER;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        return token;
    }

    {
        const CPunctSpelling *match = clexer_match_punct(lexer);
        if (match != NULL) {
            size_t spelling_len = strlen(match->spelling);
            size_t i;

            for (i = 0; i < spelling_len; i++) {
                clexer_advance(lexer);
            }
            token.type = CTOK_PUNCT;
            token.punct = match->canonical;
            token.is_digraph = match->is_digraph;
            token.length = spelling_len;
            return token;
        }
    }

    clexer_advance(lexer);
    token.type = CTOK_OTHER;
    token.length = 1;
    return token;
}

CToken clexer_next_token(CLexer *lexer)
{
    int leading_space;
    int had_newline;
    int at_line_start;
    CToken token;

    leading_space = clexer_skip_trivia(lexer, &had_newline);
    lexer->pos = clexer_skip_splices(lexer, lexer->pos);
    at_line_start = had_newline || !lexer->started;

    token = clexer_lex_token(lexer, leading_space, at_line_start);
    lexer->started = 1;
    return token;
}

CToken clexer_next_header_name(CLexer *lexer)
{
    int leading_space;
    int had_newline;
    int at_line_start;
    int c;

    leading_space = clexer_skip_trivia(lexer, &had_newline);
    lexer->pos = clexer_skip_splices(lexer, lexer->pos);
    at_line_start = had_newline || !lexer->started;
    c = clexer_current(lexer);

    /* A header-name must be part of the same logical line as whatever
     * put the caller in this context (e.g. "# include"); if skipping
     * trivia already crossed into a new line, there is no header-name
     * here - fall through to ordinary tokenization instead. */
    if (!at_line_start && (c == '<' || c == '"')) {
        CToken token = clexer_make_common(lexer, leading_space, at_line_start);
        int closing = (c == '<') ? '>' : '"';

        clexer_advance(lexer);
        while (clexer_current(lexer) >= 0 && clexer_current(lexer) != closing && clexer_current(lexer) != '\n') {
            clexer_advance(lexer);
        }
        if (clexer_current(lexer) == closing) {
            clexer_advance(lexer);
        }
        token.type = CTOK_HEADER_NAME;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        lexer->started = 1;
        return token;
    }

    {
        CToken token = clexer_lex_token(lexer, leading_space, at_line_start);
        lexer->started = 1;
        return token;
    }
}

int clexer_token_equals(const CToken *token, const char *text)
{
    size_t text_len = strlen(text);
    if (token->length != text_len) {
        return 0;
    }
    return memcmp(token->start, text, text_len) == 0;
}

int clexer_token_starts_with(const CToken *token, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    if (token->length < prefix_len) {
        return 0;
    }
    return memcmp(token->start, prefix, prefix_len) == 0;
}

/* ------------------------------------------------------------------ */
/* Test-function scanner, built on top of the tokenizer above.          */
/* ------------------------------------------------------------------ */

enum {
    ST_INIT = 0,
    ST_SAW_BOOL,
    ST_SAW_NAME,
    ST_SAW_LPAREN,
    ST_SAW_VOID
};

typedef struct {
    CTestFunction *items;
    size_t         count;
    size_t         capacity;
} CTestFunctionList;

static int clexer_list_push(CTestFunctionList *list, const CToken *name_token, int line)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        CTestFunction *grown = (CTestFunction *)realloc(list->items, new_capacity * sizeof(CTestFunction));
        if (grown == NULL) {
            return 0;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }

    list->items[list->count].name = (char *)malloc(name_token->length + 1);
    if (list->items[list->count].name == NULL) {
        return 0;
    }
    memcpy(list->items[list->count].name, name_token->start, name_token->length);
    list->items[list->count].name[name_token->length] = '\0';
    list->items[list->count].line = line;
    list->count++;
    return 1;
}

/* Advances past an entire preprocessing directive line: "token" must
 * already be a '#' that is the first token on its line; returns the
 * next token that itself starts a line (or CTOK_EOF). */
static CToken clexer_skip_directive_line(CLexer *lexer)
{
    CToken token;
    do {
        token = clexer_next_token(lexer);
    } while (token.type != CTOK_EOF && !token.at_line_start);
    return token;
}

static int clexer_is_directive_hash(const CToken *token)
{
    return token->at_line_start && token->type == CTOK_PUNCT && token->punct == CPUNCT_HASH;
}

static int clexer_state_after_mismatch(const CToken *token)
{
    return (token->type == CTOK_IDENTIFIER && token->keyword == CKW_BOOL) ? ST_SAW_BOOL : ST_INIT;
}

CTestFunction *clexer_find_test_functions(const char *source, size_t length, size_t *out_count)
{
    CLexer lexer;
    CToken token;
    CToken name_token;
    int state = ST_INIT;
    int name_line = 0;
    CTestFunctionList list;

    list.items = NULL;
    list.count = 0;
    list.capacity = 0;

    clexer_init(&lexer, source, length);
    token = clexer_next_token(&lexer);

    while (token.type != CTOK_EOF) {
        int consumed_extra = 0;

        if (clexer_is_directive_hash(&token)) {
            token = clexer_skip_directive_line(&lexer);
            continue;
        }

        switch (state) {
        case ST_INIT:
            state = (token.type == CTOK_IDENTIFIER && token.keyword == CKW_BOOL) ? ST_SAW_BOOL : ST_INIT;
            break;

        case ST_SAW_BOOL:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_NONE &&
                clexer_token_starts_with(&token, CLEXER_TEST_PREFIX)) {
                name_token = token;
                name_line = token.line;
                state = ST_SAW_NAME;
            } else {
                state = clexer_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_NAME:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_LPAREN) {
                state = ST_SAW_LPAREN;
            } else {
                state = clexer_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_LPAREN:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_VOID) {
                state = ST_SAW_VOID;
            } else {
                state = clexer_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_VOID:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_RPAREN) {
                CToken next = clexer_next_token(&lexer);

                while (next.type != CTOK_EOF && clexer_is_directive_hash(&next)) {
                    next = clexer_skip_directive_line(&lexer);
                }

                if (next.type == CTOK_PUNCT && next.punct == CPUNCT_LBRACE) {
                    if (!clexer_list_push(&list, &name_token, name_line)) {
                        clexer_free_test_functions(list.items, list.count);
                        *out_count = 0;
                        return NULL;
                    }
                    state = ST_INIT;
                } else {
                    state = clexer_state_after_mismatch(&next);
                }

                token = next;
                consumed_extra = 1;
            } else {
                state = clexer_state_after_mismatch(&token);
            }
            break;

        default:
            state = ST_INIT;
            break;
        }

        if (!consumed_extra) {
            token = clexer_next_token(&lexer);
        }
    }

    *out_count = list.count;
    return list.items;
}

void clexer_free_test_functions(CTestFunction *functions, size_t count)
{
    size_t i;
    if (functions == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(functions[i].name);
    }
    free(functions);
}
