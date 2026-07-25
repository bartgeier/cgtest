/* ctestfiles.c - see ctestfiles.h.
 *
 * Enumerates every entry in the directory (opendir/readdir on POSIX,
 * FindFirstFile/FindNextFile on Windows) and filters by filename
 * ourselves on both platforms, rather than relying on Windows' native
 * wildcard matching - readdir() has no equivalent pattern-matching at
 * all, and Windows' own wildcard semantics carry legacy 8.3-filename
 * quirks that don't behave like a literal glob. Filtering ourselves
 * keeps the matching logic identical on both platforms.
 */
#include "ctestfiles.h"
#include "cpath.h"
#include "cmsg.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#define CTESTFILES_ERROR_BUFSZ 256
#define CTESTFILES_PATTERN_SCRATCH 4096

static int ctestfiles_matches(const char *filename)
{
    static const char prefix[] = "test_";
    static const char suffix[] = ".c";
    size_t prefix_len = sizeof(prefix) - 1;
    size_t suffix_len = sizeof(suffix) - 1;
    size_t len = strlen(filename);

    if (len < prefix_len + suffix_len) {
        return 0;
    }
    if (memcmp(filename, prefix, prefix_len) != 0) {
        return 0;
    }
    return memcmp(filename + len - suffix_len, suffix, suffix_len) == 0;
}

static int ctestfiles_compare(const void *a, const void *b)
{
    char *const *sa = (char *const *)a;
    char *const *sb = (char *const *)b;
    return strcmp(*sa, *sb);
}

static CTestFileScan ctestfiles_fail(CTestFileScan *scan, const char *dir)
{
    char msg[CTESTFILES_ERROR_BUFSZ];

    cpathlist_free(&scan->files);
    cmsg_build(msg, sizeof(msg), "could not open test directory: ", dir, strlen(dir), "");

    scan->ok = 0;
    scan->error = cmsg_dup(msg, strlen(msg));
    return *scan;
}

#ifdef _WIN32

CTestFileScan ctestfiles_scan(const char *dir)
{
    CTestFileScan scan;
    char pattern[CTESTFILES_PATTERN_SCRATCH];
    WIN32_FIND_DATAA find_data;
    HANDLE handle;

    scan.ok = 1;
    scan.error = NULL;
    cpathlist_init(&scan.files);

    cpath_join(pattern, sizeof(pattern), dir, "*");

    handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return ctestfiles_fail(&scan, dir);
    }

    do {
        if (ctestfiles_matches(find_data.cFileName)) {
            if (cpathlist_register(&scan.files, dir, find_data.cFileName) == CPATHLIST_ALLOC_FAILED) {
                FindClose(handle);
                return ctestfiles_fail(&scan, dir);
            }
        }
    } while (FindNextFileA(handle, &find_data));
    FindClose(handle);

    qsort(scan.files.entries, scan.files.count, sizeof(char *), ctestfiles_compare);
    return scan;
}

#else

CTestFileScan ctestfiles_scan(const char *dir)
{
    CTestFileScan scan;
    DIR *handle;
    struct dirent *entry;

    scan.ok = 1;
    scan.error = NULL;
    cpathlist_init(&scan.files);

    handle = opendir(dir);
    if (handle == NULL) {
        return ctestfiles_fail(&scan, dir);
    }

    while ((entry = readdir(handle)) != NULL) {
        if (ctestfiles_matches(entry->d_name)) {
            if (cpathlist_register(&scan.files, dir, entry->d_name) == CPATHLIST_ALLOC_FAILED) {
                closedir(handle);
                return ctestfiles_fail(&scan, dir);
            }
        }
    }
    closedir(handle);

    qsort(scan.files.entries, scan.files.count, sizeof(char *), ctestfiles_compare);
    return scan;
}

#endif

void ctestfiles_free(CTestFileScan *scan)
{
    free(scan->error);
    cpathlist_free(&scan->files);
    scan->error = NULL;
}
