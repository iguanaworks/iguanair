#include "base.h"
#include <stdio.h>

bool startThread(THREAD_PTR *handle, void* (*target)(void*), void *arg)
{
    return false;
}

bool joinThread(THREAD_PTR *handle, void **exitVal)
{
    return false;
}

uint64_t microsSinceX()
{
    uint64_t retval = 0;
    LARGE_INTEGER freq, count;

    if (QueryPerformanceFrequency(&freq) &&
        QueryPerformanceCounter(&count))
    {
        char buffer[1024];

        retval = count.QuadPart * 1000000 / freq.QuadPart;

        sprintf(buffer, "%ld %ld = %ld\n", count, freq, retval);
        OutputDebugString(buffer);
    }

    return retval;
}
