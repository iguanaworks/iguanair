/****************************************************************************
 ** server.c ****************************************************************
 ****************************************************************************
 *
 * Common code used by all daemon/server/service implementations.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include <argp.h>
#include "version.h"

#include "iguanaIR.h"
#include "compat.h"
#include "driver.h"
#include "devicebase.h"
#include "device-interface.h"
#include "server.h"
#include "pipes.h"
#include "client-interface.h"

/* global variables, internal and shared */
serverSettings srvSettings;
usbId usbIds[] = {
    {0x1781, 0x0938, NULL}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};

void triggerCommand(THREAD_PTR cmd)
{
    THREAD_PTR flg = INVALID_THREAD_PTR;
    if (cmd == (THREAD_PTR)QUIT_TRIGGER)
        message(LOG_INFO, "Triggering shutdown.\n");

    if (writePipe(srvSettings.commPipe[WRITE], &flg, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR) ||
        writePipe(srvSettings.commPipe[WRITE], &cmd, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "failed to write flag and command over commPipe: %s\n",
                translateError(errno));
}

void initServerSettings()
{
    initializeLogging(NULL);

    /* Driver location and preference information.  The preferred list
       is a NULL terminated list of strings. */
    srvSettings.onlyPreferred = false;
    srvSettings.driverDir = NULL,
    srvSettings.preferred = (const char**)malloc(sizeof(char*));
    srvSettings.preferredCount = 0;
    srvSettings.preferred[srvSettings.preferredCount++] = NULL;

    /* timeouts that can be adjusted for different conditions */
#ifdef LIBUSB_NO_THREADS
  #ifdef LIBUSB_NO_THREADS_OPTION
    srvSettings.devSettings.recvTimeout = 1000;
  #else
    srvSettings.devSettings.recvTimeout = 100;
  #endif
#else
    srvSettings.devSettings.recvTimeout = 1000;
#endif
    srvSettings.devSettings.sendTimeout = 1000;

    /* EPIPE usually means device disconnect, but not reliably */
    srvSettings.devSettings.disconnectOnEPipe = false;

    /* default to reading device ids */
    srvSettings.readLabels = true;

    /* default to rescaning when a device is lost */
    srvSettings.autoRescan = true;

    /* default to claiming the hardware devices */
    srvSettings.justDescribe = false;

    /* default to playing nice with other drivers */
    srvSettings.unbind = false;

    /* default to just waiting for hotplug events from an external source */
    srvSettings.scanSeconds = 0;
    srvSettings.scanTimerThread = INVALID_THREAD_PTR;

    /* thread that listens for ctl requests */
    srvSettings.ctlSockThread = INVALID_THREAD_PTR;
    initializeList(&srvSettings.ctlClients);

    /* list of known devices */
    InitializeCriticalSection(&srvSettings.devsLock);
    initializeList(&srvSettings.devs);

    /* initialize the toggle workaround based on our OS */
#ifdef __APPLE__
    srvSettings.fixToggle = true;
#else
    srvSettings.fixToggle = false;
#endif

    /* old flag to handle libusb pre 1.0 threading issues */
#ifdef LIBUSB_NO_THREADS_OPTION
    srvSettings.noThreads = false;
#endif
}

static struct argp_option options[] =
{
    { NULL, 0, NULL, 0, "Driver selection options:", DRV_GROUP },
    { "driver",         'd',             "DRIVER", 0, "Use this driver in preference to others.  This command can be111 used multiple times.", DRV_GROUP },
    { "only-preferred", ARG_ONLY_PREFER, NULL,     0, "Use only drivers specified by the --driver option.",                                 DRV_GROUP },
    { "driver-dir",     ARG_DRIVER_DIR,  "DIR",    0, "Specify the location of driver objects.",                                            DRV_GROUP },

    { NULL, 0, NULL, 0, "Miscellaneous options:", MSC_GROUP },
    { "no-auto-rescan",  ARG_NO_RESCAN,    NULL,     0, "Do not automatically rescan the USB bus after device disconnect.",              MSC_GROUP },
    { "no-ids",          ARG_NO_IDS,       NULL,     0, "Do not query the device for its label.",                                        MSC_GROUP },
    { "no-labels",       ARG_NO_IDS,       NULL,     0, "DEPRECATED: same as --no-ids",                                                  MSC_GROUP },
    { "scan-timer",      ARG_SCANWHEN,   "SECS",     0, "Periodically rescan the USB bus for new devices regardless of hotplug events.", MSC_GROUP },
#ifdef __APPLE__
    { "no-bad-toggle-fix", ARG_BADTOGGLE,  NULL,     0, "On OS X our hardware has a data toggle mismatch and this disables the works around.", MSC_GROUP },
#else
    { "bad-toggle-fix",  ARG_BADTOGGLE,    NULL,     0, "On OS X our hardware has a data toggle mismatch and this works around it.",     MSC_GROUP },
#endif

    /* end of table */
    {0}
};

static error_t parseOption(int key, char *arg, struct argp_state *state)
{
    switch(key)
    {
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

    /* Miscellaneous options */
    case ARG_NO_IDS:
        srvSettings.readLabels = false;
        break;

    case ARG_NO_RESCAN:
        srvSettings.autoRescan = false;
        break;

    case ARG_SCANWHEN:
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

    case ARG_BADTOGGLE:
#ifdef __APPLE__
        srvSettings.fixToggle = false;
#else
        srvSettings.fixToggle = true;
#endif
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp parser = {
    options,
    parseOption
};

struct argp* baseArgParser()
{
    return &parser;
}

static void* scanTrigger(void *junk)
{
    while(true)
    {
        if (readPipeTimed(srvSettings.scanTimerPipe[READ],
                          (char*)&junk, 1, srvSettings.scanSeconds * 1000) == 0)
            triggerCommand((THREAD_PTR)SCAN_TRIGGER);
        else
            break;
    }
    return NULL;
}

static bool ctlSockListening = true;
static void* ctlSockListener(void *junk)
{
    /* TODO: send notices to clients when clients connect and disconnect:
       - print something like: C:0, D:0?
    */
    listenToClients("ctl", &srvSettings.ctlClients, NULL);
    ctlSockListening = false;
    return NULL;
}

bool initServer()
{
    bool retval = false;
    int x;
    for(x = 0; usbIds[x].idVendor != INVALID_VENDOR; x++)
        usbIds[x].data = &srvSettings.devSettings;

    /* print a few parameters for the user */
    message(LOG_DEBUG, "Parameters:\n");
    message(LOG_DEBUG,
            "  recvTimeout: %d\n", srvSettings.devSettings.recvTimeout);
    message(LOG_DEBUG,
            "  sendTimeout: %d\n", srvSettings.devSettings.sendTimeout);
    initializeDriverLayer(currentLogSettings());

    /* prepare the pipe for shutting down any scan thread */
    if (! createPipePair(srvSettings.scanTimerPipe))
        message(LOG_ERROR, "failed to create the scan timer pipe pair\n");
    /* start up a thread to trigger periodic rescans if requested */
    if (srvSettings.scanSeconds > 0 && ! startThread(&srvSettings.scanTimerThread, scanTrigger, NULL))
        message(LOG_ERROR, "failed to start a scanning timer.\n");
    /* prepare the pipe for shutting down the ctl listener */
    else if (! createPipePair(srvSettings.ctlSockPipe))
        message(LOG_ERROR, "failed to create the ctl pipe pair\n");
    /* start listening on the control socket */
    else if (! srvSettings.justDescribe &&
             ! startThread(&srvSettings.ctlSockThread, ctlSockListener, NULL))
        message(LOG_ERROR, "failed to start the listener.\n");
    else
    {
        /* give the ctlSockListener a quarter second to fail */
        for(x = 0; ctlSockListening && x < 25; x++)
            Sleep(10);

        if (! ctlSockListening)
            ; /* intentionally empty since we already logged errors */
        /* initialize the commPipe, driver, and device list */
        else if (! createPipePair(srvSettings.commPipe))
        {
#ifdef _WIN32
            message(LOG_ERROR, "failed to open communication pipe, is another igdaemon running?\n");
#else
            message(LOG_ERROR, "failed to open communication pipe.\n");
#endif
        }
        else if (! findDriver(srvSettings.driverDir,
                              srvSettings.preferred, srvSettings.onlyPreferred))
            message(LOG_ERROR, "failed to find a loadable driver layer.\n");
        else if (! initializeDriver())
            message(LOG_ERROR, "failed to initialize the loadable driver layer.\n");
        else if ((srvSettings.list = prepareDeviceList(usbIds, startWorker)) == NULL)
            message(LOG_ERROR, "failed to initialize the device list.\n");
        else
        {
            claimDevices(srvSettings.list, ! srvSettings.justDescribe, srvSettings.unbind);
            retval = true;
        }
    }

#if DEBUG
message(LOG_WARN, "OPEN %d %s(%d)\n", srvSettings.commPipe[READ],  __FILE__, __LINE__);
message(LOG_WARN, "OPEN %d %s(%d)\n", srvSettings.commPipe[WRITE], __FILE__, __LINE__);
#endif
    return retval;
}

void waitOnCommPipe()
{
    bool quit = false;

    /* trigger an initial device scan */
    triggerCommand((THREAD_PTR)SCAN_TRIGGER);

    while(! quit)
    {
        THREAD_PTR thread = INVALID_THREAD_PTR;
        void *exitVal;

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
            if (! updateDeviceList(srvSettings.list))
                message(LOG_ERROR, "scan failed.\n");
            if (srvSettings.justDescribe)
                break;
        }
    }

    /* wait for all the workers to finish */
    reapAllChildren(srvSettings.list);
}

char* aliasSummary(iguanaDev *idev)
{
    int len = 1;
    char name[8], *buf;

    sprintf(name, "%d", idev->usbDev->id);
    len += 2 + strlen(name);
    if (idev->locAlias != NULL)
        len += 3 + strlen(idev->locAlias);
    if (idev->userAlias != NULL)
        len += 3 + strlen(idev->userAlias);

    buf = (char*)malloc(len);
    if (buf != NULL)
    {
        strcpy(buf, "i:");
        strcat(buf, name);
        if (idev->locAlias != NULL)
        {
            strcat(buf, ",l:");
            strcat(buf, idev->locAlias);
        }
        if (idev->userAlias != NULL)
        {
            strcat(buf, ",u:");
            strcat(buf, idev->userAlias);
        }
    }
    return buf;
}

static bool summarize(itemHeader *item, void *userData)
{
    bool first = false;
    int len;
    char *sum, **buf = (char**)userData;

    sum = aliasSummary((iguanaDev*)item);
    if (sum != NULL)
    {
        if (*buf == NULL)
        {
            first = true;
            len = 1;
        }
        else
            len = strlen(*buf) + 1;
        len += strlen(sum) + 1;

        *buf = (char*)realloc(*buf, len);
        if (first)
            strcpy(*buf, sum);
        else
        {
            strcat(*buf, "|");
            strcat(*buf, sum);
        }
    }

    return true;
}

char* deviceSummary()
{
    char *buf = NULL;
    forEach(&srvSettings.devs, summarize, &buf);
    return buf;
}

typedef struct findAddrInfo
{
    const char *name;
    char *result;
} findAddrInfo;

static bool findAddress(itemHeader *item, void *userData)
{
    bool found = false;
    findAddrInfo *info = (findAddrInfo*)userData;
    iguanaDev *idev = (iguanaDev*)item;

    if (info->result == NULL)
    {
        char buf[PATH_MAX], idBuf[8];

        sprintf(idBuf, "%d", idev->usbDev->id);
        socketName(idBuf, buf, PATH_MAX);
        if (strcmp(buf, info->name) == 0)
            found = true;
        else
        {
            socketName(idev->locAlias, buf, PATH_MAX);
            if (strcmp(buf, info->name) == 0)
                found = true;
            else
            {
                socketName(idev->userAlias, buf, PATH_MAX);
                if (strcmp(buf, info->name) == 0)
                    found = true;
            }
        }
    }

    if (found)
        info->result = strdup(idev->addrStr);
    return true;
}

char* deviceAddress(const char *name)
{
    findAddrInfo info = {NULL, NULL};

    info.name = name;
    forEach(&srvSettings.devs, findAddress, &info);

    if (info.result == NULL)
        return NULL;
    return strdup(info.result);
}

void cleanupServer()
{
    cleanupDriver();
    closePipe(srvSettings.commPipe[WRITE]);
    closePipe(srvSettings.commPipe[READ]);
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", srvSettings.commPipe[WRITE], __FILE__, __LINE__);
message(LOG_WARN, "CLOSE %d %s(%d)\n", srvSettings.commPipe[READ],  __FILE__, __LINE__);
#endif

    /* shut down the ctl listener */
    closePipe(srvSettings.ctlSockPipe[WRITE]);
    joinThread(srvSettings.ctlSockThread, NULL);

    /* shut down any scan timer */
    closePipe(srvSettings.scanTimerPipe[WRITE]);
    if (srvSettings.scanSeconds > 0)
        joinThread(srvSettings.scanTimerThread, NULL);
}

void makeParentJoin(THREAD_PTR thread)
{
    if (writePipe(srvSettings.commPipe[WRITE],
                  &thread, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "Failed to write thread id to parentPipe.\n");
}
