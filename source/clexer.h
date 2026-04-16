/*
 * clexer.h — Full C lexer (C89/C99/C11 tokens)
 *
 * Usage:
 *   CLexer lex;
 *   clexer_init(&lex, source_text, source_len, filename);
 *   CToken tok;
 *   while (clexer_next(&lex, &tok) && tok.kind != TK_EOF) { ... }
 */
#ifndef CLEXER_H
#define CLEXER_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Token kinds                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    TK_EOF = 0,

    /* Literals */
    TK_IDENT,       /* identifier or keyword */
    TK_INT,         /* integer constant   : 42  0xff  0777  1UL */
    TK_FLOAT,       /* float constant     : 3.14  1e10f */
    TK_STRING,      /* string literal     : "hello\n" */
    TK_CHAR,        /* char literal       : 'a'  '\n' */

    /* Preprocessor */
    TK_PP_HASH,     /* # at start of line */
    TK_PP_INCLUDE,  /* #include */
    TK_PP_DEFINE,   /* #define  etc. – the directive word after # */

    /* Punctuation / operators (single characters stored in tok.punct) */
    TK_PUNCT,

    /* Multi-character operators */
    TK_ARROW,       /* -> */
    TK_INC,         /* ++ */
    TK_DEC,         /* -- */
    TK_LSHIFT,      /* << */
    TK_RSHIFT,      /* >> */
    TK_LEQ,         /* <= */
    TK_GEQ,         /* >= */
    TK_EQ,          /* == */
    TK_NEQ,         /* != */
    TK_AND,         /* && */
    TK_OR,          /* || */
    TK_ELLIPSIS,    /* ... */
    TK_PLUS_EQ,     /* += */
    TK_MINUS_EQ,    /* -= */
    TK_STAR_EQ,     /* *= */
    TK_SLASH_EQ,    /* /= */
    TK_PERCENT_EQ,  /* %= */
    TK_AMP_EQ,      /* &= */
    TK_PIPE_EQ,     /* |= */
    TK_CARET_EQ,    /* ^= */
    TK_LSHIFT_EQ,   /* <<= */
    TK_RSHIFT_EQ,   /* >>= */

    /* Comments (emitted when CLexer.emit_comments = 1) */
    TK_COMMENT,     /* // or block comment */

    /* Newlines (emitted when CLexer.emit_newlines = 1) */
    TK_NEWLINE,

    TK_ERROR        /* unrecognised character */
} CTokenKind;

/* ------------------------------------------------------------------ */
/* Token                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    CTokenKind  kind;

    /* Pointer into the original source buffer + byte length.
     * NOT null-terminated. Use clexer_tok_str() to copy. */
    const char *start;
    size_t      len;

    /* For TK_PUNCT: the character value */
    char        punct;

    /* Source location */
    unsigned    line;
    unsigned    col;

    /* For TK_IDENT: 1 if the identifier is a C keyword */
    int         is_keyword;
} CToken;

/* ------------------------------------------------------------------ */
/* Lexer state                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *src;        /* source buffer (not owned) */
    size_t      src_len;
    size_t      pos;        /* current byte position    */
    unsigned    line;
    unsigned    col;
    const char *filename;   /* for diagnostics          */

    /* Options */
    int emit_comments;      /* 1 → emit TK_COMMENT tokens, 0 → skip  */
    int emit_newlines;      /* 1 → emit TK_NEWLINE tokens, 0 → skip  */
} CLexer;

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

/*
 * Initialise the lexer.
 * src must remain valid for the lifetime of the lexer.
 * src_len = number of bytes (strlen for NUL-terminated strings).
 * filename is used only for diagnostics (may be NULL).
 */
void clexer_init(CLexer *lex,
                 const char *src, size_t src_len,
                 const char *filename);

/*
 * Advance to the next token and write it into *tok.
 * Returns 1 on success (including TK_EOF), 0 on internal error.
 * After TK_EOF is returned all subsequent calls keep returning TK_EOF.
 */
int clexer_next(CLexer *lex, CToken *tok);

/*
 * Copy the token text into dst (null-terminated, truncated to dst_size-1).
 * Returns the number of bytes written (excluding NUL).
 */
size_t clexer_tok_str(const CToken *tok, char *dst, size_t dst_size);

/*
 * Return a human-readable name for a token kind (for debugging).
 */
const char *clexer_kind_name(CTokenKind kind);

/*
 * Returns 1 if the token text matches the NUL-terminated string s.
 */
int clexer_tok_eq(const CToken *tok, const char *s);

#endif /* CLEXER_H */
