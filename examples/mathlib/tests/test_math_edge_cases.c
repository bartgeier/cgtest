/* test_math_edge_cases.c - a second example test file for the
 * examples/mathlib project, alongside test_math.c, showing that
 * cgtest.exe discovers and runs every test_*.c file in a test
 * directory, not just one.
 */
#include "mathlib.h"
#include "cgtest.h"

void test_div_truncates_toward_zero(void)
{
    int divisor = 2;

    /* ASSERT_FALSE: same zero-divisor precondition as test_math.c's
     * ASSERT_TRUE(divisor != 0), just phrased negatively - either
     * reads naturally depending on the check. */
    ASSERT_FALSE(divisor == 0);
    EXPECT_EQ_INT(3, mathlib_div(7, divisor));
    EXPECT_EQ_INT(-3, mathlib_div(-7, divisor));
    EXPECT_EQ_INT(-3, mathlib_div(7, -divisor));
    /* Rules out the plausible-but-wrong floor-division alternative -
     * EXPECT_FALSE, not EXPECT_EQ_INT, since this is a "must not
     * equal" check rather than an "equals this" one. */
    EXPECT_FALSE(mathlib_div(-7, divisor) == -4);
}

void test_add_is_commutative(void)
{
    EXPECT_EQ_INT(mathlib_add(3, 5), mathlib_add(5, 3));
    EXPECT_EQ_INT(mathlib_add(-4, 9), mathlib_add(9, -4));
}

void test_sub_of_a_number_from_itself_is_zero(void)
{
    EXPECT_EQ_INT(0, mathlib_sub(42, 42));
    EXPECT_EQ_INT(0, mathlib_sub(-7, -7));
}
