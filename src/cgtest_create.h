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
    int   ok;                 /* 0 = failed; see "error" */
    char *dir;                /* malloc'd absolute path to the "cgtest" directory, non-NULL only if ok */
    char *error;              /* malloc'd human-readable message, non-NULL only if !ok */

    /* Each is 1 if this call actually wrote that file, 0 if it already
     * existed and was left untouched - see cgtest_create_run()'s header
     * comment. Valid only if ok. */
    int   wrote_project;      /* cgtest-project.json */
    int   wrote_header;       /* cgtest.h */
    int   wrote_test_macros;  /* test_cgtest_macros.c */

    /* 1 if cgtest-project.json already existed (wrote_project == 0)
     * but was missing one or more optional fields (msvc,
     * single_translation_unit) that got patched in with their default
     * value - see cgtest_create_run()'s header comment. Always 0 when
     * wrote_project == 1 (nothing to patch into a file just written
     * fresh from the current template) or when nothing was missing.
     * Valid only if ok. */
    int   patched_project;

    /* 1 if cgtest-project.json already existed (wrote_project == 0)
     * but couldn't be checked for missing optional fields at all -
     * cgtest_project_scan_optional_fields() (cgtest_project.h)
     * couldn't make sense of its shape (invalid JSON, an unrecognized
     * key, etc.), so it was left completely untouched rather than
     * patched - never both this and patched_project at once. Distinct
     * from "nothing was missing" (patched_project == 0 with this also
     * 0) specifically so a caller isn't left thinking a broken file is
     * simply already up to date. Valid only if ok. */
    int   project_could_not_be_checked;
} CGTestCreateResult;

/* Ensures a template cgtest-project.json, cgtest.h, and
 * test_cgtest_macros.c exist inside "dir"'s "cgtest" child directory -
 * "dir" is always a directory, never a file path (e.g. "." creates
 * "./cgtest/cgtest-project.json", "./cgtest/cgtest.h", and
 * "./cgtest/test_cgtest_macros.c", never files directly in "."). This
 * nesting lets a project's own test files #include "cgtest/cgtest.h"
 * - the same gtest/gtest.h-style layout GoogleTest users already know
 * - instead of a bare cgtest.h competing with the project's own
 * headers at its root. Both "dir" and "dir/cgtest" are created if they
 * don't exist yet, along with any missing parent directories (like
 * "mkdir -p" - e.g. "cgtest --init foo/bar" works even if "foo"
 * doesn't exist yet either).
 *
 * Each of the three files is checked and written independently: a
 * missing one is created from the current template (the one baked
 * into this binary - see CGTestCreateResult::wrote_project/
 * wrote_header/wrote_test_macros to tell which), an already-existing
 * one is left completely untouched, never overwritten - whether it's
 * an unmodified older version or something the developer edited by
 * hand. This makes cgtest_create_run() safe (and idempotent) to call
 * again on an already-initialized "dir/cgtest": nothing errors just
 * because cgtest-project.json is already there, unlike before this
 * per-file check existed. In particular, deleting only cgtest.h (e.g.
 * to pick up a fix from a newer cgtest.exe - it never carries per-
 * project customization the way cgtest-project.json's compiler_command/
 * include_paths/etc. do) and re-running cgtest_create_run() regenerates
 * just that file, leaving cgtest-project.json and test_cgtest_macros.c
 * exactly as they were.
 *
 * An already-existing cgtest-project.json gets one more thing besides
 * "left completely untouched" or "written fresh": if a newer
 * cgtest.exe has grown an optional field (msvc, single_translation_unit)
 * that predates the file (e.g. --init was originally run with an older
 * cgtest.exe), that field is patched into the existing file with its
 * default value - see CGTestCreateResult::patched_project and
 * cgtest_project_scan_optional_fields() (cgtest_project.h) for the
 * detection and specification.md's "--init" section for the full
 * rationale. Every existing byte (values, formatting, field order) is
 * left exactly as it was; the missing field(s) are appended just
 * before the closing "}". Left alone entirely - not patched, not an
 * error - if the file doesn't parse as valid JSON, since inserting
 * into something whose shape isn't actually understood is worse than
 * doing nothing; CGTestCreateResult::project_could_not_be_checked
 * distinguishes that case from "nothing was missing" (both otherwise
 * report wrote_project == 0, patched_project == 0), so a caller isn't
 * left thinking a broken file is simply already up to date. This never
 * applies to the required fields
 * (compiler_command/include_paths/source_files/output_path/
 * test_directories, still true after "cgtest-project.json" above) -
 * none of them has a sensible project-agnostic default, unlike a
 * boolean flag defaulting to its old, pre-flag behavior.
 */
CGTestCreateResult cgtest_create_run(const char *dir);

/* Releases every owned field in "result". Safe to call on a failed
 * (ok == 0) result too. */
void cgtest_create_free(CGTestCreateResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_CREATE_H */
