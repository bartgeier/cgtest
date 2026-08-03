/* ctimer.c - see ctimer.h */
#include "ctimer.h"

#ifdef _WIN32
#include <windows.h>

double ctimer_now_ms(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
}
#else
#include <stddef.h>
#include <sys/time.h>

double ctimer_now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}
#endif
