/* test_cgtest_runner.c - unit tests for cgtest_runner_generate_source(),
 * the pure part of cgtest_runner.h that turns already-discovered test
 * functions into cgtest-runner.c's source text. Compiling and running
 * the generated result is exercised end to end via examples/mathlib
 * instead (see README/specification.md), not here - matching
 * cgtest_config.h's split between the pure parser and its disk-facing
 * cgtest_config_load(). The one exception is
 * test_run_rejects_duplicate_basenames_across_directories below: it
 * calls cgtest_runner_run() directly, but only far enough to hit the
 * duplicate-basename check, which fails before any compiler is
 * invoked - still no real compilation needed.
 *
 * Written in cgtest's own test convention (void test_<name>(void)); see
 * test_ctestscanner.c's header comment for why main() below dispatches
 * them manually instead of via a generated cgtest-runner.
 */
#include "cgtest_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    char *source = cgtest_runner_generate_source(NULL, 0, "gcc -std=c99 -o cgtest-runner cgtest-runner.c");

    CHECK(source != NULL);
    CHECK(strstr(source, "int main(void)") != NULL);
    CHECK(strstr(source, "total - failed") != NULL);
    CHECK(strstr(source, "#include \"") == NULL);

    free(source);
}

void test_embeds_the_compile_command_as_a_leading_comment(void)
{
    char *source = cgtest_runner_generate_source(NULL, 0, "gcc -std=c99 -I\"src\" -o cgtest-runner cgtest-runner.c");
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

void test_includes_the_file_and_calls_its_function(void)
{
    CTestFunction functions[1];
    CGTestRunnerFile files[1];
    char *source;

    functions[0].name = "test_math_add";
    functions[0].line = 3;

    files[0].label = "/abs/path/test_math.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c");

    CHECK(source != NULL);
    CHECK(strstr(source, "#include \"test_math.c\"") != NULL);
    CHECK(strstr(source, "/abs/path") == NULL);
    CHECK(strstr(source, "extern") == NULL);
    CHECK(strstr(source, "test_math_add()") != NULL);
    CHECK(strstr(source, "[PASS] test_math_add") != NULL);
    CHECK(strstr(source, "[FAIL] test_math_add") != NULL);
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
    functions[0].line = 3;

    files[0].label = "/abs/path/test_math.c";
    files[0].functions = functions;
    files[0].function_count = 1;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c");
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

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c");
    CHECK(source != NULL);

    /* Still #include'd (it may hold setup/helper code, just no test_
     * functions of its own), just no "== test_empty.c ==" header. */
    CHECK(strstr(source, "#include \"test_empty.c\"") != NULL);
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
    functions[0].line = 1;
    functions[1].name = "test_teardown";
    functions[1].line = 9;

    files[0].label = "test_lifecycle.c";
    files[0].functions = functions;
    files[0].function_count = 2;

    source = cgtest_runner_generate_source(files, 1, "gcc -std=c99 -o cgtest-runner cgtest-runner.c");
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
    fn_a[0].line = 1;
    fn_b[0].name = "test_from_b";
    fn_b[0].line = 1;

    files[0].label = "test_a.c";
    files[0].functions = fn_a;
    files[0].function_count = 1;
    files[1].label = "test_b.c";
    files[1].functions = fn_b;
    files[1].function_count = 1;

    source = cgtest_runner_generate_source(files, 2, "gcc -std=c99 -o cgtest-runner cgtest-runner.c");
    CHECK(source != NULL);

    call_a = strstr(source, "test_from_a()");
    call_b = strstr(source, "test_from_b()");
    CHECK(call_a != NULL);
    CHECK(call_b != NULL);
    CHECK(call_a < call_b);

    free(source);
}

#define FIXTURE_DIR "build/cgtest_runner_fixture"

static void write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    fputs(content, f);
    fclose(f);
}

void test_run_rejects_duplicate_basenames_across_directories(void)
{
    CGTestConfig config;
    CGTestRunResult result;

    mkdir(FIXTURE_DIR, 0755);
    mkdir(FIXTURE_DIR "/dir_a", 0755);
    mkdir(FIXTURE_DIR "/dir_b", 0755);
    write_file(FIXTURE_DIR "/dir_a/test_dup.c", "void test_from_a(void) { }\n");
    write_file(FIXTURE_DIR "/dir_b/test_dup.c", "void test_from_b(void) { }\n");

    memset(&config, 0, sizeof(config));
    config.compiler_command = "gcc -std=c99"; /* never actually invoked - see below */
    config.output_path = FIXTURE_DIR "/build";
    cpathlist_init(&config.include_paths);
    cpathlist_init(&config.source_files);
    cpathlist_init(&config.test_directories);
    cpathlist_register(&config.test_directories, "", FIXTURE_DIR "/dir_a");
    cpathlist_register(&config.test_directories, "", FIXTURE_DIR "/dir_b");

    result = cgtest_runner_run(&config);

    CHECK(!result.ok);
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "duplicate") != NULL);
    CHECK(strstr(result.error, "test_dup.c") != NULL);

    cgtest_runner_free(&result);
    /* Not cgtest_config_free(): compiler_command/output_path above are
     * string literals, not malloc'd, so freeing "config" the normal
     * way would be undefined behavior. */
    cpathlist_free(&config.include_paths);
    cpathlist_free(&config.source_files);
    cpathlist_free(&config.test_directories);
    remove(FIXTURE_DIR "/dir_a/test_dup.c");
    remove(FIXTURE_DIR "/dir_b/test_dup.c");
    remove(FIXTURE_DIR "/dir_a");
    remove(FIXTURE_DIR "/dir_b");
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
        { "test_embeds_the_compile_command_as_a_leading_comment", test_embeds_the_compile_command_as_a_leading_comment },
        { "test_includes_the_file_and_calls_its_function", test_includes_the_file_and_calls_its_function },
        { "test_header_uses_bare_basename_and_precedes_its_tests", test_header_uses_bare_basename_and_precedes_its_tests },
        { "test_skips_the_header_for_a_file_with_no_test_functions", test_skips_the_header_for_a_file_with_no_test_functions },
        { "test_preserves_function_order_within_a_file", test_preserves_function_order_within_a_file },
        { "test_preserves_file_order_across_files", test_preserves_file_order_across_files },
        { "test_run_rejects_duplicate_basenames_across_directories", test_run_rejects_duplicate_basenames_across_directories }
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
