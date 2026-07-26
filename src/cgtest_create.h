/* cgtest_create.h - see specification.md's "--create" example: writes a
 * template cgtest-config.json plus a (for now empty) cgtest.h alongside
 * it, so a new project has something to edit instead of writing
 * cgtest-config.json from scratch.
 *
 * Both templates are baked into the cgtest binary as string constants
 * rather than shipped as separate files on disk - there is no reliable,
 * portable way to locate a resource file relative to the running
 * executable (install layouts differ across Windows/Linux and between
 * running from a build dir vs. an installed one), so a compiled-in
 * constant sidesteps the problem entirely.
 */
#ifndef CGTEST_CREATE_H
#define CGTEST_CREATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   ok;      /* 0 = failed; see "error" */
    char *error;   /* malloc'd human-readable message, non-NULL only if !ok */
} CGTestCreateResult;

/* Creates a template cgtest-config.json and cgtest.h inside "dir" -
 * "dir" is always a directory, never a file path (e.g. "." creates
 * "./cgtest-config.json" and "./cgtest.h", never a literal file named
 * "."). "dir" is created (via a single mkdir(), not a recursive one -
 * its own parent must already exist) if it doesn't exist yet. Fails -
 * without writing anything - if a cgtest-config.json already exists in
 * "dir" ("If cgtest-config.json already exist than error and exit
 * cgtest.exe", per specification.md).
 */
CGTestCreateResult cgtest_create_run(const char *dir);

/* Releases every owned field in "result". Safe to call on a failed
 * (ok == 0) result too. */
void cgtest_create_free(CGTestCreateResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_CREATE_H */
