/* test_math_fixture.c - example test file for the examples/mathlib
 * project showing the fixture form (see specification.md ch.6): a
 * test function takes one pointer parameter instead of (void), and
 * setup_<name>/teardown_<name> initialize/clean up that fixture around
 * each call to test_<name>.
 */
#include "mathlib.h"
#include "cgtest/cgtest.h"
#include <stdlib.h>

typedef struct MathState {
    int accumulator;
} MathState;

void setup_running_total(MathState **state)
{
    *state = calloc(1, sizeof(MathState));
    (*state)->accumulator = 11;
}

void teardown_running_total(MathState *state)
{
    free(state);
}

void test_running_total(MathState *state)
{
    state->accumulator = mathlib_add(state->accumulator, 2);
    state->accumulator = mathlib_add(state->accumulator, 3);
    EXPECT_EQ_INT(16, state->accumulator);
}
