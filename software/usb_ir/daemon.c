/****************************************************************************
 ** daemon.c ****************************************************************
 ****************************************************************************
 *
 * Source for the igdaemon application that manages access to the
 * underlying USB device.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
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
#include <fcntl.h>

#include <argp.h>

#include "driver.h"
#include "pipes.h"
#include "device-interface.h"
#include "client-interface.h"
#include "server.h"

#ifdef __APPLE__
extern int darwin_hotplug(const usbId *);
#endif

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

static void workLoop()
{
    /* initialize the driver, signals, and device list */
    if (initServer(&srvSettings))
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
            /* trigger the initial device scan */
            scanHandler(SIGHUP);

#ifdef __APPLE__
            /* Support hot plug in on Mac OS X */
            if (darwin_hotplug(usbIds) != 0)
                message(LOG_ERROR, "Failed to enable hotplug\n");
#endif

            /* loop, waiting for commands.  Only returns on application shutdown. */
            waitOnCommPipe();
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
    char **addrStr = NULL;
    if (idev != NULL)
        addrStr = &idev->addrStr;
    listener = createServerPipe(name, addrStr);
    if (listener == INVALID_PIPE)
    {
        if (idev == NULL)
            message(LOG_ERROR, "Server failed to start listening on ctl socket.\n");
        else
            message(LOG_ERROR, "Worker failed to start listening.\n");
    }
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
            {
                PIPE_PTR clientFd;
                int flags;

                clientFd = accept(listener, NULL, NULL);
                flags = fcntl(clientFd, F_GETFL);
                if (flags == -1)
                    message(LOG_ERROR, "Failed read status flags for socket.\n");
                else if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) == -1)
                    message(LOG_ERROR, "Failed to set client socket to non-blocking mode.\n");
                else
                    clientConnected(clientFd, clientList, idev);
            }
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
        {
            char idxStr[4];
            sprintf(idxStr, "%d", idev->usbDev->id);
            setAlias(idxStr, true, NULL);
        }
        closeServerPipe(listener, name);
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", listener, __FILE__, __LINE__);
#endif

        /* and release any connected clients */
        while(clientList->count > 0)
            releaseClient((client*)clientList->head);
    }
}
