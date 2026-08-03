/* ctestscanner.h - purpose-built scanner, layered on CPreprocessor, that
 * finds test functions of the form:
 *
 *     void test_<name>(void) { ... }
 *     void test_<name>(Type *param) { ... }
 *
 * in a test_*.c source buffer, in the order they appear. This is not
 * part of tokenization or directive-awareness themselves (see clexer.h /
 * cpreprocessor.h) - it is a specific consumer built on top of both,
 * matching the layering CPreprocessor already established over CLexer.
 *
 * The one-parameter form (see specification.md ch.6 "Fixtures") opts a
 * test into a fixture: "Type" is captured verbatim as
 * CTestFunction::fixture_type so the caller can declare a "Type state;"
 * and pass "&state" through to the test (and its setup_<name>/
 * teardown_<name> counterparts) in the generated runner. Only the type
 * text is captured - no understanding of the type itself, or of
 * whether setup_<name>/teardown_<name> exist, is this module's job
 * (see cgtest_runner.h's pre-compile validation for the latter).
 */
#ifndef CTESTSCANNER_H
#define CTESTSCANNER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One discovered test function. "name" is malloc'd and NUL-terminated.
 * "fixture_type" is malloc'd and NUL-terminated when the function took
 * the one-parameter fixture form (e.g. "State" for "test_bar(State
 * *state)"), or NULL for the plain "(void)" form.
 *
 * "has_teardown" is NOT populated by ctestscanner_find() - a single
 * file's scan can't know whether a teardown_<name> exists elsewhere
 * among a project's other test_*.c files. It always comes back 0 here;
 * cgtest_runner_run() sets it after its own cross-file existence check
 * (see cgtest_runner.h), before cgtest_runner_generate_source() reads
 * it to decide whether to emit a teardown_<name>(&state) call at all -
 * teardown_<name> is optional (specification.md ch.6), unlike
 * setup_<name>. Meaningless when fixture_type is NULL. */
typedef struct {
    char *name;
    char *fixture_type;
    int   has_teardown;
    int   line;
} CTestFunction;

/* Scans "length" bytes at "source" for function definitions matching
 * "void test_<name>(void) {" or "void test_<name>(Type *param) {", in
 * order of appearance. Preprocessing directive lines are skipped as a
 * unit via CPreprocessor.
 *
 * On success returns a malloc'd array of *out_count entries (which may
 * be 0); the caller must release it with ctestscanner_free().
 * Returns NULL on allocation failure or when *out_count is 0.
 */
CTestFunction *ctestscanner_find(const char *source, size_t length, size_t *out_count);

/* Frees an array returned by ctestscanner_find(). */
void ctestscanner_free(CTestFunction *functions, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* CTESTSCANNER_H */
