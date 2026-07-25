/* cpath.c - see cpath.h for the contract this implements. */
#include "cpath.h"

/* Writer state for building the normalized result directly into the
 * caller's buffer. The buffer doubles as its own segment stack: popping
 * a ".." segment just scans backward through what's already been
 * written to find the previous '/' and rewinds the write cursor to it,
 * so no separate scratch storage (and no allocation) is needed.
 */
typedef struct {
    char   *buf;
    size_t  capacity;
    size_t  pos;       /* next write offset */
    size_t  root_len;  /* length of the root prefix already written; never popped past */
    int     truncated;
} CPathWriter;

static int cpath_is_sep(char c)
{
    return c == '/' || c == '\\';
}

static int cpath_is_drive_letter(const char *s)
{
    return ((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z')) && s[1] == ':';
}

/* Length of the root prefix at the start of "path" (1 for "/foo", 3 for
 * "C:/foo"), or 0 if "path" is not absolute. */
static size_t cpath_root_length(const char *path)
{
    if (cpath_is_drive_letter(path) && cpath_is_sep(path[2])) {
        return 3;
    }
    if (cpath_is_sep(path[0])) {
        return 1;
    }
    return 0;
}

static int cpath_is_absolute(const char *path)
{
    return cpath_root_length(path) != 0;
}

static void cpath_put_char(CPathWriter *w, char c)
{
    if (w->truncated) {
        return;
    }
    if (w->pos + 1 >= w->capacity) {
        w->truncated = 1;
        return;
    }
    w->buf[w->pos++] = c;
}

static void cpath_put_root(CPathWriter *w, const char *root_source, size_t root_len)
{
    size_t i;
    for (i = 0; i < root_len; i++) {
        char c = root_source[i];
        cpath_put_char(w, cpath_is_sep(c) ? '/' : c);
    }
    w->root_len = w->pos;
}

static void cpath_push_segment(CPathWriter *w, const char *segment, size_t length)
{
    size_t i;
    if (w->truncated) {
        return;
    }
    if (w->pos > 0 && w->buf[w->pos - 1] != '/') {
        cpath_put_char(w, '/');
    }
    for (i = 0; i < length && !w->truncated; i++) {
        cpath_put_char(w, segment[i]);
    }
}

/* Removes the last written segment, clamping at the root if there is
 * none left to remove (excess ".." beyond the root are dropped). */
static void cpath_pop_segment(CPathWriter *w)
{
    size_t i;
    if (w->truncated || w->pos <= w->root_len) {
        return;
    }
    i = w->pos - 1;
    while (i > w->root_len && w->buf[i - 1] != '/') {
        i--;
    }
    if (i > w->root_len) {
        i--; /* also consume the '/' separating it from the previous segment */
    }
    w->pos = i;
}

/* Splits "s" on '/' and '\' and feeds each segment to the writer,
 * dropping "." segments and popping on ".." segments. */
static void cpath_process(CPathWriter *w, const char *s)
{
    size_t i = 0;
    while (s[i] != '\0' && !w->truncated) {
        size_t start;
        size_t length;

        while (s[i] != '\0' && cpath_is_sep(s[i])) {
            i++;
        }
        if (s[i] == '\0') {
            break;
        }
        start = i;
        while (s[i] != '\0' && !cpath_is_sep(s[i])) {
            i++;
        }
        length = i - start;

        if (length == 1 && s[start] == '.') {
            /* current-directory segment: drop */
        } else if (length == 2 && s[start] == '.' && s[start + 1] == '.') {
            cpath_pop_segment(w);
        } else {
            cpath_push_segment(w, s + start, length);
        }
    }
}

CPath cpath_join(char *buf, size_t capacity, const char *base, const char *rel)
{
    CPathWriter w;
    CPath result;
    const char *root_source;
    size_t root_len;
    int rel_is_absolute;

    w.buf = buf;
    w.capacity = capacity;
    w.pos = 0;
    w.root_len = 0;
    w.truncated = (capacity == 0);

    if (!w.truncated) {
        rel_is_absolute = cpath_is_absolute(rel);
        root_source = rel_is_absolute ? rel : base;
        root_len = cpath_root_length(root_source);

        cpath_put_root(&w, root_source, root_len);
        if (rel_is_absolute) {
            cpath_process(&w, rel + root_len);
        } else {
            cpath_process(&w, base + root_len);
            cpath_process(&w, rel);
        }
        buf[w.pos] = '\0';
    }

    result.data = buf;
    result.length = w.pos;
    result.truncated = w.truncated;
    return result;
}

CPath cpath_dirname(char *buf, size_t capacity, const char *path)
{
    CPathWriter w;
    CPath result;
    size_t root_len = cpath_root_length(path);
    size_t end = root_len;
    size_t last_sep = 0;
    int found_sep = 0;
    size_t i;

    for (i = root_len; path[i] != '\0'; i++) {
        if (cpath_is_sep(path[i])) {
            last_sep = i;
            found_sep = 1;
        }
    }
    if (found_sep) {
        end = last_sep;
    }

    w.buf = buf;
    w.capacity = capacity;
    w.pos = 0;
    w.truncated = (capacity == 0);

    if (!w.truncated) {
        for (i = 0; i < end; i++) {
            cpath_put_char(&w, cpath_is_sep(path[i]) ? '/' : path[i]);
        }
        if (w.pos == 0) {
            cpath_put_char(&w, root_len > 0 ? '/' : '.');
        }
        buf[w.pos] = '\0';
    }

    result.data = buf;
    result.length = w.pos;
    result.truncated = w.truncated;
    return result;
}
