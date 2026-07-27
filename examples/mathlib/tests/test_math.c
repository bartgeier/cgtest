/* test_math.c - example test file for the examples/mathlib project.
 *
 * Discovered and run by cgtest.exe itself (via cgtest-config.json in
 * this directory's parent) once cgtest-runner.c generation exists -
 * unlike tests/test_*.c in the cgtest repo root, this file has no
 * main() of its own; the generated runner supplies it.
 */
#include "mathlib.h"

#include <stdbool.h>
#include <stdio.h>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return false; \
        } \
    } while (0)

bool test_math_add(void)
{
    CHECK(mathlib_add(2, 3) == 5);
    CHECK(mathlib_add(-2, 2) == 0);
    return true;
}

bool test_math_sub(void)
{
    CHECK(mathlib_sub(5, 3) == 2);
    CHECK(mathlib_sub(0, 7) == -7);
    return true;
}

bool test_math_div(void)
{
    CHECK(mathlib_div(6, 3) == 2);
    CHECK(mathlib_div(7, 2) == 3);
    return true;
}
