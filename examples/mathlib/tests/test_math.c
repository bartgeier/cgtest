/* test_math.c - example test file for the examples/mathlib project.
 *
 * Discovered and run by cgtest.exe itself (via cgtest-project.json in
 * this directory's parent) once cgtest-runner.c generation exists -
 * unlike tests/test_*.c in the cgtest repo root, this file has no
 * main() of its own; the generated runner supplies it.
 */
#include "mathlib.h"
#include "cgtest.h"

void test_math_add(void)
{
    EXPECT_EQ_INT(-5, mathlib_add(2, 3));
    EXPECT_EQ_INT(0, mathlib_add(-2, 2));
}

void test_math_sub(void)
{
    EXPECT_EQ_INT(2, mathlib_sub(5, 3));
    EXPECT_EQ_INT(-7, mathlib_sub(0, 7));
}

void test_math_div(void)
{
    int divisor = 3;

    /* ASSERT_TRUE: a genuine precondition - dividing by zero below
     * would be undefined behavior, so this check must stop the test
     * immediately on failure rather than let it continue. */
    ASSERT_TRUE(divisor != 0);
    EXPECT_EQ_INT(2, mathlib_div(6, divisor));
    EXPECT_EQ_INT(3, mathlib_div(7, 2));
}

void test_math_sign(void)
{
    EXPECT_EQ_STR("positive", mathlib_sign(5));
    EXPECT_EQ_STR("negative", mathlib_sign(-3));
    EXPECT_EQ_STR("zero", mathlib_sign(0));
}
