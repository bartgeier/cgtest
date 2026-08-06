/* cgtest_arq.c - see cgtest_arq.h.
 *
 * arq's Arq_Option callbacks take only an Arq_Queue*, with no user-data
 * pointer, so - following the same idiom bartgeier/ooo's OArg.c uses
 * for the same library - the callbacks below write through a single
 * file-scope pointer set just before arq_fn() runs. That makes
 * cgtest_arq_parse() non-reentrant, which is fine: it's only ever
 * called once, at the very start of cgtest_main()'s main().
 */
#include "cgtest_arq.h"
#include "cmsg.h"

#define ARQ_IMPLEMENTATION
#include "arq.h"

#include <stdlib.h>
#include <string.h>

#define CGTEST_ARQ_ARENA_SIZE 1024

typedef struct {
    int help;
    int version;
    int license;
    char const *run_path;
    char const *init_path;
    int time;
} CGTestArqRaw;

static CGTestArqRaw *raw_m;

static void fn_help(Arq_Queue *queue)
{
    (void)queue;
    raw_m->help = 1;
}

static void fn_version(Arq_Queue *queue)
{
    (void)queue;
    raw_m->version = 1;
}

static void fn_license(Arq_Queue *queue)
{
    (void)queue;
    raw_m->license = 1;
}

static void fn_run(Arq_Queue *queue)
{
    raw_m->run_path = arq_cstr_t(queue);
}

static void fn_init(Arq_Queue *queue)
{
    raw_m->init_path = arq_cstr_t(queue);
    if (raw_m->init_path == NULL) {
        raw_m->init_path = ".";
    }
}

static void fn_time(Arq_Queue *queue)
{
    (void)queue;
    raw_m->time = 1;
}

static CGTestArgs cgtest_arq_fail(const char *message)
{
    CGTestArgs args;
    args.action = CGTEST_ARG_ERROR;
    args.run_path = NULL;
    args.init_path = NULL;
    args.time = 0;
    args.error = cmsg_dup(message, strlen(message));
    return args;
}

CGTestArgs cgtest_arq_parse(int argc, char **argv)
{
    CGTestArgs args;
    CGTestArqRaw raw;
    char arena[CGTEST_ARQ_ARENA_SIZE];
    int given;
    Arq_Option options[] = {
        { 'h', "help",    fn_help,    "()" },
        { 'v', "version", fn_version, "()" },
        { 'l', "license", fn_license, "()" },
        { 'r', "run",     fn_run,     "(cstr_t path)" },
        { 'i', "init",    fn_init,    "(cstr_t path = NULL)" },
        { 't', "time",    fn_time,    "()" }
    };

    raw.help = 0;
    raw.version = 0;
    raw.license = 0;
    raw.run_path = NULL;
    raw.init_path = NULL;
    raw.time = 0;
    raw_m = &raw;

    if (0 < arq_verify(arena, sizeof(arena), options, sizeof(options) / sizeof(options[0]))) {
        return cgtest_arq_fail(arena);
    }
    if (0 < arq_fn(argc, argv, arena, sizeof(arena), options, sizeof(options) / sizeof(options[0]))) {
        return cgtest_arq_fail(arena);
    }

    given = raw.help + raw.version + raw.license + (raw.run_path != NULL) + (raw.init_path != NULL);
    if (given == 0) {
        return cgtest_arq_fail("no action given; use one of -r, -i, -v, -l or -h");
    }
    if (given > 1) {
        return cgtest_arq_fail("-r, -i, -v, -l and -h cannot be combined");
    }

    args.error = NULL;
    args.run_path = raw.run_path;
    args.init_path = raw.init_path;
    args.time = raw.time;
    if (raw.help) {
        args.action = CGTEST_ARG_HELP;
    } else if (raw.version) {
        args.action = CGTEST_ARG_VERSION;
    } else if (raw.license) {
        args.action = CGTEST_ARG_LICENSE;
    } else if (raw.run_path != NULL) {
        args.action = CGTEST_ARG_RUN;
    } else {
        args.action = CGTEST_ARG_INIT;
    }

    if (raw.time && args.action != CGTEST_ARG_RUN) {
        return cgtest_arq_fail("-t/--time can only be combined with -r/--run");
    }

    return args;
}

void cgtest_arq_free(CGTestArgs *args)
{
    free(args->error);
    args->error = NULL;
}
