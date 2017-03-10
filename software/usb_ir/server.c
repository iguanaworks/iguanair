/****************************************************************************
 ** server.c ****************************************************************
 ****************************************************************************
 *
 * Common code used by all daemon/server/service implementations.
 *
 * Copyright (C) 2009, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include "iguanaIR.h"
#include "compat.h"
#include "support.h"
#include "driver.h"
#include "devicebase.h"
#include "device-interface.h"
#include "server.h"
#include "pipes.h"

/* global server settings are grouped togther */
serverSettings srvSettings;
PIPE_PTR commPipe[2];

static usbId ids[] = {
    {0x1781, 0x0938, NULL}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};

void triggerCommand(THREAD_PTR cmd)
{
    THREAD_PTR flg = INVALID_THREAD_PTR;
    if (writePipe(srvSettings.commPipe[WRITE], &flg, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR) ||
        writePipe(srvSettings.commPipe[WRITE], &cmd, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "failed to write flag and command over commPipe: %s\n",
                translateError(errno));
}

void initServerSettings(deviceFunc devFunc)
{
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

    /* an OS-specific function */
    srvSettings.devFunc = devFunc;

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

    /* old flag to handle libusb pre 1.0 threading issues */
#ifdef LIBUSB_NO_THREADS_OPTION
    srvSettings.noThreads = false;
#endif
}

static void* scanTrigger(void *junk)
{
    while(true)
    {
        Sleep(srvSettings.scanSeconds * 1000);
        triggerCommand((THREAD_PTR)SCAN_TRIGGER);
    }
    return NULL;
}

deviceList* initServer()
{
    deviceList *list = NULL;
    int x;
    for(x = 0; ids[x].idVendor != INVALID_VENDOR; x++)
        ids[x].data = &srvSettings.devSettings;

    /* print a few parameters for the user */
    message(LOG_DEBUG, "Parameters:\n");
    message(LOG_DEBUG,
            "  recvTimeout: %d\n", srvSettings.devSettings.recvTimeout);
    message(LOG_DEBUG,
            "  sendTimeout: %d\n", srvSettings.devSettings.sendTimeout);

    /* start up a thread to trigger periodic rescans if requested */
    if (srvSettings.scanSeconds > 0 && ! startThread(&srvSettings.scanTimerThread, scanTrigger, NULL))
        message(LOG_ERROR, "failed to start a scanning timer.\n");
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
    else if ((list = prepareDeviceList(ids, srvSettings.devFunc)) == NULL)
        message(LOG_ERROR, "failed to initialize the device list.\n");
    else
        claimDevices(list, ! srvSettings.justDescribe, srvSettings.unbind);

#if DEBUG
message(LOG_WARN, "OPEN %d %s(%d)\n", srvSettings.commPipe[0],   __FILE__, __LINE__);
message(LOG_WARN, "OPEN %d %s(%d)\n", srvSettings.commPipe[1],   __FILE__, __LINE__);
#endif
    return list;
}

void cleanupServer()
{
    cleanupDriver();
}

void makeParentJoin(THREAD_PTR thread)
{
    if (writePipe(srvSettings.commPipe[WRITE],
                  &thread, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "Failed to write thread id to parentPipe.\n");
}
