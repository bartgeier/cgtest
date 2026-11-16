#ifndef GEN_COMPILE_COMMANDS_H
#define GEN_COMPILE_COMMANDS_H

#include <stdint.h>
#include <stdlib.h>

/*
 * generate_compile_commands
 *
 * Writes a JSON compilation database named compile_commands.json,
 * which tools like clangd read to learn how each source file is
 * compiled. One entry is written per source file; all entries share
 * the same directory and command.
 *
 *   project_directory_abs_path  absolute path of the project, written
 *                               to every "directory" field
 *   commands                    the compiler command split into words,
 *                               joined with spaces into "command"
 *   source_files                one "file" entry per name
 *   build_path                  directory the file is written to. A path
 *                               starting with '/' (absolute path) 
 *                               is used as it is, any other path is relative to
 *                               project_directory_abs_path. The directory
 *                               must already exist.
 *
 * Returns 0 on success, 1 if the file could not be created or written
 * or if a path is too long (limit 4095 characters).
 *
 * Example output:
 * [
 *   {
 *     "directory": "/home/user/project",
 *     "command": "gcc -DMACRO_ON -I inc -I header -o build/xxx",
 *     "file": "hello.c"
 *   },
 *   {
 *     "directory": "/home/user/project",
 *     "command": "gcc -DMACRO_ON -I inc -I header -o build/xxx",
 *     "file": "world.c"
 *   }
 * ]
 */
int generate_compile_commands(
        char const *project_directory_abs_path,
        const char **commands, const size_t number_of_commands,
        const char **source_files, const size_t number_of_files,
        const char *build_path
);

#endif

/* #define GEN_COMPILE_COMMANDS_IMPLEMENTATION */
#if defined(GEN_COMPILE_COMMANDS_IMPLEMENTATION) && !defined(GEN_COMPILE_COMMANDS_IMPLEMENTATION_DONE)
#define GEN_COMPILE_COMMANDS_IMPLEMENTATION_DONE

#include <string.h>
#include <stdio.h>

typedef struct json_tag {
        FILE *file;
        uint32_t indentation_level;
}Json_t;

/* Joins two path pieces with exactly one '/' between them.
 * An empty piece is skipped. Returns 0 on success, 1 if the result
 * (including the terminating '\0') does not fit into out_size. */
static int join_path(char *out, size_t out_size, const char *a, const char *b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    const char *sep = "/";
    if (len_a == 0 || len_b == 0 || a[len_a - 1] == '/') sep = "";
    if (len_a + strlen(sep) + len_b + 1 > out_size) return 1;
    sprintf(out, "%s%s%s", a, sep, b);
    return 0;
}

static FILE *open_in_dir(const char *dir, const char *name, const char *mode) {
    char path[4096];
    if (join_path(path, sizeof path, dir, name) != 0) return NULL;
    return fopen(path, mode);
}

static void Json_append_char_escaped(Json_t *self, char const chr) {
    switch (chr) {
    case '"':
    case '\\':
        fputc('\\', self->file);
        fputc(chr, self->file);
        break;
    case '\n':
        fputc('\\', self->file);
        fputc('n', self->file);
        break;
    case '\r':
        fputc('\\', self->file);
        fputc('r', self->file);
        break;
    case '\t':
        fputc('\\', self->file);
        fputc('t', self->file);
        break;
    default:
        if ((unsigned char)chr < 0x20) {
            fprintf(self->file, "\\u%04x", (unsigned int)(unsigned char)chr);
        } else {
            fputc(chr, self->file);
        }
        break;
    }
}

static void Json_append_linefeed(Json_t *self) {
    fputc('\n', self->file);
}

static void Json_append_indent(Json_t *self) {
    uint32_t i;
    for (i = 0; i < self->indentation_level; i++) {
        fputs("  ", self->file);
    }
}

static void Json_append_cstring(Json_t *self, char const *cstring) {
    uint32_t i;
    size_t size = strlen(cstring);
    for (i = 0; i < size; i++) {
        Json_append_char_escaped(self, cstring[i]);
    }
}

static void Json_append_begin_struct(Json_t *self) {
    Json_append_indent(self);
    Json_append_cstring(self, "{");
    Json_append_linefeed(self);
    self->indentation_level++;
}

static void Json_append_begin_array(Json_t *self) {
    Json_append_indent(self);
    Json_append_cstring(self, "[");
    Json_append_linefeed(self);
    self->indentation_level++;
}

static void Json_append_end_struct(Json_t *self) {
    self->indentation_level--;
    Json_append_linefeed(self);
    Json_append_indent(self);
    Json_append_char_escaped(self, '}');
}

static void Json_append_end_array(Json_t *self) {
    self->indentation_level--;
    Json_append_linefeed(self);
    Json_append_indent(self);
    Json_append_char_escaped(self, ']');
}

static void Json_append_nextProperty(Json_t *self) {
    fputc(',', self->file);
    Json_append_linefeed(self);
}

static void Json_append_key(Json_t *self, char const *key) {
    Json_append_indent(self);
    fputc('"', self->file);
    Json_append_cstring(self, key);
    fputs("\": ", self->file);
}

static void Json_append_value_TypString(Json_t *self, char const *value) {
    fputc('"', self->file);
    Json_append_cstring(self, value);
    fputc('"', self->file);
}

static void Json_append_values_TypString(Json_t *self, const char **commands, const size_t number_of_commands) {
    uint32_t i;
    fputc('"', self->file);
    for (i = 0; i < number_of_commands; i++) {
        Json_append_cstring(self, commands[i]);
        if (i + 1 < number_of_commands) {
            fputc(' ', self->file);
        }
    }
    fputc('"', self->file);
}


int generate_compile_commands(
    char const *project_directory_abs_path,
    const char **commands, const size_t number_of_commands,
    const char **source_files, const size_t number_of_files,
    const char *build_path
) {
    uint32_t is;
    char directory[4096];
    Json_t json = {
            NULL,         /* file */
            0             /* indentation_level */
    };

    /* an absolute build_path is used as is, a relative one is taken
     * relative to the project directory */
    if (build_path[0] == '/') {
        json.file = open_in_dir(build_path, "compile_commands.json", "w");
    } else {
        if (join_path(directory, sizeof(directory), project_directory_abs_path, build_path) != 0) {
            return 1;
        }
        json.file = open_in_dir(directory, "compile_commands.json", "w");
    }
    if (json.file == NULL) { return 1; }

    Json_append_begin_array(&json);
    for (is = 0; is < number_of_files; is++) {
        Json_append_begin_struct(&json);

        Json_append_key(&json, "directory");
        Json_append_value_TypString(&json, project_directory_abs_path);
        Json_append_nextProperty(&json);

        Json_append_key(&json, "command");
        Json_append_values_TypString(&json, commands, number_of_commands);
        Json_append_nextProperty(&json);
        
        Json_append_key(&json, "file");
        Json_append_value_TypString(&json, source_files[is]);

        Json_append_end_struct(&json);

        if (is + 1 < number_of_files) {
            Json_append_nextProperty(&json);
        } 
    }
    Json_append_end_array(&json);

    if (ferror(json.file)) {
        fclose(json.file);
        return 1;
    }
    if (fclose(json.file) != 0) {
        return 1;
    }
    return 0;
}
#endif
