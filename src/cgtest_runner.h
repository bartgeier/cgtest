/* cgtest_runner.h - implements CGTEST_ARG_RUN (see specification.md's
 * "-c --config" section): discovers test_*.c files across a
 * CGTestConfig's test_directories, scans each for test_ functions
 * (ctestscanner.h), generates cgtest-runner.c, compiles it alongside
 * the config's own source_files, and executes the result.
 *
 * The generated runner #includes each discovered test_*.c file by its
 * bare filename directly rather than compiling it as its own
 * translation unit and declaring its test_ functions extern - one
 * short line per file instead of one per function, and the compiler
 * already has each function's real definition in scope by the time
 * main() calls it, so no forward declaration is needed. A bare
 * filename only resolves because cgtest_runner_run() adds every
 * test_directories entry as its own -I to the compile command
 * (see cgtest_runner_run()'s duplicate-basename check below for why
 * that's still safe). Two tradeoffs worth knowing:
 *  - every included file shares one translation unit, so two test
 *    files defining a same-named helper (not just a test_ function)
 *    would collide - acceptable here since cgtest's own test_*.c
 *    convention keeps files self-contained;
 *  - two test files with the same basename in different
 *    test_directories would otherwise resolve ambiguously (whichever
 *    -I the compiler searches first) - cgtest_runner_run() rejects
 *    this case outright instead of risking a silently-wrong include.
 *
 * Like cgtest_config.h, the source-generation step is split out as a
 * pure function (cgtest_runner_generate_source()) so it's unit-
 * testable without touching the filesystem or a real compiler -
 * cgtest_runner_run() is the disk- and process-facing entry point
 * built on top of it.
 */
#ifndef CGTEST_RUNNER_H
#define CGTEST_RUNNER_H

#include "cgtest_config.h"
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
 * Pure - performs no filesystem access itself (though the #include
 * lines it emits will be resolved once the result is compiled).
 * Returns a malloc'd, NUL-terminated string the caller owns (free()
 * it); returns NULL only on allocation failure.
 */
char *cgtest_runner_generate_source(const CGTestRunnerFile *files, size_t file_count, const char *compile_command);

typedef struct {
    int   ok;         /* 0 = failed before a runner binary could be produced/run; see error */
    char *error;      /* malloc'd human-readable message, non-NULL only if !ok */
    int   exit_code;  /* the compiled cgtest-runner binary's exit code; valid only if ok */
} CGTestRunResult;

/* Runs "config" end to end:
 *  1. ctestfiles_scan() every entry in config->test_directories, in order.
 *  2. ctestscanner_find() every discovered test_*.c file's content.
 *  3. cgtest_runner_generate_source() the result (embedding the compile
 *     command from step 4 as a leading comment) into
 *     config->output_path/cgtest-runner.c (creating output_path if it
 *     doesn't exist yet, same as cgtest_create_run()'s directory
 *     handling).
 *  4. Compile it there via config->compiler_command, config->include_paths,
 *     and config->source_files, plus a -I for every test_directories
 *     entry (so cgtest-runner.c's bare-filename #include lines resolve) -
 *     every discovered test file is pulled in that way, not compiled
 *     separately.
 *  5. Execute the resulting binary.
 *
 * Before generating the source, any two test_*.c files across
 * different test_directories that share a basename are rejected as an
 * error (see cgtest_runner_generate_source()'s header comment for why
 * that combination would otherwise be ambiguous).
 *
 * A failure in any of steps 1-4 is reported as !ok with a message;
 * step 5's exit code (whatever the test run itself decided) is always
 * reported via exit_code, ok or not.
 */
CGTestRunResult cgtest_runner_run(const CGTestConfig *config);

/* Releases every owned field in "result". Safe to call regardless of ok. */
void cgtest_runner_free(CGTestRunResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_RUNNER_H */
