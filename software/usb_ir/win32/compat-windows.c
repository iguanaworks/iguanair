/**************************************************************************
 * compat-windows.c *******************************************************
 **************************************************************************
 *
 * A few functions must be implemented for compatibility.  This is the
 * Windows version.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "iguanaIR.h"
#include "compat.h"

#include <errno.h>
#include <stdio.h>

bool startThread(THREAD_PTR *handle, void* (*target)(void*), void *arg)
{
    *handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)target, arg, 0, NULL);
    return *handle != NULL;
}

void joinThread(THREAD_PTR *handle, void **exitVal)
{
    /* wait for the thread to exit */
    WaitForSingleObject(handle, INFINITE);
    if (exitVal != NULL)
        GetExitCodeThread(handle, (DWORD*)exitVal);
    CloseHandle(handle);
}

uint64_t microsSinceX()
{
    uint64_t retval = 0;
    LARGE_INTEGER freq, count;

    if (QueryPerformanceFrequency(&freq) &&
        QueryPerformanceCounter(&count))
        retval = count.QuadPart * 1000000 / freq.QuadPart;

    return retval;
}

bool setNonBlocking(PIPE_PTR pipe)
{
    DWORD mode = PIPE_READMODE_BYTE | PIPE_WAIT;
//    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	if (SetNamedPipeHandleState(pipe, &mode, NULL, NULL))
        return true;
    return false;
}

/* translate errno, or if the errnum == -1 translate GetLastError()  */
char globalBuffer[256];
char* translateError(int errnum)
{
    if (errnum == ETIMEDOUT)
        strcpy(globalBuffer, "Connection timed out");
    else if (errnum >= 0)
        strcpy(globalBuffer, strerror(errnum));
    else if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL, GetLastError(), 0, globalBuffer, 256, NULL) == 0)
        sprintf(globalBuffer, "FormatMessage failed to translate %d", errnum);

    return globalBuffer;
}

DIR_HANDLE findNextFile(DIR_HANDLE hFind, char *buffer)
{
    WIN32_FIND_DATA findFileData;

    if (hFind == NULL)
    {
        strcat(buffer, "\\*");
        hFind = FindFirstFile(buffer, &findFileData);
        if (hFind == INVALID_HANDLE_VALUE) 
            hFind = NULL;
    }
    else if (! FindNextFile(hFind, &findFileData))
    {
        FindClose(hFind);
        hFind = NULL;
    }

    if (hFind != NULL)
        strcpy(buffer, findFileData.cFileName);
    return hFind;
}
