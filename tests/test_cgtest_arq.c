/* test_cgtest_arq.c - unit tests for cgtest_arq_parse(), the
 * command-line argument parser built on arq.h. Written in cgtest's own
 * test convention (void test_<name>(void)); see test_ctestscanner.c's
 * header comment for why main() below dispatches them manually instead
 * of via a generated cgtest-runner.
 */
#include "cgtest_arq.h"

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

static CGTestArgs parse(char *argv[])
{
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    return cgtest_arq_parse(argc, argv);
}

void test_short_help_flag(void)
{
    char *argv[] = { "cgtest", "-h", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_HELP);
    CHECK(args.error == NULL);

    cgtest_arq_free(&args);
}

void test_long_help_flag(void)
{
    char *argv[] = { "cgtest", "--help", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_HELP);

    cgtest_arq_free(&args);
}

void test_short_version_flag(void)
{
    char *argv[] = { "cgtest", "-v", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_VERSION);

    cgtest_arq_free(&args);
}

void test_long_version_flag(void)
{
    char *argv[] = { "cgtest", "--version", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_VERSION);

    cgtest_arq_free(&args);
}

void test_short_config_flag_captures_path(void)
{
    char *argv[] = { "cgtest", "-c", "./unittest/cgtest/cgtest-project.json", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_RUN);
    CHECK(args.config_path != NULL);
    CHECK(strcmp(args.config_path, "./unittest/cgtest/cgtest-project.json") == 0);

    cgtest_arq_free(&args);
}

void test_long_config_flag_with_equals_captures_path(void)
{
    char *argv[] = { "cgtest", "--config=./unittest/cgtest/cgtest-project.json", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_RUN);
    CHECK(strcmp(args.config_path, "./unittest/cgtest/cgtest-project.json") == 0);

    cgtest_arq_free(&args);
}

void test_short_init_flag_captures_path(void)
{
    char *argv[] = { "cgtest", "-i", "./unittest/cgtest/cgtest-project.json", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_INIT);
    CHECK(args.init_path != NULL);
    CHECK(strcmp(args.init_path, "./unittest/cgtest/cgtest-project.json") == 0);

    cgtest_arq_free(&args);
}

void test_long_init_flag_captures_path(void)
{
    char *argv[] = { "cgtest", "--init", "./unittest/cgtest/cgtest-project.json", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_INIT);
    CHECK(strcmp(args.init_path, "./unittest/cgtest/cgtest-project.json") == 0);

    cgtest_arq_free(&args);
}

void test_init_flag_without_path_defaults_to_cwd(void)
{
    char *argv[] = { "cgtest", "-i", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_INIT);
    CHECK(args.init_path != NULL);
    CHECK(strcmp(args.init_path, ".") == 0);

    cgtest_arq_free(&args);
}

void test_no_arguments_is_an_error(void)
{
    char *argv[] = { "cgtest", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_ERROR);
    CHECK(args.error != NULL);

    cgtest_arq_free(&args);
}

void test_combining_config_and_version_is_an_error(void)
{
    char *argv[] = { "cgtest", "-c", "./cgtest-project.json", "-v", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_ERROR);
    CHECK(args.error != NULL);

    cgtest_arq_free(&args);
}

void test_combining_config_and_init_is_an_error(void)
{
    char *argv[] = { "cgtest", "-c", "./a.json", "-i", "./b.json", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_ERROR);
    CHECK(args.error != NULL);

    cgtest_arq_free(&args);
}

void test_unknown_flag_is_an_error(void)
{
    char *argv[] = { "cgtest", "--bogus", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_ERROR);
    CHECK(args.error != NULL);

    cgtest_arq_free(&args);
}

void test_config_flag_missing_its_path_is_an_error(void)
{
    char *argv[] = { "cgtest", "-c", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_ERROR);
    CHECK(args.error != NULL);

    cgtest_arq_free(&args);
}

void test_free_on_error_is_safe(void)
{
    char *argv[] = { "cgtest", NULL };
    CGTestArgs args = parse(argv);

    CHECK(args.action == CGTEST_ARG_ERROR);
    cgtest_arq_free(&args);
    cgtest_arq_free(&args);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_short_help_flag", test_short_help_flag },
        { "test_long_help_flag", test_long_help_flag },
        { "test_short_version_flag", test_short_version_flag },
        { "test_long_version_flag", test_long_version_flag },
        { "test_short_config_flag_captures_path", test_short_config_flag_captures_path },
        { "test_long_config_flag_with_equals_captures_path", test_long_config_flag_with_equals_captures_path },
        { "test_short_init_flag_captures_path", test_short_init_flag_captures_path },
        { "test_long_init_flag_captures_path", test_long_init_flag_captures_path },
        { "test_init_flag_without_path_defaults_to_cwd", test_init_flag_without_path_defaults_to_cwd },
        { "test_no_arguments_is_an_error", test_no_arguments_is_an_error },
        { "test_combining_config_and_version_is_an_error", test_combining_config_and_version_is_an_error },
        { "test_combining_config_and_init_is_an_error", test_combining_config_and_init_is_an_error },
        { "test_unknown_flag_is_an_error", test_unknown_flag_is_an_error },
        { "test_config_flag_missing_its_path_is_an_error", test_config_flag_missing_its_path_is_an_error },
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
