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
#include "compat.h"

#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <popt.h>

#include "driver.h"
#include "pipes.h"
#include "support.h"
#include "device-interface.h"
#include "client-interface.h"
#include "server.h"

#ifdef __APPLE__
extern int daemon_osx_support(const usbId *);
#endif

/* local variables */
static mode_t devMode = 0777;
PIPE_PTR commPipe[2];
static int logLevelTemp = 0;

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
    deviceList *list;

    /* initialize the driver and device list */
    if ((list = initServer(&srvSettings)) == NULL)
        message(LOG_ERROR, "failed to initialize the device list.\n");
    else if (! createPipePair(srvSettings.devSettings.childPipe))
        message(LOG_ERROR, "failed to open child pipe.\n");
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

#ifdef __APPLE__
        /* Support hot plug in on Mac OS X -- returns non-zero for error */
        daemon_osx_support(ids);
#endif

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
                        "Command read failed: %s\n", translateError(errno));
                /* fall through and quit */
            /* close, signalling a shutdown */
            case 0:
                quit = true;
                break;

            /* command read (right now only support scan) */
            case sizeof(THREAD_PTR):
                if (thread != INVALID_THREAD_PTR)
                    joinThread(thread, &exitVal);
                else if (! updateDeviceList(list))
                    message(LOG_ERROR, "scan failed.\n");
                break;
            }
        }

        /* wait for all the workers to finish */
        reapAllChildren(list, &srvSettings.devSettings);
    }
}

enum
{
    /* generic actions */
    ARG_LOG_FILE = 1, /* popt in MacPorts does not like starting w 0 */
    ARG_QUIETER,
    ARG_LOUDER,
    ARG_LOG_LEVEL,

    /* igdaemon specific actions */
    ARG_FOREGROUND,
    ARG_PID_FILE,
    ARG_NO_IDS,
    ARG_NO_RESCAN,
    ARG_NO_THREADS,
    ARG_DRIVER,
    ARG_ONLY_PREFER,
    ARG_DRIVER_DIR
};

static struct poptOption options[] =
{
    /* general daemon options */
    { "log-file",  'l',  POPT_ARG_STRING, NULL, ARG_LOG_FILE,   "Specify a log file (defaults to \"-\").", "filename" },
    { "quiet",     'q',  POPT_ARG_NONE,   NULL, ARG_QUIETER,    "Reduce the verbosity.", NULL },
    { "verbose",   'v',  POPT_ARG_NONE,   NULL, ARG_LOUDER,     "Increase the verbosity.", NULL },
    { "log-level", '\0', POPT_ARG_INT,    &logLevelTemp, ARG_LOG_LEVEL, "Set the verbosity.", NULL },

    /* iguanaworks specific options */
    { "no-daemon", 'n',  POPT_ARG_NONE,   NULL, ARG_FOREGROUND, "Do not fork into the background.", NULL },
    { "pid-file",  'p',  POPT_ARG_STRING, NULL, ARG_PID_FILE,   "Specify where to write the pid of the daemon process.", "filename" },
    { "no-auto-rescan", '\0', POPT_ARG_NONE, NULL, ARG_NO_RESCAN, "Do not automatically rescan the USB bus after a device disconnect.", NULL },
    { "no-ids", '\0', POPT_ARG_NONE, NULL, ARG_NO_IDS, "Do not query the iguanaworks device for its label.  Try this if fetching the label hangs.", NULL },
    { "no-labels",       '\0', POPT_ARG_NONE, NULL, ARG_NO_IDS, "DEPRECATED: same as --no-ids", NULL },
    { "receive-timeout", '\0', POPT_ARG_INT,  &srvSettings.devSettings.recvTimeout, 0, "Specify the device receive timeout.", "timeout" },
    { "send-timeout",    '\0', POPT_ARG_INT,  &srvSettings.devSettings.sendTimeout, 0, "Specify the device send timeout.", "timeout" },

#ifdef LIBUSB_NO_THREADS_OPTION
    { "no-threads", '\0', POPT_ARG_NONE, NULL, ARG_NO_THREADS, "Do not allow two threads to both access libusb calls at the same time.  Try this if the device occasionally crashes.", NULL },
#endif

    /* options specific to the drivers */
    { "driver", 'd', POPT_ARG_STRING, NULL, ARG_DRIVER, "Use this driver in preference to others.  This command can be used multiple times.", "preferred driver" },
    { "only-preferred", '\0', POPT_ARG_NONE, NULL, ARG_ONLY_PREFER, "Use only drivers specified by the --driver option.", "only preferred drivers" },
    { "driver-dir", '\0', POPT_ARG_STRING, NULL, ARG_DRIVER_DIR, "Specify the location of driver objects.", "driver directory" },


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

    /* initialize the server-level settings */
    initServerSettings(startWorker);

    poptCon = poptGetContext(NULL, argc, argv, options, 0);
    while(x != -1)
    {
        switch(x = poptGetNextOpt(poptCon))
        {
        case ARG_LOG_FILE:
            openLog(poptGetOptArg(poptCon));
            break;

        case ARG_QUIETER:
            changeLogLevel(-1);
            break;

        case ARG_LOUDER:
            changeLogLevel(+1);
            break;

        case ARG_LOG_LEVEL:
            setLogLevel(logLevelTemp);
            break;

        case ARG_FOREGROUND:
            runAsDaemon = false;
            break;

        case ARG_NO_IDS:
            srvSettings.readLabels = false;
            break;

        case ARG_NO_RESCAN:
            srvSettings.autoRescan = false;
            break;

#ifdef LIBUSB_NO_THREADS_OPTION
        case ARG_NO_THREADS:
            srvSettings.noThreads = true;
            break;
#endif

        case ARG_PID_FILE:
            pidFile = poptGetOptArg(poptCon);
            break;

        /* driver options */
        case ARG_DRIVER:
            srvSettings.preferred = (const char**)realloc(srvSettings.preferred, sizeof(char*) * (srvSettings.preferredCount + 1));
            srvSettings.preferred[srvSettings.preferredCount - 1] = poptGetOptArg(poptCon);
            srvSettings.preferred[srvSettings.preferredCount++] = NULL;
            break;

        case ARG_ONLY_PREFER:
            srvSettings.onlyPreferred = true;
            break;

        case ARG_DRIVER_DIR:
            srvSettings.driverDir = poptGetOptArg(poptCon);
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
            message(LOG_ERROR, "daemon() failed: %s\n", translateError(errno));
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

    return -1;
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

void listenToClients(iguanaDev *idev,
                     handleReaderFunc handleReader,
                     clientConnectedFunc clientConnected,
                     handleClientFunc handleClient)
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
            unlink(path);
        symlink(name, path);
    }
}
