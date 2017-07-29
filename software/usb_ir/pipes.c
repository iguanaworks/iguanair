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
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "pipes.h"
#include "logging.h"

/* local variables */
static mode_t devMode = 0777;

static bool mkdirs(char *path)
{
    bool retval = false;
    char *slash;

    slash = strrchr(path, '/');
    if (slash == NULL)
        retval = true;
    else
    {
        slash[0] = '\0';
        while(true)
        {
            /* make the new directory */
            if (mkdir(path,
                      S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0)
                retval = true;
            /* try to create the parent path if that was the problem */
            else if (errno == ENOENT && mkdirs(path))
                continue;

            break;
        }
        slash[0] = '/';
    }

    return retval;
}

PIPE_PTR createServerPipe(const char *name, char **addrStr)
{
    int sockfd, attempt = 0;
    struct sockaddr_un server = {0};
    bool retry = true;

    /* generate the server address */
    server.sun_family = PF_UNIX;
    socketName(name, server.sun_path, sizeof(server.sun_path));

    while(retry)
    {
        retry = false;
        attempt++;

        sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
#if DEBUG
message(LOG_WARN, "OPEN %d %s(%d)\n", sockfd,   __FILE__, __LINE__);
#endif

        if (sockfd == -1)
            message(LOG_ERROR, "failed to create server socket.\n");
        else if (bind(sockfd, (struct sockaddr*)&server,
                      sizeof(struct sockaddr_un)) == -1)
        {
            if (errno == EADDRINUSE)
            {
                /* check that the socket has something listening */
                int testconn;
                testconn = connectToPipe(name);
                if (testconn == -1 && errno == ECONNREFUSED && attempt == 1)
                {
                    /* if not, try unlinking the pipe and trying again */
                    unlink(server.sun_path);
                    retry = true;
                }
                else
                {
                    /* guess someone is there, whoops, close and complain */
                    closePipe(testconn);
                    message(LOG_ERROR, "failed to bind server socket %s.  Is the address currently in use?\n", server.sun_path);
                }
            }
            /* attempt to make the directory if we get ENOENT */
            else if (errno == ENOENT && mkdirs(server.sun_path))
                retry = true;
            else
                message(LOG_ERROR, "failed to bind server socket: %s\n",
                        translateError(errno));
        }
        /* start listening */
        else if (listen(sockfd, 5) == -1)
            message(LOG_ERROR,
                    "failed to put server socket in a listening state.\n");
        /* set the proper permissions */
        else if (chmod(server.sun_path, devMode) != 0)
            message(LOG_ERROR,
                    "failed to set permissions on the server socket.\n");
        else
        {
            if (addrStr != NULL)
                *addrStr = strdup(server.sun_path);
            return sockfd;
        }
        close(sockfd);
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", sockfd, __FILE__, __LINE__);
#endif
    }

    return INVALID_PIPE;
}

void closeServerPipe(PIPE_PTR fd, const char *name)
{
    char path[PATH_MAX];

    /* figure out the name */
    socketName(name, path, PATH_MAX);

    /* and nuke it */
    unlink(path);
    close(fd);
}

void socketName(const char *name, char *buffer, unsigned int length)
{
    /* left in case there is some daemon functionality that does not
       fit well with simple signalling */
    if (name == NULL)
        snprintf(buffer, length, "%sctl", IGSOCK_NAME);
    /* based on some people's usage if would be nice to allow full
       paths to the socket to be specified. */
    else if (strchr(name, '/') != NULL)
        strncpy(buffer, name, length);
    else
        snprintf(buffer, length, "%s%s", IGSOCK_NAME, name);
}

PIPE_PTR connectToPipe(const char *name)
{
    PIPE_PTR retval = INVALID_PIPE;
    struct sockaddr_un server = {0};

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

void setAlias(const char *target, bool deleteAll, const char *alias)
{
    /* prepare the device index string */
    if (deleteAll)
    {
        DIR_HANDLE dir = NULL;
        char buffer[PATH_MAX];

        /* examine symlinks in the dir and delete links to target */
        strcpy(buffer, IGSOCK_NAME);
        while((dir = findNextFile(dir, buffer)) != NULL)
        {
            char ptr[PATH_MAX], buf[PATH_MAX];
            int length;

            sprintf(buf, "%s%s", IGSOCK_NAME, buffer);
            length = readlink(buf, ptr, PATH_MAX - 1);
            if (length > 0)
            {
                ptr[length] = '\0';
                if (strcmp(target, ptr) == 0)
                    unlink(buf);
            }
        }
    }

    /* create a new symlink from alias to name */
    if (alias != NULL)
    {
        char path[PATH_MAX], *slash, *aliasCopy;
        struct stat st;

        aliasCopy = strdup(alias);
        while(1)
        {
            slash = strchr(aliasCopy, '/');
            if (slash == NULL)
                break;
            slash[0] = '|';
        }
        socketName(aliasCopy, path, PATH_MAX);
        free(aliasCopy);

        if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode))
        {
            if (unlink(path) != 0)
                message(LOG_ERROR, "failed to unlink old alias: %s\n",
                        translateError(errno));
        }
        if (symlink(target, path) != 0)
                message(LOG_ERROR, "failed to symlink alias: %s\n",
                        translateError(errno));
    }
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
