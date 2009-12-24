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

#include "iguanaIR.h"
#include "compat.h"
#include "support.h"
#include "driver.h"
#include "devicebase.h"
#include "device-interface.h"
#include "server.h"

static usbId ids[] = {
    {0x1781, 0x0938, NULL}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};

void initServerSettings(serverSettings *settings)
{
#ifdef LIBUSB_NO_THREADS
  #ifdef LIBUSB_NO_THREADS_OPTION
    settings->devSettings.recvTimeout = 1000;
  #else
    settings->devSettings.recvTimeout = 100;
  #endif
#else
    settings->devSettings.recvTimeout = 1000;
#endif
    settings->devSettings.sendTimeout = 1000;

    /* driver location and preference information */
    settings->onlyPreferred = false;
    settings->driverDir = NULL,
    settings->preferred = NULL;
    settings->preferredCount = 0;
}

deviceList* initServer(serverSettings *settings)
{
    char expectedDir[PATH_MAX];
    deviceList *list = NULL;
    int x;
    for(x = 0; ids[x].idVendor != INVALID_VENDOR; x++)
        ids[x].data = &settings->devSettings;

    /* print a few parameters for the user */
    message(LOG_DEBUG, "Parameters:\n");
    message(LOG_DEBUG, "  recvTimeout: %d\n", settings->devSettings.recvTimeout);
    message(LOG_DEBUG, "  sendTimeout: %d\n", settings->devSettings.sendTimeout);

    /* if we are expected to find the driver directory.... attempt it */
    if (settings->driverDir == NULL)
    {
        if (findDriverDir(expectedDir))
            settings->driverDir = expectedDir;
        else
            /* fall back on something reasonable? */
#ifdef _WIN32
            settings->driverDir = ".";
#else
            settings->driverDir = "/usr/lib/iguanaIR";
#endif
    }

    /* initialize the driver and device list */
    if (! findDriver(settings->driverDir, settings->preferred, settings->onlyPreferred))
        message(LOG_ERROR, "failed to find a loadable driver layer.\n");
    else if ((list = prepareDeviceList(ids, settings->devFunc)) == NULL)
        message(LOG_ERROR, "failed to initialize the device list.\n");

    return list;
}
