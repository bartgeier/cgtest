/* cgtest_config.c - see cgtest_config.h.
 *
 * Uses jsmn.h (vendored in third_party/, see specification.md) as a flat
 * JSON tokenizer - it does not build a tree, just an array of tokens the
 * caller walks manually. Since every unrecognized key is a hard error
 * (rather than being silently skipped) and none of our 5 known fields'
 * values nest more than one level deep (a string, or an array of
 * strings), that walk stays a simple linear scan - no generic recursive
 * "skip this subtree" logic is needed anywhere.
 */
#define JSMN_STATIC
#include "jsmn.h"

#include "cgtest_config.h"
#include "cpath.h"
#include "cmsg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define CGTEST_GETCWD _getcwd
#else
#include <unistd.h>
#define CGTEST_GETCWD getcwd
#endif

/* Generous relative to any real cgtest-config.json (see cpathlist.c's
 * CPATHLIST_SCRATCH_CAPACITY for the same reasoning): enough tokens for
 * thousands of listed paths, enough path-buffer bytes for any real
 * filesystem path. Exceeding either is reported as a load error, never
 * silently truncated/dropped. */
#define CGTEST_CONFIG_MAX_TOKENS   4096
#define CGTEST_CONFIG_PATH_SCRATCH 4096
#define CGTEST_CONFIG_ERROR_BUFSZ  256

enum {
    CGTEST_FIELD_COMPILER_COMMAND = 0,
    CGTEST_FIELD_INCLUDE_PATHS,
    CGTEST_FIELD_SOURCE_FILES,
    CGTEST_FIELD_OUTPUT_PATH,
    CGTEST_FIELD_TEST_DIRECTORIES,
    CGTEST_FIELD_COUNT
};

static const char *const CGTEST_CONFIG_FIELD_NAMES[CGTEST_FIELD_COUNT] = {
    "compiler_command",
    "include_paths",
    "source_files",
    "output_path",
    "test_directories"
};

/* Cleans up whatever "config" already holds (safe on partially-filled
 * or never-filled fields) and reports "message" as the failure. */
static CGTestConfig cgtest_config_fail(CGTestConfig *config, const char *message)
{
    cpathlist_free(&config->include_paths);
    cpathlist_free(&config->source_files);
    cpathlist_free(&config->test_directories);
    free(config->compiler_command);
    free(config->output_path);

    config->ok = 0;
    config->compiler_command = NULL;
    config->output_path = NULL;
    config->error = cmsg_dup(message, strlen(message));
    return *config;
}

/* Copies a JSMN_STRING token's text into a malloc'd, NUL-terminated,
 * unescaped C string. Supports the common JSON escapes (\" \\ \/ \b \f
 * \n \r \t). \uXXXX is deliberately unsupported - real file paths and
 * compiler commands have no need for it (literal UTF-8 bytes work
 * directly in JSON without escaping) - and is reported as an error
 * rather than silently mishandled. jsmn itself has already validated
 * that any backslash here is followed by one of exactly these 8 chars,
 * so that's the only case this function's own switch needs to reject.
 */
static char *jsmn_token_unescape(const char *json, const jsmntok_t *token,
                                  char *error_buf, size_t error_buf_size)
{
    size_t token_len = (size_t)(token->end - token->start);
    const char *src = json + token->start;
    char *result = (char *)malloc(token_len + 1);
    size_t out = 0;
    size_t i;

    if (result == NULL) {
        cmsg_set(error_buf, error_buf_size, "out of memory");
        return NULL;
    }

    for (i = 0; i < token_len; i++) {
        if (src[i] == '\\' && i + 1 < token_len) {
            i++;
            switch (src[i]) {
            case '\"': result[out++] = '\"'; break;
            case '\\': result[out++] = '\\'; break;
            case '/':  result[out++] = '/';  break;
            case 'b':  result[out++] = '\b'; break;
            case 'f':  result[out++] = '\f'; break;
            case 'n':  result[out++] = '\n'; break;
            case 'r':  result[out++] = '\r'; break;
            case 't':  result[out++] = '\t'; break;
            default:
                free(result);
                cmsg_set(error_buf, error_buf_size,
                    "cgtest-config.json: \\u escapes are not supported (use literal UTF-8 bytes instead)");
                return NULL;
            }
        } else {
            result[out++] = src[i];
        }
    }
    result[out] = '\0';
    return result;
}

static int cgtest_config_match_field(const char *json, const jsmntok_t *token)
{
    size_t token_len = (size_t)(token->end - token->start);
    int i;

    for (i = 0; i < CGTEST_FIELD_COUNT; i++) {
        size_t name_len = strlen(CGTEST_CONFIG_FIELD_NAMES[i]);
        if (token_len == name_len && memcmp(json + token->start, CGTEST_CONFIG_FIELD_NAMES[i], name_len) == 0) {
            return i;
        }
    }
    return -1;
}

static CPathList *cgtest_config_list_for_field(CGTestConfig *config, int field)
{
    switch (field) {
    case CGTEST_FIELD_INCLUDE_PATHS:    return &config->include_paths;
    case CGTEST_FIELD_SOURCE_FILES:     return &config->source_files;
    case CGTEST_FIELD_TEST_DIRECTORIES: return &config->test_directories;
    default:                            return NULL;
    }
}

/* Consumes the value at tokens[value_idx] (already known to belong to
 * "field") into "config". Returns the token index just past everything
 * consumed on success, or -1 with "error_buf" filled in on failure. */
static int cgtest_config_apply_field(CGTestConfig *config, int field, const char *json,
                                      const jsmntok_t *tokens, int token_count, int value_idx,
                                      const char *base_dir, char *error_buf, size_t error_buf_size)
{
    if (field == CGTEST_FIELD_COMPILER_COMMAND || field == CGTEST_FIELD_OUTPUT_PATH) {
        char *raw;

        if (tokens[value_idx].type != JSMN_STRING) {
            cmsg_build(error_buf, error_buf_size, "cgtest-config.json: field \"",
                CGTEST_CONFIG_FIELD_NAMES[field], strlen(CGTEST_CONFIG_FIELD_NAMES[field]), "\" must be a string");
            return -1;
        }

        raw = jsmn_token_unescape(json, &tokens[value_idx], error_buf, error_buf_size);
        if (raw == NULL) {
            return -1;
        }

        if (field == CGTEST_FIELD_COMPILER_COMMAND) {
            config->compiler_command = raw;
        } else {
            char scratch[CGTEST_CONFIG_PATH_SCRATCH];
            CPath joined = cpath_join(scratch, sizeof(scratch), base_dir, raw);
            config->output_path = cmsg_dup(joined.data, joined.length);
            free(raw);
            if (config->output_path == NULL) {
                cmsg_set(error_buf, error_buf_size, "out of memory");
                return -1;
            }
        }
        return value_idx + 1;
    }

    {
        CPathList *list = cgtest_config_list_for_field(config, field);
        int count;
        int i;
        int idx = value_idx;

        if (tokens[idx].type != JSMN_ARRAY) {
            cmsg_build(error_buf, error_buf_size, "cgtest-config.json: field \"",
                CGTEST_CONFIG_FIELD_NAMES[field], strlen(CGTEST_CONFIG_FIELD_NAMES[field]), "\" must be an array of strings");
            return -1;
        }
        count = tokens[idx].size;
        idx++;

        for (i = 0; i < count; i++) {
            char *element;
            CPathListStatus status;

            if (idx >= token_count || tokens[idx].type != JSMN_STRING) {
                cmsg_build(error_buf, error_buf_size, "cgtest-config.json: every element of \"",
                    CGTEST_CONFIG_FIELD_NAMES[field], strlen(CGTEST_CONFIG_FIELD_NAMES[field]), "\" must be a string");
                return -1;
            }

            element = jsmn_token_unescape(json, &tokens[idx], error_buf, error_buf_size);
            if (element == NULL) {
                return -1;
            }

            status = cpathlist_register(list, base_dir, element);
            free(element);
            if (status == CPATHLIST_ALLOC_FAILED) {
                cmsg_set(error_buf, error_buf_size, "out of memory");
                return -1;
            }
            idx++;
        }
        return idx;
    }
}

CGTestConfig cgtest_config_parse(const char *json, size_t length, const char *base_dir)
{
    CGTestConfig config;
    jsmn_parser parser;
    jsmntok_t tokens[CGTEST_CONFIG_MAX_TOKENS];
    int token_count;
    int seen[CGTEST_FIELD_COUNT];
    int idx;
    int pair;
    int i;
    char error_buf[CGTEST_CONFIG_ERROR_BUFSZ];

    memset(&config, 0, sizeof(config));
    cpathlist_init(&config.include_paths);
    cpathlist_init(&config.source_files);
    cpathlist_init(&config.test_directories);

    jsmn_init(&parser);
    token_count = jsmn_parse(&parser, json, length, tokens, CGTEST_CONFIG_MAX_TOKENS);

    if (token_count == JSMN_ERROR_NOMEM) {
        return cgtest_config_fail(&config, "cgtest-config.json has too many entries (exceeds the internal parser limit)");
    }
    if (token_count == JSMN_ERROR_INVAL) {
        return cgtest_config_fail(&config, "cgtest-config.json contains invalid JSON syntax");
    }
    if (token_count == JSMN_ERROR_PART) {
        return cgtest_config_fail(&config, "cgtest-config.json is incomplete (truncated JSON)");
    }
    if (token_count <= 0) {
        return cgtest_config_fail(&config, "cgtest-config.json is empty");
    }
    if (tokens[0].type != JSMN_OBJECT) {
        return cgtest_config_fail(&config, "cgtest-config.json must be a JSON object at the top level");
    }

    for (i = 0; i < CGTEST_FIELD_COUNT; i++) {
        seen[i] = 0;
    }

    idx = 1;
    for (pair = 0; pair < tokens[0].size; pair++) {
        int field;

        if (idx >= token_count || tokens[idx].type != JSMN_STRING) {
            return cgtest_config_fail(&config, "cgtest-config.json: expected a string key");
        }

        field = cgtest_config_match_field(json, &tokens[idx]);
        if (field < 0) {
            cmsg_build(error_buf, sizeof(error_buf), "cgtest-config.json: unknown key \"",
                json + tokens[idx].start, (size_t)(tokens[idx].end - tokens[idx].start), "\"");
            return cgtest_config_fail(&config, error_buf);
        }
        if (seen[field]) {
            cmsg_build(error_buf, sizeof(error_buf), "cgtest-config.json: duplicate key \"",
                CGTEST_CONFIG_FIELD_NAMES[field], strlen(CGTEST_CONFIG_FIELD_NAMES[field]), "\"");
            return cgtest_config_fail(&config, error_buf);
        }
        seen[field] = 1;
        idx++;

        if (idx >= token_count) {
            return cgtest_config_fail(&config, "cgtest-config.json: key is missing its value");
        }

        idx = cgtest_config_apply_field(&config, field, json, tokens, token_count, idx, base_dir,
                                         error_buf, sizeof(error_buf));
        if (idx < 0) {
            return cgtest_config_fail(&config, error_buf);
        }
    }

    for (i = 0; i < CGTEST_FIELD_COUNT; i++) {
        if (!seen[i]) {
            cmsg_build(error_buf, sizeof(error_buf), "cgtest-config.json: missing required field \"",
                CGTEST_CONFIG_FIELD_NAMES[i], strlen(CGTEST_CONFIG_FIELD_NAMES[i]), "\"");
            return cgtest_config_fail(&config, error_buf);
        }
    }

    config.ok = 1;
    config.error = NULL;
    return config;
}

CGTestConfig cgtest_config_load(const char *config_path)
{
    CGTestConfig config;
    char cwd[CGTEST_CONFIG_PATH_SCRATCH];
    char abs_config_scratch[CGTEST_CONFIG_PATH_SCRATCH];
    char file_path_scratch[CGTEST_CONFIG_PATH_SCRATCH];
    char base_dir_scratch[CGTEST_CONFIG_PATH_SCRATCH];
    CPath abs_config;
    CPath file_path;
    CPath base_dir;
    struct stat st;
    FILE *f;
    long size;
    char *buffer;
    size_t read_count;

    memset(&config, 0, sizeof(config));
    cpathlist_init(&config.include_paths);
    cpathlist_init(&config.source_files);
    cpathlist_init(&config.test_directories);

    if (CGTEST_GETCWD(cwd, sizeof(cwd)) == NULL) {
        return cgtest_config_fail(&config, "could not determine the current working directory");
    }

    abs_config = cpath_join(abs_config_scratch, sizeof(abs_config_scratch), cwd, config_path);

    /* "config_path" may name cgtest-config.json directly, or the
     * directory it lives in (matching -C/--create's directory
     * argument) - if it's a directory, look for cgtest-config.json
     * inside it. */
    if (stat(abs_config.data, &st) == 0 && S_ISDIR(st.st_mode)) {
        file_path = cpath_join(file_path_scratch, sizeof(file_path_scratch), abs_config.data, "cgtest-config.json");
    } else {
        file_path = abs_config;
    }

    base_dir = cpath_dirname(base_dir_scratch, sizeof(base_dir_scratch), file_path.data);

    f = fopen(file_path.data, "rb");
    if (f == NULL) {
        char msg[CGTEST_CONFIG_ERROR_BUFSZ];
        cmsg_build(msg, sizeof(msg), "cgtest-config.json not found: ",
                                     file_path.data, file_path.length, "");
        return cgtest_config_fail(&config, msg);
    }

    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return cgtest_config_fail(&config, "could not determine cgtest-config.json's file size");
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(f);
        return cgtest_config_fail(&config, "out of memory reading cgtest-config.json");
    }

    read_count = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(buffer);
        return cgtest_config_fail(&config, "could not read cgtest-config.json");
    }
    buffer[size] = '\0';

    config = cgtest_config_parse(buffer, (size_t)size, base_dir.data);
    free(buffer);
    return config;
}

void cgtest_config_free(CGTestConfig *config)
{
    free(config->error);
    free(config->compiler_command);
    free(config->output_path);
    cpathlist_free(&config->include_paths);
    cpathlist_free(&config->source_files);
    cpathlist_free(&config->test_directories);

    config->error = NULL;
    config->compiler_command = NULL;
    config->output_path = NULL;
}
