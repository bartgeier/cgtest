/* ctestscanner.c - see ctestscanner.h */
#include "ctestscanner.h"
#include "cpreprocessor.h"

#include <stdlib.h>
#include <string.h>

#define CTESTSCANNER_TEST_PREFIX "test_"

enum {
    ST_INIT = 0,
    ST_SAW_RETURN_VOID,
    ST_SAW_NAME,
    ST_SAW_LPAREN,
    ST_SAW_PARAM_VOID,
    ST_SAW_FIXTURE_TYPE,
    ST_SAW_FIXTURE_STAR,
    ST_SAW_FIXTURE_PARAM_NAME
};

typedef struct {
    CTestFunction *items;
    size_t         count;
    size_t         capacity;
} CTestFunctionList;

/* "fixture_type_token" is NULL for the plain "(void)" form, or the
 * captured type token for the one-parameter fixture form (see
 * ctestscanner.h). */
static int ctestscanner_list_push(CTestFunctionList *list, const CToken *name_token, int line,
                                   const CToken *fixture_type_token)
{
    char *name;
    char *fixture_type = NULL;

    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        CTestFunction *grown = (CTestFunction *)realloc(list->items, new_capacity * sizeof(CTestFunction));
        if (grown == NULL) {
            return 0;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }

    name = (char *)malloc(name_token->length + 1);
    if (name == NULL) {
        return 0;
    }
    memcpy(name, name_token->start, name_token->length);
    name[name_token->length] = '\0';

    if (fixture_type_token != NULL) {
        fixture_type = (char *)malloc(fixture_type_token->length + 1);
        if (fixture_type == NULL) {
            free(name);
            return 0;
        }
        memcpy(fixture_type, fixture_type_token->start, fixture_type_token->length);
        fixture_type[fixture_type_token->length] = '\0';
    }

    list->items[list->count].name = name;
    list->items[list->count].fixture_type = fixture_type;
    list->items[list->count].has_teardown = 0;
    list->items[list->count].line = line;
    list->count++;
    return 1;
}

/* Advances past an entire preprocessing directive line: "token" must
 * already be a '#' that is the first token on its line; returns the
 * next token that itself starts a line (or CTOK_EOF). */
static CToken ctestscanner_skip_directive_line(CPreprocessor *pp)
{
    CToken token;
    do {
        token = cpreprocessor_next_token(pp);
    } while (token.type != CTOK_EOF && !token.at_line_start);
    return token;
}

static int ctestscanner_is_directive_hash(const CToken *token)
{
    return token->at_line_start && token->type == CTOK_PUNCT && token->punct == CPUNCT_HASH;
}

static int ctestscanner_state_after_mismatch(const CToken *token)
{
    return (token->type == CTOK_IDENTIFIER && token->keyword == CKW_VOID) ? ST_SAW_RETURN_VOID : ST_INIT;
}

CTestFunction *ctestscanner_find(const char *source, size_t length, size_t *out_count)
{
    CPreprocessor pp;
    CToken token;
    CToken name_token;
    CToken fixture_type_token;
    int state = ST_INIT;
    int name_line = 0;
    CTestFunctionList list;

    list.items = NULL;
    list.count = 0;
    list.capacity = 0;

    cpreprocessor_init(&pp, source, length);
    token = cpreprocessor_next_token(&pp);

    while (token.type != CTOK_EOF) {
        int consumed_extra = 0;

        if (ctestscanner_is_directive_hash(&token)) {
            token = ctestscanner_skip_directive_line(&pp);
            continue;
        }

        switch (state) {
        case ST_INIT:
            state = (token.type == CTOK_IDENTIFIER && token.keyword == CKW_VOID) ? ST_SAW_RETURN_VOID : ST_INIT;
            break;

        case ST_SAW_RETURN_VOID:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_NONE &&
                clexer_token_starts_with(&token, CTESTSCANNER_TEST_PREFIX)) {
                name_token = token;
                name_line = token.line;
                state = ST_SAW_NAME;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_NAME:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_LPAREN) {
                state = ST_SAW_LPAREN;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_LPAREN:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_VOID) {
                state = ST_SAW_PARAM_VOID;
            } else if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_NONE) {
                /* Start of the one-parameter fixture form: "Type *param".
                 * "Type" must be a plain (non-keyword) identifier - a
                 * typedef'd fixture name, not e.g. a builtin "int". */
                fixture_type_token = token;
                state = ST_SAW_FIXTURE_TYPE;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_FIXTURE_TYPE:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_STAR) {
                state = ST_SAW_FIXTURE_STAR;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_FIXTURE_STAR:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_NONE) {
                state = ST_SAW_FIXTURE_PARAM_NAME;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_PARAM_VOID:
        case ST_SAW_FIXTURE_PARAM_NAME:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_RPAREN) {
                CToken next = cpreprocessor_next_token(&pp);
                int is_fixture = (state == ST_SAW_FIXTURE_PARAM_NAME);

                while (next.type != CTOK_EOF && ctestscanner_is_directive_hash(&next)) {
                    next = ctestscanner_skip_directive_line(&pp);
                }

                if (next.type == CTOK_PUNCT && next.punct == CPUNCT_LBRACE) {
                    if (!ctestscanner_list_push(&list, &name_token, name_line,
                                                 is_fixture ? &fixture_type_token : NULL)) {
                        ctestscanner_free(list.items, list.count);
                        *out_count = 0;
                        return NULL;
                    }
                    state = ST_INIT;
                } else {
                    state = ctestscanner_state_after_mismatch(&next);
                }

                token = next;
                consumed_extra = 1;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        default:
            state = ST_INIT;
            break;
        }

        if (!consumed_extra) {
            token = cpreprocessor_next_token(&pp);
        }
    }

    *out_count = list.count;
    return list.items;
}

void ctestscanner_free(CTestFunction *functions, size_t count)
{
    size_t i;
    if (functions == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(functions[i].name);
        free(functions[i].fixture_type);
    }
    free(functions);
}
