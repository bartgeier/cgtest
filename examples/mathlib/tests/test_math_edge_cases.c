/* test_math_edge_cases.c - a second example test file for the
 * examples/mathlib project, alongside test_math.c, showing that
 * cgtest.exe discovers and runs every test_*.c file in a test
 * directory, not just one.
 */
#include "mathlib.h"
#include "cgtest.h"

void test_div_truncates_toward_zero(void)
{
    CGTEST_CHECK(mathlib_div(7, 2) == 3);
    CGTEST_CHECK(mathlib_div(-7, 2) == -3);
    CGTEST_CHECK(mathlib_div(7, -2) == -3);
}

void test_add_is_commutative(void)
{
    CGTEST_CHECK(mathlib_add(3, 5) == mathlib_add(5, 3));
    CGTEST_CHECK(mathlib_add(-4, 9) == mathlib_add(9, -4));
}

void test_sub_of_a_number_from_itself_is_zero(void)
{
    CGTEST_CHECK(mathlib_sub(42, 42) == 0);
    CGTEST_CHECK(mathlib_sub(-7, -7) == 0);
}
