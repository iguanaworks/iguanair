/****************************************************************************
 ** support.c ***************************************************************
 ****************************************************************************
 *
 * Basic functions and definitions for I/O, both for logging and
 * device communication.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#include "base.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#include "pipes.h"
#include "support.h"
#include "iguanaIR.h"

#define NO_TERMINATOR -1

/* variables concerning the communication pipes */
static char *devRoot = "/dev/iguanaIR";
static mode_t devMode = 0777;

static char *msgPrefixes[] =
{
    "FATAL: ",
    "ERROR: ",
    "WARNING: ",
    "", /* NORMAL gets no prefix */
    "INFO: ",
    "DEBUG: ",
    "DEBUG2: ",
    "DEBUG3: "
};

/* logging variables */
static int currentLevel = LOG_NORMAL;
static FILE *log = NULL;

/* Exit the application with proper cleanup. */
void dieCleanly(int level)
{
    /* exit with appropriate value */
    if (level == LOG_FATAL)
        exit(1);
    exit(0);
}

void changeLogLevel(int difference)
{
    currentLevel += difference;
    if (currentLevel < LOG_FATAL)
        currentLevel = LOG_FATAL;
}

void openLog(const char *filename)
{
    if (log != NULL)
        fclose(log);
    log = NULL;

    if (strcmp(filename, "-") != 0)
    {
        log = fopen(filename, "a");
        if (log != NULL)
            setlinebuf(log);
    }
}

static FILE* pickStream(int level)
{
    FILE *out = NULL;

    if (level <= currentLevel ||
        level == LOG_ALWAYS)
    {
        /* if logfile is open print to it instead */
        if (log != NULL)
            out = log;
        else if (level <= LOG_WARN)
            out = stderr;
        else
            out = stdout;
    }

    return out;
}

bool wouldOutput(int level)
{
    return pickStream(level) != NULL;
}

/* Print a message to a certain debug level */
int message(int level, char *format, ...)
{
    va_list list;
    int retval = 0;
    FILE *out;

    va_start(list, format);
    out = pickStream(level);
    if (out != NULL)
    {
        char *buffer;
        if (level != LOG_ALWAYS && level != LOG_NORMAL)
        {
            char when[26];
            time_t now;

            /* figure out the timestamp */
            now = time(NULL);
            ctime_r(&now, when);
            when[20] = '\0';

            buffer = (char*)malloc(strlen(when) + \
                                   strlen(msgPrefixes[level]) + \
                                   strlen(format) + 1);
            if (buffer == NULL)
            {
                perror("FATAL: message format malloc failedx");
                exit(2);
            }
            sprintf(buffer, "%s%s%s", when, msgPrefixes[level], format);
        }
        else
            buffer = format;

        retval = vfprintf(out, buffer, list);
        /* free the format if it ws allocated */
        if (buffer != format)
            free(buffer);
    }
    va_end(list);

    if (level <= LOG_FATAL)
        dieCleanly(level);

    return retval;
}

void appendHex(int level, void *location, unsigned int length)
{
    FILE *out;
    int retval = 0;

    out = pickStream(level);
    if (out != NULL)
    {
        unsigned int x;

        retval = fprintf(out, "0x");
        for(x = 0; x < length; x++)
            retval += fprintf(out, "%2.2x", ((unsigned char*)location)[x]);
        retval += fprintf(out, "\n");
    }
}

void socketName(const char *name, char *buffer, unsigned int length)
{
    /* based on some people's usage if would be nice to allow full
     * paths to the socket to be specified. */
    if (strchr(name, '/') != NULL)
        strncpy(buffer, name, length);
    /* left in case there is some daemon functionality that does not
     * fit well with simple signalling */
    else if (name == NULL)
        snprintf(buffer, length, "%s/ctl", devRoot);
    else
        snprintf(buffer, length, "%s/%s", devRoot, name);
}

int startListening(const char *name, const char *alias)
{
    int sockfd, attempt = 0;
    struct sockaddr_un server;
    bool retry = true;

    /* generate the server address */
    server.sun_family = PF_UNIX;
    socketName(name, server.sun_path, sizeof(server.sun_path));

    while(retry)
    {
        retry = false;
        attempt++;

        sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1)
            message(LOG_ERROR, "failed to create server socket.\n");
        else if (bind(sockfd, (struct sockaddr*)&server,
                      sizeof(struct sockaddr_un)) == -1)
        {
            if (errno == EADDRINUSE)
            {
                /* check that the socket has something listening */
                int testconn;
                testconn = iguanaConnect(name);
                if (testconn == -1 && errno == 111 && attempt == 1)
                {
                    /* if not, try unlinking the pipe and trying again */
                    unlink(server.sun_path);
                    retry = true;
                }
                else
                {
                    /* guess someone is there, whoops, close and complain */
                    iguanaClose(testconn);
                    message(LOG_ERROR, "failed to bind server socket %s.  Is the address currently in use?\n", server.sun_path);
                }
            }
            else
                message(LOG_ERROR,
                        "failed to bind server socket: %s\n", strerror(errno));
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

                if (lstat(path, &st) == 0 &&
                    S_ISLNK(st.st_mode))
                    unlink(path);
                symlink(name, path);
            }

            return sockfd;
        }
        close(sockfd);
    }

    return -1;
}

void stopListening(int fd, const char *name, const char *alias)
{
    char path[PATH_MAX], ptr[PATH_MAX];
    int length;

    /* figure out the name */
    socketName(name, path, PATH_MAX);

    /* and nuke it */
    unlink(path);
    close(fd);
 
    /* find the alias and nuke it if it is a link to the name */
    if (alias != NULL)
    {
        socketName(alias, path, PATH_MAX);
        length = readlink(path, ptr, PATH_MAX - 1);
        if (length > 0)
        {
            ptr[length] = '\0';
            if (strcmp(name, ptr) == 0)
                unlink(path);
        }
    }
}

static int readAnyDataBlock(int *fds, int count, int timeout, int terminator,
                            int *index, char *buffer, int size)
{
    int retval = -1, x, max = -1;
    fd_set fdsin, fdserr;
    struct timeval tv = {0,0}, *tvp = NULL;

    /* configure the timeout if there was one*/
    if (timeout >= 0)
    {
        tv.tv_usec = timeout * 1000;
        tvp = &tv;
    }

    /* prepare the fdsets */
    FD_ZERO(&fdsin);
    for(x = 0; x < count; x++)
    {
        FD_SET(fds[x], &fdsin);
        if (fds[x] > max)
            max = fds[x];
    }
    fdserr = fdsin;

    /* select and check the return value */
    switch(select(max + 1, &fdsin, NULL, &fdserr, tvp))
    {
    case 0:
        retval = 0;
    case -1:
        if (index != NULL)
            *index = -1;
        break;

    default:
        for(x = 0; x < count; x++)
            if (FD_ISSET(fds[x], &fdserr) ||
                FD_ISSET(fds[x], &fdsin))
            {
                if (index != NULL)
                    *index = x;

                if (FD_ISSET(fds[x], &fdsin))
                {
                    /* try to read all at once */
                    int goal = 1;
                    if (terminator == NO_TERMINATOR)
                        goal = size;

                    retval = 0;
                    tv.tv_sec = 0;
                    tv.tv_usec = 0;
                    while(goal > 0)
                    {
                        int amount = readPipe(fds[x], buffer + retval, goal);
                        switch(amount)
                        {
                        case -1:
                        case 0:
                            /* break out on error or EOF */
                            goal = 0;
                            break;

                        default:
                            retval += amount;
                            /* if there is no terminator,
                               subtract from goal */
                            if (terminator == NO_TERMINATOR)
                                goal -= amount;
                            /* otherwise check for the terminator */
                            else if (buffer[retval - 1] == terminator)
                                goal = 0;
                        }
                    }
                }
                break;
            }
        break;
    }

    return retval;
}

int readBytes(int fd, int timeout,
              char *buffer, int size)
{
    return readAnyDataBlock(&fd, 1, timeout, NO_TERMINATOR,
                            NULL, buffer, size);
}

int notified(int fd, int timeout)
{
    char byte;
    return readAnyDataBlock(&fd, 1, timeout, NO_TERMINATOR,
                            NULL, &byte, 1);
}

bool notify(int fd)
{
    char byte = '\n';
    return writePipe(fd, &byte, 1) == 1;
}

int checkFD(int fd, fdSets *fds)
{
    int retval = 0;

    if (FD_ISSET(fd, &fds->err))
        retval = -1;
    else
    {
        if (FD_ISSET(fd, &fds->in))
            retval = 1;

        FD_SET(fd, &fds->next);
        if (fds->max < fd)
            fds->max = fd;
    }

    return retval;
}
