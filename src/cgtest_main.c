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
    printf("  cgtest %-30s generate, compile and run cgtest-runner.c\n", "-r, --run <path>");
    printf("  cgtest %-30s also print a scan/generate/compile/run timing breakdown\n", "-r, --run <path> -t, --time");
    printf("  cgtest %-30s create cgtest-project.json, cgtest.h, and an example test inside <dir>/cgtest\n", "-i, --init <dir>");
    printf("  cgtest %-30s print the cgtest version\n", "-v, --version");
    printf("  cgtest %-30s print the cgtest license (MIT)\n", "-l, --license");
    printf("  cgtest %-30s print this message\n", "-h, --help");
    printf("\n");
    printf("https://github.com/bartgeier/cgtest\n");
}

/* Verbatim copy of this repo's own LICENSE file (MIT, covering cgtest
 * itself plus the vendored third_party/arq and third_party/jsmn) - kept
 * in sync by hand, not read from disk, since a downstream build (in
 * particular the single-file cgtest.c amalgamation - see
 * amalgamate_cgtest.c) has no LICENSE file sitting next to it at
 * runtime to read. One printf() per line, matching print_help()'s own
 * style, rather than one big string literal - keeps every individual
 * literal trivially under ISO C90's 509-char limit without needing
 * cgtest_create.c's multi-part template-splitting machinery for
 * something this short. */
static void print_license(void)
{
    printf("MIT License\n");
    printf("\n");
    printf("Copyright (c) 2026 Bernhard Bertrand (https://github.com/bartgeier/cgtest)\n");
    printf("Copyright (c) 2010 Serge A. Zaitsev (https://github.com/zserge/jsmn)\n");
    printf("Copyright (c) 2026 Bernhard Bertrand (https://github.com/bartgeier/arq)\n");
    printf("\n");
    printf("Permission is hereby granted, free of charge, to any person obtaining a copy\n");
    printf("of this software and associated documentation files (the \"Software\"), to deal\n");
    printf("in the Software without restriction, including without limitation the rights\n");
    printf("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
    printf("copies of the Software, and to permit persons to whom the Software is\n");
    printf("furnished to do so, subject to the following conditions:\n");
    printf("\n");
    printf("The above copyright notice and this permission notice shall be included in all\n");
    printf("copies or substantial portions of the Software.\n");
    printf("\n");
    printf("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
    printf("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
    printf("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
    printf("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
    printf("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
    printf("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
    printf("SOFTWARE.\n");
}

/* Printed after --run's own output, only when -t/--time was given (see
 * CGTestArgs::time) - regardless of whether the run itself succeeded,
 * since seeing where time went is useful on a compile failure too
 * (see CGTestRunResult's field comments for what "unreached phase" -
 * printed as 0.0 - means). */
static void print_timing(const CGTestRunResult *result)
{
    printf("scan: %.1fms  generate: %.1fms  compile: %.1fms  run: %.1fms  total: %.1fms\n",
           result->scan_ms, result->generate_ms, result->compile_ms, result->run_ms, result->total_ms);
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

    case CGTEST_ARG_LICENSE:
        print_license();
        exit_code = 0;
        break;

    case CGTEST_ARG_INIT: {
        CGTestCreateResult result = cgtest_create_run(args.init_path);
        if (!result.ok) {
            fprintf(stderr, "cgtest: %s\n", result.error);
            exit_code = 1;
        } else {
            /* Each of the three files is reported individually - see
             * CGTestCreateResult::wrote_project/wrote_header/
             * wrote_test_macros (cgtest_create.h) - since cgtest_create_run()
             * leaves whichever ones already existed untouched instead of
             * always (re)writing all three. cgtest-project.json has two
             * further possible states beyond plain "left unchanged":
             * patched_project (already existing, but missing an optional
             * field a newer cgtest.exe added since it was written -
             * filled in with its default value) and
             * project_could_not_be_checked (already existing, but its
             * shape couldn't be understood well enough to even check for
             * a missing field - e.g. invalid JSON - so nothing was
             * touched; reported distinctly from "left unchanged" so this
             * isn't mistaken for "already up to date"). */
            printf("%s\n", result.dir);
            printf("  cgtest-project.json: %s\n",
                   result.wrote_project ? "created" :
                   result.patched_project ? "already exists, added missing field(s)" :
                   result.project_could_not_be_checked ? "already exists, left unchanged (could not parse it to check for missing fields)" :
                   "already exists, left unchanged");
            printf("  cgtest.h: %s\n", result.wrote_header ? "created" : "already exists, left unchanged");
            printf("  test_cgtest_macros.c: %s\n", result.wrote_test_macros ? "created" : "already exists, left unchanged");
            exit_code = 0;
        }
        cgtest_create_free(&result);
        break;
    }

    case CGTEST_ARG_RUN: {
        CGTestProject project = cgtest_project_load(args.run_path);
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
            if (args.time) {
                print_timing(&result);
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
