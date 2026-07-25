/* test_cpathlist.c - unit tests for cpathlist_register()/cpathlist_free(),
 * the growable list of resolved absolute paths built on cpath_join().
 *
 * Written in cgtest's own test convention (bool test_<name>(void)); see
 * test_ctestscanner.c's header comment for why main() below dispatches
 * them manually instead of via a generated cgtest-runner.
 */
#include "cpathlist.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return false; \
        } \
    } while (0)

bool test_register_single_path(void)
{
    CPathList list;
    CPathListStatus status;

    cpathlist_init(&list);
    status = cpathlist_register(&list, "/home/user/project", "src/main.c");

    CHECK(status == CPATHLIST_OK);
    CHECK(list.count == 1);
    CHECK(strcmp(list.entries[0].path, "/home/user/project/src/main.c") == 0);

    cpathlist_free(&list);
    return true;
}

bool test_register_multiple_paths_preserves_order(void)
{
    CPathList list;

    cpathlist_init(&list);
    CHECK(cpathlist_register(&list, "/a", "one") == CPATHLIST_OK);
    CHECK(cpathlist_register(&list, "/a", "two") == CPATHLIST_OK);
    CHECK(cpathlist_register(&list, "/a", "three") == CPATHLIST_OK);

    CHECK(list.count == 3);
    CHECK(strcmp(list.entries[0].path, "/a/one") == 0);
    CHECK(strcmp(list.entries[1].path, "/a/two") == 0);
    CHECK(strcmp(list.entries[2].path, "/a/three") == 0);

    cpathlist_free(&list);
    return true;
}

bool test_register_grows_past_initial_capacity(void)
{
    /* Initial capacity is 8; register enough entries to force at least
     * one realloc of the entries array, then confirm every entry -
     * including the earliest ones - still has correct content. Growing
     * the array of CPathEntry (pointers) must never disturb the
     * independently owned strings each pointer points to. */
    CPathList list;
    char rel[32];
    size_t i;
    const size_t n = 20;

    cpathlist_init(&list);
    for (i = 0; i < n; i++) {
        sprintf(rel, "file%u", (unsigned)i);
        CHECK(cpathlist_register(&list, "/a", rel) == CPATHLIST_OK);
    }

    CHECK(list.count == n);
    for (i = 0; i < n; i++) {
        char expected[64];
        sprintf(expected, "/a/file%u", (unsigned)i);
        CHECK(strcmp(list.entries[i].path, expected) == 0);
    }

    cpathlist_free(&list);
    return true;
}

bool test_register_reports_truncation(void)
{
    /* A single path segment longer than cpathlist's internal scratch
     * buffer (4096 bytes) must still be registered, but reported as
     * truncated rather than silently cut short. */
    CPathList list;
    static char huge_rel[8192];
    size_t i;
    CPathListStatus status;

    for (i = 0; i < sizeof(huge_rel) - 1; i++) {
        huge_rel[i] = 'a';
    }
    huge_rel[sizeof(huge_rel) - 1] = '\0';

    cpathlist_init(&list);
    status = cpathlist_register(&list, "/a", huge_rel);

    CHECK(status == CPATHLIST_TRUNCATED);
    CHECK(list.count == 1);
    CHECK(strlen(list.entries[0].path) < sizeof(huge_rel));

    cpathlist_free(&list);
    return true;
}

bool test_free_on_never_registered_list_is_safe(void)
{
    CPathList list;

    cpathlist_init(&list);
    cpathlist_free(&list);
    return true;
}

typedef struct {
    const char *name;
    bool (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_register_single_path", test_register_single_path },
        { "test_register_multiple_paths_preserves_order", test_register_multiple_paths_preserves_order },
        { "test_register_grows_past_initial_capacity", test_register_grows_past_initial_capacity },
        { "test_register_reports_truncation", test_register_reports_truncation },
        { "test_free_on_never_registered_list_is_safe", test_free_on_never_registered_list_is_safe }
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
