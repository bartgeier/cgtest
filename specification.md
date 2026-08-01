# Project Name cgtest

## 1. Overview
cgtest is a command-line C unit test DSL compiler application.
It compiles and runs a test runner from test files.  
**cgtest.exe:**  
* generates cgtest-runner.c with the info from cgtest-config.json
* compiles cgtest-runner.c and 
* executes the compiled binary.  

### Examples
```cgtest.exe --config ./unitest/cgtest/cgtest-config.json```  
Generates testrunner and excecute it.

```cgtest.exe --create ./unitest/cgtest```  
Creates a default template cgtest-config.json inside ./unitest/cgtest (creating the directory if it doesn't exist yet)  
Creates the cgtest.h file alongside it, it contains macros for unit tests.

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
  * -c --config path to the cgtest-config.json, or to the directory containing it
    (if the path is a directory, "cgtest-config.json" is looked up inside it).
    If cgtest-config.json doesn't exist than error and exit cgtest.exe with an appropriate message.  
    ```cgtest.exe --config ./unitest/cgtest/cgtest-config.json```
    ```cgtest.exe --config ./unitest/cgtest```
  * -C --create path to the directory cgtest-config.json and cgtest.h should be created in
    (the argument is always a directory, never the config file's own path; the directory is
    created if it doesn't exist yet).
    If cgtest-config.json already exist in that directory than error and exit cgtest.exe with an appropriate message.
  * -v --version of cgtest
  * -h --help of cgtest

### cgtest-config.json
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
  - `EXPECT_EQ_INT`/`EXPECT_EQ_UINT`/`EXPECT_EQ_DOUBLE`/`EXPECT_EQ_PTR`/`EXPECT_EQ_STR`/
    `EXPECT_EQ_STR_NOCASE` (and their `ASSERT_EQ_` counterparts, same EXPECT-vs-ASSERT split
    as above) compare two values and print both on failure - unlike `EXPECT_TRUE`, which only
    shows the source text of the whole condition. Each casts both operands to one canonical
    type per family (`long`/`unsigned long`/`double`/`const void *`) rather than needing one
    macro per exact C type. `EXPECT_EQ_STR`/`ASSERT_EQ_STR` compare via `strcmp()`, not
    pointer equality - same reason GoogleTest has `EXPECT_STREQ` separate from `EXPECT_EQ`.
    `EXPECT_EQ_STR_NOCASE`/`ASSERT_EQ_STR_NOCASE` are the case-insensitive counterparts
    (GoogleTest's `EXPECT_STRCASEEQ`), compared via a small portable `cgtest_strcasecmp()`
    helper rather than the non-standard `strcasecmp()`/`_stricmp()`.
* the cgtest-runner executes those function in the same order they appear in the test_file.
  - That allowes to use the first function in a file as init test setup.
  - That allowes to use the last function in a file as tear down setup.

### Source Modules file
* cgtest_arq.c cgtest_arq.h command line parser use arq lib from httpe://github.com/bartgeier/arq
* cgtest_config.c cgtest_config.h use for json parsing jsmn https://github.com/zserge/jsmn
  parses cgtest-config.json
* cgtest_create.c cgtest_create.h implements -C/--create: writes a template cgtest-config.json and cgtest.h into a directory.
* cgtest_runner.c cgtest_runner.h implements -c/--config: generates cgtest-runner.c, compiles it, and executes it.
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
cgtest.exe --config ./unitest/cgtest/cgtest-config.json
cgtest.exe --config ./unitest/cgtest
cgtest.exe --create ./unitest/cgtest
cgtest.exe --version
cgtest.exe --help
```
