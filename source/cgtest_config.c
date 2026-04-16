/*
 * cgtest_config.c — JSON config loading using jsmn
 */
#include "cgtest_config.h"

#define JSMN_STATIC
#include "jsmn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* File helpers                                                         */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path)
{
    FILE *f;
    long  sz;
    char *buf;

    f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    rewind(f);

    buf = (char *)malloc((size_t)(sz + 1));
    if (!buf) { fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

/* Resolve the parent directory of filepath into out. No trailing slash. */
static void parent_dir(const char *filepath, char *out, size_t out_size)
{
    const char *last = strrchr(filepath, '/');
#ifdef _WIN32
    {
        const char *bs = strrchr(filepath, '\\');
        if (!last || (bs && bs > last)) last = bs;
    }
#endif
    if (!last) {
        strncpy(out, ".", out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        size_t len = (size_t)(last - filepath);
        if (len >= out_size) len = out_size - 1;
        strncpy(out, filepath, len);
        out[len] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* jsmn helpers                                                         */
/* ------------------------------------------------------------------ */

static int tok_eq(const char *json, const jsmntok_t *tok, const char *key)
{
    int len = tok->end - tok->start;
    return tok->type == JSMN_STRING
        && (int)strlen(key) == len
        && strncmp(json + tok->start, key, (size_t)len) == 0;
}

static void tok_copy(const char *json, const jsmntok_t *tok,
                     char *dst, size_t dst_size)
{
    int len = tok->end - tok->start;
    if (len < 0) len = 0;
    if ((size_t)len >= dst_size) len = (int)dst_size - 1;
    strncpy(dst, json + tok->start, (size_t)len);
    dst[len] = '\0';
}

/* ------------------------------------------------------------------ */
/* cgtest_config_load                                                   */
/* ------------------------------------------------------------------ */

int cgtest_config_load(const char *config_path, CgtestConfig *cfg)
{
    char        *json;
    jsmn_parser  parser;
    jsmntok_t    tokens[1024];
    int          n, i;

    memset(cfg, 0, sizeof(*cfg));
    parent_dir(config_path, cfg->base_dir, sizeof(cfg->base_dir));

    json = read_file(config_path);
    if (!json) {
        fprintf(stderr, "cgtest: cannot open config: %s\n", config_path);
        return 1;
    }

    jsmn_init(&parser);
    n = jsmn_parse(&parser, json, strlen(json), tokens, 1024);
    if (n < 1 || tokens[0].type != JSMN_OBJECT) {
        fprintf(stderr, "cgtest: invalid JSON in %s\n", config_path);
        free(json);
        return 1;
    }

    for (i = 1; i < n; i++) {
        if (tok_eq(json, &tokens[i], "cgtest_h")) {
            tok_copy(json, &tokens[++i], cfg->cgtest_h, sizeof(cfg->cgtest_h));

        } else if (tok_eq(json, &tokens[i], "cgtestrunner_output_dir")) {
            tok_copy(json, &tokens[++i],
                     cfg->cgtestrunner_output_dir,
                     sizeof(cfg->cgtestrunner_output_dir));

        } else if (tok_eq(json, &tokens[i], "compiler")) {
            tok_copy(json, &tokens[++i], cfg->compiler, sizeof(cfg->compiler));

        } else if (tok_eq(json, &tokens[i], "compiler_arguments")) {
            int j, count = tokens[++i].size;
            for (j = 0; j < count && cfg->compiler_arguments_count < CGTEST_MAX_ARGS; j++) {
                tok_copy(json, &tokens[++i],
                         cfg->compiler_arguments[cfg->compiler_arguments_count++],
                         sizeof(cfg->compiler_arguments[0]));
            }

        } else if (tok_eq(json, &tokens[i], "include_directories")) {
            int j, count = tokens[++i].size;
            for (j = 0; j < count && cfg->include_directories_count < CGTEST_MAX_DIRS; j++) {
                tok_copy(json, &tokens[++i],
                         cfg->include_directories[cfg->include_directories_count++],
                         sizeof(cfg->include_directories[0]));
            }

        } else if (tok_eq(json, &tokens[i], "source_files")) {
            int j, count = tokens[++i].size;
            for (j = 0; j < count && cfg->source_files_count < CGTEST_MAX_FILES; j++) {
                tok_copy(json, &tokens[++i],
                         cfg->source_files[cfg->source_files_count++],
                         sizeof(cfg->source_files[0]));
            }

        } else if (tok_eq(json, &tokens[i], "test_directories")) {
            int j, count = tokens[++i].size;
            for (j = 0; j < count && cfg->test_directories_count < CGTEST_MAX_DIRS; j++) {
                tok_copy(json, &tokens[++i],
                         cfg->test_directories[cfg->test_directories_count++],
                         sizeof(cfg->test_directories[0]));
            }
        }
    }

    free(json);
    return 0;
}

/* ------------------------------------------------------------------ */
/* cgtest_config_create                                                 */
/* ------------------------------------------------------------------ */

int cgtest_config_create(const char *config_path)
{
    FILE *f;

    f = fopen(config_path, "r");
    if (f) {
        fclose(f);
        fprintf(stderr, "cgtest: file already exists: %s\n", config_path);
        return 1;
    }

    f = fopen(config_path, "w");
    if (!f) {
        fprintf(stderr, "cgtest: cannot create: %s\n", config_path);
        return 1;
    }

    fputs(
        "{\n"
        "  \"cgtest_h\": \".\",\n"
        "  \"cgtestrunner_output_dir\": \"..\",\n"
        "  \"compiler\": \"gcc\",\n"
        "  \"compiler_arguments\": [\n"
        "    \"-std=c99\",\n"
        "    \"-O2\"\n"
        "  ],\n"
        "  \"include_directories\": [],\n"
        "  \"source_files\": [],\n"
        "  \"test_directories\": []\n"
        "}\n",
        f
    );

    fclose(f);
    printf("cgtest: created %s\n", config_path);
    return 0;
}
