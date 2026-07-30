/* test_math.c - example test file for the examples/mathlib project.
 *
 * Discovered and run by cgtest.exe itself (via cgtest-config.json in
 * this directory's parent) once cgtest-runner.c generation exists -
 * unlike tests/test_*.c in the cgtest repo root, this file has no
 * main() of its own; the generated runner supplies it.
 */
#include "mathlib.h"
#include "cgtest.h"

void test_math_add(void)
{
    EXPECT_TRUE(mathlib_add(2, 3) == 5);
    EXPECT_TRUE(mathlib_add(-2, 2) == 0);
}

void test_math_sub(void)
{
    EXPECT_TRUE(mathlib_sub(5, 3) == 2);
    EXPECT_TRUE(mathlib_sub(0, 7) == -7);
}

void test_math_div(void)
{
    int divisor = 3;

    /* ASSERT_TRUE: a genuine precondition - dividing by zero below
     * would be undefined behavior, so this check must stop the test
     * immediately on failure rather than let it continue. */
    ASSERT_TRUE(divisor != 0);
    EXPECT_TRUE(mathlib_div(6, divisor) == 2);
    EXPECT_TRUE(mathlib_div(7, 2) == 3);
}
