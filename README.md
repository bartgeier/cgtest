# cgtest
A C unit test DSL compiler.

## Usage

```
cgtest --init <dir>          create cgtest-project.json, cgtest.h, and an example test inside <dir>/cgtest
cgtest --run <path>          generate, compile and run cgtest-runner.c
cgtest --run <path> --time   also print a scan/generate/compile/run timing breakdown
cgtest --version             print the cgtest version
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

https://github.com/bartgeier/cgtest

