/* cgtest_runner.h - implements CGTEST_ARG_RUN (see specification.md's
 * "-r --run" section): discovers test_*.c files across a
 * CGTestProject's test_directories, scans each for test_ functions
 * (ctestscanner.h), generates cgtest-runner.c, compiles it alongside
 * the project's own source_files and every discovered test_*.c file
 * (each as its own translation unit - see below), and executes the
 * result.
 *
 * The generated runner never #includes a discovered test_*.c file -
 * it only "extern"-declares the functions it needs to call
 * (test_<name>, setup_<name>, and, if present, teardown_<name>), and
 * cgtest_runner_build_compile_command() passes every test file to the
 * compiler as its own separate source argument, alongside
 * cgtest-runner.c itself. This means a static helper or global defined
 * in one test file can no longer collide with a same-named one in
 * another - each file keeps its own translation unit's scope, the
 * normal C rule, rather than sharing one the way #include would force.
 * A fixture's type (specification.md ch.6) still needs to be knowable
 * to cgtest-runner.c despite this - see cgtest_runner_generate_source()
 * for how that's resolved without ever needing the type's full
 * definition there.
 *
 * Two test files sharing a basename across different test_directories
 * is still rejected outright by cgtest_runner_run() (see its
 * duplicate-basename check) even though bare-filename #include
 * resolution is no longer the reason - MSVC's cl.exe otherwise names
 * each source file's object file after its own basename by default,
 * so two same-named files from different directories in one compile
 * invocation would silently collide.
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
 * call that reads them. "label" is the file's full path, used both for
 * error messages and as the actual source argument
 * cgtest_runner_build_compile_command() passes to the compiler
 * (cgtest_runner_generate_source() itself only ever uses its
 * basename, for the "== <file> ==" header it prints per file).
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
 * output), then one "typedef struct T T;" forward declaration per
 * distinct fixture type across every file (deduplicated - see below),
 * then an "extern" declaration for every discovered function, then a
 * generated main() that calls every discovered function in file order
 * (and within a file, in ctestscanner_find()'s order), printing
 * PASS/FAIL per test and a final summary, exiting nonzero iff any test
 * failed.
 *
 * Every discovered test_*.c file is its own translation unit (see
 * cgtest_runner_build_compile_command()) - this function never
 * #includes one, it only declares what it needs from it:
 *
 *     extern void test_foo(void);                 // plain (void) test
 *
 *     extern void setup_bar(State **state);        // fixture test (see below)
 *     extern void test_bar(State *state);
 *     extern void teardown_bar(State *state);       // only if CTestFunction::has_teardown
 *
 * A function whose CTestFunction::fixture_type is non-NULL (see
 * specification.md ch.6 "Fixtures") is called wrapped in its own
 * block instead of a bare "test_<name>();":
 *
 *     {
 *         State *state = NULL;
 *         setup_<name>(&state);
 *         if (!cgtest_fatal_failed) {
 *             test_<name>(state);
 *         }
 *         teardown_<name>(state);   // only if CTestFunction::has_teardown
 *     }
 *
 * "State" is forward-declared as an incomplete type ("typedef struct
 * State State;", deduplicated across every use of the same type name)
 * rather than #include'd or copied in full - this function only ever
 * holds/passes a "State *"/"State **" here, never allocates or
 * dereferences one by value, so it never needs the real definition.
 * setup_<name> takes "State **" (an out-param) rather than returning
 * "State *" specifically so it stays void-returning - EXPECT_* and
 * ASSERT_* (see cgtest.h) work inside it exactly like they do in
 * test_<name>, including ASSERT_*'s early "return;", which wouldn't
 * type-check in a function declared to return "State *". "state"
 * starts NULL and is populated by setup_<name> itself; if setup_<name>
 * hits a fatal (ASSERT_*) failure before assigning it, it stays the
 * well-defined NULL it started as - safe to skip in the test_<name>
 * call above and safe to pass to a present teardown_<name>. cgtest
 * itself never allocates or frees "state" - only the author's
 * setup_<name>/teardown_<name> do, and it's fine for neither to free
 * it: the runner process exits shortly after the last test either way.
 *
 * cgtest_fatal_failed (set only by ASSERT_*, unlike cgtest_failed which
 * both EXPECT_* and ASSERT_* set - see cgtest.h) is reset to 0 alongside
 * cgtest_failed right before this block.
 *
 * "<name>" is "test_<name>" with its "test_" prefix stripped. Callers
 * are expected to have already verified setup_<name> exists and set
 * has_teardown accordingly (see cgtest_runner_run()) - this function
 * itself performs no such check, since it's pure and has no way to
 * fail short of OOM.
 *
 * Pure - performs no filesystem access itself. Returns a malloc'd,
 * NUL-terminated string the caller owns (free() it); returns NULL only
 * on allocation failure.
 */
char *cgtest_runner_generate_source(const CGTestRunnerFile *files, size_t file_count, const char *compile_command);

/* Builds the full compiler invocation for "project": project->compiler_command
 * verbatim, then an include flag for every include_paths and
 * test_directories entry, then every source_files entry, then every
 * files[i].label (each discovered test_*.c file, compiled as its own
 * translation unit - see cgtest_runner_generate_source()'s header
 * comment for why), then "runner_c_path" itself, then the flag(s)
 * naming "runner_bin_path" as the output. Two flag dialects, chosen by
 * project->msvc: GCC/Clang's "-I\"path\"" and "-o \"path\"" (msvc == 0,
 * the default), or MSVC cl.exe's "/I\"path\"" and "/Fe:\"path\""
 * (msvc != 0) - cl.exe accepts neither "-I" nor "-o", so a plain
 * compiler_command change alone can't target it.
 *
 * Pure - performs no filesystem access itself. Returns a malloc'd,
 * NUL-terminated string the caller owns (free() it); returns NULL only
 * on allocation failure.
 */
char *cgtest_runner_build_compile_command(const CGTestProject *project, const CGTestRunnerFile *files, size_t file_count,
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
 *     compiler_command, project->include_paths, project->source_files,
 *     an include flag for every test_directories entry, and every
 *     discovered test_*.c file passed as its own source argument
 *     (compiled as its own translation unit, alongside cgtest-runner.c
 *     itself).
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
