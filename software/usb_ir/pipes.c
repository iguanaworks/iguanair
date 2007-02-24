#include "base.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include "iguanaIR.h"
#include "pipes.h"
#include "support.h"

#define NO_TERMINATOR -1

/* variables concerning the communication pipes */
static mode_t devMode = 0777;

void socketName(const char *name, char *buffer, unsigned int length)
{
    /* based on some people's usage if would be nice to allow full
     * paths to the socket to be specified. */
    if (strchr(name, '/') != NULL)
        strncpy(buffer, name, length);
    /* left in case there is some daemon functionality that does not
     * fit well with simple signalling */
    else if (name == NULL)
        snprintf(buffer, length, "%s/ctl", IGSOCK_NAME);
    else
        snprintf(buffer, length, "%s/%s", IGSOCK_NAME, name);
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
            closePipe(retval);
            retval = INVALID_PIPE;
        }
    }

    return retval;
}

static int readAnyDataBlock(PIPE_PTR *fds, int count, int timeout, int terminator,
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

int readBytes(PIPE_PTR fd, int timeout,
              char *buffer, int size)
{
    return readAnyDataBlock(&fd, 1, timeout, NO_TERMINATOR,
                            NULL, buffer, size);
}

int notified(PIPE_PTR fd, int timeout)
{
    char byte;
    return readBytes(fd, timeout, &byte, 1);
}

bool notify(PIPE_PTR fd)
{
    char byte = '\n';
    return writePipe(fd, &byte, 1) == 1;
}

int checkFD(PIPE_PTR fd, fdSets *fds)
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



bool acceptNewClients(PIPE_PTR listener, fdSets *fds, acceptedFunc accepted, iguanaDev *idev)
{
    bool retval = false;
    client *newClient;

    switch(checkFD(listener, fds))
    {
    /* quit on listener errors */
    case -1:
        message(LOG_ERROR, "error on client listener socket.\n");
        break;

    case 0:
        retval = true;
        break;

    /* add new clients to the list */
    case 1:
        accepted(accept(listener, NULL, NULL), idev)
        break;
    }

    return retval;
}

