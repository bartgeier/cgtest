/* test_ctestfiles.c - unit tests for ctestfiles_scan(), the directory
 * scanner that finds test_*.c files. Written in cgtest's own test
 * convention (bool test_<name>(void)); see test_ctestscanner.c's
 * header comment for why main() below dispatches them manually
 * instead of via a generated cgtest-runner.
 *
 * Unlike every other module tested so far, this one inherently needs
 * real filesystem interaction (there's no in-memory-buffer equivalent
 * of "a directory"), so each test that needs one builds a small
 * throw-away fixture directory under build/ and tears it down again.
 */
#include "ctestfiles.h"

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

#define FIXTURE_DIR "build/ctestfiles_fixture"

static void write_file(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fclose(f);
    }
}

static void setup_fixture(void)
{
    mkdir(FIXTURE_DIR, 0755);
    write_file(FIXTURE_DIR "/test_zulu.c");
    write_file(FIXTURE_DIR "/test_alpha.c");
    write_file(FIXTURE_DIR "/test_mike.c");
    write_file(FIXTURE_DIR "/not_a_test.c");
    write_file(FIXTURE_DIR "/test_wrong_extension.h");
    write_file(FIXTURE_DIR "/readme.txt");
}

static void teardown_fixture(void)
{
    remove(FIXTURE_DIR "/test_zulu.c");
    remove(FIXTURE_DIR "/test_alpha.c");
    remove(FIXTURE_DIR "/test_mike.c");
    remove(FIXTURE_DIR "/not_a_test.c");
    remove(FIXTURE_DIR "/test_wrong_extension.h");
    remove(FIXTURE_DIR "/readme.txt");
    remove(FIXTURE_DIR);
}

bool test_finds_only_matching_files_sorted(void)
{
    CTestFileScan scan;

    setup_fixture();
    scan = ctestfiles_scan(FIXTURE_DIR);

    CHECK(scan.ok);
    CHECK(scan.error == NULL);
    CHECK(scan.files.count == 3);
    CHECK(strcmp(scan.files.entries[0], FIXTURE_DIR "/test_alpha.c") == 0);
    CHECK(strcmp(scan.files.entries[1], FIXTURE_DIR "/test_mike.c") == 0);
    CHECK(strcmp(scan.files.entries[2], FIXTURE_DIR "/test_zulu.c") == 0);

    ctestfiles_free(&scan);
    teardown_fixture();
    return true;
}

bool test_empty_directory_yields_no_files_but_ok(void)
{
    CTestFileScan scan;

    mkdir(FIXTURE_DIR, 0755);
    scan = ctestfiles_scan(FIXTURE_DIR);

    CHECK(scan.ok);
    CHECK(scan.error == NULL);
    CHECK(scan.files.count == 0);

    ctestfiles_free(&scan);
    remove(FIXTURE_DIR);
    return true;
}

bool test_nonexistent_directory_is_an_error(void)
{
    CTestFileScan scan = ctestfiles_scan("build/this_directory_does_not_exist_xyz");

    CHECK(!scan.ok);
    CHECK(scan.error != NULL);
    CHECK(strstr(scan.error, "this_directory_does_not_exist_xyz") != NULL);

    ctestfiles_free(&scan);
    return true;
}

bool test_free_on_failed_scan_is_safe(void)
{
    CTestFileScan scan = ctestfiles_scan("build/also_does_not_exist_xyz");
    CHECK(!scan.ok);
    ctestfiles_free(&scan);
    return true;
}

typedef struct {
    const char *name;
    bool (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_finds_only_matching_files_sorted", test_finds_only_matching_files_sorted },
        { "test_empty_directory_yields_no_files_but_ok", test_empty_directory_yields_no_files_but_ok },
        { "test_nonexistent_directory_is_an_error", test_nonexistent_directory_is_an_error },
        { "test_free_on_failed_scan_is_safe", test_free_on_failed_scan_is_safe }
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
