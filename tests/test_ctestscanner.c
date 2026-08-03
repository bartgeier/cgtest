/* test_ctestscanner.c - unit tests for ctestscanner_find(), the
 * scanner that locates "void test_<name>(void) { ... }" definitions in
 * a test_*.c source buffer.
 *
 * Written in cgtest's own test convention (void test_<name>(void)) even
 * though cgtest.exe itself doesn't exist yet to run these: main() below
 * stands in for the generated cgtest-runner, calling each test_ function
 * in the order it appears, exactly as the spec describes the runner
 * doing once it exists.
 */
#include "ctestscanner.h"

#include <stdio.h>
#include <string.h>

static int test_failed = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            test_failed = 1; \
            return; \
        } \
    } while (0)

static CTestFunction *scan(const char *src, size_t *out_count)
{
    return ctestscanner_find(src, strlen(src), out_count);
}

void test_finds_single_function(void)
{
    const char *src = "void test_foo(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_foo") == 0);
    CHECK(fns[0].line == 1);

    ctestscanner_free(fns, count);
}

void test_finds_multiple_functions_in_order(void)
{
    const char *src =
        "void test_first(void) { }\n"
        "void test_second(void) { }\n"
        "void test_third(void) { }\n";
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
}

void test_ignores_prototype_declaration(void)
{
    const char *src =
        "void test_foo(void);\n"
        "void test_foo(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_foo") == 0);
    CHECK(fns[0].line == 2);

    ctestscanner_free(fns, count);
}

void test_ignores_non_void_return_type(void)
{
    const char *src = "int test_foo(void) { return 0; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_ignores_bool_return_type(void)
{
    /* The old "bool test_<name>(void)" convention is deliberately no
     * longer matched - "void" is what keeps generated test files
     * C89-portable (no <stdbool.h> required). */
    const char *src = "bool test_foo(void) { return true; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_ignores_name_without_test_prefix(void)
{
    const char *src = "void check_foo(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_ignores_name_with_test_as_substring_not_prefix(void)
{
    /* "testify_foo" does not start with the "test_" prefix (5th char
     * differs: 'i' vs '_'). */
    const char *src = "void testify_foo(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_accepts_exact_test_prefix_as_name(void)
{
    const char *src = "void test_(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_") == 0);

    ctestscanner_free(fns, count);
}

void test_ignores_wrong_parameter_list(void)
{
    const char *src = "void test_foo(int x) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

/* --- fixture form: "void test_<name>(Type *param) { ... }" -------- */

void test_finds_fixture_function_and_captures_type(void)
{
    const char *src = "void test_bar(State *state) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_bar") == 0);
    CHECK(fns[0].fixture_type != NULL);
    CHECK(strcmp(fns[0].fixture_type, "State") == 0);

    ctestscanner_free(fns, count);
}

void test_plain_void_function_has_no_fixture_type(void)
{
    const char *src = "void test_foo(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(fns[0].fixture_type == NULL);

    ctestscanner_free(fns, count);
}

void test_mixes_void_and_fixture_functions_in_order(void)
{
    const char *src =
        "void test_a(void) { }\n"
        "void test_b(State *state) { }\n"
        "void test_c(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 3);
    CHECK(strcmp(fns[0].name, "test_a") == 0);
    CHECK(fns[0].fixture_type == NULL);
    CHECK(strcmp(fns[1].name, "test_b") == 0);
    CHECK(fns[1].fixture_type != NULL);
    CHECK(strcmp(fns[1].fixture_type, "State") == 0);
    CHECK(strcmp(fns[2].name, "test_c") == 0);
    CHECK(fns[2].fixture_type == NULL);

    ctestscanner_free(fns, count);
}

void test_fixture_type_captured_verbatim_for_different_type_name(void)
{
    const char *src = "void test_widget(WidgetFixture *fx) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].fixture_type, "WidgetFixture") == 0);

    ctestscanner_free(fns, count);
}

void test_ignores_fixture_param_without_star(void)
{
    /* "State state" (by value, no pointer) does not match the fixture
     * form - fixtures are always passed by pointer (see
     * specification.md ch.6 "Shape"). */
    const char *src = "void test_bar(State state) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_ignores_fixture_param_with_keyword_type(void)
{
    /* "int *state" is deliberately not treated as a fixture - the type
     * must be a plain (typedef'd) identifier, not a builtin keyword
     * type. */
    const char *src = "void test_bar(int *state) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_ignores_fixture_prototype_declaration(void)
{
    const char *src =
        "void test_bar(State *state);\n"
        "void test_bar(State *state) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_bar") == 0);
    CHECK(strcmp(fns[0].fixture_type, "State") == 0);

    ctestscanner_free(fns, count);
}

void test_handles_fixture_signature_split_across_lines(void)
{
    const char *src =
        "void\n"
        "test_bar\n"
        "(\n"
        "State\n"
        "*\n"
        "state\n"
        ")\n"
        "{\n"
        "}\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_bar") == 0);
    CHECK(strcmp(fns[0].fixture_type, "State") == 0);

    ctestscanner_free(fns, count);
}

void test_handles_signature_split_across_lines(void)
{
    const char *src =
        "void\n"
        "test_split\n"
        "(\n"
        "void\n"
        ")\n"
        "{\n"
        "}\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_split") == 0);
    CHECK(fns[0].line == 2);

    ctestscanner_free(fns, count);
}

void test_handles_comments_between_tokens(void)
{
    const char *src =
        "void /* return type */ test_commented (/* args */ void /* here */)\n"
        "// about to open\n"
        "{ }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_commented") == 0);

    ctestscanner_free(fns, count);
}

void test_skips_include_directive(void)
{
    const char *src =
        "#include <stdio.h>\n"
        "void test_after_include(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_after_include") == 0);
    CHECK(fns[0].line == 2);

    ctestscanner_free(fns, count);
}

void test_line_numbers_correct_after_spliced_directive(void)
{
    /* The #define below spans two physical lines via a backslash
     * continuation; the following test function must still be reported
     * on its real physical line (3), not undercounted to line 2. */
    const char *src =
        "#define ADD(a, b) \\\n"
        "    ((a) + (b))\n"
        "void test_after_define(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_after_define") == 0);
    CHECK(fns[0].line == 3);

    ctestscanner_free(fns, count);
}

void test_finds_function_disabled_by_ifdef(void)
{
    /* Documented limitation: directive LINES are skipped as opaque
     * tokens, but their conditional-compilation MEANING is not
     * evaluated (that is preprocessor phase 4, out of scope for this
     * tokenizer-level scanner). A function textually present inside an
     * #ifdef/#if 0 block is still reported. */
    const char *src =
        "#ifdef DISABLED\n"
        "void test_hidden(void) { }\n"
        "#endif\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_hidden") == 0);

    ctestscanner_free(fns, count);
}

void test_ignores_prototype_followed_by_directive(void)
{
    const char *src =
        "void test_foo(void)\n"
        "#ifdef SOMETHING\n"
        "{ }\n"
        "#endif\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 1);
    CHECK(strcmp(fns[0].name, "test_foo") == 0);

    ctestscanner_free(fns, count);
}

void test_returns_null_on_no_matches(void)
{
    const char *src = "int not_a_test(void) { return 0; }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_returns_null_on_empty_source(void)
{
    size_t count;
    CTestFunction *fns = scan("", &count);

    CHECK(count == 0);
    CHECK(fns == NULL);
}

void test_realistic_file_with_setup_and_teardown(void)
{
    const char *src =
        "#include \"cgtest.h\"\n"
        "\n"
        "void test_setup(void) { }\n"
        "\n"
        "void test_math_add(void)\n"
        "{\n"
        "    EXPECT_TRUE(1 + 1 == 2);\n"
        "}\n"
        "\n"
        "void test_math_sub(void) { EXPECT_TRUE(2 - 1 == 1); }\n"
        "\n"
        "static int helper(void) { return 0; }\n"
        "\n"
        "void test_teardown(void);\n"
        "\n"
        "void test_teardown(void) { }\n";
    size_t count;
    CTestFunction *fns = scan(src, &count);

    CHECK(count == 4);
    CHECK(strcmp(fns[0].name, "test_setup") == 0);
    CHECK(strcmp(fns[1].name, "test_math_add") == 0);
    CHECK(strcmp(fns[2].name, "test_math_sub") == 0);
    CHECK(strcmp(fns[3].name, "test_teardown") == 0);
    CHECK(fns[3].line == 16);

    ctestscanner_free(fns, count);
}

/* --- stand-in test runner: cgtest-runner does not exist yet -------- */

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_finds_single_function", test_finds_single_function },
        { "test_finds_multiple_functions_in_order", test_finds_multiple_functions_in_order },
        { "test_ignores_prototype_declaration", test_ignores_prototype_declaration },
        { "test_ignores_non_void_return_type", test_ignores_non_void_return_type },
        { "test_ignores_bool_return_type", test_ignores_bool_return_type },
        { "test_ignores_name_without_test_prefix", test_ignores_name_without_test_prefix },
        { "test_ignores_name_with_test_as_substring_not_prefix", test_ignores_name_with_test_as_substring_not_prefix },
        { "test_accepts_exact_test_prefix_as_name", test_accepts_exact_test_prefix_as_name },
        { "test_ignores_wrong_parameter_list", test_ignores_wrong_parameter_list },
        { "test_finds_fixture_function_and_captures_type", test_finds_fixture_function_and_captures_type },
        { "test_plain_void_function_has_no_fixture_type", test_plain_void_function_has_no_fixture_type },
        { "test_mixes_void_and_fixture_functions_in_order", test_mixes_void_and_fixture_functions_in_order },
        { "test_fixture_type_captured_verbatim_for_different_type_name", test_fixture_type_captured_verbatim_for_different_type_name },
        { "test_ignores_fixture_param_without_star", test_ignores_fixture_param_without_star },
        { "test_ignores_fixture_param_with_keyword_type", test_ignores_fixture_param_with_keyword_type },
        { "test_ignores_fixture_prototype_declaration", test_ignores_fixture_prototype_declaration },
        { "test_handles_fixture_signature_split_across_lines", test_handles_fixture_signature_split_across_lines },
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
