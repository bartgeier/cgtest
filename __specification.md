# Project Name cgtest

## 1. Overview
cgtest is a command-line C unit test DSL compiler application.
It compiles and runs a test runner from test files.  
**cgtest.exe:**  
* generates cgtest-runner.c with the info from cgtest-config.json
* compiles cgtest-runner.c and 
* executes the compiled binary.  

### Examples
```cgtest.exe --create ./unitest/cgtest/cgtest-config.json```  
If config path doesn't exist error and exit.
If path exists creates a default template cgtest-config.json file. 
The cgtest-config.json could have a diffrent name example config.json.
This file contains informations and paths to create the cgtest_runner.c and exe.
All paths in the config are relative paths to the cgtest-config.json file.

```cgtest.exe --config ./unitest/cgtest/cgtest-config.json```  
If path doesn't exist error and exit.  
If file already exist error and exit.  
Path to cgtest.h is in the cgtest-config.json file.  
Creates the cgtest.h file if it not exist.  
If cgtest.h exist it checks and compares the date with the date from cgtest.exe  
if cgtest.exe is younger then cgtest.exe overwrite cgtest.h. 
Generates testrunner and excecute it.  

---

## 2. Goals

* Must be written in C (C89 standard)
* Must compile with gcc, clang cl.exe or any c compiler
* Must compile on Windows/Linux/Mac
* Must not use external libraries (unless specified)

---

## 3. Build system

Use Tsoding nob.h buildsystem.  
For downloading arq, jsmn, amalgamate we use curl in nob.

---


### cgtest.h
cgtest.h it contains macros for unit tests.

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
  * Where to create cgtest.h file.
  * set compiler command "gcc -std=c99 -O3" 
  * Include path list
  * Source file list
  * Output path to generate cgtest-runner.c and cgtest-runner.exe
  * Directory list where to find the unitest c test....c files.  

#### Example config.json
Use for json parser single header jsmn.h in c https://github.com/zserge/jsmn
``` 
{
  "cgtest_h": ".",
  "cgtestrunner_output_dir": "..",
  "compiler": "gcc",
  "compiler_arguments": [
    "-std=c99",
    "O3"
  ],
  "include_directorys": [
    "../../source/",
    "../../arq/"
  ],
  "source_files": [
    "../../source/app_abc.c",
    "../../source/app_main.c"
  ],
  "test_directorys": [
    "..",
    "../special/"
  ]
}
```

### Unit test files
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

### Some Source Modules files
* cgtest_lexer.c cgtest_lexer.h => a c lexer.  
  for scanning and listening the test_function with in the test_files.
* cgtest_arq.c cgtest_arq.h command line parser use arq lib from http://github.com/bartgeier/arq
* cgtest_config.c cgtest_config.h use for json parsing jsmn https://github.com/zserge/jsmn
  parses cgtest-config.json
* cgtest_main.c main function
* cgtest_header.h cgtest_header.c creates generates cgtest.h.
  cgtest.h it contains macros for unit tests is not part of cgtest.c cgtest.exe itself.  
  It is #include "cgtest/cgtest.h" in the test files.
  cgtest.h lives as a kind of resource file in the exe.
  But not as windows rescources more like an string. So we can compile on Linus and Mac too.
  

---

## 5. CLI Interface

Define exactly how the program is used:

```bash
./app add "Buy milk"
./app list
./app remove 3
