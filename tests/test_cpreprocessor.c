/* test_cpreprocessor.c - unit tests for CPreprocessor's directive-line
 * state tracking (cpreprocessor_next_token()): recognizing when a
 * CTOK_HEADER_NAME should be requested from the lexer after "# include",
 * "# embed" (C23), "__has_include(...)" and "__has_embed(...)" (C23),
 * while leaving everything else identical to plain clexer_next_token().
 *
 * Same stand-in convention as test_ctestscanner.c: void test_<name>(void)
 * functions, manually invoked by main() below until cgtest.exe exists.
 */
#include "cpreprocessor.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int test_failed = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            test_failed = 1; \
            return; \
        } \
    } while (0)

/* ---- small helpers to keep test bodies readable -------------------- */

static bool next_is(CPreprocessor *pp, CTokenType type, const char *text)
{
    CToken tok = cpreprocessor_next_token(pp);
    if (tok.type != type) {
        fprintf(stderr, "  expected type %d, got %d ('%.*s')\n",
                (int)type, (int)tok.type, (int)tok.length, tok.start);
        return false;
    }
    if (text != NULL && !clexer_token_equals(&tok, text)) {
        fprintf(stderr, "  expected text '%s', got '%.*s'\n", text, (int)tok.length, tok.start);
        return false;
    }
    return true;
}

static bool next_is_eof(CPreprocessor *pp)
{
    CToken tok = cpreprocessor_next_token(pp);
    return tok.type == CTOK_EOF;
}

/* ---- tests ---------------------------------------------------------- */

void test_angle_include_becomes_header_name(void)
{
    CPreprocessor pp;
    cpreprocessor_init(&pp, "#include <stdio.h>\n", strlen("#include <stdio.h>\n"));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "include"));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<stdio.h>"));
    CHECK(next_is_eof(&pp));
}

void test_quoted_include_becomes_header_name(void)
{
    CPreprocessor pp;
    const char *src = "#include \"myheader.h\"\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "include"));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "\"myheader.h\""));
    CHECK(next_is_eof(&pp));
}

void test_embed_directive_becomes_header_name(void)
{
    /* C23 #embed also takes a header-name argument. */
    CPreprocessor pp;
    const char *src = "#embed <data.bin>\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "embed"));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<data.bin>"));
    CHECK(next_is_eof(&pp));
}

void test_has_include_in_if_directive(void)
{
    CPreprocessor pp;
    const char *src = "#if __has_include(<stdio.h>)\n#endif\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "if"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "__has_include"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<stdio.h>"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "endif"));
    CHECK(next_is_eof(&pp));
}

void test_has_embed_with_quoted_argument(void)
{
    CPreprocessor pp;
    const char *src = "#if __has_embed(\"art.png\")\n#endif\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "if"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "__has_embed"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "\"art.png\""));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
}

void test_has_include_with_quoted_argument(void)
{
    CPreprocessor pp;
    const char *src = "#if __has_include(\"myheader.h\")\n#endif\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "if"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "__has_include"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "\"myheader.h\""));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "endif"));
    CHECK(next_is_eof(&pp));
}

void test_has_embed_with_angle_argument(void)
{
    CPreprocessor pp;
    const char *src = "#if __has_embed(<data.bin>)\n#endif\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "if"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "__has_embed"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<data.bin>"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
}

void test_two_has_include_on_same_line(void)
{
    /* Both occurrences must independently trigger header-name mode. */
    CPreprocessor pp;
    const char *src = "#if __has_include(<a.h>) && __has_include(<b.h>)\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "if"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "__has_include"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<a.h>"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
    CHECK(next_is(&pp, CTOK_PUNCT, "&&"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "__has_include"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<b.h>"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
}

void test_other_directive_does_not_trigger_header_name(void)
{
    /* "#define" is neither include nor embed - '<' after it must stay an
     * ordinary less-than punctuator, never a header-name. */
    CPreprocessor pp;
    const char *src = "#define LT(a, b) ((a) < (b))\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "define"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "LT"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "a"));
    CHECK(next_is(&pp, CTOK_PUNCT, ","));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "b"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "a"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
    CHECK(next_is(&pp, CTOK_PUNCT, "<"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "b"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
}

void test_ordinary_comparisons_are_unaffected(void)
{
    /* No directive at all: '<' and '>' must behave as plain operators. */
    CPreprocessor pp;
    const char *src = "int a; if (a < b > c) { }\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_IDENTIFIER, "int"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "a"));
    CHECK(next_is(&pp, CTOK_PUNCT, ";"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "if"));
    CHECK(next_is(&pp, CTOK_PUNCT, "("));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "a"));
    CHECK(next_is(&pp, CTOK_PUNCT, "<"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "b"));
    CHECK(next_is(&pp, CTOK_PUNCT, ">"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "c"));
    CHECK(next_is(&pp, CTOK_PUNCT, ")"));
    CHECK(next_is(&pp, CTOK_PUNCT, "{"));
    CHECK(next_is(&pp, CTOK_PUNCT, "}"));
    CHECK(next_is_eof(&pp));
}

void test_has_include_identifier_without_paren_is_not_misdetected(void)
{
    /* __has_include used as a plain identifier (not actually called)
     * must not corrupt tokenization of what follows it. */
    CPreprocessor pp;
    const char *src = "__has_include + 1;\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_IDENTIFIER, "__has_include"));
    CHECK(next_is(&pp, CTOK_PUNCT, "+"));
    CHECK(next_is(&pp, CTOK_PP_NUMBER, "1"));
    CHECK(next_is(&pp, CTOK_PUNCT, ";"));
    CHECK(next_is_eof(&pp));
}

void test_macro_named_include_falls_back_to_identifier(void)
{
    /* "#include FOO_HEADER" (macro-based header, common in real code):
     * there is no literal '<' or '"' right after "include", so it must
     * fall back to an ordinary identifier token, not crash or hang. */
    CPreprocessor pp;
    const char *src = "#include FOO_HEADER\nint x;\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "include"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "FOO_HEADER"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "int"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "x"));
    CHECK(next_is(&pp, CTOK_PUNCT, ";"));
    CHECK(next_is_eof(&pp));
}

void test_include_with_nothing_after_does_not_leak_into_next_line(void)
{
    /* Malformed: "#include" with no header-name before the line ends.
     * The '<...>' on the FOLLOWING line must not be swallowed as if it
     * belonged to this directive - it's unrelated content one line down. */
    CPreprocessor pp;
    const char *src = "#include\n<stdio.h>\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "include"));
    /* Must NOT be CTOK_HEADER_NAME: it's on the next line. */
    CHECK(next_is(&pp, CTOK_PUNCT, "<"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "stdio"));
    CHECK(next_is(&pp, CTOK_PUNCT, "."));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "h"));
    CHECK(next_is(&pp, CTOK_PUNCT, ">"));
    CHECK(next_is_eof(&pp));
}

void test_consecutive_include_directives_both_recognized(void)
{
    /* Regression: after the first directive's header-name state resets,
     * the SECOND '#include' must still be tracked correctly - a naive
     * implementation that only classifies tokens on the "normal" path
     * can lose track of a '#' returned via the header-name fallback. */
    CPreprocessor pp;
    const char *src = "#include <a.h>\n#include <b.h>\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "include"));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<a.h>"));
    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "include"));
    CHECK(next_is(&pp, CTOK_HEADER_NAME, "<b.h>"));
    CHECK(next_is_eof(&pp));
}

void test_hash_at_line_start_required(void)
{
    /* A '#' that is NOT the first token on its line is just the ordinary
     * punctuator (e.g. stringizing inside a macro body); it must not be
     * mistaken for the start of a directive. */
    CPreprocessor pp;
    const char *src = "a # include <x>\n";
    cpreprocessor_init(&pp, src, strlen(src));

    CHECK(next_is(&pp, CTOK_IDENTIFIER, "a"));
    CHECK(next_is(&pp, CTOK_PUNCT, "#"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "include"));
    CHECK(next_is(&pp, CTOK_PUNCT, "<"));
    CHECK(next_is(&pp, CTOK_IDENTIFIER, "x"));
    CHECK(next_is(&pp, CTOK_PUNCT, ">"));
    CHECK(next_is_eof(&pp));
}

/* --- stand-in test runner: cgtest-runner does not exist yet -------- */

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_angle_include_becomes_header_name", test_angle_include_becomes_header_name },
        { "test_quoted_include_becomes_header_name", test_quoted_include_becomes_header_name },
        { "test_embed_directive_becomes_header_name", test_embed_directive_becomes_header_name },
        { "test_has_include_in_if_directive", test_has_include_in_if_directive },
        { "test_has_embed_with_quoted_argument", test_has_embed_with_quoted_argument },
        { "test_has_include_with_quoted_argument", test_has_include_with_quoted_argument },
        { "test_has_embed_with_angle_argument", test_has_embed_with_angle_argument },
        { "test_two_has_include_on_same_line", test_two_has_include_on_same_line },
        { "test_other_directive_does_not_trigger_header_name", test_other_directive_does_not_trigger_header_name },
        { "test_ordinary_comparisons_are_unaffected", test_ordinary_comparisons_are_unaffected },
        { "test_has_include_identifier_without_paren_is_not_misdetected", test_has_include_identifier_without_paren_is_not_misdetected },
        { "test_macro_named_include_falls_back_to_identifier", test_macro_named_include_falls_back_to_identifier },
        { "test_include_with_nothing_after_does_not_leak_into_next_line", test_include_with_nothing_after_does_not_leak_into_next_line },
        { "test_consecutive_include_directives_both_recognized", test_consecutive_include_directives_both_recognized },
        { "test_hash_at_line_start_required", test_hash_at_line_start_required }
    };
    size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t i;
    size_t failed = 0;

    for (i = 0; i < count; i++) {
        test_failed = 0;
        cases[i].fn();
        printf("[%s] %s\n", test_failed ? "FAIL" : "PASS", cases[i].name);
        if (test_failed) {
            failed++;
        }
    }

    printf("\n%lu/%lu passed\n", (unsigned long)(count - failed), (unsigned long)count);
    return failed == 0 ? 0 : 1;
}
