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

void test_expect_ne_int(void)
{
    EXPECT_NE_INT(42, 43);
}

void test_assert_ne_int(void)
{
    ASSERT_NE_INT(42, 43);
}

void test_expect_eq_uint(void)
{
    EXPECT_EQ_UINT(42u, 42u);
}

void test_assert_eq_uint(void)
{
    ASSERT_EQ_UINT(42u, 42u);
}

void test_expect_ne_uint(void)
{
    EXPECT_NE_UINT(42u, 43u);
}

void test_assert_ne_uint(void)
{
    ASSERT_NE_UINT(42u, 43u);
}

void test_expect_eq_float(void)
{
    /* 1.1f - 1.0f isn't bit-identical to 0.1f - this passes only
     * because of EXPECT_EQ_FLOAT's epsilon tolerance; exact equality
     * would fail here. */
    EXPECT_EQ_FLOAT(0.1f, 1.1f - 1.0f);
}

void test_assert_eq_float(void)
{
    ASSERT_EQ_FLOAT(0.1f, 1.1f - 1.0f);
}

void test_expect_ne_float(void)
{
    /* Unlike the rounding noise above, this is a real difference,
     * far outside the epsilon tolerance. */
    EXPECT_NE_FLOAT(4.2f, 4.3f);
}

void test_assert_ne_float(void)
{
    ASSERT_NE_FLOAT(4.2f, 4.3f);
}

void test_expect_eq_double(void)
{
    /* Same idea as EQ_FLOAT above, in double precision. */
    EXPECT_EQ_DOUBLE(0.1 + 0.2, 0.3);
}

void test_assert_eq_double(void)
{
    ASSERT_EQ_DOUBLE(0.1 + 0.2, 0.3);
}

void test_expect_ne_double(void)
{
    EXPECT_NE_DOUBLE(4.2, 4.3);
}

void test_assert_ne_double(void)
{
    ASSERT_NE_DOUBLE(4.2, 4.3);
}

void test_expect_near_double(void)
{
    EXPECT_NEAR_DOUBLE(4.2, 4.2000001, 0.001);
}

void test_assert_near_double(void)
{
    ASSERT_NEAR_DOUBLE(4.2, 4.2000001, 0.001);
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

void test_expect_ne_ptr(void)
{
    int x = 0;
    int y = 0;

    EXPECT_NE_PTR(&x, &y);
}

void test_assert_ne_ptr(void)
{
    int x = 0;
    int y = 0;

    ASSERT_NE_PTR(&x, &y);
}

void test_expect_eq_str(void)
{
    EXPECT_EQ_STR("cgtest", "cgtest");
}

void test_assert_eq_str(void)
{
    ASSERT_EQ_STR("cgtest", "cgtest");
}

void test_expect_ne_str(void)
{
    EXPECT_NE_STR("cgtest", "gtest");
}

void test_assert_ne_str(void)
{
    ASSERT_NE_STR("cgtest", "gtest");
}

void test_expect_eq_str_nocase(void)
{
    EXPECT_EQ_STR_NOCASE("CGTest", "cgtest");
}

void test_assert_eq_str_nocase(void)
{
    ASSERT_EQ_STR_NOCASE("CGTest", "cgtest");
}

void test_expect_ne_str_nocase(void)
{
    EXPECT_NE_STR_NOCASE("CGTest", "gtest");
}

void test_assert_ne_str_nocase(void)
{
    ASSERT_NE_STR_NOCASE("CGTest", "gtest");
}
