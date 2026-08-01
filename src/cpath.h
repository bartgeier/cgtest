/* cpath.h - lexical (string-only) file path joining and normalization.
 *
 * cpath_join() merges a base path with a relative path the way a shell
 * would resolve "cd base && cd rel && pwd" textually - without ever
 * touching the filesystem. This is deliberate: no realpath(), no symlink
 * resolution, no checking that anything exists. That keeps it dependency
 * free, deterministic, and safe to run on paths that don't exist yet
 * (e.g. output directories from cgtest-project.json).
 *
 * Scope notes (deliberate simplifications, documented rather than silent):
 *  - "base" is assumed to already be an absolute path (resolved by the
 *    caller, e.g. via getcwd()). cpath_join() never inspects the current
 *    working directory or the running executable's location - both are
 *    irrelevant to it by design.
 *  - Both '/' and '\' are accepted as separators in the input (so Windows-
 *    and Unix-style input can be mixed), but output always uses '/'.
 *  - If "rel" is itself an absolute path, "base" is discarded entirely
 *    and the (normalized) "rel" is the result - matching common
 *    path-join semantics (e.g. Python's os.path.join).
 *  - "." segments are dropped; ".." segments pop the preceding segment.
 *    ".." segments in excess of what's available (i.e. that would climb
 *    above the root) are silently clamped at the root rather than
 *    treated as an error.
 *  - The caller supplies the output buffer and its capacity; cpath_join()
 *    never allocates. If the normalized result doesn't fit, it is
 *    truncated (always still NUL-terminated, if capacity > 0) and
 *    CPath::truncated is set - no separate int-return/out-param needed.
 */
#ifndef CPATH_H
#define CPATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char   *data;       /* == buf passed to cpath_join() */
    size_t  length;     /* bytes written to data, excluding the NUL */
    int     truncated;  /* 1 if capacity was too small to fit the full result */
} CPath;

/* Joins "base" and "rel" into a normalized absolute path, written into
 * "buf" (capacity "capacity" bytes, including room for the NUL
 * terminator). See the file header comment above for exact semantics.
 */
CPath cpath_join(char *buf, size_t capacity, const char *base, const char *rel);

/* Returns the directory portion of "path" (everything before its last
 * '/' or '\'), written into "buf" the same way cpath_join() writes to
 * its buffer (capacity, truncation, NUL-termination all work the same
 * way). Purely lexical, like cpath_join() - "path" need not exist.
 * A path with no separator at all yields "."; the root itself ("/" or
 * "C:/") yields itself, never something shorter.
 */
CPath cpath_dirname(char *buf, size_t capacity, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* CPATH_H */
