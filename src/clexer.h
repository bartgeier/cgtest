/* clexer.h - a C23-conformant lexer: translation phases 1-3 (line splicing,
 * comment/whitespace removal, tokenization into preprocessing-tokens).
 *
 * Scope notes (deliberate simplifications, documented rather than silent):
 *  - Trigraphs are NOT translated. They were deprecated in C99 and removed
 *    from the standard in C23, which is this lexer's target dialect.
 *  - Extended identifier characters (C23 6.4.2, Annex D) are accepted
 *    permissively: any byte >= 0x80 (i.e. any UTF-8 continuation/lead byte)
 *    is treated as identifier-nondigit, rather than validated against the
 *    exact Annex D Unicode code point ranges. Universal character names
 *    (\uXXXX, \UXXXXXXXX) ARE validated and consumed correctly.
 *  - header-name tokens are only produced by clexer_next_header_name(),
 *    since per the standard they are only meaningful in the context of a
 *    #include (or __has_include) directive - a context only the caller
 *    knows it is in.
 *  - Preprocessing directives (#include, #define, #if, ...) are tokenized
 *    like any other tokens (a '#' punctuator marks the start of a
 *    directive line via CToken::at_line_start); this lexer does NOT
 *    execute directives (no macro expansion, no conditional-compilation
 *    evaluation, no file inclusion). That is translation phase 4, a
 *    preprocessor built on top of this tokenizer, not part of it.
 */
#ifndef CLEXER_H
#define CLEXER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTOK_EOF = 0,
    CTOK_IDENTIFIER,      /* includes keywords; see CToken::keyword */
    CTOK_PP_NUMBER,       /* C23 6.4.8 pp-number (superset of int/float constants) */
    CTOK_CHAR_CONST,      /* 'x', u'x', U'x', L'x', u8'x' */
    CTOK_STRING_LITERAL,  /* "s", u"s", U"s", L"s", u8"s" */
    CTOK_HEADER_NAME,     /* <...> or "..." - only via clexer_next_header_name() */
    CTOK_PUNCT,
    CTOK_OTHER            /* any non-whitespace char matching no other category */
} CTokenType;

/* Encoding prefix on a character constant or string literal. */
typedef enum {
    CENC_NONE = 0,
    CENC_U8,  /* u8"..."  (u8'...' as of C23) */
    CENC_u,   /* u"..."   u'...'              */
    CENC_U,   /* U"..."   U'...'              */
    CENC_L    /* L"..."   L'...'              */
} CEncodingPrefix;

/* Every reserved keyword through C23. Underscore-prefixed spellings kept
 * for compatibility (_Bool, _Alignas, ...) get their own entries distinct
 * from the C23 convenience spellings (bool, alignas, ...) since both
 * remain valid, separate keywords in C23. */
typedef enum {
    CKW_NONE = 0,
    /* C89/C90 */
    CKW_AUTO, CKW_BREAK, CKW_CASE, CKW_CHAR, CKW_CONST, CKW_CONTINUE,
    CKW_DEFAULT, CKW_DO, CKW_DOUBLE, CKW_ELSE, CKW_ENUM, CKW_EXTERN,
    CKW_FLOAT, CKW_FOR, CKW_GOTO, CKW_IF, CKW_INT, CKW_LONG, CKW_REGISTER,
    CKW_RETURN, CKW_SHORT, CKW_SIGNED, CKW_SIZEOF, CKW_STATIC, CKW_STRUCT,
    CKW_SWITCH, CKW_TYPEDEF, CKW_UNION, CKW_UNSIGNED, CKW_VOID,
    CKW_VOLATILE, CKW_WHILE,
    /* C99 */
    CKW_INLINE, CKW_RESTRICT, CKW_U_BOOL, CKW_U_COMPLEX, CKW_U_IMAGINARY,
    /* C11 */
    CKW_U_ALIGNAS, CKW_U_ALIGNOF, CKW_U_ATOMIC, CKW_U_GENERIC,
    CKW_U_NORETURN, CKW_U_STATIC_ASSERT, CKW_U_THREAD_LOCAL,
    /* C23 */
    CKW_ALIGNAS, CKW_ALIGNOF, CKW_BOOL, CKW_CONSTEXPR, CKW_FALSE,
    CKW_NULLPTR, CKW_STATIC_ASSERT, CKW_THREAD_LOCAL, CKW_TRUE,
    CKW_TYPEOF, CKW_TYPEOF_UNQUAL, CKW_U_BITINT, CKW_U_DECIMAL32,
    CKW_U_DECIMAL64, CKW_U_DECIMAL128
} CKeyword;

/* Every punctuator, digraphs normalized to their canonical spelling's id
 * (e.g. "<:" and "[" both yield CPUNCT_LBRACKET; check CToken::is_digraph
 * to tell which spelling was actually used). */
typedef enum {
    CPUNCT_NONE = 0,
    CPUNCT_LBRACKET, CPUNCT_RBRACKET, CPUNCT_LPAREN, CPUNCT_RPAREN,
    CPUNCT_LBRACE, CPUNCT_RBRACE, CPUNCT_DOT, CPUNCT_ELLIPSIS, CPUNCT_ARROW,
    CPUNCT_PLUSPLUS, CPUNCT_MINUSMINUS, CPUNCT_AMP, CPUNCT_STAR,
    CPUNCT_PLUS, CPUNCT_MINUS, CPUNCT_TILDE, CPUNCT_BANG, CPUNCT_SLASH,
    CPUNCT_PERCENT, CPUNCT_SHL, CPUNCT_SHR, CPUNCT_LT, CPUNCT_GT,
    CPUNCT_LE, CPUNCT_GE, CPUNCT_EQ, CPUNCT_NE, CPUNCT_CARET, CPUNCT_PIPE,
    CPUNCT_AMPAMP, CPUNCT_PIPEPIPE, CPUNCT_QUESTION, CPUNCT_COLON,
    CPUNCT_SEMI, CPUNCT_ASSIGN, CPUNCT_STAR_ASSIGN, CPUNCT_SLASH_ASSIGN,
    CPUNCT_PERCENT_ASSIGN, CPUNCT_PLUS_ASSIGN, CPUNCT_MINUS_ASSIGN,
    CPUNCT_SHL_ASSIGN, CPUNCT_SHR_ASSIGN, CPUNCT_AMP_ASSIGN,
    CPUNCT_CARET_ASSIGN, CPUNCT_PIPE_ASSIGN, CPUNCT_COMMA, CPUNCT_HASH,
    CPUNCT_HASHHASH, CPUNCT_COLONCOLON
} CPunct;

/* A token references a slice of the original source buffer. "start" is
 * not NUL-terminated; use "length" for its extent. */
typedef struct {
    CTokenType      type;
    const char     *start;
    size_t          length;
    int             line;
    int             leading_space;  /* preceded by whitespace and/or a comment */
    int             at_line_start;  /* first token produced on its logical line */
    CKeyword        keyword;        /* CKW_NONE unless type == CTOK_IDENTIFIER and text is a keyword */
    CEncodingPrefix encoding;       /* for CTOK_CHAR_CONST / CTOK_STRING_LITERAL, else CENC_NONE */
    CPunct          punct;          /* for CTOK_PUNCT, else CPUNCT_NONE */
    int             is_digraph;     /* CTOK_PUNCT was spelled as a digraph (e.g. "<:" for "[") */
} CToken;

typedef struct {
    const char *src;
    size_t      length;
    size_t      pos;    /* always a spliced (logical) position, never mid backslash-newline */
    int         line;
    int         started; /* 0 until the first token has been produced */
} CLexer;

/* Prepares "lexer" to scan "length" bytes starting at "source". */
void clexer_init(CLexer *lexer, const char *source, size_t length);

/* Reads and returns the next preprocessing-token. Handles backslash-newline
 * line splicing transparently anywhere in the source (not just inside
 * directives), skips whitespace and both comment styles, and performs
 * maximal-munch tokenization. End of input is signaled by the returned
 * token's type being CTOK_EOF - keep calling this in a loop until you see
 * one.
 */
CToken clexer_next_token(CLexer *lexer);

/* Like clexer_next_token(), but if the next significant character is '<'
 * or '"', it is scanned as a CTOK_HEADER_NAME token (the h-char-sequence
 * or q-char-sequence form) instead of ordinary tokens. Use this only when
 * the caller has determined - from previously read tokens - that it is
 * positioned right after a "# include" (or "# __has_include") directive
 * token, since header-name is a context-sensitive token per the standard.
 * Falls back to clexer_next_token()'s behavior otherwise.
 */
CToken clexer_next_header_name(CLexer *lexer);

/* Convenience helpers for matching a token's raw source text against a
 * NUL-terminated string (exact spelling, e.g. won't equate a digraph with
 * its canonical form - use token->punct for that). */
int clexer_token_equals(const CToken *token, const char *text);
int clexer_token_starts_with(const CToken *token, const char *prefix);

/* Human-readable names, mainly for diagnostics/debugging. */
const char *clexer_keyword_name(CKeyword keyword);
const char *clexer_punct_name(CPunct punct);

#ifdef __cplusplus
}
#endif

#endif /* CLEXER_H */
