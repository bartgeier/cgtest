/* ctestscanner.h - purpose-built scanner, layered on CPreprocessor, that
 * finds test functions of the form:
 *
 *     bool test_<name>(void) { ... }
 *
 * in a test_*.c source buffer, in the order they appear. This is not
 * part of tokenization or directive-awareness themselves (see clexer.h /
 * cpreprocessor.h) - it is a specific consumer built on top of both,
 * matching the layering CPreprocessor already established over CLexer.
 */
#ifndef CTESTSCANNER_H
#define CTESTSCANNER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One discovered test function. "name" is malloc'd and NUL-terminated. */
typedef struct {
    char *name;
    int   line;
} CTestFunction;

/* Scans "length" bytes at "source" for function definitions matching
 * "bool test_<name>(void) {", in order of appearance. Preprocessing
 * directive lines are skipped as a unit via CPreprocessor.
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
