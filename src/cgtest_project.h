/* cgtest_project.h - see specification.md's "cgtest-project.json" section.
 * Parses a cgtest-project.json file (or an in-memory buffer of the same
 * content, for testing) into CGTestProject: every path already resolved
 * to an absolute path via cpath_join()/cpathlist_register(), relative
 * to the project file's own directory.
 *
 * The 5 fields compiler_command, include_paths, source_files,
 * output_path, and test_directories are required. A missing required
 * field, an unrecognized key, or a field with the wrong JSON type is a
 * load error (see specification.md: "error and exit ... with an
 * appropriate message" - this module never terminates the process
 * itself, it just reports failure via CGTestProject::ok/error and
 * leaves acting on that to the caller). "msvc" is the one optional
 * field, defaulting to false when absent.
 */
#ifndef CGTEST_PROJECT_H
#define CGTEST_PROJECT_H

#include "cpathlist.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int        ok;                /* 0 = failed; every field below except "error" is undefined if so */
    char      *error;             /* malloc'd human-readable message, non-NULL only if !ok */
    char      *compiler_command;  /* malloc'd, e.g. "gcc -std=c99 -O3" */
    CPathList  include_paths;     /* absolute dirs, one -I per entry */
    CPathList  source_files;      /* absolute .c files compiled alongside the runner */
    char      *output_path;       /* absolute dir; cgtest-runner.c/.exe get generated here */
    CPathList  test_directories;  /* absolute dirs scanned for test_*.c files */
    int        msvc;              /* optional, defaults to 0; see cgtest_runner.h */
} CGTestProject;

/* Parses "json" (a "length"-byte buffer, not necessarily NUL-terminated)
 * as cgtest-project.json's content, resolving every relative path
 * against the already-absolute "base_dir". Pure - performs no
 * filesystem access itself, which is what makes it unit-testable with
 * in-memory buffers; see cgtest_project_load() for the disk-facing
 * entry point built on top of this one.
 */
CGTestProject cgtest_project_parse(const char *json, size_t length, const char *base_dir);

/* Reads "project_path" from disk, resolves it to an absolute path
 * (against the current working directory, if it's relative), and
 * parses it via cgtest_project_parse() with that file's own directory
 * as base_dir. "project_path" may name cgtest-project.json directly, or
 * a directory containing it (mirroring -C/--create's directory
 * argument) - if it resolves to a directory, "cgtest-project.json" is
 * looked up inside it.
 */
CGTestProject cgtest_project_load(const char *project_path);

/* Releases every owned field in "project". Safe to call on a failed
 * (ok == 0) project too. */
void cgtest_project_free(CGTestProject *project);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_PROJECT_H */
