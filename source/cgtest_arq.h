/*
 * cgtest_arq.h — CLI parsing for cgtest using arq
 */
#ifndef CGTEST_ARQ_H
#define CGTEST_ARQ_H


/* ------------------------------------------------------------------ */
/* Parsed arguments                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char const *create_path;  /* set when cmd == CGTEST_CMD_CREATE */
    char const *config_path;  /* set when cmd == CGTEST_CMD_CONFIG */
} CgtestArgs;

/* ------------------------------------------------------------------ */
/* API                                                                */
/* ------------------------------------------------------------------ */

/*
 * Parse argc/argv and populate *args.
 * Returns 0 on success, non-zero if parsing failed (check args->error_msg).
 */
void cgtest_arq_parse(int argc, char **argv, CgtestArgs *args);

#endif /* CGTEST_ARQ_H */
