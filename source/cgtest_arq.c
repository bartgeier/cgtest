#include "cgtest_arq.h"

#define ARQ_IMPLEMENTATION
#include "arq.h"

#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Callback state (arq callbacks are bare function pointers, no ctx)   */
/* ------------------------------------------------------------------ */

static CgtestArgs g_args = {0};

/* ------------------------------------------------------------------ */
/* Callbacks — populate g_args, no side effects                        */
/* ------------------------------------------------------------------ */

static void on_create(Arq_Queue *queue)
{
    g_args.create_path = arq_cstr_t(queue);
}

static void on_config(Arq_Queue *queue)
{
    g_args.config_path = arq_cstr_t(queue);
}

static void on_version(Arq_Queue *queue)
{
    arq_unused(queue);
    printf("Version 1");
    exit(0);
}

static void on_help(Arq_Queue *queue)
{
    arq_unused(queue);
    printf(
        "Help:\n"
        "--config\n"
        "--create\n"
        "-h --help\n"
        "-v --version\n"
    );
    exit(0);
}

/* ------------------------------------------------------------------ */
/* Option table                                                         */
/* ------------------------------------------------------------------ */

static Arq_Option OPTIONS[] = {
    { '\0', "create",  on_create,  "(cstr_t create_path)" },
    { '\0', "config",  on_config,  "(cstr_t config_path)" },
    { 'v',  "version", on_version, "()"            },
    { 'h',  "help",    on_help,    "()"            }
};

#define NUM_OPTIONS (sizeof(OPTIONS) / sizeof(OPTIONS[0]))

/* ------------------------------------------------------------------ */
/* cgtest_arq_parse                                                     */
/* ------------------------------------------------------------------ */

void cgtest_arq_parse(int argc, char **argv, CgtestArgs *args)
{
    char     arena[8192];
    uint32_t errors;

    memset(&g_args, 0, sizeof(g_args));

    errors = arq_verify(arena, sizeof(arena), OPTIONS, NUM_OPTIONS);
    if (errors > 0) {
        printf(arena);
        exit(1);
    }

    errors = arq_fn(argc, argv, arena, sizeof(arena), OPTIONS, NUM_OPTIONS);
    if (errors > 0) {
        printf(arena);
        exit(1);
    }

    if (!((g_args.config_path != NULL) ^ (g_args.create_path != NULL))) {
        printf("no valid arguments, see --help");
        exit(1);
    }

    *args = g_args;
}
