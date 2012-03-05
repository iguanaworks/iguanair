/****************************************************************************
 ** daemon.h ****************************************************************
 ****************************************************************************
 *
 * Header included ONLY in daemon.c/service.c
 *
 * Copyright (C) 2009, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include <malloc.h>

#include "iguanaIR.h"
#include "compat.h"
#include "support.h"
#include "driver.h"
#include "devicebase.h"
#include "device-interface.h"
#include "server.h"

/* global server settings are grouped togther */
serverSettings srvSettings;

static usbId ids[] = {
    {0x1781, 0x0938, NULL}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};

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

    /* an OS-specific function */
    srvSettings.devFunc = devFunc;

    /* default to reading device ids */
    srvSettings.readLabels = true;

    /* default to rescaning when a device is lost */
    srvSettings.autoRescan = true;

    /* old flag to handle libusb pre 1.0 threading issues */
#ifdef LIBUSB_NO_THREADS_OPTION
    srvSettings.noThreads = false;
#endif
}

deviceList* initServer()
{
    char expectedDir[PATH_MAX];
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

    /* if we are expected to find the driver directory.... attempt it */
    if (srvSettings.driverDir == NULL)
    {
        if (findDriverDir(expectedDir))
            srvSettings.driverDir = expectedDir;
        else
            /* fall back on something reasonable? */
#ifdef _WIN32
            srvSettings.driverDir = ".";
#else
            srvSettings.driverDir = "/usr/lib/iguanaIR";
#endif
    }

    /* initialize the driver and device list */
    if (! findDriver(srvSettings.driverDir,
                     srvSettings.preferred, srvSettings.onlyPreferred))
        message(LOG_ERROR, "failed to find a loadable driver layer.\n");
    else if ((list = prepareDeviceList(ids, srvSettings.devFunc)) == NULL)
        message(LOG_ERROR, "failed to initialize the device list.\n");

    return list;
}
