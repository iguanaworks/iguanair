/**************************************************************************
 * pipes.c **************************************************************
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
#include "iguanaIR.h"
#include "compat.h"

#include <stdio.h>

#include "pipes.h"
#include "support.h"

#define ANONYMOUS_NAME IGSOCK_NAME "INTERNAL"

bool createPipePair(PIPE_PTR *pair)
{
#if 1
    bool retval = false;

    /* create the pipe at a known (stupid) name */
    pair[READ] = CreateNamedPipe(ANONYMOUS_NAME,
                                 PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                 PIPE_UNLIMITED_INSTANCES,
                                 64, 64, NMPWAIT_USE_DEFAULT_WAIT, NULL);
    if (pair[READ] != INVALID_HANDLE_VALUE)
    {
        OVERLAPPED server;
        memset(&server, 0, sizeof(OVERLAPPED));
        server.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        /* server starts waiting now */
        ConnectNamedPipe(pair[READ], &server);

        /* child connects to the server */
        pair[WRITE] = CreateFile(ANONYMOUS_NAME,
                                 GENERIC_WRITE, 
                                 0,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_FLAG_OVERLAPPED,
                                 NULL);
        if (pair[WRITE] != INVALID_HANDLE_VALUE)
        {
            /* ensure server side also sees a connection */
            WaitForSingleObject(server.hEvent, INFINITE);
            CloseHandle(server.hEvent);
            retval = true;
        }
        else
        {
            CancelIo(pair[READ]);
            CloseHandle(pair[READ]);
        }
    }

    return retval;
#else
    return (CreatePipe(pair + READ, pair + WRITE, NULL, 0) == TRUE);
#endif
}

int readPipeTimed(PIPE_PTR fd, char *buf, int count, int timeout)
{
    int retval = -1;
    OVERLAPPED over = { (ULONG_PTR)NULL };
    DWORD read;

    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ReadFile(fd, buf, count, NULL, &over) == TRUE)
        retval = count;
    else if (GetLastError() == ERROR_HANDLE_EOF)
        retval = 0;
    else if (GetLastError() == ERROR_IO_PENDING)
        switch(WaitForSingleObject(over.hEvent, timeout))
        {
        case WAIT_OBJECT_0:
            GetOverlappedResult(fd, &over, &read, TRUE);
            retval = read;
            break;

        case WAIT_TIMEOUT:
            errno = ERROR_TIMEOUT;
            /* cancel the IO so it doesn't consume later messages */
            CancelIo(fd);
            /* see if it completed JUST now */
            if (GetOverlappedResult(fd, &over, &read, TRUE) == S_OK)
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

int readBytes(PIPE_PTR fd, int timeout,
              char *buffer, int size)
{
    return readPipeTimed(fd, buffer, size, timeout);
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
