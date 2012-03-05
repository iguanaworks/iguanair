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

#ifndef _DAEMON_BASE_H_
#define _DAEMON_BASE_H_

typedef struct 
{
    /* driver location and prefered driver information */
    bool onlyPreferred;
    const char *driverDir,
              **preferred;
    int preferredCount;

    /* timeouts and other device settings */
    deviceSettings devSettings;

    /* function called whenever a new device is found */
    deviceFunc devFunc;

    /* whether the server should ask devices for their labels */
    bool readLabels;

    /* should the server rescan the usb bus after a disconnect */ 
    bool autoRescan;

    /* libusb pre 1.0 had problems with threading and this was a
       mechanism to deal with those issues. */
#ifdef LIBUSB_NO_THREADS_OPTION
    bool noThreads;
#endif
} serverSettings;

void initServerSettings(deviceFunc devFunc);
deviceList* initServer();

/* global settings object */
extern serverSettings srvSettings;

#endif
