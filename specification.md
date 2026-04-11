# Project Name cgtest

## 1. Overview
cgtest is a command-line C unit test DSL compiler application.
It compiles and runs a test runner from test files.
cgtest.exe:  
* generates cgtest-runner.c 
* compiles cgtest-runner.c and 
* executes the compiled binary.

Example:

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

## 4. Features

### Core Features
* Pars command-line arguments
  * -d --dir adds a directory path (to test functions) to the list of test directorys
  * -c --compiler "gcc -std=c99 -o /unittest/testrunner.exe" 
  * instead of using for --compiler we could use a cgtest-config.json
    - json parser single header in c https://github.com/zserge/jsmn
* Search test directorys for files their nameing starts with test_...  
  - test_math.c
  - test_strview.c
* This files are than scanned for functions their nameing starts also with test_... with following signature:
  - bool test_(void)
  - bool test_math_add(void) {....}
  - bool test_goal_sub(void) {....}
  - bool test_div(void) {....}
* the cgtest-runner executes those function in the same order they appear in the test_file.
  - That allowes to use the first function in a file as init test setup.
  - That allowes to use the last function in a file as tear down setup.

### Source Modules file
* cgtest_arq.c cgtest_arq.h command line parser use arq lib from httpe://github.com/bartgeier/arq
* cgtest_config.c cgtest_config.h use for json parsing jsmn https://github.com/zserge/jsmn
  parses cgtest-config.json
* clexer.c clexer.h for scanning and listening the test_function with in the test_files.
* cgtest_main.c main function

- Search items
- Sort items
- Colorized terminal output

---

## 5. CLI Interface

Define exactly how the program is used:

```bash
./app add "Buy milk"
./app list
./app remove 3
