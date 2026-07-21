/* cpreprocessor.h - directive-aware layer on top of clexer's phase 1-3
 * tokenizer. clexer.h intentionally has no notion of "#include" or any
 * other directive; CPreprocessor is the thin piece that recognizes just
 * enough of the directive grammar to know when a header-name token
 * should be requested from the lexer, swapping clexer_next_token() for
 * clexer_next_header_name() at exactly that point.
 *
 * This is still NOT a full preprocessor: it does not expand macros,
 * evaluate #if/#ifdef conditions, or perform file inclusion. It only
 * tracks the minimal per-line state needed to disambiguate header-name
 * tokens after:
 *   # include <...>        / # include "..."
 *   # embed   <...>        / # embed   "..."         (C23)
 *   __has_include(<...>)   / __has_include("...")
 *   __has_embed(<...>)     / __has_embed("...")       (C23)
 */
#ifndef CPREPROCESSOR_H
#define CPREPROCESSOR_H

#include "clexer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CPP_LSTATE_NONE = 0,
    CPP_LSTATE_AFTER_HASH,          /* just saw '#' at the start of a line */
    CPP_LSTATE_AFTER_HAS_INCLUDE,   /* saw __has_include/__has_embed, awaiting '(' */
    CPP_LSTATE_EXPECT_HEADER_NAME   /* next token must be lexed as a header-name */
} CPreprocessorLineState;

typedef struct {
    CLexer                 lexer;
    CPreprocessorLineState line_state;
} CPreprocessor;

/* Prepares "pp" to scan "length" bytes starting at "source". */
void cpreprocessor_init(CPreprocessor *pp, const char *source, size_t length);

/* Like clexer_next_token(), but transparently requests a CTOK_HEADER_NAME
 * token from the underlying lexer at the points where the directive
 * grammar requires one (see the file header comment above). End of
 * input is signaled by the returned token's type being CTOK_EOF.
 */
CToken cpreprocessor_next_token(CPreprocessor *pp);

#ifdef __cplusplus
}
#endif

#endif /* CPREPROCESSOR_H */
