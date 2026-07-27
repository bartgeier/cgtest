/* test_cgtest_config.c - unit tests for cgtest_config_parse(), the
 * cgtest-config.json parser. Written in cgtest's own test convention
 * (bool test_<name>(void)); see test_ctestscanner.c's header comment
 * for why main() below dispatches them manually instead of via a
 * generated cgtest-runner.
 */
#include "cgtest_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return false; \
        } \
    } while (0)

#define FIXTURE_DIR "build/cgtest_config_fixture"
#define CONFIG_PATH FIXTURE_DIR "/cgtest-config.json"

static CGTestConfig parse(const char *json)
{
    return cgtest_config_parse(json, strlen(json), "/base");
}

static void write_fixture_config(void)
{
    static const char *const json =
        "{\n"
        "    \"compiler_command\": \"gcc -std=c99 -O3\",\n"
        "    \"include_paths\": [\"src\"],\n"
        "    \"source_files\": [\"src/a.c\"],\n"
        "    \"output_path\": \"./build\",\n"
        "    \"test_directories\": [\"tests\"]\n"
        "}\n";
    FILE *f;

    mkdir(FIXTURE_DIR, 0755);
    f = fopen(CONFIG_PATH, "wb");
    fputs(json, f);
    fclose(f);
}

static void remove_fixture_config(void)
{
    remove(CONFIG_PATH);
    remove(FIXTURE_DIR);
}

bool test_parses_a_complete_config(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc -std=c99 -O3\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\", \"src/b.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestConfig config = parse(json);

    CHECK(config.ok);
    CHECK(config.error == NULL);
    CHECK(strcmp(config.compiler_command, "gcc -std=c99 -O3") == 0);
    CHECK(config.include_paths.count == 1);
    CHECK(strcmp(config.include_paths.entries[0], "/base/src") == 0);
    CHECK(config.source_files.count == 2);
    CHECK(strcmp(config.source_files.entries[0], "/base/src/a.c") == 0);
    CHECK(strcmp(config.source_files.entries[1], "/base/src/b.c") == 0);
    CHECK(strcmp(config.output_path, "/base/build") == 0);
    CHECK(config.test_directories.count == 1);
    CHECK(strcmp(config.test_directories.entries[0], "/base/tests") == 0);

    cgtest_config_free(&config);
    return true;
}

bool test_field_order_does_not_matter(void)
{
    const char *json =
        "{"
        "\"test_directories\": [\"tests\"],"
        "\"output_path\": \"./build\","
        "\"source_files\": [\"src/a.c\"],"
        "\"include_paths\": [\"src\"],"
        "\"compiler_command\": \"gcc\""
        "}";
    CGTestConfig config = parse(json);

    CHECK(config.ok);
    CHECK(strcmp(config.compiler_command, "gcc") == 0);

    cgtest_config_free(&config);
    return true;
}

bool test_missing_field_is_reported_by_name(void)
{
    const char *json =
        "{"
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestConfig config = parse(json);

    CHECK(!config.ok);
    CHECK(config.error != NULL);
    CHECK(strstr(config.error, "compiler_command") != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_unknown_key_is_a_hard_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"],"
        "\"includes_paths\": [\"typo\"]"
        "}";
    CGTestConfig config = parse(json);

    CHECK(!config.ok);
    CHECK(strstr(config.error, "includes_paths") != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_duplicate_key_is_an_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc\","
        "\"compiler_command\": \"clang\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestConfig config = parse(json);

    CHECK(!config.ok);
    CHECK(strstr(config.error, "compiler_command") != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_wrong_type_for_string_field_is_an_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": [\"not\", \"a\", \"string\"],"
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestConfig config = parse(json);

    CHECK(!config.ok);
    CHECK(strstr(config.error, "compiler_command") != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_wrong_type_for_array_element_is_an_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc\","
        "\"include_paths\": [\"src\", 42],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestConfig config = parse(json);

    CHECK(!config.ok);
    CHECK(strstr(config.error, "include_paths") != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_invalid_json_is_an_error(void)
{
    CGTestConfig config = parse("{ this is not json ");

    CHECK(!config.ok);
    CHECK(config.error != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_empty_json_is_an_error(void)
{
    CGTestConfig config = parse("");

    CHECK(!config.ok);
    CHECK(config.error != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_non_object_top_level_is_an_error(void)
{
    CGTestConfig config = parse("[\"just\", \"an\", \"array\"]");

    CHECK(!config.ok);
    CHECK(config.error != NULL);

    cgtest_config_free(&config);
    return true;
}

bool test_string_escapes_are_unescaped(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc \\\"quoted\\\"\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestConfig config = parse(json);

    CHECK(config.ok);
    CHECK(strcmp(config.compiler_command, "gcc \"quoted\"") == 0);

    cgtest_config_free(&config);
    return true;
}

bool test_free_on_failed_config_is_safe(void)
{
    CGTestConfig config = parse("not json at all {{{");
    CHECK(!config.ok);
    cgtest_config_free(&config);
    return true;
}

bool test_load_accepts_the_config_file_directly(void)
{
    CGTestConfig config;

    write_fixture_config();
    config = cgtest_config_load(CONFIG_PATH);

    CHECK(config.ok);
    CHECK(config.error == NULL);
    CHECK(strcmp(config.compiler_command, "gcc -std=c99 -O3") == 0);

    cgtest_config_free(&config);
    remove_fixture_config();
    return true;
}

bool test_load_accepts_the_containing_directory(void)
{
    CGTestConfig config;

    write_fixture_config();
    config = cgtest_config_load(FIXTURE_DIR);

    CHECK(config.ok);
    CHECK(config.error == NULL);
    CHECK(strcmp(config.compiler_command, "gcc -std=c99 -O3") == 0);
    CHECK(config.include_paths.count == 1);
    CHECK(strstr(config.include_paths.entries[0], "/" FIXTURE_DIR "/src") != NULL);

    cgtest_config_free(&config);
    remove_fixture_config();
    return true;
}

bool test_load_missing_config_in_existing_directory_is_an_error(void)
{
    CGTestConfig config;

    mkdir(FIXTURE_DIR, 0755);
    config = cgtest_config_load(FIXTURE_DIR);

    CHECK(!config.ok);
    CHECK(config.error != NULL);
    CHECK(strstr(config.error, "cgtest-config.json not found") != NULL);

    cgtest_config_free(&config);
    remove(FIXTURE_DIR);
    return true;
}

typedef struct {
    const char *name;
    bool (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_parses_a_complete_config", test_parses_a_complete_config },
        { "test_field_order_does_not_matter", test_field_order_does_not_matter },
        { "test_missing_field_is_reported_by_name", test_missing_field_is_reported_by_name },
        { "test_unknown_key_is_a_hard_error", test_unknown_key_is_a_hard_error },
        { "test_duplicate_key_is_an_error", test_duplicate_key_is_an_error },
        { "test_wrong_type_for_string_field_is_an_error", test_wrong_type_for_string_field_is_an_error },
        { "test_wrong_type_for_array_element_is_an_error", test_wrong_type_for_array_element_is_an_error },
        { "test_invalid_json_is_an_error", test_invalid_json_is_an_error },
        { "test_empty_json_is_an_error", test_empty_json_is_an_error },
        { "test_non_object_top_level_is_an_error", test_non_object_top_level_is_an_error },
        { "test_string_escapes_are_unescaped", test_string_escapes_are_unescaped },
        { "test_free_on_failed_config_is_safe", test_free_on_failed_config_is_safe },
        { "test_load_accepts_the_config_file_directly", test_load_accepts_the_config_file_directly },
        { "test_load_accepts_the_containing_directory", test_load_accepts_the_containing_directory },
        { "test_load_missing_config_in_existing_directory_is_an_error", test_load_missing_config_in_existing_directory_is_an_error }
    };
    size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t i;
    size_t failed = 0;

    for (i = 0; i < count; i++) {
        bool ok = cases[i].fn();
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", cases[i].name);
        if (!ok) {
            failed++;
        }
    }

    printf("\n%zu/%zu passed\n", count - failed, count);
    return failed == 0 ? 0 : 1;
}
