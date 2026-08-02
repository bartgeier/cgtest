/* cgtest_main.c - cgtest.exe's entry point (see specification.md).
 *
 * Purely a dispatcher: cgtest_arq_parse() and every module it calls
 * report outcome via a returned struct rather than printing or
 * exiting themselves (see cgtest_arq.h, cgtest_create.h,
 * cgtest_project.h, cgtest_runner.h) - this file is the one place
 * allowed to write to stdout/stderr and call exit (via main()'s
 * return value).
 */
#include "cgtest_arq.h"
#include "cgtest_create.h"
#include "cgtest_project.h"
#include "cgtest_runner.h"

#include <stdio.h>

#define CGTEST_VERSION "0.1.0"

static void print_help(void)
{
    printf("cgtest - a command-line C unit test DSL compiler\n");
    printf("\n");
    printf("  cgtest --config <path>   generate, compile and run cgtest-runner.c\n");
    printf("  cgtest --init <dir>      create cgtest-project.json, cgtest.h, and an example test inside <dir>\n");
    printf("  cgtest --version         print the cgtest version\n");
    printf("  cgtest --help            print this message\n");
    printf("\n");
    printf("https://github.com/bartgeier/cgtest\n");
}

int main(int argc, char **argv)
{
    CGTestArgs args = cgtest_arq_parse(argc, argv);
    int exit_code;

    switch (args.action) {
    case CGTEST_ARG_ERROR:
        fprintf(stderr, "cgtest: %s\n", args.error);
        exit_code = 1;
        break;

    case CGTEST_ARG_HELP:
        print_help();
        exit_code = 0;
        break;

    case CGTEST_ARG_VERSION:
        printf("cgtest %s\n", CGTEST_VERSION);
        exit_code = 0;
        break;

    case CGTEST_ARG_INIT: {
        CGTestCreateResult result = cgtest_create_run(args.init_path);
        if (!result.ok) {
            fprintf(stderr, "cgtest: %s\n", result.error);
            exit_code = 1;
        } else {
            printf("created %s\n", args.init_path);
            exit_code = 0;
        }
        cgtest_create_free(&result);
        break;
    }

    case CGTEST_ARG_RUN: {
        CGTestProject project = cgtest_project_load(args.config_path);
        if (!project.ok) {
            fprintf(stderr, "cgtest: %s\n", project.error);
            exit_code = 1;
        } else {
            CGTestRunResult result = cgtest_runner_run(&project);
            if (!result.ok) {
                fprintf(stderr, "cgtest: %s\n", result.error);
                exit_code = 1;
            } else {
                exit_code = result.exit_code;
            }
            cgtest_runner_free(&result);
        }
        cgtest_project_free(&project);
        break;
    }

    default:
        fprintf(stderr, "cgtest: internal error: unhandled action\n");
        exit_code = 1;
        break;
    }

    cgtest_arq_free(&args);
    return exit_code;
}
