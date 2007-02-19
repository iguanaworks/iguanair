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
#include "base.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <popt.h>

#include "pipes.h"
#include "support.h"
#include "usbclient.h"
#include "iguanadev.h"

/* local variables */
static usbId ids[] = {
    {0x1781, 0x0938}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};
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
    else if (createPipePair(commPipe) != 0)
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
