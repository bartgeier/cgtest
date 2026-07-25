/* test_ctestscanner.c - unit tests for ctestscanner_find(), the
 * scanner that locates "bool test_<name>(void) { ... }" definitions in
 * a test_*.c source buffer.
 *
 * Written in cgtest's own test convention (bool test_<name>(void)) even
 * though cgtest.exe itself doesn't exist yet to run these: main() below
 * stands in for the generated cgtest-runner, calling each test_ function
 * in the order it appears, exactly as the spec describes the runner
 * doing once it exists.
 */
#include "ctestscanner.h"

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

static CTestFunction *scan(const char *src, size_t *out_count)
{
    return ctestscanner_find(src, strlen(src), out_count);
}

bool test_finds_single_function(void)
{
    const char *src = "bool test_foo(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_foo") == 0);
    CHECK(fns[0].line == 1);

    ctestscanner_free(fns, count);
    return true;
}

bool test_finds_multiple_functions_in_order(void)
{
    const char *src =
        "bool test_first(void) { return true; }\n"
        "bool test_second(void) { return true; }\n"
        "bool test_third(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 3);
    CHECK(strcmp(fns[0].name, "test_first") == 0);
    CHECK(fns[0].line == 1);
    CHECK(strcmp(fns[1].name, "test_second") == 0);
    CHECK(fns[1].line == 2);
    CHECK(strcmp(fns[2].name, "test_third") == 0);
    CHECK(fns[2].line == 3);

    ctestscanner_free(fns, count);
    return true;
}

bool test_ignores_prototype_declaration(void)
{
    const char *src =
        "bool test_foo(void);\n"
        "bool test_foo(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_foo") == 0);
    CHECK(fns[0].line == 2);

    ctestscanner_free(fns, count);
    return true;
}

bool test_ignores_non_bool_return_type(void)
{
    const char *src = "int test_foo(void) { return 0; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
    return true;
}

bool test_ignores_underscore_bool_keyword(void)
{
    /* _Bool (C99/C11) is a distinct keyword from C23's "bool"; only the
     * latter matches the test-function signature. */
    const char *src = "_Bool test_foo(void) { return 1; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
    return true;
}

bool test_ignores_name_without_test_prefix(void)
{
    const char *src = "bool check_foo(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
    return true;
}

bool test_ignores_name_with_test_as_substring_not_prefix(void)
{
    /* "testify_foo" does not start with the "test_" prefix (5th char
     * differs: 'i' vs '_'). */
    const char *src = "bool testify_foo(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
    return true;
}

bool test_accepts_exact_test_prefix_as_name(void)
{
    const char *src = "bool test_(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_") == 0);

    ctestscanner_free(fns, count);
    return true;
}

bool test_ignores_wrong_parameter_list(void)
{
    const char *src = "bool test_foo(int x) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
    return true;
}

bool test_handles_signature_split_across_lines(void)
{
    const char *src =
        "bool\n"
        "test_split\n"
        "(\n"
        "void\n"
        ")\n"
        "{\n"
        "    return true;\n"
        "}\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_split") == 0);
    CHECK(fns[0].line == 2);

    ctestscanner_free(fns, count);
    return true;
}

bool test_handles_comments_between_tokens(void)
{
    const char *src =
        "bool /* return type */ test_commented (/* args */ void /* here */)\n"
        "// about to open\n"
        "{ return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_commented") == 0);

    ctestscanner_free(fns, count);
    return true;
}

bool test_skips_include_directive(void)
{
    const char *src =
        "#include <stdio.h>\n"
        "bool test_after_include(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_after_include") == 0);
    CHECK(fns[0].line == 2);

    ctestscanner_free(fns, count);
    return true;
}

bool test_line_numbers_correct_after_spliced_directive(void)
{
    /* The #define below spans two physical lines via a backslash
     * continuation; the following test function must still be reported
     * on its real physical line (3), not undercounted to line 2. */
    const char *src =
        "#define ADD(a, b) \\\n"
        "    ((a) + (b))\n"
        "bool test_after_define(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_after_define") == 0);
    CHECK(fns[0].line == 3);

    ctestscanner_free(fns, count);
    return true;
}

bool test_finds_function_disabled_by_ifdef(void)
{
    /* Documented limitation: directive LINES are skipped as opaque
     * tokens, but their conditional-compilation MEANING is not
     * evaluated (that is preprocessor phase 4, out of scope for this
     * tokenizer-level scanner). A function textually present inside an
     * #ifdef/#if 0 block is still reported. */
    const char *src =
        "#ifdef DISABLED\n"
        "bool test_hidden(void) { return true; }\n"
        "#endif\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_hidden") == 0);

    ctestscanner_free(fns, count);
    return true;
}

bool test_ignores_prototype_followed_by_directive(void)
{
    const char *src =
        "bool test_foo(void)\n"
        "#ifdef SOMETHING\n"
        "{ return true; }\n"
        "#endif\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_foo") == 0);

    ctestscanner_free(fns, count);
    return true;
}

bool test_returns_null_on_no_matches(void)
{
    const char *src = "int not_a_test(void) { return 0; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
    return true;
}

bool test_returns_null_on_empty_source(void)
{
    size_t count;
    CTestFunction *fns = scan("", &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
    return true;
}

bool test_realistic_file_with_setup_and_teardown(void)
{
    const char *src =
        "#include \"cgtest.h\"\n"
        "\n"
        "bool test_setup(void) { return true; }\n"
        "\n"
        "bool test_math_add(void)\n"
        "{\n"
        "    return 1 + 1 == 2;\n"
        "}\n"
        "\n"
        "bool test_math_sub(void) { return 2 - 1 == 1; }\n"
        "\n"
        "static int helper(void) { return 0; }\n"
        "\n"
        "bool test_teardown(void);\n"
        "\n"
        "bool test_teardown(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 4);
    CHECK(strcmp(fns[0].name, "test_setup") == 0);
    CHECK(strcmp(fns[1].name, "test_math_add") == 0);
    CHECK(strcmp(fns[2].name, "test_math_sub") == 0);
    CHECK(strcmp(fns[3].name, "test_teardown") == 0);
    CHECK(fns[3].line == 16);

    ctestscanner_free(fns, count);
    return true;
}

/* --- stand-in test runner: cgtest-runner does not exist yet -------- */

typedef struct {
    const char *name;
    bool (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_finds_single_function", test_finds_single_function },
        { "test_finds_multiple_functions_in_order", test_finds_multiple_functions_in_order },
        { "test_ignores_prototype_declaration", test_ignores_prototype_declaration },
        { "test_ignores_non_bool_return_type", test_ignores_non_bool_return_type },
        { "test_ignores_underscore_bool_keyword", test_ignores_underscore_bool_keyword },
        { "test_ignores_name_without_test_prefix", test_ignores_name_without_test_prefix },
        { "test_ignores_name_with_test_as_substring_not_prefix", test_ignores_name_with_test_as_substring_not_prefix },
        { "test_accepts_exact_test_prefix_as_name", test_accepts_exact_test_prefix_as_name },
        { "test_ignores_wrong_parameter_list", test_ignores_wrong_parameter_list },
        { "test_handles_signature_split_across_lines", test_handles_signature_split_across_lines },
        { "test_handles_comments_between_tokens", test_handles_comments_between_tokens },
        { "test_skips_include_directive", test_skips_include_directive },
        { "test_line_numbers_correct_after_spliced_directive", test_line_numbers_correct_after_spliced_directive },
        { "test_finds_function_disabled_by_ifdef", test_finds_function_disabled_by_ifdef },
        { "test_ignores_prototype_followed_by_directive", test_ignores_prototype_followed_by_directive },
        { "test_returns_null_on_no_matches", test_returns_null_on_no_matches },
        { "test_returns_null_on_empty_source", test_returns_null_on_empty_source },
        { "test_realistic_file_with_setup_and_teardown", test_realistic_file_with_setup_and_teardown }
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
