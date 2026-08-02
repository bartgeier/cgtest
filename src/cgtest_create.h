/* cgtest_create.h - see specification.md's "--init" example: writes a
 * template cgtest-project.json, a cgtest.h (EXPECT_TRUE/EXPECT_FALSE/
 * ASSERT_TRUE/ASSERT_FALSE; the EXPECT_EQ_/EXPECT_NE_ family for INT/
 * UINT/FLOAT/DOUBLE/PTR/STR/STR_NOCASE; EXPECT_NEAR_DOUBLE; the
 * EXPECT_LT_/LE_/GT_/GE_ family for INT/UINT/FLOAT/DOUBLE; their
 * ASSERT_ counterparts; and the cgtest_failed flag the generated
 * runner reads), and a test_cgtest_macros.c with one example test per
 * macro from that header, into "dir"'s "cgtest" child directory.
 * cgtest-project.json's default test_directories already includes ".",
 * so "cgtest --run dir/cgtest" discovers and runs that example
 * immediately - a new project has something that actually passes out
 * of the box, not just files to edit.
 *
 * All three templates are baked into the cgtest binary as string
 * constants rather than shipped as separate files on disk - there is
 * no reliable, portable way to locate a resource file relative to the
 * running executable (install layouts differ across Windows/Linux and
 * between running from a build dir vs. an installed one), so a
 * compiled-in constant sidesteps the problem entirely.
 */
#ifndef CGTEST_CREATE_H
#define CGTEST_CREATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   ok;      /* 0 = failed; see "error" */
    char *dir;     /* malloc'd absolute path to the created "cgtest" directory, non-NULL only if ok */
    char *error;   /* malloc'd human-readable message, non-NULL only if !ok */
} CGTestCreateResult;

/* Creates a template cgtest-project.json, cgtest.h, and
 * test_cgtest_macros.c inside "dir"'s "cgtest" child directory - "dir"
 * is always a directory, never a file path (e.g. "." creates
 * "./cgtest/cgtest-project.json", "./cgtest/cgtest.h", and
 * "./cgtest/test_cgtest_macros.c", never files directly in "."). This
 * nesting lets a project's own test files #include "cgtest/cgtest.h"
 * - the same gtest/gtest.h-style layout GoogleTest users already know
 * - instead of a bare cgtest.h competing with the project's own
 * headers at its root. Both "dir" and "dir/cgtest" are created if they
 * don't exist yet, along with any missing parent directories (like
 * "mkdir -p" - e.g. "cgtest --init foo/bar" works even if "foo"
 * doesn't exist yet either). Fails - without writing anything - if a
 * cgtest-project.json already exists in "dir/cgtest" ("If
 * cgtest-project.json already exist than error and exit cgtest.exe",
 * per specification.md).
 */
CGTestCreateResult cgtest_create_run(const char *dir);

/* Releases every owned field in "result". Safe to call on a failed
 * (ok == 0) result too. */
void cgtest_create_free(CGTestCreateResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_CREATE_H */
