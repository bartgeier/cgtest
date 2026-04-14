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

```cgtest.exe --create ./unitest/cgtest/cgtest-config.json```  
Creates a default template cgtest-config.json  
Creates the cgtest.h file it contains macros for unit tests.

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
  * -c --config path to the cgtest-config.json.
    If cgtest-config.json doesn't exist than error and exit cgtest.exe with an appropriate message.  
    ```cgtest.exe --config ./unitest/cgtest/cgtest-config.json```
  * -C --create path to where cgtest-config.json should be created.
    If cgtest-config.json already exist than error and exit cgtest.exe with an appropriate message.
  * -v --version of cgtest
  * -h --help of cgtest

### cgtest-config.json
  * set compiler command "gcc -std=c99 -O3" 
  * Include path list
  * Source file list
  * Output path to generate cgtest-runner.c and cgtest-runner.exe
  * Directory list where to find the unitest c test....c files.  

Use for json parser single header jsmn.h in c https://github.com/zserge/jsmn
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
