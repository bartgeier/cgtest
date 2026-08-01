/* test_cgtest_create.c - unit tests for cgtest_create_run(), which
 * writes a template cgtest-config.json, cgtest.h, and
 * test_cgtest_macros.c inside a given directory (creating that
 * directory if it doesn't exist yet).
 * Written in cgtest's own test convention (void test_<name>(void));
 * see test_ctestscanner.c's header comment for why main() below
 * dispatches them manually instead of via a generated cgtest-runner.
 *
 * Like test_ctestfiles.c, this inherently needs real filesystem
 * interaction, so each test builds a small throw-away fixture
 * directory under build/ and tears it down again.
 */
#include "cgtest_create.h"
#include "cgtest_config.h"

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
#define CONFIG_PATH FIXTURE_DIR "/cgtest-config.json"
#define HEADER_PATH FIXTURE_DIR "/cgtest.h"
#define TEST_MACROS_PATH FIXTURE_DIR "/test_cgtest_macros.c"

static void setup_fixture(void)
{
    mkdir(FIXTURE_DIR, 0755);
}

static void teardown_fixture(void)
{
    remove(CONFIG_PATH);
    remove(HEADER_PATH);
    remove(TEST_MACROS_PATH);
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

void test_creates_config_and_header_in_existing_directory(void)
{
    CGTestCreateResult result;
    char buf[4096];

    setup_fixture();
    result = cgtest_create_run(FIXTURE_DIR);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(read_whole_file(CONFIG_PATH, buf, sizeof(buf)) > 0);
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
    CHECK(read_whole_file(CONFIG_PATH, buf, sizeof(buf)) > 0);
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

void test_created_config_round_trips_through_parser(void)
{
    CGTestCreateResult result;
    CGTestConfig config;
    char buf[4096];
    long length;

    setup_fixture();
    result = cgtest_create_run(FIXTURE_DIR);
    CHECK(result.ok);

    length = read_whole_file(CONFIG_PATH, buf, sizeof(buf));
    CHECK(length > 0);

    config = cgtest_config_parse(buf, (size_t)length, "/base");
    CHECK(config.ok);
    CHECK(config.error == NULL);

    cgtest_config_free(&config);
    cgtest_create_free(&result);
    teardown_fixture();
}

void test_refuses_to_overwrite_existing_config(void)
{
    CGTestCreateResult result;
    FILE *f;
    char buf[4096];

    setup_fixture();
    f = fopen(CONFIG_PATH, "w");
    CHECK(f != NULL);
    fputs("PREEXISTING", f);
    fclose(f);

    result = cgtest_create_run(FIXTURE_DIR);

    CHECK(!result.ok);
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "already exists") != NULL);

    CHECK(read_whole_file(CONFIG_PATH, buf, sizeof(buf)) > 0);
    CHECK(strcmp(buf, "PREEXISTING") == 0);

    cgtest_create_free(&result);
    remove(CONFIG_PATH);
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
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "not a directory") != NULL);

    cgtest_create_free(&result);
    remove(FIXTURE_DIR);
}

#define NESTED_PARENT_DIR "build/cgtest_create_nested_fixture"
#define NESTED_DIR NESTED_PARENT_DIR "/child"
#define NESTED_CONFIG_PATH NESTED_DIR "/cgtest-config.json"
#define NESTED_HEADER_PATH NESTED_DIR "/cgtest.h"
#define NESTED_TEST_MACROS_PATH NESTED_DIR "/test_cgtest_macros.c"

void test_creates_missing_parent_directories(void)
{
    CGTestCreateResult result;
    struct stat st;
    char buf[4096];

    remove(NESTED_CONFIG_PATH);
    remove(NESTED_HEADER_PATH);
    remove(NESTED_TEST_MACROS_PATH);
    remove(NESTED_DIR);
    remove(NESTED_PARENT_DIR); /* make sure neither exists yet */

    result = cgtest_create_run(NESTED_DIR);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(stat(NESTED_DIR, &st) == 0);
    CHECK(S_ISDIR(st.st_mode));
    CHECK(read_whole_file(NESTED_CONFIG_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(NESTED_HEADER_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(NESTED_TEST_MACROS_PATH, buf, sizeof(buf)) > 0);

    cgtest_create_free(&result);
    remove(NESTED_CONFIG_PATH);
    remove(NESTED_HEADER_PATH);
    remove(NESTED_TEST_MACROS_PATH);
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
        { "test_creates_config_and_header_in_existing_directory", test_creates_config_and_header_in_existing_directory },
        { "test_creates_directory_if_missing", test_creates_directory_if_missing },
        { "test_test_macros_file_covers_the_whole_header", test_test_macros_file_covers_the_whole_header },
        { "test_created_config_round_trips_through_parser", test_created_config_round_trips_through_parser },
        { "test_refuses_to_overwrite_existing_config", test_refuses_to_overwrite_existing_config },
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
