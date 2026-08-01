/* test_cgtest_macros.c - one example per macro from cgtest.h, alongside
 * test_math.c and test_math_edge_cases.c. These checks don't exercise
 * mathlib at all; they exist purely to show each macro's call shape.
 */
#include "cgtest.h"

void test_expect_true(void)
{
    EXPECT_TRUE(1 == 1);
}

void test_expect_false(void)
{
    EXPECT_FALSE(1 == 2);
}

void test_assert_true(void)
{
    ASSERT_TRUE(1 == 1);
}

void test_assert_false(void)
{
    ASSERT_FALSE(1 == 2);
}

void test_expect_eq_int(void)
{
    EXPECT_EQ_INT(42, 42);
}

void test_assert_eq_int(void)
{
    ASSERT_EQ_INT(42, 42);
}

void test_expect_eq_uint(void)
{
    EXPECT_EQ_UINT(42u, 42u);
}

void test_assert_eq_uint(void)
{
    ASSERT_EQ_UINT(42u, 42u);
}

void test_expect_eq_double(void)
{
    EXPECT_EQ_DOUBLE(4.2, 4.2);
}

void test_assert_eq_double(void)
{
    ASSERT_EQ_DOUBLE(4.2, 4.2);
}

void test_expect_eq_ptr(void)
{
    int x = 0;

    EXPECT_EQ_PTR(&x, &x);
}

void test_assert_eq_ptr(void)
{
    int x = 0;

    ASSERT_EQ_PTR(&x, &x);
}

void test_expect_eq_str(void)
{
    EXPECT_EQ_STR("cgtest", "cgtest");
}

void test_assert_eq_str(void)
{
    ASSERT_EQ_STR("cgtest", "cgtest");
}

void test_expect_eq_str_nocase(void)
{
    EXPECT_EQ_STR_NOCASE("CGTest", "cgtest");
}

void test_assert_eq_str_nocase(void)
{
    ASSERT_EQ_STR_NOCASE("CGTest", "cgtest");
}
