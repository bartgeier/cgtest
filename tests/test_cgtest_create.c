/* test_cgtest_create.c - unit tests for cgtest_create_run(), which
 * writes a template cgtest-config.json and cgtest.h. Written in
 * cgtest's own test convention (bool test_<name>(void)); see
 * test_ctestscanner.c's header comment for why main() below dispatches
 * them manually instead of via a generated cgtest-runner.
 *
 * Like test_ctestfiles.c, this inherently needs real filesystem
 * interaction, so each test builds a small throw-away fixture
 * directory under build/ and tears it down again.
 */
#include "cgtest_create.h"
#include "cgtest_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return false; \
        } \
    } while (0)

#define FIXTURE_DIR "build/cgtest_create_fixture"
#define CONFIG_PATH FIXTURE_DIR "/cgtest-config.json"
#define HEADER_PATH FIXTURE_DIR "/cgtest.h"

static void setup_fixture(void)
{
    mkdir(FIXTURE_DIR, 0755);
}

static void teardown_fixture(void)
{
    remove(CONFIG_PATH);
    remove(HEADER_PATH);
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

bool test_creates_config_and_header_files(void)
{
    CGTestCreateResult result;
    char buf[4096];

    setup_fixture();
    result = cgtest_create_run(CONFIG_PATH);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(read_whole_file(CONFIG_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(HEADER_PATH, buf, sizeof(buf)) > 0);

    cgtest_create_free(&result);
    teardown_fixture();
    return true;
}

bool test_directory_path_appends_config_filename(void)
{
    CGTestCreateResult result;
    char buf[4096];

    setup_fixture();
    result = cgtest_create_run(FIXTURE_DIR);

    CHECK(result.ok);
    CHECK(result.error == NULL);
    CHECK(read_whole_file(CONFIG_PATH, buf, sizeof(buf)) > 0);
    CHECK(read_whole_file(HEADER_PATH, buf, sizeof(buf)) > 0);

    cgtest_create_free(&result);
    teardown_fixture();
    return true;
}

bool test_created_config_round_trips_through_parser(void)
{
    CGTestCreateResult result;
    CGTestConfig config;
    char buf[4096];
    long length;

    setup_fixture();
    result = cgtest_create_run(CONFIG_PATH);
    CHECK(result.ok);

    length = read_whole_file(CONFIG_PATH, buf, sizeof(buf));
    CHECK(length > 0);

    config = cgtest_config_parse(buf, (size_t)length, "/base");
    CHECK(config.ok);
    CHECK(config.error == NULL);

    cgtest_config_free(&config);
    cgtest_create_free(&result);
    teardown_fixture();
    return true;
}

bool test_refuses_to_overwrite_existing_config(void)
{
    CGTestCreateResult result;
    FILE *f;
    char buf[4096];

    setup_fixture();
    f = fopen(CONFIG_PATH, "w");
    CHECK(f != NULL);
    fputs("PREEXISTING", f);
    fclose(f);

    result = cgtest_create_run(CONFIG_PATH);

    CHECK(!result.ok);
    CHECK(result.error != NULL);
    CHECK(strstr(result.error, "already exists") != NULL);

    CHECK(read_whole_file(CONFIG_PATH, buf, sizeof(buf)) > 0);
    CHECK(strcmp(buf, "PREEXISTING") == 0);

    cgtest_create_free(&result);
    remove(CONFIG_PATH);
    teardown_fixture();
    return true;
}

bool test_missing_directory_is_an_error(void)
{
    CGTestCreateResult result = cgtest_create_run("build/cgtest_create_fixture_missing/cgtest-config.json");

    CHECK(!result.ok);
    CHECK(result.error != NULL);

    cgtest_create_free(&result);
    return true;
}

bool test_free_on_error_is_safe(void)
{
    CGTestCreateResult result = cgtest_create_run("build/cgtest_create_fixture_missing/cgtest-config.json");
    CHECK(!result.ok);
    cgtest_create_free(&result);
    cgtest_create_free(&result);
    return true;
}

typedef struct {
    const char *name;
    bool (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_creates_config_and_header_files", test_creates_config_and_header_files },
        { "test_directory_path_appends_config_filename", test_directory_path_appends_config_filename },
        { "test_created_config_round_trips_through_parser", test_created_config_round_trips_through_parser },
        { "test_refuses_to_overwrite_existing_config", test_refuses_to_overwrite_existing_config },
        { "test_missing_directory_is_an_error", test_missing_directory_is_an_error },
        { "test_free_on_error_is_safe", test_free_on_error_is_safe }
    };
    size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t i;
    size_t failed = 0;

    for (i = 0; i < count; i++) {
        bool ok = cases[i].fn();
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", cases[i].name);
        if (!ok) {
            failed++;
        }
    }

    printf("\n%zu/%zu passed\n", count - failed, count);
    return failed == 0 ? 0 : 1;
}
