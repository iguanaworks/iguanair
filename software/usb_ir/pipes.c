/****************************************************************************
 ** pipes.c *****************************************************************
 ****************************************************************************
 *
 * Provides an implementation of low level communication over Unix
 * sockets.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */

#include "iguanaIR.h"
#include "compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "pipes.h"
#include "support.h"

void socketName(const char *name, char *buffer, unsigned int length)
{
    /* based on some people's usage if would be nice to allow full
       paths to the socket to be specified. */
    if (strchr(name, '/') != NULL)
        strncpy(buffer, name, length);
    /* left in case there is some daemon functionality that does not
       fit well with simple signalling */
    else if (name == NULL)
        snprintf(buffer, length, "%sctl", IGSOCK_NAME);
    else
        snprintf(buffer, length, "%s%s", IGSOCK_NAME, name);
}

PIPE_PTR connectToPipe(const char *name)
{
    PIPE_PTR retval = INVALID_PIPE;
    struct sockaddr_un server;

    /* generate the server address */
    server.sun_family = PF_UNIX;
    socketName(name, server.sun_path, sizeof(server.sun_path));

    /* connect to the server */
    retval = socket(PF_UNIX, SOCK_STREAM, 0);
    if (retval != INVALID_PIPE)
    {
        if (connect(retval,
                    (struct sockaddr *)&server,
                    sizeof(struct sockaddr_un)) == -1)
        {
#if DEBUG
printf("CLOSE %d %s(%d)\n", retval, __FILE__, __LINE__);
#endif
            closePipe(retval);
            retval = INVALID_PIPE;
        }
    }

    return retval;
}

int readPipeTimed(PIPE_PTR fd, char *buffer, int size, int timeout)
{
    int retval = -1;
    fd_set fdsin, fdserr;
    struct timeval tv = {0,0}, *tvp = NULL;
    int64_t stoptime = (int64_t)microsSinceX() + timeout * 1000;

    /* prepare the fds for the select call */
    eintr_loop:
    FD_ZERO(&fdsin);
    FD_SET(fd, &fdsin);
    fdserr = fdsin;

    /* configure the timeout if there was one*/
    if (timeout >= 0)
    {
        int64_t timeremaining = stoptime - (int64_t)microsSinceX();
        tvp = &tv;
        if (timeremaining < 0) timeremaining = 0;
        tv.tv_sec = timeremaining / 1000000;
        tv.tv_usec = timeremaining % 1000000;
    }

    switch(select(fd + 1, &fdsin, NULL, &fdserr, tvp))
    {
    /* error */
    case -1:
        if (errno == EINTR)
            /* I hate goto, but it's the right move here */
            goto eintr_loop;
        break;

    /* timeout */
    case 0:
        retval = 0;
        break;

    default:
        if (FD_ISSET(fd, &fdsin))
        {
            int goal = size;

            retval = 0;
            while(goal > 0)
            {
                int amount = readPipe(fd, buffer + retval, goal);
                switch(amount)
                {
                case -1:
                    retval = -1;
                case 0:
                    /* break out on error or EOF */
                    if (retval == 0)
                    {
                        retval = -1;
                        errno = EPIPE;
                    }
                    goal = 0;
                    break;

                default:
                    retval += amount;
                    goal -= amount;
                }
            }
        }
        else
            errno = EIO;
        break;
    }
    return retval;
}

int notified(PIPE_PTR fd, int timeout)
{
    char byte;
    return readPipeTimed(fd, &byte, 1, timeout);
}

bool notify(PIPE_PTR fd)
{
    char byte = '\n';
    return writePipe(fd, &byte, 1) == 1;
}
