/* cgtest_arq.h - parses cgtest.exe's command-line arguments (see
 * specification.md's "Pars command-line arguments" section) into a
 * single CGTestArgs, using the arq.h library
 * (https://github.com/bartgeier/arq) to do the actual argv scanning.
 *
 * Like cgtest_project.h and ctestfiles.h, this module never terminates
 * the process or writes to stdout/stderr itself - it only reports the
 * outcome via the returned struct, leaving printing and exiting to
 * cgtest_main.c.
 */
#ifndef CGTEST_ARQ_H
#define CGTEST_ARQ_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CGTEST_ARG_ERROR,    /* argv was invalid; see CGTestArgs::error */
    CGTEST_ARG_HELP,     /* -h/--help was given */
    CGTEST_ARG_VERSION,  /* -v/--version was given */
    CGTEST_ARG_RUN,      /* -c/--config <path> was given */
    CGTEST_ARG_INIT      /* -i/--init <path> was given */
} CGTestArgAction;

typedef struct {
    CGTestArgAction  action;
    char const      *config_path;  /* points into argv; set iff action == CGTEST_ARG_RUN */
    char const      *init_path;    /* points into argv; set iff action == CGTEST_ARG_INIT */
    char            *error;        /* malloc'd human-readable message, non-NULL iff action == CGTEST_ARG_ERROR */
} CGTestArgs;

/* Parses "argv" ("argc" entries, argv[0] the program name as usual)
 * per specification.md: -c/--config <path>, -i/--init <path>,
 * -v/--version, -h/--help. Exactly one of these must be given -
 * combining more than one, or giving none, is reported as an error.
 */
CGTestArgs cgtest_arq_parse(int argc, char **argv);

/* Releases every owned field in "args". Safe to call regardless of
 * action. */
void cgtest_arq_free(CGTestArgs *args);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_ARQ_H */
