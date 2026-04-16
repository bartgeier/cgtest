/*
 * nob.c — Build script for cgtest using Tsoding's nob.h
 *
 * Bootstrap (first time only):
 *   curl -sSfL -o nob.h https://raw.githubusercontent.com/tsoding/nob.h/main/nob.h
 *   gcc -o nob nob.c && ./nob
 *
 *   Windows (MSVC):
 *   curl -sSfL -o nob.h https://raw.githubusercontent.com/tsoding/nob.h/main/nob.h
 *   cl nob.c /Fenob.exe && nob.exe
 *
 * What it does:
 *   1. Creates required directories (source, arq, jsmn, amalgamate)
 *   2. Downloads arq.h, jsmn.h (via curl) if missing
 *   3. Compiles source files into cgtest.exe
 */

#define NOB_IMPLEMENTATION
#include "nob.h"

/* ------------------------------------------------------------------ */
/* Directory structure                                                  */
/* ------------------------------------------------------------------ */

static const char *DIRS[] = {
    "source",
    "arq",
    "jsmn",
};

/* ------------------------------------------------------------------ */
/* Dependencies                                                         */
/* ------------------------------------------------------------------ */

typedef struct { const char *url; const char *dest; } dep_t;

static const dep_t DEPS[] = {
    {
        "https://raw.githubusercontent.com/bartgeier/arq/master/amalgamate/arq.h",
        "arq/arq.h"
    },
    {
        "https://raw.githubusercontent.com/zserge/jsmn/master/jsmn.h",
        "jsmn/jsmn.h"
    },
    /* nob.h is a build prerequisite — must be present before nob.c compiles.
     * See bootstrap instructions at the top of this file. */
};



static bool ensure_dep(const char *url, const char *dest)
{
    Nob_Cmd cmd = {0};
    if (nob_file_exists(dest)) return true;
    nob_log(NOB_INFO, "Downloading %s -> %s", url, dest);
    nob_cmd_append(&cmd, "curl", "-sSfL", "-o", dest, url);
    return nob_cmd_run_sync(cmd);
}

/* ------------------------------------------------------------------ */
/* Source files                                                         */
/* ------------------------------------------------------------------ */

static const char *SRCS[] = {
    "source/cgtest_main.c",
    "source/cgtest_config.c",
    "source/cgtest_lexer.c",
    "source/cgtest_header.c",
    "source/cgtest_runner_gen.c",
};

/* ------------------------------------------------------------------ */
/* Build                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    size_t i;
    Nob_Cmd cmd = {0};

    /* 1. Create directories */
    for (i = 0; i < NOB_ARRAY_LEN(DIRS); i++) {
        if (!nob_mkdir_if_not_exists(DIRS[i])) return 1;
    }

    /* 2. Fetch dependencies */
    for (i = 0; i < NOB_ARRAY_LEN(DEPS); i++) {
        if (!ensure_dep(DEPS[i].url, DEPS[i].dest)) return 1;
    }

    /* 3. Compile cgtest */
    nob_cmd_append(&cmd, "gcc");
    nob_cmd_append(&cmd, "-std=c99");
    nob_cmd_append(&cmd, "-Wall", "-Wextra");
    nob_cmd_append(&cmd, "-O2");
    nob_cmd_append(&cmd, "-Ijsmn", "-Iarq", "-Isource");

    for (i = 0; i < NOB_ARRAY_LEN(SRCS); i++) {
        nob_cmd_append(&cmd, SRCS[i]);
    }

    nob_cmd_append(&cmd, "-o", "cgtest");

    nob_log(NOB_INFO, "Compiling cgtest...");
    if (!nob_cmd_run_sync(cmd)) return 1;
    nob_log(NOB_INFO, "Build successful: ./cgtest");
    return 0;
}
