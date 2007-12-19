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

#include "base.h"
#include <popt.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#include "iguanaIR.h"
#include "usbclient.h"
#include "pipes.h"
#include "support.h"
#include "device-interface.h"
#include "client-interface.h"

/* local variables */
static usbId ids[] = {
    {0x1781, 0x0938}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};
static mode_t devMode = 0777;
PIPE_PTR commPipe[2];
#ifdef LIBUSB_NO_THREADS
  static unsigned int recvTimeout = 100;
#else
  static unsigned int recvTimeout = 1000;
#endif
static unsigned int sendTimeout = 1000;

static void quitHandler(int UNUSED(sig))
{
#if DEBUG
printf("CLOSE %d %s(%d)\n", commPipe[WRITE], __FILE__, __LINE__);
#endif
    closePipe(commPipe[WRITE]);
}

static void scanHandler(int UNUSED(sig))
{
    THREAD_PTR x = INVALID_THREAD_PTR;
    writePipe(commPipe[WRITE], &x, sizeof(THREAD_PTR));
}

static void workLoop()
{
    /* initialize the device list */
    usbDeviceList list;

    if (! initDeviceList(&list, ids, recvTimeout, sendTimeout, startWorker))
        message(LOG_ERROR, "failed to initialize device list.\n");
    else if (signal(SIGINT, quitHandler) == SIG_ERR)
        message(LOG_ERROR, "failed to install SIGINT handler.\n");
    else if (signal(SIGTERM, quitHandler) == SIG_ERR)
        message(LOG_ERROR, "failed to install SIGTERM handler.\n");
    else if (signal(SIGHUP, scanHandler) == SIG_ERR)
        message(LOG_ERROR, "failed to install SIGHUP handler.\n");
    else if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        message(LOG_ERROR, "failed to ignore SIGPIPE messages.\n");
    else if (! createPipePair(commPipe))
        message(LOG_ERROR, "failed to open communication pipe.\n");
    else
    {
        bool quit = false;

#if DEBUG
printf("OPEN %d %s(%d)\n", commPipe[0], __FILE__, __LINE__);
printf("OPEN %d %s(%d)\n", commPipe[1], __FILE__, __LINE__);
#endif

        setParentPipe(commPipe[WRITE]);

        /* trigger the initial device scan */
        scanHandler(SIGHUP);

        /* now wait for commands */
        while(! quit)
        {
            THREAD_PTR thread = INVALID_THREAD_PTR;
            void *exitVal;

            switch(readPipe(commPipe[READ], &thread, sizeof(THREAD_PTR)))
            {
            /* error */
            default:
                message(LOG_ERROR,
                        "Command read failed: %s\n", strerror(errno));
                /* fall through and quit */
            /* close, signalling a shutdown */
            case 0:
                quit = true;
                break;

            /* command read (right now only support scan) */
            case sizeof(THREAD_PTR):
                if (thread != INVALID_THREAD_PTR)
                    joinThread(thread, &exitVal);
                else if (! updateDeviceList(&list))
                    message(LOG_ERROR, "scan failed.\n");
                break;
            }
        }

        /* wait for all the workers to finish */
        reapAllChildren(&list);
    }
}

static struct poptOption options[] =
{
    /* general daemon options */
    { "log-file", 'l', POPT_ARG_STRING, NULL, 'l', "Specify a log file (defaults to \"-\").", "filename" },
    { "no-daemon", 'n', POPT_ARG_NONE, NULL, 'n', "Do not fork into the background.", NULL },
    { "pid-file", 'p', POPT_ARG_STRING, NULL, 'p', "Specify where to write the pid of the daemon process.", "filename" },
    { "quiet", 'q', POPT_ARG_NONE, NULL, 'q', "Reduce the verbosity.", NULL },
    { "verbose", 'v', POPT_ARG_NONE, NULL, 'v', "Increase the verbosity.", NULL },

    /* iguanaworks specific options */
    { "no-labels", '\0', POPT_ARG_NONE, NULL, 'b', "Do not query the iguanaworks device for it's label.  Try this if fetching the label hangs.", NULL },
    { "receive-timeout", '\0', POPT_ARG_INT, &recvTimeout, 0, "Specify the device receive timeout.", "timeout" },
    { "send-timeout", '\0', POPT_ARG_INT, &sendTimeout, 0, "Specify the device send timeout.", "timeout" },

    POPT_AUTOHELP
    POPT_TABLEEND
};

static void exitOnOptError(poptContext poptCon, char *msg)
{
    message(LOG_ERROR, msg, poptBadOption(poptCon, 0));
    poptPrintHelp(poptCon, stderr, 0);
    exit(1);
}

int main(int argc, const char **argv)
{
    int exitval = 0, x = 0;
    bool runAsDaemon = true;
    const char *pidFile = NULL, **leftOvers;
    poptContext poptCon;

    poptCon = poptGetContext(NULL, argc, argv, options, 0);

    while(x != -1)
    {
        switch(x = poptGetNextOpt(poptCon))
        {
        case 'n':
            runAsDaemon = false;
            break;

        case 'b':
            readLabels = false;
            break;

        case 'p':
            pidFile = poptGetOptArg(poptCon);
            break;

        case 'l':
            openLog(poptGetOptArg(poptCon));
            break;

        case 'q':
            changeLogLevel(-1);
            break;

        case 'v':
            changeLogLevel(+1);
            break;

        /* Error handling starts here */
        case POPT_ERROR_NOARG:
            exitOnOptError(poptCon, "Missing argument for '%s'\n");
            break;

        case POPT_ERROR_BADNUMBER:
            exitOnOptError(poptCon, "Need a number instead of '%s'\n");
            break;

        case POPT_ERROR_BADOPT:
            if (strcmp(poptBadOption(poptCon, 0), "-h") == 0)
            {
                poptPrintHelp(poptCon, stdout, 0);
                exit(0);
            }
            exitOnOptError(poptCon, "Unknown option '%s'\n");
            break;

        case -1:
            break;
        default:
            message(LOG_FATAL,
                    "Unexpected return value from popt: %d:%s\n",
                    x, poptStrerror(x));
            break;
        }
    }

    /* what if we have extra parameters? */
    leftOvers = poptGetArgs(poptCon);
    if (leftOvers != NULL && leftOvers[0] != NULL)
    {
        message(LOG_ERROR, "Unknown argument '%s'\n", leftOvers[0]);
        poptPrintHelp(poptCon, stderr, 0);
        exit(1);
    }
    poptFreeContext(poptCon);

    /* run as a daemon if requested and possible */
    if (runAsDaemon)
    {
        message(LOG_DEBUG, "Forking into the background.\n");
        if (daemon(0, 0) == 0)
            umask(0);
        else
        {
            message(LOG_ERROR, "daemon() failed: %s\n", strerror(errno));
            exitval = 1;
        }
    }

    /* write the pid out if requested */
    if (exitval == 0 && pidFile != NULL)
    {
        FILE *pf;
        pf = fopen(pidFile, "w");
        if (pf == NULL)
        {
            message(LOG_ERROR, "Failed to open pid file.\n");
            exitval = 2;
        }
        else
        {
            fprintf(pf, "%d\n", getpid());
            fclose(pf);
        }
    }

    /* if startup succeeded wait for user signals */
    if (exitval == 0)
        workLoop();

    return exitval;
}

static bool mkdirs(char *path)
{
    bool retval = false;
    char *slash;

    slash = strrchr(path, '/');
    if (slash == NULL)
    {
        *((char*)NULL) = 5;
        
        retval = true;
    }
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

static int startListening(const char *name, const char *alias)
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
#if DEBUG
printf("CLOSE %d %s(%d)\n", sockfd, __FILE__, __LINE__);
#endif
        close(sockfd);
    }

    return -1;
}

static void stopListening(int fd, const char *name, const char *alias)
{
    char path[PATH_MAX], ptr[PATH_MAX];
    int length;

    /* figure out the name */
    socketName(name, path, PATH_MAX);

    /* and nuke it */
    unlink(path);
#if DEBUG
printf("CLOSE %d %s(%d)\n", fd, __FILE__, __LINE__);
#endif
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

void listenToClients(char *name, char *alias, iguanaDev *idev,
                     handleReaderFunc handleReader,
                     clientConnectedFunc clientConnected,
                     handleClientFunc handleClient)
{
    PIPE_PTR listener;

    listener = startListening(name, alias);
    if (listener == INVALID_PIPE)
        message(LOG_ERROR, "Worker failed to start listening.\n");
    else
    {
        fd_set fds, fdsin, fdserr;

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
                message(LOG_ERROR, "select failed: %s\n", strerror(errno));
                break;
            }
        }

        stopListening(listener, name, alias);
    }
}
