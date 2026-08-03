/* cgtest_runner.h - implements CGTEST_ARG_RUN (see specification.md's
 * "-r --run" section): discovers test_*.c files across a
 * CGTestProject's test_directories, scans each for test_ functions
 * (ctestscanner.h), generates cgtest-runner.c, compiles it alongside
 * the project's own source_files, and executes the result.
 *
 * The generated runner #includes each discovered test_*.c file by its
 * bare filename directly rather than compiling it as its own
 * translation unit and declaring its test_ functions extern - one
 * short line per file instead of one per function, and the compiler
 * already has each function's real definition in scope by the time
 * main() calls it, so no forward declaration is needed. A bare
 * filename only resolves because cgtest_runner_run() adds every
 * test_directories entry as its own include flag to the compile
 * command (see cgtest_runner_run()'s duplicate-basename check below
 * for why that's still safe). Two tradeoffs worth knowing:
 *  - every included file shares one translation unit, so two test
 *    files defining a same-named helper (not just a test_ function)
 *    would collide - acceptable here since cgtest's own test_*.c
 *    convention keeps files self-contained;
 *  - two test files with the same basename in different
 *    test_directories would otherwise resolve ambiguously (whichever
 *    include path the compiler searches first) - cgtest_runner_run()
 *    rejects this case outright instead of risking a silently-wrong
 *    include.
 *
 * Like cgtest_project.h, the source-generation step is split out as a
 * pure function (cgtest_runner_generate_source()) so it's unit-
 * testable without touching the filesystem or a real compiler -
 * cgtest_runner_run() is the disk- and process-facing entry point
 * built on top of it.
 */
#ifndef CGTEST_RUNNER_H
#define CGTEST_RUNNER_H

#include "cgtest_project.h"
#include "ctestscanner.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One test_*.c file, already scanned for its test_ functions in the
 * order they appear (see ctestscanner_find()). Non-owning - "label"
 * and "functions" must outlive the cgtest_runner_generate_source()
 * call that reads them. "label" is the file's full path; only its
 * basename (everything after the last '/') ends up in the generated
 * #include line - the full path still matters for error messages and
 * for cgtest_runner_run()'s duplicate-basename check.
 */
typedef struct {
    const char    *label;
    CTestFunction *functions;
    size_t         function_count;
} CGTestRunnerFile;

/* Builds cgtest-runner.c's full source text: a leading comment
 * embedding "compile_command" verbatim (the exact command
 * cgtest_runner_run() is about to invoke to compile this very file -
 * purely informational, so anyone inspecting or manually re-running
 * cgtest-runner.c can see it without digging through cgtest.exe's own
 * output), then #include "<basename of files[i].label>" for every
 * file (in array order), then a generated main() that calls every
 * discovered function in that same file order (and within a file, in
 * ctestscanner_find()'s order), printing PASS/FAIL per test and a
 * final summary, exiting nonzero iff any test failed.
 *
 * A function whose CTestFunction::fixture_type is non-NULL (see
 * specification.md ch.6 "Fixtures") is called wrapped in its own
 * block instead of a bare "test_<name>();":
 *
 *     {
 *         <fixture_type> state;
 *         setup_<name>(&state);
 *         if (!cgtest_fatal_failed) {
 *             test_<name>(&state);
 *         }
 *         teardown_<name>(&state);   // only if CTestFunction::has_teardown
 *     }
 *
 * cgtest_fatal_failed (set only by ASSERT_*, unlike cgtest_failed which
 * both EXPECT_* and ASSERT_* set - see cgtest.h) is reset to 0 alongside
 * cgtest_failed right before this block; if setup_<name> hit a fatal
 * failure, *state may be only partially initialized, so test_<name> is
 * skipped rather than run against it - matching GoogleTest's own
 * SetUp()/TestBody() behavior. teardown_<name>(&state) is emitted only
 * when CTestFunction::has_teardown is set - unlike setup_<name>, a
 * fixture with nothing to release just omits it (specification.md ch.6),
 * rather than requiring the author to write a no-op function.
 *
 * "<name>" is "test_<name>" with its "test_" prefix stripped. Callers
 * are expected to have already verified setup_<name> exists and set
 * has_teardown accordingly (see cgtest_runner_run()) - this function
 * itself performs no such check, since it's pure and has no way to
 * fail short of OOM.
 *
 * Pure - performs no filesystem access itself (though the #include
 * lines it emits will be resolved once the result is compiled).
 * Returns a malloc'd, NUL-terminated string the caller owns (free()
 * it); returns NULL only on allocation failure.
 */
char *cgtest_runner_generate_source(const CGTestRunnerFile *files, size_t file_count, const char *compile_command);

/* Builds the full compiler invocation for "project": project->compiler_command
 * verbatim, then an include flag for every include_paths and
 * test_directories entry, then every source_files entry, then
 * "runner_c_path" itself, then the flag(s) naming "runner_bin_path" as
 * the output. Two flag dialects, chosen by project->msvc: GCC/Clang's
 * "-I\"path\"" and "-o \"path\"" (msvc == 0, the default), or MSVC
 * cl.exe's "/I\"path\"" and "/Fe:\"path\"" (msvc != 0) - cl.exe accepts
 * neither "-I" nor "-o", so a plain compiler_command change alone can't
 * target it.
 *
 * Pure - performs no filesystem access itself. Returns a malloc'd,
 * NUL-terminated string the caller owns (free() it); returns NULL only
 * on allocation failure.
 */
char *cgtest_runner_build_compile_command(const CGTestProject *project,
                                           const char *runner_c_path, const char *runner_bin_path);

typedef struct {
    int    ok;          /* 0 = failed before a runner binary could be produced/run; see error */
    char  *error;       /* malloc'd human-readable message, non-NULL only if !ok */
    int    exit_code;   /* the compiled cgtest-runner binary's exit code; valid only if ok */

    /* Wall-clock milliseconds per phase (see ctimer.h), always
     * measured regardless of the -t/--time flag - printing them (only
     * when that flag was given) is cgtest_main.c's job, the one place
     * allowed to write to stdout/stderr (see its own header comment).
     * A phase never reached because an earlier one failed stays 0.0;
     * total_ms is the actual wall time from cgtest_runner_run()'s
     * entry to its return, which may exceed the sum of the four below
     * if the failure happened mid-phase (e.g. partway through file
     * discovery, before "scan" is considered complete). */
    double scan_ms;       /* ctestfiles_scan()/ctestscanner_find() + fixture/duplicate-basename validation */
    double generate_ms;   /* cgtest_runner_generate_source() + writing cgtest-runner.c to disk */
    double compile_ms;    /* the compiler invocation */
    double run_ms;        /* executing the compiled cgtest-runner binary */
    double total_ms;
} CGTestRunResult;

/* Runs "project" end to end:
 *  1. ctestfiles_scan() every entry in project->test_directories, in order.
 *  2. ctestscanner_find() every discovered test_*.c file's content.
 *  3. cgtest_runner_generate_source() the result (embedding the compile
 *     command from step 4 as a leading comment) into
 *     project->output_path/cgtest-runner.c (creating output_path if it
 *     doesn't exist yet, same as cgtest_create_run()'s directory
 *     handling).
 *  4. Compile it there via cgtest_runner_build_compile_command(): project->
 *     compiler_command, project->include_paths, and project->source_files,
 *     plus an include flag for every test_directories entry (so
 *     cgtest-runner.c's bare-filename #include lines resolve) - every
 *     discovered test file is pulled in that way, not compiled
 *     separately.
 *  5. Execute the resulting binary.
 *
 * Before generating the source, any two test_*.c files across
 * different test_directories that share a basename are rejected as an
 * error (see cgtest_runner_generate_source()'s header comment for why
 * that combination would otherwise be ambiguous). Likewise, every
 * discovered test function with a fixture parameter (CTestFunction::
 * fixture_type != NULL) must have a setup_<name> identifier present
 * somewhere among the discovered test files - existence-only, not a
 * signature check (see specification.md ch.6 "Validation before
 * invoking the compiler") - otherwise it is rejected as an error too,
 * rather than surfacing as a raw linker error once the compiler runs.
 * teardown_<name> is optional: its presence is checked the same way,
 * but a missing one is not an error - CTestFunction::has_teardown is
 * simply set to whether it was found, for cgtest_runner_generate_source()
 * to act on.
 *
 * A failure in any of steps 1-4 is reported as !ok with a message;
 * step 5's exit code (whatever the test run itself decided) is always
 * reported via exit_code, ok or not.
 */
CGTestRunResult cgtest_runner_run(const CGTestProject *project);

/* Releases every owned field in "result". Safe to call regardless of ok. */
void cgtest_runner_free(CGTestRunResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_RUNNER_H */
