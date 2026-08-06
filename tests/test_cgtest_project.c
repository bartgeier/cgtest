/* test_cgtest_project.c - unit tests for cgtest_project_parse(), the
 * cgtest-project.json parser. Written in cgtest's own test convention
 * (void test_<name>(void)); see test_ctestscanner.c's header comment
 * for why main() below dispatches them manually instead of via a
 * generated cgtest-runner.
 */
#include "cgtest_project.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int test_failed = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            test_failed = 1; \
            return; \
        } \
    } while (0)

#define FIXTURE_DIR "build/cgtest_project_fixture"
#define PROJECT_PATH FIXTURE_DIR "/cgtest-project.json"

static CGTestProject parse(const char *json)
{
    return cgtest_project_parse(json, strlen(json), "/base");
}

static void write_fixture_project(void)
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
    f = fopen(PROJECT_PATH, "wb");
    fputs(json, f);
    fclose(f);
}

static void remove_fixture_project(void)
{
    remove(PROJECT_PATH);
    remove(FIXTURE_DIR);
}

void test_parses_a_complete_project(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc -std=c99 -O3\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\", \"src/b.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestProject project = parse(json);

    CHECK(project.ok);
    CHECK(project.error == NULL);
    CHECK(strcmp(project.compiler_command, "gcc -std=c99 -O3") == 0);
    CHECK(project.include_paths.count == 1);
    CHECK(strcmp(project.include_paths.entries[0], "/base/src") == 0);
    CHECK(project.source_files.count == 2);
    CHECK(strcmp(project.source_files.entries[0], "/base/src/a.c") == 0);
    CHECK(strcmp(project.source_files.entries[1], "/base/src/b.c") == 0);
    CHECK(strcmp(project.output_path, "/base/build") == 0);
    CHECK(project.test_directories.count == 1);
    CHECK(strcmp(project.test_directories.entries[0], "/base/tests") == 0);

    cgtest_project_free(&project);
}

void test_msvc_defaults_to_false_when_absent(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc -std=c99 -O3\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestProject project = parse(json);

    CHECK(project.ok);
    CHECK(project.msvc == 0);

    cgtest_project_free(&project);
}

void test_msvc_can_be_set_true(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"cl /TC /W4\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"],"
        "\"msvc\": true"
        "}";
    CGTestProject project = parse(json);

    CHECK(project.ok);
    CHECK(project.msvc == 1);

    cgtest_project_free(&project);
}

void test_msvc_wrong_type_is_an_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"],"
        "\"msvc\": \"yes\""
        "}";
    CGTestProject project = parse(json);

    CHECK(!project.ok);
    CHECK(strstr(project.error, "msvc") != NULL);

    cgtest_project_free(&project);
}

void test_single_translation_unit_defaults_to_false_when_absent(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc -std=c99 -O3\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestProject project = parse(json);

    CHECK(project.ok);
    CHECK(project.single_translation_unit == 0);

    cgtest_project_free(&project);
}

void test_single_translation_unit_can_be_set_true(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc -std=c99 -O3\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"],"
        "\"single_translation_unit\": true"
        "}";
    CGTestProject project = parse(json);

    CHECK(project.ok);
    CHECK(project.single_translation_unit == 1);

    cgtest_project_free(&project);
}

void test_single_translation_unit_wrong_type_is_an_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"],"
        "\"single_translation_unit\": \"yes\""
        "}";
    CGTestProject project = parse(json);

    CHECK(!project.ok);
    CHECK(strstr(project.error, "single_translation_unit") != NULL);

    cgtest_project_free(&project);
}

void test_field_order_does_not_matter(void)
{
    const char *json =
        "{"
        "\"test_directories\": [\"tests\"],"
        "\"output_path\": \"./build\","
        "\"source_files\": [\"src/a.c\"],"
        "\"include_paths\": [\"src\"],"
        "\"compiler_command\": \"gcc\""
        "}";
    CGTestProject project = parse(json);

    CHECK(project.ok);
    CHECK(strcmp(project.compiler_command, "gcc") == 0);

    cgtest_project_free(&project);
}

void test_missing_field_is_reported_by_name(void)
{
    const char *json =
        "{"
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestProject project = parse(json);

    CHECK(!project.ok);
    CHECK(project.error != NULL);
    CHECK(strstr(project.error, "compiler_command") != NULL);

    cgtest_project_free(&project);
}

void test_unknown_key_is_a_hard_error(void)
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
    CGTestProject project = parse(json);

    CHECK(!project.ok);
    CHECK(strstr(project.error, "includes_paths") != NULL);

    cgtest_project_free(&project);
}

void test_duplicate_key_is_an_error(void)
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
    CGTestProject project = parse(json);

    CHECK(!project.ok);
    CHECK(strstr(project.error, "compiler_command") != NULL);

    cgtest_project_free(&project);
}

void test_wrong_type_for_string_field_is_an_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": [\"not\", \"a\", \"string\"],"
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestProject project = parse(json);

    CHECK(!project.ok);
    CHECK(strstr(project.error, "compiler_command") != NULL);

    cgtest_project_free(&project);
}

void test_wrong_type_for_array_element_is_an_error(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc\","
        "\"include_paths\": [\"src\", 42],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestProject project = parse(json);

    CHECK(!project.ok);
    CHECK(strstr(project.error, "include_paths") != NULL);

    cgtest_project_free(&project);
}

void test_invalid_json_is_an_error(void)
{
    CGTestProject project = parse("{ this is not json ");

    CHECK(!project.ok);
    CHECK(project.error != NULL);

    cgtest_project_free(&project);
}

void test_empty_json_is_an_error(void)
{
    CGTestProject project = parse("");

    CHECK(!project.ok);
    CHECK(project.error != NULL);

    cgtest_project_free(&project);
}

void test_non_object_top_level_is_an_error(void)
{
    CGTestProject project = parse("[\"just\", \"an\", \"array\"]");

    CHECK(!project.ok);
    CHECK(project.error != NULL);

    cgtest_project_free(&project);
}

void test_string_escapes_are_unescaped(void)
{
    const char *json =
        "{"
        "\"compiler_command\": \"gcc \\\"quoted\\\"\","
        "\"include_paths\": [\"src\"],"
        "\"source_files\": [\"src/a.c\"],"
        "\"output_path\": \"./build\","
        "\"test_directories\": [\"tests\"]"
        "}";
    CGTestProject project = parse(json);

    CHECK(project.ok);
    CHECK(strcmp(project.compiler_command, "gcc \"quoted\"") == 0);

    cgtest_project_free(&project);
}

void test_free_on_failed_project_is_safe(void)
{
    CGTestProject project = parse("not json at all {{{");
    CHECK(!project.ok);
    cgtest_project_free(&project);
}

void test_load_accepts_the_project_file_directly(void)
{
    CGTestProject project;

    write_fixture_project();
    project = cgtest_project_load(PROJECT_PATH);

    CHECK(project.ok);
    CHECK(project.error == NULL);
    CHECK(strcmp(project.compiler_command, "gcc -std=c99 -O3") == 0);

    cgtest_project_free(&project);
    remove_fixture_project();
}

void test_load_accepts_the_containing_directory(void)
{
    CGTestProject project;

    write_fixture_project();
    project = cgtest_project_load(FIXTURE_DIR);

    CHECK(project.ok);
    CHECK(project.error == NULL);
    CHECK(strcmp(project.compiler_command, "gcc -std=c99 -O3") == 0);
    CHECK(project.include_paths.count == 1);
    CHECK(strstr(project.include_paths.entries[0], "/" FIXTURE_DIR "/src") != NULL);

    cgtest_project_free(&project);
    remove_fixture_project();
}

void test_load_missing_project_in_existing_directory_is_an_error(void)
{
    CGTestProject project;

    mkdir(FIXTURE_DIR, 0755);
    project = cgtest_project_load(FIXTURE_DIR);

    CHECK(!project.ok);
    CHECK(project.error != NULL);
    CHECK(strstr(project.error, "cgtest-project.json not found") != NULL);

    cgtest_project_free(&project);
    remove(FIXTURE_DIR);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_parses_a_complete_project", test_parses_a_complete_project },
        { "test_msvc_defaults_to_false_when_absent", test_msvc_defaults_to_false_when_absent },
        { "test_msvc_can_be_set_true", test_msvc_can_be_set_true },
        { "test_msvc_wrong_type_is_an_error", test_msvc_wrong_type_is_an_error },
        { "test_single_translation_unit_defaults_to_false_when_absent", test_single_translation_unit_defaults_to_false_when_absent },
        { "test_single_translation_unit_can_be_set_true", test_single_translation_unit_can_be_set_true },
        { "test_single_translation_unit_wrong_type_is_an_error", test_single_translation_unit_wrong_type_is_an_error },
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
        { "test_free_on_failed_project_is_safe", test_free_on_failed_project_is_safe },
        { "test_load_accepts_the_project_file_directly", test_load_accepts_the_project_file_directly },
        { "test_load_accepts_the_containing_directory", test_load_accepts_the_containing_directory },
        { "test_load_missing_project_in_existing_directory_is_an_error", test_load_missing_project_in_existing_directory_is_an_error }
    };
    size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t i;
    size_t failed = 0;

    for (i = 0; i < count; i++) {
        test_failed = 0;
        cases[i].fn();
        printf("[%s] %s\n", test_failed ? "FAIL" : "PASS", cases[i].name);
        if (test_failed) {
            failed++;
        }
    }

    printf("\n%zu/%zu passed\n", count - failed, count);
    return failed == 0 ? 0 : 1;
}
