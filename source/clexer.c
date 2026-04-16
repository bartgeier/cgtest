/*
 * clexer.c — Full C lexer implementation
 */
#include "clexer.h"

#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* C keywords                                                           */
/* ------------------------------------------------------------------ */

static const char *KEYWORDS[] = {
    "auto",     "break",    "case",     "char",     "const",
    "continue", "default",  "do",       "double",   "else",
    "enum",     "extern",   "float",    "for",      "goto",
    "if",       "inline",   "int",      "long",     "register",
    "restrict", "return",   "short",    "signed",   "sizeof",
    "static",   "struct",   "switch",   "typedef",  "union",
    "unsigned", "void",     "volatile", "while",
    /* C99 */
    "_Bool",    "_Complex", "_Imaginary",
    /* C11 */
    "_Alignas", "_Alignof", "_Atomic",  "_Generic", "_Noreturn",
    "_Static_assert", "_Thread_local",
    /* common extensions */
    "__asm__",  "__volatile__", "__inline__",
    NULL
};

static int is_keyword(const char *s, size_t len)
{
    const char **kw;
    for (kw = KEYWORDS; *kw; kw++) {
        size_t kwlen = strlen(*kw);
        if (kwlen == len && strncmp(*kw, s, len) == 0) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Character classification helpers                                     */
/* ------------------------------------------------------------------ */

static int is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alnum(char c) { return is_alpha(c) || is_digit(c); }
static int is_hex(char c)
{
    return is_digit(c)
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}
static int is_oct(char c) { return c >= '0' && c <= '7'; }

/* ------------------------------------------------------------------ */
/* Cursor helpers                                                        */
/* ------------------------------------------------------------------ */

static char peek(const CLexer *lex, size_t offset)
{
    size_t p = lex->pos + offset;
    if (p >= lex->src_len) return '\0';
    return lex->src[p];
}

static char cur(const CLexer *lex) { return peek(lex, 0); }
static char nxt(const CLexer *lex) { return peek(lex, 1); }

static void advance(CLexer *lex)
{
    if (lex->pos >= lex->src_len) return;
    if (lex->src[lex->pos] == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    lex->pos++;
}

static void advance_n(CLexer *lex, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) advance(lex);
}

/* Build a token from [start_pos, lex->pos) */
static CToken make_tok(const CLexer *lex, CTokenKind kind,
                       size_t start_pos, unsigned line, unsigned col)
{
    CToken t;
    t.kind       = kind;
    t.start      = lex->src + start_pos;
    t.len        = lex->pos - start_pos;
    t.punct      = '\0';
    t.line       = line;
    t.col        = col;
    t.is_keyword = 0;
    return t;
}

/* ------------------------------------------------------------------ */
/* Skip whitespace / line continuations                                 */
/* ------------------------------------------------------------------ */

static void skip_whitespace(CLexer *lex)
{
    for (;;) {
        /* Line continuation: backslash-newline */
        if (cur(lex) == '\\' && nxt(lex) == '\n') {
            advance_n(lex, 2);
            continue;
        }
        if (cur(lex) == ' ' || cur(lex) == '\t' ||
            cur(lex) == '\r' || cur(lex) == '\f' || cur(lex) == '\v') {
            advance(lex);
            continue;
        }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Escape sequence inside string/char literals                          */
/* ------------------------------------------------------------------ */

static void skip_escape(CLexer *lex)
{
    /* We are positioned on the char after '\' */
    char c = cur(lex);
    if (c == 'x') {
        advance(lex); /* x */
        while (is_hex(cur(lex))) advance(lex);
    } else if (is_oct(c)) {
        int n = 0;
        while (n < 3 && is_oct(cur(lex))) { advance(lex); n++; }
    } else if (c == 'u') {
        advance(lex); /* u */
        int n = 0;
        while (n < 4 && is_hex(cur(lex))) { advance(lex); n++; }
    } else if (c == 'U') {
        advance(lex); /* U */
        int n = 0;
        while (n < 8 && is_hex(cur(lex))) { advance(lex); n++; }
    } else if (c != '\0') {
        advance(lex); /* single-char escape: n t r 0 a b f v ' " \ etc. */
    }
}

/* ------------------------------------------------------------------ */
/* Lex: string literal "..."  (also handles L"" u"" U"" u8"")          */
/* ------------------------------------------------------------------ */

static CToken lex_string(CLexer *lex, size_t start, unsigned line, unsigned col)
{
    /* skip opening " */
    advance(lex);
    while (cur(lex) != '\0' && cur(lex) != '\n') {
        if (cur(lex) == '"') { advance(lex); break; }
        if (cur(lex) == '\\') { advance(lex); skip_escape(lex); }
        else advance(lex);
    }
    return make_tok(lex, TK_STRING, start, line, col);
}

/* ------------------------------------------------------------------ */
/* Lex: char literal '...'                                              */
/* ------------------------------------------------------------------ */

static CToken lex_char(CLexer *lex, size_t start, unsigned line, unsigned col)
{
    advance(lex); /* skip ' */
    while (cur(lex) != '\0' && cur(lex) != '\n') {
        if (cur(lex) == '\'') { advance(lex); break; }
        if (cur(lex) == '\\') { advance(lex); skip_escape(lex); }
        else advance(lex);
    }
    return make_tok(lex, TK_CHAR, start, line, col);
}

/* ------------------------------------------------------------------ */
/* Lex: number (int or float)                                           */
/*                                                                      */
/* Grammar (simplified):                                                */
/*   0x[hex]+[uUlL]*                                                    */
/*   0[oct]*[uUlL]*                                                     */
/*   [digit]+([.][digit]*)?([eE][+-]?[digit]+)?[fFlLuU]*               */
/* ------------------------------------------------------------------ */

static CToken lex_number(CLexer *lex, size_t start, unsigned line, unsigned col)
{
    CTokenKind kind = TK_INT;

    if (cur(lex) == '0' && (nxt(lex) == 'x' || nxt(lex) == 'X')) {
        advance_n(lex, 2); /* 0x */
        while (is_hex(cur(lex)) || cur(lex) == '_') advance(lex);
    } else if (cur(lex) == '0' && is_oct(nxt(lex))) {
        advance(lex); /* 0 */
        while (is_oct(cur(lex))) advance(lex);
    } else {
        while (is_digit(cur(lex))) advance(lex);
        if (cur(lex) == '.' && is_digit(nxt(lex))) {
            kind = TK_FLOAT;
            advance(lex); /* . */
            while (is_digit(cur(lex))) advance(lex);
        }
        if (cur(lex) == 'e' || cur(lex) == 'E') {
            kind = TK_FLOAT;
            advance(lex);
            if (cur(lex) == '+' || cur(lex) == '-') advance(lex);
            while (is_digit(cur(lex))) advance(lex);
        }
    }

    /* suffix: u U l L f F */
    while (cur(lex) == 'u' || cur(lex) == 'U' ||
           cur(lex) == 'l' || cur(lex) == 'L' ||
           cur(lex) == 'f' || cur(lex) == 'F') {
        advance(lex);
    }

    return make_tok(lex, kind, start, line, col);
}

/* ------------------------------------------------------------------ */
/* Lex: identifier or keyword                                           */
/* ------------------------------------------------------------------ */

static CToken lex_ident(CLexer *lex, size_t start, unsigned line, unsigned col)
{
    CToken t;
    while (is_alnum(cur(lex))) advance(lex);
    t            = make_tok(lex, TK_IDENT, start, line, col);
    t.is_keyword = is_keyword(t.start, t.len);
    return t;
}

/* ------------------------------------------------------------------ */
/* Lex: line comment  // ...                                            */
/* ------------------------------------------------------------------ */

static CToken lex_line_comment(CLexer *lex, size_t start,
                               unsigned line, unsigned col)
{
    while (cur(lex) != '\0' && cur(lex) != '\n') advance(lex);
    return make_tok(lex, TK_COMMENT, start, line, col);
}

/* ------------------------------------------------------------------ */
/* Lex: block comment  /* ... *\/                                       */
/* ------------------------------------------------------------------ */

static CToken lex_block_comment(CLexer *lex, size_t start,
                                unsigned line, unsigned col)
{
    while (cur(lex) != '\0') {
        if (cur(lex) == '*' && nxt(lex) == '/') {
            advance_n(lex, 2);
            break;
        }
        advance(lex);
    }
    return make_tok(lex, TK_COMMENT, start, line, col);
}

/* ------------------------------------------------------------------ */
/* Lex: multi-char operators                                            */
/* ------------------------------------------------------------------ */

static CToken lex_punct(CLexer *lex, size_t start, unsigned line, unsigned col)
{
    char c = cur(lex);
    char n = nxt(lex);
    CToken t;

    /* Three-character operators */
    if (c == '.' && n == '.' && peek(lex, 2) == '.') {
        advance_n(lex, 3);
        return make_tok(lex, TK_ELLIPSIS, start, line, col);
    }
    if (c == '<' && n == '<' && peek(lex, 2) == '=') {
        advance_n(lex, 3);
        return make_tok(lex, TK_LSHIFT_EQ, start, line, col);
    }
    if (c == '>' && n == '>' && peek(lex, 2) == '=') {
        advance_n(lex, 3);
        return make_tok(lex, TK_RSHIFT_EQ, start, line, col);
    }

    /* Two-character operators */
    if (c == '-' && n == '>') { advance_n(lex, 2); return make_tok(lex, TK_ARROW,     start, line, col); }
    if (c == '+' && n == '+') { advance_n(lex, 2); return make_tok(lex, TK_INC,       start, line, col); }
    if (c == '-' && n == '-') { advance_n(lex, 2); return make_tok(lex, TK_DEC,       start, line, col); }
    if (c == '<' && n == '<') { advance_n(lex, 2); return make_tok(lex, TK_LSHIFT,    start, line, col); }
    if (c == '>' && n == '>') { advance_n(lex, 2); return make_tok(lex, TK_RSHIFT,    start, line, col); }
    if (c == '<' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_LEQ,       start, line, col); }
    if (c == '>' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_GEQ,       start, line, col); }
    if (c == '=' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_EQ,        start, line, col); }
    if (c == '!' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_NEQ,       start, line, col); }
    if (c == '&' && n == '&') { advance_n(lex, 2); return make_tok(lex, TK_AND,       start, line, col); }
    if (c == '|' && n == '|') { advance_n(lex, 2); return make_tok(lex, TK_OR,        start, line, col); }
    if (c == '+' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_PLUS_EQ,   start, line, col); }
    if (c == '-' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_MINUS_EQ,  start, line, col); }
    if (c == '*' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_STAR_EQ,   start, line, col); }
    if (c == '/' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_SLASH_EQ,  start, line, col); }
    if (c == '%' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_PERCENT_EQ,start, line, col); }
    if (c == '&' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_AMP_EQ,    start, line, col); }
    if (c == '|' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_PIPE_EQ,   start, line, col); }
    if (c == '^' && n == '=') { advance_n(lex, 2); return make_tok(lex, TK_CARET_EQ,  start, line, col); }

    /* Single-character punctuation */
    advance(lex);
    t = make_tok(lex, TK_PUNCT, start, line, col);
    t.punct = c;
    return t;
}

/* ------------------------------------------------------------------ */
/* clexer_init                                                          */
/* ------------------------------------------------------------------ */

void clexer_init(CLexer *lex,
                 const char *src, size_t src_len,
                 const char *filename)
{
    lex->src           = src;
    lex->src_len       = src_len;
    lex->pos           = 0;
    lex->line          = 1;
    lex->col           = 1;
    lex->filename      = filename ? filename : "<unknown>";
    lex->emit_comments = 0;
    lex->emit_newlines = 0;
}

/* ------------------------------------------------------------------ */
/* clexer_next                                                          */
/* ------------------------------------------------------------------ */

int clexer_next(CLexer *lex, CToken *tok)
{
    for (;;) {
        size_t   start;
        unsigned line, col;

        skip_whitespace(lex);

        start = lex->pos;
        line  = lex->line;
        col   = lex->col;

        if (lex->pos >= lex->src_len) {
            *tok = make_tok(lex, TK_EOF, start, line, col);
            return 1;
        }

        /* Newline */
        if (cur(lex) == '\n') {
            advance(lex);
            if (lex->emit_newlines) {
                *tok = make_tok(lex, TK_NEWLINE, start, line, col);
                return 1;
            }
            continue;
        }

        /* Preprocessor directive: # at start of line (col==1 after ws skip) */
        if (cur(lex) == '#') {
            if (col == 1) {
                advance(lex); /* skip # */
                *tok = make_tok(lex, TK_PP_HASH, start, line, col);
                return 1;
            }
            /* # inside expression (stringification / token paste) */
            advance(lex);
            CToken t = make_tok(lex, TK_PUNCT, start, line, col);
            t.punct = '#';
            *tok = t;
            return 1;
        }

        /* Comments */
        if (cur(lex) == '/' && nxt(lex) == '/') {
            advance_n(lex, 2);
            CToken t = lex_line_comment(lex, start, line, col);
            if (lex->emit_comments) { *tok = t; return 1; }
            continue;
        }
        if (cur(lex) == '/' && nxt(lex) == '*') {
            advance_n(lex, 2);
            CToken t = lex_block_comment(lex, start, line, col);
            if (lex->emit_comments) { *tok = t; return 1; }
            continue;
        }

        /* String literals (with optional prefix L u U u8) */
        if (cur(lex) == '"') {
            *tok = lex_string(lex, start, line, col);
            return 1;
        }
        if ((cur(lex) == 'L' || cur(lex) == 'u' || cur(lex) == 'U')
            && nxt(lex) == '"') {
            advance(lex);
            *tok = lex_string(lex, start, line, col);
            return 1;
        }
        /* u8"..." */
        if (cur(lex) == 'u' && nxt(lex) == '8' && peek(lex, 2) == '"') {
            advance_n(lex, 2);
            *tok = lex_string(lex, start, line, col);
            return 1;
        }

        /* Char literals (with optional prefix L u U) */
        if (cur(lex) == '\'') {
            *tok = lex_char(lex, start, line, col);
            return 1;
        }
        if ((cur(lex) == 'L' || cur(lex) == 'u' || cur(lex) == 'U')
            && nxt(lex) == '\'') {
            advance(lex);
            *tok = lex_char(lex, start, line, col);
            return 1;
        }

        /* Numbers */
        if (is_digit(cur(lex))) {
            *tok = lex_number(lex, start, line, col);
            return 1;
        }
        /* Floats starting with '.' e.g. .5f */
        if (cur(lex) == '.' && is_digit(nxt(lex))) {
            *tok = lex_number(lex, start, line, col);
            return 1;
        }

        /* Identifiers / keywords */
        if (is_alpha(cur(lex))) {
            *tok = lex_ident(lex, start, line, col);
            return 1;
        }

        /* Everything else: operators and punctuation */
        *tok = lex_punct(lex, start, line, col);
        return 1;
    }
}

/* ------------------------------------------------------------------ */
/* Utility                                                              */
/* ------------------------------------------------------------------ */

size_t clexer_tok_str(const CToken *tok, char *dst, size_t dst_size)
{
    size_t len;
    if (!dst || dst_size == 0) return 0;
    len = tok->len < dst_size - 1 ? tok->len : dst_size - 1;
    memcpy(dst, tok->start, len);
    dst[len] = '\0';
    return len;
}

int clexer_tok_eq(const CToken *tok, const char *s)
{
    size_t slen = strlen(s);
    return tok->len == slen && strncmp(tok->start, s, slen) == 0;
}

const char *clexer_kind_name(CTokenKind kind)
{
    switch (kind) {
    case TK_EOF:        return "EOF";
    case TK_IDENT:      return "IDENT";
    case TK_INT:        return "INT";
    case TK_FLOAT:      return "FLOAT";
    case TK_STRING:     return "STRING";
    case TK_CHAR:       return "CHAR";
    case TK_PP_HASH:    return "PP_HASH";
    case TK_PP_INCLUDE: return "PP_INCLUDE";
    case TK_PP_DEFINE:  return "PP_DEFINE";
    case TK_PUNCT:      return "PUNCT";
    case TK_ARROW:      return "ARROW";
    case TK_INC:        return "INC";
    case TK_DEC:        return "DEC";
    case TK_LSHIFT:     return "LSHIFT";
    case TK_RSHIFT:     return "RSHIFT";
    case TK_LEQ:        return "LEQ";
    case TK_GEQ:        return "GEQ";
    case TK_EQ:         return "EQ";
    case TK_NEQ:        return "NEQ";
    case TK_AND:        return "AND";
    case TK_OR:         return "OR";
    case TK_ELLIPSIS:   return "ELLIPSIS";
    case TK_PLUS_EQ:    return "PLUS_EQ";
    case TK_MINUS_EQ:   return "MINUS_EQ";
    case TK_STAR_EQ:    return "STAR_EQ";
    case TK_SLASH_EQ:   return "SLASH_EQ";
    case TK_PERCENT_EQ: return "PERCENT_EQ";
    case TK_AMP_EQ:     return "AMP_EQ";
    case TK_PIPE_EQ:    return "PIPE_EQ";
    case TK_CARET_EQ:   return "CARET_EQ";
    case TK_LSHIFT_EQ:  return "LSHIFT_EQ";
    case TK_RSHIFT_EQ:  return "RSHIFT_EQ";
    case TK_COMMENT:    return "COMMENT";
    case TK_NEWLINE:    return "NEWLINE";
    case TK_ERROR:      return "ERROR";
    default:            return "UNKNOWN";
    }
}
