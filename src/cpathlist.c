/* cpathlist.c - see cpathlist.h */
#include "cpathlist.h"
#include "cpath.h"

#include <stdlib.h>
#include <string.h>

/* Generous relative to real filesystem path length limits (Linux's
 * PATH_MAX is typically 4096; Windows historically 260 but modern APIs
 * support far more) - long enough that legitimate config paths never
 * hit it, small enough to be a trivial stack buffer. */
#define CPATHLIST_SCRATCH_CAPACITY 4096

void cpathlist_init(CPathList *list)
{
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

CPathListStatus cpathlist_register(CPathList *list, const char *base, const char *rel)
{
    char scratch[CPATHLIST_SCRATCH_CAPACITY];
    CPath joined;
    char *owned;

    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        char **grown = (char **)realloc(list->entries, new_capacity * sizeof(char *));
        if (grown == NULL) {
            return CPATHLIST_ALLOC_FAILED;
        }
        list->entries = grown;
        list->capacity = new_capacity;
    }

    joined = cpath_join(scratch, sizeof(scratch), base, rel);

    owned = (char *)malloc(joined.length + 1);
    if (owned == NULL) {
        return CPATHLIST_ALLOC_FAILED;
    }
    memcpy(owned, joined.data, joined.length);
    owned[joined.length] = '\0';

    list->entries[list->count] = owned;
    list->count++;

    return joined.truncated ? CPATHLIST_TRUNCATED : CPATHLIST_OK;
}

void cpathlist_free(CPathList *list)
{
    size_t i;
    for (i = 0; i < list->count; i++) {
        free(list->entries[i]);
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}
