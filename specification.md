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

```cgtest.exe --init ./unitest```  
Creates a default template cgtest-project.json inside ./unitest/cgtest (creating both directories
if they don't exist yet). The three files always go into a "cgtest" child of the given directory,
never the directory itself, so a project's own test files can `#include "cgtest/cgtest.h"` - the
same gtest/gtest.h-style layout GoogleTest users already know - instead of a bare cgtest.h
competing with the project's own headers at its root.  
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
    path). The three files are written into a "cgtest" child of that directory, not the
    directory itself (e.g. `--init ./unitest` writes into `./unitest/cgtest`), so a project's
    own test files can `#include "cgtest/cgtest.h"`; both directories are created if they
    don't exist yet.
    Each of the three files is checked and written independently: a missing one is created from
    the template baked into this cgtest.exe; an already-existing one is left completely
    untouched, never overwritten, whether it's an unmodified older version or something the
    developer edited by hand - so `--init` is safe (and idempotent) to re-run on an
    already-initialized directory, never an error just because e.g. cgtest-project.json is
    already there. In particular, deleting only cgtest.h (it never carries per-project
    customization the way cgtest-project.json's compiler_command/include_paths/etc. do - so
    there's nothing project-specific to lose) and re-running `--init` regenerates just that
    file, picking up a fix from a newer cgtest.exe without disturbing cgtest-project.json or
    test_cgtest_macros.c.
  * -t --time a modifier, not an action of its own: combined with -r/--run, prints a
    scan/generate/compile/run wall-clock timing breakdown after the normal output
    (see ctimer.h for the portable timer and CGTestRunResult for the measured
    fields). Given alone, or combined with -i/-v/-h, it is an error - it has nothing
    to modify otherwise.
    ```cgtest.exe --run ./unitest/cgtest --time```
  * -v --version of cgtest
  * -h --help of cgtest

### cgtest-project.json
  * set compiler command "gcc -std=c89 -O0 -Wall -Wextra -pedantic-errors" 
  * Include path list
  * Source file list
  * Output path to generate cgtest-runner.c and cgtest-runner.exe
  * Directory list where to find the test_*.c files.  
  * `msvc` (optional, defaults to `false`): switches the flags cgtest appends to
    `compiler_command` from GCC/Clang style (`-I"path"`, `-o "path"`) to MSVC
    `cl.exe` style (`/I"path"`, `/Fe:"path"`) - `compiler_command` alone can't
    express this, since `cl.exe` doesn't accept `-I`/`-o` at all.
  * `single_translation_unit` (optional, defaults to `false`): compiles every
    discovered test file into cgtest-runner.c's own translation unit (via
    `#include`) instead of passing each as its own separate source argument -
    see ch.6 "Single-translation-unit mode" for the mechanism and its tradeoff.

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
* cgtest_create.c cgtest_create.h implements -i/--init: writes a template cgtest-project.json, cgtest.h, and test_cgtest_macros.c into a "cgtest" child of a directory.
  - cgtest.h only `extern`-declares its few shared helpers (`cgtest_relpath()`, `cgtest_print_str_field()`, `cgtest_strcasecmp()` - see ch.4's macro descriptions above), the same
    `extern int cgtest_failed;` pattern used for the runner's own pass/fail flags - never `static`
    definitions copied into the header itself. A `static` definition there would give every
    `#include`'ing test_\*.c file its own private copy (its own translation unit in separate-TU
    mode - ch.6/cgtest_runner.h), one `-Wunused-function` flags in any file that doesn't happen to
    call the specific macro relying on it, even though some other file does.
* cgtest_runner.c cgtest_runner.h implements -r/--run: generates cgtest-runner.c, compiles it, and executes it.
  - cgtest-runner.c is where cgtest.h's `extern`-declared helpers above are actually defined -
    unconditionally, once, with external (non-`static`) linkage, satisfied by the linker from
    every file that calls them regardless of `single_translation_unit`.
* clexer.c clexer.h a C23 lexer/tokenizer.
* cpreprocessor.c cpreprocessor.h directive-aware layer on top of clexer.c/h (recognizes #include/#embed/__has_include/__has_embed enough to disambiguate header-name tokens).
* ctestscanner.c ctestscanner.h for scanning and listing the test_function within the test_files, built on top of cpreprocessor.c/h.
* ctestfiles.c ctestfiles.h finds test_*.c files within a directory.
* cpath.c cpath.h lexical path joining and normalization.
* cpathlist.c cpathlist.h a growable list of resolved absolute paths, built on top of cpath.c/h.
* cmsg.c cmsg.h helper for building bounded, truncation-safe human-readable error messages.
* ctimer.c ctimer.h portable wall-clock elapsed-time measurement, used by -r/--run's -t/--time
  phase breakdown.
* cgtest_main.c main function

- Colorized terminal output

---

## 5. CLI Interface

```bash
cgtest.exe --run ./unitest/cgtest/cgtest-project.json
cgtest.exe --run ./unitest/cgtest
cgtest.exe --run ./unitest/cgtest --time
cgtest.exe --init ./unitest
cgtest.exe --version
cgtest.exe --help
```

---

## 6. Fixtures

Implemented in ctestscanner.c/h and cgtest_runner.c/h; see
examples/mathlib/tests/test_math_fixture.c for a working example. One guiding
constraint shaped the design: a core reason to use cgtest over GoogleTest is faster
compilation and execution, so the fixture design stays fully static - no vtables/virtual
dispatch, no per-test polymorphic instance construction the way GoogleTest's class-based
`TEST_F` fixtures work, and no allocation cgtest itself imposes (only the author's own
`setup_<name>` does, if it needs to). Everything below is plain C structs and functions
the compiler resolves at compile time.

### Shape

A test function opts into a fixture by taking one pointer parameter instead of `(void)`:

```c
void test_bar(State *state) { ... }
```

Two more functions, name-derived from the test function (`test_<name>` ->
`setup_<name>`/`teardown_<name>`), initialize and clean up that fixture:

```c
typedef struct State {
    int value;
} State;

void setup_bar(State **state) { ... }
void teardown_bar(State *state) { ... }
```

`State`'s struct tag must match its typedef name (`typedef struct State { ... } State;`,
not the tag-less `typedef struct { ... } State;`) - see "Generated code" below for why.

Both `setup_<name>` and `teardown_<name>` return `void` - `setup_<name>` reports a
failure the same way any other test-adjacent code does, by calling `EXPECT_*`/`ASSERT_*`
(see "Generated code" below for how a fatal one affects `test_bar`, and why `setup_<name>`
takes a `State **` out-param rather than returning `State *`).

`setup_<name>` is mandatory: `*state` is passed to `test_<name>` uninitialized otherwise.
`teardown_<name>` is optional: a fixture with nothing to release can simply not define
one, rather than being required to write a no-op function - GoogleTest treats
`SetUp()`/`TearDown()` symmetrically (both optional, since both are virtual methods with
empty default bodies), but cgtest's lack of that mechanism (no vtables - see the
constraint above) makes `setup_<name>` a deliberate exception, not a symmetric pair.

`setup_<name>` owns allocating `*state` (typically via `calloc`/`malloc` - see "Generated
code" below for why cgtest can't stack-allocate it directly the way earlier designs did).
Neither cgtest nor `teardown_<name>` is required to free it: the runner process exits
shortly after the last test regardless, so an unfreed `*state` is reclaimed by the OS
either way. `teardown_<name>` freeing it anyway is the recommended pattern when prompt
cleanup matters (e.g. a fixture holding a real OS resource, not just memory) - see
examples/mathlib/tests/test_cgtest_macros.c's `teardown_counter` for the pattern.

### Per-test, not per-file

Setup/teardown wrap *each* call to `test_bar`, not the whole file, matching GoogleTest's
per-test semantics (fresh state, no mutation leaking between tests) rather than cgtest's
existing "first/last function in a file as manual setup/teardown" convention (see
chapter 4), which continues to exist unchanged as a separate, simpler idiom for tests
that don't need a fixture.

### Generated code

Every discovered test_*.c file compiles as its own translation unit and is passed to the
compiler as its own source argument (see cgtest_runner_build_compile_command()) - the
generated cgtest-runner.c never `#include`s one. This is why: the original design
`#include`d every discovered file directly into cgtest-runner.c so it shared one
translation unit with the generated `main()`, simple to generate but meaning two test
files defining a same-named `static` helper (not just a `test_` function) would collide -
a real, recurring annoyance once fixtures made cross-file naming conflicts more likely.
Compiling each file separately and declaring only what cgtest-runner.c actually calls as
`extern` fixes that: each file keeps its own translation unit's scope, the normal C rule.

```c
typedef struct State State;                  /* one per distinct fixture type - see below */

extern void test_foo(void);                  /* plain (void) test */

extern void setup_bar(State **state);        /* fixture test */
extern void test_bar(State *state);
extern void teardown_bar(State *state);       /* only if teardown_bar was found - see below */
```

A fixture test is then called wrapped in its own block instead of a bare `test_bar();`:

```c
{
    State *state = NULL;
    setup_bar(&state);
    if (!cgtest_fatal_failed) {
        test_bar(state);
    }
    teardown_bar(state);   /* only if teardown_bar was found - see below */
}
```

`State` is forward-declared as an *incomplete* type (`typedef struct State State;`,
deduplicated - the same name forward-declared twice is a hard error under strict C89
`-pedantic-errors`, since that redundant-typedef allowance is C11-only) rather than
`#include`d or copied in full. cgtest-runner.c only ever holds or passes a `State *`/
`State **` - it never allocates or dereferences a `State` by value - so it never needs the
real definition. This only produces a well-defined, standards-compliant type across the
two separately-compiled translation units (not just two same-spelled opaque types that
happen to work by ABI accident) because `State`'s struct tag matches its typedef name
(see "Shape" above) - C treats an incomplete tagged type in one translation unit and its
later completion in another as the same type when the tags match.

`setup_bar` takes `State **` (an out-param) rather than returning `State *` specifically
so it stays `void`-returning - `EXPECT_*`/`ASSERT_*` work inside it exactly like they do
in `test_bar`, including `ASSERT_*`'s early `return;`, which wouldn't type-check in a
function declared to return `State *`. `state` starts `NULL` and is populated by
`setup_bar` itself; if `setup_bar` hits a fatal (`ASSERT_*`) failure before assigning it,
`state` stays the well-defined `NULL` it started as - safe to skip in the `test_bar` call
above and safe to pass to a present `teardown_bar`.

`cgtest_fatal_failed` is a second flag alongside `cgtest_failed`, set only by `ASSERT_*`
(never `EXPECT_*`) and reset to 0 by the runner right before `setup_bar` runs, same as
`cgtest_failed`. A fatal `setup_bar` failure means `test_bar` is skipped rather than run -
matching GoogleTest's own `SetUp()`/`TestBody()` behavior, where a fatal `SetUp()` failure
skips `TestBody()` but a non-fatal `EXPECT_*` one does not. A present `teardown_bar` still
always runs regardless of that check; when none was found (see "Validation before
invoking the compiler" below), the call is omitted entirely rather than emitted against a
function that doesn't exist.

### Single-translation-unit mode

`single_translation_unit` (cgtest-project.json, optional, defaults to `false` - see
"cgtest-project.json" above) toggles how the code behind the `extern` declarations above
actually reaches cgtest-runner.c, without changing the fixture technique itself (the same
opaque `typedef struct State State;` forward-declare, the same `extern`-declared
`setup_bar`/`test_bar`/`teardown_bar`, the same developer-owns-`malloc`/`free`
`State`) - both modes generate that part of cgtest-runner.c identically.

* `false` (default, "separate TU"): as described above - every discovered test file
  compiles as its own translation unit and is passed to the compiler as its own source
  argument (see `cgtest_runner_build_compile_command()`); the `extern` declarations are
  satisfied by the linker.
* `true` ("single TU"): `cgtest_runner_generate_source()` instead appends one
  `#include "<absolute path>"` line per discovered test file right after the `extern`
  declarations and before `main()`, and `cgtest_runner_build_compile_command()` passes
  only cgtest-runner.c to the compiler - no test file is ever passed as its own source
  argument in this mode. The `extern` declarations ahead of the `#include`s are harmless
  (an ordinary declaration-before-definition, legal C), and since the `#include`d text
  itself completes `State`'s forward declaration by the time `main()` runs, nothing about
  the fixture mechanism needs to change for this mode either.

The motivation is compile speed: a fixed per-translation-unit optimizer cost is paid once
per discovered test file in separate-TU mode, but only once total in single-TU mode - at
`-O2`/`-O3` (see "cgtest-project.json"'s `compiler_command`), where that cost dominates for
many small test files, this can make single-TU mode substantially faster to compile while
still exercising the optimizer for real (unlike dropping to `-O0`, which avoids the cost by
skipping optimization entirely instead of amortizing it).

The cost: single-TU mode reintroduces exactly what separate-TU mode's own design (see
"Generated code" above) was built to avoid - two test files defining the same-named
`static` helper or global now share one translation unit again, and the compiler rejects
it as an ordinary redefinition error rather than the two files' `static` symbols simply
staying distinct the way separate compilation guarantees. cgtest makes no attempt to
detect or pre-empt this (no pre-flight duplicate-symbol scan) - it ships as a known,
accepted tradeoff of opting into the flag, revisited only if it turns out to be a real
problem in practice.

The duplicate-basename-across-directories check (see "Validation before invoking the
compiler" below) is unconditional across both modes even though its original motivation -
MSVC's `cl.exe` naming each source file's object file after its own basename, so two
same-named files from different directories passed as separate source arguments would
silently collide - only actually applies in separate-TU mode (single-TU mode never passes
a test file as its own source argument to begin with). It stays one rule regardless of
mode rather than one whose applicability depends on it, trading a small amount of
single-TU-mode strictness for not needing to explain a mode-dependent exception.

### Explicitly rejected: returning the fixture from setup_<name>

`State *setup_bar(void)` (returning the fixture instead of the `State **` out-param
above) was considered and rejected. It reads more naturally, but breaks `ASSERT_*`'s
early `return;` inside `setup_bar` - `return;` (no value) doesn't type-check in a function
declared to return `State *`. Making that work would mean a distinct assertion macro
family just for setup functions (returning `NULL` on failure instead of a bare `return;`),
adding real API surface and a special case ("`ASSERT_*` works everywhere except inside
`setup_<name>`") for a stylistic win. The out-param keeps `setup_<name>` `void`-returning,
so every macro behaves identically in `test_`/`setup_`/`teardown_` with no exception to
remember.

### What ctestscanner.h needed

`ctestscanner_find()` matched only the literal pattern `void test_<name>(void) {` (see
`CTestFunction` in ctestscanner.h) before this chapter was implemented. It now also
recognizes the one-parameter form and captures the parameter's type text (e.g. `State`)
verbatim into `CTestFunction::fixture_type` (`NULL` for the plain `(void)` form), used to
emit the forward declaration and `State *state = NULL;` local above - no other
understanding of the type is required, since the C compiler enforces everything else
(including whether its struct tag actually matches its typedef name, as "Generated code"
above requires).

### Validation before invoking the compiler

Before compiling, cgtest checks that `setup_<name>` exists for every test that takes a
fixture, and fails with a clear cgtest-level message (like the existing
duplicate-basename-across-directories check in cgtest_runner.c) rather than surfacing a
raw linker error. This check is existence-only - it does *not* compare `setup_<name>`'s
declared parameter type against `test_<name>`'s; a type mismatch (e.g. a typo'd type
name) is left entirely to the C compiler's own type checking, which already reports it
better than a bespoke cgtest-side comparison would.

`teardown_<name>` is checked for existence the same way, but a missing one is not an
error - it's recorded (`CTestFunction::has_teardown`) so "Generated code" above can omit
the call entirely instead.

### Explicitly rejected: multiple fixtures per test

`test_more(State0 *a, State1 *b)` was considered and rejected. Supporting it isn't a
small extension - fixtures shared across many tests naturally want to be keyed by type
or parameter name rather than by test name, which is a different naming convention, not
a generalization of the one above, and scanning would need to parse an arbitrary
parameter list instead of "0 or 1 params". The author gets the same result today by
aggregating manually:

```c
typedef struct MoreState { State0 s0; State1 s1; } MoreState;
```

with one `setup_more`/`teardown_more` initializing both members. cgtest caps fixtures at
exactly one per test and points to composition for anything more.
