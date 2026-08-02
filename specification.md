# Project Name cgtest

## 1. Overview
cgtest is a command-line C unit test DSL compiler application.
It compiles and runs a test runner from test files.  
**cgtest.exe:**  
* generates cgtest-runner.c with the info from cgtest-project.json
* compiles cgtest-runner.c and 
* executes the compiled binary.  

### Examples
```cgtest.exe --run ./unitest/cgtest/cgtest-project.json```  
Generates testrunner and excecute it.

```cgtest.exe --init ./unitest/cgtest```  
Creates a default template cgtest-project.json inside ./unitest/cgtest (creating the directory if it doesn't exist yet)  
Creates the cgtest.h file alongside it, it contains macros for unit tests.  
Also creates test_cgtest_macros.c alongside both - one example test function per macro from
cgtest.h. The template project file's `test_directories` already includes `.`, so
`cgtest.exe --run ./unitest/cgtest` discovers and runs that example immediately - a
freshly created project has something that actually passes out of the box, not just files
to edit.

---

## 2. Goals

* Must be written in C (C89 standard)
* Must compile with gcc or clang
* Must run on Windows/Linux
* Must not use external libraries (unless specified)

---

## 3. Non-Goals (Important)
What the project should NOT do.

- No GUI
- No network features
- No external database systems

---

## 4. Features of cgtest.exe

### Pars command-line arguments
  * For parsing the command-line arguments cgtest uses the arq lib from
  https://github.com/bartgeier/arq
  * -r --run path to the cgtest-project.json, or to the directory containing it
    (if the path is a directory, "cgtest-project.json" is looked up inside it).
    If cgtest-project.json doesn't exist than error and exit cgtest.exe with an appropriate message.  
    ```cgtest.exe --run ./unitest/cgtest/cgtest-project.json```
    ```cgtest.exe --run ./unitest/cgtest```
  * -i --init path to the directory cgtest-project.json, cgtest.h, and test_cgtest_macros.c
    should be created in (the argument is always a directory, never the project file's own
    path; the directory is created if it doesn't exist yet).
    If cgtest-project.json already exist in that directory than error and exit cgtest.exe with an appropriate message.
  * -v --version of cgtest
  * -h --help of cgtest

### cgtest-project.json
  * set compiler command "gcc -std=c99 -O3" 
  * Include path list
  * Source file list
  * Output path to generate cgtest-runner.c and cgtest-runner.exe
  * Directory list where to find the test_*.c files.  
  * `msvc` (optional, defaults to `false`): switches the flags cgtest appends to
    `compiler_command` from GCC/Clang style (`-I"path"`, `-o "path"`) to MSVC
    `cl.exe` style (`/I"path"`, `/Fe:"path"`) - `compiler_command` alone can't
    express this, since `cl.exe` doesn't accept `-I`/`-o` at all.

Use for json parser single header jsmn.h in c https://github.com/zserge/jsmn
* Search test directorys for files their nameing starts with test_...  
  - test_math.c
  - test_strview.c
* This files are than scanned for functions their nameing starts also with test_... with following signature:
  - void test_(void)
  - void test_math_add(void) {....}
  - void test_goal_sub(void) {....}
  - void test_div(void) {....}
  - A `void` return keeps test files C89-portable (no `<stdbool.h>` needed). Failures are
    reported via macros from the generated cgtest.h, in the GoogleTest style:
    `EXPECT_TRUE(cond)`/`EXPECT_FALSE(cond)` log a failure and let the test function keep
    running; `ASSERT_TRUE(cond)`/`ASSERT_FALSE(cond)` log a failure and return from the test
    function immediately (for a precondition the rest of the function depends on). All four
    set the `cgtest_failed` flag; the runner resets that flag before calling each test and
    reads it back afterwards to decide pass/fail.
  - `EXPECT_EQ_INT`/`EXPECT_EQ_UINT`/`EXPECT_EQ_FLOAT`/`EXPECT_EQ_DOUBLE`/`EXPECT_EQ_PTR`/
    `EXPECT_EQ_STR`/`EXPECT_EQ_STR_NOCASE` and their `EXPECT_NE_` counterparts (and all of
    their `ASSERT_` equivalents, same EXPECT-vs-ASSERT split as above) compare two values and
    print both on failure - unlike `EXPECT_TRUE`, which only shows the source text of the
    whole condition. Each casts both operands to one canonical type per family (`long`/
    `unsigned long`/`float`/`double`/`const void *`) rather than needing one macro per exact C
    type. `EXPECT_EQ_STR`/`ASSERT_EQ_STR` compare via `strcmp()`, not pointer equality - same
    reason GoogleTest has `EXPECT_STREQ` separate from `EXPECT_EQ`. `EXPECT_EQ_STR_NOCASE`/
    `ASSERT_EQ_STR_NOCASE` are the case-insensitive counterparts (GoogleTest's
    `EXPECT_STRCASEEQ`), compared via a small portable `cgtest_strcasecmp()` helper rather
    than the non-standard `strcasecmp()`/`_stricmp()`. The `NE_` family (GoogleTest's
    `EXPECT_NE`/`EXPECT_STRNE`/`EXPECT_STRCASENE`) asserts the two values differ instead, and
    takes its first argument name as `unexpected` rather than `expected`, since there's no
    "expected" value when the check is "must not equal". Per type family, `EQ_`/`NE_` and
    `EXPECT_`/`ASSERT_` all share one internal comparison macro (`CGTEST_CMP_<TYPE>_` for
    INT/UINT/PTR/STR/STR_NOCASE), parameterized on the operator that decides failure (`!=` for
    `EQ_`, `==` for `NE_`), the printed label/format, and whether to `return` afterwards - only
    the label text and operator differ between the four generated macros of a family.
  - `EXPECT_EQ_FLOAT`/`EXPECT_EQ_DOUBLE` (and their `ASSERT_`/`NE_` counterparts) don't compare
    exactly - unlike the other `EQ_` macros, exact equality would fail for nearly every real
    floating-point computation, even ones that are correct for all practical purposes. Instead
    they use `CGTEST_APPROX_FLOAT_`/`CGTEST_APPROX_DOUBLE_`, an internal macro (same
    parameterization scheme as `CGTEST_CMP_<TYPE>_` above) checking `|expected - actual| <= 4
    * EPSILON * max(1.0, |expected|, |actual|)` (`FLT_EPSILON`/`DBL_EPSILON` from
    `<float.h>`) - similar in spirit to GoogleTest's ULP-based `EXPECT_FLOAT_EQ`/
    `EXPECT_DOUBLE_EQ`, but using an epsilon-relative diff instead of literal bit-distance, so
    it needs no 64-bit integer type for the `double` case and stays portable to plain C89. The
    `max(1.0, ...)` floor keeps the tolerance from collapsing to near-zero once both values are
    close to 0. On failure, the message shows the actual difference alongside the tolerance
    that was exceeded (or, for `NE_`, not exceeded). No `<math.h>`/`fabs()` dependency, same
    reason `cgtest_strcasecmp()` avoids `strcasecmp()`. `EXPECT_NE_FLOAT`/`EXPECT_NE_DOUBLE`
    aren't something GoogleTest itself provides (it has no "confidently different floats"
    macro), but are included for consistency with every other type family here having a
    matching `NE_`.
  - `EXPECT_NEAR_DOUBLE(expected, actual, abs_error)`/`ASSERT_NEAR_DOUBLE(...)` (GoogleTest's
    `EXPECT_NEAR`) check `expected` and `actual` are within a caller-supplied `abs_error` of
    each other, for when `EXPECT_EQ_DOUBLE`'s built-in `4 * EPSILON` tolerance isn't the right
    fit - e.g. a computation whose error bound is known to be much looser or tighter than that.
    On failure it prints the actual difference alongside the max allowed one, not just the two
    operands. No `<math.h>`/`fabs()` dependency - the absolute difference is computed with a
    plain sign flip. There's no `NE_` form of this one - "must differ by more than a
    caller-supplied X" isn't a common enough need to justify it.
  - `EXPECT_LT_INT`/`EXPECT_LE_INT`/`EXPECT_GT_INT`/`EXPECT_GE_INT` (and the same four for
    `UINT`/`FLOAT`/`DOUBLE`, plus all of their `ASSERT_` equivalents - GoogleTest's
    `EXPECT_LT`/`EXPECT_LE`/`EXPECT_GT`/`EXPECT_GE`) are ordering comparisons: `<`/`<=`/`>`/
    `>=`. Unlike `EQ_`/`NE_` on `FLOAT`/`DOUBLE`, these don't need an epsilon tolerance -
    ordering is exact and well-defined with no "rounding noise" to absorb the way equality has
    - so `INT`/`UINT` reuse the plain-operator `CGTEST_CMP_INT_`/`CGTEST_CMP_UINT_` cores
    already used by their `EQ_`/`NE_` macros, and `FLOAT`/`DOUBLE` get their own
    plain-operator `CGTEST_CMP_FLOAT_`/`CGTEST_CMP_DOUBLE_` cores (distinct from
    `CGTEST_APPROX_FLOAT_`/`CGTEST_APPROX_DOUBLE_`, which only `EQ_`/`NE_` use). No `PTR_` or
    `STR_` ordering family - ordering two arbitrary pointers with `<`/`>` is only
    well-defined in C if they point into the same array, and GoogleTest itself has no
    `EXPECT_STRLT` either. Arguments are named `val1`/`val2` rather than `expected`/`actual`,
    since there's no "expected" value for e.g. "must be less than".
* the cgtest-runner executes those function in the same order they appear in the test_file.
  - That allowes to use the first function in a file as init test setup.
  - That allowes to use the last function in a file as tear down setup.

### Source Modules file
* cgtest_arq.c cgtest_arq.h command line parser use arq lib from https://github.com/bartgeier/arq
* cgtest_project.c cgtest_project.h use for json parsing jsmn https://github.com/zserge/jsmn
  parses cgtest-project.json
* cgtest_create.c cgtest_create.h implements -i/--init: writes a template cgtest-project.json, cgtest.h, and test_cgtest_macros.c into a directory.
* cgtest_runner.c cgtest_runner.h implements -r/--run: generates cgtest-runner.c, compiles it, and executes it.
* clexer.c clexer.h a C23 lexer/tokenizer.
* cpreprocessor.c cpreprocessor.h directive-aware layer on top of clexer.c/h (recognizes #include/#embed/__has_include/__has_embed enough to disambiguate header-name tokens).
* ctestscanner.c ctestscanner.h for scanning and listing the test_function within the test_files, built on top of cpreprocessor.c/h.
* ctestfiles.c ctestfiles.h finds test_*.c files within a directory.
* cpath.c cpath.h lexical path joining and normalization.
* cpathlist.c cpathlist.h a growable list of resolved absolute paths, built on top of cpath.c/h.
* cmsg.c cmsg.h helper for building bounded, truncation-safe human-readable error messages.
* cgtest_main.c main function

- Colorized terminal output

---

## 5. CLI Interface

```bash
cgtest.exe --run ./unitest/cgtest/cgtest-project.json
cgtest.exe --run ./unitest/cgtest
cgtest.exe --init ./unitest/cgtest
cgtest.exe --version
cgtest.exe --help
```
