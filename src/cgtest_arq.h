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
    CGTEST_ARG_RUN,      /* -r/--run <path> was given */
    CGTEST_ARG_INIT      /* -i/--init <path> was given */
} CGTestArgAction;

typedef struct {
    CGTestArgAction  action;
    char const      *run_path;     /* points into argv; set iff action == CGTEST_ARG_RUN */
    char const      *init_path;    /* points into argv; set iff action == CGTEST_ARG_INIT */
    int              time;         /* 1 if -t/--time was given; meaningful only when action == CGTEST_ARG_RUN */
    char            *error;        /* malloc'd human-readable message, non-NULL iff action == CGTEST_ARG_ERROR */
} CGTestArgs;

/* Parses "argv" ("argc" entries, argv[0] the program name as usual)
 * per specification.md: -r/--run <path>, -i/--init <path>,
 * -v/--version, -h/--help. Exactly one of these must be given -
 * combining more than one, or giving none, is reported as an error.
 *
 * -t/--time is a separate modifier flag (not an action of its own):
 * it prints a scan/generate/compile/run timing breakdown alongside
 * -r/--run's normal output (see cgtest_runner.h's CGTestRunResult).
 * Giving it without -r/--run, or alongside -i/-v/-h, is an error - it
 * has nothing to modify otherwise.
 */
CGTestArgs cgtest_arq_parse(int argc, char **argv);

/* Releases every owned field in "args". Safe to call regardless of
 * action. */
void cgtest_arq_free(CGTestArgs *args);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_ARQ_H */
