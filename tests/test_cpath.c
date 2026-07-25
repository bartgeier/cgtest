/* test_cpath.c - unit tests for cpath_join(), the lexical path-joining
 * and normalization function.
 *
 * Written in cgtest's own test convention (bool test_<name>(void)); see
 * test_ctestscanner.c's header comment for why main() below dispatches
 * them manually instead of via a generated cgtest-runner.
 */
#include "cpath.h"

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

bool test_joins_base_and_relative(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/home/user/project", "src/main.c");

    CHECK(strcmp(p.data, "/home/user/project/src/main.c") == 0);
    CHECK(p.length == strlen("/home/user/project/src/main.c"));
    CHECK(!p.truncated);
    return true;
}

bool test_avoids_double_slash_on_trailing_leading_slash(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/home/user/", "file.c");

    CHECK(strcmp(p.data, "/home/user/file.c") == 0);
    CHECK(!p.truncated);
    return true;
}

bool test_directory_rel_joins_same_as_file_rel(void)
{
    /* cpath_join doesn't distinguish files from directories - it's purely
     * lexical - so a directory-shaped rel (with or without a trailing
     * slash) joins exactly like a file-shaped one, and the trailing
     * slash carries no segment of its own. */
    char buf[256];
    CPath with_slash = cpath_join(buf, sizeof(buf), "/home/user/project", "src/include/");
    char buf2[256];
    CPath without_slash = cpath_join(buf2, sizeof(buf2), "/home/user/project", "src/include");

    CHECK(strcmp(with_slash.data, "/home/user/project/src/include") == 0);
    CHECK(strcmp(without_slash.data, with_slash.data) == 0);
    return true;
}

bool test_drops_dot_segments(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/home/user", "./src/./main.c");

    CHECK(strcmp(p.data, "/home/user/src/main.c") == 0);
    return true;
}

bool test_dotdot_climbs_within_bounds(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/a/b/c", "../d");

    CHECK(strcmp(p.data, "/a/b/d") == 0);
    return true;
}

bool test_dotdot_in_base_and_rel_combine(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/a/b/c", "../../x");

    CHECK(strcmp(p.data, "/a/x") == 0);
    return true;
}

bool test_excess_dotdot_clamps_at_root(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/a", "../../..");

    CHECK(strcmp(p.data, "/") == 0);
    CHECK(p.length == 1);
    CHECK(!p.truncated);
    return true;
}

bool test_empty_rel_yields_normalized_base(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/a/./b/../c", "");

    CHECK(strcmp(p.data, "/a/c") == 0);
    return true;
}

bool test_absolute_rel_discards_base(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/home/user", "/etc/passwd");

    CHECK(strcmp(p.data, "/etc/passwd") == 0);
    return true;
}

bool test_absolute_rel_is_still_normalized(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/home/user", "/etc/../etc/./passwd");

    CHECK(strcmp(p.data, "/etc/passwd") == 0);
    return true;
}

bool test_backslashes_normalize_to_forward_slashes(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "C:\\Users\\me", "proj\\file.c");

    CHECK(strcmp(p.data, "C:/Users/me/proj/file.c") == 0);
    return true;
}

bool test_windows_drive_absolute_rel_discards_base(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/home/user", "D:\\other\\file.c");

    CHECK(strcmp(p.data, "D:/other/file.c") == 0);
    return true;
}

bool test_mixed_separators_within_one_path(void)
{
    char buf[256];
    CPath p = cpath_join(buf, sizeof(buf), "/a", "b\\c/d");

    CHECK(strcmp(p.data, "/a/b/c/d") == 0);
    return true;
}

bool test_truncation_is_reported_and_null_terminated(void)
{
    char buf[8];
    CPath p = cpath_join(buf, sizeof(buf), "/home/user/project", "src/main.c");

    CHECK(p.truncated);
    CHECK(p.length == strlen(p.data));
    CHECK(p.length < sizeof(buf));
    return true;
}

bool test_zero_capacity_is_safely_truncated(void)
{
    char buf[1];
    CPath p = cpath_join(buf, 0, "/a", "b");

    CHECK(p.truncated);
    CHECK(p.length == 0);
    return true;
}

bool test_exact_fit_is_not_truncated(void)
{
    char buf[16];
    CPath p;
    const char *expected = "/a/b/c";

    p = cpath_join(buf, strlen(expected) + 1, "/a/b", "c");

    CHECK(!p.truncated);
    CHECK(strcmp(p.data, expected) == 0);
    return true;
}

typedef struct {
    const char *name;
    bool (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_joins_base_and_relative", test_joins_base_and_relative },
        { "test_avoids_double_slash_on_trailing_leading_slash", test_avoids_double_slash_on_trailing_leading_slash },
        { "test_directory_rel_joins_same_as_file_rel", test_directory_rel_joins_same_as_file_rel },
        { "test_drops_dot_segments", test_drops_dot_segments },
        { "test_dotdot_climbs_within_bounds", test_dotdot_climbs_within_bounds },
        { "test_dotdot_in_base_and_rel_combine", test_dotdot_in_base_and_rel_combine },
        { "test_excess_dotdot_clamps_at_root", test_excess_dotdot_clamps_at_root },
        { "test_empty_rel_yields_normalized_base", test_empty_rel_yields_normalized_base },
        { "test_absolute_rel_discards_base", test_absolute_rel_discards_base },
        { "test_absolute_rel_is_still_normalized", test_absolute_rel_is_still_normalized },
        { "test_backslashes_normalize_to_forward_slashes", test_backslashes_normalize_to_forward_slashes },
        { "test_windows_drive_absolute_rel_discards_base", test_windows_drive_absolute_rel_discards_base },
        { "test_mixed_separators_within_one_path", test_mixed_separators_within_one_path },
        { "test_truncation_is_reported_and_null_terminated", test_truncation_is_reported_and_null_terminated },
        { "test_zero_capacity_is_safely_truncated", test_zero_capacity_is_safely_truncated },
        { "test_exact_fit_is_not_truncated", test_exact_fit_is_not_truncated }
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
