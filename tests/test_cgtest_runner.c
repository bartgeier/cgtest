/* test_cgtest_runner.c - unit tests for cgtest_runner_generate_source(),
 * the pure part of cgtest_runner.h that turns already-discovered test
 * functions into cgtest-runner.c's source text. Compiling and running
 * the generated result is exercised end to end via examples/mathlib
 * instead (see README/specification.md), not here - matching
 * cgtest_project.h's split between the pure parser and its disk-facing
 * cgtest_project_load(). The exceptions are the test_run_*() tests
 * below that call cgtest_runner_run() directly: some (duplicate
 * basenames, missing setup) fail before any compiler is invoked; the
 * rest genuinely compile+run a tiny throwaway project, since some
 * behavior (has_teardown, real linking across separately-compiled test
 * files) can only be proven that way.
 *
 * Written in cgtest's own test convention (void test_<name>(void)); see
 * test_ctestscanner.c's header comment for why main() below dispatches
 * them manually instead of via a generated cgtest-runner.
 */
#include "cgtest_runner.h"
#include "cpath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define CGTEST_TEST_GETCWD _getcwd
#else
#include <unistd.h>
#define CGTEST_TEST_GETCWD getcwd
#endif

static int test_failed = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            test_failed = 1; \
            return; \
        } \
    } while (0)

void test_generates_valid_shell_with_no_files(void)
{
    char *source = cgtest_runner_generate_source(NULL, 0, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);

    CHECK(source != NULL);
    CHECK(strstr(source, "int main(void)") != NULL);
    CHECK(strstr(source, "total - failed") != NULL);
    CHECK(strstr(source, "extern") == NULL);

    free(source);
}

void test_declares_and_defines_shared_helpers_in_generated_runner(void)
{
    /* Companion to test_header_declares_shared_helpers_extern_not_static
     * in test_cgtest_create.c: cgtest.h only "extern"-declares
     * cgtest_relpath()/cgtest_print_str_field()/cgtest_strcasecmp() -
     * this is where their one, non-`static` definition actually lives,
     * unconditionally, regardless of file_count (same as cgtest_failed/
     * cgtest_fatal_failed just above them). Non-`static` is the whole
     * point: an unused `static` definition is what -Wunused-function
     * flags, and a plain external-linkage one never is, regardless of
     * whether anything in this translation unit happens to call it. */
    char *source = cgtest_runner_generate_source(NULL, 0, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);

    CHECK(source != NULL);
    CHECK(strstr(source, "const char *cgtest_relpath(const char *file)\n{") != NULL);
    CHECK(strstr(source, "void cgtest_print_str_field(const char *prefix, const char *s)\n{") != NULL);
    CHECK(strstr(source, "int cgtest_strcasecmp(const char *a, const char *b)\n{") != NULL);
    CHECK(strstr(source, "static const char *cgtest_relpath") == NULL);
    CHECK(strstr(source, "static void cgtest_print_str_field") == NULL);
    CHECK(strstr(source, "static int cgtest_strcasecmp") == NULL);

    free(source);
}

void test_embeds_the_compile_command_as_a_leading_comment(void)
{
    char *source = cgtest_runner_generate_source(NULL, 0, "gcc -std=c99 -I\"src\" -o cgtest-runner cgtest-runner.c", 0);
    const char *comment;
    const char *includes;

    CHECK(source != NULL);
    comment = strstr(source, "/* compile: gcc -std=c99 -I\"src\" -o cgtest-runner cgtest-runner.c */");
    includes = strstr(source, "#include <stdio.h>");
    CHECK(comment != NULL);
    CHECK(includes != NULL);
    CHECK(comment < includes);

    free(source);
}

void test_declares_extern_and_calls_its_function(void)
{
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;

    functions[0].name = "test_math_add";
    functions[0].fixture_type = NULL;
    functions[0].line = 3;

    files[0].label = "/abs/path/test_math.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);

    CHECK(source != NULL);
    /* Never #include'd - each test file is its own translation unit
     * (see cgtest_runner.h); only "extern"-declared here. */
    CHECK(strstr(source, "#include \"") == NULL);
    CHECK(strstr(source, "extern void test_math_add(void);") != NULL);
    CHECK(strstr(source, "/abs/path") == NULL);
    CHECK(strstr(source, "test_math_add()") != NULL);
    CHECK(strstr(source, "[       OK ]%s test_math_add") != NULL);
    CHECK(strstr(source, "[  FAILED  ]%s test_math_add") != NULL);
    CHECK(strstr(source, "== test_math.c ==") != NULL);

    free(source);
}

void test_header_uses_bare_basename_and_precedes_its_tests(void)
{
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;
    const char *header;
    const char *call;

    functions[0].name = "test_math_add";
    functions[0].fixture_type = NULL;
    functions[0].line = 3;

    files[0].label = "/abs/path/test_math.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    header = strstr(source, "== test_math.c ==");
    call = strstr(source, "test_math_add()");
    CHECK(header != NULL);
    CHECK(call != NULL);
    CHECK(header < call);

    free(source);
}

void test_skips_the_header_for_a_file_with_no_test_functions(void)
{
    CGTestRunnerFile files[1];
    char *source;

    files[0].label = "test_empty.c";
    files[0].functions = NULL;
    files[0].function_count = 0;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    /* No functions discovered in it - nothing to "extern" and no
     * "== test_empty.c ==" header either. */
    CHECK(strstr(source, "test_empty") == NULL);
    CHECK(strstr(source, "== test_empty.c ==") == NULL);

    free(source);
}

void test_preserves_function_order_within_a_file(void)
{
    CTestFunction functions[2];
    CGTestRunnerFile files[1];
    char *source;
    const char *first_call;
    const char *second_call;

    functions[0].name = "test_setup";
    functions[0].fixture_type = NULL;
    functions[0].line = 1;
    functions[1].name = "test_teardown";
    functions[1].fixture_type = NULL;
    functions[1].line = 9;

    files[0].label = "test_lifecycle.c";
    files[0].functions = functions;
    files[0].function_count = 2;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    first_call = strstr(source, "test_setup()");
    second_call = strstr(source, "test_teardown()");
    CHECK(first_call != NULL);
    CHECK(second_call != NULL);
    CHECK(first_call < second_call);

    free(source);
}

void test_preserves_file_order_across_files(void)
{
    CTestFunction fn_a[1];
    CTestFunction fn_b[1];
    CGTestRunnerFile files[2];
    char *source;
    const char *call_a;
    const char *call_b;

    fn_a[0].name = "test_from_a";
    fn_a[0].fixture_type = NULL;
    fn_a[0].line = 1;
    fn_b[0].name = "test_from_b";
    fn_b[0].fixture_type = NULL;
    fn_b[0].line = 1;

    files[0].label = "test_a.c";
    files[0].functions = fn_a;
    files[0].function_count = 1;
    files[1].label = "test_b.c";
    files[1].functions = fn_b;
    files[1].function_count = 1;

    source = cgtest_runner_generate_source(files, 2, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    call_a = strstr(source, "test_from_a()");
    call_b = strstr(source, "test_from_b()");
    CHECK(call_a != NULL);
    CHECK(call_b != NULL);
    CHECK(call_a < call_b);

    free(source);
}

void test_generates_setup_teardown_wrapper_for_fixture_function(void)
{
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;

    functions[0].name = "test_bar";
    functions[0].fixture_type = "State";
    functions[0].has_teardown = 1;
    functions[0].line = 3;

    files[0].label = "/abs/path/test_widget.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);

    CHECK(source != NULL);
    CHECK(strstr(source, "typedef struct State State;") != NULL);
    CHECK(strstr(source, "extern void setup_bar(State **state);") != NULL);
    CHECK(strstr(source, "extern void test_bar(State *state);") != NULL);
    CHECK(strstr(source, "extern void teardown_bar(State *state);") != NULL);
    CHECK(strstr(source, "State *state = NULL;") != NULL);
    CHECK(strstr(source, "setup_bar(&state);") != NULL);
    CHECK(strstr(source, "test_bar(state);") != NULL);
    CHECK(strstr(source, "teardown_bar(state);") != NULL);
    /* Not called bare, the way a (void) test would be. */
    CHECK(strstr(source, "test_bar();") == NULL);

    free(source);
}

void test_deduplicates_fixture_type_forward_declaration(void)
{
    /* Repeating "typedef struct State State;" once per test sharing
     * that fixture type would be a hard error under strict C89
     * -pedantic-errors (that redundant-typedef allowance is C11-only -
     * see cgtest_runner_generate_source()'s header comment) - it must
     * be forward-declared exactly once regardless of how many tests
     * (here: two, in two different files) use it. */
    CTestFunction fn_a[1];
    CTestFunction fn_b[1];
    CGTestRunnerFile files[2];
    char *source;
    const char *first;
    const char *second;

    fn_a[0].name = "test_bar";
    fn_a[0].fixture_type = "State";
    fn_a[0].has_teardown = 0;
    fn_a[0].line = 1;
    fn_b[0].name = "test_baz";
    fn_b[0].fixture_type = "State";
    fn_b[0].has_teardown = 0;
    fn_b[0].line = 1;

    files[0].label = "test_a.c";
    files[0].functions = fn_a;
    files[0].function_count = 1;
    files[1].label = "test_b.c";
    files[1].functions = fn_b;
    files[1].function_count = 1;

    source = cgtest_runner_generate_source(files, 2, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    first = strstr(source, "typedef struct State State;");
    CHECK(first != NULL);
    second = strstr(first + 1, "typedef struct State State;");
    CHECK(second == NULL);

    free(source);
}

void test_forward_declares_each_distinct_fixture_type_once(void)
{
    CTestFunction fn_a[1];
    CTestFunction fn_b[1];
    CGTestRunnerFile files[2];
    char *source;

    fn_a[0].name = "test_bar";
    fn_a[0].fixture_type = "State";
    fn_a[0].has_teardown = 0;
    fn_a[0].line = 1;
    fn_b[0].name = "test_baz";
    fn_b[0].fixture_type = "Counter";
    fn_b[0].has_teardown = 0;
    fn_b[0].line = 1;

    files[0].label = "test_a.c";
    files[0].functions = fn_a;
    files[0].function_count = 1;
    files[1].label = "test_b.c";
    files[1].functions = fn_b;
    files[1].function_count = 1;

    source = cgtest_runner_generate_source(files, 2, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    CHECK(strstr(source, "typedef struct State State;") != NULL);
    CHECK(strstr(source, "typedef struct Counter Counter;") != NULL);

    free(source);
}

void test_omits_teardown_call_when_has_teardown_is_unset(void)
{
    /* teardown_<name> is optional (specification.md ch.6) - when
     * CTestFunction::has_teardown is 0 (as set by cgtest_runner_run()
     * when no teardown_bar was found among the discovered test files),
     * the generated wrapper must not reference it at all, not even a
     * call (or extern declaration) for something that doesn't exist. */
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;

    functions[0].name = "test_bar";
    functions[0].fixture_type = "State";
    functions[0].has_teardown = 0;
    functions[0].line = 3;

    files[0].label = "test_widget.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);

    CHECK(source != NULL);
    CHECK(strstr(source, "State *state = NULL;") != NULL);
    CHECK(strstr(source, "setup_bar(&state);") != NULL);
    CHECK(strstr(source, "test_bar(state);") != NULL);
    CHECK(strstr(source, "teardown_bar") == NULL);

    free(source);
}

void test_fixture_test_call_is_guarded_by_fatal_failed_check(void)
{
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;
    const char *setup_call;
    const char *guard;
    const char *test_call;
    const char *teardown_call;

    functions[0].name = "test_bar";
    functions[0].fixture_type = "State";
    functions[0].has_teardown = 1;
    functions[0].line = 3;

    files[0].label = "test_widget.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    /* cgtest_fatal_failed (set only by ASSERT_*, see cgtest.h) is reset
     * alongside cgtest_failed before setup_bar runs, then checked right
     * after it - a fatal failure during setup_bar means test_bar is
     * skipped, but a present teardown_bar still always runs regardless
     * (specification.md ch.6). */
    CHECK(strstr(source, "cgtest_fatal_failed = 0;") != NULL);

    setup_call = strstr(source, "setup_bar(&state);");
    guard = strstr(source, "if (!cgtest_fatal_failed) {");
    test_call = strstr(source, "test_bar(state);");
    teardown_call = strstr(source, "teardown_bar(state);");

    CHECK(setup_call != NULL);
    CHECK(guard != NULL);
    CHECK(test_call != NULL);
    CHECK(teardown_call != NULL);
    CHECK(setup_call < guard);
    CHECK(guard < test_call);
    CHECK(test_call < teardown_call);

    free(source);
}

void test_fixture_wrapper_precedes_pass_fail_verdict(void)
{
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;
    const char *teardown_call;
    const char *verdict;

    functions[0].name = "test_bar";
    functions[0].fixture_type = "State";
    functions[0].has_teardown = 1;
    functions[0].line = 3;

    files[0].label = "test_widget.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);

    teardown_call = strstr(source, "teardown_bar(state);");
    verdict = strstr(source, "if (!cgtest_failed)");
    CHECK(teardown_call != NULL);
    CHECK(verdict != NULL);
    CHECK(teardown_call < verdict);

    free(source);
}

void test_single_translation_unit_includes_each_file_before_its_extern_declarations(void)
{
    /* single_translation_unit=1 (specification.md ch.6 "Single-
     * translation-unit mode") still emits the same "extern" declaration
     * as the default mode, but the "#include" line for each file comes
     * first, not after: extern-declaring a function ahead of its real
     * (#include'd) definition is fine either order, but a fixture
     * type's forward declare ahead of its real definition is not (see
     * test_single_translation_unit_skips_fixture_forward_declare below)
     * - #include has to come first so that block can be skipped
     * entirely instead of reordered around. */
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;
    const char *extern_decl;
    const char *include_line;
    const char *main_fn;

    functions[0].name = "test_math_add";
    functions[0].fixture_type = NULL;
    functions[0].line = 3;

    files[0].label = "/abs/path/test_math.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 1);
    CHECK(source != NULL);

    extern_decl = strstr(source, "extern void test_math_add(void);");
    include_line = strstr(source, "#include \"/abs/path/test_math.c\"");
    main_fn = strstr(source, "int main(void)");
    CHECK(extern_decl != NULL);
    CHECK(include_line != NULL);
    CHECK(main_fn != NULL);
    CHECK(include_line < extern_decl);
    CHECK(extern_decl < main_fn);

    free(source);
}

void test_default_mode_never_includes_test_files(void)
{
    /* Companion to the test above: with single_translation_unit left at
     * its default (0), no "#include" line for a discovered test file is
     * emitted at all - only the "extern" declaration. */
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;

    functions[0].name = "test_math_add";
    functions[0].fixture_type = NULL;
    functions[0].line = 3;

    files[0].label = "/abs/path/test_math.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 0);
    CHECK(source != NULL);
    CHECK(strstr(source, "#include \"/abs/path/test_math.c\"") == NULL);

    free(source);
}

void test_single_translation_unit_skips_fixture_forward_declare(void)
{
    /* Regression test: single_translation_unit=1 must NOT also emit
     * "typedef struct State State;" the way the default mode does (see
     * test_generates_setup_teardown_wrapper_for_fixture_function) - by
     * the time this line would be emitted, the #include block already
     * provided State's real, complete definition (see
     * test_single_translation_unit_includes_each_file_before_its_extern_
     * declarations), and a second, incomplete forward declaration of the
     * same typedef name in the same translation unit is a hard error
     * under -pedantic-errors (found by actually compiling
     * examples/mathlib with single_translation_unit=true - it has
     * exactly this shape: a fixture type, discovered via a real
     * "test_bar(State *state)" signature). The "extern" declarations
     * for setup_bar/test_bar themselves are still expected, same as
     * every other mode/fixture combination - only the forward declare is
     * mode-specific. */
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;

    functions[0].name = "test_bar";
    functions[0].fixture_type = "State";
    functions[0].has_teardown = 0;
    functions[0].line = 3;

    files[0].label = "/abs/path/test_widget.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c", 1);
    CHECK(source != NULL);

    CHECK(strstr(source, "typedef struct State State;") == NULL);
    CHECK(strstr(source, "#include \"/abs/path/test_widget.c\"") != NULL);
    CHECK(strstr(source, "extern void setup_bar(State **state);") != NULL);
    CHECK(strstr(source, "extern void test_bar(State *state);") != NULL);

    free(source);
}

#define FIXTURE_DIR "build/cgtest_runner_fixture"

static void write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    fputs(content, f);
    fclose(f);
}

/* Every other test_run_*() test below registers test_directories with a
 * relative literal (e.g. FIXTURE_DIR) directly, which is fine in
 * separate-TU mode: each discovered file is passed to the compiler as
 * its own command-line source argument, resolved against the compiler
 * process's own cwd regardless. single_translation_unit=1 instead
 * "#include"s each file's label from inside cgtest-runner.c
 * (cgtest_runner.h) - a relative label there resolves against
 * cgtest-runner.c's own directory (the normal quoted-#include rule),
 * not the invoking process's cwd, so it needs the same absolute-path
 * guarantee cgtest_project_load() provides in real use. Only the two
 * single-TU test_run_*() tests below need this; the rest keep using
 * plain relative literals like every test above them. */
static void register_absolute_test_directory(CPathList *test_directories, const char *relative_dir)
{
    char cwd[4096];
    CGTEST_TEST_GETCWD(cwd, sizeof(cwd));
    cpathlist_register(test_directories, cwd, relative_dir);
}

void test_build_compile_command_uses_gcc_flags_by_default(void)
{
    CGTestProject project;
    CGTestRunnerFile files[1];
    char *cmd;

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.include_paths, "", "src");
    cpathlist_register(&project.source_files, "", "src/mathlib.c");
    cpathlist_register(&project.test_directories, "", "tests");

    files[0].label = "tests/test_math.c";
    files[0].functions = NULL;
    files[0].function_count = 0;

    cmd = cgtest_runner_build_compile_command(&project, files, 1, "build/cgtest-runner.c", "build/cgtest-runner");

    CHECK(cmd != NULL);
    CHECK(strstr(cmd, "-I\"src\"") != NULL);
    CHECK(strstr(cmd, "-I\"tests\"") != NULL);
    CHECK(strstr(cmd, "\"src/mathlib.c\"") != NULL);
    CHECK(strstr(cmd, "\"tests/test_math.c\"") != NULL);
    CHECK(strstr(cmd, "-o \"build/cgtest-runner\"") != NULL);
    CHECK(strstr(cmd, "/I") == NULL);
    CHECK(strstr(cmd, "/Fe") == NULL);

    free(cmd);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
}

void test_build_compile_command_uses_msvc_flags_when_configured(void)
{
    CGTestProject project;
    CGTestRunnerFile files[1];
    char *cmd;

    memset(&project, 0, sizeof(project));
    project.compiler_command = "cl /TC /W4";
    project.msvc = 1;
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.include_paths, "", "src");
    cpathlist_register(&project.source_files, "", "src/mathlib.c");
    cpathlist_register(&project.test_directories, "", "tests");

    files[0].label = "tests/test_math.c";
    files[0].functions = NULL;
    files[0].function_count = 0;

    cmd = cgtest_runner_build_compile_command(&project, files, 1, "build/cgtest-runner.c", "build/cgtest-runner.exe");

    CHECK(cmd != NULL);
    CHECK(strstr(cmd, "/I\"src\"") != NULL);
    CHECK(strstr(cmd, "/I\"tests\"") != NULL);
    CHECK(strstr(cmd, "\"src/mathlib.c\"") != NULL);
    CHECK(strstr(cmd, "\"tests/test_math.c\"") != NULL);
    CHECK(strstr(cmd, "/Fe:\"build/cgtest-runner.exe\"") != NULL);
    CHECK(strstr(cmd, "-I") == NULL);
    CHECK(strstr(cmd, " -o ") == NULL);

    free(cmd);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
}

void test_build_compile_command_with_no_test_files(void)
{
    CGTestProject project;
    char *cmd;

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);

    cmd = cgtest_runner_build_compile_command(&project, NULL, 0, "build/cgtest-runner.c", "build/cgtest-runner");

    CHECK(cmd != NULL);
    CHECK(strstr(cmd, "\"build/cgtest-runner.c\"") != NULL);

    free(cmd);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
}

void test_build_compile_command_omits_test_files_as_separate_sources_when_single_translation_unit(void)
{
    /* single_translation_unit=1 (specification.md ch.6): every
     * discovered test file's code already reached cgtest-runner.c via
     * the "#include" lines cgtest_runner_generate_source() emits, so
     * this function must not also pass it as its own source argument -
     * that would compile it twice. */
    CGTestProject project;
    CGTestRunnerFile files[1];
    char *cmd;

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    project.single_translation_unit = 1;
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.test_directories, "", "tests");

    files[0].label = "tests/test_math.c";
    files[0].functions = NULL;
    files[0].function_count = 0;

    cmd = cgtest_runner_build_compile_command(&project, files, 1, "build/cgtest-runner.c", "build/cgtest-runner");

    CHECK(cmd != NULL);
    CHECK(strstr(cmd, "\"tests/test_math.c\"") == NULL);
    CHECK(strstr(cmd, "\"build/cgtest-runner.c\"") != NULL);
    CHECK(strstr(cmd, "-I\"tests\"") != NULL);

    free(cmd);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
}

void test_run_rejects_duplicate_basenames_across_directories(void)
{
    CGTestProject project;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/dir_a", 0755);
    mkdir(FIXTURE_DIR "/dir_b", 0755);
    write_file(FIXTURE_DIR "/dir_a/test_dup.c", "void test_from_a(void) { }\n");
    write_file(FIXTURE_DIR "/dir_b/test_dup.c", "void test_from_b(void) { }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99"; /* never actually invoked - see below */
    project.output_path = FIXTURE_DIR "/build";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.test_directories, "", FIXTURE_DIR "/dir_a");
    cpathlist_register(&project.test_directories, "", FIXTURE_DIR "/dir_b");

    result = cgtest_runner_run(&project);

    CHECK(!result.ok);
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "duplicate") != NULL);
    CHECK(strstr(result.error, "test_dup.c") != NULL);

    cgtest_runner_free(&result);
    /* Not cgtest_project_free(): compiler_command/output_path above are
     * string literals, not malloc'd, so freeing "project" the normal
     * way would be undefined behavior. */
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/dir_a/test_dup.c");
    remove(FIXTURE_DIR "/dir_b/test_dup.c");
    remove(FIXTURE_DIR "/dir_a");
    remove(FIXTURE_DIR "/dir_b");
    remove(FIXTURE_DIR);
}

void test_run_rejects_fixture_test_missing_setup_function(void)
{
    CGTestProject project;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/missing_setup", 0755);
    write_file(FIXTURE_DIR "/missing_setup/test_widget.c",
        "typedef struct State { int x; } State;\n"
        "void teardown_bar(State *state) { (void)state; }\n"
        "void test_bar(State *state) { (void)state; }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99"; /* never actually invoked - see below */
    project.output_path = FIXTURE_DIR "/missing_setup/build";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.test_directories, "", FIXTURE_DIR "/missing_setup");

    result = cgtest_runner_run(&project);

    CHECK(!result.ok);
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "missing") != NULL);
    CHECK(strstr(result.error, "setup_bar") != NULL);

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/missing_setup/test_widget.c");
    remove(FIXTURE_DIR "/missing_setup");
    remove(FIXTURE_DIR);
}

void test_run_succeeds_with_fixture_test_missing_teardown_function(void)
{
    /* Unlike setup_bar, teardown_bar is optional (specification.md
     * ch.6): a fixture with nothing to release doesn't require the
     * author to write a no-op function - here, cgtest never frees
     * *state either (the process exits right after; see
     * cgtest_runner.h), so there is genuinely nothing that must run.
     * This test runs the full pipeline for real - not just far enough
     * to hit a validation error, the way the duplicate-basename/
     * missing-setup tests do - to prove the generated runner actually
     * compiles and runs without a teardown_bar in sight, and that
     * setup_bar's calloc'd value correctly reaches test_bar through
     * the "State **" out-param (see the "value_ok" marker file below -
     * proof beyond just a nonzero exit code, since these bare test
     * files don't use cgtest.h's EXPECT_* and ASSERT_* macros at all). */
    CGTestProject project;
    CGTestRunResult result;
    FILE *marker;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/missing_teardown", 0755);
    write_file(FIXTURE_DIR "/missing_teardown/test_widget.c",
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "typedef struct State { int x; } State;\n"
        "void setup_bar(State **state) {\n"
        "    *state = calloc(1, sizeof(State));\n"
        "    (*state)->x = 42;\n"
        "}\n"
        "void test_bar(State *state) {\n"
        "    if (state->x == 42) {\n"
        "        FILE *f = fopen(\"" FIXTURE_DIR "/missing_teardown/value_ok\", \"w\");\n"
        "        if (f != NULL) { fclose(f); }\n"
        "    }\n"
        "}\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    project.output_path = FIXTURE_DIR "/missing_teardown/build";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.test_directories, "", FIXTURE_DIR "/missing_teardown");

    result = cgtest_runner_run(&project);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(result.exit_code == 0);

    marker = fopen(FIXTURE_DIR "/missing_teardown/value_ok", "r");
    CHECK(marker != NULL);
    if (marker != NULL) {
        fclose(marker);
    }

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/missing_teardown/value_ok");
    remove(FIXTURE_DIR "/missing_teardown/test_widget.c");
    remove(FIXTURE_DIR "/missing_teardown/build/cgtest-runner.c");
    remove(FIXTURE_DIR "/missing_teardown/build/cgtest-runner");
    remove(FIXTURE_DIR "/missing_teardown/build");
    remove(FIXTURE_DIR "/missing_teardown");
    remove(FIXTURE_DIR);
}

void test_run_still_calls_teardown_function_when_present(void)
{
    /* Companion to the "missing teardown" test above: when
     * teardown_bar IS present, it must still actually run - proven
     * here via a real compile+run, not just a generated-source string
     * check, since has_teardown is computed by cgtest_runner_run()
     * itself (cgtest_runner_generate_source() alone can't exercise
     * this). teardown_bar writes a marker file (and frees *state,
     * demonstrating the recommended pattern when prompt cleanup
     * matters - specification.md ch.6); its absence would mean
     * teardown_bar was silently skipped despite existing. */
    CGTestProject project;
    CGTestRunResult result;
    FILE *marker;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/present_teardown", 0755);
    write_file(FIXTURE_DIR "/present_teardown/test_widget.c",
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "typedef struct State { int x; } State;\n"
        "void setup_bar(State **state) { *state = calloc(1, sizeof(State)); }\n"
        "void teardown_bar(State *state) {\n"
        "    FILE *f = fopen(\"" FIXTURE_DIR "/present_teardown/teardown_ran\", \"w\");\n"
        "    if (f != NULL) { fclose(f); }\n"
        "    free(state);\n"
        "}\n"
        "void test_bar(State *state) { (void)state; }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    project.output_path = FIXTURE_DIR "/present_teardown/build";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.test_directories, "", FIXTURE_DIR "/present_teardown");

    result = cgtest_runner_run(&project);

    CHECK(result.ok);
    CHECK(result.exit_code == 0);

    marker = fopen(FIXTURE_DIR "/present_teardown/teardown_ran", "r");
    CHECK(marker != NULL);
    if (marker != NULL) {
        fclose(marker);
    }

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/present_teardown/teardown_ran");
    remove(FIXTURE_DIR "/present_teardown/test_widget.c");
    remove(FIXTURE_DIR "/present_teardown/build/cgtest-runner.c");
    remove(FIXTURE_DIR "/present_teardown/build/cgtest-runner");
    remove(FIXTURE_DIR "/present_teardown/build");
    remove(FIXTURE_DIR "/present_teardown");
    remove(FIXTURE_DIR);
}

void test_run_allows_same_named_static_helper_across_files(void)
{
    /* The point of compiling each test file as its own translation
     * unit instead of #include-ing them all into one cgtest-runner.c
     * (see cgtest_runner.h): two files can each define their own
     * private "static int helper(void)" without colliding, the normal
     * C rule for separate translation units - previously (when every
     * file shared one TU via #include) this was a hard compile error. */
    CGTestProject project;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/same_helper", 0755);
    write_file(FIXTURE_DIR "/same_helper/test_one.c",
        "static int helper(void) { return 1; }\n"
        "void test_one(void) { (void)helper(); }\n");
    write_file(FIXTURE_DIR "/same_helper/test_two.c",
        "static int helper(void) { return 2; }\n"
        "void test_two(void) { (void)helper(); }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    project.output_path = FIXTURE_DIR "/same_helper/build";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.test_directories, "", FIXTURE_DIR "/same_helper");

    result = cgtest_runner_run(&project);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(result.exit_code == 0);

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/same_helper/test_one.c");
    remove(FIXTURE_DIR "/same_helper/test_two.c");
    remove(FIXTURE_DIR "/same_helper/build/cgtest-runner.c");
    remove(FIXTURE_DIR "/same_helper/build/cgtest-runner");
    remove(FIXTURE_DIR "/same_helper/build");
    remove(FIXTURE_DIR "/same_helper");
    remove(FIXTURE_DIR);
}

void test_run_single_translation_unit_compiles_and_runs(void)
{
    /* End-to-end proof that single_translation_unit=1 (specification.md
     * ch.6) actually produces a compiling, runnable cgtest-runner - not
     * just a generated-source string containing the right "#include"
     * lines (test_single_translation_unit_includes_each_file_before_its_
     * extern_declarations already covers that in isolation). */
    CGTestProject project;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/single_tu", 0755);
    write_file(FIXTURE_DIR "/single_tu/test_widget.c", "void test_bar(void) { }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    project.output_path = FIXTURE_DIR "/single_tu/build";
    project.single_translation_unit = 1;
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    register_absolute_test_directory(&project.test_directories, FIXTURE_DIR "/single_tu");

    result = cgtest_runner_run(&project);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(result.exit_code == 0);

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/single_tu/test_widget.c");
    remove(FIXTURE_DIR "/single_tu/build/cgtest-runner.c");
    remove(FIXTURE_DIR "/single_tu/build/cgtest-runner");
    remove(FIXTURE_DIR "/single_tu/build");
    remove(FIXTURE_DIR "/single_tu");
    remove(FIXTURE_DIR);
}

void test_run_single_translation_unit_compiles_fixture_test_under_pedantic_errors(void)
{
    /* Regression test for the bug test_single_translation_unit_skips_
     * fixture_forward_declare guards at the generated-source level:
     * this compiles a real fixture test end to end with
     * "-pedantic-errors" (found by running examples/mathlib, which uses
     * "-std=c89 -pedantic-errors", with single_translation_unit=true -
     * it failed with "redefinition of typedef 'State'" before the fix
     * that made the fixture forward-declare block single-TU-aware). */
    CGTestProject project;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/single_tu_fixture", 0755);
    write_file(FIXTURE_DIR "/single_tu_fixture/test_widget.c",
        "#include <stdlib.h>\n"
        "typedef struct State { int x; } State;\n"
        "void setup_bar(State **state) { *state = calloc(1, sizeof(State)); }\n"
        "void test_bar(State *state) { (void)state; }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99 -pedantic-errors";
    project.output_path = FIXTURE_DIR "/single_tu_fixture/build";
    project.single_translation_unit = 1;
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    register_absolute_test_directory(&project.test_directories, FIXTURE_DIR "/single_tu_fixture");

    result = cgtest_runner_run(&project);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(result.exit_code == 0);

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/single_tu_fixture/test_widget.c");
    remove(FIXTURE_DIR "/single_tu_fixture/build/cgtest-runner.c");
    remove(FIXTURE_DIR "/single_tu_fixture/build/cgtest-runner");
    remove(FIXTURE_DIR "/single_tu_fixture/build");
    remove(FIXTURE_DIR "/single_tu_fixture");
    remove(FIXTURE_DIR);
}

void test_run_single_translation_unit_fails_on_duplicate_static_helper(void)
{
    /* The known, accepted tradeoff of single_translation_unit=1
     * (specification.md ch.6 "Single-translation-unit mode"): unlike
     * test_run_allows_same_named_static_helper_across_files' default
     * mode, two files defining the same-named "static int helper(void)"
     * now share one translation unit and the compiler rejects it as a
     * redefinition - cgtest does not detect or pre-empt this itself, so
     * it must surface as an ordinary compile failure. */
    CGTestProject project;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/single_tu_collision", 0755);
    write_file(FIXTURE_DIR "/single_tu_collision/test_one.c",
        "static int helper(void) { return 1; }\n"
        "void test_one(void) { (void)helper(); }\n");
    write_file(FIXTURE_DIR "/single_tu_collision/test_two.c",
        "static int helper(void) { return 2; }\n"
        "void test_two(void) { (void)helper(); }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    project.output_path = FIXTURE_DIR "/single_tu_collision/build";
    project.single_translation_unit = 1;
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    register_absolute_test_directory(&project.test_directories, FIXTURE_DIR "/single_tu_collision");

    result = cgtest_runner_run(&project);

    CHECK(!result.ok);
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "compilation failed") != NULL);

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/single_tu_collision/test_one.c");
    remove(FIXTURE_DIR "/single_tu_collision/test_two.c");
    remove(FIXTURE_DIR "/single_tu_collision/build/cgtest-runner.c");
    remove(FIXTURE_DIR "/single_tu_collision/build");
    remove(FIXTURE_DIR "/single_tu_collision");
    remove(FIXTURE_DIR);
}

void test_run_populates_timing_fields(void)
{
    /* Timing (CGTestRunResult::scan_ms/generate_ms/compile_ms/run_ms/
     * total_ms - see ctimer.h) is always measured, regardless of
     * whether -t/--time was given - printing it is cgtest_main.c's
     * job. A real compile is genuinely exercised here (unlike the
     * duplicate-basename/missing-setup tests, which fail before
     * reaching one), so compile_ms should be the dominant phase, not
     * just nonzero - matching what a real cgtest --run --time
     * invocation actually shows. */
    CGTestProject project;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/timing", 0755);
    write_file(FIXTURE_DIR "/timing/test_widget.c", "void test_bar(void) { }\n");

    memset(&project, 0, sizeof(project));
    project.compiler_command = "gcc -std=c99";
    project.output_path = FIXTURE_DIR "/timing/build";
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);
    cpathlist_register(&project.test_directories, "", FIXTURE_DIR "/timing");

    result = cgtest_runner_run(&project);

    CHECK(result.ok);
    CHECK(result.scan_ms >= 0.0);
    CHECK(result.generate_ms >= 0.0);
    CHECK(result.compile_ms > 0.0);
    CHECK(result.run_ms >= 0.0);
    CHECK(result.total_ms >= result.compile_ms);

    cgtest_runner_free(&result);
    cpathlist_free(&project.include_paths);
    cpathlist_free(&project.source_files);
    cpathlist_free(&project.test_directories);
    remove(FIXTURE_DIR "/timing/test_widget.c");
    remove(FIXTURE_DIR "/timing/build/cgtest-runner.c");
    remove(FIXTURE_DIR "/timing/build/cgtest-runner");
    remove(FIXTURE_DIR "/timing/build");
    remove(FIXTURE_DIR "/timing");
    remove(FIXTURE_DIR);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_generates_valid_shell_with_no_files", test_generates_valid_shell_with_no_files },
        { "test_declares_and_defines_shared_helpers_in_generated_runner", test_declares_and_defines_shared_helpers_in_generated_runner },
        { "test_embeds_the_compile_command_as_a_leading_comment", test_embeds_the_compile_command_as_a_leading_comment },
        { "test_declares_extern_and_calls_its_function", test_declares_extern_and_calls_its_function },
        { "test_header_uses_bare_basename_and_precedes_its_tests", test_header_uses_bare_basename_and_precedes_its_tests },
        { "test_skips_the_header_for_a_file_with_no_test_functions", test_skips_the_header_for_a_file_with_no_test_functions },
        { "test_preserves_function_order_within_a_file", test_preserves_function_order_within_a_file },
        { "test_preserves_file_order_across_files", test_preserves_file_order_across_files },
        { "test_generates_setup_teardown_wrapper_for_fixture_function", test_generates_setup_teardown_wrapper_for_fixture_function },
        { "test_deduplicates_fixture_type_forward_declaration", test_deduplicates_fixture_type_forward_declaration },
        { "test_forward_declares_each_distinct_fixture_type_once", test_forward_declares_each_distinct_fixture_type_once },
        { "test_omits_teardown_call_when_has_teardown_is_unset", test_omits_teardown_call_when_has_teardown_is_unset },
        { "test_fixture_test_call_is_guarded_by_fatal_failed_check", test_fixture_test_call_is_guarded_by_fatal_failed_check },
        { "test_fixture_wrapper_precedes_pass_fail_verdict", test_fixture_wrapper_precedes_pass_fail_verdict },
        { "test_single_translation_unit_includes_each_file_before_its_extern_declarations", test_single_translation_unit_includes_each_file_before_its_extern_declarations },
        { "test_default_mode_never_includes_test_files", test_default_mode_never_includes_test_files },
        { "test_single_translation_unit_skips_fixture_forward_declare", test_single_translation_unit_skips_fixture_forward_declare },
        { "test_build_compile_command_uses_gcc_flags_by_default", test_build_compile_command_uses_gcc_flags_by_default },
        { "test_build_compile_command_uses_msvc_flags_when_configured", test_build_compile_command_uses_msvc_flags_when_configured },
        { "test_build_compile_command_with_no_test_files", test_build_compile_command_with_no_test_files },
        { "test_build_compile_command_omits_test_files_as_separate_sources_when_single_translation_unit", test_build_compile_command_omits_test_files_as_separate_sources_when_single_translation_unit },
        { "test_run_rejects_duplicate_basenames_across_directories", test_run_rejects_duplicate_basenames_across_directories },
        { "test_run_rejects_fixture_test_missing_setup_function", test_run_rejects_fixture_test_missing_setup_function },
        { "test_run_succeeds_with_fixture_test_missing_teardown_function", test_run_succeeds_with_fixture_test_missing_teardown_function },
        { "test_run_still_calls_teardown_function_when_present", test_run_still_calls_teardown_function_when_present },
        { "test_run_allows_same_named_static_helper_across_files", test_run_allows_same_named_static_helper_across_files },
        { "test_run_single_translation_unit_compiles_and_runs", test_run_single_translation_unit_compiles_and_runs },
        { "test_run_single_translation_unit_compiles_fixture_test_under_pedantic_errors", test_run_single_translation_unit_compiles_fixture_test_under_pedantic_errors },
        { "test_run_single_translation_unit_fails_on_duplicate_static_helper", test_run_single_translation_unit_fails_on_duplicate_static_helper },
        { "test_run_populates_timing_fields", test_run_populates_timing_fields }
    };
    size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t i;
    size_t failed = 0;

    for (i = 0; i < count; i++) {
        test_failed = 0;
        cases[i].fn();
        printf("[%s] %s\n", test_failed ? "FAIL" : "PASS", cases[i].name);
        if (test_failed) {
            failed++;
        }
    }

    printf("\n%zu/%zu passed\n", count - failed, count);
    return failed == 0 ? 0 : 1;
}
