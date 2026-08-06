/* test_cgtest_create.c - unit tests for cgtest_create_run(), which
 * writes a template cgtest-project.json, cgtest.h, and
 * test_cgtest_macros.c inside a given directory's "cgtest" child
 * (creating both if they don't exist yet).
 * Written in cgtest's own test convention (void test_<name>(void));
 * see test_ctestscanner.c's header comment for why main() below
 * dispatches them manually instead of via a generated cgtest-runner.
 *
 * Like test_ctestfiles.c, this inherently needs real filesystem
 * interaction, so each test builds a small throw-away fixture
 * directory under build/ and tears it down again.
 */
#include "cgtest_create.h"
#include "cgtest_project.h"

#include <stdio.h>
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

#define FIXTURE_DIR "build/cgtest_create_fixture"
#define FIXTURE_CGTEST_DIR FIXTURE_DIR "/cgtest"
#define PROJECT_PATH FIXTURE_CGTEST_DIR "/cgtest-project.json"
#define HEADER_PATH FIXTURE_CGTEST_DIR "/cgtest.h"
#define TEST_MACROS_PATH FIXTURE_CGTEST_DIR "/test_cgtest_macros.c"

static void setup_fixture(void)
{
    mkdir(FIXTURE_DIR, 0755);
}

static void teardown_fixture(void)
{
    remove(PROJECT_PATH);
    remove(HEADER_PATH);
    remove(TEST_MACROS_PATH);
    remove(FIXTURE_CGTEST_DIR);
    remove(FIXTURE_DIR);
}

static long read_whole_file(const char *path, char *buf, size_t bufsize)
{
    FILE *f = fopen(path, "rb");
    size_t read_count;

    if (f == NULL) {
        return -1;
    }
    read_count = fread(buf, 1, bufsize - 1, f);
    fclose(f);
    buf[read_count] = '\0';
    return (long)read_count;
}

void test_creates_project_and_header_in_existing_directory(void)
{
    CGTestCreateResult result;
    char buf[4096];

    setup_fixture();
    result = cgtest_create_run(FIXTURE_DIR);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(result.dir != NULL);
    CHECK(strstr(result.dir, "/cgtest") != NULL);
    CHECK(result.wrote_project);
    CHECK(result.wrote_header);
    CHECK(result.wrote_test_macros);
    CHECK(read_whole_file(PROJECT_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(HEADER_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(TEST_MACROS_PATH, buf, sizeof(buf)) > 0);

    cgtest_create_free(&result);
    teardown_fixture();
}

void test_creates_directory_if_missing(void)
{
    CGTestCreateResult result;
    struct stat st;
    char buf[4096];

    remove(FIXTURE_DIR); /* make sure it does NOT exist yet */
    result = cgtest_create_run(FIXTURE_DIR);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(stat(FIXTURE_DIR, &st) == 0);
    CHECK(S_ISDIR(st.st_mode));
    CHECK(read_whole_file(PROJECT_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(HEADER_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(TEST_MACROS_PATH, buf, sizeof(buf)) > 0);

    cgtest_create_free(&result);
    teardown_fixture();
}

void test_test_macros_file_covers_the_whole_header(void)
{
    CGTestCreateResult result;
    char buf[16384];
    long length;

    setup_fixture();
    result = cgtest_create_run(FIXTURE_DIR);
    CHECK(result.ok);

    length = read_whole_file(TEST_MACROS_PATH, buf, sizeof(buf));
    CHECK(length > 0);

    /* Spot-check the first and last macro families rather than every
     * single one - if both ends of the template chain made it into
     * the file, the ones in between did too. */
    CHECK(strstr(buf, "#include \"cgtest.h\"") != NULL);
    CHECK(strstr(buf, "EXPECT_TRUE(1 == 1)") != NULL);
    CHECK(strstr(buf, "ASSERT_NE_STR_NOCASE(\"CGTest\", \"gtest\")") != NULL);

    cgtest_create_free(&result);
    teardown_fixture();
}

void test_header_declares_shared_helpers_extern_not_static(void)
{
    /* Regression test: cgtest_relpath()/cgtest_print_str_field()/
     * cgtest_strcasecmp() used to be `static` definitions copied
     * directly into cgtest.h, which meant every #include'ing test_*.c
     * file (its own translation unit in separate-TU mode - see
     * cgtest_runner.h) got its own private, possibly-uncalled copy -
     * one -Wunused-function flagged in any file that didn't happen to
     * invoke the specific macro family relying on it (found by
     * running examples/mathlib with "-Wall -Wextra -pedantic-errors").
     * cgtest.h must instead only "extern"-declare them, the same
     * pattern already used for cgtest_failed/cgtest_fatal_failed - see
     * test_declares_and_defines_shared_helpers_in_generated_runner in
     * test_cgtest_runner.c for where the actual definitions now live. */
    CGTestCreateResult result;
    char buf[32768];
    long length;

    setup_fixture();
    result = cgtest_create_run(FIXTURE_DIR);
    CHECK(result.ok);

    length = read_whole_file(HEADER_PATH, buf, sizeof(buf));
    CHECK(length > 0);

    CHECK(strstr(buf, "extern const char *cgtest_relpath(const char *file);") != NULL);
    CHECK(strstr(buf, "extern void cgtest_print_str_field(const char *prefix, const char *s);") != NULL);
    CHECK(strstr(buf, "extern int cgtest_strcasecmp(const char *a, const char *b);") != NULL);
    CHECK(strstr(buf, "static const char *cgtest_relpath") == NULL);
    CHECK(strstr(buf, "static void cgtest_print_str_field") == NULL);
    CHECK(strstr(buf, "static int cgtest_strcasecmp") == NULL);

    cgtest_create_free(&result);
    teardown_fixture();
}

void test_created_project_round_trips_through_parser(void)
{
    CGTestCreateResult result;
    CGTestProject project;
    char buf[4096];
    long length;

    setup_fixture();
    result = cgtest_create_run(FIXTURE_DIR);
    CHECK(result.ok);

    length = read_whole_file(PROJECT_PATH, buf, sizeof(buf));
    CHECK(length > 0);

    project = cgtest_project_parse(buf, (size_t)length, "/base");
    CHECK(project.ok);
    CHECK(project.error == NULL);

    cgtest_project_free(&project);
    cgtest_create_free(&result);
    teardown_fixture();
}

void test_leaves_existing_project_untouched_and_fills_in_missing_files(void)
{
    /* cgtest_create_run() used to refuse outright (writing nothing) the
     * moment cgtest-project.json existed - this checks each of the
     * three files independently instead (cgtest_create.h): an existing
     * cgtest-project.json is left completely untouched, but cgtest.h
     * and test_cgtest_macros.c, both missing here, are still filled in
     * and the call succeeds - the scenario that matters is a developer
     * re-running --init on an already-initialized project to pick up a
     * newer cgtest.exe's cgtest.h fix without disturbing their already-
     * customized cgtest-project.json. */
    CGTestCreateResult result;
    FILE *f;
    char buf[4096];

    setup_fixture();
    mkdir(FIXTURE_CGTEST_DIR, 0755);
    f = fopen(PROJECT_PATH, "w");
    CHECK(f != NULL);
    fputs("PREEXISTING", f);
    fclose(f);

    result = cgtest_create_run(FIXTURE_DIR);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(result.dir != NULL);
    CHECK(!result.wrote_project);
    CHECK(result.wrote_header);
    CHECK(result.wrote_test_macros);

    CHECK(read_whole_file(PROJECT_PATH, buf, sizeof(buf)) > 0);
    CHECK(strcmp(buf, "PREEXISTING") == 0);
    CHECK(read_whole_file(HEADER_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(TEST_MACROS_PATH, buf, sizeof(buf)) > 0);

    cgtest_create_free(&result);
    teardown_fixture();
}

void test_rerunning_on_a_complete_project_is_an_idempotent_no_op(void)
{
    /* Companion to the test above: once all three files exist (a
     * normal, complete --init already ran), calling cgtest_create_run()
     * again must succeed without writing anything - not error, not
     * silently overwrite. */
    CGTestCreateResult first;
    CGTestCreateResult second;
    char before[4096];
    char after[4096];

    setup_fixture();
    first = cgtest_create_run(FIXTURE_DIR);
    CHECK(first.ok);
    CHECK(read_whole_file(PROJECT_PATH, before, sizeof(before)) > 0);
    cgtest_create_free(&first);

    second = cgtest_create_run(FIXTURE_DIR);

    CHECK(second.ok);
    CHECK(second.error == NULL);
    CHECK(!second.wrote_project);
    CHECK(!second.wrote_header);
    CHECK(!second.wrote_test_macros);
    CHECK(read_whole_file(PROJECT_PATH, after, sizeof(after)) > 0);
    CHECK(strcmp(before, after) == 0);

    cgtest_create_free(&second);
    teardown_fixture();
}

void test_regenerates_only_a_deleted_header(void)
{
    /* The exact upgrade scenario this per-file check exists for: a
     * developer deletes cgtest.h alone (to pick up a fix from a newer
     * cgtest.exe - cgtest.h never carries per-project customization the
     * way cgtest-project.json's compiler_command/include_paths/etc. do)
     * and re-runs --init. cgtest-project.json (customized here, like
     * test_leaves_existing_project_untouched_and_fills_in_missing_files
     * above) and test_cgtest_macros.c must stay exactly as they were;
     * only cgtest.h comes back. */
    CGTestCreateResult first;
    CGTestCreateResult second;
    char project_before[4096];
    char project_after[4096];
    char test_macros_before[4096];
    char test_macros_after[4096];
    char header_after[32768];

    setup_fixture();
    first = cgtest_create_run(FIXTURE_DIR);
    CHECK(first.ok);
    cgtest_create_free(&first);

    CHECK(read_whole_file(PROJECT_PATH, project_before, sizeof(project_before)) > 0);
    CHECK(read_whole_file(TEST_MACROS_PATH, test_macros_before, sizeof(test_macros_before)) > 0);
    CHECK(remove(HEADER_PATH) == 0);

    second = cgtest_create_run(FIXTURE_DIR);

    CHECK(second.ok);
    CHECK(second.error == NULL);
    CHECK(!second.wrote_project);
    CHECK(second.wrote_header);
    CHECK(!second.wrote_test_macros);

    CHECK(read_whole_file(PROJECT_PATH, project_after, sizeof(project_after)) > 0);
    CHECK(strcmp(project_before, project_after) == 0);
    CHECK(read_whole_file(TEST_MACROS_PATH, test_macros_after, sizeof(test_macros_after)) > 0);
    CHECK(strcmp(test_macros_before, test_macros_after) == 0);
    CHECK(read_whole_file(HEADER_PATH, header_after, sizeof(header_after)) > 0);
    CHECK(strstr(header_after, "extern const char *cgtest_relpath(const char *file);") != NULL);

    cgtest_create_free(&second);
    teardown_fixture();
}

void test_path_that_is_a_regular_file_is_an_error(void)
{
    CGTestCreateResult result;
    FILE *f;

    remove(FIXTURE_DIR);
    f = fopen(FIXTURE_DIR, "w"); /* a plain file where a directory is expected */
    CHECK(f != NULL);
    fclose(f);

    result = cgtest_create_run(FIXTURE_DIR);

    CHECK(!result.ok);
    CHECK(result.dir == NULL);
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "not a directory") != NULL);

    cgtest_create_free(&result);
    remove(FIXTURE_DIR);
}

#define NESTED_PARENT_DIR "build/cgtest_create_nested_fixture"
#define NESTED_DIR NESTED_PARENT_DIR "/child"
#define NESTED_CGTEST_DIR NESTED_DIR "/cgtest"
#define NESTED_PROJECT_PATH NESTED_CGTEST_DIR "/cgtest-project.json"
#define NESTED_HEADER_PATH NESTED_CGTEST_DIR "/cgtest.h"
#define NESTED_TEST_MACROS_PATH NESTED_CGTEST_DIR "/test_cgtest_macros.c"

void test_creates_missing_parent_directories(void)
{
    CGTestCreateResult result;
    struct stat st;
    char buf[4096];

    remove(NESTED_PROJECT_PATH);
    remove(NESTED_HEADER_PATH);
    remove(NESTED_TEST_MACROS_PATH);
    remove(NESTED_CGTEST_DIR);
    remove(NESTED_DIR);
    remove(NESTED_PARENT_DIR); /* make sure neither exists yet */

    result = cgtest_create_run(NESTED_DIR);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(stat(NESTED_CGTEST_DIR, &st) == 0);
    CHECK(S_ISDIR(st.st_mode));
    CHECK(read_whole_file(NESTED_PROJECT_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(NESTED_HEADER_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(NESTED_TEST_MACROS_PATH, buf, sizeof(buf)) > 0);

    cgtest_create_free(&result);
    remove(NESTED_PROJECT_PATH);
    remove(NESTED_HEADER_PATH);
    remove(NESTED_TEST_MACROS_PATH);
    remove(NESTED_CGTEST_DIR);
    remove(NESTED_DIR);
    remove(NESTED_PARENT_DIR);
}

#define OBSTRUCTION_FILE "build/cgtest_create_obstruction_file"

void test_parent_segment_that_is_a_regular_file_is_an_error(void)
{
    CGTestCreateResult result;
    FILE *f;

    remove(OBSTRUCTION_FILE);
    f = fopen(OBSTRUCTION_FILE, "w"); /* a plain file standing in for a parent directory */
    CHECK(f != NULL);
    fclose(f);

    result = cgtest_create_run(OBSTRUCTION_FILE "/nested");

    CHECK(!result.ok);
    CHECK(result.error != NULL);

    cgtest_create_free(&result);
    remove(OBSTRUCTION_FILE);
}

void test_free_on_error_is_safe(void)
{
    CGTestCreateResult result;
    FILE *f;

    remove(OBSTRUCTION_FILE);
    f = fopen(OBSTRUCTION_FILE, "w");
    CHECK(f != NULL);
    fclose(f);

    result = cgtest_create_run(OBSTRUCTION_FILE "/nested");
    CHECK(!result.ok);
    cgtest_create_free(&result);
    cgtest_create_free(&result);
    remove(OBSTRUCTION_FILE);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_creates_project_and_header_in_existing_directory", test_creates_project_and_header_in_existing_directory },
        { "test_creates_directory_if_missing", test_creates_directory_if_missing },
        { "test_test_macros_file_covers_the_whole_header", test_test_macros_file_covers_the_whole_header },
        { "test_header_declares_shared_helpers_extern_not_static", test_header_declares_shared_helpers_extern_not_static },
        { "test_created_project_round_trips_through_parser", test_created_project_round_trips_through_parser },
        { "test_leaves_existing_project_untouched_and_fills_in_missing_files", test_leaves_existing_project_untouched_and_fills_in_missing_files },
        { "test_rerunning_on_a_complete_project_is_an_idempotent_no_op", test_rerunning_on_a_complete_project_is_an_idempotent_no_op },
        { "test_regenerates_only_a_deleted_header", test_regenerates_only_a_deleted_header },
        { "test_path_that_is_a_regular_file_is_an_error", test_path_that_is_a_regular_file_is_an_error },
        { "test_creates_missing_parent_directories", test_creates_missing_parent_directories },
        { "test_parent_segment_that_is_a_regular_file_is_an_error", test_parent_segment_that_is_a_regular_file_is_an_error },
        { "test_free_on_error_is_safe", test_free_on_error_is_safe }
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
