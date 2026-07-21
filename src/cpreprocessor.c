/* cpreprocessor.c - see cpreprocessor.h */
#include "cpreprocessor.h"

static int clexer_token_is_include_keyword(const CToken *token)
{
    return clexer_token_equals(token, "include") || clexer_token_equals(token, "embed");
}

static int clexer_token_is_has_include_keyword(const CToken *token)
{
    return clexer_token_equals(token, "__has_include") || clexer_token_equals(token, "__has_embed");
}

void cpreprocessor_init(CPreprocessor *pp, const char *source, size_t length)
{
    clexer_init(&pp->lexer, source, length);
    pp->line_state = CPP_LSTATE_NONE;
}

CToken cpreprocessor_next_token(CPreprocessor *pp)
{
    CToken token;

    /* clexer_next_header_name() may still fall back to an ordinary token:
     * either it wasn't actually followed by '<'/'"' (e.g. a macro-named
     * include, "#include FOO"), or trivia-skipping crossed into a new
     * line first (a malformed directive with nothing after it). Either
     * way, that fallback token must flow through the same classification
     * below as any other token - it is not necessarily consumed yet.
     */
    token = (pp->line_state == CPP_LSTATE_EXPECT_HEADER_NAME)
            ? clexer_next_header_name(&pp->lexer)
            : clexer_next_token(&pp->lexer);

    if (token.type == CTOK_HEADER_NAME) {
        pp->line_state = CPP_LSTATE_NONE;
        return token;
    }

    /* A new logical line always ends whatever directive we were tracking. */
    if (token.at_line_start) {
        pp->line_state = (token.type == CTOK_PUNCT && token.punct == CPUNCT_HASH)
                          ? CPP_LSTATE_AFTER_HASH
                          : CPP_LSTATE_NONE;
        return token;
    }

    switch (pp->line_state) {
    case CPP_LSTATE_AFTER_HASH:
        if (token.type == CTOK_IDENTIFIER && clexer_token_is_include_keyword(&token)) {
            pp->line_state = CPP_LSTATE_EXPECT_HEADER_NAME;
        } else if (token.type == CTOK_IDENTIFIER && clexer_token_is_has_include_keyword(&token)) {
            pp->line_state = CPP_LSTATE_AFTER_HAS_INCLUDE;
        } else {
            pp->line_state = CPP_LSTATE_NONE;
        }
        break;

    case CPP_LSTATE_AFTER_HAS_INCLUDE:
        if (token.type == CTOK_PUNCT && token.punct == CPUNCT_LPAREN) {
            pp->line_state = CPP_LSTATE_EXPECT_HEADER_NAME;
        } else {
            pp->line_state = CPP_LSTATE_NONE;
        }
        break;

    case CPP_LSTATE_NONE:
    case CPP_LSTATE_EXPECT_HEADER_NAME:
    default:
        /* __has_include / __has_embed may also appear later in a directive
         * line (typically deep in a #if expression), independent of what
         * immediately followed '#'. Also covers the fallback case above:
         * an expected header-name that didn't materialize just stops
         * being expected. */
        if (token.type == CTOK_IDENTIFIER && clexer_token_is_has_include_keyword(&token)) {
            pp->line_state = CPP_LSTATE_AFTER_HAS_INCLUDE;
        } else {
            pp->line_state = CPP_LSTATE_NONE;
        }
        break;
    }

    return token;
}
