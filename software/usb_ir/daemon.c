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
#include "support.h"
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
#if DEBUG
printf("CLOSE %d %s(%d)\n", srvSettings.commPipe[WRITE], __FILE__, __LINE__);
#endif
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
#if DEBUG
printf("CLOSE %d %s(%d)\n", sockfd, __FILE__, __LINE__);
#endif
        close(sockfd);
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
#if DEBUG
printf("CLOSE %d %s(%d)\n", fd, __FILE__, __LINE__);
#endif
    close(fd);
}

static void workLoop()
{
    deviceList *list;
    int ctlSock = INVALID_PIPE;

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
        else if (! srvSettings.justDescribe && (ctlSock = startListening(NULL)) == INVALID_PIPE)
            message(LOG_ERROR, "failed to open the control socket.\n");
        else
        {
            bool quit = false;

#if DEBUG
printf("OPEN %d %s(%d)\n", srvSettings.commPipe[0], __FILE__, __LINE__);
printf("OPEN %d %s(%d)\n", srvSettings.commPipe[1], __FILE__, __LINE__);
#endif

            /* trigger the initial device scan */
            scanHandler(SIGHUP);

#ifdef __APPLE__
            /* Support hot plug in on Mac OS X -- TODO: returns non-zero for error */
            darwin_hotplug(ids);
#endif

            /* loop, waiting for commands */
            while(! quit)
            {
                THREAD_PTR thread = INVALID_THREAD_PTR;
                void *exitVal;

                /* TODO: not doing anything w ctlSock now, but need to accept a cmd:
                            - list - show what clients exist like: 0:1.2:fred
                         and send notices to clients when clients connect and disconnect:
                            - print something like: C:0, D:0?
                 */

                /* wait for a new ctl connection, a command from an
                   existing ctl connection, or a message from an exiting
                   child thread. */

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

            /* close up the server socket */
            stopListening(ctlSock, NULL);
        }

        /* release any server-side resources on the way out */
        cleanupServer();
    }
}

enum
{
    /* generic actions */
    ARG_LOG_LEVEL = 0x100,

    /* igdaemon specific actions */
    ARG_NO_IDS = 0x200,
    ARG_NO_RESCAN,
    ARG_NO_SCANWHEN,
    ARG_NO_THREADS,
    ARG_ONLY_PREFER,
    ARG_DRIVER_DIR,
    ARG_RECV_TIMEOUT,
    ARG_SEND_TIMEOUT,
    ARG_UNBIND,
    ARG_HANDLE_EPIPE,
    ARG_DEVICELIST,

    /* defines for argp */
    GROUP0 = 0
};

static struct argp_option options[] =
{
    /* general daemon options */
    { "log-file",    'l',           "FILE",   0, "Specify a log file (defaults to \"-\").", GROUP0 },
    { "quiet",       'q',           NULL,     0, "Reduce the verbosity.",                   GROUP0 },
    { "verbose",     'v',           NULL,     0, "Increase the verbosity.",                 GROUP0 },
    { "version",     'V',           NULL,     0, "Print the build and version numbers.",    GROUP0 },
    { "log-level",   ARG_LOG_LEVEL, "NUM",    0, "Set the verbosity directly.",             GROUP0 },

    /* iguanaworks specific options */
#ifndef __APPLE__
    { "no-daemon",       'n',              NULL,     0, "Do not fork into the background.",                                 GROUP0 },
#endif
    { "pid-file",        'p',              "FILE",   0, "Specify where to write the pid of the daemon process.",            GROUP0 },
    { "no-auto-rescan",  ARG_NO_RESCAN,    NULL,     0, "Do not automatically rescan the USB bus after device disconnect.", GROUP0 },
    { "scan-timer",      ARG_NO_SCANWHEN,  "SECS",   0, "Periodically rescan the USB bus for new devices instead of waiting for hotplug events.", GROUP0 },
    { "no-ids",          ARG_NO_IDS,       NULL,     0, "Do not query the device for its label.",                           GROUP0 },
    { "no-labels",       ARG_NO_IDS,       NULL,     0, "DEPRECATED: same as --no-ids",                                     GROUP0 },
    { "receive-timeout", ARG_RECV_TIMEOUT, "MSTIME", 0, "Specify the device receive timeout.",                              GROUP0 },
    { "send-timeout",    ARG_SEND_TIMEOUT, "MSTIME", 0, "Specify the device send timeout.",                                 GROUP0 },
    { "auto-unbind",     ARG_UNBIND,       NULL,     0, "Attempt to unbind busy devices.  Use with caution.",               GROUP0 },
    { "no-ignore-epipe", ARG_HANDLE_EPIPE, NULL,     0, "Disconnect on EPIPE errors.  Default is to ignore spurious errors generated by some hardware.", GROUP0 },
    { "devices",         ARG_DEVICELIST,   NULL,     0, "Implies --no-daemon.  List information about connected devices.",  GROUP0 },

#ifdef LIBUSB_NO_THREADS_OPTION
    { "no-threads", ARG_NO_THREADS, NULL, 0, "Do not allow two threads to both access libusb calls at the same time.  Only useful for versions of libusb < 1.0", GROUP0 },
#endif

    /* options specific to the drivers */
    { "driver",         'd',             "DRIVER", 0, "Use this driver in preference to others.  This command can be used multiple times.", GROUP0 },
    { "only-preferred", ARG_ONLY_PREFER, NULL,     0, "Use only drivers specified by the --driver option.",                                 GROUP0 },
    { "driver-dir",     ARG_DRIVER_DIR,  "DIR",    0, "Specify the location of driver objects.",                                            GROUP0 },

    /* end of table */
    {0}
};

static error_t parseOption(int key, char *arg, struct argp_state *state)
{
    struct parameters *params = (struct parameters*)state->input;
    switch(key)
    {
    case 'l':
        openLog(arg);
        break;

    case 'q':
        changeLogLevel(-1);
        break;

    case 'v':
        changeLogLevel(+1);
        break;

    case 'V':
        printf("Software version: %s\n", IGUANAIR_VER_STR("igdaemon"));
        exit(0);
        break;

    case ARG_LOG_LEVEL:
    {
        char *end;
        long int res = strtol(arg, &end, 0);
        if (arg[0] == '\0' || end[0] != '\0' || res < LOG_FATAL || res > LOG_DEBUG3 )
        {
            argp_error(state, "Log level requires a numeric argument between %d and %d\n",
                       LOG_FATAL, LOG_DEBUG3);
            return ARGP_HELP_STD_ERR;
        }
        else
            setLogLevel(res);
        break;
    }

#ifndef __APPLE__
    case 'n':
        params->runAsDaemon = false;
        break;
#endif

    case ARG_NO_IDS:
        srvSettings.readLabels = false;
        break;

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

    case ARG_NO_RESCAN:
        srvSettings.autoRescan = false;
        break;

    case ARG_NO_SCANWHEN:
    {
        char *end;
        long int res = strtol(arg, &end, 0);
        if (arg[0] == '\0' || end[0] != '\0' || res < 0 || res > 3600 )
        {
            argp_error(state, "Scan timeer requires a numeric argument between 0 and 3600\n");
            return ARGP_HELP_STD_ERR;
        }
        else
            srvSettings.scanSeconds = res;
        break;
    }

#ifdef LIBUSB_NO_THREADS_OPTION
    case ARG_NO_THREADS:
        srvSettings.noThreads = true;
        break;
#endif

    case 'p':
        params->pidFile = arg;
        break;

    /* driver options */
    case 'd':
        srvSettings.preferred = (const char**)realloc(srvSettings.preferred, sizeof(char*) * (srvSettings.preferredCount + 1));
        srvSettings.preferred[srvSettings.preferredCount - 1] = arg;
        srvSettings.preferred[srvSettings.preferredCount++] = NULL;
        break;

    case ARG_ONLY_PREFER:
        srvSettings.onlyPreferred = true;
        break;

    case ARG_DRIVER_DIR:
        srvSettings.driverDir = arg;
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

    /* initialize the server-level settings */
    initServerSettings(startWorker);

    /* initialize the parameters structure and parse the cmd line args */
    params.runAsDaemon = true;
    params.pidFile = NULL;
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

void listenToClients(iguanaDev *idev)
{
    PIPE_PTR listener;
    char name[4];

    /* start the listener */
    sprintf(name, "%d", idev->usbDev->id);
    listener = startListening(name);
    if (listener == INVALID_PIPE)
        message(LOG_ERROR, "Worker failed to start listening.\n");
    else
    {
        fd_set fds, fdsin, fdserr;

        /* check the initial aliases */
        getID(idev);

        /* loop while checking the pipes for activity */
        FD_ZERO(&fdsin);
        FD_ZERO(&fdserr);
        while(true)
        {
            client *john;
            int max;
            FD_ZERO(&fds);

            /* first check the listener and read pipe for error */
            if (FD_ISSET(listener, &fdserr) ||
                FD_ISSET(idev->readerPipe[READ], &fdserr))
                break;

            /* take care of messages from the reader */
            if (FD_ISSET(idev->readerPipe[READ], &fdsin) &&
                ! handleReader(idev))
                break;
            FD_SET(idev->readerPipe[READ], &fds);
            max = idev->readerPipe[READ];

            /* next handle incoming connections */
            if (FD_ISSET(listener, &fdsin))
                clientConnected(accept(listener, NULL, NULL), idev);
            FD_SET(listener, &fds);
            if (listener > max)
                max = listener;

            /* last check the clients */
            for(john = (client*)idev->clientList.head; john != NULL;)
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

        setAlias(idev->usbDev->id, NULL);
        stopListening(listener, name);

        /* and release any connected clients */
        while(idev->clientList.count > 0)
            releaseClient((client*)idev->clientList.head);
    }
}

void setAlias(unsigned int id, const char *alias)
{
    /* find the alias and nuke it if it is a link to the name */
    DIR_HANDLE dir = NULL;
    char buffer[PATH_MAX], name[4];

    /* prepare the name string */
    sprintf(name, "%d", id);

    /* look through all symlinks in the directory and delete links to
       name */
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
            if (strcmp(name, ptr) == 0)
                unlink(buf);
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
        if (symlink(name, path) != 0)
                message(LOG_ERROR, "failed to symlink alias: %s\n",
                        translateError(errno));
    }
}
