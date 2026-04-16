/*
 * cgtest_main.c — entry point
 */
#include "cgtest_arq.h"
#include "cgtest_config.h"

#include <stdio.h>

/* ------------------------------------------------------------------ */
/* --create                                                             */
/* ------------------------------------------------------------------ */

static int cmd_create(const char *config_path)
{
    return cgtest_config_create(config_path);
}

/* ------------------------------------------------------------------ */
/* --config                                                             */
/* ------------------------------------------------------------------ */

static int cmd_config(const char *config_path, const char *exe_path)
{
    cgtest_config      cfg;
    cgtest_scan_result scan;
    char               cgtest_h_path[1024];
    int                rc;

    rc = cgtest_config_load(config_path, &cfg);
    if (rc != 0) return rc;

    snprintf(cgtest_h_path, sizeof(cgtest_h_path),
             "%s/%s/cgtest.h", cfg.base_dir, cfg.cgtest_h);

    rc = cgtest_header_install(cgtest_h_path, exe_path, 0);
    if (rc != 0) return rc;

    rc = cgtest_lexer_scan(cfg.base_dir,
                           (const char (*)[512])cfg.test_directories,
                           cfg.test_directories_count,
                           &scan);
    if (rc != 0) return rc;

    if (scan.total_fn_count == 0) {
        printf("cgtest: no test functions found.\n");
        return 0;
    }

    printf("cgtest: found %d test function(s) in %d file(s)\n",
           scan.total_fn_count, scan.file_count);

    rc = cgtest_runner_gen(&cfg, &scan);
    if (rc != 0) return rc;

    return cgtest_runner_compile_and_run(&cfg);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    CgtestArgs args;

    cgtest_arq_parse(argc, argv, &args);

    if (args.create_path != NULL)
        return cmd_create(args.create_path);

    if (args.config_path != NULL)
        return cmd_config(args.config_path, argv[0]);

    return 0;
}
