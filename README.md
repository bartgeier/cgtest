
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
curl -fL -o cgtest.c https://github.com/bartgeier/cgtest/raw/refs/tags/v0.1.3/cgtest.c
```

## Compile cgtest

```
gcc -std=c89 cgtest.c -o cgtest
```
[more Compiler examples](docs/compile_cgtest.c.md)

## Create a fresh project
```
cgtest --init examples/freshProject
```
[examples/freshProject/cgtest/cgtest.h](examples/freshProject/cgtest/cgtest.h)  
[examples/freshProject/cgtest/cgtest-project.json](examples/freshProject/cgtest/cgtest-project.json)  

**See how to use cgtest macors:**  
[examples/freshProject/cgtest/test_cgtest_macros.c](examples/freshProject/cgtest/test_cgtest_macros.c)  

**Run test:**
```
cgtest --run examples/freshProject/cgtest
```

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
[more cgtest-project.json examples](docs/cgtest-project.json.md)

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

When a new version of cgtest is installed, update an existing project as follows:

1. Delete the project's `cgtest.h`:

   ```text
   examples/mathlib/cgtest.h
   ```

2. Run:

   ```text
   cgtest --init examples/mathlib
   ```

`--init` is safe to run on an existing project:

* It never overwrites an existing `cgtest.h`. If the file is missing, a new one is created.
* It updates `cgtest-project.json` when the new cgtest version introduces new configuration fields.

## Usage

```
cgtest -i, --init <dir>               create cgtest-project.json, cgtest.h, and an example test inside <dir>/cgtest
cgtest -r, --run <path>               generate, compile and run cgtest-runner.c
cgtest -r, --run <path> -t, --time    also print a scan/generate/compile/run timing breakdown
cgtest -v, --version                  print the cgtest version
cgtest -l, --license                  print the cgtest license (MIT)
cgtest -h, --help                     print this message
```
## Test discovery

Test directories are registered in `cgtest-project.json`. When `cgtest --run` generates the test runner, it scans those directories for C source files matching:

```text
test_*.c
```

Within those files, cgtest looks for test functions with the following form:

```c
void test_name(void)
{
    /* test code */
}
```

Each matching function is automatically added to the generated test runner.

There is therefore no separate test registration call in the C source. The project configuration defines **where to look**, while the filename and function naming conventions define **what is a test**.


## Fixtures

A test function opts into a fixture by taking one pointer parameter instead of `(void)`.

For a fixture named `bar`, cgtest expects a `State` type and corresponding `setup_bar` and optional `teardown_bar` functions. The setup function is called before each test and is **required**. The teardown function is called afterwards when provided.

The fixture type needs a **struct tag matching its typedef name**. cgtest uses the type through a forward declaration when generating the test runner, so the tag is required for the type to be referenced before its full definition.

```c
typedef struct State { int value; } State;  /* tag must match the typedef name */

void setup_bar(State **state)
{
    *state = calloc(1, sizeof(State));
    (*state)->value = 42;
}

void teardown_bar(State *state)
{
    free(state);
}

void test_bar(State *state)
{
    EXPECT_EQ_INT(42, state->value);
}
```

The fixture instance is created by `setup_bar` before each call to `test_bar`. If `teardown_bar` is provided, it is called after the test to release resources owned by the fixture. If there is nothing that requires explicit cleanup, `teardown_bar` can be omitted.

### Using third-party types

The matching tag is required for the **fixture type itself**, not for types used inside the fixture. This makes it possible to use third-party structs even when their type does not have a matching tag:

```c
typedef struct State {
    ThirdPartyThing thing;
} State;
```

A pointer can also be used when the third-party object needs to be allocated separately:

```c
typedef struct State {
    ThirdPartyThing *thing;
} State;
```

See [specification.md](specification.md) chapter 6 for the full design.  
