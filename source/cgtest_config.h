/*
 * cgtest_config.h — configuration file loading and creation
 */
#ifndef CGTEST_CONFIG_H
#define CGTEST_CONFIG_H

/* ------------------------------------------------------------------ */
/* Limits                                                               */
/* ------------------------------------------------------------------ */

#define CGTEST_MAX_PATH   512
#define CGTEST_MAX_ARGS    32
#define CGTEST_MAX_DIRS    64
#define CGTEST_MAX_FILES  256

/* ------------------------------------------------------------------ */
/* Config struct                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    /* Fields from cgtest-config.json */
    char cgtest_h               [CGTEST_MAX_PATH];
    char cgtestrunner_output_dir[CGTEST_MAX_PATH];
    char compiler               [CGTEST_MAX_PATH];

    char compiler_arguments  [CGTEST_MAX_ARGS ][CGTEST_MAX_PATH];
    int  compiler_arguments_count;

    char include_directories [CGTEST_MAX_DIRS ][CGTEST_MAX_PATH];
    int  include_directories_count;

    char source_files        [CGTEST_MAX_FILES][CGTEST_MAX_PATH];
    int  source_files_count;

    char test_directories    [CGTEST_MAX_DIRS ][CGTEST_MAX_PATH];
    int  test_directories_count;

    /* Resolved: absolute directory that contains the config file */
    char base_dir[CGTEST_MAX_PATH];
} CgtestConfig;

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

/*
 * Load a cgtest-config.json from config_path into *cfg.
 * Returns 0 on success.
 */
int cgtest_config_load(const char *config_path, CgtestConfig *cfg);

/*
 * Write a default cgtest-config.json at config_path.
 * Fails if the file already exists.
 * Returns 0 on success.
 */
int cgtest_config_create(const char *config_path);

#endif /* CGTEST_CONFIG_H */
