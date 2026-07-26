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

/* Creates a template cgtest-config.json at "config_path", plus
 * "cgtest.h" alongside it in the same directory. Fails - without
 * writing anything - if a file already exists at "config_path"
 * ("If cgtest-config.json already exist than error and exit
 * cgtest.exe", per specification.md). "config_path"'s directory must
 * already exist; this function never creates directories.
 */
CGTestCreateResult cgtest_create_run(const char *config_path);

/* Releases every owned field in "result". Safe to call on a failed
 * (ok == 0) result too. */
void cgtest_create_free(CGTestCreateResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_CREATE_H */
