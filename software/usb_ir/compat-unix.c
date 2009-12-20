/****************************************************************************
 ** base.c ****************************************************************
 ****************************************************************************
 *
 * A few functions must be implemented for compatibility.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */
#include "iguanaIR.h"
#include "compat.h"

#include <string.h>
#include <fcntl.h>

#include "support.h"

#if USE_CLOCK_GETTIME
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

#elif HAVE_MACH_ABSOLUTE_TIME
/*
  Only seen this on OS X, but I suppose other systems are possible.
 */
#include <mach/mach_time.h>

uint64_t microsSinceX()
{
    uint64_t elapsed_ms;
    mach_timebase_info_data_t mtid;

    /* NOTE: not exactly thread safe initialization */
    mach_timebase_info(&mtid);
    elapsed_ms = (double)mach_absolute_time() * mtid.numer / mtid.denom / 1000;

    return elapsed_ms;
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
#error No supported mechanism for subsecond timing found.

#endif

bool setNonBlocking(PIPE_PTR pipe)
{
    int flags;

    flags = fcntl(pipe, F_GETFL);
    if (flags == -1)
        message(LOG_ERROR,
                "Failed read status flags for socket.\n");
    else
        return fcntl(pipe, F_SETFL, flags | O_NONBLOCK) != -1;

    return false;
}

char* translateError(int errnum)
{
    return strerror(errnum);
}

DIR_HANDLE findNextFile(DIR_HANDLE hFind, char *buffer)
{
    if (hFind == NULL)
        hFind = opendir(buffer);

    if (hFind != NULL)
    {
        struct dirent *dent;

        if ((dent = readdir(hFind)) == NULL)
        {
            closedir(hFind);
            hFind = NULL;
        }
        else
            strcpy(buffer, dent->d_name);
    }

    return hFind;
}
