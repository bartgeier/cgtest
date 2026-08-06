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
 * leaves acting on that to the caller). "msvc" and
 * "single_translation_unit" are the two optional fields, both
 * defaulting to false when absent.
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
    int        single_translation_unit; /* optional, defaults to 0; see cgtest_runner.h */
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
 * a directory containing it (mirroring -i/--init's directory
 * argument) - if it resolves to a directory, "cgtest-project.json" is
 * looked up inside it.
 */
CGTestProject cgtest_project_load(const char *project_path);

/* Releases every owned field in "project". Safe to call on a failed
 * (ok == 0) project too. */
void cgtest_project_free(CGTestProject *project);

/* Scans "json" (a "length"-byte buffer, not necessarily NUL-terminated)
 * for which of cgtest-project.json's optional fields ("msvc",
 * "single_translation_unit") are present at its top level - without
 * resolving paths, validating value types, or building a CGTestProject
 * the way cgtest_project_parse() does. Used only by cgtest_create_run()
 * (cgtest_create.h) to detect which optional fields are missing from
 * an already-existing cgtest-project.json - e.g. one written by an
 * older cgtest.exe, before "single_translation_unit" existed - so it
 * can patch just those in with their default value rather than leaving
 * the file permanently missing a newer optional field.
 *
 * Deliberately conservative, not lenient: invalid JSON, a non-object
 * top level, an unrecognized key, a duplicate key, or a field value
 * that isn't a string/array/primitive (every shape any field defined
 * so far actually uses - same one-level-deep assumption as
 * cgtest_project_parse(), see this module's own header comment) all
 * return 0 with "out_has_msvc"/"out_has_single_translation_unit" left
 * untouched - if this can't fully make sense of the file's shape, the
 * safest thing its caller can do is leave the file alone entirely
 * rather than risk inserting a field into something it doesn't
 * actually understand.
 *
 * Returns 1 on success (both out-params set), 0 otherwise.
 */
int cgtest_project_scan_optional_fields(const char *json, size_t length,
                                         int *out_has_msvc, int *out_has_single_translation_unit);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_PROJECT_H */
