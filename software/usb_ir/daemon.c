/****************************************************************************
 ** daemon.c ****************************************************************
 ****************************************************************************
 *
 * Source for the igdaemon application that manages access to the
 * underlying USB device.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */

#include <popt.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#include "iguanaIR.h"
#include "base.h"
#include "usbclient.h"
#include "pipes.h"
#include "support.h"
#include "protocol.h"
#include "igdaemon.h"

/* local variables */
static usbId ids[] = {
    {0x1781, 0x0938}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};
static mode_t devMode = 0777;
static PIPE_PTR commPipe[2];
#ifdef LIBUSB_NO_THREADS
static unsigned int recvTimeout = 100;
#else
static unsigned int recvTimeout = 1000;
#endif
static unsigned int sendTimeout = 1000;

static void quitHandler(int sig)
{
    closePipe(commPipe[WRITE]);
}

static void scanHandler(int sig)
{
    writePipe(commPipe[WRITE], &sig, 1);
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
        message(LOG_ERROR, "failed to install SIGPIPE handler.\n");
    else if (! createPipePair(commPipe))
        message(LOG_ERROR, "failed to open communication pipe.\n");
    else
    {
        bool quit = false;

        /* trigger initial scan */
        scanHandler(SIGHUP);

        /* now wait for commands */
        while(! quit)
        {
            char cmd;

            switch(readPipe(commPipe[READ], &cmd, 1))
            {
            /* error */
            default:
                message(LOG_ERROR, "Command read failed.\n");
                break;

            /* close, signalling a shutdown */
            case 0:
                quit = true;
                break;

            /* command read (right now only support scan) */
            case 1:
                if (! updateDeviceList(&list))
                    message(LOG_ERROR, "scan failed.\n");
                break;
            }
        }

        /* wait for all the workers to finish */
        reapAllChildren(&list);
    }
}

static bool forkOff()
{
    bool retval;

    switch(fork())
    {
    case -1:
        break;

    case 0:
        /* child process continue */
        retval = true;
        break;

    default:
        /* original process exit */
        exit(0);
    }

    return retval;
}

/* based on: http://www.unixguide.net/unix/programming/1.7.shtml */
static bool daemonize()
{
    bool retval = false;
    message(LOG_DEBUG, "Forking into the background.\n");

    if (! forkOff())
        message(LOG_ERROR, "fork() failed: %s\n", strerror(errno));
    else if (setsid() == -1)
        message(LOG_ERROR, "setsid() failed: %s\n", strerror(errno));
    else if (! forkOff())
        message(LOG_ERROR, "second fork() failed: %s\n", strerror(errno));
    else if (chdir("/") == -1)
        message(LOG_ERROR, "chdir(\"/\") failed: %s\n", strerror(errno));
    else
    {
        umask(0);
        if ((stdin = freopen("/dev/null", "r", stdin)) == NULL)
            message(LOG_ERROR, "Failed to reopen stdin", strerror(errno));
        else if ((stdin = freopen("/dev/null", "w", stdout)) == NULL)
            message(LOG_ERROR, "Failed to reopen stdout", strerror(errno));
        else if ((stdin = freopen("/dev/null", "w", stderr)) == NULL)
            message(LOG_ERROR, "Failed to reopen stderr", strerror(errno));
        else
            retval = true;
    }

    return retval;
}

static struct poptOption options[] =
{
    /* general daemon options */
    { "log-file", 'l', POPT_ARG_STRING, NULL, 'l', "Specify a log file (defaults to \"-\").", "filename" },
    { "no-daemon", 'n', POPT_ARG_NONE, NULL, 'n', "Do not fork into teh background.", NULL },
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
    int x, retval = 1;
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



    for(x = 1; x < argc; x++)
    {
    }

    /* run as a daemon if requested and possible */
    if (runAsDaemon && ! daemonize())
        message(LOG_ERROR, "Failed to daemonize.\n");
    else
    {
        bool success = false;

        /* write the pid out if requested */
        if (pidFile != NULL)
        {
            FILE *pf;
            pf = fopen(pidFile, "w");
            if (pf == NULL)
                message(LOG_ERROR, "Failed to open pid file.\n");
            else
            {
                fprintf(pf, "%d\n", getpid());
                fclose(pf);
                success = true;
            }
        }
        else
            success = true;

        if (success)
        {
            /* wait for user signals */
            workLoop();
            retval = 0;
        }
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

static void stopListening(int fd, const char *name, const char *alias)
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
