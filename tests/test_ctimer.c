/* test_ctimer.c - unit tests for ctimer_now_ms(), the portable
 * wall-clock timer used by cgtest_runner.c's -t/--time phase
 * breakdown.
 *
 * Written in cgtest's own test convention (void test_<name>(void)); see
 * test_ctestscanner.c's header comment for why main() below dispatches
 * them manually instead of via a generated cgtest-runner.
 */
#include "ctimer.h"

#include <stdio.h>

static int test_failed = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            test_failed = 1; \
            return; \
        } \
    } while (0)

void test_now_ms_is_nonnegative(void)
{
    CHECK(ctimer_now_ms() >= 0.0);
}

void test_now_ms_is_monotonic_non_decreasing(void)
{
    double first = ctimer_now_ms();
    double second = ctimer_now_ms();

    CHECK(second >= first);
}

void test_elapsed_time_grows_with_busy_work(void)
{
    /* No sleep() available portably without its own platform split -
     * a large-enough busy loop (not optimized away: the Makefile's
     * CFLAGS carry no -O flag, so this runs at -O0) burns measurable
     * wall time instead. */
    volatile long i;
    double start = ctimer_now_ms();
    double elapsed;

    for (i = 0; i < 50000000L; i++) {
        /* spin */
    }

    elapsed = ctimer_now_ms() - start;
    CHECK(elapsed > 0.0);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase cases[] = {
        { "test_now_ms_is_nonnegative", test_now_ms_is_nonnegative },
        { "test_now_ms_is_monotonic_non_decreasing", test_now_ms_is_monotonic_non_decreasing },
        { "test_elapsed_time_grows_with_busy_work", test_elapsed_time_grows_with_busy_work }
    };
    size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t i;
    size_t failed = 0;

    for (i = 0; i < count; i++) {
        test_failed = 0;
        cases[i].fn();
        printf("[%s] %s\n", test_failed ? "FAIL" : "PASS", cases[i].name);
        if (test_failed) {
            failed++;
        }
    }

    printf("\n%lu/%lu passed\n", (unsigned long)(count - failed), (unsigned long)count);
    return failed == 0 ? 0 : 1;
}
