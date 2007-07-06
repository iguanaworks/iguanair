#include "base.h"

#ifdef __APPLE__
    #include "darwin/clock_gettime.h"
#endif

#ifdef CLOCK_MONOTONIC
  #define CLOCK_SOURCE CLOCK_MONOTONIC
#else
  #define CLOCK_SOURCE CLOCK_REALTIME
#endif

#if HAVE_CLOCK_GETTIME
#include <time.h>

uint64_t microsSinceX()
{
    struct timespec tp;

    if (clock_gettime(CLOCK_SOURCE, &tp) != 0)
        return 0;

    return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
}

#elif HAVE_GETTIMEOFDAY
#include <sys/time.h>

uint64_t microsSinceX()
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        return 0;

    return tv.tv_sec * 1000000 + tv.tv_usec;
}

#else
#error Not supported mechanism for subsecond timing found.

#endif
