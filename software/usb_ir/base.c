#include "base.h"

#include <time.h>

#ifdef CLOCK_MONOTONIC
  #define CLOCK_SOURCE CLOCK_MONOTONIC
#else
  #define CLOCK_SOURCE CLOCK_REALTIME
#endif

uint64_t microsSinceX()
{
    struct timespec tp;

    if (clock_gettime(CLOCK_SOURCE, &tp) != 0)
        return 0;

    return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
}
