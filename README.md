
# cgtest

**cgtest is a C unit test DSL compiler.**

This is my first Claude-assisted “vibe coding” project.  
I wanted to explore what it would be like to build something from scratch while using Claude to help me understand the problem, experiment with designs, and turn ideas into working C code.

The project also comes from a very specific habit I have with my DIY projects: **I tend to compile everything at once.** I don't normally maintain elaborate incremental build systems or carefully separated compilation units. For a small C project, I often just throw the sources together and compile the whole thing.

That makes the usual C++ unit-testing approach, such as GoogleTest, feel unnecessarily heavy for the kind of projects I build.

cgtest takes a different approach. You describe tests in plain C using a small DSL inspired by GoogleTest's assertion style (`EXPECT_EQ`, `ASSERT_TRUE`, etc.). It supports the usual building blocks you expect from a unit-testing framework, including **fixtures and assertions**, but keeps the implementation and generated test runner in plain C89.

The workflow is deliberately simple:

```text
cgtest --run
        ↓
cgtest-runner.c
        ↓
C compiler
        ↓
test runner
```

There are no C++ templates, no virtual dispatch, and no external dependencies. The generated runner consists of ordinary C89 structs and functions that a C compiler can compile directly.

This is particularly relevant to my “compile everything at once” workflow. GoogleTest is a powerful C++ testing framework with features such as fixtures and matchers, but its implementation relies heavily on C++ language facilities. A translation unit that includes `gtest.h` therefore brings a substantial amount of framework machinery into the compilation.

cgtest takes the opposite approach: **keep the test authoring experience expressive, but make the generated implementation boring C.**

Fixtures are represented as ordinary C data and setup/teardown functions. Assertions become straightforward C code. The test runner is generated rather than implemented as a large generic C++ framework.

So cgtest isn't intended to replace GoogleTest. It's an experiment in seeing how far a small C-native testing DSL can go while keeping the implementation understandable, dependency-free, and friendly to a simple “compile the whole project” workflow.

The entire compiler is written in C89 and can also be used as a single amalgamated `cgtest.c` file for a drop-in build.


## Download cgtest.c

```
curl -fL -o cgtest.c https://github.com/bartgeier/cgtest/raw/refs/tags/v0.1.0/cgtest.c
```

## Building cgtest

```
gcc -std=c89 cgtest.c -o cgtest
```

## Create a fresh project
```
cgtest --init examples/freshPorject
```
**Run test:**
```
cgtest --run examples/freshPorject/cgtest
```
**See how to use cgtest macors:**

[examples/freshProject/cgtest/test_cgtest_macros.c](examples/freshProject/cgtest/test_cgtest_macros.c)  

## cgtest-project.json
[examples/mathlib/cgtest/cgtest-project.json](examples/mathlib/cgtest/cgtest-project.json)  
[examples/freshProject/cgtest/cgtest-project.json](examples/freshProject/cgtest/cgtest-project.json)
```json
{
    "compiler_command": "gcc -std=c89 -O0 -Wall -Wextra -pedantic-errors",
    "msvc": false,
    "single_translation_unit": false,
    "include_paths": [],
    "source_files": [],
    "output_path": "../build",
    "test_directories": [
        "."
    ]
}
```

* **compiler_command**  
  used to compile `cgtest-runner.c`
* **msvc**  
  **false** GCC/Clang style (`-I"path"`, `-o "path"`) **default**  
  **true** `cl.exe` MSVC style (`/I"path"`, `/Fe:"path"`)
* **single_translation_unit**  
  **false** every `test_*.c` has its own translation unit. Compiles slower. **default**  
  **true**  every `test_*.c` file is in the same translation unit. Compiles faster.
* **include_paths**  
  list of directories added to the compile command
* **source_files**  
  list of the project's own `.c` files  
  the generated test runner needs to link against.
* **output_path**  
  directory `cgtest-runner.c` and the compiled binary of the test-runner.exe
* **test_directories**  
  directories cgtest searches for `test_*.c` files.

Of the five required fields (`compiler_command`, `include_paths`,
`source_files`, `output_path`, `test_directories`), none has a
project-agnostic default - `--init` always writes them, and `--run` errors out
if any are missing.

## Update cgtest

When a new version of cgtest installed is then do this for updating you project: 
1. delete examples/mathlib/cgtest.h  
1. cgtest --init examples/mathlib  
   * It never overrides an existing cgtest.h file but creates a new one if missing.
   * updates the cgtest-runner.json if a new field is introduced.

## Usage

```
cgtest --init <dir>          create cgtest-project.json, cgtest.h, and an example test inside <dir>/cgtest
cgtest --run <path>          generate, compile and run cgtest-runner.c
cgtest --run <path> --time   also print a scan/generate/compile/run timing breakdown
cgtest --version             print the cgtest version
cgtest --license             print the cgtest license (MIT)
cgtest --help                print this message
```

## Fixtures

A test function opts into a fixture by taking one pointer parameter instead of
`(void)`. `setup_<name>` allocates and initializes it before each call and is required;
`teardown_<name>` cleans it up afterwards and is optional (skip it if there's nothing
to release - the fixture is reclaimed when the process exits either way):

```c
typedef struct State { int value; } State;   /* tag must match the typedef name */

void setup_bar(State **state) {
    *state = calloc(1, sizeof(State));
    (*state)->value = 42;
}
void teardown_bar(State *state) { free(state); /* only if prompt cleanup matters */ }

void test_bar(State *state) {
    EXPECT_EQ_INT(42, state->value);
}
```

See [specification.md](specification.md) chapter 6 for the full design.  
