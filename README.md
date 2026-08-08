# cgtest
A C unit test DSL compiler. You describe tests in plain C, and cgtest generates,
compiles, and runs a test runner from them: `--init` scaffolds a project
(`cgtest-project.json`, `cgtest.h`, and an example test), `--run` reads that project
file, generates `cgtest-runner.c`, compiles it, and executes it. Written in C89 with
no external dependencies, and available as a single amalgamated `cgtest.c` for a
drop-in build.

Inspired by GoogleTest's macro/assertion style (`EXPECT_EQ`, `ASSERT_TRUE`, etc.), but
compiled as plain C89 rather than C++. GoogleTest's fixtures and matchers are template-
and vtable-heavy, so every translation unit that includes `gtest.h` pays real C++
template-instantiation cost; cgtest's generated runner is plain structs and functions a
C compiler resolves directly, with no templates and no virtual dispatch to instantiate.
In practice that makes cgtest noticeably faster to compile than an equivalent GoogleTest
suite.

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

```
examples/freshProject/cgtest/test_cgtest_macros.c
```
## cgtest-project.json
```
examples/mahtlib/cgtest/cgtest-project.json
```
```
examples/freshProject/cgtest/cgtest-project.json
```

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
   * It never overrides an existing cgtest.h file
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

See specification.md chapter 6 for the full design.  
