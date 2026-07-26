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
    char const *config_path;
    char const *create_path;
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

static void fn_config(Arq_Queue *queue)
{
    raw_m->config_path = arq_cstr_t(queue);
}

static void fn_create(Arq_Queue *queue)
{
    raw_m->create_path = arq_cstr_t(queue);
}

static CGTestArgs cgtest_arq_fail(const char *message)
{
    CGTestArgs args;
    args.action = CGTEST_ARG_ERROR;
    args.config_path = NULL;
    args.create_path = NULL;
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
        { 'c', "config",  fn_config,  "(cstr_t path)" },
        { 'C', "create",  fn_create,  "(cstr_t path)" }
    };

    raw.help = 0;
    raw.version = 0;
    raw.config_path = NULL;
    raw.create_path = NULL;
    raw_m = &raw;

    if (0 < arq_verify(arena, sizeof(arena), options, sizeof(options) / sizeof(options[0]))) {
        return cgtest_arq_fail(arena);
    }
    if (0 < arq_fn(argc, argv, arena, sizeof(arena), options, sizeof(options) / sizeof(options[0]))) {
        return cgtest_arq_fail(arena);
    }

    given = raw.help + raw.version + (raw.config_path != NULL) + (raw.create_path != NULL);
    if (given == 0) {
        return cgtest_arq_fail("no action given; use one of -c, -C, -v or -h");
    }
    if (given > 1) {
        return cgtest_arq_fail("-c, -C, -v and -h cannot be combined");
    }

    args.error = NULL;
    args.config_path = raw.config_path;
    args.create_path = raw.create_path;
    if (raw.help) {
        args.action = CGTEST_ARG_HELP;
    } else if (raw.version) {
        args.action = CGTEST_ARG_VERSION;
    } else if (raw.config_path != NULL) {
        args.action = CGTEST_ARG_RUN;
    } else {
        args.action = CGTEST_ARG_CREATE;
    }
    return args;
}

void cgtest_arq_free(CGTestArgs *args)
{
    free(args->error);
    args->error = NULL;
}
