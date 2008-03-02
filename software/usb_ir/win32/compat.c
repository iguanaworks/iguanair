/**************************************************************************
 * compat.c ***************************************************************
 **************************************************************************
 *
 * TODO: DESCRIBE AND DOCUMENT THIS FILE
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include <stdio.h>

#include "compat.h"
#include "iguanaIR.h"

bool startThread(THREAD_PTR *handle, void* (*target)(void*), void *arg)
{
    *handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)target, arg, 0, NULL);
    return *handle != NULL;
}

void joinThread(THREAD_PTR *handle, void **exitVal)
{
    /* wait for the thread to exit */
    WaitForSingleObject(handle, INFINITE);
    GetExitCodeThread(handle, (DWORD*)exitVal);
    CloseHandle(handle);
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
