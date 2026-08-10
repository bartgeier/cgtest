/* amalgamate_cgtest.c - entry point for the `amalgamate` CLI tool
 * (https://github.com/rindeal/Amalgamate) to produce cgtest.c, a
 * single-file drop-in build of cgtest.exe: no Makefile, no src/ tree,
 * no third_party/ - just download cgtest.c, compile it, run it. Run
 * `make cgtest.c` to (re)generate it after changing anything under
 * src/ or third_party/ - never edit cgtest.c by hand, it's overwritten
 * every time.
 *
 * Order here doesn't affect correctness (each #include'd .c file pulls
 * in its own prototypes via its own header's #include, and the
 * `amalgamate` tool only inlines any given file once regardless of how
 * many times it's #include'd - see cgtest_project.c's JSMN_STATIC use,
 * pulled in once here despite jsmn.h being reachable from more than
 * one file) - listed bottom-up (leaf modules first, cgtest_main.c
 * last) purely so a reader skimming cgtest.c sees dependencies before
 * their users.
 */

/*** Start of inlined file: cpath.c ***/

/*** Start of inlined file: cpath.h ***/
#ifndef CPATH_H
#define CPATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char   *data;       /* == buf passed to cpath_join() */
    size_t  length;     /* bytes written to data, excluding the NUL */
    int     truncated;  /* 1 if capacity was too small to fit the full result */
} CPath;

/* Joins "base" and "rel" into a normalized absolute path, written into
 * "buf" (capacity "capacity" bytes, including room for the NUL
 * terminator). See the file header comment above for exact semantics.
 */
CPath cpath_join(char *buf, size_t capacity, const char *base, const char *rel);

/* Returns the directory portion of "path" (everything before its last
 * '/' or '\'), written into "buf" the same way cpath_join() writes to
 * its buffer (capacity, truncation, NUL-termination all work the same
 * way). Purely lexical, like cpath_join() - "path" need not exist.
 * A path with no separator at all yields "."; the root itself ("/" or
 * "C:/") yields itself, never something shorter.
 */
CPath cpath_dirname(char *buf, size_t capacity, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* CPATH_H */

/*** End of inlined file: cpath.h ***/

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

/*** End of inlined file: cpath.c ***/



/*** Start of inlined file: cpathlist.c ***/

/*** Start of inlined file: cpathlist.h ***/
#ifndef CPATHLIST_H
#define CPATHLIST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "entries[i]" is malloc'd, NUL-terminated, and owned independently of
 * every other entry (see file header comment). */
typedef struct {
    char  **entries;
    size_t  count;
    size_t  capacity;
} CPathList;

typedef enum {
    CPATHLIST_ALLOC_FAILED = 0,  /* nothing was registered */
    CPATHLIST_OK           = 1,
    CPATHLIST_TRUNCATED    = 2   /* registered, but the joined result was cut short */
} CPathListStatus;

/* Prepares "list" to have entries registered into it. */
void cpathlist_init(CPathList *list);

/* Joins "base" and "rel" (see cpath_join()) and appends the normalized
 * absolute result to "list" as a new, independently owned entry.
 */
CPathListStatus cpathlist_register(CPathList *list, const char *base, const char *rel);

/* Releases every entry in "list" and its backing array. Safe to call
 * on a list that was only ever cpathlist_init()'d. */
void cpathlist_free(CPathList *list);

#ifdef __cplusplus
}
#endif

#endif /* CPATHLIST_H */

/*** End of inlined file: cpathlist.h ***/

#include <stdlib.h>
#include <string.h>

/* Generous relative to real filesystem path length limits (Linux's
 * PATH_MAX is typically 4096; Windows historically 260 but modern APIs
 * support far more) - long enough that legitimate project paths never
 * hit it, small enough to be a trivial stack buffer. */
#define CPATHLIST_SCRATCH_CAPACITY 4096

void cpathlist_init(CPathList *list)
{
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

CPathListStatus cpathlist_register(CPathList *list, const char *base, const char *rel)
{
    char scratch[CPATHLIST_SCRATCH_CAPACITY];
    CPath joined;
    char *owned;

    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        char **grown = (char **)realloc(list->entries, new_capacity * sizeof(char *));
        if (grown == NULL) {
            return CPATHLIST_ALLOC_FAILED;
        }
        list->entries = grown;
        list->capacity = new_capacity;
    }

    joined = cpath_join(scratch, sizeof(scratch), base, rel);

    owned = (char *)malloc(joined.length + 1);
    if (owned == NULL) {
        return CPATHLIST_ALLOC_FAILED;
    }
    memcpy(owned, joined.data, joined.length);
    owned[joined.length] = '\0';

    list->entries[list->count] = owned;
    list->count++;

    return joined.truncated ? CPATHLIST_TRUNCATED : CPATHLIST_OK;
}

void cpathlist_free(CPathList *list)
{
    size_t i;
    for (i = 0; i < list->count; i++) {
        free(list->entries[i]);
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

/*** End of inlined file: cpathlist.c ***/


/*** Start of inlined file: cmsg.c ***/

/*** Start of inlined file: cmsg.h ***/
#ifndef CMSG_H
#define CMSG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Writes "prefix" + up to "text_len" bytes of "text" + "suffix" into
 * "buf" (capacity "bufsize"), truncating whichever piece doesn't fit
 * rather than overflowing. Always NUL-terminates if bufsize > 0; does
 * nothing if bufsize == 0. */
void cmsg_build(char *buf, size_t bufsize, const char *prefix, const char *text, size_t text_len, const char *suffix);

/* Equivalent to cmsg_build(buf, bufsize, message, "", 0, ""). */
void cmsg_set(char *buf, size_t bufsize, const char *message);

/* Returns a malloc'd, NUL-terminated copy of the first "length" bytes
 * of "text". Returns NULL only on allocation failure. */
char *cmsg_dup(const char *text, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* CMSG_H */

/*** End of inlined file: cmsg.h ***/

#include <stdlib.h>
#include <string.h>

void cmsg_build(char *buf, size_t bufsize, const char *prefix, const char *text, size_t text_len, const char *suffix)
{
    size_t pos = 0;
    size_t i;

    if (bufsize == 0) {
        return;
    }

    for (i = 0; prefix[i] != '\0' && pos + 1 < bufsize; i++) {
        buf[pos++] = prefix[i];
    }
    for (i = 0; i < text_len && pos + 1 < bufsize; i++) {
        buf[pos++] = text[i];
    }
    for (i = 0; suffix[i] != '\0' && pos + 1 < bufsize; i++) {
        buf[pos++] = suffix[i];
    }
    buf[pos] = '\0';
}

void cmsg_set(char *buf, size_t bufsize, const char *message)
{
    cmsg_build(buf, bufsize, message, "", 0, "");
}

char *cmsg_dup(const char *text, size_t length)
{
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

/*** End of inlined file: cmsg.c ***/


/*** Start of inlined file: clexer.c ***/

/*** Start of inlined file: clexer.h ***/
#ifndef CLEXER_H
#define CLEXER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTOK_EOF = 0,
    CTOK_IDENTIFIER,      /* includes keywords; see CToken::keyword */
    CTOK_PP_NUMBER,       /* C23 6.4.8 pp-number (superset of int/float constants) */
    CTOK_CHAR_CONST,      /* 'x', u'x', U'x', L'x', u8'x' */
    CTOK_STRING_LITERAL,  /* "s", u"s", U"s", L"s", u8"s" */
    CTOK_HEADER_NAME,     /* <...> or "..." - only via clexer_next_header_name() */
    CTOK_PUNCT,
    CTOK_OTHER            /* any non-whitespace char matching no other category */
} CTokenType;

/* Encoding prefix on a character constant or string literal. */
typedef enum {
    CENC_NONE = 0,
    CENC_U8,  /* u8"..."  (u8'...' as of C23) */
    CENC_u,   /* u"..."   u'...'              */
    CENC_U,   /* U"..."   U'...'              */
    CENC_L    /* L"..."   L'...'              */
} CEncodingPrefix;

/* Every reserved keyword through C23. Underscore-prefixed spellings kept
 * for compatibility (_Bool, _Alignas, ...) get their own entries distinct
 * from the C23 convenience spellings (bool, alignas, ...) since both
 * remain valid, separate keywords in C23. */
typedef enum {
    CKW_NONE = 0,
    /* C89/C90 */
    CKW_AUTO, CKW_BREAK, CKW_CASE, CKW_CHAR, CKW_CONST, CKW_CONTINUE,
    CKW_DEFAULT, CKW_DO, CKW_DOUBLE, CKW_ELSE, CKW_ENUM, CKW_EXTERN,
    CKW_FLOAT, CKW_FOR, CKW_GOTO, CKW_IF, CKW_INT, CKW_LONG, CKW_REGISTER,
    CKW_RETURN, CKW_SHORT, CKW_SIGNED, CKW_SIZEOF, CKW_STATIC, CKW_STRUCT,
    CKW_SWITCH, CKW_TYPEDEF, CKW_UNION, CKW_UNSIGNED, CKW_VOID,
    CKW_VOLATILE, CKW_WHILE,
    /* C99 */
    CKW_INLINE, CKW_RESTRICT, CKW_U_BOOL, CKW_U_COMPLEX, CKW_U_IMAGINARY,
    /* C11 */
    CKW_U_ALIGNAS, CKW_U_ALIGNOF, CKW_U_ATOMIC, CKW_U_GENERIC,
    CKW_U_NORETURN, CKW_U_STATIC_ASSERT, CKW_U_THREAD_LOCAL,
    /* C23 */
    CKW_ALIGNAS, CKW_ALIGNOF, CKW_BOOL, CKW_CONSTEXPR, CKW_FALSE,
    CKW_NULLPTR, CKW_STATIC_ASSERT, CKW_THREAD_LOCAL, CKW_TRUE,
    CKW_TYPEOF, CKW_TYPEOF_UNQUAL, CKW_U_BITINT, CKW_U_DECIMAL32,
    CKW_U_DECIMAL64, CKW_U_DECIMAL128
} CKeyword;

/* Every punctuator, digraphs normalized to their canonical spelling's id
 * (e.g. "<:" and "[" both yield CPUNCT_LBRACKET; check CToken::is_digraph
 * to tell which spelling was actually used). */
typedef enum {
    CPUNCT_NONE = 0,
    CPUNCT_LBRACKET, CPUNCT_RBRACKET, CPUNCT_LPAREN, CPUNCT_RPAREN,
    CPUNCT_LBRACE, CPUNCT_RBRACE, CPUNCT_DOT, CPUNCT_ELLIPSIS, CPUNCT_ARROW,
    CPUNCT_PLUSPLUS, CPUNCT_MINUSMINUS, CPUNCT_AMP, CPUNCT_STAR,
    CPUNCT_PLUS, CPUNCT_MINUS, CPUNCT_TILDE, CPUNCT_BANG, CPUNCT_SLASH,
    CPUNCT_PERCENT, CPUNCT_SHL, CPUNCT_SHR, CPUNCT_LT, CPUNCT_GT,
    CPUNCT_LE, CPUNCT_GE, CPUNCT_EQ, CPUNCT_NE, CPUNCT_CARET, CPUNCT_PIPE,
    CPUNCT_AMPAMP, CPUNCT_PIPEPIPE, CPUNCT_QUESTION, CPUNCT_COLON,
    CPUNCT_SEMI, CPUNCT_ASSIGN, CPUNCT_STAR_ASSIGN, CPUNCT_SLASH_ASSIGN,
    CPUNCT_PERCENT_ASSIGN, CPUNCT_PLUS_ASSIGN, CPUNCT_MINUS_ASSIGN,
    CPUNCT_SHL_ASSIGN, CPUNCT_SHR_ASSIGN, CPUNCT_AMP_ASSIGN,
    CPUNCT_CARET_ASSIGN, CPUNCT_PIPE_ASSIGN, CPUNCT_COMMA, CPUNCT_HASH,
    CPUNCT_HASHHASH, CPUNCT_COLONCOLON
} CPunct;

/* A token references a slice of the original source buffer. "start" is
 * not NUL-terminated; use "length" for its extent. */
typedef struct {
    CTokenType      type;
    const char     *start;
    size_t          length;
    int             line;
    int             leading_space;  /* preceded by whitespace and/or a comment */
    int             at_line_start;  /* first token produced on its logical line */
    CKeyword        keyword;        /* CKW_NONE unless type == CTOK_IDENTIFIER and text is a keyword */
    CEncodingPrefix encoding;       /* for CTOK_CHAR_CONST / CTOK_STRING_LITERAL, else CENC_NONE */
    CPunct          punct;          /* for CTOK_PUNCT, else CPUNCT_NONE */
    int             is_digraph;     /* CTOK_PUNCT was spelled as a digraph (e.g. "<:" for "[") */
} CToken;

typedef struct {
    const char *src;
    size_t      length;
    size_t      pos;    /* always a spliced (logical) position, never mid backslash-newline */
    int         line;
    int         started; /* 0 until the first token has been produced */
} CLexer;

/* Prepares "lexer" to scan "length" bytes starting at "source". */
void clexer_init(CLexer *lexer, const char *source, size_t length);

/* Reads and returns the next preprocessing-token. Handles backslash-newline
 * line splicing transparently anywhere in the source (not just inside
 * directives), skips whitespace and both comment styles, and performs
 * maximal-munch tokenization. End of input is signaled by the returned
 * token's type being CTOK_EOF - keep calling this in a loop until you see
 * one.
 */
CToken clexer_next_token(CLexer *lexer);

/* Like clexer_next_token(), but if the next significant character is '<'
 * or '"', it is scanned as a CTOK_HEADER_NAME token (the h-char-sequence
 * or q-char-sequence form) instead of ordinary tokens. Use this only when
 * the caller has determined - from previously read tokens - that it is
 * positioned right after a "# include" (or "# __has_include") directive
 * token, since header-name is a context-sensitive token per the standard.
 * Falls back to clexer_next_token()'s behavior otherwise.
 */
CToken clexer_next_header_name(CLexer *lexer);

/* Convenience helpers for matching a token's raw source text against a
 * NUL-terminated string (exact spelling, e.g. won't equate a digraph with
 * its canonical form - use token->punct for that). */
int clexer_token_equals(const CToken *token, const char *text);
int clexer_token_starts_with(const CToken *token, const char *prefix);

/* Human-readable names, mainly for diagnostics/debugging. */
const char *clexer_keyword_name(CKeyword keyword);
const char *clexer_punct_name(CPunct punct);

#ifdef __cplusplus
}
#endif

#endif /* CLEXER_H */

/*** End of inlined file: clexer.h ***/

#include <string.h>

/* ------------------------------------------------------------------ */
/* Character stream, with transparent phase-2 line splicing.           */
/* ------------------------------------------------------------------ */

/* Length, in bytes, of a backslash-newline (or backslash-CR-LF) splice
 * starting exactly at "pos", or 0 if there is none there. */
static size_t clexer_splice_len(const CLexer *lexer, size_t pos)
{
    if (pos + 1 < lexer->length && lexer->src[pos] == '\\') {
        if (lexer->src[pos + 1] == '\n') {
            return 2;
        }
        if (pos + 2 < lexer->length && lexer->src[pos + 1] == '\r' && lexer->src[pos + 2] == '\n') {
            return 3;
        }
    }
    return 0;
}

/* Advances "pos" past any run of consecutive splices starting there. */
static size_t clexer_skip_splices(const CLexer *lexer, size_t pos)
{
    size_t splice;
    while ((splice = clexer_splice_len(lexer, pos)) != 0) {
        pos += splice;
    }
    return pos;
}

static int clexer_current(const CLexer *lexer)
{
    size_t pos = clexer_skip_splices(lexer, lexer->pos);
    if (pos >= lexer->length) {
        return -1;
    }
    return (unsigned char)lexer->src[pos];
}

/* Logical lookahead: the character "n" positions after the current one,
 * with splices transparently skipped both before and between them. */
static int clexer_peek(const CLexer *lexer, size_t n)
{
    size_t pos = clexer_skip_splices(lexer, lexer->pos);
    size_t i;

    for (i = 0; i < n; i++) {
        if (pos >= lexer->length) {
            return -1;
        }
        pos = clexer_skip_splices(lexer, pos + 1);
    }

    if (pos >= lexer->length) {
        return -1;
    }
    return (unsigned char)lexer->src[pos];
}

/* Consumes exactly one logical character (skipping any splice that
 * precedes it), keeping lexer->pos always at a spliced position.
 *
 * Each spliced backslash-newline still corresponds to one physical
 * source line, so it must bump lexer->line too - otherwise every line
 * number reported after a backslash-continued line (e.g. inside a
 * multi-line #define) would undercount by the number of splices. */
static void clexer_advance(CLexer *lexer)
{
    size_t pos = lexer->pos;
    size_t splice;

    while ((splice = clexer_splice_len(lexer, pos)) != 0) {
        pos += splice;
        lexer->line++;
    }

    if (pos >= lexer->length) {
        lexer->pos = lexer->length;
        return;
    }
    if (lexer->src[pos] == '\n') {
        lexer->line++;
    }
    lexer->pos = pos + 1;
}

void clexer_init(CLexer *lexer, const char *source, size_t length)
{
    lexer->src = source;
    lexer->length = length;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->started = 0;
}

/* ------------------------------------------------------------------ */
/* Trivia: whitespace and both comment styles.                         */
/* ------------------------------------------------------------------ */

/* Skips whitespace and comments. Reports whether anything was skipped
 * (leading_space) and whether a real newline was crossed outside of any
 * comment (had_newline) - a newline hidden inside a slash-star comment
 * does NOT count, matching the standard's comment-first-then-directives
 * translation order. */
static int clexer_skip_trivia(CLexer *lexer, int *out_had_newline)
{
    int had_space = 0;
    int had_newline = 0;

    for (;;) {
        int c = clexer_current(lexer);

        if (c < 0) {
            break;
        }

        if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
            clexer_advance(lexer);
            had_space = 1;
            continue;
        }

        if (c == '\n') {
            clexer_advance(lexer);
            had_space = 1;
            had_newline = 1;
            continue;
        }

        if (c == '/' && clexer_peek(lexer, 1) == '/') {
            while (clexer_current(lexer) >= 0 && clexer_current(lexer) != '\n') {
                clexer_advance(lexer);
            }
            had_space = 1;
            continue;
        }

        if (c == '/' && clexer_peek(lexer, 1) == '*') {
            clexer_advance(lexer);
            clexer_advance(lexer);
            while (clexer_current(lexer) >= 0 &&
                   !(clexer_current(lexer) == '*' && clexer_peek(lexer, 1) == '/')) {
                clexer_advance(lexer);
            }
            if (clexer_current(lexer) >= 0) {
                clexer_advance(lexer); /* '*' */
                clexer_advance(lexer); /* '/' */
            }
            had_space = 1;
            continue;
        }

        break;
    }

    if (out_had_newline != NULL) {
        *out_had_newline = had_newline;
    }
    return had_space;
}

/* ------------------------------------------------------------------ */
/* Identifiers, including universal-character-names and (permissively)  */
/* raw extended (UTF-8) identifier bytes.                                */
/* ------------------------------------------------------------------ */

static int clexer_is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int clexer_is_hex_digit(int c)
{
    return clexer_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int clexer_is_ascii_ident_start(int c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int clexer_is_ascii_ident_continue(int c)
{
    return clexer_is_ascii_ident_start(c) || clexer_is_digit(c);
}

/* C23 permits identifier characters outside the basic character set
 * (6.4.2, Annex D). Rather than encoding the exact Unicode ranges, any
 * UTF-8 lead/continuation byte is accepted; see clexer.h's scope notes. */
static int clexer_is_extended_ident_byte(int c)
{
    return c >= 0x80 && c <= 0xff;
}

/* Length, in source characters, of a valid universal-character-name
 * (\uXXXX or \UXXXXXXXX) starting at the current position, or 0. */
static size_t clexer_ucn_len(const CLexer *lexer)
{
    int second;
    size_t hex_count;
    size_t i;

    if (clexer_current(lexer) != '\\') {
        return 0;
    }
    second = clexer_peek(lexer, 1);
    if (second == 'u') {
        hex_count = 4;
    } else if (second == 'U') {
        hex_count = 8;
    } else {
        return 0;
    }
    for (i = 0; i < hex_count; i++) {
        if (!clexer_is_hex_digit(clexer_peek(lexer, 2 + i))) {
            return 0;
        }
    }
    return 2 + hex_count;
}

static int clexer_is_ident_start_here(const CLexer *lexer)
{
    int c = clexer_current(lexer);
    return clexer_is_ascii_ident_start(c) ||
           clexer_is_extended_ident_byte(c) ||
           clexer_ucn_len(lexer) > 0;
}

static void clexer_scan_identifier(CLexer *lexer)
{
    for (;;) {
        int c = clexer_current(lexer);
        size_t ucn;

        if (clexer_is_ascii_ident_continue(c) || clexer_is_extended_ident_byte(c)) {
            clexer_advance(lexer);
            continue;
        }
        ucn = clexer_ucn_len(lexer);
        if (ucn > 0) {
            size_t i;
            for (i = 0; i < ucn; i++) {
                clexer_advance(lexer);
            }
            continue;
        }
        break;
    }
}

typedef struct {
    const char *text;
    CKeyword    keyword;
} CKeywordEntry;

static const CKeywordEntry clexer_keywords[] = {
    /* C89/C90 */
    { "auto", CKW_AUTO }, { "break", CKW_BREAK }, { "case", CKW_CASE },
    { "char", CKW_CHAR }, { "const", CKW_CONST }, { "continue", CKW_CONTINUE },
    { "default", CKW_DEFAULT }, { "do", CKW_DO }, { "double", CKW_DOUBLE },
    { "else", CKW_ELSE }, { "enum", CKW_ENUM }, { "extern", CKW_EXTERN },
    { "float", CKW_FLOAT }, { "for", CKW_FOR }, { "goto", CKW_GOTO },
    { "if", CKW_IF }, { "int", CKW_INT }, { "long", CKW_LONG },
    { "register", CKW_REGISTER }, { "return", CKW_RETURN }, { "short", CKW_SHORT },
    { "signed", CKW_SIGNED }, { "sizeof", CKW_SIZEOF }, { "static", CKW_STATIC },
    { "struct", CKW_STRUCT }, { "switch", CKW_SWITCH }, { "typedef", CKW_TYPEDEF },
    { "union", CKW_UNION }, { "unsigned", CKW_UNSIGNED }, { "void", CKW_VOID },
    { "volatile", CKW_VOLATILE }, { "while", CKW_WHILE },
    /* C99 */
    { "inline", CKW_INLINE }, { "restrict", CKW_RESTRICT },
    { "_Bool", CKW_U_BOOL }, { "_Complex", CKW_U_COMPLEX }, { "_Imaginary", CKW_U_IMAGINARY },
    /* C11 */
    { "_Alignas", CKW_U_ALIGNAS }, { "_Alignof", CKW_U_ALIGNOF },
    { "_Atomic", CKW_U_ATOMIC }, { "_Generic", CKW_U_GENERIC },
    { "_Noreturn", CKW_U_NORETURN }, { "_Static_assert", CKW_U_STATIC_ASSERT },
    { "_Thread_local", CKW_U_THREAD_LOCAL },
    /* C23 */
    { "alignas", CKW_ALIGNAS }, { "alignof", CKW_ALIGNOF }, { "bool", CKW_BOOL },
    { "constexpr", CKW_CONSTEXPR }, { "false", CKW_FALSE }, { "nullptr", CKW_NULLPTR },
    { "static_assert", CKW_STATIC_ASSERT }, { "thread_local", CKW_THREAD_LOCAL },
    { "true", CKW_TRUE }, { "typeof", CKW_TYPEOF }, { "typeof_unqual", CKW_TYPEOF_UNQUAL },
    { "_BitInt", CKW_U_BITINT }, { "_Decimal32", CKW_U_DECIMAL32 },
    { "_Decimal64", CKW_U_DECIMAL64 }, { "_Decimal128", CKW_U_DECIMAL128 },
    { NULL, CKW_NONE }
};

static CKeyword clexer_lookup_keyword(const char *start, size_t length)
{
    size_t i;
    for (i = 0; clexer_keywords[i].text != NULL; i++) {
        size_t klen = strlen(clexer_keywords[i].text);
        if (klen == length && memcmp(clexer_keywords[i].text, start, length) == 0) {
            return clexer_keywords[i].keyword;
        }
    }
    return CKW_NONE;
}

const char *clexer_keyword_name(CKeyword keyword)
{
    size_t i;
    for (i = 0; clexer_keywords[i].text != NULL; i++) {
        if (clexer_keywords[i].keyword == keyword) {
            return clexer_keywords[i].text;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* pp-number (C23 6.4.8) - a superset of integer/floating constants,    */
/* deliberately permissive; semantic validation is not this layer's job.*/
/* ------------------------------------------------------------------ */

static void clexer_scan_pp_number(CLexer *lexer)
{
    clexer_advance(lexer); /* the leading digit, or '.' followed by a digit */

    for (;;) {
        int c = clexer_current(lexer);
        size_t ucn;

        if (clexer_is_digit(c) || c == '.') {
            clexer_advance(lexer);
            continue;
        }

        if (c == 'e' || c == 'E' || c == 'p' || c == 'P') {
            int next = clexer_peek(lexer, 1);
            if (next == '+' || next == '-') {
                clexer_advance(lexer);
                clexer_advance(lexer);
                continue;
            }
            /* no sign: 'e'/'p' itself still qualifies below as identifier-nondigit */
        }

        if (c == '\'') {
            /* C23 digit separator: pp-number ' digit | pp-number ' nondigit */
            int next = clexer_peek(lexer, 1);
            if (clexer_is_digit(next) || clexer_is_ascii_ident_start(next)) {
                clexer_advance(lexer);
                clexer_advance(lexer);
                continue;
            }
            break;
        }

        if (clexer_is_ascii_ident_start(c) || clexer_is_extended_ident_byte(c)) {
            clexer_advance(lexer);
            continue;
        }

        ucn = clexer_ucn_len(lexer);
        if (ucn > 0) {
            size_t i;
            for (i = 0; i < ucn; i++) {
                clexer_advance(lexer);
            }
            continue;
        }

        break;
    }
}

/* ------------------------------------------------------------------ */
/* Character constants and string literals (with encoding prefixes).    */
/* ------------------------------------------------------------------ */

static void clexer_scan_quoted(CLexer *lexer, int quote)
{
    clexer_advance(lexer); /* opening quote */
    while (clexer_current(lexer) >= 0 && clexer_current(lexer) != quote && clexer_current(lexer) != '\n') {
        if (clexer_current(lexer) == '\\' && clexer_peek(lexer, 1) >= 0) {
            clexer_advance(lexer);
        }
        clexer_advance(lexer);
    }
    if (clexer_current(lexer) == quote) {
        clexer_advance(lexer); /* closing quote */
    }
}

/* Does "text" (of length "len") appear at the current position, directly
 * followed by a quote character? Used to recognize u8/u/U/L literal
 * prefixes without misclassifying identifiers like "U8Value". */
static int clexer_prefix_matches(const CLexer *lexer, const char *text, size_t len)
{
    size_t i;
    int after;

    for (i = 0; i < len; i++) {
        if (clexer_peek(lexer, i) != (unsigned char)text[i]) {
            return 0;
        }
    }
    after = clexer_peek(lexer, len);
    return after == '"' || after == '\'';
}

/* ------------------------------------------------------------------ */
/* Punctuators, including digraphs and the C23 "::" addition.           */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *spelling;
    CPunct      canonical;
    int         is_digraph;
} CPunctSpelling;

/* Ordered longest-spelling-first so the first match is the maximal munch
 * the standard requires (6.4p4). */
static const CPunctSpelling clexer_punct_table[] = {
    { "%:%:", CPUNCT_HASHHASH,   1 },
    { "...",  CPUNCT_ELLIPSIS,   0 },
    { "<<=",  CPUNCT_SHL_ASSIGN, 0 },
    { ">>=",  CPUNCT_SHR_ASSIGN, 0 },
    { "->",   CPUNCT_ARROW,      0 },
    { "++",   CPUNCT_PLUSPLUS,   0 },
    { "--",   CPUNCT_MINUSMINUS, 0 },
    { "<<",   CPUNCT_SHL,        0 },
    { ">>",   CPUNCT_SHR,        0 },
    { "<=",   CPUNCT_LE,         0 },
    { ">=",   CPUNCT_GE,         0 },
    { "==",   CPUNCT_EQ,         0 },
    { "!=",   CPUNCT_NE,         0 },
    { "&&",   CPUNCT_AMPAMP,     0 },
    { "||",   CPUNCT_PIPEPIPE,   0 },
    { "*=",   CPUNCT_STAR_ASSIGN,    0 },
    { "/=",   CPUNCT_SLASH_ASSIGN,   0 },
    { "%=",   CPUNCT_PERCENT_ASSIGN, 0 },
    { "+=",   CPUNCT_PLUS_ASSIGN,    0 },
    { "-=",   CPUNCT_MINUS_ASSIGN,   0 },
    { "&=",   CPUNCT_AMP_ASSIGN,     0 },
    { "^=",   CPUNCT_CARET_ASSIGN,   0 },
    { "|=",   CPUNCT_PIPE_ASSIGN,    0 },
    { "::",   CPUNCT_COLONCOLON, 0 },
    { "<:",   CPUNCT_LBRACKET,   1 },
    { ":>",   CPUNCT_RBRACKET,   1 },
    { "<%",   CPUNCT_LBRACE,     1 },
    { "%>",   CPUNCT_RBRACE,     1 },
    { "%:",   CPUNCT_HASH,       1 },
    { "##",   CPUNCT_HASHHASH,   0 },
    { "[",    CPUNCT_LBRACKET,   0 },
    { "]",    CPUNCT_RBRACKET,   0 },
    { "(",    CPUNCT_LPAREN,     0 },
    { ")",    CPUNCT_RPAREN,     0 },
    { "{",    CPUNCT_LBRACE,     0 },
    { "}",    CPUNCT_RBRACE,     0 },
    { ".",    CPUNCT_DOT,        0 },
    { "&",    CPUNCT_AMP,        0 },
    { "*",    CPUNCT_STAR,       0 },
    { "+",    CPUNCT_PLUS,       0 },
    { "-",    CPUNCT_MINUS,      0 },
    { "~",    CPUNCT_TILDE,      0 },
    { "!",    CPUNCT_BANG,       0 },
    { "/",    CPUNCT_SLASH,      0 },
    { "%",    CPUNCT_PERCENT,    0 },
    { "<",    CPUNCT_LT,         0 },
    { ">",    CPUNCT_GT,         0 },
    { "^",    CPUNCT_CARET,      0 },
    { "|",    CPUNCT_PIPE,       0 },
    { "?",    CPUNCT_QUESTION,   0 },
    { ":",    CPUNCT_COLON,      0 },
    { ";",    CPUNCT_SEMI,       0 },
    { "=",    CPUNCT_ASSIGN,     0 },
    { ",",    CPUNCT_COMMA,      0 },
    { "#",    CPUNCT_HASH,       0 },
    { NULL,   CPUNCT_NONE,       0 }
};

static const CPunctSpelling *clexer_match_punct(const CLexer *lexer)
{
    size_t i;
    for (i = 0; clexer_punct_table[i].spelling != NULL; i++) {
        size_t len = strlen(clexer_punct_table[i].spelling);
        size_t j;
        int matched = 1;

        for (j = 0; j < len; j++) {
            if (clexer_peek(lexer, j) != (unsigned char)clexer_punct_table[i].spelling[j]) {
                matched = 0;
                break;
            }
        }
        if (matched) {
            return &clexer_punct_table[i];
        }
    }
    return NULL;
}

const char *clexer_punct_name(CPunct punct)
{
    size_t i;
    for (i = 0; clexer_punct_table[i].spelling != NULL; i++) {
        if (clexer_punct_table[i].canonical == punct && !clexer_punct_table[i].is_digraph) {
            return clexer_punct_table[i].spelling;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Token dispatch.                                                      */
/* ------------------------------------------------------------------ */

static CToken clexer_make_common(const CLexer *lexer, int leading_space, int at_line_start)
{
    CToken token;

    token.start = lexer->src + lexer->pos;
    token.line = lexer->line;
    token.leading_space = leading_space;
    token.at_line_start = at_line_start;
    token.keyword = CKW_NONE;
    token.encoding = CENC_NONE;
    token.punct = CPUNCT_NONE;
    token.is_digraph = 0;
    token.type = CTOK_EOF;
    token.length = 0;
    return token;
}

static CToken clexer_lex_token(CLexer *lexer, int leading_space, int at_line_start)
{
    CToken token = clexer_make_common(lexer, leading_space, at_line_start);
    int c = clexer_current(lexer);

    if (c < 0) {
        return token;
    }

    {
        CEncodingPrefix enc = CENC_NONE;
        size_t plen = 0;

        if (clexer_prefix_matches(lexer, "u8", 2)) {
            enc = CENC_U8;
            plen = 2;
        } else if (clexer_prefix_matches(lexer, "u", 1)) {
            enc = CENC_u;
            plen = 1;
        } else if (clexer_prefix_matches(lexer, "U", 1)) {
            enc = CENC_U;
            plen = 1;
        } else if (clexer_prefix_matches(lexer, "L", 1)) {
            enc = CENC_L;
            plen = 1;
        }

        if (plen > 0) {
            size_t i;
            int quote;

            for (i = 0; i < plen; i++) {
                clexer_advance(lexer);
            }
            quote = clexer_current(lexer);
            clexer_scan_quoted(lexer, quote);
            token.type = (quote == '\'') ? CTOK_CHAR_CONST : CTOK_STRING_LITERAL;
            token.encoding = enc;
            token.length = (size_t)((lexer->src + lexer->pos) - token.start);
            return token;
        }
    }

    if (c == '"') {
        clexer_scan_quoted(lexer, '"');
        token.type = CTOK_STRING_LITERAL;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        return token;
    }

    if (c == '\'') {
        clexer_scan_quoted(lexer, '\'');
        token.type = CTOK_CHAR_CONST;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        return token;
    }

    if (clexer_is_ident_start_here(lexer)) {
        clexer_scan_identifier(lexer);
        token.type = CTOK_IDENTIFIER;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        token.keyword = clexer_lookup_keyword(token.start, token.length);
        return token;
    }

    if (clexer_is_digit(c) || (c == '.' && clexer_is_digit(clexer_peek(lexer, 1)))) {
        clexer_scan_pp_number(lexer);
        token.type = CTOK_PP_NUMBER;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        return token;
    }

    {
        const CPunctSpelling *match = clexer_match_punct(lexer);
        if (match != NULL) {
            size_t spelling_len = strlen(match->spelling);
            size_t i;

            for (i = 0; i < spelling_len; i++) {
                clexer_advance(lexer);
            }
            token.type = CTOK_PUNCT;
            token.punct = match->canonical;
            token.is_digraph = match->is_digraph;
            token.length = spelling_len;
            return token;
        }
    }

    clexer_advance(lexer);
    token.type = CTOK_OTHER;
    token.length = 1;
    return token;
}

CToken clexer_next_token(CLexer *lexer)
{
    int leading_space;
    int had_newline;
    int at_line_start;
    CToken token;

    leading_space = clexer_skip_trivia(lexer, &had_newline);
    lexer->pos = clexer_skip_splices(lexer, lexer->pos);
    at_line_start = had_newline || !lexer->started;

    token = clexer_lex_token(lexer, leading_space, at_line_start);
    lexer->started = 1;
    return token;
}

CToken clexer_next_header_name(CLexer *lexer)
{
    int leading_space;
    int had_newline;
    int at_line_start;
    int c;

    leading_space = clexer_skip_trivia(lexer, &had_newline);
    lexer->pos = clexer_skip_splices(lexer, lexer->pos);
    at_line_start = had_newline || !lexer->started;
    c = clexer_current(lexer);

    /* A header-name must be part of the same logical line as whatever
     * put the caller in this context (e.g. "# include"); if skipping
     * trivia already crossed into a new line, there is no header-name
     * here - fall through to ordinary tokenization instead. */
    if (!at_line_start && (c == '<' || c == '"')) {
        CToken token = clexer_make_common(lexer, leading_space, at_line_start);
        int closing = (c == '<') ? '>' : '"';

        clexer_advance(lexer);
        while (clexer_current(lexer) >= 0 && clexer_current(lexer) != closing && clexer_current(lexer) != '\n') {
            clexer_advance(lexer);
        }
        if (clexer_current(lexer) == closing) {
            clexer_advance(lexer);
        }
        token.type = CTOK_HEADER_NAME;
        token.length = (size_t)((lexer->src + lexer->pos) - token.start);
        lexer->started = 1;
        return token;
    }

    {
        CToken token = clexer_lex_token(lexer, leading_space, at_line_start);
        lexer->started = 1;
        return token;
    }
}

int clexer_token_equals(const CToken *token, const char *text)
{
    size_t text_len = strlen(text);
    if (token->length != text_len) {
        return 0;
    }
    return memcmp(token->start, text, text_len) == 0;
}

int clexer_token_starts_with(const CToken *token, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    if (token->length < prefix_len) {
        return 0;
    }
    return memcmp(token->start, prefix, prefix_len) == 0;
}

/*** End of inlined file: clexer.c ***/


/*** Start of inlined file: cpreprocessor.c ***/

/*** Start of inlined file: cpreprocessor.h ***/
#ifndef CPREPROCESSOR_H
#define CPREPROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CPP_LSTATE_NONE = 0,
    CPP_LSTATE_AFTER_HASH,          /* just saw '#' at the start of a line */
    CPP_LSTATE_AFTER_HAS_INCLUDE,   /* saw __has_include/__has_embed, awaiting '(' */
    CPP_LSTATE_EXPECT_HEADER_NAME   /* next token must be lexed as a header-name */
} CPreprocessorLineState;

typedef struct {
    CLexer                 lexer;
    CPreprocessorLineState line_state;
} CPreprocessor;

/* Prepares "pp" to scan "length" bytes starting at "source". */
void cpreprocessor_init(CPreprocessor *pp, const char *source, size_t length);

/* Like clexer_next_token(), but transparently requests a CTOK_HEADER_NAME
 * token from the underlying lexer at the points where the directive
 * grammar requires one (see the file header comment above). End of
 * input is signaled by the returned token's type being CTOK_EOF.
 */
CToken cpreprocessor_next_token(CPreprocessor *pp);

#ifdef __cplusplus
}
#endif

#endif /* CPREPROCESSOR_H */

/*** End of inlined file: cpreprocessor.h ***/

static int clexer_token_is_include_keyword(const CToken *token)
{
    return clexer_token_equals(token, "include") || clexer_token_equals(token, "embed");
}

static int clexer_token_is_has_include_keyword(const CToken *token)
{
    return clexer_token_equals(token, "__has_include") || clexer_token_equals(token, "__has_embed");
}

void cpreprocessor_init(CPreprocessor *pp, const char *source, size_t length)
{
    clexer_init(&pp->lexer, source, length);
    pp->line_state = CPP_LSTATE_NONE;
}

CToken cpreprocessor_next_token(CPreprocessor *pp)
{
    CToken token;

    /* clexer_next_header_name() may still fall back to an ordinary token:
     * either it wasn't actually followed by '<'/'"' (e.g. a macro-named
     * include, "#include FOO"), or trivia-skipping crossed into a new
     * line first (a malformed directive with nothing after it). Either
     * way, that fallback token must flow through the same classification
     * below as any other token - it is not necessarily consumed yet.
     */
    token = (pp->line_state == CPP_LSTATE_EXPECT_HEADER_NAME)
            ? clexer_next_header_name(&pp->lexer)
            : clexer_next_token(&pp->lexer);

    if (token.type == CTOK_HEADER_NAME) {
        pp->line_state = CPP_LSTATE_NONE;
        return token;
    }

    /* A new logical line always ends whatever directive we were tracking. */
    if (token.at_line_start) {
        pp->line_state = (token.type == CTOK_PUNCT && token.punct == CPUNCT_HASH)
                          ? CPP_LSTATE_AFTER_HASH
                          : CPP_LSTATE_NONE;
        return token;
    }

    switch (pp->line_state) {
    case CPP_LSTATE_AFTER_HASH:
        if (token.type == CTOK_IDENTIFIER && clexer_token_is_include_keyword(&token)) {
            pp->line_state = CPP_LSTATE_EXPECT_HEADER_NAME;
        } else if (token.type == CTOK_IDENTIFIER && clexer_token_is_has_include_keyword(&token)) {
            pp->line_state = CPP_LSTATE_AFTER_HAS_INCLUDE;
        } else {
            pp->line_state = CPP_LSTATE_NONE;
        }
        break;

    case CPP_LSTATE_AFTER_HAS_INCLUDE:
        if (token.type == CTOK_PUNCT && token.punct == CPUNCT_LPAREN) {
            pp->line_state = CPP_LSTATE_EXPECT_HEADER_NAME;
        } else {
            pp->line_state = CPP_LSTATE_NONE;
        }
        break;

    case CPP_LSTATE_NONE:
    case CPP_LSTATE_EXPECT_HEADER_NAME:
    default:
        /* __has_include / __has_embed may also appear later in a directive
         * line (typically deep in a #if expression), independent of what
         * immediately followed '#'. Also covers the fallback case above:
         * an expected header-name that didn't materialize just stops
         * being expected. */
        if (token.type == CTOK_IDENTIFIER && clexer_token_is_has_include_keyword(&token)) {
            pp->line_state = CPP_LSTATE_AFTER_HAS_INCLUDE;
        } else {
            pp->line_state = CPP_LSTATE_NONE;
        }
        break;
    }

    return token;
}

/*** End of inlined file: cpreprocessor.c ***/


/*** Start of inlined file: ctestscanner.c ***/

/*** Start of inlined file: ctestscanner.h ***/
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

/*** End of inlined file: ctestscanner.h ***/

#include <stdlib.h>
#include <string.h>

#define CTESTSCANNER_TEST_PREFIX "test_"

enum {
    ST_INIT = 0,
    ST_SAW_RETURN_VOID,
    ST_SAW_NAME,
    ST_SAW_LPAREN,
    ST_SAW_PARAM_VOID,
    ST_SAW_FIXTURE_TYPE,
    ST_SAW_FIXTURE_STAR,
    ST_SAW_FIXTURE_PARAM_NAME
};

typedef struct {
    CTestFunction *items;
    size_t         count;
    size_t         capacity;
} CTestFunctionList;

/* "fixture_type_token" is NULL for the plain "(void)" form, or the
 * captured type token for the one-parameter fixture form (see
 * ctestscanner.h). */
static int ctestscanner_list_push(CTestFunctionList *list, const CToken *name_token, int line,
                                   const CToken *fixture_type_token)
{
    char *name;
    char *fixture_type = NULL;

    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        CTestFunction *grown = (CTestFunction *)realloc(list->items, new_capacity * sizeof(CTestFunction));
        if (grown == NULL) {
            return 0;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }

    name = (char *)malloc(name_token->length + 1);
    if (name == NULL) {
        return 0;
    }
    memcpy(name, name_token->start, name_token->length);
    name[name_token->length] = '\0';

    if (fixture_type_token != NULL) {
        fixture_type = (char *)malloc(fixture_type_token->length + 1);
        if (fixture_type == NULL) {
            free(name);
            return 0;
        }
        memcpy(fixture_type, fixture_type_token->start, fixture_type_token->length);
        fixture_type[fixture_type_token->length] = '\0';
    }

    list->items[list->count].name = name;
    list->items[list->count].fixture_type = fixture_type;
    list->items[list->count].has_teardown = 0;
    list->items[list->count].line = line;
    list->count++;
    return 1;
}

/* Advances past an entire preprocessing directive line: "token" must
 * already be a '#' that is the first token on its line; returns the
 * next token that itself starts a line (or CTOK_EOF). */
static CToken ctestscanner_skip_directive_line(CPreprocessor *pp)
{
    CToken token;
    do {
        token = cpreprocessor_next_token(pp);
    } while (token.type != CTOK_EOF && !token.at_line_start);
    return token;
}

static int ctestscanner_is_directive_hash(const CToken *token)
{
    return token->at_line_start && token->type == CTOK_PUNCT && token->punct == CPUNCT_HASH;
}

static int ctestscanner_state_after_mismatch(const CToken *token)
{
    return (token->type == CTOK_IDENTIFIER && token->keyword == CKW_VOID) ? ST_SAW_RETURN_VOID : ST_INIT;
}

CTestFunction *ctestscanner_find(const char *source, size_t length, size_t *out_count)
{
    CPreprocessor pp;
    CToken token;
    CToken name_token;
    CToken fixture_type_token;
    int state = ST_INIT;
    int name_line = 0;
    CTestFunctionList list;

    list.items = NULL;
    list.count = 0;
    list.capacity = 0;

    cpreprocessor_init(&pp, source, length);
    token = cpreprocessor_next_token(&pp);

    while (token.type != CTOK_EOF) {
        int consumed_extra = 0;

        if (ctestscanner_is_directive_hash(&token)) {
            token = ctestscanner_skip_directive_line(&pp);
            continue;
        }

        switch (state) {
        case ST_INIT:
            state = (token.type == CTOK_IDENTIFIER && token.keyword == CKW_VOID) ? ST_SAW_RETURN_VOID : ST_INIT;
            break;

        case ST_SAW_RETURN_VOID:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_NONE &&
                clexer_token_starts_with(&token, CTESTSCANNER_TEST_PREFIX)) {
                name_token = token;
                name_line = token.line;
                state = ST_SAW_NAME;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_NAME:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_LPAREN) {
                state = ST_SAW_LPAREN;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_LPAREN:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_VOID) {
                state = ST_SAW_PARAM_VOID;
            } else if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_NONE) {
                /* Start of the one-parameter fixture form: "Type *param".
                 * "Type" must be a plain (non-keyword) identifier - a
                 * typedef'd fixture name, not e.g. a builtin "int". */
                fixture_type_token = token;
                state = ST_SAW_FIXTURE_TYPE;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_FIXTURE_TYPE:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_STAR) {
                state = ST_SAW_FIXTURE_STAR;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_FIXTURE_STAR:
            if (token.type == CTOK_IDENTIFIER && token.keyword == CKW_NONE) {
                state = ST_SAW_FIXTURE_PARAM_NAME;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        case ST_SAW_PARAM_VOID:
        case ST_SAW_FIXTURE_PARAM_NAME:
            if (token.type == CTOK_PUNCT && token.punct == CPUNCT_RPAREN) {
                CToken next = cpreprocessor_next_token(&pp);
                int is_fixture = (state == ST_SAW_FIXTURE_PARAM_NAME);

                while (next.type != CTOK_EOF && ctestscanner_is_directive_hash(&next)) {
                    next = ctestscanner_skip_directive_line(&pp);
                }

                if (next.type == CTOK_PUNCT && next.punct == CPUNCT_LBRACE) {
                    if (!ctestscanner_list_push(&list, &name_token, name_line,
                                                 is_fixture ? &fixture_type_token : NULL)) {
                        ctestscanner_free(list.items, list.count);
                        *out_count = 0;
                        return NULL;
                    }
                    state = ST_INIT;
                } else {
                    state = ctestscanner_state_after_mismatch(&next);
                }

                token = next;
                consumed_extra = 1;
            } else {
                state = ctestscanner_state_after_mismatch(&token);
            }
            break;

        default:
            state = ST_INIT;
            break;
        }

        if (!consumed_extra) {
            token = cpreprocessor_next_token(&pp);
        }
    }

    *out_count = list.count;
    return list.items;
}

void ctestscanner_free(CTestFunction *functions, size_t count)
{
    size_t i;
    if (functions == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(functions[i].name);
        free(functions[i].fixture_type);
    }
    free(functions);
}

/*** End of inlined file: ctestscanner.c ***/


/*** Start of inlined file: ctestfiles.c ***/

/*** Start of inlined file: ctestfiles.h ***/
#ifndef CTESTFILES_H
#define CTESTFILES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int        ok;      /* 0 = failed; "files" is safe to free either way */
    char      *error;   /* malloc'd, non-NULL only if !ok */
    CPathList  files;    /* absolute paths to test_*.c files directly in "dir", sorted */
} CTestFileScan;

/* Scans "dir" for immediate (non-recursive) entries named "test_*.c". */
CTestFileScan ctestfiles_scan(const char *dir);

/* Releases every owned field in "scan". Safe to call on a failed
 * (ok == 0) scan too. */
void ctestfiles_free(CTestFileScan *scan);

#ifdef __cplusplus
}
#endif

#endif /* CTESTFILES_H */

/*** End of inlined file: ctestfiles.h ***/

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#define CTESTFILES_ERROR_BUFSZ 256
#define CTESTFILES_PATTERN_SCRATCH 4096

static int ctestfiles_matches(const char *filename)
{
    static const char prefix[] = "test_";
    static const char suffix[] = ".c";
    size_t prefix_len = sizeof(prefix) - 1;
    size_t suffix_len = sizeof(suffix) - 1;
    size_t len = strlen(filename);

    if (len < prefix_len + suffix_len) {
        return 0;
    }
    if (memcmp(filename, prefix, prefix_len) != 0) {
        return 0;
    }
    return memcmp(filename + len - suffix_len, suffix, suffix_len) == 0;
}

static int ctestfiles_compare(const void *a, const void *b)
{
    char *const *sa = (char *const *)a;
    char *const *sb = (char *const *)b;
    return strcmp(*sa, *sb);
}

static CTestFileScan ctestfiles_fail(CTestFileScan *scan, const char *dir)
{
    char msg[CTESTFILES_ERROR_BUFSZ];

    cpathlist_free(&scan->files);
    cmsg_build(msg, sizeof(msg), "could not open test directory: ", dir, strlen(dir), "");

    scan->ok = 0;
    scan->error = cmsg_dup(msg, strlen(msg));
    return *scan;
}

#ifdef _WIN32

CTestFileScan ctestfiles_scan(const char *dir)
{
    CTestFileScan scan;
    char pattern[CTESTFILES_PATTERN_SCRATCH];
    WIN32_FIND_DATAA find_data;
    HANDLE handle;

    scan.ok = 1;
    scan.error = NULL;
    cpathlist_init(&scan.files);

    cpath_join(pattern, sizeof(pattern), dir, "*");

    handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return ctestfiles_fail(&scan, dir);
    }

    do {
        if (ctestfiles_matches(find_data.cFileName)) {
            if (cpathlist_register(&scan.files, dir, find_data.cFileName) == CPATHLIST_ALLOC_FAILED) {
                FindClose(handle);
                return ctestfiles_fail(&scan, dir);
            }
        }
    } while (FindNextFileA(handle, &find_data));
    FindClose(handle);

    qsort(scan.files.entries, scan.files.count, sizeof(char *), ctestfiles_compare);
    return scan;
}

#else

CTestFileScan ctestfiles_scan(const char *dir)
{
    CTestFileScan scan;
    DIR *handle;
    struct dirent *entry;

    scan.ok = 1;
    scan.error = NULL;
    cpathlist_init(&scan.files);

    handle = opendir(dir);
    if (handle == NULL) {
        return ctestfiles_fail(&scan, dir);
    }

    while ((entry = readdir(handle)) != NULL) {
        if (ctestfiles_matches(entry->d_name)) {
            if (cpathlist_register(&scan.files, dir, entry->d_name) == CPATHLIST_ALLOC_FAILED) {
                closedir(handle);
                return ctestfiles_fail(&scan, dir);
            }
        }
    }
    closedir(handle);

    qsort(scan.files.entries, scan.files.count, sizeof(char *), ctestfiles_compare);
    return scan;
}

#endif

void ctestfiles_free(CTestFileScan *scan)
{
    free(scan->error);
    cpathlist_free(&scan->files);
    scan->error = NULL;
}

/*** End of inlined file: ctestfiles.c ***/


/*** Start of inlined file: ctimer.c ***/

/*** Start of inlined file: ctimer.h ***/
#ifndef CTIMER_H
#define CTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the current wall-clock time in milliseconds, as a
 * monotonically non-decreasing value relative to other calls within
 * the same process run. Not tied to any particular epoch - only
 * differences between two calls (ctimer_now_ms() - earlier_value) are
 * meaningful. */
double ctimer_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* CTIMER_H */

/*** End of inlined file: ctimer.h ***/

#ifdef _WIN32
#include <windows.h>

double ctimer_now_ms(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
}
#else
#include <stddef.h>
#include <sys/time.h>

double ctimer_now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}
#endif

/*** End of inlined file: ctimer.c ***/


/*** Start of inlined file: cgtest_project.c ***/
#define JSMN_STATIC

/*** Start of inlined file: jsmn.h ***/
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
  JSMN_UNDEFINED = 0,
  JSMN_OBJECT = 1 << 0,
  JSMN_ARRAY = 1 << 1,
  JSMN_STRING = 1 << 2,
  JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

enum jsmnerr {
  /* Not enough tokens were provided */
  JSMN_ERROR_NOMEM = -1,
  /* Invalid character inside JSON string */
  JSMN_ERROR_INVAL = -2,
  /* The string is not a full JSON packet, more bytes expected */
  JSMN_ERROR_PART = -3
};

/**
 * JSON token description.
 * type		type (object, array, string etc.)
 * start	start position in JSON data string
 * end		end position in JSON data string
 */
typedef struct jsmntok {
  jsmntype_t type;
  int start;
  int end;
  int size;
#ifdef JSMN_PARENT_LINKS
  int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string.
 */
typedef struct jsmn_parser {
  unsigned int pos;     /* offset in the JSON string */
  unsigned int toknext; /* next token to allocate */
  int toksuper;         /* superior token node, e.g. parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
JSMN_API void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each
 * describing
 * a single JSON object.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER
/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                   const size_t num_tokens) {
  jsmntok_t *tok;
  if (parser->toknext >= num_tokens) {
    return NULL;
  }
  tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
#ifdef JSMN_PARENT_LINKS
  tok->parent = -1;
#endif
  return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
                            const int start, const int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                const size_t len, jsmntok_t *tokens,
                                const size_t num_tokens) {
  jsmntok_t *token;
  int start;

  start = parser->pos;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    switch (js[parser->pos]) {
#ifndef JSMN_STRICT
    /* In strict mode primitive must be followed by "," or "}" or "]" */
    case ':':
#endif
    case '\t':
    case '\r':
    case '\n':
    case ' ':
    case ',':
    case ']':
    case '}':
      goto found;
    default:
                   /* to quiet a warning from gcc*/
      break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
      parser->pos = start;
      return JSMN_ERROR_INVAL;
    }
  }
#ifdef JSMN_STRICT
  /* In strict mode primitive must be followed by a comma/object/array */
  parser->pos = start;
  return JSMN_ERROR_PART;
#endif

found:
  if (tokens == NULL) {
    parser->pos--;
    return 0;
  }
  token = jsmn_alloc_token(parser, tokens, num_tokens);
  if (token == NULL) {
    parser->pos = start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
  token->parent = parser->toksuper;
#endif
  parser->pos--;
  return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                             const size_t len, jsmntok_t *tokens,
                             const size_t num_tokens) {
  jsmntok_t *token;

  int start = parser->pos;

  /* Skip starting quote */
  parser->pos++;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c = js[parser->pos];

    /* Quote: end of string */
    if (c == '\"') {
      if (tokens == NULL) {
        return 0;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
      token->parent = parser->toksuper;
#endif
      return 0;
    }

    /* Backslash: Quoted symbol expected */
    if (c == '\\' && parser->pos + 1 < len) {
      int i;
      parser->pos++;
      switch (js[parser->pos]) {
      /* Allowed escaped symbols */
      case '\"':
      case '/':
      case '\\':
      case 'b':
      case 'f':
      case 'r':
      case 'n':
      case 't':
        break;
      /* Allows escaped symbol \uXXXX */
      case 'u':
        parser->pos++;
        for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0';
             i++) {
          /* If it isn't a hex character we have an error */
          if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) ||   /* 0-9 */
                (js[parser->pos] >= 65 && js[parser->pos] <= 70) ||   /* A-F */
                (js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
            parser->pos = start;
            return JSMN_ERROR_INVAL;
          }
          parser->pos++;
        }
        parser->pos--;
        break;
      /* Unexpected symbol */
      default:
        parser->pos = start;
        return JSMN_ERROR_INVAL;
      }
    }
  }
  parser->pos = start;
  return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens) {
  int r;
  int i;
  jsmntok_t *token;
  int count = parser->toknext;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c;
    jsmntype_t type;

    c = js[parser->pos];
    switch (c) {
    case '{':
    case '[':
      count++;
      if (tokens == NULL) {
        break;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        return JSMN_ERROR_NOMEM;
      }
      if (parser->toksuper != -1) {
        jsmntok_t *t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
        /* In strict mode an object or array can't become a key */
        if (t->type == JSMN_OBJECT) {
          return JSMN_ERROR_INVAL;
        }
#endif
        t->size++;
#ifdef JSMN_PARENT_LINKS
        token->parent = parser->toksuper;
#endif
      }
      token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
      token->start = parser->pos;
      parser->toksuper = parser->toknext - 1;
      break;
    case '}':
    case ']':
      if (tokens == NULL) {
        break;
      }
      type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
      if (parser->toknext < 1) {
        return JSMN_ERROR_INVAL;
      }
      token = &tokens[parser->toknext - 1];
      for (;;) {
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          token->end = parser->pos + 1;
          parser->toksuper = token->parent;
          break;
        }
        if (token->parent == -1) {
          if (token->type != type || parser->toksuper == -1) {
            return JSMN_ERROR_INVAL;
          }
          break;
        }
        token = &tokens[token->parent];
      }
#else
      for (i = parser->toknext - 1; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          parser->toksuper = -1;
          token->end = parser->pos + 1;
          break;
        }
      }
      /* Error if unmatched closing bracket */
      if (i == -1) {
        return JSMN_ERROR_INVAL;
      }
      for (; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          parser->toksuper = i;
          break;
        }
      }
#endif
      break;
    case '\"':
      r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;
    case '\t':
    case '\r':
    case '\n':
    case ' ':
      break;
    case ':':
      parser->toksuper = parser->toknext - 1;
      break;
    case ',':
      if (tokens != NULL && parser->toksuper != -1 &&
          tokens[parser->toksuper].type != JSMN_ARRAY &&
          tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
        parser->toksuper = tokens[parser->toksuper].parent;
#else
        for (i = parser->toknext - 1; i >= 0; i--) {
          if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
            if (tokens[i].start != -1 && tokens[i].end == -1) {
              parser->toksuper = i;
              break;
            }
          }
        }
#endif
      }
      break;
#ifdef JSMN_STRICT
    /* In strict mode primitives are: numbers and booleans */
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 't':
    case 'f':
    case 'n':
      /* And they must not be keys of the object */
      if (tokens != NULL && parser->toksuper != -1) {
        const jsmntok_t *t = &tokens[parser->toksuper];
        if (t->type == JSMN_OBJECT ||
            (t->type == JSMN_STRING && t->size != 0)) {
          return JSMN_ERROR_INVAL;
        }
      }
#else
    /* In non-strict mode every unquoted value is a primitive */
    default:
#endif
      r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;

#ifdef JSMN_STRICT
    /* Unexpected char in strict mode */
    default:
      return JSMN_ERROR_INVAL;
#endif
    }
  }

  if (tokens != NULL) {
    for (i = parser->toknext - 1; i >= 0; i--) {
      /* Unmatched opened object or array */
      if (tokens[i].start != -1 && tokens[i].end == -1) {
        return JSMN_ERROR_PART;
      }
    }
  }

  return count;
}

/**
 * Creates a new parser based over a given buffer with an array of tokens
 * available.
 */
JSMN_API void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

#endif /* JSMN_HEADER */

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */

/*** End of inlined file: jsmn.h ***/



/*** Start of inlined file: cgtest_project.h ***/
#ifndef CGTEST_PROJECT_H
#define CGTEST_PROJECT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int        ok;                /* 0 = failed; every field below except "error" is undefined if so */
    char      *error;             /* malloc'd human-readable message, non-NULL only if !ok */
    char      *compiler_command;  /* malloc'd, e.g. "gcc -std=c99 -O3" */
    CPathList  include_paths;     /* absolute dirs, one -I per entry */
    CPathList  source_files;      /* absolute .c files compiled alongside the runner */
    char      *output_path;       /* absolute dir; cgtest-runner.c/.exe get generated here */
    CPathList  test_directories;  /* absolute dirs scanned for test_*.c files */
    int        msvc;              /* optional, defaults to 0; see cgtest_runner.h */
    int        single_translation_unit; /* optional, defaults to 0; see cgtest_runner.h */
} CGTestProject;

/* Parses "json" (a "length"-byte buffer, not necessarily NUL-terminated)
 * as cgtest-project.json's content, resolving every relative path
 * against the already-absolute "base_dir". Pure - performs no
 * filesystem access itself, which is what makes it unit-testable with
 * in-memory buffers; see cgtest_project_load() for the disk-facing
 * entry point built on top of this one.
 */
CGTestProject cgtest_project_parse(const char *json, size_t length, const char *base_dir);

/* Reads "project_path" from disk, resolves it to an absolute path
 * (against the current working directory, if it's relative), and
 * parses it via cgtest_project_parse() with that file's own directory
 * as base_dir. "project_path" may name cgtest-project.json directly, or
 * a directory containing it (mirroring -i/--init's directory
 * argument) - if it resolves to a directory, "cgtest-project.json" is
 * looked up inside it.
 */
CGTestProject cgtest_project_load(const char *project_path);

/* Releases every owned field in "project". Safe to call on a failed
 * (ok == 0) project too. */
void cgtest_project_free(CGTestProject *project);

/* Scans "json" (a "length"-byte buffer, not necessarily NUL-terminated)
 * for which of cgtest-project.json's optional fields ("msvc",
 * "single_translation_unit") are present at its top level - without
 * resolving paths, validating value types, or building a CGTestProject
 * the way cgtest_project_parse() does. Used only by cgtest_create_run()
 * (cgtest_create.h) to detect which optional fields are missing from
 * an already-existing cgtest-project.json - e.g. one written by an
 * older cgtest.exe, before "single_translation_unit" existed - so it
 * can patch just those in with their default value rather than leaving
 * the file permanently missing a newer optional field.
 *
 * Deliberately conservative, not lenient: invalid JSON, a non-object
 * top level, an unrecognized key, a duplicate key, or a field value
 * that isn't a string/array/primitive (every shape any field defined
 * so far actually uses - same one-level-deep assumption as
 * cgtest_project_parse(), see this module's own header comment) all
 * return 0 with "out_has_msvc"/"out_has_single_translation_unit" left
 * untouched - if this can't fully make sense of the file's shape, the
 * safest thing its caller can do is leave the file alone entirely
 * rather than risk inserting a field into something it doesn't
 * actually understand.
 *
 * Returns 1 on success (both out-params set), 0 otherwise.
 */
int cgtest_project_scan_optional_fields(const char *json, size_t length,
                                         int *out_has_msvc, int *out_has_single_translation_unit);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_PROJECT_H */

/*** End of inlined file: cgtest_project.h ***/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define CGTEST_GETCWD _getcwd
#else
#include <unistd.h>
#define CGTEST_GETCWD getcwd
#endif

/* MSVC's <sys/stat.h> defines S_IFDIR/S_IFMT but not the S_ISDIR()
 * convenience macro POSIX builds already get for free. */
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

/* Generous relative to any real cgtest-project.json (see cpathlist.c's
 * CPATHLIST_SCRATCH_CAPACITY for the same reasoning): enough tokens for
 * thousands of listed paths, enough path-buffer bytes for any real
 * filesystem path. Exceeding either is reported as a load error, never
 * silently truncated/dropped. */
#define CGTEST_PROJECT_MAX_TOKENS   4096
#define CGTEST_PROJECT_PATH_SCRATCH 4096
#define CGTEST_PROJECT_ERROR_BUFSZ  256

enum {
    CGTEST_FIELD_COMPILER_COMMAND = 0,
    CGTEST_FIELD_INCLUDE_PATHS,
    CGTEST_FIELD_SOURCE_FILES,
    CGTEST_FIELD_OUTPUT_PATH,
    CGTEST_FIELD_TEST_DIRECTORIES,
    CGTEST_FIELD_MSVC,  /* optional - skipped by the required-fields check below */
    CGTEST_FIELD_SINGLE_TRANSLATION_UNIT,  /* optional - skipped by the required-fields check below */
    CGTEST_FIELD_COUNT
};

static const char *const CGTEST_PROJECT_FIELD_NAMES[CGTEST_FIELD_COUNT] = {
    "compiler_command",
    "include_paths",
    "source_files",
    "output_path",
    "test_directories",
    "msvc",
    "single_translation_unit"
};

/* Cleans up whatever "project" already holds (safe on partially-filled
 * or never-filled fields) and reports "message" as the failure. */
static CGTestProject cgtest_project_fail(CGTestProject *project, const char *message)
{
    cpathlist_free(&project->include_paths);
    cpathlist_free(&project->source_files);
    cpathlist_free(&project->test_directories);
    free(project->compiler_command);
    free(project->output_path);

    project->ok = 0;
    project->compiler_command = NULL;
    project->output_path = NULL;
    project->error = cmsg_dup(message, strlen(message));
    return *project;
}

/* Copies a JSMN_STRING token's text into a malloc'd, NUL-terminated,
 * unescaped C string. Supports the common JSON escapes (\" \\ \/ \b \f
 * \n \r \t). \uXXXX is deliberately unsupported - real file paths and
 * compiler commands have no need for it (literal UTF-8 bytes work
 * directly in JSON without escaping) - and is reported as an error
 * rather than silently mishandled. jsmn itself has already validated
 * that any backslash here is followed by one of exactly these 8 chars,
 * so that's the only case this function's own switch needs to reject.
 */
static char *jsmn_token_unescape(const char *json, const jsmntok_t *token,
                                  char *error_buf, size_t error_buf_size)
{
    size_t token_len = (size_t)(token->end - token->start);
    const char *src = json + token->start;
    char *result = (char *)malloc(token_len + 1);
    size_t out = 0;
    size_t i;

    if (result == NULL) {
        cmsg_set(error_buf, error_buf_size, "out of memory");
        return NULL;
    }

    for (i = 0; i < token_len; i++) {
        if (src[i] == '\\' && i + 1 < token_len) {
            i++;
            switch (src[i]) {
            case '\"': result[out++] = '\"'; break;
            case '\\': result[out++] = '\\'; break;
            case '/':  result[out++] = '/';  break;
            case 'b':  result[out++] = '\b'; break;
            case 'f':  result[out++] = '\f'; break;
            case 'n':  result[out++] = '\n'; break;
            case 'r':  result[out++] = '\r'; break;
            case 't':  result[out++] = '\t'; break;
            default:
                free(result);
                cmsg_set(error_buf, error_buf_size,
                    "cgtest-project.json: \\u escapes are not supported (use literal UTF-8 bytes instead)");
                return NULL;
            }
        } else {
            result[out++] = src[i];
        }
    }
    result[out] = '\0';
    return result;
}

static int cgtest_project_match_field(const char *json, const jsmntok_t *token)
{
    size_t token_len = (size_t)(token->end - token->start);
    int i;

    for (i = 0; i < CGTEST_FIELD_COUNT; i++) {
        size_t name_len = strlen(CGTEST_PROJECT_FIELD_NAMES[i]);
        if (token_len == name_len && memcmp(json + token->start, CGTEST_PROJECT_FIELD_NAMES[i], name_len) == 0) {
            return i;
        }
    }
    return -1;
}

static CPathList *cgtest_project_list_for_field(CGTestProject *project, int field)
{
    switch (field) {
    case CGTEST_FIELD_INCLUDE_PATHS:    return &project->include_paths;
    case CGTEST_FIELD_SOURCE_FILES:     return &project->source_files;
    case CGTEST_FIELD_TEST_DIRECTORIES: return &project->test_directories;
    default:                            return NULL;
    }
}

/* Consumes the value at tokens[value_idx] (already known to belong to
 * "field") into "project". Returns the token index just past everything
 * consumed on success, or -1 with "error_buf" filled in on failure. */
static int cgtest_project_apply_field(CGTestProject *project, int field, const char *json,
                                      const jsmntok_t *tokens, int token_count, int value_idx,
                                      const char *base_dir, char *error_buf, size_t error_buf_size)
{
    if (field == CGTEST_FIELD_COMPILER_COMMAND || field == CGTEST_FIELD_OUTPUT_PATH) {
        char *raw;

        if (tokens[value_idx].type != JSMN_STRING) {
            cmsg_build(error_buf, error_buf_size, "cgtest-project.json: field \"",
                CGTEST_PROJECT_FIELD_NAMES[field], strlen(CGTEST_PROJECT_FIELD_NAMES[field]), "\" must be a string");
            return -1;
        }

        raw = jsmn_token_unescape(json, &tokens[value_idx], error_buf, error_buf_size);
        if (raw == NULL) {
            return -1;
        }

        if (field == CGTEST_FIELD_COMPILER_COMMAND) {
            project->compiler_command = raw;
        } else {
            char scratch[CGTEST_PROJECT_PATH_SCRATCH];
            CPath joined = cpath_join(scratch, sizeof(scratch), base_dir, raw);
            project->output_path = cmsg_dup(joined.data, joined.length);
            free(raw);
            if (project->output_path == NULL) {
                cmsg_set(error_buf, error_buf_size, "out of memory");
                return -1;
            }
        }
        return value_idx + 1;
    }

    if (field == CGTEST_FIELD_MSVC || field == CGTEST_FIELD_SINGLE_TRANSLATION_UNIT) {
        const jsmntok_t *token = &tokens[value_idx];
        size_t token_len = (size_t)(token->end - token->start);
        int *target = field == CGTEST_FIELD_MSVC ? &project->msvc : &project->single_translation_unit;

        if (token->type == JSMN_PRIMITIVE && token_len == 4 && memcmp(json + token->start, "true", 4) == 0) {
            *target = 1;
        } else if (token->type == JSMN_PRIMITIVE && token_len == 5 && memcmp(json + token->start, "false", 5) == 0) {
            *target = 0;
        } else {
            cmsg_build(error_buf, error_buf_size, "cgtest-project.json: field \"",
                CGTEST_PROJECT_FIELD_NAMES[field], strlen(CGTEST_PROJECT_FIELD_NAMES[field]), "\" must be a boolean");
            return -1;
        }
        return value_idx + 1;
    }

    {
        CPathList *list = cgtest_project_list_for_field(project, field);
        int count;
        int i;
        int idx = value_idx;

        if (tokens[idx].type != JSMN_ARRAY) {
            cmsg_build(error_buf, error_buf_size, "cgtest-project.json: field \"",
                CGTEST_PROJECT_FIELD_NAMES[field], strlen(CGTEST_PROJECT_FIELD_NAMES[field]), "\" must be an array of strings");
            return -1;
        }
        count = tokens[idx].size;
        idx++;

        for (i = 0; i < count; i++) {
            char *element;
            CPathListStatus status;

            if (idx >= token_count || tokens[idx].type != JSMN_STRING) {
                cmsg_build(error_buf, error_buf_size, "cgtest-project.json: every element of \"",
                    CGTEST_PROJECT_FIELD_NAMES[field], strlen(CGTEST_PROJECT_FIELD_NAMES[field]), "\" must be a string");
                return -1;
            }

            element = jsmn_token_unescape(json, &tokens[idx], error_buf, error_buf_size);
            if (element == NULL) {
                return -1;
            }

            status = cpathlist_register(list, base_dir, element);
            free(element);
            if (status == CPATHLIST_ALLOC_FAILED) {
                cmsg_set(error_buf, error_buf_size, "out of memory");
                return -1;
            }
            idx++;
        }
        return idx;
    }
}

CGTestProject cgtest_project_parse(const char *json, size_t length, const char *base_dir)
{
    CGTestProject project;
    jsmn_parser parser;
    jsmntok_t tokens[CGTEST_PROJECT_MAX_TOKENS];
    int token_count;
    int seen[CGTEST_FIELD_COUNT];
    int idx;
    int pair;
    int i;
    char error_buf[CGTEST_PROJECT_ERROR_BUFSZ];

    memset(&project, 0, sizeof(project));
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);

    jsmn_init(&parser);
    token_count = jsmn_parse(&parser, json, length, tokens, CGTEST_PROJECT_MAX_TOKENS);

    if (token_count == JSMN_ERROR_NOMEM) {
        return cgtest_project_fail(&project, "cgtest-project.json has too many entries (exceeds the internal parser limit)");
    }
    if (token_count == JSMN_ERROR_INVAL) {
        return cgtest_project_fail(&project, "cgtest-project.json contains invalid JSON syntax");
    }
    if (token_count == JSMN_ERROR_PART) {
        return cgtest_project_fail(&project, "cgtest-project.json is incomplete (truncated JSON)");
    }
    if (token_count <= 0) {
        return cgtest_project_fail(&project, "cgtest-project.json is empty");
    }
    if (tokens[0].type != JSMN_OBJECT) {
        return cgtest_project_fail(&project, "cgtest-project.json must be a JSON object at the top level");
    }

    for (i = 0; i < CGTEST_FIELD_COUNT; i++) {
        seen[i] = 0;
    }

    idx = 1;
    for (pair = 0; pair < tokens[0].size; pair++) {
        int field;

        if (idx >= token_count || tokens[idx].type != JSMN_STRING) {
            return cgtest_project_fail(&project, "cgtest-project.json: expected a string key");
        }

        field = cgtest_project_match_field(json, &tokens[idx]);
        if (field < 0) {
            cmsg_build(error_buf, sizeof(error_buf), "cgtest-project.json: unknown key \"",
                json + tokens[idx].start, (size_t)(tokens[idx].end - tokens[idx].start), "\"");
            return cgtest_project_fail(&project, error_buf);
        }
        if (seen[field]) {
            cmsg_build(error_buf, sizeof(error_buf), "cgtest-project.json: duplicate key \"",
                CGTEST_PROJECT_FIELD_NAMES[field], strlen(CGTEST_PROJECT_FIELD_NAMES[field]), "\"");
            return cgtest_project_fail(&project, error_buf);
        }
        seen[field] = 1;
        idx++;

        if (idx >= token_count) {
            return cgtest_project_fail(&project, "cgtest-project.json: key is missing its value");
        }

        idx = cgtest_project_apply_field(&project, field, json, tokens, token_count, idx, base_dir,
                                         error_buf, sizeof(error_buf));
        if (idx < 0) {
            return cgtest_project_fail(&project, error_buf);
        }
    }

    for (i = 0; i < CGTEST_FIELD_COUNT; i++) {
        if (i == CGTEST_FIELD_MSVC || i == CGTEST_FIELD_SINGLE_TRANSLATION_UNIT) {
            continue;
        }
        if (!seen[i]) {
            cmsg_build(error_buf, sizeof(error_buf), "cgtest-project.json: missing required field \"",
                CGTEST_PROJECT_FIELD_NAMES[i], strlen(CGTEST_PROJECT_FIELD_NAMES[i]), "\"");
            return cgtest_project_fail(&project, error_buf);
        }
    }

    project.ok = 1;
    project.error = NULL;
    return project;
}

CGTestProject cgtest_project_load(const char *project_path)
{
    CGTestProject project;
    char cwd[CGTEST_PROJECT_PATH_SCRATCH];
    char abs_project_scratch[CGTEST_PROJECT_PATH_SCRATCH];
    char file_path_scratch[CGTEST_PROJECT_PATH_SCRATCH];
    char base_dir_scratch[CGTEST_PROJECT_PATH_SCRATCH];
    CPath abs_project;
    CPath file_path;
    CPath base_dir;
    struct stat st;
    FILE *f;
    long size;
    char *buffer;
    size_t read_count;

    memset(&project, 0, sizeof(project));
    cpathlist_init(&project.include_paths);
    cpathlist_init(&project.source_files);
    cpathlist_init(&project.test_directories);

    if (CGTEST_GETCWD(cwd, sizeof(cwd)) == NULL) {
        return cgtest_project_fail(&project, "could not determine the current working directory");
    }

    abs_project = cpath_join(abs_project_scratch, sizeof(abs_project_scratch), cwd, project_path);

    /* "project_path" may name cgtest-project.json directly, or the
     * directory it lives in (matching -i/--init's directory
     * argument) - if it's a directory, look for cgtest-project.json
     * inside it. */
    if (stat(abs_project.data, &st) == 0 && S_ISDIR(st.st_mode)) {
        file_path = cpath_join(file_path_scratch, sizeof(file_path_scratch), abs_project.data, "cgtest-project.json");
    } else {
        file_path = abs_project;
    }

    base_dir = cpath_dirname(base_dir_scratch, sizeof(base_dir_scratch), file_path.data);

    f = fopen(file_path.data, "rb");
    if (f == NULL) {
        char msg[CGTEST_PROJECT_ERROR_BUFSZ];
        cmsg_build(msg, sizeof(msg), "cgtest-project.json not found: ",
                                     file_path.data, file_path.length, "");
        return cgtest_project_fail(&project, msg);
    }

    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return cgtest_project_fail(&project, "could not determine cgtest-project.json's file size");
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(f);
        return cgtest_project_fail(&project, "out of memory reading cgtest-project.json");
    }

    read_count = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(buffer);
        return cgtest_project_fail(&project, "could not read cgtest-project.json");
    }
    buffer[size] = '\0';

    project = cgtest_project_parse(buffer, (size_t)size, base_dir.data);
    free(buffer);
    return project;
}

void cgtest_project_free(CGTestProject *project)
{
    free(project->error);
    free(project->compiler_command);
    free(project->output_path);
    cpathlist_free(&project->include_paths);
    cpathlist_free(&project->source_files);
    cpathlist_free(&project->test_directories);

    project->error = NULL;
    project->compiler_command = NULL;
    project->output_path = NULL;
}

/* Returns 1 if the first non-whitespace byte in "json" at or after
 * "pos" is "expected", 0 otherwise (including "pos" already at or past
 * "length"). Used only to verify a comma/closing-bracket actually
 * separates consecutive array elements or object pairs - jsmn's own
 * tokenizer is more lenient than strict JSON about this (it happily
 * tokenizes "[\"a\" \"b\"]" as two elements, comma or not), which is
 * fine for cgtest_project_parse()'s own everyday leniency but not
 * acceptable here: this function exists specifically to tell
 * cgtest_create_run() whether it's safe to splice new text into the
 * file, and a missing separator is exactly the kind of malformed input
 * that must not be papered over first (found via examples/mathlib's
 * own cgtest-project.json, which had exactly this typo). */
static int cgtest_project_next_nonspace_is(const char *json, size_t length, size_t pos, char expected)
{
    while (pos < length && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }
    return pos < length && json[pos] == expected;
}

/* jsmn's JSMN_STRING tokens end at their closing quote itself
 * (excluded from the token's own start/end, which cover only its
 * content) - every other token type this file uses (JSMN_PRIMITIVE,
 * JSMN_ARRAY) already ends past its full representation (a primitive
 * has no delimiter to exclude; an array's ".end" already covers its
 * closing "]"). Callers that want "the byte position right after this
 * token's complete text, quote included" - i.e. where a following
 * comma/bracket/brace would appear - need this rather than the raw
 * ".end" for a string. */
static size_t cgtest_project_token_end(const jsmntok_t *token)
{
    return (size_t)token->end + (token->type == JSMN_STRING ? 1 : 0);
}

int cgtest_project_scan_optional_fields(const char *json, size_t length,
                                         int *out_has_msvc, int *out_has_single_translation_unit)
{
    jsmn_parser parser;
    jsmntok_t tokens[CGTEST_PROJECT_MAX_TOKENS];
    int token_count;
    int seen[CGTEST_FIELD_COUNT];
    int idx;
    int pair;
    int i;

    jsmn_init(&parser);
    token_count = jsmn_parse(&parser, json, length, tokens, CGTEST_PROJECT_MAX_TOKENS);
    if (token_count <= 0 || tokens[0].type != JSMN_OBJECT) {
        return 0;
    }

    for (i = 0; i < CGTEST_FIELD_COUNT; i++) {
        seen[i] = 0;
    }

    idx = 1;
    for (pair = 0; pair < tokens[0].size; pair++) {
        int field;
        int value_idx;

        if (idx >= token_count || tokens[idx].type != JSMN_STRING) {
            return 0;
        }
        field = cgtest_project_match_field(json, &tokens[idx]);
        if (field < 0 || seen[field]) {
            return 0;
        }
        seen[field] = 1;
        idx++;

        if (idx >= token_count) {
            return 0;
        }
        value_idx = idx;

        /* Skip this field's value without inspecting it further - a
         * string/primitive is exactly one token; an array is its own
         * token plus one per element (see this function's own header
         * comment in cgtest_project.h for why nothing deeper is
         * needed). Anything else is a shape no field defined so far
         * uses - bail out rather than guess how to skip it. Each
         * array element's own separator is checked here too (see
         * cgtest_project_next_nonspace_is()'s header comment) - jsmn
         * itself doesn't require one between elements. */
        if (tokens[value_idx].type == JSMN_ARRAY) {
            int count = tokens[value_idx].size;
            idx = value_idx + 1;
            for (i = 0; i < count; i++) {
                if (idx >= token_count) {
                    return 0;
                }
                if (!cgtest_project_next_nonspace_is(json, length, cgtest_project_token_end(&tokens[idx]), i + 1 < count ? ',' : ']')) {
                    return 0;
                }
                idx++;
            }
        } else if (tokens[value_idx].type == JSMN_STRING || tokens[value_idx].type == JSMN_PRIMITIVE) {
            idx = value_idx + 1;
        } else {
            return 0;
        }

        /* Same check for the top-level object's own pairs - a comma
         * before the next key, or the closing "}" after the last
         * pair's value. "value_idx" (not the last-consumed token) is
         * always right here - jsmn sets a compound token's (array's)
         * own ".end" to cover its closing "]", the same as a scalar
         * token's ".end" already covers its own value. */
        if (!cgtest_project_next_nonspace_is(json, length, cgtest_project_token_end(&tokens[value_idx]),
                                              pair + 1 < tokens[0].size ? ',' : '}')) {
            return 0;
        }
    }

    *out_has_msvc = seen[CGTEST_FIELD_MSVC];
    *out_has_single_translation_unit = seen[CGTEST_FIELD_SINGLE_TRANSLATION_UNIT];
    return 1;
}

/*** End of inlined file: cgtest_project.c ***/


/*** Start of inlined file: cgtest_create.c ***/

/*** Start of inlined file: cgtest_create.h ***/
#ifndef CGTEST_CREATE_H
#define CGTEST_CREATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   ok;                 /* 0 = failed; see "error" */
    char *dir;                /* malloc'd absolute path to the "cgtest" directory, non-NULL only if ok */
    char *error;              /* malloc'd human-readable message, non-NULL only if !ok */

    /* Each is 1 if this call actually wrote that file, 0 if it already
     * existed and was left untouched - see cgtest_create_run()'s header
     * comment. Valid only if ok. */
    int   wrote_project;      /* cgtest-project.json */
    int   wrote_header;       /* cgtest.h */
    int   wrote_test_macros;  /* test_cgtest_macros.c */

    /* 1 if cgtest-project.json already existed (wrote_project == 0)
     * but was missing one or more optional fields (msvc,
     * single_translation_unit) that got patched in with their default
     * value - see cgtest_create_run()'s header comment. Always 0 when
     * wrote_project == 1 (nothing to patch into a file just written
     * fresh from the current template) or when nothing was missing.
     * Valid only if ok. */
    int   patched_project;

    /* 1 if cgtest-project.json already existed (wrote_project == 0)
     * but couldn't be checked for missing optional fields at all -
     * cgtest_project_scan_optional_fields() (cgtest_project.h)
     * couldn't make sense of its shape (invalid JSON, an unrecognized
     * key, etc.), so it was left completely untouched rather than
     * patched - never both this and patched_project at once. Distinct
     * from "nothing was missing" (patched_project == 0 with this also
     * 0) specifically so a caller isn't left thinking a broken file is
     * simply already up to date. Valid only if ok. */
    int   project_could_not_be_checked;
} CGTestCreateResult;

/* Ensures a template cgtest-project.json, cgtest.h, and
 * test_cgtest_macros.c exist inside "dir"'s "cgtest" child directory -
 * "dir" is always a directory, never a file path (e.g. "." creates
 * "./cgtest/cgtest-project.json", "./cgtest/cgtest.h", and
 * "./cgtest/test_cgtest_macros.c", never files directly in "."). This
 * nesting lets a project's own test files #include "cgtest/cgtest.h"
 * - the same gtest/gtest.h-style layout GoogleTest users already know
 * - instead of a bare cgtest.h competing with the project's own
 * headers at its root. Both "dir" and "dir/cgtest" are created if they
 * don't exist yet, along with any missing parent directories (like
 * "mkdir -p" - e.g. "cgtest --init foo/bar" works even if "foo"
 * doesn't exist yet either).
 *
 * Each of the three files is checked and written independently: a
 * missing one is created from the current template (the one baked
 * into this binary - see CGTestCreateResult::wrote_project/
 * wrote_header/wrote_test_macros to tell which), an already-existing
 * one is left completely untouched, never overwritten - whether it's
 * an unmodified older version or something the developer edited by
 * hand. This makes cgtest_create_run() safe (and idempotent) to call
 * again on an already-initialized "dir/cgtest": nothing errors just
 * because cgtest-project.json is already there, unlike before this
 * per-file check existed. In particular, deleting only cgtest.h (e.g.
 * to pick up a fix from a newer cgtest.exe - it never carries per-
 * project customization the way cgtest-project.json's compiler_command/
 * include_paths/etc. do) and re-running cgtest_create_run() regenerates
 * just that file, leaving cgtest-project.json and test_cgtest_macros.c
 * exactly as they were.
 *
 * An already-existing cgtest-project.json gets one more thing besides
 * "left completely untouched" or "written fresh": if a newer
 * cgtest.exe has grown an optional field (msvc, single_translation_unit)
 * that predates the file (e.g. --init was originally run with an older
 * cgtest.exe), that field is patched into the existing file with its
 * default value - see CGTestCreateResult::patched_project and
 * cgtest_project_scan_optional_fields() (cgtest_project.h) for the
 * detection and specification.md's "--init" section for the full
 * rationale. Every existing byte (values, formatting, field order) is
 * left exactly as it was; the missing field(s) are appended just
 * before the closing "}". Left alone entirely - not patched, not an
 * error - if the file doesn't parse as valid JSON, since inserting
 * into something whose shape isn't actually understood is worse than
 * doing nothing; CGTestCreateResult::project_could_not_be_checked
 * distinguishes that case from "nothing was missing" (both otherwise
 * report wrote_project == 0, patched_project == 0), so a caller isn't
 * left thinking a broken file is simply already up to date. This never
 * applies to the required fields
 * (compiler_command/include_paths/source_files/output_path/
 * test_directories, still true after "cgtest-project.json" above) -
 * none of them has a sensible project-agnostic default, unlike a
 * boolean flag defaulting to its old, pre-flag behavior.
 */
CGTestCreateResult cgtest_create_run(const char *dir);

/* Releases every owned field in "result". Safe to call on a failed
 * (ok == 0) result too. */
void cgtest_create_free(CGTestCreateResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_CREATE_H */

/*** End of inlined file: cgtest_create.h ***/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define CGTEST_GETCWD _getcwd
#define CGTEST_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define CGTEST_GETCWD getcwd
#define CGTEST_MKDIR(path) mkdir(path, 0755)
#endif

/* MSVC's <sys/stat.h> defines S_IFDIR/S_IFMT but not the S_ISDIR()
 * convenience macro POSIX builds already get for free. */
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#define CGTEST_CREATE_PATH_SCRATCH 4096
#define CGTEST_CREATE_ERROR_BUFSZ  256

/* Points at "." for test_directories rather than a fictional path -
 * test_cgtest_macros.c (see CGTEST_TEST_MACROS_TEMPLATE1 and friends
 * below) is written into the same directory as this project file, so
 * "cgtest --run ." finds and runs it immediately, no editing
 * required first. source_files/include_paths start empty since that
 * example needs neither - add your own project's files here.
 * output_path points at "../build" rather than "./build" so generated
 * build artifacts land as a sibling of the "cgtest" directory, not
 * inside it alongside the scaffold files themselves. */
static const char *const CGTEST_PROJECT_TEMPLATE =
    "{\n"
    "    \"compiler_command\": \"gcc -std=c89 -O0 -Wall -Wextra -pedantic-errors\",\n"
    "    \"msvc\": false,\n"
    "    \"single_translation_unit\": false,\n"
    "    \"include_paths\": [],\n"
    "    \"source_files\": [],\n"
    "    \"output_path\": \"../build\",\n"
    "    \"test_directories\": [\n"
    "        \".\"\n"
    "    ]\n"
    "}\n";

/* Split into several literals rather than one: ISO C90 compilers are
 * only required to support string literals up to 509 chars (after
 * adjacent concatenation), and the combined template exceeds that. */
static const char *const CGTEST_H_TEMPLATE_HEAD1 =
    "#ifndef CGTEST_H\n"
    "#define CGTEST_H\n"
    "\n"
    "/* Generated by \"cgtest --init\". Test functions have the form\n"
    " * \"void test_<name>(void) { ... }\" - a plain \"void\" keeps test\n"
    " * files C89-portable (no <stdbool.h>). EXPECT_* macros report a\n"
    " * failed check and keep running the test; ASSERT_* macros report\n"
    " * it and return immediately - use ASSERT for a precondition the\n"
    " * rest of the function depends on (e.g. a NULL check before a\n"
    " * dereference), EXPECT otherwise.\n";

static const char *const CGTEST_H_TEMPLATE_HEAD1B =
    " * The EXPECT_EQ_/EXPECT_NE_ macros (INT/UINT/FLOAT/DOUBLE/PTR/\n"
    " * STR/STR_NOCASE, and their ASSERT_ counterparts) additionally\n"
    " * print both operands' values on failure. FLOAT/DOUBLE compare\n"
    " * via a small relative tolerance rather than exact equality,\n"
    " * appropriate for results of floating-point arithmetic.\n";

static const char *const CGTEST_H_TEMPLATE_HEAD1C =
    " * EXPECT_NEAR_DOUBLE/ASSERT_NEAR_DOUBLE(expected, actual,\n"
    " * abs_error) instead take a caller-supplied tolerance, for when\n"
    " * the built-in one isn't the right fit. EXPECT_LT_/LE_/GT_/GE_\n"
    " * (INT/UINT/FLOAT/DOUBLE, and their ASSERT_ counterparts) are\n"
    " * ordering comparisons - </<=/>/>=. */\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_HEAD2 =
    "#include <float.h>\n"
    "#include <stdio.h>\n"
    "#include <string.h>\n"
    "\n"
    "/* The generated runner reads this after calling each test to\n"
    " * decide pass/fail, resetting it to 0 first. */\n"
    "extern int cgtest_failed;\n"
    "\n";

/* Set only by ASSERT_* (never EXPECT_*). For a fixture test (see
 * ch.6), the generated runner skips test_<name>(&state) when this is
 * set after setup_<name>(&state) - matching GoogleTest's SetUp()/
 * TestBody() behavior. */
static const char *const CGTEST_H_TEMPLATE_FATAL_FAILED =
    "/* Set only by ASSERT_* (never EXPECT_*). For a fixture test (see\n"
    " * ch.6), the generated runner skips test_<name>(&state) when this\n"
    " * is set after setup_<name>(&state) - matching GoogleTest's\n"
    " * SetUp()/TestBody() behavior. */\n"
    "extern int cgtest_fatal_failed;\n"
    "\n";

/* cgtest_relpath()/cgtest_print_str_field()/cgtest_strcasecmp() below
 * are declared here but defined in the generated cgtest-runner.c (see
 * cgtest_runner_generate_source()), the same "extern"-in-cgtest.h /
 * defined-once-in-the-generated-runner split cgtest_failed/
 * cgtest_fatal_failed above already use - not `static` bodies copied
 * into every #include'ing test_*.c file. That split matters beyond
 * just avoiding duplicated code: in separate-TU mode (cgtest_runner.h,
 * the default), each test_*.c file is its own translation unit, so a
 * `static` definition here would give every one of them its own
 * private, potentially-uncalled copy - one that -Wunused-function
 * flags in any file that doesn't happen to invoke the specific macro
 * family relying on it (e.g. a file with no EXPECT_EQ_STR_NOCASE call
 * still got its own unused cgtest_strcasecmp before this). A single
 * non-`static` definition has external linkage instead, satisfied by
 * the linker from every file that calls it - never "unused" from any
 * one translation unit's point of view. */
static const char *const CGTEST_H_TEMPLATE_RELPATH1 =
    "/* Shortens __FILE__ to a path relative to the current working\n"
    " * directory (falls back to the full path if it isn't under it),\n"
    " * so FAIL messages stay short - jump-to-file in an editor's\n"
    " * quickfix list still works as long as the editor's own cwd\n"
    " * matches wherever the test binary was run from. Implemented in\n"
    " * the generated cgtest-runner.c, not here - see cgtest_runner.h. */\n"
    "extern const char *cgtest_relpath(const char *file);\n"
    "\n";

/* Declared here, defined in the generated cgtest-runner.c - see the
 * comment above CGTEST_H_TEMPLATE_RELPATH1. Prints one
 * "  <prefix>\"...\"\n" line per '\n' found in "s" - used by
 * EXPECT_EQ_STR/ASSERT_EQ_STR so a real newline embedded in a tested
 * string can't visually merge with the FAIL block around it. Every
 * other non-printable byte - "\r", "\t", other named C escapes, and
 * anything outside 0x20-0x7e (including ESC, which could otherwise
 * inject an arbitrary terminal escape sequence) - is hex-escaped
 * inline instead of passed through raw; none of those are printed as
 * a real control byte, so unlike "\n" none of them need their own
 * line break either. Lines after the first are indented to line up
 * under the opening quote. */
static const char *const CGTEST_H_TEMPLATE_STRFIELD1 =
    "extern void cgtest_print_str_field(const char *prefix, const char *s);\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EXPECT_TRUE =
    "#define EXPECT_TRUE(cond) \\\n"
    "    do { \\\n"
    "        if (!(cond)) { \\\n"
    "            fprintf(stderr, \"%s:%d: FAIL: EXPECT_TRUE(%s)\\n\", cgtest_relpath(__FILE__), __LINE__, #cond); \\\n"
    "            cgtest_failed = 1; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EXPECT_FALSE =
    "#define EXPECT_FALSE(cond) \\\n"
    "    do { \\\n"
    "        if (cond) { \\\n"
    "            fprintf(stderr, \"%s:%d: FAIL: EXPECT_FALSE(%s)\\n\", cgtest_relpath(__FILE__), __LINE__, #cond); \\\n"
    "            cgtest_failed = 1; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_ASSERT_TRUE =
    "#define ASSERT_TRUE(cond) \\\n"
    "    do { \\\n"
    "        if (!(cond)) { \\\n"
    "            fprintf(stderr, \"%s:%d: FAIL: ASSERT_TRUE(%s)\\n\", cgtest_relpath(__FILE__), __LINE__, #cond); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            cgtest_fatal_failed = 1; \\\n"
    "            return; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_ASSERT_FALSE =
    "#define ASSERT_FALSE(cond) \\\n"
    "    do { \\\n"
    "        if (cond) { \\\n"
    "            fprintf(stderr, \"%s:%d: FAIL: ASSERT_FALSE(%s)\\n\", cgtest_relpath(__FILE__), __LINE__, #cond); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            cgtest_fatal_failed = 1; \\\n"
    "            return; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

/* The EXPECT_EQ_/EXPECT_NE_ and ASSERT_ variants below print both
 * operands' values on failure (EXPECT_TRUE/ASSERT_TRUE above only
 * ever show the source text of the whole condition). Each casts both
 * operands to one canonical type per family - long/unsigned long/
 * double/const void pointer - rather than having one macro per exact
 * C type, so e.g. char, short, int and long can all go through
 * EXPECT_EQ_INT. EQ and NE (and EXPECT and ASSERT) share one
 * CGTEST_CMP_*_ comparison core per family, parameterized on the
 * operator that decides failure, the printed label/format, and
 * whether to return afterwards - see EXPECT_EQ_INT/EXPECT_NE_INT
 * below for how it's used. */
static const char *const CGTEST_H_TEMPLATE_CMP_INT1 =
    "#define CGTEST_CMP_INT_(macro_name, fail_op, fmt, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        long cgtest_lhs_ = (long)(lhs); \\\n"
    "        long cgtest_rhs_ = (long)(rhs); \\\n"
    "        if (cgtest_lhs_ fail_op cgtest_rhs_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_INT2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            fprintf(stderr, fmt, cgtest_lhs_, cgtest_rhs_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_INT =
    "#define EXPECT_EQ_INT(expected, actual) \\\n"
    "    CGTEST_CMP_INT_(\"EXPECT_EQ_INT\", !=, \"  expected: %ld\\n  actual:   %ld\\n\", ((void)0), expected, actual)\n"
    "\n"
    "#define ASSERT_EQ_INT(expected, actual) \\\n"
    "    CGTEST_CMP_INT_(\"ASSERT_EQ_INT\", !=, \"  expected: %ld\\n  actual:   %ld\\n\", { cgtest_fatal_failed = 1; return; }, expected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_INT2 =
    "#define EXPECT_NE_INT(unexpected, actual) \\\n"
    "    CGTEST_CMP_INT_(\"EXPECT_NE_INT\", ==, \"  unexpected: %ld\\n  actual:     %ld\\n\", ((void)0), unexpected, actual)\n"
    "\n"
    "#define ASSERT_NE_INT(unexpected, actual) \\\n"
    "    CGTEST_CMP_INT_(\"ASSERT_NE_INT\", ==, \"  unexpected: %ld\\n  actual:     %ld\\n\", { cgtest_fatal_failed = 1; return; }, unexpected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_CMP_UINT1 =
    "#define CGTEST_CMP_UINT_(macro_name, fail_op, fmt, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        unsigned long cgtest_lhs_ = (unsigned long)(lhs); \\\n"
    "        unsigned long cgtest_rhs_ = (unsigned long)(rhs); \\\n"
    "        if (cgtest_lhs_ fail_op cgtest_rhs_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_UINT2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            fprintf(stderr, fmt, cgtest_lhs_, cgtest_rhs_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_UINT =
    "#define EXPECT_EQ_UINT(expected, actual) \\\n"
    "    CGTEST_CMP_UINT_(\"EXPECT_EQ_UINT\", !=, \"  expected: %lu\\n  actual:   %lu\\n\", ((void)0), expected, actual)\n"
    "\n"
    "#define ASSERT_EQ_UINT(expected, actual) \\\n"
    "    CGTEST_CMP_UINT_(\"ASSERT_EQ_UINT\", !=, \"  expected: %lu\\n  actual:   %lu\\n\", { cgtest_fatal_failed = 1; return; }, expected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_UINT2 =
    "#define EXPECT_NE_UINT(unexpected, actual) \\\n"
    "    CGTEST_CMP_UINT_(\"EXPECT_NE_UINT\", ==, \"  unexpected: %lu\\n  actual:     %lu\\n\", ((void)0), unexpected, actual)\n"
    "\n"
    "#define ASSERT_NE_UINT(unexpected, actual) \\\n"
    "    CGTEST_CMP_UINT_(\"ASSERT_NE_UINT\", ==, \"  unexpected: %lu\\n  actual:     %lu\\n\", { cgtest_fatal_failed = 1; return; }, unexpected, actual)\n"
    "\n";

/* Approximate equality via a small relative tolerance, replacing
 * exact bitwise equality (which would fail for nearly every real
 * floating-point computation, even ones that are correct for all
 * practical purposes) with something closer in spirit to GoogleTest's
 * ULP-based EXPECT_FLOAT_EQ/EXPECT_DOUBLE_EQ, but using an
 * epsilon-relative diff instead of literal bit-distance - no 64-bit
 * integer type needed for the double case that way, so this stays
 * portable to plain C89. diff = |expected - actual|, scale = max(1.0,
 * |expected|, |actual|) - the 1.0 floor keeps the tolerance from
 * collapsing to near-zero once both values are close to 0 - and
 * tolerance = 4 * EPSILON * scale. No <math.h>/fabs() dependency,
 * same reason cgtest_strcasecmp() avoids strcasecmp(). NE_ is the
 * logical negation (fails when the values ARE within tolerance) -
 * GoogleTest has no "confidently different floats" macro of its own,
 * but every other type family here has a matching NE_, so these do
 * too. CGTEST_APPROX_FLOAT_/CGTEST_APPROX_DOUBLE_ only differ in
 * which C type and <float.h> epsilon constant they use. */
static const char *const CGTEST_H_TEMPLATE_CMP_FLOAT1 =
    "#define CGTEST_APPROX_FLOAT_(macro_name, fail_op, fmt, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        float cgtest_lhs_ = (float)(lhs); \\\n"
    "        float cgtest_rhs_ = (float)(rhs); \\\n"
    "        float cgtest_diff_raw_ = cgtest_lhs_ - cgtest_rhs_; \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_FLOAT2 =
    "        float cgtest_diff_ = (cgtest_diff_raw_ < 0 ? -cgtest_diff_raw_ : cgtest_diff_raw_); \\\n"
    "        float cgtest_lhs_abs_ = (cgtest_lhs_ < 0 ? -cgtest_lhs_ : cgtest_lhs_); \\\n"
    "        float cgtest_rhs_abs_ = (cgtest_rhs_ < 0 ? -cgtest_rhs_ : cgtest_rhs_); \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_FLOAT3 =
    "        float cgtest_scale_ = (cgtest_lhs_abs_ > cgtest_rhs_abs_ ? cgtest_lhs_abs_ : cgtest_rhs_abs_); \\\n"
    "        float cgtest_tol_ = 4 * FLT_EPSILON * (cgtest_scale_ < 1.0f ? 1.0f : cgtest_scale_); \\\n"
    "        if (cgtest_diff_ fail_op cgtest_tol_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_FLOAT4 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            fprintf(stderr, fmt, cgtest_lhs_, cgtest_rhs_, cgtest_diff_, cgtest_tol_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_FLOAT =
    "#define EXPECT_EQ_FLOAT(expected, actual) \\\n"
    "    CGTEST_APPROX_FLOAT_(\"EXPECT_EQ_FLOAT\", >, \"  expected: %g\\n  actual:   %g\\n  diff:     %g (max allowed: %g)\\n\", ((void)0), expected, actual)\n"
    "\n"
    "#define ASSERT_EQ_FLOAT(expected, actual) \\\n"
    "    CGTEST_APPROX_FLOAT_(\"ASSERT_EQ_FLOAT\", >, \"  expected: %g\\n  actual:   %g\\n  diff:     %g (max allowed: %g)\\n\", { cgtest_fatal_failed = 1; return; }, expected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_FLOAT2 =
    "#define EXPECT_NE_FLOAT(unexpected, actual) \\\n"
    "    CGTEST_APPROX_FLOAT_(\"EXPECT_NE_FLOAT\", <=, \"  unexpected: %g\\n  actual:     %g\\n  diff:     %g (must exceed: %g)\\n\", ((void)0), unexpected, actual)\n"
    "\n"
    "#define ASSERT_NE_FLOAT(unexpected, actual) \\\n"
    "    CGTEST_APPROX_FLOAT_(\"ASSERT_NE_FLOAT\", <=, \"  unexpected: %g\\n  actual:     %g\\n  diff:     %g (must exceed: %g)\\n\", { cgtest_fatal_failed = 1; return; }, unexpected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_CMP_DOUBLE1 =
    "#define CGTEST_APPROX_DOUBLE_(macro_name, fail_op, fmt, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        double cgtest_lhs_ = (double)(lhs); \\\n"
    "        double cgtest_rhs_ = (double)(rhs); \\\n"
    "        double cgtest_diff_raw_ = cgtest_lhs_ - cgtest_rhs_; \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_DOUBLE2 =
    "        double cgtest_diff_ = (cgtest_diff_raw_ < 0 ? -cgtest_diff_raw_ : cgtest_diff_raw_); \\\n"
    "        double cgtest_lhs_abs_ = (cgtest_lhs_ < 0 ? -cgtest_lhs_ : cgtest_lhs_); \\\n"
    "        double cgtest_rhs_abs_ = (cgtest_rhs_ < 0 ? -cgtest_rhs_ : cgtest_rhs_); \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_DOUBLE3 =
    "        double cgtest_scale_ = (cgtest_lhs_abs_ > cgtest_rhs_abs_ ? cgtest_lhs_abs_ : cgtest_rhs_abs_); \\\n"
    "        double cgtest_tol_ = 4 * DBL_EPSILON * (cgtest_scale_ < 1.0 ? 1.0 : cgtest_scale_); \\\n"
    "        if (cgtest_diff_ fail_op cgtest_tol_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_DOUBLE4 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            fprintf(stderr, fmt, cgtest_lhs_, cgtest_rhs_, cgtest_diff_, cgtest_tol_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_DOUBLE =
    "#define EXPECT_EQ_DOUBLE(expected, actual) \\\n"
    "    CGTEST_APPROX_DOUBLE_(\"EXPECT_EQ_DOUBLE\", >, \"  expected: %g\\n  actual:   %g\\n  diff:     %g (max allowed: %g)\\n\", ((void)0), expected, actual)\n"
    "\n"
    "#define ASSERT_EQ_DOUBLE(expected, actual) \\\n"
    "    CGTEST_APPROX_DOUBLE_(\"ASSERT_EQ_DOUBLE\", >, \"  expected: %g\\n  actual:   %g\\n  diff:     %g (max allowed: %g)\\n\", { cgtest_fatal_failed = 1; return; }, expected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_DOUBLE2 =
    "#define EXPECT_NE_DOUBLE(unexpected, actual) \\\n"
    "    CGTEST_APPROX_DOUBLE_(\"EXPECT_NE_DOUBLE\", <=, \"  unexpected: %g\\n  actual:     %g\\n  diff:     %g (must exceed: %g)\\n\", ((void)0), unexpected, actual)\n"
    "\n"
    "#define ASSERT_NE_DOUBLE(unexpected, actual) \\\n"
    "    CGTEST_APPROX_DOUBLE_(\"ASSERT_NE_DOUBLE\", <=, \"  unexpected: %g\\n  actual:     %g\\n  diff:     %g (must exceed: %g)\\n\", { cgtest_fatal_failed = 1; return; }, unexpected, actual)\n"
    "\n";

/* Unlike EXPECT_EQ_DOUBLE's own built-in tolerance, this one takes a
 * caller-supplied abs_error instead - useful when the fixed 4*EPSILON
 * relative tolerance above isn't the right fit (e.g. a much looser or
 * tighter bound than "4 epsilons" is actually appropriate for the
 * computation in question). No <math.h>/fabs() dependency - the sign
 * flip below is all that's needed for an absolute value. */
static const char *const CGTEST_H_TEMPLATE_NEAR_DOUBLE1 =
    "#define CGTEST_NEAR_DOUBLE_(macro_name, on_fail, expected, actual, abs_error) \\\n"
    "    do { \\\n"
    "        double cgtest_exp_ = (double)(expected); \\\n"
    "        double cgtest_act_ = (double)(actual); \\\n"
    "        double cgtest_tol_ = (double)(abs_error); \\\n"
    "        double cgtest_diff_ = cgtest_exp_ - cgtest_act_; \\\n"
    "        if (cgtest_diff_ < 0) { \\\n"
    "            cgtest_diff_ = -cgtest_diff_; \\\n"
    "        } \\\n"
    "        if (cgtest_diff_ > cgtest_tol_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_NEAR_DOUBLE2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #expected, #actual, #abs_error); \\\n"
    "            fprintf(stderr, \"  expected: %g\\n  actual:   %g\\n  diff:     %g (max allowed: %g)\\n\", \\\n"
    "                    cgtest_exp_, cgtest_act_, cgtest_diff_, cgtest_tol_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_NEAR_DOUBLE3 =
    "#define EXPECT_NEAR_DOUBLE(expected, actual, abs_error) \\\n"
    "    CGTEST_NEAR_DOUBLE_(\"EXPECT_NEAR_DOUBLE\", ((void)0), expected, actual, abs_error)\n"
    "\n"
    "#define ASSERT_NEAR_DOUBLE(expected, actual, abs_error) \\\n"
    "    CGTEST_NEAR_DOUBLE_(\"ASSERT_NEAR_DOUBLE\", { cgtest_fatal_failed = 1; return; }, expected, actual, abs_error)\n"
    "\n";

/* LT_/LE_/GT_/GE_ (GoogleTest's EXPECT_LT/LE/GT/GE) - ordering
 * comparisons, one macro per relation per type family. Unlike EQ_/
 * NE_ on FLOAT/DOUBLE, these don't need an epsilon tolerance: </<=/
 * >/>= are exact, well-defined operations with no "rounding noise"
 * to absorb the way equality has. So INT_/UINT_ reuse the existing
 * CGTEST_CMP_INT_/CGTEST_CMP_UINT_ cores from the EQ_/NE_ macros
 * above, and FLOAT_/DOUBLE_ get their own plain-operator
 * CGTEST_CMP_FLOAT_/CGTEST_CMP_DOUBLE_ cores (distinct from
 * CGTEST_APPROX_FLOAT_/CGTEST_APPROX_DOUBLE_, which only EQ_/NE_
 * use). No PTR_ or STR_ family - ordering two arbitrary pointers
 * with </> is only well-defined in C if they point into the same
 * array, and GoogleTest itself has no EXPECT_STRLT either. "val1"/
 * "val2" instead of "expected"/"actual", since there's no "expected"
 * value for e.g. "must be less than". */
static const char *const CGTEST_H_TEMPLATE_CMP_FLOAT_ORD1 =
    "#define CGTEST_CMP_FLOAT_(macro_name, fail_op, fmt, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        float cgtest_lhs_ = (float)(lhs); \\\n"
    "        float cgtest_rhs_ = (float)(rhs); \\\n"
    "        if (cgtest_lhs_ fail_op cgtest_rhs_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_FLOAT_ORD2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            fprintf(stderr, fmt, cgtest_lhs_, cgtest_rhs_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_CMP_DOUBLE_ORD1 =
    "#define CGTEST_CMP_DOUBLE_(macro_name, fail_op, fmt, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        double cgtest_lhs_ = (double)(lhs); \\\n"
    "        double cgtest_rhs_ = (double)(rhs); \\\n"
    "        if (cgtest_lhs_ fail_op cgtest_rhs_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_DOUBLE_ORD2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            fprintf(stderr, fmt, cgtest_lhs_, cgtest_rhs_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LT_INT =
    "#define EXPECT_LT_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"EXPECT_LT_INT\", >=, \"  val1: %ld\\n  val2: %ld\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LT_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"ASSERT_LT_INT\", >=, \"  val1: %ld\\n  val2: %ld\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LE_INT =
    "#define EXPECT_LE_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"EXPECT_LE_INT\", >, \"  val1: %ld\\n  val2: %ld\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LE_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"ASSERT_LE_INT\", >, \"  val1: %ld\\n  val2: %ld\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GT_INT =
    "#define EXPECT_GT_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"EXPECT_GT_INT\", <=, \"  val1: %ld\\n  val2: %ld\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GT_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"ASSERT_GT_INT\", <=, \"  val1: %ld\\n  val2: %ld\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GE_INT =
    "#define EXPECT_GE_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"EXPECT_GE_INT\", <, \"  val1: %ld\\n  val2: %ld\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GE_INT(val1, val2) \\\n"
    "    CGTEST_CMP_INT_(\"ASSERT_GE_INT\", <, \"  val1: %ld\\n  val2: %ld\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LT_UINT =
    "#define EXPECT_LT_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"EXPECT_LT_UINT\", >=, \"  val1: %lu\\n  val2: %lu\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LT_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"ASSERT_LT_UINT\", >=, \"  val1: %lu\\n  val2: %lu\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LE_UINT =
    "#define EXPECT_LE_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"EXPECT_LE_UINT\", >, \"  val1: %lu\\n  val2: %lu\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LE_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"ASSERT_LE_UINT\", >, \"  val1: %lu\\n  val2: %lu\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GT_UINT =
    "#define EXPECT_GT_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"EXPECT_GT_UINT\", <=, \"  val1: %lu\\n  val2: %lu\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GT_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"ASSERT_GT_UINT\", <=, \"  val1: %lu\\n  val2: %lu\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GE_UINT =
    "#define EXPECT_GE_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"EXPECT_GE_UINT\", <, \"  val1: %lu\\n  val2: %lu\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GE_UINT(val1, val2) \\\n"
    "    CGTEST_CMP_UINT_(\"ASSERT_GE_UINT\", <, \"  val1: %lu\\n  val2: %lu\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LT_FLOAT =
    "#define EXPECT_LT_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"EXPECT_LT_FLOAT\", >=, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LT_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"ASSERT_LT_FLOAT\", >=, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LE_FLOAT =
    "#define EXPECT_LE_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"EXPECT_LE_FLOAT\", >, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LE_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"ASSERT_LE_FLOAT\", >, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GT_FLOAT =
    "#define EXPECT_GT_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"EXPECT_GT_FLOAT\", <=, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GT_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"ASSERT_GT_FLOAT\", <=, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GE_FLOAT =
    "#define EXPECT_GE_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"EXPECT_GE_FLOAT\", <, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GE_FLOAT(val1, val2) \\\n"
    "    CGTEST_CMP_FLOAT_(\"ASSERT_GE_FLOAT\", <, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LT_DOUBLE =
    "#define EXPECT_LT_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"EXPECT_LT_DOUBLE\", >=, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LT_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"ASSERT_LT_DOUBLE\", >=, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_LE_DOUBLE =
    "#define EXPECT_LE_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"EXPECT_LE_DOUBLE\", >, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_LE_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"ASSERT_LE_DOUBLE\", >, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GT_DOUBLE =
    "#define EXPECT_GT_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"EXPECT_GT_DOUBLE\", <=, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GT_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"ASSERT_GT_DOUBLE\", <=, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_GE_DOUBLE =
    "#define EXPECT_GE_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"EXPECT_GE_DOUBLE\", <, \"  val1: %g\\n  val2: %g\\n\", ((void)0), val1, val2)\n"
    "\n"
    "#define ASSERT_GE_DOUBLE(val1, val2) \\\n"
    "    CGTEST_CMP_DOUBLE_(\"ASSERT_GE_DOUBLE\", <, \"  val1: %g\\n  val2: %g\\n\", { cgtest_fatal_failed = 1; return; }, val1, val2)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_CMP_PTR1 =
    "#define CGTEST_CMP_PTR_(macro_name, fail_op, fmt, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        const void *cgtest_lhs_ = (const void *)(lhs); \\\n"
    "        const void *cgtest_rhs_ = (const void *)(rhs); \\\n"
    "        if (cgtest_lhs_ fail_op cgtest_rhs_) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_PTR2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            fprintf(stderr, fmt, cgtest_lhs_, cgtest_rhs_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_PTR =
    "#define EXPECT_EQ_PTR(expected, actual) \\\n"
    "    CGTEST_CMP_PTR_(\"EXPECT_EQ_PTR\", !=, \"  expected: %p\\n  actual:   %p\\n\", ((void)0), expected, actual)\n"
    "\n"
    "#define ASSERT_EQ_PTR(expected, actual) \\\n"
    "    CGTEST_CMP_PTR_(\"ASSERT_EQ_PTR\", !=, \"  expected: %p\\n  actual:   %p\\n\", { cgtest_fatal_failed = 1; return; }, expected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_PTR2 =
    "#define EXPECT_NE_PTR(unexpected, actual) \\\n"
    "    CGTEST_CMP_PTR_(\"EXPECT_NE_PTR\", ==, \"  unexpected: %p\\n  actual:     %p\\n\", ((void)0), unexpected, actual)\n"
    "\n"
    "#define ASSERT_NE_PTR(unexpected, actual) \\\n"
    "    CGTEST_CMP_PTR_(\"ASSERT_NE_PTR\", ==, \"  unexpected: %p\\n  actual:     %p\\n\", { cgtest_fatal_failed = 1; return; }, unexpected, actual)\n"
    "\n";

/* Content comparison via strcmp(), not pointer equality - two equal
 * C-strings stored at different addresses must still count as equal.
 * Same reason GoogleTest has EXPECT_STREQ separate from EXPECT_EQ.
 * "fail_op" compares strcmp()'s result against 0 (!= for EQ, == for
 * NE), same trick as the CGTEST_CMP_INT_/UINT_/DOUBLE_/PTR_ families
 * above. */
static const char *const CGTEST_H_TEMPLATE_CMP_STR1 =
    "#define CGTEST_CMP_STR_(macro_name, fail_op, lhs_label, rhs_label, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        const char *cgtest_lhs_ = (lhs); \\\n"
    "        const char *cgtest_rhs_ = (rhs); \\\n"
    "        if (strcmp(cgtest_lhs_, cgtest_rhs_) fail_op 0) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_STR2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            cgtest_print_str_field(lhs_label, cgtest_lhs_); \\\n"
    "            cgtest_print_str_field(rhs_label, cgtest_rhs_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_STR =
    "#define EXPECT_EQ_STR(expected, actual) \\\n"
    "    CGTEST_CMP_STR_(\"EXPECT_EQ_STR\", !=, \"  expected: \", \"  actual:   \", ((void)0), expected, actual)\n"
    "\n"
    "#define ASSERT_EQ_STR(expected, actual) \\\n"
    "    CGTEST_CMP_STR_(\"ASSERT_EQ_STR\", !=, \"  expected: \", \"  actual:   \", { cgtest_fatal_failed = 1; return; }, expected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_STR2 =
    "#define EXPECT_NE_STR(unexpected, actual) \\\n"
    "    CGTEST_CMP_STR_(\"EXPECT_NE_STR\", ==, \"  unexpected: \", \"  actual:     \", ((void)0), unexpected, actual)\n"
    "\n"
    "#define ASSERT_NE_STR(unexpected, actual) \\\n"
    "    CGTEST_CMP_STR_(\"ASSERT_NE_STR\", ==, \"  unexpected: \", \"  actual:     \", { cgtest_fatal_failed = 1; return; }, unexpected, actual)\n"
    "\n";

/* Declared here, defined in the generated cgtest-runner.c - see the
 * comment above CGTEST_H_TEMPLATE_RELPATH1. Byte-wise case-insensitive
 * comparison, like strcasecmp() - not used directly since strcasecmp()
 * isn't standard C (POSIX only, and named _stricmp on MSVC); this
 * stays portable to plain C89/C99. Returns 0 on a case-insensitive
 * match, same convention as strcmp(). */
static const char *const CGTEST_H_TEMPLATE_STRCASECMP1 =
    "extern int cgtest_strcasecmp(const char *a, const char *b);\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_CMP_STR_NOCASE1 =
    "#define CGTEST_CMP_STR_NOCASE_(macro_name, fail_op, lhs_label, rhs_label, on_fail, lhs, rhs) \\\n"
    "    do { \\\n"
    "        const char *cgtest_lhs_ = (lhs); \\\n"
    "        const char *cgtest_rhs_ = (rhs); \\\n"
    "        if (cgtest_strcasecmp(cgtest_lhs_, cgtest_rhs_) fail_op 0) { \\\n";

static const char *const CGTEST_H_TEMPLATE_CMP_STR_NOCASE2 =
    "            fprintf(stderr, \"%s:%d: FAIL: \" macro_name \"(%s, %s)\\n\", \\\n"
    "                    cgtest_relpath(__FILE__), __LINE__, #lhs, #rhs); \\\n"
    "            cgtest_print_str_field(lhs_label, cgtest_lhs_); \\\n"
    "            cgtest_print_str_field(rhs_label, cgtest_rhs_); \\\n"
    "            cgtest_failed = 1; \\\n"
    "            on_fail; \\\n"
    "        } \\\n"
    "    } while (0)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_STR_NOCASE =
    "#define EXPECT_EQ_STR_NOCASE(expected, actual) \\\n"
    "    CGTEST_CMP_STR_NOCASE_(\"EXPECT_EQ_STR_NOCASE\", !=, \"  expected: \", \"  actual:   \", ((void)0), expected, actual)\n"
    "\n"
    "#define ASSERT_EQ_STR_NOCASE(expected, actual) \\\n"
    "    CGTEST_CMP_STR_NOCASE_(\"ASSERT_EQ_STR_NOCASE\", !=, \"  expected: \", \"  actual:   \", { cgtest_fatal_failed = 1; return; }, expected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_EQ_NE_STR_NOCASE2 =
    "#define EXPECT_NE_STR_NOCASE(unexpected, actual) \\\n"
    "    CGTEST_CMP_STR_NOCASE_(\"EXPECT_NE_STR_NOCASE\", ==, \"  unexpected: \", \"  actual:     \", ((void)0), unexpected, actual)\n"
    "\n"
    "#define ASSERT_NE_STR_NOCASE(unexpected, actual) \\\n"
    "    CGTEST_CMP_STR_NOCASE_(\"ASSERT_NE_STR_NOCASE\", ==, \"  unexpected: \", \"  actual:     \", { cgtest_fatal_failed = 1; return; }, unexpected, actual)\n"
    "\n";

static const char *const CGTEST_H_TEMPLATE_FOOTER =
    "#endif /* CGTEST_H */\n";

/* Third template, written as "test_cgtest_macros.c" alongside the
 * other two - one example test function per macro from cgtest.h
 * above, so a freshly created project has something that actually
 * runs and passes immediately (cgtest-project.json's default
 * test_directories already includes "."), not just two files to
 * read. Kept in lockstep with examples/mathlib/tests/
 * test_cgtest_macros.c in the cgtest repo itself - same test
 * function bodies, only the header comment and #include line differ
 * (that copy lives alongside test_math.c rather than cgtest.h itself,
 * so it includes via "cgtest/cgtest.h" and doesn't reference the
 * mathlib example's other files in its header comment). */
static const char *const CGTEST_TEST_MACROS_TEMPLATE1 =
    "/* test_cgtest_macros.c - a fixture example, then one example per\n"
    " * macro from cgtest.h. These checks do not do anything useful; they\n"
    " * exist purely to show each macro's call shape (and, for the first\n"
    " * one, the fixture shape - see specification.md ch.6 \"Fixtures\").\n"
    " * Discovered and run automatically by \"cgtest --run .\" - \".\" is\n"
    " * already in cgtest-project.json's test_directories by default. */\n"
    "#include \"cgtest.h\"\n"
    "#include <stdlib.h>\n"
    "\n"
    "typedef struct Counter {\n"
    "    int value;\n"
    "} Counter;\n"
    "\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE2 =
    "void setup_counter(Counter **counter)\n"
    "{\n"
    "    *counter = calloc(1, sizeof(Counter));\n"
    "    (*counter)->value = 42;\n"
    "}\n"
    "\n"
    "void teardown_counter(Counter *counter)\n"
    "{\n"
    "    free(counter);\n"
    "}\n"
    "\n"
    "void test_counter(Counter *counter)\n"
    "{\n"
    "    EXPECT_EQ_INT(42, counter->value);\n"
    "}\n"
    "\n"
    "void test_expect_true(void)\n"
    "{\n"
    "    EXPECT_TRUE(1 == 1);\n"
    "}\n"
    "\n"
    "void test_expect_false(void)\n"
    "{\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE3 =
    "    EXPECT_FALSE(1 == 2);\n"
    "}\n"
    "\n"
    "void test_assert_true(void)\n"
    "{\n"
    "    ASSERT_TRUE(1 == 1);\n"
    "}\n"
    "\n"
    "void test_assert_false(void)\n"
    "{\n"
    "    ASSERT_FALSE(1 == 2);\n"
    "}\n"
    "\n"
    "void test_expect_eq_int(void)\n"
    "{\n"
    "    EXPECT_EQ_INT(42, 42);\n"
    "}\n"
    "\n"
    "void test_assert_eq_int(void)\n"
    "{\n"
    "    ASSERT_EQ_INT(42, 42);\n"
    "}\n"
    "\n"
    "void test_expect_ne_int(void)\n"
    "{\n"
    "    EXPECT_NE_INT(42, 43);\n"
    "}\n"
    "\n"
    "void test_assert_ne_int(void)\n"
    "{\n"
    "    ASSERT_NE_INT(42, 43);\n"
    "}\n"
    "\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE4 =
    "void test_expect_eq_uint(void)\n"
    "{\n"
    "    EXPECT_EQ_UINT(42u, 42u);\n"
    "}\n"
    "\n"
    "void test_assert_eq_uint(void)\n"
    "{\n"
    "    ASSERT_EQ_UINT(42u, 42u);\n"
    "}\n"
    "\n"
    "void test_expect_ne_uint(void)\n"
    "{\n"
    "    EXPECT_NE_UINT(42u, 43u);\n"
    "}\n"
    "\n"
    "void test_assert_ne_uint(void)\n"
    "{\n"
    "    ASSERT_NE_UINT(42u, 43u);\n"
    "}\n"
    "\n"
    "void test_expect_eq_float(void)\n"
    "{\n"
    "    /* 1.1f - 1.0f isn't bit-identical to 0.1f - this passes only\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE5 =
    "     * because of EXPECT_EQ_FLOAT's epsilon tolerance; exact equality\n"
    "     * would fail here. */\n"
    "    EXPECT_EQ_FLOAT(0.1f, 1.1f - 1.0f);\n"
    "}\n"
    "\n"
    "void test_assert_eq_float(void)\n"
    "{\n"
    "    ASSERT_EQ_FLOAT(0.1f, 1.1f - 1.0f);\n"
    "}\n"
    "\n"
    "void test_expect_ne_float(void)\n"
    "{\n"
    "    /* Unlike the rounding noise above, this is a real difference,\n"
    "     * far outside the epsilon tolerance. */\n"
    "    EXPECT_NE_FLOAT(4.2f, 4.3f);\n"
    "}\n"
    "\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE6 =
    "void test_assert_ne_float(void)\n"
    "{\n"
    "    ASSERT_NE_FLOAT(4.2f, 4.3f);\n"
    "}\n"
    "\n"
    "void test_expect_eq_double(void)\n"
    "{\n"
    "    /* Same idea as EQ_FLOAT above, in double precision. */\n"
    "    EXPECT_EQ_DOUBLE(0.1 + 0.2, 0.3);\n"
    "}\n"
    "\n"
    "void test_assert_eq_double(void)\n"
    "{\n"
    "    ASSERT_EQ_DOUBLE(0.1 + 0.2, 0.3);\n"
    "}\n"
    "\n"
    "void test_expect_ne_double(void)\n"
    "{\n"
    "    EXPECT_NE_DOUBLE(4.2, 4.3);\n"
    "}\n"
    "\n"
    "void test_assert_ne_double(void)\n"
    "{\n"
    "    ASSERT_NE_DOUBLE(4.2, 4.3);\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE7 =
    "}\n"
    "\n"
    "void test_expect_near_double(void)\n"
    "{\n"
    "    EXPECT_NEAR_DOUBLE(4.2, 4.2000001, 0.001);\n"
    "}\n"
    "\n"
    "void test_assert_near_double(void)\n"
    "{\n"
    "    ASSERT_NEAR_DOUBLE(4.2, 4.2000001, 0.001);\n"
    "}\n"
    "\n"
    "void test_expect_lt_int(void)\n"
    "{\n"
    "    EXPECT_LT_INT(1, 2);\n"
    "}\n"
    "\n"
    "void test_assert_lt_int(void)\n"
    "{\n"
    "    ASSERT_LT_INT(1, 2);\n"
    "}\n"
    "\n"
    "void test_expect_le_int(void)\n"
    "{\n"
    "    EXPECT_LE_INT(2, 2);\n"
    "}\n"
    "\n"
    "void test_assert_le_int(void)\n"
    "{\n"
    "    ASSERT_LE_INT(2, 2);\n"
    "}\n"
    "\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE8 =
    "void test_expect_gt_int(void)\n"
    "{\n"
    "    EXPECT_GT_INT(2, 1);\n"
    "}\n"
    "\n"
    "void test_assert_gt_int(void)\n"
    "{\n"
    "    ASSERT_GT_INT(2, 1);\n"
    "}\n"
    "\n"
    "void test_expect_ge_int(void)\n"
    "{\n"
    "    EXPECT_GE_INT(2, 2);\n"
    "}\n"
    "\n"
    "void test_assert_ge_int(void)\n"
    "{\n"
    "    ASSERT_GE_INT(2, 2);\n"
    "}\n"
    "\n"
    "void test_expect_lt_uint(void)\n"
    "{\n"
    "    EXPECT_LT_UINT(1u, 2u);\n"
    "}\n"
    "\n"
    "void test_assert_lt_uint(void)\n"
    "{\n"
    "    ASSERT_LT_UINT(1u, 2u);\n"
    "}\n"
    "\n"
    "void test_expect_le_uint(void)\n"
    "{\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE9 =
    "    EXPECT_LE_UINT(2u, 2u);\n"
    "}\n"
    "\n"
    "void test_assert_le_uint(void)\n"
    "{\n"
    "    ASSERT_LE_UINT(2u, 2u);\n"
    "}\n"
    "\n"
    "void test_expect_gt_uint(void)\n"
    "{\n"
    "    EXPECT_GT_UINT(2u, 1u);\n"
    "}\n"
    "\n"
    "void test_assert_gt_uint(void)\n"
    "{\n"
    "    ASSERT_GT_UINT(2u, 1u);\n"
    "}\n"
    "\n"
    "void test_expect_ge_uint(void)\n"
    "{\n"
    "    EXPECT_GE_UINT(2u, 2u);\n"
    "}\n"
    "\n"
    "void test_assert_ge_uint(void)\n"
    "{\n"
    "    ASSERT_GE_UINT(2u, 2u);\n"
    "}\n"
    "\n"
    "void test_expect_lt_float(void)\n"
    "{\n"
    "    EXPECT_LT_FLOAT(1.0f, 2.0f);\n"
    "}\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE10 =
    "\n"
    "void test_assert_lt_float(void)\n"
    "{\n"
    "    ASSERT_LT_FLOAT(1.0f, 2.0f);\n"
    "}\n"
    "\n"
    "void test_expect_le_float(void)\n"
    "{\n"
    "    EXPECT_LE_FLOAT(2.0f, 2.0f);\n"
    "}\n"
    "\n"
    "void test_assert_le_float(void)\n"
    "{\n"
    "    ASSERT_LE_FLOAT(2.0f, 2.0f);\n"
    "}\n"
    "\n"
    "void test_expect_gt_float(void)\n"
    "{\n"
    "    EXPECT_GT_FLOAT(2.0f, 1.0f);\n"
    "}\n"
    "\n"
    "void test_assert_gt_float(void)\n"
    "{\n"
    "    ASSERT_GT_FLOAT(2.0f, 1.0f);\n"
    "}\n"
    "\n"
    "void test_expect_ge_float(void)\n"
    "{\n"
    "    EXPECT_GE_FLOAT(2.0f, 2.0f);\n"
    "}\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE11 =
    "\n"
    "void test_assert_ge_float(void)\n"
    "{\n"
    "    ASSERT_GE_FLOAT(2.0f, 2.0f);\n"
    "}\n"
    "\n"
    "void test_expect_lt_double(void)\n"
    "{\n"
    "    EXPECT_LT_DOUBLE(1.0, 2.0);\n"
    "}\n"
    "\n"
    "void test_assert_lt_double(void)\n"
    "{\n"
    "    ASSERT_LT_DOUBLE(1.0, 2.0);\n"
    "}\n"
    "\n"
    "void test_expect_le_double(void)\n"
    "{\n"
    "    EXPECT_LE_DOUBLE(2.0, 2.0);\n"
    "}\n"
    "\n"
    "void test_assert_le_double(void)\n"
    "{\n"
    "    ASSERT_LE_DOUBLE(2.0, 2.0);\n"
    "}\n"
    "\n"
    "void test_expect_gt_double(void)\n"
    "{\n"
    "    EXPECT_GT_DOUBLE(2.0, 1.0);\n"
    "}\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE12 =
    "\n"
    "void test_assert_gt_double(void)\n"
    "{\n"
    "    ASSERT_GT_DOUBLE(2.0, 1.0);\n"
    "}\n"
    "\n"
    "void test_expect_ge_double(void)\n"
    "{\n"
    "    EXPECT_GE_DOUBLE(2.0, 2.0);\n"
    "}\n"
    "\n"
    "void test_assert_ge_double(void)\n"
    "{\n"
    "    ASSERT_GE_DOUBLE(2.0, 2.0);\n"
    "}\n"
    "\n"
    "void test_expect_eq_ptr(void)\n"
    "{\n"
    "    int x = 0;\n"
    "\n"
    "    EXPECT_EQ_PTR(&x, &x);\n"
    "}\n"
    "\n"
    "void test_assert_eq_ptr(void)\n"
    "{\n"
    "    int x = 0;\n"
    "\n"
    "    ASSERT_EQ_PTR(&x, &x);\n"
    "}\n"
    "\n"
    "void test_expect_ne_ptr(void)\n"
    "{\n"
    "    int x = 0;\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE13 =
    "    int y = 0;\n"
    "\n"
    "    EXPECT_NE_PTR(&x, &y);\n"
    "}\n"
    "\n"
    "void test_assert_ne_ptr(void)\n"
    "{\n"
    "    int x = 0;\n"
    "    int y = 0;\n"
    "\n"
    "    ASSERT_NE_PTR(&x, &y);\n"
    "}\n"
    "\n"
    "void test_expect_eq_str(void)\n"
    "{\n"
    "    EXPECT_EQ_STR(\"cgtest\", \"cgtest\");\n"
    "}\n"
    "\n"
    "void test_assert_eq_str(void)\n"
    "{\n"
    "    ASSERT_EQ_STR(\"cgtest\", \"cgtest\");\n"
    "}\n"
    "\n"
    "void test_expect_ne_str(void)\n"
    "{\n"
    "    EXPECT_NE_STR(\"cgtest\", \"gtest\");\n"
    "}\n"
    "\n"
    "void test_assert_ne_str(void)\n"
    "{\n";

static const char *const CGTEST_TEST_MACROS_TEMPLATE14 =
    "    ASSERT_NE_STR(\"cgtest\", \"gtest\");\n"
    "}\n"
    "\n"
    "void test_expect_eq_str_nocase(void)\n"
    "{\n"
    "    EXPECT_EQ_STR_NOCASE(\"CGTest\", \"cgtest\");\n"
    "}\n"
    "\n"
    "void test_assert_eq_str_nocase(void)\n"
    "{\n"
    "    ASSERT_EQ_STR_NOCASE(\"CGTest\", \"cgtest\");\n"
    "}\n"
    "\n"
    "void test_expect_ne_str_nocase(void)\n"
    "{\n"
    "    EXPECT_NE_STR_NOCASE(\"CGTest\", \"gtest\");\n"
    "}\n"
    "\n"
    "void test_assert_ne_str_nocase(void)\n"
    "{\n"
    "    ASSERT_NE_STR_NOCASE(\"CGTest\", \"gtest\");\n"
    "}\n";

static CGTestCreateResult cgtest_create_fail(const char *message)
{
    CGTestCreateResult result;
    result.ok = 0;
    result.dir = NULL;
    result.error = cmsg_dup(message, strlen(message));
    result.wrote_project = 0;
    result.wrote_header = 0;
    result.wrote_test_macros = 0;
    result.patched_project = 0;
    result.project_could_not_be_checked = 0;
    return result;
}

/* The variadic "..." parts (each a "const char *", terminated by a
 * NULL sentinel) are written back to back into one file - lets a
 * template be split across as many string literals as needed (see
 * CGTEST_H_TEMPLATE_HEAD1 and friends) without ever concatenating
 * them in memory. */
static int cgtest_create_write_file(const char *path, char *error_buf, size_t error_buf_size, ...)
{
    FILE *f;
    va_list args;
    const char *part;
    size_t len;

    f = fopen(path, "wb");
    if (f == NULL) {
        cmsg_build(error_buf, error_buf_size, "could not create ", path, strlen(path), "");
        return 0;
    }

    va_start(args, error_buf_size);
    for (part = va_arg(args, const char *); part != NULL; part = va_arg(args, const char *)) {
        len = strlen(part);
        if (fwrite(part, 1, len, f) != len) {
            va_end(args);
            fclose(f);
            cmsg_build(error_buf, error_buf_size, "could not write ", path, strlen(path), "");
            return 0;
        }
    }
    va_end(args);

    fclose(f);
    return 1;
}

/* Reads "path" whole into a malloc'd, NUL-terminated buffer, setting
 * *out_length to the byte count read (excluding the NUL). Returns NULL
 * on any I/O or allocation failure. Same shape as
 * cgtest_runner_read_file() (cgtest_runner.c) - each module that needs
 * this keeps its own small copy rather than sharing one, matching this
 * codebase's existing convention (e.g. CGTEST_GETCWD is likewise
 * redefined per file, not shared via a common header). */
static char *cgtest_create_read_file(const char *path, size_t *out_length)
{
    FILE *f;
    long size;
    char *buffer;
    size_t read_count;

    f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(f);
        return NULL;
    }

    read_count = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(buffer);
        return NULL;
    }
    buffer[size] = '\0';

    *out_length = (size_t)size;
    return buffer;
}

/* The exact text appended for each optional field missing from an
 * already-existing cgtest-project.json (see
 * cgtest_create_patch_missing_optional_fields() below) - same
 * indentation/formatting as CGTEST_PROJECT_TEMPLATE's own "msvc"/
 * "single_translation_unit" lines, minus the trailing comma (added
 * separately, since whether one is needed depends on how many fields
 * are being patched in together). */
static const char *const CGTEST_CREATE_MSVC_DEFAULT_LINE = "    \"msvc\": false";
static const char *const CGTEST_CREATE_SINGLE_TU_DEFAULT_LINE = "    \"single_translation_unit\": false";

/* If "path" (an already-existing cgtest-project.json - see
 * cgtest_create_run()'s header comment) is missing "msvc" and/or
 * "single_translation_unit", appends whichever is missing - with its
 * default value - just before the file's closing "}", leaving every
 * other byte (existing values, formatting, field order) untouched.
 * *out_patched is set to 1 if anything was actually appended.
 * *out_could_not_check is set to 1 instead if the file couldn't be
 * understood at all (see cgtest_project_scan_optional_fields()'s
 * header comment for why that's left alone rather than guessed at) -
 * distinct from "nothing was missing" (both out-params 0) so a caller
 * isn't left thinking a broken file is simply already up to date.
 * Returns 1 on success (regardless of which out-param ends up set), 0
 * with "error_buf" filled in on an I/O failure. */
static int cgtest_create_patch_missing_optional_fields(const char *path, char *error_buf, size_t error_buf_size,
                                                         int *out_patched, int *out_could_not_check)
{
    char *content;
    size_t length;
    int has_msvc;
    int has_single_translation_unit;
    const char *missing[2];
    size_t missing_count;
    size_t content_end;
    char *patched;
    size_t offset;
    size_t i;
    size_t part_len;
    FILE *f;

    *out_patched = 0;
    *out_could_not_check = 0;

    content = cgtest_create_read_file(path, &length);
    if (content == NULL) {
        cmsg_build(error_buf, error_buf_size, "could not read ", path, strlen(path), "");
        return 0;
    }

    if (!cgtest_project_scan_optional_fields(content, length, &has_msvc, &has_single_translation_unit)) {
        free(content);
        *out_could_not_check = 1;
        return 1;
    }

    missing_count = 0;
    if (!has_msvc) {
        missing[missing_count++] = CGTEST_CREATE_MSVC_DEFAULT_LINE;
    }
    if (!has_single_translation_unit) {
        missing[missing_count++] = CGTEST_CREATE_SINGLE_TU_DEFAULT_LINE;
    }

    if (missing_count == 0) {
        free(content);
        return 1;
    }

    /* cgtest-project.json's fields never nest a "{" (every value is a
     * string, an array, or a boolean - see cgtest_project.c's own
     * header comment), so the whole file is exactly one JSON object
     * and its closing "}" is simply the last non-whitespace byte -
     * find it by trimming trailing whitespace rather than re-parsing
     * again. */
    content_end = length;
    while (content_end > 0 &&
           (content[content_end - 1] == ' ' || content[content_end - 1] == '\t' ||
            content[content_end - 1] == '\r' || content[content_end - 1] == '\n')) {
        content_end--;
    }
    if (content_end == 0 || content[content_end - 1] != '}') {
        /* Unreachable in practice - cgtest_project_scan_optional_fields()
         * above already confirmed a top-level object - but never patch
         * a file whose shape isn't exactly what was just assumed. */
        free(content);
        *out_could_not_check = 1;
        return 1;
    }
    content_end--; /* now at the '}' itself */

    /* Trim the whitespace before the "}" too, back to the real end of
     * the previous field's value (e.g. the "]" closing
     * test_directories) - that's where the new ",\n" belongs. */
    while (content_end > 0 &&
           (content[content_end - 1] == ' ' || content[content_end - 1] == '\t' ||
            content[content_end - 1] == '\r' || content[content_end - 1] == '\n')) {
        content_end--;
    }

    /* content_end bytes of the original file, plus ",\n" and the line
     * itself per missing field, plus a closing "\n}\n" - comfortably
     * bounded by content_end + 128 regardless of missing_count (at
     * most 2 today). */
    patched = (char *)malloc(content_end + 128);
    if (patched == NULL) {
        free(content);
        cmsg_set(error_buf, error_buf_size, "out of memory");
        return 0;
    }

    memcpy(patched, content, content_end);
    offset = content_end;
    for (i = 0; i < missing_count; i++) {
        patched[offset++] = ',';
        patched[offset++] = '\n';
        part_len = strlen(missing[i]);
        memcpy(patched + offset, missing[i], part_len);
        offset += part_len;
    }
    patched[offset++] = '\n';
    patched[offset++] = '}';
    patched[offset++] = '\n';

    free(content);

    f = fopen(path, "wb");
    if (f == NULL) {
        free(patched);
        cmsg_build(error_buf, error_buf_size, "could not create ", path, strlen(path), "");
        return 0;
    }
    if (fwrite(patched, 1, offset, f) != offset) {
        fclose(f);
        free(patched);
        cmsg_build(error_buf, error_buf_size, "could not write ", path, strlen(path), "");
        return 0;
    }
    fclose(f);
    free(patched);

    *out_patched = 1;
    return 1;
}

/* Creates "path" and every missing parent directory along the way,
 * like "mkdir -p" - "path" must already be an absolute path using
 * only '/' separators, which is what cpath_join() always produces
 * regardless of platform (see cpath.h). Mutates "path" temporarily
 * (never leaves it modified once this returns) to test/create each
 * prefix in turn, always including the trailing '/' in the prefix
 * tested/created - "C:" alone means "current directory on C:" on
 * Windows, not the drive root, so a Windows drive root must be
 * tested/created as "C:/", never as "C:". Returns 0 on success,
 * matching CGTEST_MKDIR()'s own convention. */
static int cgtest_create_mkdir_p(char *path)
{
    char *p;
    struct stat st;

    for (p = path + 1; *p != '\0'; p++) {
        if (*p == '/') {
            char saved = *(p + 1);
            *(p + 1) = '\0';
            if (stat(path, &st) == 0) {
                if (!S_ISDIR(st.st_mode)) {
                    *(p + 1) = saved;
                    return -1;
                }
            } else if (CGTEST_MKDIR(path) != 0) {
                *(p + 1) = saved;
                return -1;
            }
            *(p + 1) = saved;
        }
    }

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return CGTEST_MKDIR(path);
}

/* Makes sure "dir" exists as a directory - creating it (and any
 * missing parents) if it doesn't, or reporting a clear "not a
 * directory" error if a file already occupies that path. Used once
 * for the caller-supplied directory and once for its "cgtest" child,
 * since both need the same create-if-missing/reject-if-a-file logic. */
static int cgtest_create_ensure_dir(CPath dir, char *error_buf, size_t error_buf_size)
{
    struct stat st;

    if (stat(dir.data, &st) != 0) {
        if (cgtest_create_mkdir_p(dir.data) != 0) {
            cmsg_build(error_buf, error_buf_size, "could not create directory ", dir.data, dir.length, "");
            return 0;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        cmsg_build(error_buf, error_buf_size, "not a directory: ", dir.data, dir.length, "");
        return 0;
    }
    return 1;
}

CGTestCreateResult cgtest_create_run(const char *dir)
{
    CGTestCreateResult result;
    char cwd[CGTEST_CREATE_PATH_SCRATCH];
    char abs_dir_scratch[CGTEST_CREATE_PATH_SCRATCH];
    char cgtest_dir_scratch[CGTEST_CREATE_PATH_SCRATCH];
    char project_scratch[CGTEST_CREATE_PATH_SCRATCH];
    char header_scratch[CGTEST_CREATE_PATH_SCRATCH];
    char test_scratch[CGTEST_CREATE_PATH_SCRATCH];
    CPath abs_dir;
    CPath cgtest_dir;
    CPath project_path;
    CPath header_path;
    CPath test_path;
    struct stat st;
    char error_buf[CGTEST_CREATE_ERROR_BUFSZ];

    if (CGTEST_GETCWD(cwd, sizeof(cwd)) == NULL) {
        return cgtest_create_fail("could not determine the current working directory");
    }

    abs_dir = cpath_join(abs_dir_scratch, sizeof(abs_dir_scratch), cwd, dir);

    if (!cgtest_create_ensure_dir(abs_dir, error_buf, sizeof(error_buf))) {
        return cgtest_create_fail(error_buf);
    }

    /* The three files live in a "cgtest" child of "dir" rather than
     * "dir" itself, so a project's tests can #include "cgtest/cgtest.h"
     * - the same gtest/gtest.h-style layout GoogleTest users already
     * know - instead of a bare cgtest.h competing with the project's
     * own headers at its root. */
    cgtest_dir = cpath_join(cgtest_dir_scratch, sizeof(cgtest_dir_scratch), abs_dir.data, "cgtest");

    if (!cgtest_create_ensure_dir(cgtest_dir, error_buf, sizeof(error_buf))) {
        return cgtest_create_fail(error_buf);
    }

    project_path = cpath_join(project_scratch, sizeof(project_scratch), cgtest_dir.data, "cgtest-project.json");
    header_path = cpath_join(header_scratch, sizeof(header_scratch), cgtest_dir.data, "cgtest.h");
    test_path = cpath_join(test_scratch, sizeof(test_scratch), cgtest_dir.data, "test_cgtest_macros.c");

    /* Each of the three files is written only if it doesn't already
     * exist - see cgtest_create_run()'s header comment for why (in
     * short: an existing cgtest-project.json/cgtest.h/
     * test_cgtest_macros.c is never overwritten, whether it's an
     * unmodified older version or something hand-edited, but a
     * missing one - e.g. a deleted cgtest.h, to pick up a newer
     * cgtest.exe's fix - is still filled back in). */
    if (stat(project_path.data, &st) == 0) {
        result.wrote_project = 0;
        if (!cgtest_create_patch_missing_optional_fields(project_path.data, error_buf, sizeof(error_buf),
                                                          &result.patched_project, &result.project_could_not_be_checked)) {
            return cgtest_create_fail(error_buf);
        }
    } else if (!cgtest_create_write_file(project_path.data, error_buf, sizeof(error_buf),
                                   CGTEST_PROJECT_TEMPLATE, (const char *)NULL)) {
        return cgtest_create_fail(error_buf);
    } else {
        result.wrote_project = 1;
        result.patched_project = 0;
        result.project_could_not_be_checked = 0;
    }

    if (stat(header_path.data, &st) == 0) {
        result.wrote_header = 0;
    } else if (!cgtest_create_write_file(header_path.data, error_buf, sizeof(error_buf),
                                   CGTEST_H_TEMPLATE_HEAD1, CGTEST_H_TEMPLATE_HEAD1B, CGTEST_H_TEMPLATE_HEAD1C,
                                   CGTEST_H_TEMPLATE_HEAD2, CGTEST_H_TEMPLATE_FATAL_FAILED,
                                   CGTEST_H_TEMPLATE_RELPATH1, CGTEST_H_TEMPLATE_STRFIELD1,
                                   CGTEST_H_TEMPLATE_EXPECT_TRUE, CGTEST_H_TEMPLATE_EXPECT_FALSE,
                                   CGTEST_H_TEMPLATE_ASSERT_TRUE, CGTEST_H_TEMPLATE_ASSERT_FALSE,
                                   CGTEST_H_TEMPLATE_CMP_INT1, CGTEST_H_TEMPLATE_CMP_INT2,
                                   CGTEST_H_TEMPLATE_EQ_NE_INT, CGTEST_H_TEMPLATE_EQ_NE_INT2,
                                   CGTEST_H_TEMPLATE_CMP_UINT1, CGTEST_H_TEMPLATE_CMP_UINT2,
                                   CGTEST_H_TEMPLATE_EQ_NE_UINT, CGTEST_H_TEMPLATE_EQ_NE_UINT2,
                                   CGTEST_H_TEMPLATE_CMP_FLOAT1, CGTEST_H_TEMPLATE_CMP_FLOAT2,
                                   CGTEST_H_TEMPLATE_CMP_FLOAT3, CGTEST_H_TEMPLATE_CMP_FLOAT4,
                                   CGTEST_H_TEMPLATE_EQ_NE_FLOAT, CGTEST_H_TEMPLATE_EQ_NE_FLOAT2,
                                   CGTEST_H_TEMPLATE_CMP_DOUBLE1, CGTEST_H_TEMPLATE_CMP_DOUBLE2,
                                   CGTEST_H_TEMPLATE_CMP_DOUBLE3, CGTEST_H_TEMPLATE_CMP_DOUBLE4,
                                   CGTEST_H_TEMPLATE_EQ_NE_DOUBLE, CGTEST_H_TEMPLATE_EQ_NE_DOUBLE2,
                                   CGTEST_H_TEMPLATE_NEAR_DOUBLE1, CGTEST_H_TEMPLATE_NEAR_DOUBLE2,
                                   CGTEST_H_TEMPLATE_NEAR_DOUBLE3,
                                   CGTEST_H_TEMPLATE_CMP_FLOAT_ORD1, CGTEST_H_TEMPLATE_CMP_FLOAT_ORD2,
                                   CGTEST_H_TEMPLATE_CMP_DOUBLE_ORD1, CGTEST_H_TEMPLATE_CMP_DOUBLE_ORD2,
                                   CGTEST_H_TEMPLATE_LT_INT, CGTEST_H_TEMPLATE_LE_INT,
                                   CGTEST_H_TEMPLATE_GT_INT, CGTEST_H_TEMPLATE_GE_INT,
                                   CGTEST_H_TEMPLATE_LT_UINT, CGTEST_H_TEMPLATE_LE_UINT,
                                   CGTEST_H_TEMPLATE_GT_UINT, CGTEST_H_TEMPLATE_GE_UINT,
                                   CGTEST_H_TEMPLATE_LT_FLOAT, CGTEST_H_TEMPLATE_LE_FLOAT,
                                   CGTEST_H_TEMPLATE_GT_FLOAT, CGTEST_H_TEMPLATE_GE_FLOAT,
                                   CGTEST_H_TEMPLATE_LT_DOUBLE, CGTEST_H_TEMPLATE_LE_DOUBLE,
                                   CGTEST_H_TEMPLATE_GT_DOUBLE, CGTEST_H_TEMPLATE_GE_DOUBLE,
                                   CGTEST_H_TEMPLATE_CMP_PTR1, CGTEST_H_TEMPLATE_CMP_PTR2,
                                   CGTEST_H_TEMPLATE_EQ_NE_PTR, CGTEST_H_TEMPLATE_EQ_NE_PTR2,
                                   CGTEST_H_TEMPLATE_STRCASECMP1,
                                   CGTEST_H_TEMPLATE_CMP_STR1, CGTEST_H_TEMPLATE_CMP_STR2,
                                   CGTEST_H_TEMPLATE_EQ_NE_STR, CGTEST_H_TEMPLATE_EQ_NE_STR2,
                                   CGTEST_H_TEMPLATE_CMP_STR_NOCASE1, CGTEST_H_TEMPLATE_CMP_STR_NOCASE2,
                                   CGTEST_H_TEMPLATE_EQ_NE_STR_NOCASE, CGTEST_H_TEMPLATE_EQ_NE_STR_NOCASE2,
                                   CGTEST_H_TEMPLATE_FOOTER,
                                   (const char *)NULL)) {
        return cgtest_create_fail(error_buf);
    } else {
        result.wrote_header = 1;
    }

    if (stat(test_path.data, &st) == 0) {
        result.wrote_test_macros = 0;
    } else if (!cgtest_create_write_file(test_path.data, error_buf, sizeof(error_buf),
                                   CGTEST_TEST_MACROS_TEMPLATE1, CGTEST_TEST_MACROS_TEMPLATE2,
                                   CGTEST_TEST_MACROS_TEMPLATE3,
                                   CGTEST_TEST_MACROS_TEMPLATE4, CGTEST_TEST_MACROS_TEMPLATE5,
                                   CGTEST_TEST_MACROS_TEMPLATE6, CGTEST_TEST_MACROS_TEMPLATE7,
                                   CGTEST_TEST_MACROS_TEMPLATE8, CGTEST_TEST_MACROS_TEMPLATE9,
                                   CGTEST_TEST_MACROS_TEMPLATE10, CGTEST_TEST_MACROS_TEMPLATE11,
                                   CGTEST_TEST_MACROS_TEMPLATE12, CGTEST_TEST_MACROS_TEMPLATE13,
                                   CGTEST_TEST_MACROS_TEMPLATE14,
                                   (const char *)NULL)) {
        return cgtest_create_fail(error_buf);
    } else {
        result.wrote_test_macros = 1;
    }

    result.ok = 1;
    result.dir = cmsg_dup(cgtest_dir.data, cgtest_dir.length);
    result.error = NULL;
    return result;
}

void cgtest_create_free(CGTestCreateResult *result)
{
    free(result->dir);
    result->dir = NULL;
    free(result->error);
    result->error = NULL;
}

/*** End of inlined file: cgtest_create.c ***/


/*** Start of inlined file: cgtest_runner.c ***/

/*** Start of inlined file: cgtest_runner.h ***/
#ifndef CGTEST_RUNNER_H
#define CGTEST_RUNNER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One test_*.c file, already scanned for its test_ functions in the
 * order they appear (see ctestscanner_find()). Non-owning - "label"
 * and "functions" must outlive the cgtest_runner_generate_source()
 * call that reads them. "label" is the file's full path, used both for
 * error messages and as the actual source argument
 * cgtest_runner_build_compile_command() passes to the compiler
 * (cgtest_runner_generate_source() itself only ever uses its
 * basename, for the "== <file> ==" header it prints per file).
 */
typedef struct {
    const char    *label;
    CTestFunction *functions;
    size_t         function_count;
} CGTestRunnerFile;

/* Builds cgtest-runner.c's full source text: a leading comment
 * embedding "compile_command" verbatim (the exact command
 * cgtest_runner_run() is about to invoke to compile this very file -
 * purely informational, so anyone inspecting or manually re-running
 * cgtest-runner.c can see it without digging through cgtest.exe's own
 * output), then one "typedef struct T T;" forward declaration per
 * distinct fixture type across every file (deduplicated - see below),
 * then an "extern" declaration for every discovered function, then a
 * generated main() that calls every discovered function in file order
 * (and within a file, in ctestscanner_find()'s order), printing
 * PASS/FAIL per test and a final summary, exiting nonzero iff any test
 * failed.
 *
 * Regardless of "single_translation_unit", every discovered function is
 * first declared the same way, only "extern" - this function never
 * #includes a file just to make its functions visible:
 *
 *     extern void test_foo(void);                 // plain (void) test
 *
 *     extern void setup_bar(State **state);        // fixture test (see below)
 *     extern void test_bar(State *state);
 *     extern void teardown_bar(State *state);       // only if CTestFunction::has_teardown
 *
 * When "single_translation_unit" is 0 (the default), that's the whole
 * story - each file compiles as its own translation unit (see
 * cgtest_runner_build_compile_command()) and the "extern" declarations
 * above are satisfied by the linker. When it's nonzero, one
 * "#include "<files[i].label>"" line per file is appended right after
 * every "extern" declaration above and before main() - see
 * cgtest_runner.h's header comment for why (and its tradeoff).
 * "files[i].label" is used there as-is (an absolute path, always
 * '/'-separated regardless of platform - see cpath.h), not just its
 * basename the way the "== <file> ==" header below does.
 *
 * A function whose CTestFunction::fixture_type is non-NULL (see
 * specification.md ch.6 "Fixtures") is called wrapped in its own
 * block instead of a bare "test_<name>();":
 *
 *     {
 *         State *state = NULL;
 *         setup_<name>(&state);
 *         if (!cgtest_fatal_failed) {
 *             test_<name>(state);
 *         }
 *         teardown_<name>(state);   // only if CTestFunction::has_teardown
 *     }
 *
 * "State" is forward-declared as an incomplete type ("typedef struct
 * State State;", deduplicated across every use of the same type name)
 * rather than #include'd or copied in full - this function only ever
 * holds/passes a "State *"/"State **" here, never allocates or
 * dereferences one by value, so it never needs the real definition.
 * setup_<name> takes "State **" (an out-param) rather than returning
 * "State *" specifically so it stays void-returning - EXPECT_* and
 * ASSERT_* (see cgtest.h) work inside it exactly like they do in
 * test_<name>, including ASSERT_*'s early "return;", which wouldn't
 * type-check in a function declared to return "State *". "state"
 * starts NULL and is populated by setup_<name> itself; if setup_<name>
 * hits a fatal (ASSERT_*) failure before assigning it, it stays the
 * well-defined NULL it started as - safe to skip in the test_<name>
 * call above and safe to pass to a present teardown_<name>. cgtest
 * itself never allocates or frees "state" - only the author's
 * setup_<name>/teardown_<name> do, and it's fine for neither to free
 * it: the runner process exits shortly after the last test either way.
 *
 * cgtest_fatal_failed (set only by ASSERT_*, unlike cgtest_failed which
 * both EXPECT_* and ASSERT_* set - see cgtest.h) is reset to 0 alongside
 * cgtest_failed right before this block.
 *
 * "<name>" is "test_<name>" with its "test_" prefix stripped. Callers
 * are expected to have already verified setup_<name> exists and set
 * has_teardown accordingly (see cgtest_runner_run()) - this function
 * itself performs no such check, since it's pure and has no way to
 * fail short of OOM.
 *
 * Pure - performs no filesystem access itself. Returns a malloc'd,
 * NUL-terminated string the caller owns (free() it); returns NULL only
 * on allocation failure.
 */
char *cgtest_runner_generate_source(const CGTestRunnerFile *files, size_t file_count, const char *compile_command,
                                     int single_translation_unit);

/* Builds the full compiler invocation for "project": project->compiler_command
 * verbatim, then an include flag for every include_paths and
 * test_directories entry, then every source_files entry, then - only
 * when project->single_translation_unit is 0, the default - every
 * files[i].label (each discovered test_*.c file, compiled as its own
 * translation unit - see cgtest_runner_generate_source()'s header
 * comment for why), then "runner_c_path" itself, then the flag(s)
 * naming "runner_bin_path" as the output. When project->
 * single_translation_unit is nonzero, "files"/"file_count" affect
 * nothing here - every discovered file's code already reached
 * cgtest-runner.c via the "#include" lines cgtest_runner_generate_source()
 * emitted, so passing them as separate source arguments too would
 * compile each one twice. Two flag dialects, chosen by
 * project->msvc: GCC/Clang's "-I\"path\"" and "-o \"path\"" (msvc == 0,
 * the default), or MSVC cl.exe's "/I\"path\"" and "/Fe:\"path\""
 * (msvc != 0) - cl.exe accepts neither "-I" nor "-o", so a plain
 * compiler_command change alone can't target it.
 *
 * Pure - performs no filesystem access itself. Returns a malloc'd,
 * NUL-terminated string the caller owns (free() it); returns NULL only
 * on allocation failure.
 */
char *cgtest_runner_build_compile_command(const CGTestProject *project, const CGTestRunnerFile *files, size_t file_count,
                                           const char *runner_c_path, const char *runner_bin_path);

typedef struct {
    int    ok;          /* 0 = failed before a runner binary could be produced/run; see error */
    char  *error;       /* malloc'd human-readable message, non-NULL only if !ok */
    int    exit_code;   /* the compiled cgtest-runner binary's exit code; valid only if ok */

    /* Wall-clock milliseconds per phase (see ctimer.h), always
     * measured regardless of the -t/--time flag - printing them (only
     * when that flag was given) is cgtest_main.c's job, the one place
     * allowed to write to stdout/stderr (see its own header comment).
     * A phase never reached because an earlier one failed stays 0.0;
     * total_ms is the actual wall time from cgtest_runner_run()'s
     * entry to its return, which may exceed the sum of the four below
     * if the failure happened mid-phase (e.g. partway through file
     * discovery, before "scan" is considered complete). */
    double scan_ms;       /* ctestfiles_scan()/ctestscanner_find() + fixture/duplicate-basename validation */
    double generate_ms;   /* cgtest_runner_generate_source() + writing cgtest-runner.c to disk */
    double compile_ms;    /* the compiler invocation */
    double run_ms;        /* executing the compiled cgtest-runner binary */
    double total_ms;
} CGTestRunResult;

/* Runs "project" end to end:
 *  1. ctestfiles_scan() every entry in project->test_directories, in order.
 *  2. ctestscanner_find() every discovered test_*.c file's content.
 *  3. cgtest_runner_generate_source() the result (embedding the compile
 *     command from step 4 as a leading comment) into
 *     project->output_path/cgtest-runner.c (creating output_path if it
 *     doesn't exist yet, same as cgtest_create_run()'s directory
 *     handling).
 *  4. Compile it there via cgtest_runner_build_compile_command(): project->
 *     compiler_command, project->include_paths, project->source_files,
 *     an include flag for every test_directories entry, and every
 *     discovered test_*.c file passed as its own source argument
 *     (compiled as its own translation unit, alongside cgtest-runner.c
 *     itself).
 *  5. Execute the resulting binary.
 *
 * Before generating the source, any two test_*.c files across
 * different test_directories that share a basename are rejected as an
 * error (see cgtest_runner_generate_source()'s header comment for why
 * that combination would otherwise be ambiguous). Likewise, every
 * discovered test function with a fixture parameter (CTestFunction::
 * fixture_type != NULL) must have a setup_<name> identifier present
 * somewhere among the discovered test files - existence-only, not a
 * signature check (see specification.md ch.6 "Validation before
 * invoking the compiler") - otherwise it is rejected as an error too,
 * rather than surfacing as a raw linker error once the compiler runs.
 * teardown_<name> is optional: its presence is checked the same way,
 * but a missing one is not an error - CTestFunction::has_teardown is
 * simply set to whether it was found, for cgtest_runner_generate_source()
 * to act on.
 *
 * A failure in any of steps 1-4 is reported as !ok with a message;
 * step 5's exit code (whatever the test run itself decided) is always
 * reported via exit_code, ok or not.
 */
CGTestRunResult cgtest_runner_run(const CGTestProject *project);

/* Releases every owned field in "result". Safe to call regardless of ok. */
void cgtest_runner_free(CGTestRunResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CGTEST_RUNNER_H */

/*** End of inlined file: cgtest_runner.h ***/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define CGTEST_RUNNER_MKDIR(path) _mkdir(path)
#define CGTEST_RUNNER_EXE_SUFFIX  ".exe"
static int cgtest_runner_decode_exit(int system_result)
{
    return system_result;
}
/* cmd.exe (which system() shells out to) treats '/' as a switch
 * character, not a path separator - unlike compilers/fopen()/stat(),
 * which all accept forward slashes fine on Windows. Without this,
 * "build/dir/cgtest-runner.exe" is parsed as the command "build" with
 * "/dir/cgtest-runner.exe" read as a chain of switches, and cmd.exe
 * reports "build" as not found. Only the binary actually invoked via
 * system() (below) needs this; cpath_join()'s forward slashes are
 * fine everywhere else, including as compiler arguments. */
static void cgtest_runner_to_native_sep(char *s)
{
    for (; *s != '\0'; s++) {
        if (*s == '/') {
            *s = '\\';
        }
    }
}
#else
#include <unistd.h>
#include <sys/wait.h>
#define CGTEST_RUNNER_MKDIR(path) mkdir(path, 0755)
#define CGTEST_RUNNER_EXE_SUFFIX  ""
static int cgtest_runner_decode_exit(int system_result)
{
    if (system_result == -1) {
        return -1;
    }
    if (WIFEXITED(system_result)) {
        return WEXITSTATUS(system_result);
    }
    return 1;
}
#endif

/* MSVC's <sys/stat.h> defines S_IFDIR/S_IFMT but not the S_ISDIR()
 * convenience macro POSIX builds already get for free. */
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#define CGTEST_RUNNER_PATH_SCRATCH 4096
#define CGTEST_RUNNER_ERROR_BUFSZ  256
#define CGTEST_RUNNER_IDENT_BUFSZ  128

/* A small growable buffer for building unbounded-length text (a
 * generated C source file, or a compiler command line with an
 * arbitrary number of paths) - grows by doubling, like ctestscanner's
 * CTestFunctionList. */
typedef struct {
    char   *data;
    size_t  length;
    size_t  capacity;
} CGTestRunnerBuf;

static int cgtest_runner_buf_init(CGTestRunnerBuf *buf)
{
    buf->capacity = 256;
    buf->length = 0;
    buf->data = (char *)malloc(buf->capacity);
    if (buf->data == NULL) {
        return 0;
    }
    buf->data[0] = '\0';
    return 1;
}

static int cgtest_runner_buf_append(CGTestRunnerBuf *buf, const char *text, size_t text_len)
{
    if (buf->length + text_len + 1 > buf->capacity) {
        size_t new_capacity = buf->capacity;
        char *grown;
        while (new_capacity < buf->length + text_len + 1) {
            new_capacity *= 2;
        }
        grown = (char *)realloc(buf->data, new_capacity);
        if (grown == NULL) {
            return 0;
        }
        buf->data = grown;
        buf->capacity = new_capacity;
    }
    memcpy(buf->data + buf->length, text, text_len);
    buf->length += text_len;
    buf->data[buf->length] = '\0';
    return 1;
}

static int cgtest_runner_buf_append_cstr(CGTestRunnerBuf *buf, const char *text)
{
    return cgtest_runner_buf_append(buf, text, strlen(text));
}

/* Returns a pointer to the filename portion of "path" (everything
 * after its last '/'). Every path this module deals with was built
 * via cpath_join()/cpathlist_register(), which always normalizes to
 * '/' regardless of platform (see cpath.h), so a plain '/' search is
 * enough here - no separate '\\' case is needed. */
static const char *cgtest_runner_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

char *cgtest_runner_generate_source(const CGTestRunnerFile *files, size_t file_count, const char *compile_command,
                                     int single_translation_unit)
{
    CGTestRunnerBuf buf;
    size_t i;
    size_t j;
    size_t k;
    const char **seen_types;
    size_t seen_count;
    size_t total_functions;

    if (!cgtest_runner_buf_init(&buf)) {
        return NULL;
    }

    if (!cgtest_runner_buf_append_cstr(&buf, "/* Generated by cgtest.exe - do not edit. */\n") ||
        !cgtest_runner_buf_append_cstr(&buf, "/* compile: ") ||
        !cgtest_runner_buf_append_cstr(&buf, compile_command) ||
        !cgtest_runner_buf_append_cstr(&buf, " */\n") ||
        !cgtest_runner_buf_append_cstr(&buf,
            "#include <ctype.h>\n"
            "#include <stdio.h>\n"
            "#include <string.h>\n"
            /* isatty()/getcwd() are POSIX, not ISO C, so they need
             * their own headers per platform - isatty() decides below
             * whether the [ RUN ]/[ OK ]/[ FAILED ] lines get ANSI
             * color codes (same reasoning as GoogleTest's own color
             * detection: only meaningful on a real terminal, and would
             * otherwise pollute output piped into a log file or an
             * editor's quickfix list); getcwd() backs cgtest_relpath()
             * below. */
            "#ifdef _WIN32\n"
            "#include <direct.h>\n"
            "#include <io.h>\n"
            "#define CGTEST_ISATTY(fd) _isatty(fd)\n"
            "#define CGTEST_GETCWD _getcwd\n"
            "#else\n"
            "#include <unistd.h>\n"
            "#define CGTEST_ISATTY(fd) isatty(fd)\n"
            "#define CGTEST_GETCWD getcwd\n"
            "#endif\n"
            "\n"
            "int cgtest_failed = 0;\n"
            /* Set only by ASSERT_* (never EXPECT_*) - see cgtest.h.
             * Checked after setup_<name>(&state) below to decide
             * whether test_<name>(&state) runs at all, matching
             * GoogleTest's SetUp()/TestBody() behavior (specification.md
             * ch.6). */
            "int cgtest_fatal_failed = 0;\n"
            "\n")) {
        goto fail;
    }

    /* cgtest_relpath()/cgtest_print_str_field()/cgtest_strcasecmp()
     * are declared "extern" in cgtest.h (see CGTEST_H_TEMPLATE_RELPATH1
     * and friends in cgtest_create.c) but defined here, unconditionally,
     * the one place they exist regardless of which macros any given
     * test file actually uses - not `static` inside cgtest.h itself,
     * which would give every #include'ing test_*.c file (in separate-TU
     * mode - cgtest_runner.h) its own private, possibly-uncalled copy
     * that -Wunused-function would flag. A single non-`static`
     * definition has external linkage, satisfied by the linker from
     * every file that calls it, never "unused" from any one
     * translation unit's point of view. */
    if (!cgtest_runner_buf_append_cstr(&buf,
            "const char *cgtest_relpath(const char *file)\n"
            "{\n"
            "    static char cwd[4096];\n"
            "    size_t i;\n"
            "    size_t len;\n"
            "\n"
            "    if (CGTEST_GETCWD(cwd, sizeof(cwd)) == NULL) {\n"
            "        return file;\n"
            "    }\n"
            "\n") ||
        !cgtest_runner_buf_append_cstr(&buf,
            "    len = strlen(cwd);\n"
            "    for (i = 0; i < len; i++) {\n"
            "        char a = file[i];\n"
            "        char b = cwd[i];\n"
            "        if (a == '\\\\') a = '/';\n"
            "        if (b == '\\\\') b = '/';\n"
            "        if (a != b) {\n"
            "            return file;\n"
            "        }\n"
            "    }\n"
            "    if (file[len] != '/' && file[len] != '\\\\') {\n"
            "        return file;\n"
            "    }\n"
            "    return file + len + 1;\n"
            "}\n"
            "\n") ||
        !cgtest_runner_buf_append_cstr(&buf,
            "void cgtest_print_str_field(const char *prefix, const char *s)\n"
            "{\n"
            "    size_t indent = strlen(prefix);\n"
            "    int first = 1;\n"
            "    size_t i;\n"
            "    unsigned char c;\n"
            "\n"
            "    for (;;) {\n"
            "        if (first) {\n"
            "            fprintf(stderr, \"%s\\\"\", prefix);\n"
            "            first = 0;\n"
            "        } else {\n"
            "            for (i = 0; i < indent; i++) {\n"
            "                fputc(' ', stderr);\n"
            "            }\n"
            "            fputc('\"', stderr);\n"
            "        }\n"
            "\n") ||
        !cgtest_runner_buf_append_cstr(&buf,
            "        for (;;) {\n"
            "            c = (unsigned char)*s;\n"
            "            if (c == '\\0') {\n"
            "                fputs(\"\\\"\\n\", stderr);\n"
            "                return;\n"
            "            }\n"
            "            if (c == '\\n') {\n"
            "                fputs(\"\\\\n\\\"\\n\", stderr);\n"
            "                s++;\n"
            "                break;\n"
            "            }\n"
            "            switch (c) {\n"
            "                case '\\r': fputs(\"\\\\r\", stderr); break;\n"
            "                case '\\t': fputs(\"\\\\t\", stderr); break;\n") ||
        !cgtest_runner_buf_append_cstr(&buf,
            "                case '\\a': fputs(\"\\\\a\", stderr); break;\n"
            "                case '\\b': fputs(\"\\\\b\", stderr); break;\n"
            "                case '\\v': fputs(\"\\\\v\", stderr); break;\n"
            "                case '\\f': fputs(\"\\\\f\", stderr); break;\n") ||
        !cgtest_runner_buf_append_cstr(&buf,
            "                default:\n"
            "                    if (c < 0x20 || c >= 0x7f) {\n"
            "                        fprintf(stderr, \"\\\\x%02x\", (unsigned int)c);\n"
            "                    } else {\n"
            "                        fputc((int)c, stderr);\n"
            "                    }\n"
            "                    break;\n"
            "            }\n"
            "            s++;\n"
            "        }\n"
            "    }\n"
            "}\n"
            "\n") ||
        !cgtest_runner_buf_append_cstr(&buf,
            "int cgtest_strcasecmp(const char *a, const char *b)\n"
            "{\n"
            "    unsigned char ca, cb;\n"
            "\n"
            "    for (;;) {\n"
            "        ca = (unsigned char)*a;\n"
            "        cb = (unsigned char)*b;\n"
            "        if (tolower(ca) != tolower(cb)) {\n"
            "            return 1;\n"
            "        }\n"
            "        if (ca == '\\0') {\n"
            "            return 0;\n"
            "        }\n"
            "        a++;\n"
            "        b++;\n"
            "    }\n"
            "}\n"
            "\n")) {
        goto fail;
    }

    /* single_translation_unit's "single TU" mode (see cgtest_runner.h):
     * every discovered file's real code is #include'd here, before
     * anything below that might otherwise need to know about it -
     * cgtest_runner_build_compile_command() then passes only
     * cgtest-runner.c to the compiler, not files[i].label separately.
     * This has to happen before the fixture-forward-declare block right
     * below, not after it: that block's whole purpose is declaring a
     * fixture type before its real definition is available (see its own
     * comment) - once single-TU mode's #include has already provided
     * the real definition, forward-declaring it too would be a second,
     * incompatible declaration of the same typedef name in one
     * translation unit, a hard error under -pedantic-errors (same
     * reasoning as that block's own dedup requirement, just violated a
     * different way). This block is skipped entirely in that mode
     * instead of being reordered around it. */
    if (single_translation_unit) {
        for (i = 0; i < file_count; i++) {
            if (!cgtest_runner_buf_append_cstr(&buf, "#include \"") ||
                !cgtest_runner_buf_append_cstr(&buf, files[i].label) ||
                !cgtest_runner_buf_append_cstr(&buf, "\"\n")) {
                goto fail;
            }
        }
        if (file_count > 0 && !cgtest_runner_buf_append_cstr(&buf, "\n")) {
            goto fail;
        }
    }

    /* By default (single_translation_unit == 0), every discovered
     * test_*.c file compiles as its own translation unit (see
     * cgtest_runner_build_compile_command()) - cgtest-runner.c never
     * #includes them, so their own static helpers/globals stay properly
     * file-scoped instead of sharing a namespace with every other test
     * file. What cgtest-runner.c needs from each file instead is
     * declared here: a forward declaration of each distinct fixture
     * type (see below), then an "extern" for every discovered function.
     * In single-TU mode (see above), the fixture type is already fully
     * defined by the #include block above by this point, so this whole
     * forward-declare block is skipped - only the "extern" function
     * declarations further below are still emitted in both modes
     * (harmless in single-TU mode: an ordinary, matching redeclaration
     * of an already-#include'd definition, not a second definition).
     *
     * Fixture types are forward-declared as incomplete ("typedef struct
     * T T;") rather than #include'd or copied in full - cgtest-runner.c
     * only ever holds/passes a "T *"/"T **", never allocates or
     * dereferences a "T" by value, so it never needs T's real
     * definition (see specification.md ch.6 "Generated code"). This
     * requires the test file's own definition to tag its struct to
     * match the typedef name (e.g. "typedef struct T { ... } T;", not
     * the tag-less "typedef struct { ... } T;") - only then are the two
     * separately-compiled "T"s the same type by C's own cross-
     * translation-unit compatibility rule, not just two same-spelled
     * but distinct opaque types that happen to work by ABI accident.
     *
     * Deduplicated across every file/function (counted via
     * "total_functions" as an upper bound for the scratch array below)
     * - the same type name forward-declared twice is a hard error under
     * strict C89 -pedantic-errors (that redundant-typedef allowance is
     * C11-only), and two *different* fixture tests sharing a type name
     * is an ordinary, expected case (e.g. every test in one file using
     * the same fixture). */
    total_functions = 0;
    for (i = 0; i < file_count && !single_translation_unit; i++) {
        total_functions += files[i].function_count;
    }

    seen_types = NULL;
    if (total_functions > 0) {
        seen_types = (const char **)malloc(total_functions * sizeof(const char *));
        if (seen_types == NULL) {
            goto fail;
        }
    }

    seen_count = 0;
    for (i = 0; i < file_count && !single_translation_unit; i++) {
        for (j = 0; j < files[i].function_count; j++) {
            const char *fixture_type = files[i].functions[j].fixture_type;
            int already_seen;

            if (fixture_type == NULL) {
                continue;
            }

            already_seen = 0;
            for (k = 0; k < seen_count; k++) {
                if (strcmp(seen_types[k], fixture_type) == 0) {
                    already_seen = 1;
                    break;
                }
            }
            if (already_seen) {
                continue;
            }

            if (!cgtest_runner_buf_append_cstr(&buf, "typedef struct ") ||
                !cgtest_runner_buf_append_cstr(&buf, fixture_type) ||
                !cgtest_runner_buf_append_cstr(&buf, " ") ||
                !cgtest_runner_buf_append_cstr(&buf, fixture_type) ||
                !cgtest_runner_buf_append_cstr(&buf, ";\n")) {
                free(seen_types);
                goto fail;
            }
            seen_types[seen_count++] = fixture_type;
        }
    }
    free(seen_types);

    if (seen_count > 0 && !cgtest_runner_buf_append_cstr(&buf, "\n")) {
        goto fail;
    }

    for (i = 0; i < file_count; i++) {
        for (j = 0; j < files[i].function_count; j++) {
            const char *name = files[i].functions[j].name;
            const char *fixture_type = files[i].functions[j].fixture_type;

            if (fixture_type == NULL) {
                if (!cgtest_runner_buf_append_cstr(&buf, "extern void ") ||
                    !cgtest_runner_buf_append_cstr(&buf, name) ||
                    !cgtest_runner_buf_append_cstr(&buf, "(void);\n")) {
                    goto fail;
                }
            } else {
                /* "<name>" here is "name" with its "test_" prefix
                 * stripped (guaranteed present: ctestscanner_find()
                 * only ever reports functions matching "test_<name>").
                 * setup_<name> takes a "T **" (out-param, not a return
                 * value) specifically so it stays void-returning -
                 * EXPECT_* and ASSERT_* (see cgtest.h) work inside it
                 * completely unchanged, including ASSERT_*'s early
                 * "return;", which wouldn't type-check in a function
                 * declared to return "T *". */
                const char *suffix = name + 5;
                int has_teardown = files[i].functions[j].has_teardown;

                if (!cgtest_runner_buf_append_cstr(&buf, "extern void setup_") ||
                    !cgtest_runner_buf_append_cstr(&buf, suffix) ||
                    !cgtest_runner_buf_append_cstr(&buf, "(") ||
                    !cgtest_runner_buf_append_cstr(&buf, fixture_type) ||
                    !cgtest_runner_buf_append_cstr(&buf, " **state);\n") ||
                    !cgtest_runner_buf_append_cstr(&buf, "extern void ") ||
                    !cgtest_runner_buf_append_cstr(&buf, name) ||
                    !cgtest_runner_buf_append_cstr(&buf, "(") ||
                    !cgtest_runner_buf_append_cstr(&buf, fixture_type) ||
                    !cgtest_runner_buf_append_cstr(&buf, " *state);\n")) {
                    goto fail;
                }

                if (has_teardown) {
                    if (!cgtest_runner_buf_append_cstr(&buf, "extern void teardown_") ||
                        !cgtest_runner_buf_append_cstr(&buf, suffix) ||
                        !cgtest_runner_buf_append_cstr(&buf, "(") ||
                        !cgtest_runner_buf_append_cstr(&buf, fixture_type) ||
                        !cgtest_runner_buf_append_cstr(&buf, " *state);\n")) {
                        goto fail;
                    }
                }
            }
        }
    }

    if (!cgtest_runner_buf_append_cstr(&buf,
            "\n"
            "int main(void)\n"
            "{\n"
            "    int total = 0;\n"
            "    int failed = 0;\n"
            "    int file_total = 0;\n"
            "    int file_failed = 0;\n"
            "    const char *cgtest_green;\n"
            "    const char *cgtest_red;\n"
            "    const char *cgtest_reset;\n"
            "\n"
            /* Without this, stdout is fully block-buffered whenever it
             * isn't a tty (piped, redirected, or captured by an
             * editor's :make) - every printf() below would then sit
             * buffered until exit while stderr's FAIL: lines (from
             * EXPECT_TRUE/EXPECT_FALSE/ASSERT_TRUE/ASSERT_FALSE)
             * appear immediately, making failures print out of
             * chronological order. */
            "    setvbuf(stdout, NULL, _IOLBF, 0);\n"
            "    cgtest_green = CGTEST_ISATTY(1) ? \"\\x1b[32m\" : \"\";\n"
            "    cgtest_red   = CGTEST_ISATTY(1) ? \"\\x1b[31m\" : \"\";\n"
            "    cgtest_reset = CGTEST_ISATTY(1) ? \"\\x1b[0m\"  : \"\";\n"
            "\n")) {
        goto fail;
    }

    for (i = 0; i < file_count; i++) {
        if (files[i].function_count == 0) {
            continue;
        }

        if (!cgtest_runner_buf_append_cstr(&buf, "    file_total = 0;\n    file_failed = 0;\n") ||
            !cgtest_runner_buf_append_cstr(&buf, "    printf(\"== ") ||
            !cgtest_runner_buf_append_cstr(&buf, cgtest_runner_basename(files[i].label)) ||
            !cgtest_runner_buf_append_cstr(&buf, " ==\\n\");\n")) {
            goto fail;
        }

        for (j = 0; j < files[i].function_count; j++) {
            const char *name = files[i].functions[j].name;
            const char *fixture_type = files[i].functions[j].fixture_type;
            /* Printed before the call, not after: whether the test
             * passed isn't known until it's actually run, so this
             * announces which test any FAIL: lines below belong to -
             * same reason GoogleTest has "[ RUN      ]" ahead of its
             * "[       OK ]"/"[  FAILED  ]" verdict rather than trying
             * to print the verdict first. Bracket text/width matches
             * GoogleTest's own literally, not just the general idea. */
            /* Color wraps only the bracket token itself, matching
             * GoogleTest exactly: its PrettyUnitTestResultPrinter
             * colors "[ RUN      ]"/"[       OK ]"/"[  FAILED  ]" but
             * prints the test name after it in the default color. */
            if (!cgtest_runner_buf_append_cstr(&buf, "    printf(\"%s[ RUN      ]%s ") ||
                !cgtest_runner_buf_append_cstr(&buf, name) ||
                !cgtest_runner_buf_append_cstr(&buf, "\\n\", cgtest_green, cgtest_reset);\n    total++;\n    file_total++;\n    cgtest_failed = 0;\n    cgtest_fatal_failed = 0;\n")) {
                goto fail;
            }

            if (fixture_type == NULL) {
                if (!cgtest_runner_buf_append_cstr(&buf, "    ") ||
                    !cgtest_runner_buf_append_cstr(&buf, name) ||
                    !cgtest_runner_buf_append_cstr(&buf, "();\n")) {
                    goto fail;
                }
            } else {
                /* setup_<name> (mandatory) and, if present,
                 * teardown_<name> (optional - specification.md ch.6;
                 * a fixture with nothing to release just omits it,
                 * rather than requiring a no-op function) wrap this
                 * call. "<name>" here is "name" with its "test_" prefix
                 * stripped (guaranteed present: ctestscanner_find()
                 * only ever reports functions matching "test_<name>").
                 * "state" starts NULL and is populated by setup_<name>
                 * itself (an out-param, not a return value - see the
                 * extern declaration above for why); cgtest-runner.c
                 * never allocates or frees *state, only ever holds the
                 * pointer setup_<name> hands back - if setup_<name>
                 * never assigns it (e.g. a fatal ASSERT_* failure before
                 * doing so), "state" stays the well-defined NULL it
                 * started as, safe to pass to a present teardown_<name>.
                 * test_<name>(state) is skipped when setup_<name> hit a
                 * fatal (ASSERT_*) failure, matching GoogleTest's
                 * SetUp()/TestBody() behavior; a present
                 * teardown_<name>(state) still always runs regardless. */
                const char *suffix = name + 5;
                int has_teardown = files[i].functions[j].has_teardown;

                if (!cgtest_runner_buf_append_cstr(&buf, "    {\n        ") ||
                    !cgtest_runner_buf_append_cstr(&buf, fixture_type) ||
                    !cgtest_runner_buf_append_cstr(&buf, " *state = NULL;\n        setup_") ||
                    !cgtest_runner_buf_append_cstr(&buf, suffix) ||
                    !cgtest_runner_buf_append_cstr(&buf, "(&state);\n        if (!cgtest_fatal_failed) {\n            ") ||
                    !cgtest_runner_buf_append_cstr(&buf, name) ||
                    !cgtest_runner_buf_append_cstr(&buf, "(state);\n        }\n")) {
                    goto fail;
                }

                if (has_teardown) {
                    if (!cgtest_runner_buf_append_cstr(&buf, "        teardown_") ||
                        !cgtest_runner_buf_append_cstr(&buf, suffix) ||
                        !cgtest_runner_buf_append_cstr(&buf, "(state);\n")) {
                        goto fail;
                    }
                }

                if (!cgtest_runner_buf_append_cstr(&buf, "    }\n")) {
                    goto fail;
                }
            }

            if (!cgtest_runner_buf_append_cstr(&buf, "    if (!cgtest_failed) {\n        printf(\"%s[       OK ]%s ") ||
                !cgtest_runner_buf_append_cstr(&buf, name) ||
                !cgtest_runner_buf_append_cstr(&buf, "\\n\", cgtest_green, cgtest_reset);\n    } else {\n        printf(\"%s[  FAILED  ]%s ") ||
                !cgtest_runner_buf_append_cstr(&buf, name) ||
                !cgtest_runner_buf_append_cstr(&buf, "\\n\", cgtest_red, cgtest_reset);\n        failed++;\n        file_failed++;\n    }\n")) {
                goto fail;
            }
        }

        if (!cgtest_runner_buf_append_cstr(&buf,
                "    printf(\"             %s%d/%d passed%s\\n\\n\", file_failed == 0 ? cgtest_green : cgtest_red, "
                "file_total - file_failed, file_total, cgtest_reset);\n\n")) {
            goto fail;
        }
    }

    if (!cgtest_runner_buf_append_cstr(&buf,
            "    printf(\"%stotal %d/%d tests passed%s\\n\", failed == 0 ? cgtest_green : cgtest_red, "
            "total - failed, total, cgtest_reset);\n"
            "    return failed == 0 ? 0 : 1;\n"
            "}\n")) {
        goto fail;
    }

    return buf.data;

fail:
    free(buf.data);
    return NULL;
}

static void cgtest_runner_set_error(CGTestRunResult *result, const char *message)
{
    result->ok = 0;
    result->error = cmsg_dup(message, strlen(message));
}

/* Reads "path" whole into a malloc'd, NUL-terminated buffer, setting
 * *out_length to the byte count read (excluding the NUL). Returns
 * NULL on any I/O or allocation failure. */
static char *cgtest_runner_read_file(const char *path, size_t *out_length)
{
    FILE *f;
    long size;
    char *buffer;
    size_t read_count;

    f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(f);
        return NULL;
    }

    read_count = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(buffer);
        return NULL;
    }
    buffer[size] = '\0';

    *out_length = (size_t)size;
    return buffer;
}

/* When project->single_translation_unit is 0 (the default), every
 * discovered test_*.c file (files[i].label) is passed as its own
 * source argument, the same way project->source_files are - each
 * compiles as its own translation unit, matching what
 * cgtest_runner_generate_source() emits in that mode (an "extern"
 * declaration per function, not an #include). When it's nonzero,
 * "files"/"file_count" are not turned into source arguments here at all
 * - every file's code already reached cgtest-runner.c via the
 * "#include" lines cgtest_runner_generate_source() emitted, so it's the
 * only source argument this function adds. Every test_directories
 * entry is still added as its own include flag either way - not for
 * resolving the test files themselves, but so a test file's own
 * #include of a sibling header elsewhere in test_directories still
 * keeps working. */
char *cgtest_runner_build_compile_command(const CGTestProject *project, const CGTestRunnerFile *files, size_t file_count,
                                           const char *runner_c_path, const char *runner_bin_path)
{
    CGTestRunnerBuf buf;
    size_t i;
    const char *include_flag = project->msvc ? " /I\"" : " -I\"";

    if (!cgtest_runner_buf_init(&buf)) {
        return NULL;
    }

    if (!cgtest_runner_buf_append_cstr(&buf, project->compiler_command)) {
        goto fail;
    }

    for (i = 0; i < project->include_paths.count; i++) {
        if (!cgtest_runner_buf_append_cstr(&buf, include_flag) ||
            !cgtest_runner_buf_append_cstr(&buf, project->include_paths.entries[i]) ||
            !cgtest_runner_buf_append_cstr(&buf, "\"")) {
            goto fail;
        }
    }
    for (i = 0; i < project->test_directories.count; i++) {
        if (!cgtest_runner_buf_append_cstr(&buf, include_flag) ||
            !cgtest_runner_buf_append_cstr(&buf, project->test_directories.entries[i]) ||
            !cgtest_runner_buf_append_cstr(&buf, "\"")) {
            goto fail;
        }
    }
    for (i = 0; i < project->source_files.count; i++) {
        if (!cgtest_runner_buf_append_cstr(&buf, " \"") ||
            !cgtest_runner_buf_append_cstr(&buf, project->source_files.entries[i]) ||
            !cgtest_runner_buf_append_cstr(&buf, "\"")) {
            goto fail;
        }
    }
    if (!project->single_translation_unit) {
        for (i = 0; i < file_count; i++) {
            if (!cgtest_runner_buf_append_cstr(&buf, " \"") ||
                !cgtest_runner_buf_append_cstr(&buf, files[i].label) ||
                !cgtest_runner_buf_append_cstr(&buf, "\"")) {
                goto fail;
            }
        }
    }
    if (!cgtest_runner_buf_append_cstr(&buf, " \"") ||
        !cgtest_runner_buf_append_cstr(&buf, runner_c_path) ||
        !cgtest_runner_buf_append_cstr(&buf, "\"")) {
        goto fail;
    }
    if (project->msvc) {
        /* cl.exe has no "-o" - the output name is set via "/Fe:path",
         * directly adjacent to the flag, no space before the quote. */
        if (!cgtest_runner_buf_append_cstr(&buf, " /Fe:\"") ||
            !cgtest_runner_buf_append_cstr(&buf, runner_bin_path) ||
            !cgtest_runner_buf_append_cstr(&buf, "\"")) {
            goto fail;
        }
    } else {
        if (!cgtest_runner_buf_append_cstr(&buf, " -o \"") ||
            !cgtest_runner_buf_append_cstr(&buf, runner_bin_path) ||
            !cgtest_runner_buf_append_cstr(&buf, "\"")) {
            goto fail;
        }
    }

    return buf.data;

fail:
    free(buf.data);
    return NULL;
}

/* Returns 1 if "source" tokenizes to an identifier token spelled
 * exactly "name" anywhere in it, 0 otherwise. Used only for the
 * existence-only fixture check below - it does not distinguish a
 * function definition from a declaration, a call, or any other use,
 * since specification.md ch.6 "Validation before invoking the
 * compiler" only asks that setup_<name>/teardown_<name> exist
 * somewhere, leaving everything else (signature match included) to
 * the C compiler itself. */
static int cgtest_runner_source_has_identifier(const char *source, size_t length, const char *name)
{
    CLexer lexer;
    CToken token;
    size_t name_len = strlen(name);

    clexer_init(&lexer, source, length);
    token = clexer_next_token(&lexer);
    while (token.type != CTOK_EOF) {
        if (token.type == CTOK_IDENTIFIER && token.length == name_len && memcmp(token.start, name, name_len) == 0) {
            return 1;
        }
        token = clexer_next_token(&lexer);
    }
    return 0;
}

static int cgtest_runner_any_file_has_identifier(char *const *contents, const size_t *lengths, size_t count, const char *name)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (cgtest_runner_source_has_identifier(contents[i], lengths[i], name)) {
            return 1;
        }
    }
    return 0;
}

/* Before compiling, every discovered test_ function with a fixture
 * parameter (CTestFunction::fixture_type != NULL) must have a
 * setup_<name> identifier present somewhere among "contents" (every
 * discovered test_*.c file's raw source, not just the ones with their
 * own test_ functions - a shared fixture file might have neither) -
 * otherwise this fails with a clear cgtest-level message instead of
 * surfacing as a raw linker error once the compiler runs. Existence-
 * only (see cgtest_runner_source_has_identifier() above) - a
 * parameter-type mismatch is left entirely to the C compiler's own
 * type checking.
 *
 * teardown_<name> is optional (specification.md ch.6) - unlike a
 * missing setup_<name>, "*state" is never read before setup_<name>
 * runs, so there is nothing unsafe about skipping a teardown that
 * doesn't exist. Rather than erroring, this records whether it exists
 * in fn->has_teardown for cgtest_runner_generate_source() to act on.
 *
 * Returns 1 and leaves "result" untouched on success; returns 0 with
 * "result" set to the failure if any setup_<name> is missing. */
static int cgtest_runner_check_fixtures(CGTestRunnerFile *runner_files, size_t runner_file_count,
                                         char *const *contents, const size_t *lengths, size_t file_count,
                                         CGTestRunResult *result)
{
    size_t i;
    size_t j;

    for (i = 0; i < runner_file_count; i++) {
        for (j = 0; j < runner_files[i].function_count; j++) {
            CTestFunction *fn = &runner_files[i].functions[j];
            const char *suffix;
            char ident[CGTEST_RUNNER_IDENT_BUFSZ];
            char msg[CGTEST_RUNNER_ERROR_BUFSZ];

            if (fn->fixture_type == NULL) {
                continue;
            }
            /* "test_" prefix guaranteed by ctestscanner_find(). */
            suffix = fn->name + 5;

            cmsg_build(ident, sizeof(ident), "setup_", suffix, strlen(suffix), "");
            if (!cgtest_runner_any_file_has_identifier(contents, lengths, file_count, ident)) {
                cmsg_build(msg, sizeof(msg), "missing fixture setup function: ", ident, strlen(ident), "");
                cgtest_runner_set_error(result, msg);
                return 0;
            }

            cmsg_build(ident, sizeof(ident), "teardown_", suffix, strlen(suffix), "");
            fn->has_teardown = cgtest_runner_any_file_has_identifier(contents, lengths, file_count, ident);
        }
    }
    return 1;
}

CGTestRunResult cgtest_runner_run(const CGTestProject *project)
{
    CGTestRunResult result;
    CPathList test_files;
    CGTestRunnerFile *runner_files;
    size_t runner_file_count;
    char **file_contents;
    size_t *file_lengths;
    size_t file_contents_count;
    char *source;
    char runner_c_scratch[CGTEST_RUNNER_PATH_SCRATCH];
    char runner_bin_scratch[CGTEST_RUNNER_PATH_SCRATCH];
    CPath runner_c_path;
    CPath runner_bin_path;
    struct stat st;
    FILE *f;
    char *compile_cmd;
    int system_result;
    size_t i;
    double t_start;
    double t_phase;
    double t_now;
    double scan_ms;
    double generate_ms;
    double compile_ms;
    double run_ms;

    t_start = ctimer_now_ms();
    t_phase = t_start;
    scan_ms = 0.0;
    generate_ms = 0.0;
    compile_ms = 0.0;
    run_ms = 0.0;

    result.ok = 0;
    result.error = NULL;
    result.exit_code = -1;

    cpathlist_init(&test_files);
    runner_files = NULL;
    runner_file_count = 0;
    file_contents = NULL;
    file_lengths = NULL;
    file_contents_count = 0;
    source = NULL;
    compile_cmd = NULL;

    for (i = 0; i < project->test_directories.count; i++) {
        CTestFileScan scan = ctestfiles_scan(project->test_directories.entries[i]);
        size_t j;

        if (!scan.ok) {
            cgtest_runner_set_error(&result, scan.error);
            ctestfiles_free(&scan);
            goto cleanup;
        }

        for (j = 0; j < scan.files.count; j++) {
            if (cpathlist_register(&test_files, "", scan.files.entries[j]) == CPATHLIST_ALLOC_FAILED) {
                cgtest_runner_set_error(&result, "out of memory");
                ctestfiles_free(&scan);
                goto cleanup;
            }
        }
        ctestfiles_free(&scan);
    }

    if (test_files.count == 0) {
        cgtest_runner_set_error(&result, "no test_*.c files found in any test_directories entry");
        goto cleanup;
    }

    runner_files = (CGTestRunnerFile *)malloc(test_files.count * sizeof(CGTestRunnerFile));
    file_contents = (char **)malloc(test_files.count * sizeof(char *));
    file_lengths = (size_t *)malloc(test_files.count * sizeof(size_t));
    if (runner_files == NULL || file_contents == NULL || file_lengths == NULL) {
        cgtest_runner_set_error(&result, "out of memory");
        goto cleanup;
    }

    /* Contents are kept around (not freed right after scanning) so
     * cgtest_runner_check_fixtures() below can search every file's raw
     * text for setup_<name>/teardown_<name> identifiers - freed
     * together with everything else in "cleanup". */
    for (i = 0; i < test_files.count; i++) {
        CTestFunction *functions;
        size_t count;

        file_contents[i] = cgtest_runner_read_file(test_files.entries[i], &file_lengths[i]);
        if (file_contents[i] == NULL) {
            char msg[CGTEST_RUNNER_ERROR_BUFSZ];
            cmsg_build(msg, sizeof(msg), "could not read test file: ", test_files.entries[i], strlen(test_files.entries[i]), "");
            cgtest_runner_set_error(&result, msg);
            goto cleanup;
        }
        file_contents_count++;

        functions = ctestscanner_find(file_contents[i], file_lengths[i], &count);

        if (count > 0) {
            runner_files[runner_file_count].label = test_files.entries[i];
            runner_files[runner_file_count].functions = functions;
            runner_files[runner_file_count].function_count = count;
            runner_file_count++;
        }
    }

    if (runner_file_count == 0) {
        cgtest_runner_set_error(&result, "no test_ functions found in any test_*.c file");
        goto cleanup;
    }

    if (!cgtest_runner_check_fixtures(runner_files, runner_file_count, file_contents, file_lengths,
                                       test_files.count, &result)) {
        goto cleanup;
    }

    /* Two test files sharing a basename across different test_directories
     * is rejected outright regardless of project->single_translation_unit
     * - see cgtest_runner.h for why this stays one unconditional rule
     * rather than one whose applicability depends on the mode: in
     * separate-TU mode (the default), MSVC's cl.exe names each source
     * file's object file after its own basename by default, so two
     * same-named files from different directories in one compile
     * invocation would silently collide (whichever compiles second
     * overwrites the first's object file). Reject that up front rather
     * than risk a silently-wrong build. */
    for (i = 0; i < runner_file_count; i++) {
        size_t k;
        for (k = i + 1; k < runner_file_count; k++) {
            const char *name_i = cgtest_runner_basename(runner_files[i].label);
            const char *name_k = cgtest_runner_basename(runner_files[k].label);
            if (strcmp(name_i, name_k) == 0) {
                char msg[CGTEST_RUNNER_ERROR_BUFSZ];
                cmsg_build(msg, sizeof(msg), "duplicate test file name across test_directories: ", name_i, strlen(name_i), "");
                cgtest_runner_set_error(&result, msg);
                goto cleanup;
            }
        }
    }

    /* "scan" ends here: discovery, function scanning, and every
     * pre-compile validation check above are done - everything below
     * is building/writing cgtest-runner.c itself. */
    t_now = ctimer_now_ms();
    scan_ms = t_now - t_phase;
    t_phase = t_now;

    if (stat(project->output_path, &st) != 0) {
        if (CGTEST_RUNNER_MKDIR(project->output_path) != 0) {
            char msg[CGTEST_RUNNER_ERROR_BUFSZ];
            cmsg_build(msg, sizeof(msg), "could not create output directory ", project->output_path, strlen(project->output_path), "");
            cgtest_runner_set_error(&result, msg);
            goto cleanup;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        char msg[CGTEST_RUNNER_ERROR_BUFSZ];
        cmsg_build(msg, sizeof(msg), "not a directory: ", project->output_path, strlen(project->output_path), "");
        cgtest_runner_set_error(&result, msg);
        goto cleanup;
    }

    runner_c_path = cpath_join(runner_c_scratch, sizeof(runner_c_scratch), project->output_path, "cgtest-runner.c");
    runner_bin_path = cpath_join(runner_bin_scratch, sizeof(runner_bin_scratch), project->output_path,
                                  "cgtest-runner" CGTEST_RUNNER_EXE_SUFFIX);

    /* Computed before generate_source() (rather than alongside the
     * system() call below) so its text can be embedded as a leading
     * comment in cgtest-runner.c itself - handy for anyone who wants
     * to see or re-run the exact compile invocation without digging
     * through cgtest.exe's own output. */
    compile_cmd = cgtest_runner_build_compile_command(project, runner_files, runner_file_count, runner_c_path.data, runner_bin_path.data);
    if (compile_cmd == NULL) {
        cgtest_runner_set_error(&result, "out of memory");
        goto cleanup;
    }

    source = cgtest_runner_generate_source(runner_files, runner_file_count, compile_cmd, project->single_translation_unit);
    if (source == NULL) {
        cgtest_runner_set_error(&result, "out of memory");
        goto cleanup;
    }

    f = fopen(runner_c_path.data, "wb");
    if (f == NULL) {
        char msg[CGTEST_RUNNER_ERROR_BUFSZ];
        cmsg_build(msg, sizeof(msg), "could not create ", runner_c_path.data, runner_c_path.length, "");
        cgtest_runner_set_error(&result, msg);
        goto cleanup;
    }
    {
        size_t len = strlen(source);
        if (fwrite(source, 1, len, f) != len) {
            fclose(f);
            cgtest_runner_set_error(&result, "could not write cgtest-runner.c");
            goto cleanup;
        }
    }
    fclose(f);

    t_now = ctimer_now_ms();
    generate_ms = t_now - t_phase;
    t_phase = t_now;

    system_result = system(compile_cmd);

    /* Measured immediately after the call, before checking whether it
     * failed - compile_ms reflects time actually spent compiling
     * either way, useful even (especially) when the compile fails. */
    t_now = ctimer_now_ms();
    compile_ms = t_now - t_phase;
    t_phase = t_now;

    if (system_result != 0) {
        cgtest_runner_set_error(&result, "compilation failed (see compiler output above)");
        goto cleanup;
    }

    {
        char exec_cmd[CGTEST_RUNNER_PATH_SCRATCH + 4];
#ifdef _WIN32
        cgtest_runner_to_native_sep(runner_bin_path.data);
#endif
        cmsg_build(exec_cmd, sizeof(exec_cmd), "\"", runner_bin_path.data, runner_bin_path.length, "\"");
        system_result = system(exec_cmd);
    }

    /* Same reasoning as compile_ms above - measured before the
     * system_result == -1 check. */
    t_now = ctimer_now_ms();
    run_ms = t_now - t_phase;

    if (system_result == -1) {
        cgtest_runner_set_error(&result, "could not execute cgtest-runner");
        goto cleanup;
    }

    result.ok = 1;
    result.error = NULL;
    result.exit_code = cgtest_runner_decode_exit(system_result);

cleanup:
    result.scan_ms = scan_ms;
    result.generate_ms = generate_ms;
    result.compile_ms = compile_ms;
    result.run_ms = run_ms;
    result.total_ms = ctimer_now_ms() - t_start;

    free(compile_cmd);
    free(source);
    for (i = 0; i < runner_file_count; i++) {
        ctestscanner_free(runner_files[i].functions, runner_files[i].function_count);
    }
    free(runner_files);
    for (i = 0; i < file_contents_count; i++) {
        free(file_contents[i]);
    }
    free(file_contents);
    free(file_lengths);
    cpathlist_free(&test_files);
    return result;
}

void cgtest_runner_free(CGTestRunResult *result)
{
    free(result->error);
    result->error = NULL;
}

/*** End of inlined file: cgtest_runner.c ***/


/*** Start of inlined file: cgtest_arq.c ***/

/*** Start of inlined file: cgtest_arq.h ***/
#ifndef CGTEST_ARQ_H
#define CGTEST_ARQ_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CGTEST_ARG_ERROR,    /* argv was invalid; see CGTestArgs::error */
    CGTEST_ARG_HELP,     /* -h/--help was given */
    CGTEST_ARG_VERSION,  /* -v/--version was given */
    CGTEST_ARG_LICENSE,  /* -l/--license was given */
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
 * -v/--version, -h/--help, -l/--license. Exactly one of these must be
 * given - combining more than one, or giving none, is reported as an
 * error.
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

/*** End of inlined file: cgtest_arq.h ***/

#define ARQ_IMPLEMENTATION

/*** Start of inlined file: arq.h ***/
/*** Start of inlined file: arq_int.h ***/
#ifndef ARQ_STDINT_H
#define ARQ_STDINT_H

#include <stddef.h>  /* for size_t, ptrdiff_t */

#if defined(_MSC_VER)
    #include <stdint.h>
#elif defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C++ C99 */
    #include <stdint.h>
    #if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
        #define ARQ64
    #else
        #define ARQ32
    #endif
#else
    /* C89 */
    typedef signed char        int8_t;
    typedef unsigned char      uint8_t;

    typedef short              int16_t;
    typedef unsigned short     uint16_t;

    typedef int                int32_t;
    typedef unsigned int       uint32_t;

    typedef int8_t   int_least8_t;
    typedef int16_t  int_least16_t;
    typedef int32_t  int_least32_t;

    typedef uint8_t  uint_least8_t;
    typedef uint16_t uint_least16_t;
    typedef uint32_t uint_least32_t;

    #define INT8_MIN   (-128)
    #define INT8_MAX   127
    #define UINT8_MAX  255

    #define INT16_MIN  (-32768)
    #define INT16_MAX  32767
    #define UINT16_MAX 65535

    #define INT32_MIN  (-2147483647 - 1)
    #define INT32_MAX  2147483647
    #define UINT32_MAX 4294967295U

    /* ----------------- 32-bit vs 64-bit detection ----------------- */
    #if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
        #define ARQ64
        #ifndef UINT64_T_DEFINED
            typedef size_t     uint64_t;
            typedef ptrdiff_t  int64_t;
            #define UINT64_T_DEFINED
        #endif
        #define UINT64_MAX ((size_t)-1)
        #define INT64_MAX  ((ptrdiff_t)(UINT64_MAX >> 1))
        #define INT64_MIN  (-INT64_MAX - 1)
    #else
        #define ARQ32
    #endif

    /* ----------------- Pointer-sized integer ----------------- */
    #ifndef UINTPTR_T_DEFINED
        #if defined(_MSC_VER)
            typedef unsigned __int64 uintptr_t;  /* MSVC 64-bit safe */
        #else
            typedef size_t uintptr_t;            /* GCC/Clang: pointer size */
        #endif
        #define UINTPTR_T_DEFINED
    #endif

#endif

#endif /* ARQ_STDINT_H */

/*** End of inlined file: arq_int.h ***/

/*** Start of inlined file: arq_main.h ***/
#ifndef ARQ_H
#define ARQ_H

typedef struct Arq_Queue_tag Arq_Queue;

typedef void (*function_pointer_t)(Arq_Queue *queue);
typedef struct {
        char chr;                /* -v        */
        const char *name;        /* --version */
        function_pointer_t fn;
        const char *arguments;   /* "uint8_t, bool = false" */
} Arq_Option;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t arq_verify(
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
);

uint32_t arq_fn(
        int argc, char **argv,
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
);

void arq_unused(Arq_Queue *queue);
uint32_t arq_uint(Arq_Queue *queue);
uint32_t arq_array_size(Arq_Queue *queue);
int32_t arq_int(Arq_Queue *queue);
double arq_float(Arq_Queue *queue);
char const *arq_cstr_t(Arq_Queue *queue);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_main.h ***/

#ifdef ARQ_IMPLEMENTATION

/*** Start of inlined file: arq_token.h ***/
#ifndef ARQ_TOKEN_H
#define ARQ_TOKEN_H

typedef struct {
        uint32_t id;
        uint32_t size;
        char const *at;
} Arq_Token;

#define ARQ_LIST_OF_IDS                          \
                                                 \
        /* helper to control tokenizer */        \
        X(ARQ_NO_TOKEN)                          \
                                                 \
        /* literals */                           \
        X(ARQ_P_DEC)                             \
        X(ARQ_N_DEC)                             \
        X(ARQ_DEC_FLOAT)                         \
        X(ARQ_HEX)                               \
        X(ARQ_HEX_FLOAT)                         \
        X(ARQ_CMD_RAW_STR)                       \
                                                 \
        /* option operators */                   \
        X(ARQ_OP_EQ)                             \
        X(ARQ_OP_COMMA)                          \
        X(ARQ_OP_ARRAY)                          \
        X(ARQ_OP_L_PARENTHESIS)                  \
        X(ARQ_OP_R_PARENTHESIS)                  \
        X(ARQ_OP_TERMINATOR)                     \
        X(ARQ_OP_UNKNOWN)                        \
                                                 \
        X(ARQ_IDENTFIER)                         \
        /* option keywords */                    \
        X(ARQ_NULL)                              \
        /* option types */                       \
        X(ARQ_TYPE_CSTR)                         \
        X(ARQ_TYPE_UINT)                         \
        X(ARQ_TYPE_ARRAY_SIZE)                   \
        X(ARQ_TYPE_INT)                          \
        X(ARQ_TYPE_FLOAT)                        \
                                                 \
        /* command line tokens */                \
        X(ARQ_CMD_SHORT_OPTION)                  \
        X(ARQ_CMD_LONG_OPTION)                   \
        X(ARQ_CMD_DASHDASH)                      \

#define X(name)name,
typedef enum {
        ARQ_LIST_OF_IDS
        end
} Arq_SymbolID;
#undef X

extern char const *symbol_names[];

#endif

/*** End of inlined file: arq_token.h ***/

/*** Start of inlined file: arq_symbols.c ***/
#define X(name) #name,
char const *symbol_names[] = {
        ARQ_LIST_OF_IDS
};
#undef X

/*** End of inlined file: arq_symbols.c ***/

/*** Start of inlined file: arq_msg.h ***/
#ifndef ARQ_MSG_H
#define ARQ_MSG_H

typedef struct {
        uint32_t SIZE; /* sizeof(error_buffer), */
        uint32_t size;
        char *at;
} Arq_msg;

#ifdef __cplusplus
extern "C" {
#endif

void arq_msg_clear(Arq_msg *m);
void arq_msg_format(Arq_msg *m);

void arq_msg_append_lf(Arq_msg *m);
void arq_msg_append_chr(Arq_msg *m, char const chr);
void arq_msg_append_nchr(Arq_msg *m, char const chr, uint32_t const num_of_chr);
void arq_msg_append_cstr(Arq_msg *m, char const *cstr);
void arq_msg_append_str(Arq_msg *m, char const *str, uint32_t const size);

void arq_msg_set_cstr(Arq_msg *m, char const *cstr);

void arq_msg_insert_line_str(Arq_msg *m, uint32_t line_number, char const *str, uint32_t const size);
void arq_msg_insert_line_cstr(Arq_msg *m, uint32_t line_number, char const *cstr);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_msg.h ***/

/*** Start of inlined file: arq_msg.c ***/
#include <assert.h>
#include <string.h>

void arq_msg_clear(Arq_msg *m) {
        m->size = 0;
}

void arq_msg_format(Arq_msg *m) {
        uint32_t i;
        uint32_t number_of_lf = 0;
        for (i = 0; i < m->size; i++) {
                uint32_t const last = m->size - 1;
                if (m->at[i] == '\n' && i < last) {
                        number_of_lf++;
                }
        }
        {
                uint32_t j;
                uint32_t const INDENT_SIZE = 4;
                uint32_t const shift_right = number_of_lf * INDENT_SIZE;
                assert(m->size + shift_right < m->SIZE);
                for (i = 0; i < m->size; i++) {
                        m->at[m->size - 1 - i + shift_right] = m->at[m->size - 1 - i];
                }
                m->size += shift_right;
                m->at[m->size] = '\0';
                j = 0;
                for (i = shift_right; i < m->size; i++) {
                        m->at[j++] = m->at[i];
                        if (m->at[i] == '\n' && i + INDENT_SIZE < m->size) {
                                memset(&m->at[j], ' ', INDENT_SIZE);
                                j += INDENT_SIZE;
                        }
                }
        }
}

void arq_msg_append_chr(Arq_msg *m, char const chr) {
        assert(m->size + 1 < m->SIZE);
        m->at[m->size++] = chr;
        m->at[m->size] = 0; /* thats wy m->size has to be smaller than m->SIZE */
}

void arq_msg_append_nchr(Arq_msg *m, char const chr, uint32_t const num_of_chr) {
        uint32_t i;
        for (i = 0; i < num_of_chr; i++) {
                arq_msg_append_chr(m, chr);
        }
}

void arq_msg_append_lf(Arq_msg *m) {
        arq_msg_append_chr(m, '\n');
}

void arq_msg_append_cstr(Arq_msg *m, char const *cstr) {
        uint32_t const STRLEN = strlen(cstr);
        uint32_t i;
        for (i = 0; i < STRLEN; i++) {
                arq_msg_append_chr(m, cstr[i]);
        }
}

void arq_msg_append_str(Arq_msg *m, char const *str, uint32_t const size) {
        uint32_t i;
        for (i = 0; i < size; i++) {
                arq_msg_append_chr(m, str[i]);
        }
}

void arq_msg_insert_line_str(Arq_msg *m, uint32_t line_number, char const *str, uint32_t const size) {
        uint32_t A, B, C;
        uint32_t line_counter = 0;
        assert(m->size + size <= m->SIZE);
        for (A = 0; A < m->size; A++) {
                /* find start idx A */
                if (m->at[A] == '\n') line_counter++;
                if (line_counter == line_number) break;
        }
        for (B = m->size - 1; B > A; B--) {
                /* shift right, create space for insertion of str */
                m->at[B + size] = m->at[B];
        }
        m->size = m->size + size;
        A++;
        for(C = 0; C < size; C++) {
                /* copy str into arq->msg */
                m->at[A++] = str[C];
        }
}

void arq_msg_set_cstr(Arq_msg *m, char const *cstr) {
        arq_msg_clear(m);
        arq_msg_append_cstr(m, cstr);
}

void arq_msg_insert_line_cstr(Arq_msg *m, uint32_t line_number, char const *cstr) {
        arq_msg_insert_line_str(m, line_number, cstr, strlen(cstr));
}

/*** End of inlined file: arq_msg.c ***/

/*** Start of inlined file: arq_bool.h ***/
#ifndef ARQ_BOOL_H
#define ARQ_BOOL_H

#if defined(_MSC_VER)
    #include <stdbool.h>
#elif defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C++, >= C99 */
    #include <stdbool.h>
#else
    /* C89 */
    typedef int bool;
    #define true 1
    #define false 0
#endif

#endif

/*** End of inlined file: arq_bool.h ***/

/*** Start of inlined file: arq_conversion.h ***/
#ifndef ARQ_TOK_H
#define ARQ_TOK_H

typedef struct {
        bool error;
        uint32_t u;
} uint_o;

typedef struct {
        bool error;
        int32_t i;
} int_o;

typedef struct {
        bool error;
        double f;
} float_o;

typedef union {
        uint_o ou;
        int_o oi;
        float_o of;
} union_o;

#ifdef __cplusplus
extern "C" {
#endif

bool token_long_option_eq(Arq_Token const *token, char const *cstr);

uint_o arq_tok_pDec_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr);
int_o arq_tok_sDec_to_int(Arq_Token const *token, Arq_msg *error_msg, char const *cstr);
uint_o arq_tok_hex_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr);

float_o arq_tok_decFloat_to_float(Arq_Token const *token);
float_o arq_tok_hexFloat_to_float(Arq_Token const *token);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_conversion.h ***/

/*** Start of inlined file: arq_inttypes.h ***/
#ifndef ARQ_INTTYPES_H
#define ARQ_INTTYPES_H

#if defined(_MSC_VER)
    #include <inttypes.h>
#elif defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C++, >= C99 */
    #include <inttypes.h>
#else
    /* C89 */
    #define PRId8  "d"
    #define PRId16 "d"
    #define PRId32 "d"
    #define PRIu8  "u"
    #define PRIu16 "u"
    #define PRIu32 "u"
    #define PRId64 "ld"
    #define PRIu64 "lu"
#endif

#endif /* ARQ_INTTYPES_H */

/*** End of inlined file: arq_inttypes.h ***/

/*** Start of inlined file: arq_conversion.c ***/
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

bool token_long_option_eq(Arq_Token const *token, char const *cstr) {
        uint32_t i;
        if (strlen(cstr) != token->size - 2) {
                return false;

        }
        for (i = 2; i < token->size; i++) {
                if (cstr[i - 2] != token->at[i]) {
                        return false;
                }
        }
        return true;
}

uint_o arq_tok_pDec_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr) {
        uint_o result = {0};
        uint32_t i;
        assert(token->id == ARQ_P_DEC);
        if (token->at[0] == '+') {
                i = 1;
        } else {
                i = 0;
        }
        for (; i < token->size; i++) {
                uint32_t digit = token->at[i] - '0';
                if (result.u > (UINT32_MAX - digit) / 10) {
                        if (error_msg != NULL) {
                                Arq_Token tok = *token;
                                char buffer[12];
                                sprintf(buffer, "%" PRIu32, UINT32_MAX);
                                arq_msg_clear(error_msg);
                                arq_msg_append_cstr(error_msg, cstr);
                                /*arq_msg_append_cstr(error_msg, "Token '");*/
                                arq_msg_append_str(error_msg, tok.at, tok.size);
                                arq_msg_append_cstr(error_msg, "' positive number > UINT32_MAX ");
                                arq_msg_append_cstr(error_msg, buffer);
                                arq_msg_append_lf(error_msg);
                        }
                        result.error = true;
                        return result;
                }
                result.u = result.u * 10 + digit;
        }
        return result;
}

int_o arq_tok_sDec_to_int(Arq_Token const *token, Arq_msg *error_msg, char const *cstr) {
        int_o result = {0};
        int32_t SIGN;
        uint32_t i;
        assert(token->id == ARQ_P_DEC || token->id == ARQ_N_DEC);

        if (token->at[0] == '-') {
                SIGN = -1;
                i = 1;
        } else if (token->at[0] == '+') {
                SIGN = 1;
                i = 1;
        } else {
                SIGN = 1;
                i = 0;
        }
        assert(SIGN != 0);

        for (; i < token->size; i++) {
                char const ch = token->at[i];
                int32_t const digit = ch - '0';

                if (SIGN > 0) {
                        if (result.i > (INT32_MAX - digit) / 10) {
                                result.error = true;
                                if (error_msg != NULL) {
                                        Arq_Token tok = *token;
                                        char buffer[12];
                                        sprintf(buffer, "%" PRId32, INT32_MAX);
                                        arq_msg_clear(error_msg);
                                        arq_msg_append_cstr(error_msg, cstr);
                                        arq_msg_append_str(error_msg, tok.at, tok.size);
                                        arq_msg_append_cstr(error_msg, "' positive number > INT32_MAX ");
                                        arq_msg_append_cstr(error_msg, buffer);
                                        arq_msg_append_lf(error_msg);
                                }
                                return result;
                        }
                        result.i = result.i * 10 + digit;
                } else {
                        if (result.i < (INT32_MIN + digit) / 10) {
                                result.error = true;
                                if (error_msg != NULL) {
                                        Arq_Token tok = *token;
                                        char buffer[12];
                                        sprintf(buffer, "%" PRId32, INT32_MIN);
                                        arq_msg_clear(error_msg);
                                        arq_msg_append_cstr(error_msg, cstr);
                                        arq_msg_append_str(error_msg, tok.at, tok.size);
                                        arq_msg_append_cstr(error_msg, "' negative number < INT32_MIN ");
                                        arq_msg_append_cstr(error_msg, buffer);
                                        arq_msg_append_lf(error_msg);
                                }
                                return result;
                        }
                        result.i = result.i * 10 - digit;
                }
        }
        return result;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}
static int decval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    return -1;
}

uint_o arq_tok_hex_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr) {
        uint_o result = {0};
        uint32_t i;
        assert(token->id == ARQ_HEX);
        for (i = 2; i < token->size; i++) {
                char const ch = token->at[i];
                int const digit = hexval(ch);
                assert(digit >= 0);
                if (result.u > (UINT32_MAX - digit) / 10) {
                        result.error = true;
                        if (error_msg != NULL) {
                                Arq_Token tok = *token;
                                arq_msg_clear(error_msg);
                                arq_msg_append_cstr(error_msg, cstr);
                                arq_msg_append_str(error_msg, tok.at, tok.size);
                                arq_msg_append_cstr(error_msg, "' more than 8 hex digits");
                                arq_msg_append_lf(error_msg);
                        }
                        return result;
                }
                result.u = result.u * 16 + digit;
        }
        return result;
}

static double arq_pow(double base, int exp) {
        double result = 1.0;
        int negative = 0;

        if (exp < 0) {
                negative = 1;
                exp = -exp;
        }

        while (exp > 0) {
                if (exp % 2 == 1) {
                        result *= base;
                }
                base *= base;
                exp /= 2;
        }

        if (negative) {
                return 1.0 / result;
        }
        return result;
}

float_o arq_tok_decFloat_to_float(Arq_Token const *token) {
        float_o result;
        double value = 0.0;
        int mantissa_sign = 1;
        int exp10 = 0;
        int exp_sign = 1;

        uint32_t i = 0;

        assert(token->id == ARQ_DEC_FLOAT);

        if (i < token->size) {
                switch (token->at[i]) {
                case '+':
                        mantissa_sign = 1;
                        i++;
                        break;
                case '-':
                        mantissa_sign = -1;
                        i++;
                        break;
                }
        }

        /* integer part */
        while (i < token->size) {
                int v = decval(token->at[i]);
                if (v < 0) {
                        break;
                }
                value = value * 10.0 + (double)v;
                i++;
        }

        /* fractional part */
        if (i < token->size && '.' == token->at[i]) {
                double place = 1.0 / 10.0;
                i++;
                while (i < token->size) {
                        int v = decval(token->at[i]);
                        if (v < 0) {
                                break;
                        }
                        value += (double)v * place;
                        place /= 10.0;
                        i++;
                }
        }
        value = mantissa_sign * value;

        /* exponent part (binary exponent) */
        if (i < token->size + 1 && ('e' == token->at[i] || 'E' == token->at[i])) {
                i++;
                switch (token->at[i]) {
                case '+':
                        exp_sign = 1;
                        i++;
                        break;
                case '-':
                        exp_sign = -1;
                        i++;
                        break;
                }
                while (i < token->size) {
                        if (exp10 < 2000) {
                                exp10 = exp10 * 10 + (token->at[i] - '0');
                        }
                        i++;
                }
                if (exp_sign > 0 && exp10 > 1200) {
                        result.f = HUGE_VAL; /* INFINITY */
                        result.error = false;
                        return result;
                }
                if (exp_sign < 0 && exp10 > 1200) {
                        result.f = 0.0;
                        result.error = false;
                        return result;
                }
        }

        /* scale by value * 10^exp10 */
        {
                result.f = value * arq_pow(10.0, (double)(exp_sign * exp10));
                result.error = false;
                return result;
        }
}

float_o arq_tok_hexFloat_to_float(Arq_Token const *token) {
        float_o result;
        double value = 0.0;
        int exp10 = 0;
        int exp_sign = 1;
        int frac_sign;

        uint32_t i = 0;

        if (token->at[i] == '-') {
                frac_sign = -1;
                i++;
        } else if (token->at[i] == '+') {
                frac_sign = 1;
                i++;
        } else {
                frac_sign = 1;
        }

        assert(token->id == ARQ_HEX_FLOAT);
        assert(token->at[i] == '0' && (token->at[i + 1] == 'x' || token->at[i + 1] == 'X'));
        i = i + 2;

        /* integer part */
        while (i < token->size) {
               int v = hexval(token->at[i]);
                if (v < 0) {
                        break;
                }
                value = value * 16.0 + (double)v;
                i++;
        }

        /* fractional part */
        if (i < token->size && '.' == token->at[i]) {
                double place = 1.0 / 16.0;
                i++;
                while (i < token->size) {
                        int v = hexval(token->at[i]);
                        if (v < 0) {
                                break;
                        }
                        value += (double)v * place;
                        place /= 16.0;
                        i++;
                }
        }

        /* exponent part (binary exponent) */
        if (i < token->size + 1 && ('p' == token->at[i] || 'P' == token->at[i])) {
                i++;
                switch (token->at[i]) {
                case '+':
                        exp_sign = 1;
                        i++;
                        break;
                case '-':
                        exp_sign = -1;
                        i++;
                        break;
                }
                while (i < token->size) {
                        if (exp10 < 2000) {
                                exp10 = exp10 * 10 + (token->at[i] - '0');
                        }
                        i++;
                }
                if (exp_sign > 0 && exp10 > 1200) {
                        result.f = HUGE_VAL; /* INFINITY */
                        result.error = false;
                        return result;
                }
                if (exp_sign < 0 && exp10 > 1200) {
                        result.f = 0.0;
                        result.error = false;
                        return result;
                }
        }

        /* scale by 2^final_exp */
        {
                int final_exp = exp_sign * exp10;
                result.f = frac_sign * ldexp(value, final_exp);
                result.error = false;
                return result;
        }
}

/*** End of inlined file: arq_conversion.c ***/

/*** Start of inlined file: arq_lexer.h ***/
#ifndef ARQ_LEXER_H
#define ARQ_LEXER_H

typedef struct {
        uint32_t cursor_idx;
        uint32_t SIZE;
        char const *at;
        Arq_Token token;
} Arq_Lexer;

typedef struct {
        Arq_Lexer lexer;
        uint32_t idx;
} Arq_LexerOpt;

typedef struct {
        Arq_Lexer lexer;
        uint32_t state;
        int argc;
        char **argv;
        int argIdx;
} Arq_LexerCmd;

#ifdef __cplusplus
extern "C" {
#endif

Arq_Lexer arq_lexer_create(void);
Arq_LexerOpt arq_lexerOpt_create(void);
Arq_LexerCmd arq_lexerCmd_create(int argc, char **argv);

void arq_lexer_next_opt_token(Arq_LexerOpt *l);

void arq_lexerCmd_reset(Arq_LexerCmd *cmd);
void arq_lexer_next_cmd_token(Arq_LexerCmd *l);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_lexer.h ***/

/*** Start of inlined file: arq_lexer.c ***/
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>

typedef struct {
    uint32_t id;
    char const *at;
} KeyWord;

static KeyWord const key_words[] = {
        {  ARQ_NULL,          "NULL" },
        {  ARQ_TYPE_CSTR,     "cstr_t" },
        {  ARQ_TYPE_UINT,     "uint" },
        {  ARQ_TYPE_INT,      "int" },
        {  ARQ_TYPE_FLOAT,    "float" },
};

static bool str_eq_keyword(char const *str, uint32_t const str_size, KeyWord const *cstr) {
        uint32_t i;
        if (str_size != strlen(cstr->at)) {
                return false;
        }
        for (i = 0; i < str_size; i++) {
                if (str[i] != cstr->at[i]) {
                        return false;
                }
        }
        return true;
}

static bool is_identifier(char const chr) {
        return isalnum(chr) || chr == '_';
}

static bool identifier_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if (isalpha(l->at[idx]) || l->at[idx] == '_') {
                l->cursor_idx += 1;
                return true;
        }
        return false;
}

static bool array_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if ((idx + 1 < l->SIZE)
        && (l->at[idx] == '[')
        && (l->at[idx + 1] == ']')) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool hex_start(Arq_Lexer *l) {
        uint32_t idx = l->cursor_idx;
        if (l->at[idx] == '+' || l->at[idx] == '-') {
                if (idx + 1 == l->SIZE) {
                        return false;
                }
                idx++;
        }
        if (l->at[idx] != '0') {
                return false;
        }
        if (idx + 1 == l->SIZE) {
                return false;
        }
        idx++;
        if (l->at[idx] != 'x' && l->at[idx + 1] != 'X') {
                return false;
        }
        if (idx + 1 == l->SIZE) {
                return false;
        }
        idx++;
        if (!isxdigit(l->at[idx])) {
                return false;
        }
        l->cursor_idx = idx + 1;
        return true;
}

static bool has_hex_exponent(char const s) {
    return (s == 'p') || (s == 'P');
}

static bool p_dec_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if (isdigit(l->at[idx])) {
                l->cursor_idx += 1;
                return true;
        } else if (idx + 1 < l->SIZE
        && l->at[idx] == '+'
        && isdigit(l->at[idx + 1])) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool n_dec_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if (idx + 1 < l->SIZE
        && l->at[idx] == '-'
        && isdigit(l->at[idx + 1])) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool has_dec_exponent(Arq_Lexer *l) {
        if (l->cursor_idx + 1 < l->SIZE) {
                uint32_t const idx = l->cursor_idx;
                char const chr = l->at[l->cursor_idx];
                bool isExp = (chr == 'e') || (chr == 'E');
                l->cursor_idx++;
                isExp &= p_dec_start(l) || n_dec_start(l);
                if (isExp) {
                        return true;
                }
                l->cursor_idx = idx;
        }
        return false;
}
#if 1
static void dec_float(Arq_Lexer *l, Arq_Token *t) {
        if (l->cursor_idx < l->SIZE && ('.' == l->at[l->cursor_idx])) {
                /* fractional part */
                t->id = ARQ_DEC_FLOAT;
                l->cursor_idx++;
                t->size++;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t->size++;
                }
        }
        if (has_dec_exponent(l)) {
                t->id = ARQ_DEC_FLOAT;
                t->size = &l->at[l->cursor_idx] - t->at;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t->size++;
                }
                return;
        }
        return;
}
#else
static void dec_float(Arq_Lexer *l, Arq_Token *t) {
        if (l->cursor_idx < l->SIZE && ('.' == l->at[l->cursor_idx])) {
                /* fractional part */
                l->cursor_idx++;
                t->size++;
                if (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        t->id = ARQ_DEC_FLOAT;
                }
                l->cursor_idx++;
                t->size++;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t->size++;
                }
        }
        if (has_dec_exponent(l)) {
                t->id = ARQ_DEC_FLOAT;
                t->size = &l->at[l->cursor_idx] - t->at;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t->size++;
                }
                return;
        }
        return;
}
#endif
static void skip_space(Arq_Lexer *l) {
    while (l->cursor_idx < l->SIZE && (l->at[l->cursor_idx] == 0 || isspace(l->at[l->cursor_idx]))) {
            l->cursor_idx++;
    }
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/* cmd_ */
static bool is_long_identifier(char chr) {
        return isalnum(chr) || chr == '-' || chr == '_';
}

static bool is_short_identifier(char chr) {
        return isalpha(chr) || chr == '?';
}

static bool start_short_identifier(Arq_Lexer *l) {
        if (l->at[l->cursor_idx] == '-'
        && is_short_identifier(l->at[l->cursor_idx + 1])) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool start_long_identifier(Arq_Lexer *l) {
        if (l->at[l->cursor_idx] == '-'
        && l->at[l->cursor_idx + 1] == '-'
        && is_long_identifier(l->at[l->cursor_idx + 2])) {
                l->cursor_idx += 3;
                return true;
        }
        return false;
}

static bool start_dash_dash(Arq_Lexer *l) {
        if (l->at[l->cursor_idx] == '-'
        && l->at[l->cursor_idx + 1] == '-'
        && l->SIZE == 2) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static Arq_Token next_token(Arq_Lexer *l, bool has_identifier) {
        Arq_Token t = {0};
        skip_space(l);
        t.at = &l->at[l->cursor_idx];
        t.size = 0;
        if (l->cursor_idx == l->SIZE ) {
                /* space tail */
                t.id = ARQ_NO_TOKEN;
                t.size = 0;
                return t;
        }

        if (l->at[l->cursor_idx] == '=') {
                t.id = ARQ_OP_EQ;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == ',') {
                t.id = ARQ_OP_COMMA;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == '(') {
                t.id = ARQ_OP_L_PARENTHESIS;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == ')') {
                t.id = ARQ_OP_R_PARENTHESIS;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == 0) {
                t.id = ARQ_OP_TERMINATOR;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (has_identifier) {
                if (identifier_start(l)) {
                        uint32_t i;
                        t.id = ARQ_IDENTFIER;
                        t.size = &l->at[l->cursor_idx] - t.at;
                        while (l->cursor_idx < l->SIZE && is_identifier(l->at[l->cursor_idx])) {
                                l->cursor_idx++;
                                t.size++;
                        }
                        for (i = 0; i < sizeof(key_words)/sizeof(KeyWord); i++) {
                                if (str_eq_keyword(t.at,t.size, &key_words[i])) {
                                        t.id = key_words[i].id;
                                }
                        }
                        return t;
                }
        }
        if (array_start(l)) {
                t.id = ARQ_OP_ARRAY;
                t.size = &l->at[l->cursor_idx] - t.at;
                return t;
        }

        if (hex_start(l)) {
                t.id = ARQ_HEX;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && isxdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                if (l->cursor_idx < l->SIZE && ('.' == l->at[l->cursor_idx])) {
                        t.id = ARQ_NO_TOKEN;
                        l->cursor_idx++;
                        t.size++;
                        while (l->cursor_idx < l->SIZE && isxdigit(l->at[l->cursor_idx])) {
                                l->cursor_idx++;
                                t.size++;
                        }
                        if (l->cursor_idx < l->SIZE && has_hex_exponent(l->at[l->cursor_idx])) {
                                l->cursor_idx++;
                                t.size++;
                                if (p_dec_start(l) || n_dec_start(l)) {
                                        t.id = ARQ_HEX_FLOAT;
                                        t.size = &l->at[l->cursor_idx] - t.at;
                                        while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                                                l->cursor_idx++;
                                                t.size++;
                                        }
                                        return t;
                                }
                        }
                } else if (l->cursor_idx < l->SIZE && has_hex_exponent(l->at[l->cursor_idx])) {
                        t.id = ARQ_NO_TOKEN;
                        l->cursor_idx++;
                        t.size++;
                        if (p_dec_start(l) || n_dec_start(l)) {
                                t.id = ARQ_HEX_FLOAT;
                                t.size = &l->at[l->cursor_idx] - t.at;
                                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                                        l->cursor_idx++;
                                        t.size++;
                                }
                                return t;
                        }
                } else {
                        if (t.at[0] == '0') {
                                return t;
                        }
                        t.size = 0;
                        t.id = ARQ_NO_TOKEN;
                }

        }

        if (l->at[l->cursor_idx] ==  '.') {
                if (l->cursor_idx + 1 < l->SIZE && (isdigit(l->at[l->cursor_idx + 1]))) {
                        dec_float(l, &t);
                        switch (t.id) {
                        case ARQ_DEC_FLOAT: return t;
                        default: break;
                        };
                }
        }

        if (p_dec_start(l)) {
                t.id = ARQ_P_DEC;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                dec_float(l, &t);
                switch (t.id) {
                case ARQ_DEC_FLOAT:
                case ARQ_P_DEC: return t;
                default: break;
                };
        }

        if (n_dec_start(l)) {
                t.id = ARQ_N_DEC;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                dec_float(l, &t);
                switch (t.id) {
                case ARQ_DEC_FLOAT:
                case ARQ_N_DEC: return t;
                default: break;
                };
        }

        if (start_short_identifier(l)) {
                t.id = ARQ_CMD_SHORT_OPTION;
                t.size = &l->at[l->cursor_idx] - t.at;
                return t;
        }

        if (start_long_identifier(l)) {
                t.id = ARQ_CMD_LONG_OPTION;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && is_long_identifier(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                if (l->cursor_idx == l->SIZE) {
                        return t;
                }
                if (l->cursor_idx + 1 < l->SIZE && l->at[l->cursor_idx] == '=') {
                        return t;
                }
        }

        if (start_dash_dash(l)) {
                t.id = ARQ_CMD_DASHDASH;
                t.size = &l->at[l->cursor_idx] - t.at;
                return t;
        }

        if (l->cursor_idx < l->SIZE) {
                t.id = ARQ_OP_UNKNOWN;
                while (l->cursor_idx < l->SIZE && !isspace(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
        }
        return t;
}

void arq_lexer_next_opt_token(Arq_LexerOpt *opt) {
        bool has_identifier = true;
        opt->lexer.token = next_token(&opt->lexer, has_identifier);
}

Arq_Lexer arq_lexer_create(void) {
        Arq_Lexer lexer;
        lexer.cursor_idx = 0;
        lexer.SIZE = 0;
        lexer.at = NULL;
        lexer.token.at = NULL;
        lexer.token.id = 0;
        lexer.token.size = 0;
        return lexer;
}

Arq_LexerOpt arq_lexerOpt_create(void) {
        Arq_LexerOpt opt;
        opt.lexer = arq_lexer_create();
        opt.idx = 0;
        return opt;
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

static Arq_Token next_cmd_token(Arq_Lexer *lexer) {
        bool has_identifier = false;
        Arq_Token token = next_token(lexer, has_identifier);
#if 0
        if (token.id == ARQ_CMD_SHORT_OPTION) {
                return token;
        }
        if (lexer->cursor_idx < lexer->SIZE) {
                token.id = ARQ_CMD_RAW_STR;
                token.size = lexer->SIZE;
                lexer->cursor_idx = lexer->SIZE;
        }
#endif
        return token;
}

Arq_LexerCmd arq_lexerCmd_create(int argc, char **argv) {
        Arq_LexerCmd cmd;
        cmd.lexer = arq_lexer_create();
        cmd.argc = argc - 1;
        cmd.argv = argv + 1;
        cmd.argIdx = 0;
        cmd.state = 0;
        return cmd;
}

void arq_lexerCmd_reset(Arq_LexerCmd *cmd) {
        cmd->lexer = arq_lexer_create();
        cmd->argIdx = 0;
        return;
}

#if 0
void arq_lexer_next_cmd_token(Arq_LexerCmd *cmd) {
        if (cmd->argIdx >= cmd->argc) {
                cmd->lexer.token.id = ARQ_NO_TOKEN;
                cmd->lexer.SIZE = 0;
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.at = NULL;
                cmd->lexer.token.at = NULL;
                cmd->lexer.token.size = 0;
                cmd->bundeling = false;
                return;
        }

        if (!cmd->bundeling) {
                cmd->lexer.SIZE = strlen(cmd->argv[cmd->argIdx]);
                cmd->lexer.at = cmd->argv[cmd->argIdx];
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.token = next_cmd_token(&cmd->lexer);
        } else if (is_short_identifier(cmd->lexer.at[cmd->lexer.cursor_idx])) {
                /* bundled options, Option clustering */
                cmd->lexer.token.at = &cmd->lexer.at[cmd->lexer.cursor_idx];
                cmd->lexer.token.id = ARQ_CMD_SHORT_OPTION;
                cmd->lexer.token.size = 1;
                cmd->lexer.cursor_idx++;
        } else {
                cmd->lexer.token = next_cmd_token(&cmd->lexer);
        }

        if (cmd->lexer.cursor_idx < cmd->lexer.SIZE) {
                cmd->bundeling = true;
                return;
        }
        if (cmd->argIdx < cmd->argc) {
                cmd->argIdx++;
        }
        cmd->bundeling = false;
        return;
}
#else
void arq_lexer_next_cmd_token(Arq_LexerCmd *cmd) {
        if (cmd->argIdx >= cmd->argc) {
                cmd->lexer.token.id = ARQ_NO_TOKEN;
                cmd->lexer.SIZE = 0;
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.at = NULL;
                cmd->lexer.token.at = NULL;
                cmd->lexer.token.size = 0;
                cmd->state = 0; /* init=0, token=1, bundeling=2 */
                return;
        }

        switch (cmd->state) {
        case 0: /* Init */ {
                cmd->lexer.SIZE = strlen(cmd->argv[cmd->argIdx]);
                cmd->lexer.at = cmd->argv[cmd->argIdx];
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.token = next_cmd_token(&cmd->lexer);
                if (cmd->lexer.cursor_idx == cmd->lexer.SIZE) {
                        cmd->argIdx++;
                        return;
                }

                if (cmd->lexer.cursor_idx < cmd->lexer.SIZE &&
                cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION &&
                is_short_identifier(cmd->lexer.at[cmd->lexer.cursor_idx])) {
                        cmd->state = 2; /* bundeling */
                        return;
                }

                if (cmd->lexer.cursor_idx + 1 < cmd->lexer.SIZE &&
                cmd->lexer.at[cmd->lexer.cursor_idx] == '=') {
                        cmd->lexer.cursor_idx++;
                        cmd->state = 1; /* token */
                        return;
                }
                cmd->state = 1; /* token */
                return;
        }
        case 1: /* token */ {
                cmd->lexer.token = next_cmd_token(&cmd->lexer);
                if (cmd->lexer.cursor_idx == cmd->lexer.SIZE) {
                        cmd->state = 0; /* init */
                        cmd->argIdx++;
                        return;
                }

                if (cmd->lexer.cursor_idx + 1 < cmd->lexer.SIZE &&
                cmd->lexer.at[cmd->lexer.cursor_idx] == '=') {
                        cmd->lexer.cursor_idx++;
                        return;
                }
                } return;
        case 2: /* bundeling */ {
                cmd->lexer.token.at = &cmd->lexer.at[cmd->lexer.cursor_idx];
                cmd->lexer.token.id = ARQ_CMD_SHORT_OPTION;
                cmd->lexer.token.size = 1;
                cmd->lexer.cursor_idx++;

                if (cmd->lexer.cursor_idx == cmd->lexer.SIZE) {
                        cmd->state = 0; /* init */
                        cmd->argIdx++;
                        return;
                }

                if (cmd->lexer.cursor_idx < cmd->lexer.SIZE &&
                is_short_identifier(cmd->lexer.at[cmd->lexer.cursor_idx])) {
                        return;
                }

                if (cmd->lexer.cursor_idx + 1 < cmd->lexer.SIZE &&
                cmd->lexer.at[cmd->lexer.cursor_idx] == '=') {
                        cmd->lexer.cursor_idx++;
                        cmd->state = 1; /* token */
                        return;
                }

                cmd->state = 1; /* token */
                } return;
        default:
                assert(false);
                return;
        }
}
#endif

/*** End of inlined file: arq_lexer.c ***/

/*** Start of inlined file: arq_immediate.h ***/
#ifndef ARQ_IMMEDIATE_H
#define ARQ_IMMEDIATE_H

#define CMD_LINE_FAILURE "CMD line failure:\nToken '"
#define OPTION_FAILURE "Option failure:\nToken '"

typedef  bool (*arq_fn_imm_literal_error)(Arq_LexerOpt*,  Arq_msg*);

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm(Arq_SymbolID const id, Arq_LexerOpt *opt);
bool arq_imm_noToken(Arq_Token *token);
bool arq_imm_not_identifier(Arq_LexerOpt *opt);

bool arq_imm_literal_uint_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);
bool arq_imm_literal_int_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);
bool arq_imm_literal_float_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);
bool arq_imm_literal_NULL_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);

typedef union_o (*arq_imm_default)(Arq_LexerOpt *opt);
union_o arq_imm_default_uint(Arq_LexerOpt *opt);
union_o arq_imm_default_int(Arq_LexerOpt *opt);
union_o arq_imm_default_float(Arq_LexerOpt *opt);
char const *arq_imm_default_cstr_t(Arq_LexerOpt *opt);

bool arq_imm_is_a_NULL(Arq_LexerOpt *opt);

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm_cmd_is_dashdash(Arq_LexerCmd *cmd);

void arq_imm_cmd_next(Arq_LexerCmd *cmd);
bool arq_imm_cmd_has_token_left(Arq_LexerCmd *cmd);
bool arq_imm_end_of_line(Arq_LexerCmd *cmd);

Arq_LexerOpt arq_imm_get_long(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
);
Arq_LexerOpt arq_imm_get_short(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
);

void arq_imm_cmd_not_a_option(Arq_LexerCmd const *cmd, Arq_msg *error_msg);
bool arq_imm_cmd_is_long_option(Arq_LexerCmd *cmd);
bool arq_imm_cmd_is_short_option(Arq_LexerCmd *cmd);

typedef bool (*arq_imm_is)(Arq_LexerCmd *cmd);
bool arq_imm_is_uint(Arq_LexerCmd *cmd);
bool arq_imm_is_int(Arq_LexerCmd *cmd);
bool arq_imm_is_float(Arq_LexerCmd *cmd);

typedef bool (*arq_imm_optional_argument)(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_uint(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_int(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_float(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_cstr_t(Arq_LexerCmd *cmd, char const **cstr);
bool arq_imm_pick_cstr_t(Arq_LexerCmd *cmd, char const **cstr);

typedef union_o (*arq_imm_argument)(Arq_LexerCmd *cmd, Arq_msg *error_msg);
union_o arq_imm_argument_uint(Arq_LexerCmd *cmd, Arq_msg *error_msg);
union_o arq_imm_argument_int(Arq_LexerCmd *cmd, Arq_msg *error_msg);
union_o arq_imm_argument_float(Arq_LexerCmd *cmd, Arq_msg *error_msg);
char const *arq_imm_argument_csrt_t(Arq_LexerCmd *cmd, Arq_msg *error_msg);

#endif

/*** End of inlined file: arq_immediate.h ***/

/*** Start of inlined file: arq_immediate.c ***/
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm(Arq_SymbolID const id, Arq_LexerOpt *opt) {
        const bool b = (opt->lexer.token.id == id);
        if (b) {
                arq_lexer_next_opt_token(opt);
        }
        return b;
}

bool arq_imm_noToken(Arq_Token *token) {
        const bool b = (token->id == ARQ_NO_TOKEN);
        return b;
}

bool arq_imm_not_identifier(Arq_LexerOpt *opt) {
        const bool b = (opt->lexer.token.id == ARQ_IDENTFIER);
        if (b) {
                arq_lexer_next_opt_token(opt);
        }
        return !b;
}

bool arq_imm_literal_uint_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        uint_o num;
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC:
                num = arq_tok_pDec_to_uint(&opt->lexer.token, error_msg, OPTION_FAILURE);
                break;
        case ARQ_HEX:
                num = arq_tok_hex_to_uint(&opt->lexer.token, error_msg, OPTION_FAILURE);
                break;
        default:
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' is not a uint literal\n");
                num.u = 0;
                num.error = true;
                break;
        }
        if (!num.error) {
                /* success */
                arq_lexer_next_opt_token(opt);
        }
        return num.error; /* return true if successful */
}

bool arq_imm_literal_int_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        int_o num;
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC: case ARQ_N_DEC:
                num = arq_tok_sDec_to_int(&opt->lexer.token, error_msg, OPTION_FAILURE);
                break;
        case ARQ_HEX: {
                uint_o const n = arq_tok_hex_to_uint(&opt->lexer.token, NULL, "");
                num.i = (int32_t)n.u;
                num.error = n.error;
                } break;
        default:
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' is not a int literal\n");
                num.i = 0;
                num.error = true;
                break;
        }
        if (!num.error) {
                /* success */
                arq_lexer_next_opt_token(opt);
        }
        return num.error; /* return true if successful */
}

bool arq_imm_literal_float_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        float_o num;
        switch (opt->lexer.token.id) {
        case ARQ_DEC_FLOAT:
                num = arq_tok_decFloat_to_float(&opt->lexer.token);
                break;
        case ARQ_HEX_FLOAT:
                num = arq_tok_hexFloat_to_float(&opt->lexer.token);
                break;
        default:
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' is not a float literal\n");
                num.f = 0.0;
                num.error = true;
                break;
        }
        if (!num.error) {
                /* success */
                arq_lexer_next_opt_token(opt);
        }
        return num.error; /* return true if successful */
}

bool arq_imm_literal_NULL_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        bool const b = opt->lexer.token.id == ARQ_NULL;
        if (b) {
                /* success */
                arq_lexer_next_opt_token(opt);
                return false;
        } else {
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' must be NULL\n");
                return true;
        }
}

union_o arq_imm_default_uint(Arq_LexerOpt *opt) {
        union_o num = {0};
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC:
                num.ou = arq_tok_pDec_to_uint(&opt->lexer.token, NULL, "");
                break;
        case ARQ_HEX:
                num.ou = arq_tok_hex_to_uint(&opt->lexer.token, NULL, "");
                break;
        default:
                assert(false);
                break;
        }
        assert(num.ou.error == false);
        arq_lexer_next_opt_token(opt);
        return num;
}

union_o arq_imm_default_int(Arq_LexerOpt *opt) {
        union_o num = {0};
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC: case ARQ_N_DEC:
                num.oi = arq_tok_sDec_to_int(&opt->lexer.token, NULL, "");
                break;
        case ARQ_HEX: {
                uint_o const x = arq_tok_hex_to_uint(&opt->lexer.token, NULL, "");
                num.oi.i = (int32_t)x.u;
                num.oi.error = x.error;
                } break;
        default:
                assert(false);
                break;
        }
        assert(num.oi.error == false);
        arq_lexer_next_opt_token(opt);
        return num;
}

union_o arq_imm_default_float(Arq_LexerOpt *opt) {
        union_o num = {0};
        switch (opt->lexer.token.id) {
        case ARQ_DEC_FLOAT:
                num.of = arq_tok_decFloat_to_float(&opt->lexer.token);
                break;
        case ARQ_HEX_FLOAT:
                num.of = arq_tok_hexFloat_to_float(&opt->lexer.token);
                break;
        default:
                assert(false);
                break;
        }
        assert(num.of.error == false);
        arq_lexer_next_opt_token(opt);
        return num;
}

char const *arq_imm_default_cstr_t(Arq_LexerOpt *opt) {
        arq_lexer_next_opt_token(opt);
        return NULL;
}

union_o arq_imm_default_value(Arq_LexerOpt *opt) {
        union_o a = {0};
        (void) opt;
        return a;
}

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm_cmd_is_dashdash(Arq_LexerCmd *cmd) {
        Arq_Token const *token = &cmd->lexer.token;
        const bool b = (token->id == ARQ_CMD_DASHDASH);
        if (b) {
                arq_imm_cmd_next(cmd);
        }
        return b;
}

/* jumps over a bundel of short options */
/* -shello */
/* s is a short option */
/* if s take no argument then 'h' and maybe 'ello' are all short options */
/* if s takes an argument then 'hello' is the argument */
/*     'next_bundle_idx' over jumps 'hello' because in the vector they all bundled short options */
/*     'hello' should be a cstring in the vector but it isn't. */
/*     That's why we have to increment idx for every char (short option) in the bundle 'hello' */
static void next_bundle_idx(Arq_LexerCmd *cmd) {
        char const *const begin = cmd->lexer.token.at;
        char const *const end = begin + strlen(begin);
        char const *x = begin;
        assert(*end == 0);
        while ((cmd->lexer.token.id != ARQ_NO_TOKEN) && (x >= begin) && (x < end)) {
                arq_lexer_next_cmd_token(cmd);
                x = cmd->lexer.token.at;
        }
}

void arq_imm_cmd_next(Arq_LexerCmd *cmd) {
        if (cmd->lexer.token.id != ARQ_NO_TOKEN) {
                arq_lexer_next_cmd_token(cmd);
        }
}

bool arq_imm_cmd_has_token_left(Arq_LexerCmd *cmd) {
        return cmd->lexer.token.id != ARQ_NO_TOKEN;
}

bool arq_imm_cmd_is_long_option(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_CMD_LONG_OPTION);
}

bool arq_imm_cmd_is_short_option(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION);
}

bool arq_imm_is_uint(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_P_DEC) || (cmd->lexer.token.id == ARQ_HEX);
}

bool arq_imm_is_int(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_P_DEC) || (cmd->lexer.token.id == ARQ_N_DEC) || (cmd->lexer.token.id == ARQ_HEX);
}

bool arq_imm_is_float(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_DEC_FLOAT) || (cmd->lexer.token.id == ARQ_HEX_FLOAT);
}

Arq_LexerOpt arq_imm_get_long(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
) {
        Arq_LexerOpt opt = arq_lexerOpt_create();
        Arq_Token const *token = &cmd->lexer.token;
        for (opt.idx = 0; opt.idx < num_of_options; opt.idx++) {
                if (token_long_option_eq(token, options[opt.idx].name)) {
                        opt.lexer.at = options[opt.idx].arguments;
                        opt.lexer.SIZE = strlen(options[opt.idx].arguments);
                        opt.lexer.cursor_idx = 0;
                        arq_imm_cmd_next(cmd);
                        return opt;
                }
        }
        arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
        /* arq_msg_append_cstr(error_msg, "'--"); */
        arq_msg_append_str(error_msg, token->at, token->size);
        arq_msg_append_cstr(error_msg, "' unknown long option ");
        arq_msg_append_lf(error_msg);
        return opt;
}
Arq_LexerOpt arq_imm_get_short(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
) {
        Arq_Token const *token = &cmd->lexer.token;
        uint32_t const IDX = (token->at[0] == '-') ? 1 : 0; /* : 0 because of bundled short options */
        Arq_LexerOpt opt = arq_lexerOpt_create();
        for (opt.idx = 0; opt.idx < num_of_options; opt.idx++) {
                if (token->at[IDX] == options[opt.idx].chr) {
                        opt.lexer.at = options[opt.idx].arguments;
                        opt.lexer.SIZE = strlen(options[opt.idx].arguments);
                        opt.lexer.cursor_idx = 0;
                        arq_imm_cmd_next(cmd);
                        return opt;
                }
        }
        arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
        /* arq_msg_append_cstr(error_msg, "'-"); */
        arq_msg_append_str(error_msg, token->at, token->size);
        arq_msg_append_cstr(error_msg, "' unknown short option ");
        arq_msg_append_lf(error_msg);
        return opt;
}

void arq_imm_cmd_not_a_option(Arq_LexerCmd const *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
        arq_msg_append_str(error_msg, token->at, token->size);
        arq_msg_append_cstr(error_msg, "' is not an option");
        arq_msg_append_lf(error_msg);
}
bool arq_imm_end_of_line(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_NO_TOKEN);
}

bool arq_imm_optional_argument_uint(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        switch (token->id) {
        case ARQ_P_DEC:
                num->ou = arq_tok_pDec_to_uint(token, error_msg, CMD_LINE_FAILURE);
                break;
        case ARQ_HEX:
                num->ou = arq_tok_hex_to_uint(token, error_msg, CMD_LINE_FAILURE);
                break;
        default:
                return false;
        }
        if (num->ou.error) {
                return true; /* overflow */
        }
        arq_imm_cmd_next(cmd);
        return false;
}

bool arq_imm_optional_argument_int(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        switch (token->id) {
        case ARQ_P_DEC:
        case ARQ_N_DEC:
                num->oi = arq_tok_sDec_to_int(token, error_msg, CMD_LINE_FAILURE);
                break;
        case ARQ_HEX: {
                uint_o n = arq_tok_hex_to_uint(token, error_msg, CMD_LINE_FAILURE);
                num->oi.i = (int32_t)n.u;
                num->oi.error = n.error;
                } break;
        default:
                return false;
        }
        if (num->oi.error) {
                return true; /* overflow */
        }
        arq_imm_cmd_next(cmd);
        return false;
}

bool arq_imm_optional_argument_float(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        (void)error_msg;
        switch (token->id) {
        case ARQ_DEC_FLOAT:
                num->of = arq_tok_decFloat_to_float(token);
                break;
        case ARQ_HEX_FLOAT:
                num->of = arq_tok_hexFloat_to_float(token);
                break;
        default:
                return false;
        }
        if (num->of.error) {
                return true;
        }
        arq_imm_cmd_next(cmd);
        return false;
}

bool arq_imm_optional_argument_cstr_t(Arq_LexerCmd *cmd, char const **cstr) {
        Arq_Token const *token = &cmd->lexer.token;
        if (token->id != ARQ_CMD_LONG_OPTION
        && token->id != ARQ_CMD_SHORT_OPTION) {
                *cstr = token->at;
                if (*cstr != NULL) {
                        next_bundle_idx(cmd);
                        return true;
                }
        }
        return false;
}

bool arq_imm_pick_cstr_t(Arq_LexerCmd *cmd, char const **cstr) {
        Arq_Token const *token = &cmd->lexer.token;
        if (token->id != ARQ_NO_TOKEN) {
                *cstr = arq_imm_argument_csrt_t(cmd, NULL);
                return true;
        }
        *cstr = NULL;
        return false;
}

union_o arq_imm_argument_uint(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        union_o result = {0};
        char const *cstr = CMD_LINE_FAILURE;
        switch (token->id) {
        case ARQ_HEX:
                result.ou = arq_tok_hex_to_uint(token, error_msg, cstr);
                break;
        case ARQ_P_DEC:
                result.ou = arq_tok_pDec_to_uint(token, error_msg, cstr);
                break;
        default:
                if (error_msg != NULL) {
                        Arq_Token const tok = *token;
                        arq_msg_append_cstr(error_msg, cstr);
                        arq_msg_append_str(error_msg, tok.at, tok.size);
                        arq_msg_append_cstr(error_msg, "' is not a positiv number");
                        arq_msg_append_lf(error_msg);
                }
                result.ou.error = true;
                return result;
        }
        arq_imm_cmd_next(cmd);
        return result;
}

union_o arq_imm_argument_int(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        union_o result = {0};
        char const *cstr = CMD_LINE_FAILURE;
        switch (token->id) {
        case ARQ_HEX: {
                uint_o const r = arq_tok_hex_to_uint(token, error_msg, cstr);
                result.oi.i = (int32_t) r.u;
                result.oi.error = r.error;
                } break;
        case ARQ_P_DEC:
        case ARQ_N_DEC:
                result.oi = arq_tok_sDec_to_int(token, error_msg, cstr);
                break;
        default:
                if (error_msg != NULL) {
                        Arq_Token const tok = *token;
                        arq_msg_append_cstr(error_msg, cstr);
                        arq_msg_append_str(error_msg, tok.at, tok.size);
                        arq_msg_append_cstr(error_msg, "' is not a signed number");
                        arq_msg_append_lf(error_msg);
                }
                result.oi.error = true;
                return result;
        }
        arq_imm_cmd_next(cmd);
        return result;
}

union_o arq_imm_argument_float(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        union_o result = {0};
        char const *cstr = CMD_LINE_FAILURE;
        switch (token->id) {
        case ARQ_HEX_FLOAT:
                result.of = arq_tok_hexFloat_to_float(token);
                break;
        case ARQ_DEC_FLOAT:
                result.of = arq_tok_decFloat_to_float(token);
                break;
        default:
                if (error_msg != NULL) {
                        Arq_Token const tok = *token;
                        arq_msg_append_cstr(error_msg, cstr);
                        arq_msg_append_str(error_msg, tok.at, tok.size);
                        arq_msg_append_cstr(error_msg, "' is not a float number");
                        arq_msg_append_lf(error_msg);
                }
                result.of.error = true;
                return result;
        }
        arq_imm_cmd_next(cmd);
        return result;
}

char const *arq_imm_argument_csrt_t(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token *token = &cmd->lexer.token;
        char const *result;
        if (token->id == ARQ_NO_TOKEN) {
                Arq_Token const tok = *token;
                arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
                arq_msg_append_str(error_msg, tok.at, tok.size);
                arq_msg_append_cstr(error_msg, "' is not a c string => expected an argument");
                arq_msg_append_lf(error_msg);
                result = NULL;
                return result;
        }

        /* Even it looks like a short or long option but it is not it expects an argument */
        result = token->at;
        next_bundle_idx(cmd);
        return result;
}

/*** End of inlined file: arq_immediate.c ***/

/*** Start of inlined file: arq_arena.h ***/
#ifndef ARQ_ARENA_H
#define ARQ_ARENA_H

#define ARQ_ARENA_SIZE_OF_PADDING sizeof(size_t)

typedef struct {
        uint32_t SIZE;
        uint32_t size;
        char at[1];
} Arq_Arena;

#ifdef __cplusplus
extern "C" {
#endif

Arq_Arena *arq_arena_init(void *buffer, uint32_t const size);
void *arq_arena_malloc(Arq_Arena *m, uint32_t const num_of_bytes);
void *arq_arena_malloc_rest(Arq_Arena *m, uint32_t const size_of_header, uint32_t const size_of_element, uint32_t *num_of_elements);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_arena.h ***/

/*** Start of inlined file: arq_arena.c ***/
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

Arq_Arena *arq_arena_init(void *buffer, uint32_t const _size) {
        uint32_t const offset = (size_t)buffer % ARQ_ARENA_SIZE_OF_PADDING;
        uint32_t const padding = offset > 0 ? ARQ_ARENA_SIZE_OF_PADDING - offset : 0;
        uint32_t const size = _size - padding;
        uint32_t const header_size = offsetof(Arq_Arena, at);
        Arq_Arena *m = (Arq_Arena *)((char*)buffer + padding);
        assert(_size > padding);
        assert(size > header_size);
        assert((size_t)m % ARQ_ARENA_SIZE_OF_PADDING == 0 && "buffer does not align");
        m->SIZE = size - header_size;
        m->size = 0;
        m->at[0] = 0;
        return m;
}

void *arq_arena_malloc(Arq_Arena *m, uint32_t const num_of_bytes) {
        uint32_t const padded_size = ARQ_ARENA_SIZE_OF_PADDING * ((num_of_bytes + ARQ_ARENA_SIZE_OF_PADDING - 1) / ARQ_ARENA_SIZE_OF_PADDING);

        if (num_of_bytes == 0) return NULL;
        assert(m->size + num_of_bytes <= m->SIZE && "arq_arena_malloc need more memory");

        if (m->size + padded_size <= m->SIZE) {
                uint32_t const begin = m->size;
                void *buffer = &m->at[begin];
                m->size += padded_size;
                assert((size_t)buffer % ARQ_ARENA_SIZE_OF_PADDING == 0 && "buffer does not align");
                return buffer;
        } else {
                uint32_t const begin = m->size;
                void *buffer = &m->at[begin];
                m->size += num_of_bytes;
                assert((size_t)buffer % ARQ_ARENA_SIZE_OF_PADDING == 0 && "buffer does not align");
                return buffer;
        }
}

void *arq_arena_malloc_rest(Arq_Arena *m, uint32_t const size_of_header, uint32_t const size_of_element, uint32_t *num_of_elements) {
        uint32_t const size = (m->SIZE - m->size);
        assert(size_of_element > 0);
        assert(size >= size_of_element && "size >= size_of_element arq_arena need more memory");
        *num_of_elements = (size - size_of_header) / size_of_element;
        return arq_arena_malloc(m, size);
}

/*** End of inlined file: arq_arena.c ***/

/*** Start of inlined file: arq_log.h ***/
#ifndef ARQ_LOG_H
#define ARQ_LOG_H

#ifdef ARQ_LOG_MEMORY
        #include <stdio.h>
        #define log_memory(args) do {   \
                        printf args;    \
                        printf("\n");   \
                } while (0)
                /* do { printf(fmt "\n", ##__VA_ARGS__); } while (0) */
#else
        #define log_memory(args) do {} while (0)
#endif

#ifdef ARQ_LOG_TOKENS

        #include <string.h>
        #include <stdio.h>
        void log_print_token_member(Arq_Token *t, uint32_t toknr);
        void log_options_tokens(Arq_Option const *options, uint32_t const num_of_options);
        void log_cmd_tokens(int argc, char **argv);
#else
        #define log_print_token_member(token, nr) ((void)0)
        #define log_options_tokens(opt, num_of_options) ((void)0)
        #define log_cmd_tokens(argc, argv) ((void)0)
#endif

#ifdef ARQ_LOG_TOKENIZER
        /* used for interpreter logging */

        #include <stdio.h>
        #define log_int_banner(fmt) \
                do { printf("---------" fmt "------------\n"); } while (0)
        #define log_int_ln() \
                do { printf("\n"); } while (0)
        #define log_int_comment(fmt) \
                do { printf("    " fmt "\n"); } while (0)
        #define log_int_token(fmt) \
                do { printf("%s\n",symbol_names[fmt]); } while (0)
        #define log_int_token_indent(fmt) \
                do { printf("    %s\n",symbol_names[fmt]); } while (0)
        #define log_inta(args) do { \
                printf("    ");     \
                printf args ;       \
                printf("\n");       \
        } while (0)
        void log_int_uint(uint_o const *x);
        void log_int_int(int_o const *x);
        void log_int_float(float_o const *x);
#else
        #define log_int_banner(fmt) do {} while (0)
        #define log_int_ln() do {} while (0)
        #define log_int_comment(fmt) do {} while (0)
        #define log_int_token(fmt) do {} while (0)
        #define log_int_token_indent(fmt) do {} while (0)
        #define log_inta(args) do {} while (0)
        #define log_int_uint(x) ((void)0);
        #define log_int_int(x) ((void)0);
        #define log_int_float(x) ((void)0);
#endif

#endif

/*** End of inlined file: arq_log.h ***/

/*** Start of inlined file: arq_log.c ***/
#ifdef ARQ_LOG_TOKENS

void log_print_token_member(Arq_Token *t, uint32_t toknr) {
        uint32_t i;
        printf("%3d %30s -> ", toknr, symbol_names[t->id]);
        printf("%2d ", t->size);
        for (i = 0; i < t->size; i++) {
                putchar(t->at[i]);
        }
        printf("\n");
}

void log_options_tokens(Arq_Option const *options, uint32_t const num_of_options) {
        uint32_t n;
        for (n = 0; n < num_of_options; n++) {
                uint32_t i = 0;
                Arq_LexerOpt opt = arq_lexerOpt_create();
                opt.lexer.at = options[n].arguments;
                opt.lexer.SIZE = strlen(options[n].arguments);
                opt.lexer.cursor_idx = 0;
                printf("Option[%d] -%c --%s %s\n", n, options[n].chr, options[n].name, options[n].arguments);
                do {
                        arq_lexer_next_opt_token(&opt);
                        log_print_token_member(&opt.lexer.token, i++);
                } while (opt.lexer.token.id != ARQ_NO_TOKEN);
                printf("\n");
        }
}

void log_cmd_tokens(int argc, char **argv)  {
        Arq_LexerCmd cmd = arq_lexerCmd_create(argc, argv);
        uint32_t i = 0;
        printf("Command line tokens:\n");
        do {
                arq_lexer_next_cmd_token(&cmd);
                log_print_token_member(&cmd.lexer.token, i++);

        } while(cmd.lexer.token.id != ARQ_NO_TOKEN);
        printf("\n");
}
#endif

#ifdef ARQ_LOG_TOKENIZER
void log_int_uint(uint_o const *x) {
        printf("    ");
        printf("%d\n", x->u);
}
void log_int_int(int_o const *x) {
        printf("    ");
        printf("%d\n", x->i);
}
void log_int_float(float_o const *x) {
        printf("    ");
        printf("%f\n", x->f);
}
#endif

/*** End of inlined file: arq_log.c ***/

/*** Start of inlined file: arq_queue.h ***/
#ifndef ARQ_QUEUE_H
#define ARQ_QUEUE_H

typedef struct {
        Arq_SymbolID type_id;
        union {
                uint8_t u8;
                uint16_t u16;
                uint32_t u32;
                /* uint64_t u64; */
                int8_t i8;
                int16_t i16;
                int32_t i32;
                /* int64_t i64; */
                double f;
                char const *cstr;
        } value;
} Arq_Argument;

struct Arq_Queue_tag{
        uint32_t shrink;
        uint32_t NUM_OF_ARGUMENTS;
        uint32_t read_idx;
        uint32_t write_idx;
        Arq_Argument at[1];
};

#ifdef __cplusplus
extern "C" {
#endif

Arq_Queue *arq_queue_malloc(Arq_Arena *arena);
void arq_queue_clear(Arq_Queue *queue);

typedef void (*arq_push)(Arq_Queue *queue, union_o const *x);
void arq_push_uint(Arq_Queue *queue, union_o const *x);
void arq_push_int(Arq_Queue *queue, union_o const *x);
void arq_push_float(Arq_Queue *queue, union_o const *x);
uint32_t *arq_push_array_size(Arq_Queue *queue, uint32_t n);
void arq_push_cstr_t(Arq_Queue *queue, char const *cstr);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_queue.h ***/

/*** Start of inlined file: arq_queue.c ***/
#include <stddef.h>
#include <string.h>
#include <assert.h>

Arq_Queue *arq_queue_malloc(Arq_Arena *arena) {
        uint32_t NUM_OF_ARGUMENTS;
        uint32_t const shrink_snapshot = arena->size;
        Arq_Queue *queue = (Arq_Queue *)arq_arena_malloc_rest(arena, offsetof(Arq_Queue, at), sizeof(Arq_Argument), &NUM_OF_ARGUMENTS);
        queue->shrink = shrink_snapshot;
        queue->NUM_OF_ARGUMENTS = NUM_OF_ARGUMENTS;
        queue->read_idx = 0;
        queue->write_idx = 0;
        {
                Arq_Argument a;
                a.type_id = ARQ_TYPE_UINT;
                a.value.u32 = 0;
                queue->at[0] = a;
        }
        return queue;
}

void arq_queue_clear(Arq_Queue *queue) {
        queue->read_idx = 0;
        queue->write_idx = 0;
}

static Arq_Argument pop(Arq_Queue *queue) {
        assert(queue->read_idx < queue->write_idx && "queue is empty");
        assert(queue->read_idx < queue->NUM_OF_ARGUMENTS);
        {
                Arq_Argument argument = queue->at[queue->read_idx];
                queue->read_idx++;
                return argument;
        }
}

static void push(Arq_Queue *queue, Arq_Argument const *argument) {
        assert(queue->write_idx < queue->NUM_OF_ARGUMENTS);
        queue->at[queue->write_idx] = *argument;
        queue->write_idx++;
}

void arq_unused(Arq_Queue *queue) {
        (void)pop(queue);
}

uint32_t arq_uint(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_UINT);
        return t.value.u32;
}

uint32_t arq_array_size(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_ARRAY_SIZE);
        return t.value.u32;
}

#if 0
uint64_t arq_uint64_t(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_OPT_UINT64_T);
        return t.value.u64;
}
#endif

int32_t arq_int(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_INT);
        return t.value.i32;
}

double arq_float(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_FLOAT);
        return t.value.f;
}

char const *arq_cstr_t(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_CSTR);
        return t.value.cstr;
}

void arq_push_uint(Arq_Queue *queue, union_o const *x) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_UINT;
        a.value.u32 = x->ou.u;
        push(queue, &a);
        log_int_uint(&x->ou);
}

uint32_t *arq_push_array_size(Arq_Queue *queue, uint32_t n) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_ARRAY_SIZE;
        a.value.u32 = n;
        push(queue, &a);
        return &queue->at[queue->write_idx - 1].value.u32;
}

#if 0
void arq_push_uint64_t(Arq_Queue *queue, uint64_t n) {
        Arq_Argument a;
        a.type_id = ARQ_OPT_UINT64_T;
        a.value.u64 = n;
        push(queue, &a);
}
#endif

void arq_push_int(Arq_Queue *queue, union_o const *x) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_INT;
        a.value.i32 = x->oi.i;
        push(queue, &a);
        log_int_int(&x->oi);
}

void arq_push_float(Arq_Queue *queue, union_o const *x) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_FLOAT;
        a.value.f = x->of.f;
        push(queue, &a);
        log_int_float(&x->of);
}

void arq_push_cstr_t(Arq_Queue *queue, char const * cstr) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_CSTR;
        a.value.cstr = cstr;
        push(queue, &a);
}

/*** End of inlined file: arq_queue.c ***/

/*** Start of inlined file: arq_main.c ***/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

static void error_msg_append_option(Arq_msg *error_msg, Arq_Option const *o) {
        assert(o != NULL);
        if (o->chr != 0) {
                arq_msg_append_cstr(error_msg, "-");
                arq_msg_append_chr(error_msg, o->chr);
                arq_msg_append_cstr(error_msg, " ");
        }
        if (strlen(o->name) != 0) {
                arq_msg_append_cstr(error_msg, "--");
                arq_msg_append_cstr(error_msg, o->name);
                arq_msg_append_cstr(error_msg, " ");
        }
        if (strlen(o->arguments ) != 0) {
                arq_msg_append_cstr(error_msg, o->arguments);
        }
        arq_msg_append_lf(error_msg);
}

static void error_msg_insert_cmd_line(Arq_msg *m, uint32_t line_nr, Arq_LexerCmd *cmd) {
        Arq_Token const token = cmd->lexer.token;
        uint32_t A_IDX, B_IDX, C_IDX, D_IDX, ARGV_LEN;
        uint32_t ln_count = 0;
        uint32_t i;
        log_int_token(cmd->lexer.token.id);
        A_IDX = 0;
        for (i = 0; i < m->size; i++) {
                if (m->at[i] == '\n') {
                        ln_count++;
                        if (line_nr == ln_count) {
                                A_IDX = i + 1;
                        }
                }
        }

        B_IDX = m->size;
        cmd->argIdx = 0;
        cmd->lexer = arq_lexer_create();
        cmd->state = 0;
        arq_lexer_next_cmd_token(cmd);
        while(true) {
                /* render argv to calculate argv_len */
                if (cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION) {
                        uint32_t const x = cmd->lexer.token.at[0] == '-' ? 1 : 0; /* because of short option bundeling  */
                        arq_msg_append_chr(m, cmd->lexer.token.at[0]);
                        arq_msg_append_nchr(m, cmd->lexer.token.at[1], x);
                        arq_msg_append_chr(m, '_');
                } else if (cmd->lexer.token.id == ARQ_CMD_LONG_OPTION) {
                        arq_msg_append_str(m, cmd->lexer.token.at, cmd->lexer.token.size);
                        arq_msg_append_chr(m, '_');
                } else if (cmd->lexer.token.id == ARQ_NO_TOKEN) {
                } else {
                        arq_msg_append_str(m, cmd->lexer.token.at, cmd->lexer.token.size);
                        arq_msg_append_chr(m, '_');
                }
                if (cmd->lexer.token.at == token.at) {
                        break;
                }
                arq_lexer_next_cmd_token(cmd);
        }
        arq_msg_append_lf(m);
        C_IDX = m->size;
        ARGV_LEN = C_IDX - B_IDX;
        {
                uint32_t const shift_right = B_IDX - A_IDX;
                for (i = 0; i < shift_right; i++) {
                        uint32_t const idx = B_IDX - 1 - i;
                        m->at[idx + ARGV_LEN] = m->at[idx];
                }
        }

        D_IDX = m->size;
        cmd->argIdx = 0;
        cmd->lexer = arq_lexer_create();
        cmd->state = 0;
        arq_lexer_next_cmd_token(cmd);
        while(true) {
                /* render argv once more for moving argv */
                if (cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION) {
                        uint32_t const x = cmd->lexer.token.at[0] == '-' ? 1 : 0; /* because of short option bundeling  */
                        arq_msg_append_chr(m, cmd->lexer.token.at[0]);
                        arq_msg_append_nchr(m, cmd->lexer.token.at[1], x);
                        arq_msg_append_chr(m, ' ');
                } else if (cmd->lexer.token.id == ARQ_CMD_LONG_OPTION) {
                        arq_msg_append_str(m, cmd->lexer.token.at, cmd->lexer.token.size);
                        arq_msg_append_chr(m, ' ');
                } else if (cmd->lexer.token.id == ARQ_NO_TOKEN) {
                } else {
                        arq_msg_append_str(m, cmd->lexer.token.at, cmd->lexer.token.size);
                        arq_msg_append_chr(m, ' ');
                }
                if (cmd->lexer.token.at == token.at) {
                        break;
                }
                arq_lexer_next_cmd_token(cmd);
        }
        arq_msg_append_lf(m);

        {
                /* moving rendered argv */
                for (i = 0; i < ARGV_LEN; i++) {
                        m->at[i + A_IDX] =  m->at[i + D_IDX];
                }
                /* delete argv */
                m->size = D_IDX;
        }
}

static void output_error_msg(Arq_msg *error_msg, char *arena_buffer) {
        uint32_t i;
        arq_msg_format(error_msg);
        for (i = 0; i < error_msg->size; i++) {
                arena_buffer[i] = error_msg->at[i];
        }
        arena_buffer[error_msg->size] = 0;
        assert(arena_buffer[error_msg->size] == 0);
}

static void call_back_function(Arq_Option const *options, uint32_t option_idx, Arq_Queue *queue) {
        Arq_Option const *option = &options[option_idx];
        option->fn(queue);
        assert(queue->read_idx == queue->write_idx && "Queue is not empty, there are still arguments in the queue!");
        arq_queue_clear(queue);
}

static uint32_t arq_option_parameter_idx(Arq_Option const *option) {
        uint32_t STRLEN;
        uint32_t result = 0;
        if (option->chr != 0) {
                result += 3;
        }
        STRLEN = strlen(option->name);
        if (STRLEN > 0) {
                result += STRLEN + 3;
        }
        return result;
}

uint32_t arq_verify(
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
) {
        Arq_msg error_msg;
        uint32_t i;
        Arq_Arena *arena;
        (void) buffer_size;

        arena = arq_arena_init(arena_buffer, buffer_size);

        {
                uint32_t SIZE_OF_ERROR_MSG;
                uint32_t const shrink = arena->size;
                char *chr = (char *)arq_arena_malloc_rest(arena, 0, sizeof(char), &SIZE_OF_ERROR_MSG);
                arena->size = shrink;
                error_msg.SIZE = SIZE_OF_ERROR_MSG;
                error_msg.size = 0;
                error_msg.at = chr;
        }

        for (i = 0; i < num_of_options; i++) {
                bool for_loop_continue = false;
                Arq_LexerOpt opt = arq_lexerOpt_create();
                opt.lexer.at = options[i].arguments;
                opt.lexer.SIZE = strlen(options[i].arguments);
                opt.lexer.cursor_idx = 0;
                arq_lexer_next_opt_token(&opt);
                arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                arq_msg_append_cstr(&error_msg, "' missing open parenthesis '('\n");
                if (arq_imm(ARQ_OP_L_PARENTHESIS, &opt)) {
                        if (arq_imm(ARQ_OP_R_PARENTHESIS, &opt)) {
                                if (arq_imm_noToken(&opt.lexer.token)) {
                                        continue;
                                }
                                arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                arq_msg_append_cstr(&error_msg, "' after ')' no tokens allowed!\n");
                                goto error;
                        }
                        do {
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                                arq_fn_imm_literal_error LITERAL_ERROR = NULL;
                                if      (arq_imm(ARQ_TYPE_UINT,  &opt)) LITERAL_ERROR = arq_imm_literal_uint_error;
                                else if (arq_imm(ARQ_TYPE_INT,   &opt)) LITERAL_ERROR = arq_imm_literal_int_error;
                                else if (arq_imm(ARQ_TYPE_FLOAT, &opt)) LITERAL_ERROR = arq_imm_literal_float_error;
                                else if (arq_imm(ARQ_TYPE_CSTR,  &opt)) LITERAL_ERROR = arq_imm_literal_NULL_error;
                                else {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' is not a type\n");
                                        goto error;
                                }

                                if (arq_imm_not_identifier(&opt)) {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' is not a parameter name\n");
                                        goto error;
                                } else {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' but expected '=' or '[]' or ',' or ')'\n");
                                }
                                if (arq_imm(ARQ_OP_EQ, &opt)) {
                                        if (LITERAL_ERROR(&opt, &error_msg)) {
                                                goto error;
                                        }
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' but expected ',' or ')'\n");
                                } else if (arq_imm(ARQ_OP_ARRAY, &opt)) {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' but expected ',' or ')'\n");
                                }
                                if (arq_imm(ARQ_OP_COMMA, &opt)) {
                                        continue;
                                }
                                if (arq_imm(ARQ_OP_R_PARENTHESIS, &opt)) {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' after ')' no tokens allowed!\n");
                                        if (arq_imm_noToken(&opt.lexer.token)) {
                                                for_loop_continue = true;
                                                break;
                                        }
                                }
                                goto error;
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                        } while (opt.lexer.token.id != ARQ_NO_TOKEN);
                        if (for_loop_continue) {
                                continue;
                        }
                }
error:
                {
                        uint32_t n;
                        uint_o ups;
                        ups.error = true;
                        ups.u = opt.lexer.cursor_idx - opt.lexer.token.size;
                        n = arq_option_parameter_idx(&options[i]) + ups.u;
                        error_msg_append_option(&error_msg, &options[i]);
                        arq_msg_append_nchr(&error_msg, ' ', n);
                        arq_msg_append_cstr(&error_msg, "^\n");
                        output_error_msg(&error_msg, arena_buffer);
                        return error_msg.size;
                }

        } /* for loop */
        /* assert(false); */
        return 0;
}

uint32_t arq_fn(
        int argc, char **argv,
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
) {
        Arq_LexerCmd cmd = arq_lexerCmd_create(argc, argv);
        Arq_LexerOpt opt;
        Arq_Arena *arena;
        Arq_msg error_msg;
        Arq_Queue *queue;

        log_memory( ("size of arq_int %ld bit\n", 8 * sizeof(((uint_o *)0)->u) ));

        log_options_tokens(options, num_of_options);
        log_cmd_tokens(argc, argv);

        log_memory(("------- memory usage in byte --------"));
        log_memory(("%11shead %2scapacity %6srest", "", "", ""));
        log_memory(("Buffer    %5d %10d %10s", 0, buffer_size, "-"));
        arena = arq_arena_init(arena_buffer, buffer_size);
        log_memory(("Arena     %5d %10d %10s", (int)offsetof(Arq_Arena, at), arena->SIZE, "-"));

        {
                uint32_t SIZE_OF_ERROR_MSG = 500;
                error_msg.at = (char *)arq_arena_malloc(arena, SIZE_OF_ERROR_MSG);
                error_msg.SIZE = SIZE_OF_ERROR_MSG;
                error_msg.size = 0;
                log_memory(("error_msg %5d %10d %10d", 0, error_msg.SIZE, (int)(arena->SIZE - arena->size)));
        }

        log_int_banner("interpreter");

        queue = arq_queue_malloc(arena);

        log_memory(("Arq_Queue %5d %10d %10d",
                (int)offsetof(Arq_Queue, at),
                (int)(queue->NUM_OF_ARGUMENTS * sizeof(queue->at[0])),
                (int)(arena->SIZE - arena->size))
                /*(int)(arena->SIZE - queue->NUM_OF_ARGUMENTS * sizeof(queue->at[0])))*/

        );
        log_memory(("\n%d arguments fit in the queue.\n", queue->NUM_OF_ARGUMENTS));

        arq_lexer_next_cmd_token(&cmd);
        while(arq_imm_cmd_has_token_left(&cmd)) {
                struct {
                        arq_imm_default DEFAULT_VALUE;
                        arq_imm_optional_argument OPTIONAL_ARGUMENT;
                        arq_push PUSH;
                        arq_imm_is IS_LITERAL_TYPE;
                        arq_imm_argument ARGUMENT;
                } imm;
                /* Arq_OptVector *opt = NULL; */
                log_int_ln();
                if (arq_imm_cmd_is_long_option(&cmd)) {
                        log_int_token(ARQ_CMD_LONG_OPTION);
                        opt = arq_imm_get_long(options, num_of_options, &cmd, &error_msg);
                        if (opt.lexer.at == NULL) {
                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                output_error_msg(&error_msg, arena_buffer);
                                return error_msg.size;
                        }
                } else if (arq_imm_cmd_is_short_option(&cmd)) {
                        log_int_token(ARQ_CMD_SHORT_OPTION);
                        opt = arq_imm_get_short(options, num_of_options, &cmd, &error_msg);
                        if (opt.lexer.at == NULL) {
                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                output_error_msg(&error_msg, arena_buffer);
                                return error_msg.size;
                        }
                } else if (arq_imm_end_of_line(&cmd)) {
                        log_int_token(ARQ_NO_TOKEN);
                        arena_buffer[0] = 0;
                        return 0;
                } else {
                        arq_imm_cmd_not_a_option(&cmd, &error_msg);
                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                        output_error_msg(&error_msg, arena_buffer);
                        return error_msg.size;
                }
                arq_lexer_next_opt_token(&opt);
                (void)arq_imm(ARQ_OP_L_PARENTHESIS, &opt);
                while (true) {
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                        if (arq_imm(ARQ_TYPE_CSTR, &opt)) {
                                char const *cstr;
                                log_int_token_indent(ARQ_TYPE_CSTR);
                                (void)arq_imm_not_identifier(&opt);
                                if (arq_imm(ARQ_OP_EQ, &opt)) {
                                        cstr = arq_imm_default_cstr_t(&opt);
                                        if (arq_imm_cmd_is_dashdash(&cmd)) {
                                                cstr = arq_imm_argument_csrt_t(&cmd, &error_msg);
                                                if (cstr == NULL) {
                                                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                                        arq_msg_append_cstr(&error_msg, "'--' alone isn't enough if you want '--' as an argument then do -- --\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' allows you to set an argument that looks like an option -- --hello\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' undoes optional behavior in case of an cstr_t = NULL\n");
                                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                                        output_error_msg(&error_msg, arena_buffer);
                                                        return error_msg.size;
                                                }
                                        } else {
                                                /* For a short option with an optional cstr_t as an argument. */
                                                /* It is not always possible to include the argument immediately after the option. */
                                                /* This is the case whether the next character is a bundled option or a character from a cstr_t. */
                                                /* If the next character is a number, then it is an argument => here is it possible. */
                                                /* {'S', "cstring", fn_cstring, &ctx, "cstr_t = NULL"}, */
                                                /* failure: -abcShello    => the 'h' is interpreted as short option part of the bundle (no space) thats why failure */
                                                /* ok:      -abcS hello   => is string token fine */
                                                /* ok:      -abcS69       => 69 is a number fine can't be a short option */
                                                (void)arq_imm_optional_argument_cstr_t(&cmd, &cstr);
                                        }
                                        arq_push_cstr_t(queue, cstr);
                                } else if (arq_imm(ARQ_OP_ARRAY, &opt)) {
                                        struct {
                                                bool on;
                                                bool edge;
                                        } dashdash = {0};
                                        uint32_t *array_size = arq_push_array_size(queue, 0);
                                        log_inta(("u32 %u // init array_size", *array_size));
                                        while (1) {
                                                dashdash.on |= arq_imm_cmd_is_dashdash(&cmd);
                                                if (dashdash.on && !arq_imm_pick_cstr_t(&cmd, &cstr)) {
                                                        if (dashdash.edge) {
                                                                break;
                                                        }
                                                        arq_msg_append_cstr(&error_msg, CMD_LINE_FAILURE);
                                                        arq_msg_append_cstr(&error_msg, "'--' alone isn't enough if you want '--' as an argument then do -- --\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' allows to set an argument that looks like an option -- --hello\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' switch to positional arguments in case of an cstr_t array\n");
                                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                                        output_error_msg(&error_msg, arena_buffer);
                                                        return error_msg.size;
                                                }
                                                if (!dashdash.on && !arq_imm_optional_argument_cstr_t(&cmd, &cstr)) {
                                                        break;
                                                }
                                                dashdash.edge = dashdash.on;
                                                *array_size += 1;
                                                arq_push_cstr_t(queue, cstr);
                                                log_inta(("cstr %s", cstr));
                                        }
                                } else {
                                        /* A short option with a mandatory argument allows the argument to be included immediately after the option. */
                                        /* However, this short option must be the last option in a bundle of options. */
                                        /* {'S', "cstring", fn_cstring, &ctx, "cstr_t"}, */
                                        /* 'hello' is the argument */
                                        /* ok: -abcShello    => 'hello' is the argument */
                                        /* ok: -abcS hello   => 'hello' is the argument */
                                        /* ok: -abcS--hello  => '--hello' is the argument */
                                        /* ok: -abcS-hello   => '-hello' is the argument */
                                        cstr = arq_imm_argument_csrt_t(&cmd, &error_msg);
                                        if (cstr == NULL) {
                                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                                error_msg_append_option(&error_msg, &options[opt.idx]);
                                                output_error_msg(&error_msg, arena_buffer);
                                                return error_msg.size;
                                        }
                                        arq_push_cstr_t(queue, cstr);
                                }
                                if (arq_imm(ARQ_OP_COMMA, &opt)) continue;
                                goto terminator;
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                        } else if (arq_imm(ARQ_TYPE_UINT, &opt)) {
                                log_int_token_indent(ARQ_TYPE_UINT);
                                imm.DEFAULT_VALUE = arq_imm_default_uint;
                                imm.OPTIONAL_ARGUMENT = arq_imm_optional_argument_uint;
                                imm.PUSH = arq_push_uint;
                                imm.IS_LITERAL_TYPE = arq_imm_is_uint;
                                imm.ARGUMENT = arq_imm_argument_uint;

                        } else if (arq_imm(ARQ_TYPE_INT, &opt)) {
                                log_int_token_indent(ARQ_TYPE_INT);
                                imm.DEFAULT_VALUE = arq_imm_default_int;
                                imm.OPTIONAL_ARGUMENT = arq_imm_optional_argument_int;
                                imm.PUSH = arq_push_int;
                                imm.IS_LITERAL_TYPE = arq_imm_is_int;
                                imm.ARGUMENT = arq_imm_argument_int;

                        } else if (arq_imm(ARQ_TYPE_FLOAT, &opt)) {
                                log_int_token_indent(ARQ_TYPE_FLOAT);
                                imm.DEFAULT_VALUE = arq_imm_default_float;
                                imm.OPTIONAL_ARGUMENT = arq_imm_optional_argument_float;
                                imm.PUSH = arq_push_float;
                                imm.IS_LITERAL_TYPE = arq_imm_is_float;
                                imm.ARGUMENT = arq_imm_argument_float;

                        } else {
                                goto terminator;
                        }

                        (void)arq_imm_not_identifier(&opt);
                        if (arq_imm(ARQ_OP_EQ, &opt)) {
                                union_o x = imm.DEFAULT_VALUE(&opt);
                                /*uint_o num = arq_imm_default_uint(&opt);*/
                                if (imm.OPTIONAL_ARGUMENT(&cmd, &x, &error_msg)) {
                                        /* overflow */
                                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                        output_error_msg(&error_msg, arena_buffer);
                                        return error_msg.size;
                                }
                                imm.PUSH(queue, &x);
                        } else if (arq_imm(ARQ_OP_ARRAY, &opt)) {
                                uint32_t *array_size = arq_push_array_size(queue, 0);
                                log_inta(("u32 %u // init array_size", *array_size));
                                while (imm.IS_LITERAL_TYPE(&cmd)) {
                                        union_o x = {0};
                                        if (imm.OPTIONAL_ARGUMENT(&cmd, &x, &error_msg)) {
                                                /* overflow */
                                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                                error_msg_append_option(&error_msg, &options[opt.idx]);
                                                output_error_msg(&error_msg, arena_buffer);
                                                return error_msg.size;
                                        }
                                        *array_size += 1;
                                        imm.PUSH(queue, &x);
                                }
                        } else {
                                union_o x = {0};
                                arq_msg_clear(&error_msg);
                                x = imm.ARGUMENT(&cmd, &error_msg);
                                if (error_msg.size > 0) {
                                        /* wasn't an uint32_t number or overflow */
                                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                        output_error_msg(&error_msg, arena_buffer);
                                        return error_msg.size;
                                }
                                imm.PUSH(queue, &x);
                        }
                        if (arq_imm(ARQ_OP_COMMA, &opt)) continue;
terminator:
                        if (arq_imm(ARQ_OP_R_PARENTHESIS, &opt)) {
                                log_int_comment("call_back_function");
                                call_back_function(options, opt.idx, queue);
                                break;
                        }
                        assert(false);
                } /* while */
        } /* while */
        arena_buffer[0] = 0;
        return 0;
}

/*** End of inlined file: arq_main.c ***/

#endif

/*** Start of inlined file: license.h ***/
#if 1
/*
The MIT License (MIT)

Copyright (c) 2026 Bernhard Bertrand

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#endif
/*** End of inlined file: license.h ***/

/*** End of inlined file: arq.h ***/


#include <stdlib.h>
#include <string.h>

#define CGTEST_ARQ_ARENA_SIZE 1024

typedef struct {
    int help;
    int version;
    int license;
    char const *run_path;
    char const *init_path;
    int time;
} CGTestArqRaw;

static CGTestArqRaw *raw_m;

static void fn_help(Arq_Queue *queue)
{
    (void)queue;
    raw_m->help = 1;
}

static void fn_version(Arq_Queue *queue)
{
    (void)queue;
    raw_m->version = 1;
}

static void fn_license(Arq_Queue *queue)
{
    (void)queue;
    raw_m->license = 1;
}

static void fn_run(Arq_Queue *queue)
{
    raw_m->run_path = arq_cstr_t(queue);
}

static void fn_init(Arq_Queue *queue)
{
    raw_m->init_path = arq_cstr_t(queue);
    if (raw_m->init_path == NULL) {
        raw_m->init_path = ".";
    }
}

static void fn_time(Arq_Queue *queue)
{
    (void)queue;
    raw_m->time = 1;
}

static CGTestArgs cgtest_arq_fail(const char *message)
{
    CGTestArgs args;
    args.action = CGTEST_ARG_ERROR;
    args.run_path = NULL;
    args.init_path = NULL;
    args.time = 0;
    args.error = cmsg_dup(message, strlen(message));
    return args;
}

CGTestArgs cgtest_arq_parse(int argc, char **argv)
{
    CGTestArgs args;
    CGTestArqRaw raw;
    char arena[CGTEST_ARQ_ARENA_SIZE];
    int given;
    Arq_Option options[] = {
        { 'h', "help",    fn_help,    "()" },
        { 'v', "version", fn_version, "()" },
        { 'l', "license", fn_license, "()" },
        { 'r', "run",     fn_run,     "(cstr_t path)" },
        { 'i', "init",    fn_init,    "(cstr_t path = NULL)" },
        { 't', "time",    fn_time,    "()" }
    };

    raw.help = 0;
    raw.version = 0;
    raw.license = 0;
    raw.run_path = NULL;
    raw.init_path = NULL;
    raw.time = 0;
    raw_m = &raw;

    if (0 < arq_verify(arena, sizeof(arena), options, sizeof(options) / sizeof(options[0]))) {
        return cgtest_arq_fail(arena);
    }
    if (0 < arq_fn(argc, argv, arena, sizeof(arena), options, sizeof(options) / sizeof(options[0]))) {
        return cgtest_arq_fail(arena);
    }

    given = raw.help + raw.version + raw.license + (raw.run_path != NULL) + (raw.init_path != NULL);
    if (given == 0) {
        return cgtest_arq_fail("no action given; use one of -r, -i, -v, -l or -h");
    }
    if (given > 1) {
        return cgtest_arq_fail("-r, -i, -v, -l and -h cannot be combined");
    }

    args.error = NULL;
    args.run_path = raw.run_path;
    args.init_path = raw.init_path;
    args.time = raw.time;
    if (raw.help) {
        args.action = CGTEST_ARG_HELP;
    } else if (raw.version) {
        args.action = CGTEST_ARG_VERSION;
    } else if (raw.license) {
        args.action = CGTEST_ARG_LICENSE;
    } else if (raw.run_path != NULL) {
        args.action = CGTEST_ARG_RUN;
    } else {
        args.action = CGTEST_ARG_INIT;
    }

    if (raw.time && args.action != CGTEST_ARG_RUN) {
        return cgtest_arq_fail("-t/--time can only be combined with -r/--run");
    }

    return args;
}

void cgtest_arq_free(CGTestArgs *args)
{
    free(args->error);
    args->error = NULL;
}

/*** End of inlined file: cgtest_arq.c ***/


/*** Start of inlined file: cgtest_main.c ***/
#include <stdio.h>

#define CGTEST_VERSION "0.1.1"

static void print_help(void)
{
    printf("cgtest - a command-line C unit test DSL compiler\n");
    printf("\n");
    printf("  cgtest %-30s generate, compile and run cgtest-runner.c\n", "-r, --run <path>");
    printf("  cgtest %-30s also print a scan/generate/compile/run timing breakdown\n", "-r, --run <path> -t, --time");
    printf("  cgtest %-30s create cgtest-project.json, cgtest.h, and an example test inside <dir>/cgtest\n", "-i, --init <dir>");
    printf("  cgtest %-30s print the cgtest version\n", "-v, --version");
    printf("  cgtest %-30s print the cgtest license (MIT)\n", "-l, --license");
    printf("  cgtest %-30s print this message\n", "-h, --help");
    printf("\n");
    printf("https://github.com/bartgeier/cgtest\n");
}

/* Verbatim copy of this repo's own LICENSE file (MIT, covering cgtest
 * itself plus the vendored third_party/arq and third_party/jsmn) - kept
 * in sync by hand, not read from disk, since a downstream build (in
 * particular the single-file cgtest.c amalgamation - see
 * amalgamate_cgtest.c) has no LICENSE file sitting next to it at
 * runtime to read. One printf() per line, matching print_help()'s own
 * style, rather than one big string literal - keeps every individual
 * literal trivially under ISO C90's 509-char limit without needing
 * cgtest_create.c's multi-part template-splitting machinery for
 * something this short. */
static void print_license(void)
{
    printf("MIT License\n");
    printf("\n");
    printf("Copyright (c) 2026 Bernhard Bertrand (https://github.com/bartgeier/cgtest)\n");
    printf("Copyright (c) 2010 Serge A. Zaitsev (https://github.com/zserge/jsmn)\n");
    printf("Copyright (c) 2026 Bernhard Bertrand (https://github.com/bartgeier/arq)\n");
    printf("\n");
    printf("Permission is hereby granted, free of charge, to any person obtaining a copy\n");
    printf("of this software and associated documentation files (the \"Software\"), to deal\n");
    printf("in the Software without restriction, including without limitation the rights\n");
    printf("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
    printf("copies of the Software, and to permit persons to whom the Software is\n");
    printf("furnished to do so, subject to the following conditions:\n");
    printf("\n");
    printf("The above copyright notice and this permission notice shall be included in all\n");
    printf("copies or substantial portions of the Software.\n");
    printf("\n");
    printf("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
    printf("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
    printf("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
    printf("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
    printf("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
    printf("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
    printf("SOFTWARE.\n");
}

/* Printed after --run's own output, only when -t/--time was given (see
 * CGTestArgs::time) - regardless of whether the run itself succeeded,
 * since seeing where time went is useful on a compile failure too
 * (see CGTestRunResult's field comments for what "unreached phase" -
 * printed as 0.0 - means). */
static void print_timing(const CGTestRunResult *result)
{
    printf("scan: %.1fms  generate: %.1fms  compile: %.1fms  run: %.1fms  total: %.1fms\n",
           result->scan_ms, result->generate_ms, result->compile_ms, result->run_ms, result->total_ms);
}

int main(int argc, char **argv)
{
    CGTestArgs args = cgtest_arq_parse(argc, argv);
    int exit_code;

    switch (args.action) {
    case CGTEST_ARG_ERROR:
        fprintf(stderr, "cgtest: %s\n", args.error);
        exit_code = 1;
        break;

    case CGTEST_ARG_HELP:
        print_help();
        exit_code = 0;
        break;

    case CGTEST_ARG_VERSION:
        printf("cgtest %s\n", CGTEST_VERSION);
        exit_code = 0;
        break;

    case CGTEST_ARG_LICENSE:
        print_license();
        exit_code = 0;
        break;

    case CGTEST_ARG_INIT: {
        CGTestCreateResult result = cgtest_create_run(args.init_path);
        if (!result.ok) {
            fprintf(stderr, "cgtest: %s\n", result.error);
            exit_code = 1;
        } else {
            /* Each of the three files is reported individually - see
             * CGTestCreateResult::wrote_project/wrote_header/
             * wrote_test_macros (cgtest_create.h) - since cgtest_create_run()
             * leaves whichever ones already existed untouched instead of
             * always (re)writing all three. cgtest-project.json has two
             * further possible states beyond plain "left unchanged":
             * patched_project (already existing, but missing an optional
             * field a newer cgtest.exe added since it was written -
             * filled in with its default value) and
             * project_could_not_be_checked (already existing, but its
             * shape couldn't be understood well enough to even check for
             * a missing field - e.g. invalid JSON - so nothing was
             * touched; reported distinctly from "left unchanged" so this
             * isn't mistaken for "already up to date"). */
            printf("%s\n", result.dir);
            printf("  cgtest-project.json: %s\n",
                   result.wrote_project ? "created" :
                   result.patched_project ? "already exists, added missing field(s)" :
                   result.project_could_not_be_checked ? "already exists, left unchanged (could not parse it to check for missing fields)" :
                   "already exists, left unchanged");
            printf("  cgtest.h: %s\n", result.wrote_header ? "created" : "already exists, left unchanged");
            printf("  test_cgtest_macros.c: %s\n", result.wrote_test_macros ? "created" : "already exists, left unchanged");
            exit_code = 0;
        }
        cgtest_create_free(&result);
        break;
    }

    case CGTEST_ARG_RUN: {
        CGTestProject project = cgtest_project_load(args.run_path);
        if (!project.ok) {
            fprintf(stderr, "cgtest: %s\n", project.error);
            exit_code = 1;
        } else {
            CGTestRunResult result = cgtest_runner_run(&project);
            if (!result.ok) {
                fprintf(stderr, "cgtest: %s\n", result.error);
                exit_code = 1;
            } else {
                exit_code = result.exit_code;
            }
            if (args.time) {
                print_timing(&result);
            }
            cgtest_runner_free(&result);
        }
        cgtest_project_free(&project);
        break;
    }

    default:
        fprintf(stderr, "cgtest: internal error: unhandled action\n");
        exit_code = 1;
        break;
    }

    cgtest_arq_free(&args);
    return exit_code;
}

/*** End of inlined file: cgtest_main.c ***/

