/* cpathlist.h - a growable, incrementally built list of resolved
 * absolute paths, layered on top of cpath_join() the same way
 * cpreprocessor.c is layered on clexer.c. Each registered path is
 * joined against a base, normalized, and copied into its own
 * independently owned, exact-fit allocation - not a shared arena -
 * so growing the list (reallocating the array of entries) never
 * invalidates a CPathEntry::path pointer already handed out. Same
 * storage pattern ctestscanner.h uses for CTestFunction::name.
 *
 * Scope notes (deliberate simplifications, documented rather than silent):
 *  - cpathlist_register() builds the joined path in a fixed-size
 *    internal scratch buffer before copying it into owned storage.
 *    That buffer is generous relative to real filesystem path length
 *    limits (see CPATHLIST_SCRATCH_CAPACITY in cpathlist.c), but a
 *    pathological base+rel combination could still exceed it -
 *    CPATHLIST_TRUNCATED signals that rather than truncating silently.
 *  - This module knows nothing about cgtest-config.json's schema: it's
 *    a generic list-of-paths container. Distinguishing include dirs
 *    from source files from test dirs is the caller's job (e.g. one
 *    CPathList per category), not this module's.
 */
#ifndef CPATHLIST_H
#define CPATHLIST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One registered path. "path" is malloc'd, NUL-terminated, and owned
 * independently of every other entry (see file header comment). */
typedef struct {
    char *path;
} CPathEntry;

typedef struct {
    CPathEntry *entries;
    size_t      count;
    size_t      capacity;
} CPathList;

typedef enum {
    CPATHLIST_ALLOC_FAILED = 0,  /* nothing was registered */
    CPATHLIST_OK           = 1,
    CPATHLIST_TRUNCATED    = 2   /* registered, but the joined result was cut short */
} CPathListStatus;

/* Prepares "list" to have entries registered into it. */
void cpathlist_init(CPathList *list);

/* Joins "base" and "rel" (see cpath_join()) and appends the normalized
 * absolute result to "list" as a new, independently owned entry.
 */
CPathListStatus cpathlist_register(CPathList *list, const char *base, const char *rel);

/* Releases every entry in "list" and its backing array. Safe to call
 * on a list that was only ever cpathlist_init()'d. */
void cpathlist_free(CPathList *list);

#ifdef __cplusplus
}
#endif

#endif /* CPATHLIST_H */
