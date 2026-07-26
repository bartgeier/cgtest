/* cgtest_main.c - cgtest.exe's entry point (see specification.md).
 *
 * Purely a dispatcher: cgtest_arq_parse() and every module it calls
 * report outcome via a returned struct rather than printing or
 * exiting themselves (see cgtest_arq.h, cgtest_create.h,
 * cgtest_config.h) - this file is the one place allowed to write to
 * stdout/stderr and call exit (via main()'s return value).
 *
 * The RUN action (-c/--config) is not implemented yet: discovering
 * test_*.c files' test_ functions across a whole config, generating
 * cgtest-runner.c, compiling it and running it are all still future
 * work. It is dispatched here as a clearly-labeled stub rather than
 * left out, since -c is already a valid, parsed action.
 */
#include "cgtest_arq.h"
#include "cgtest_create.h"

#include <stdio.h>

#define CGTEST_VERSION "0.1.0"

static void print_help(void)
{
    printf("cgtest - a command-line C unit test DSL compiler\n");
    printf("\n");
    printf("  cgtest --config <path>   generate, compile and run cgtest-runner.c\n");
    printf("  cgtest --create <path>   create a template cgtest-config.json (and cgtest.h) at <path>\n");
    printf("  cgtest --version         print the cgtest version\n");
    printf("  cgtest --help            print this message\n");
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

    case CGTEST_ARG_CREATE: {
        CGTestCreateResult result = cgtest_create_run(args.create_path);
        if (!result.ok) {
            fprintf(stderr, "cgtest: %s\n", result.error);
            exit_code = 1;
        } else {
            printf("created %s\n", args.create_path);
            exit_code = 0;
        }
        cgtest_create_free(&result);
        break;
    }

    case CGTEST_ARG_RUN:
    default:
        fprintf(stderr, "cgtest: --config is not implemented yet\n");
        exit_code = 1;
        break;
    }

    cgtest_arq_free(&args);
    return exit_code;
}
