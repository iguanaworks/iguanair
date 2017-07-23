/****************************************************************************
 ** daemon.c ****************************************************************
 ****************************************************************************
 *
 * Source for the igdaemon application that manages access to the
 * underlying USB device.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include "iguanaIR.h"
#include "version.h"
#include "compat.h"

#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <argp.h>

#include "driver.h"
#include "pipes.h"
#include "device-interface.h"
#include "client-interface.h"
#include "server.h"

#ifdef __APPLE__
extern int darwin_hotplug(const usbId *);
#endif

/* we need to call connect without doing a version check when we're
 * trying to detect another igdaemon */
IGUANAIR_API PIPE_PTR iguanaConnect_internal(const char *name, unsigned int protocol, bool checkVersion);

/* local variables */
static mode_t devMode = 0777;

struct parameters
{
    bool runAsDaemon;
    const char *pidFile;
};

static void quitHandler(int UNUSED(sig))
{
    triggerCommand((THREAD_PTR)QUIT_TRIGGER);
}

static void scanHandler(int UNUSED(sig))
{
    triggerCommand((THREAD_PTR)SCAN_TRIGGER);
}

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

static int startListening(const char *name)
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
                testconn = iguanaConnect_internal(name, IG_PROTOCOL_VERSION, false);
                if (testconn == -1 && errno == ECONNREFUSED && attempt == 1)
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
            return sockfd;
        close(sockfd);
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", sockfd, __FILE__, __LINE__);
#endif
    }

    return INVALID_PIPE;
}

static void stopListening(int fd, const char *name)
{
    char path[PATH_MAX];

    /* figure out the name */
    socketName(name, path, PATH_MAX);

    /* and nuke it */
    unlink(path);
    close(fd);
}

static void workLoop()
{
    deviceList *list;

    /* initialize the driver, signals, and device list */
    if ((list = initServer(&srvSettings)) == NULL)
        message(LOG_ERROR, "failed to initialize the device list.\n");
    else
    {
        if (signal(SIGINT, quitHandler) == SIG_ERR)
            message(LOG_ERROR, "failed to install SIGINT handler.\n");
        else if (signal(SIGTERM, quitHandler) == SIG_ERR)
            message(LOG_ERROR, "failed to install SIGTERM handler.\n");
        else if (signal(SIGHUP, scanHandler) == SIG_ERR)
            message(LOG_ERROR, "failed to install SIGHUP handler.\n");
        else if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
            message(LOG_ERROR, "failed to ignore SIGPIPE messages.\n");
        else
        {
            bool quit = false;

            /* trigger the initial device scan */
            scanHandler(SIGHUP);

#ifdef __APPLE__
            /* Support hot plug in on Mac OS X */
            if (darwin_hotplug(usbIds) != 0)
                message(LOG_ERROR, "Failed to enable hotplug\n");
#endif

            /* loop, waiting for commands */
            while(! quit)
            {
                THREAD_PTR thread = INVALID_THREAD_PTR;
                void *exitVal;

                /* read a command and check for error */
                if (readPipe(srvSettings.commPipe[READ], &thread,
                             sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
                {
                    message(LOG_ERROR,
                            "CommPipe read failed: %s\n", translateError(errno));
                    quit = true;
                }
                /* threads trigger a join by telling the main thread their id */
                else if (thread != INVALID_THREAD_PTR)
                    joinThread(thread, &exitVal);
                /* read the actual command (came from a signal handler) */
                else if (readPipe(srvSettings.commPipe[READ], &thread,
                                  sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
                {
                    message(LOG_ERROR,
                            "Command read failed: %s\n", translateError(errno));
                    quit = true;
                }
                /* handle the shutdown command */
                else if (thread == (THREAD_PTR)QUIT_TRIGGER)
                    quit = true;
                /* complain about unknown commands */
                else if (thread != (THREAD_PTR)SCAN_TRIGGER)
                    message(LOG_ERROR,
                            "Unknown command from commPipe: %d\n", thread);
                /* handle the scan/rescan command */
                else
                {
                    if (srvSettings.justDescribe)
                        message(LOG_NORMAL, "Detected Iguanaworks devices:\n");
                    if (! updateDeviceList(list))
                        message(LOG_ERROR, "scan failed.\n");
                    if (srvSettings.justDescribe)
                        break;
                }
            }

            /* wait for all the workers to finish */
            reapAllChildren(list);
        }

        /* release any server-side resources on the way out */
        cleanupServer();
    }
}

enum
{
    ARG_RECV_TIMEOUT = LAST_BASE_ARG,
    ARG_SEND_TIMEOUT,
    ARG_UNBIND,
    ARG_HANDLE_EPIPE,
    ARG_DEVICELIST,
    ARG_NO_THREADS
};

static struct argp_option options[] =
{
    /* iguanaworks specific options */
    { NULL, 0, NULL, 0, "Operating system specific options:", OS_GROUP },
#ifndef __APPLE__
    { "no-daemon",       'n',              NULL,     0, "Do not fork into the background.",                                 OS_GROUP },
#endif
    { "receive-timeout", ARG_RECV_TIMEOUT, "MSTIME", 0, "Specify the device receive timeout.",                              OS_GROUP },
    { "send-timeout",    ARG_SEND_TIMEOUT, "MSTIME", 0, "Specify the device send timeout.",                                 OS_GROUP },
    { "auto-unbind",     ARG_UNBIND,       NULL,     0, "Attempt to unbind busy devices.  Use with caution.",               OS_GROUP },
    { "no-ignore-epipe", ARG_HANDLE_EPIPE, NULL,     0, "Disconnect on EPIPE errors.  Default is to ignore spurious errors generated by some hardware.", OS_GROUP },
    { "devices",         ARG_DEVICELIST,   NULL,     0, "Implies --no-daemon.  List information about connected devices.",  OS_GROUP },
#ifdef LIBUSB_NO_THREADS_OPTION
    { "no-threads", ARG_NO_THREADS, NULL, 0, "Do not allow two threads to both access libusb calls at the same time.  Only useful for versions of libusb < 1.0", OS_GROUP },
#endif
    { "pid-file",        'p',              "FILE",   0, "Specify where to write the pid of the daemon process.",            OS_GROUP },    

    { NULL, 0, NULL, 0, "Help related options:", HELP_GROUP },

    /* end of table */
    {0}
};

static error_t parseOption(int key, char *arg, struct argp_state *state)
{
    struct parameters *params = (struct parameters*)state->input;
    switch(key)
    {
#ifndef __APPLE__
    case 'n':
        params->runAsDaemon = false;
        break;
#endif

    case ARG_RECV_TIMEOUT:
    {
        char *end;
        long int res = strtol(arg, &end, 0);
        if (arg[0] == '\0' || end[0] != '\0' || res < 0 || res > 10000 )
        {
            argp_error(state, "Receive timeout requires a numeric argument between 0 and 10000\n");
            return ARGP_HELP_STD_ERR;
        }
        else
            srvSettings.devSettings.recvTimeout = res;
        break;
    }

    case ARG_SEND_TIMEOUT:
    {
        char *end;
        long int res = strtol(arg, &end, 0);
        if (arg[0] == '\0' || end[0] != '\0' || res < 0 || res > 10000 )
        {
            argp_error(state, "Send timeout requires a numeric argument between 0 and 10000\n");
            return ARGP_HELP_STD_ERR;
        }
        else
            srvSettings.devSettings.sendTimeout = res;
        break;
    }

    case ARG_UNBIND:
        srvSettings.unbind = true;
        break;

    case ARG_HANDLE_EPIPE:
        srvSettings.devSettings.disconnectOnEPipe = true;
        break;

    case ARG_DEVICELIST:
        params->runAsDaemon = false;
        srvSettings.justDescribe = true;
        break;

#ifdef LIBUSB_NO_THREADS_OPTION
    case ARG_NO_THREADS:
        srvSettings.noThreads = true;
        break;
#endif

    case 'p':
        params->pidFile = arg;
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp parser = {
    options,
    parseOption,
    NULL,
    "This daemon acts as a user-space driver controlling access to IguanaIR devices.\n",
    NULL,
    NULL,
    NULL
};

int main(int argc, char **argv)
{
    int retval = 0;
    struct parameters params;
    struct argp_child children[3];

    /* initialize the parameters structure */
    params.runAsDaemon = true;
    params.pidFile = NULL;

    /* initialize the server-level settings */
    initServerSettings();

    /* include the log and base argument parsers */
    memset(children, 0, sizeof(struct argp_child) * 3);
    children[0].argp = baseArgParser();
    children[1].argp = logArgParser();
    parser.children = children;

    /* parse the cmd line args */
    if (argp_parse(&parser, argc, argv, 0, NULL, &params) != 0)
        retval = 3;

#ifndef __APPLE__
    /* run as a daemon if requested and possible */
    if (retval == 0 && params.runAsDaemon)
    {
        message(LOG_DEBUG, "Forking into the background.\n");
        if (daemon(0, 0) == 0)
            umask(0);
        else
        {
            message(LOG_ERROR, "daemon() failed: %s\n", translateError(errno));
            retval = 1;
        }
    }
#endif

    /* write the pid out if requested */
    if (retval == 0 && params.pidFile != NULL)
    {
        FILE *pf;
        pf = fopen(params.pidFile, "w");
        if (pf == NULL)
        {
            message(LOG_ERROR, "Failed to open pid file.\n");
            retval = 2;
        }
        else
        {
            fprintf(pf, "%d\n", getpid());
            fclose(pf);
        }
    }

    /* if startup succeeded wait for user signals */
    if (retval == 0)
        workLoop();

    return retval;
}

void listenToClients(const char *name, listHeader *clientList, iguanaDev *idev)
{
    PIPE_PTR listener;

    /* start the listener */
    listener = startListening(name);
    if (listener == INVALID_PIPE)
        message(LOG_ERROR, "Worker failed to start listening.\n");
    else
    {
        fd_set fds, fdsin, fdserr;

        /* check the initial aliases */
        if (idev != NULL)
            getID(idev);

        /* loop while checking the pipes for activity */
        FD_ZERO(&fdsin);
        FD_ZERO(&fdserr);
        while(true)
        {
            PIPE_PTR reader;
            client *john;
            int max = 0;
            FD_ZERO(&fds);

            /* the reader is either feedback from a device or a way to
               cleanly shutdown */
            if (idev == NULL)
                reader = srvSettings.ctlSockPipe[READ];
            else
                reader = idev->readerPipe[READ];

            /* check the read pipe for error */
            if (FD_ISSET(reader, &fdserr))
                break;

            /* take care of messages from the reader */
            if (FD_ISSET(reader, &fdsin) &&
                (idev == NULL || ! handleReader(idev)))
                break;
            FD_SET(reader, &fds);
            max = reader;

            /* check the listener for error */
            if (FD_ISSET(listener, &fdserr))
                break;

            /* next handle incoming connections */
            if (FD_ISSET(listener, &fdsin))
                clientConnected(accept(listener, NULL, NULL), clientList, idev);
            FD_SET(listener, &fds);
            if (listener > max)
                max = listener;

            /* last check the clients */
            for(john = (client*)clientList->head; john != NULL;)
            {
                client *next;

                next = (client*)john->header.next;

                if ((! FD_ISSET(john->fd, &fdserr) &&
                     ! FD_ISSET(john->fd, &fdsin)) ||
                    handleClient(john))
                {
                    FD_SET(john->fd, &fds);
                    if (john->fd > max)
                        max = john->fd;
                }

                john = next;
            }

            /* wait until there is data ready */
            fdsin = fdserr = fds;
            if (select(max + 1, &fdsin, NULL, &fdserr, NULL) < 0)
            {
                message(LOG_ERROR,
                        "select failed: %s\n", translateError(errno));
                break;
            }
        }

        /* unlink any existing aliases */
        if (idev != NULL)
            setAlias(idev, true, NULL);
        stopListening(listener, name);
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", listener, __FILE__, __LINE__);
#endif

        /* and release any connected clients */
        while(clientList->count > 0)
            releaseClient((client*)clientList->head);
    }
}

void setAlias(iguanaDev *idev, bool deleteAll, const char *alias)
{
    /* prepare the device index string */
    char idxStr[4];
    sprintf(idxStr, "%d", idev->usbDev->id);

    if (deleteAll)
    {
        DIR_HANDLE dir = NULL;
        char buffer[PATH_MAX];

        /* examine symlinks in the dir and delete links to idxStr */
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
                if (strcmp(idxStr, ptr) == 0)
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
        if (symlink(idxStr, path) != 0)
                message(LOG_ERROR, "failed to symlink alias: %s\n",
                        translateError(errno));
    }
}
