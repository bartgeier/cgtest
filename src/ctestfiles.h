/* ctestfiles.h - finds test_*.c files directly inside a directory (not
 * recursive), matching specification.md's "Search test directorys for
 * files their nameing starts with test_...". Distinct from
 * ctestscanner.h, which scans a file's *contents* for test_ functions
 * - this module only looks at filenames.
 *
 * Scope notes (deliberate simplifications, documented rather than silent):
 *  - Not recursive: only "dir"'s immediate entries are considered, not
 *    its subdirectories.
 *  - Matching is filename-only ("test_" prefix, ".c" suffix); entries
 *    are not stat()'d to confirm they're regular files rather than,
 *    say, a directory that happens to be named that way.
 *  - Results are sorted alphabetically (by full path, which for a
 *    single directory is equivalent to sorting by filename) for
 *    deterministic run-to-run test ordering - directory enumeration
 *    order is not guaranteed stable across filesystems or runs.
 *  - A directory that doesn't exist or can't be read is a hard error
 *    naming the directory, not a silent empty result - same
 *    "surface it, don't swallow it" stance as cgtest_config.h and
 *    cpathlist.h's CPATHLIST_TRUNCATED.
 */
#ifndef CTESTFILES_H
#define CTESTFILES_H

#include "cpathlist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int        ok;      /* 0 = failed; "files" is safe to free either way */
    char      *error;   /* malloc'd, non-NULL only if !ok */
    CPathList  files;    /* absolute paths to test_*.c files directly in "dir", sorted */
} CTestFileScan;

/* Scans "dir" for immediate (non-recursive) entries named "test_*.c". */
CTestFileScan ctestfiles_scan(const char *dir);

/* Releases every owned field in "scan". Safe to call on a failed
 * (ok == 0) scan too. */
void ctestfiles_free(CTestFileScan *scan);

#ifdef __cplusplus
}
#endif

#endif /* CTESTFILES_H */
