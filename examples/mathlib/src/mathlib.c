#include "mathlib.h"

int mathlib_add(int a, int b)
{
    return a + b;
}

int mathlib_sub(int a, int b)
{
    return a - b;
}

int mathlib_div(int a, int b)
{
    return a / b;
}

const char *mathlib_sign(int x)
{
    if (x > 0) return "positive";
    if (x < 0) return "negative";
    return "zero";
}
