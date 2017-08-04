/**************************************************************************
 * pipes.c **************************************************************
 **************************************************************************
 *
 * Implementation of the basic pipe functions, but we rely on OS
 * specific implementations as well.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "iguanaIR.h"
#include "compat.h"
#include "pipes.h"

#include <stdio.h> /* sprintf */
#include <errno.h>

bool createPipePair(PIPE_PTR *pair)
{
// TODO: seems like this function should just be a call to CreatePair, but that does not work
// return (CreatePipe(pair + READ, pair + WRITE, NULL, 0) == TRUE);
    bool retval = false;
    char buf[PATH_MAX];

    /* create the pipe at a known (stupid) name */
    sprintf(buf, "%sINTERNAL-%p", IGSOCK_NAME, pair);
    pair[READ] = CreateNamedPipe(buf,
                                 PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                 PIPE_UNLIMITED_INSTANCES,
                                 64, 64, NMPWAIT_USE_DEFAULT_WAIT, NULL);
    if (pair[READ] != INVALID_HANDLE_VALUE)
    {
        OVERLAPPED over;
        memset(&over, 0, sizeof(OVERLAPPED));
        over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        /* server starts waiting now */
        ConnectNamedPipe(pair[READ], &over);

        /* child connects to the server */
        pair[WRITE] = CreateFile(buf,
                                 GENERIC_WRITE, 
                                 0,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_FLAG_OVERLAPPED,
                                 NULL);
        if (pair[WRITE] != INVALID_HANDLE_VALUE)
        {
            /* ensure server side also sees a connection */
            WaitForSingleObject(over.hEvent, INFINITE);
            retval = true;
        }
        else
        {
            CancelIo(pair[READ]);
            CloseHandle(pair[READ]);
        }
        CloseHandle(over.hEvent);
    }

    return retval;
}

int readPipeTimed(PIPE_PTR fd, char *buf, int count, int timeout)
{
    int retval = -1;
    OVERLAPPED over = { (ULONG_PTR)NULL };
    DWORD read;

    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ReadFile(fd, buf, count, NULL, &over) == TRUE)
        retval = count;
    else if (GetLastError() == ERROR_HANDLE_EOF ||
             GetLastError() == ERROR_BROKEN_PIPE)
        retval = 0;
    else if (GetLastError() == ERROR_IO_PENDING)
        switch(WaitForSingleObject(over.hEvent, timeout))
        {
        case WAIT_TIMEOUT:
            errno = ETIMEDOUT;
            /* cancel the IO so it doesn't consume later messages */
            CancelIo(fd);
            /* see if it completed JUST now */
        case WAIT_OBJECT_0:
            if (GetOverlappedResult(fd, &over, &read, TRUE))
                retval = read;
            break;

        default:
            OutputDebugString("WaitForSingleObject failed.\n");
            break;
        }
    CloseHandle(over.hEvent);

    return retval;
}

int readPipe(PIPE_PTR fd, void *buf, int count)
{
    return readPipeTimed(fd, buf, count, INFINITE);
}

int writePipe(PIPE_PTR fd, const void *buf, int count)
{
    DWORD written;
    if (WriteFile(fd, buf, count, &written, NULL) == TRUE)
        return written;
    return -1;
}

int notified(PIPE_PTR fd, int timeout)
{
    char byte;
    return readPipeTimed(fd, &byte, 1, timeout);
}

bool notify(PIPE_PTR fd)
{
    char byte;
    return writePipe(fd, &byte, 1) == 1;
}

void socketName(const char *name, char *buffer, unsigned int length)
{
    snprintf(buffer, length, "%s%s", IGSOCK_NAME, name);
}

PIPE_PTR connectToPipe(const char *name)
{
    PIPE_PTR retval = INVALID_PIPE;
    char buffer[256];

    socketName(name, buffer, 256);
    retval = CreateFile(buffer,
                        GENERIC_READ | GENERIC_WRITE, 
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        NULL);
    if (retval == INVALID_HANDLE_VALUE)
        retval = INVALID_PIPE;

    return retval;
}
