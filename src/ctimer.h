/* ctimer.h - portable wall-clock elapsed-time measurement, used by
 * cgtest_runner.c to time cgtest.exe --run's scan/generate/compile/run
 * phases for the -t/--time flag (see cgtest_arq.h). C89's own
 * <time.h> isn't good enough here: clock() measures CPU time, which
 * is misleading once system() spawns a subprocess (a compiler
 * invocation sitting mostly idle waiting on disk I/O would look
 * artificially fast), and time() only has 1-second resolution, too
 * coarse for a compile step that might take a fraction of a second.
 * This uses gettimeofday() on POSIX / QueryPerformanceCounter() on
 * Windows instead - the same platform-split pattern already used
 * elsewhere in this codebase (see cgtest_runner.c's mkdir/isatty).
 */
#ifndef CTIMER_H
#define CTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the current wall-clock time in milliseconds, as a
 * monotonically non-decreasing value relative to other calls within
 * the same process run. Not tied to any particular epoch - only
 * differences between two calls (ctimer_now_ms() - earlier_value) are
 * meaningful. */
double ctimer_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* CTIMER_H */
