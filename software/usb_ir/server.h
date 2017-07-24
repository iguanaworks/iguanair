/****************************************************************************
 ** server.h ****************************************************************
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

#pragma once

#include "logging.h"

enum
{
    /* start possible commands that can be triggered by signals at an
       arbitrary but recognizable value */
    SCAN_TRIGGER = 0x55,
    QUIT_TRIGGER,
};
void triggerCommand(THREAD_PTR cmd);

enum
{
    /* igdaemon specific actions */
    ARG_NO_IDS = 0x200,
    ARG_NO_RESCAN,
    ARG_SCANWHEN,
    ARG_BADTOGGLE,
    ARG_ONLY_PREFER,
    ARG_DRIVER_DIR,
    LAST_BASE_ARG,

    /* defines for argp */
    DRV_GROUP = MSC_GROUP + 1,
    OS_GROUP,
    HELP_GROUP
};

typedef struct
{
    /* driver location and prefered driver information */
    bool onlyPreferred;
    const char *driverDir,
              **preferred;
    int preferredCount;

    /* timeouts and other device settings */
    deviceSettings devSettings;

    /* whether the server should ask devices for their labels */
    bool readLabels;

    /* should the server rescan the usb bus after a disconnect */
    bool autoRescan;

    /* user may request that we just print information about devices */
    bool justDescribe;

    /* we can tell the driver to try and unbind other drivers */
    bool unbind;

    /* should the server scan the usb bus periodically? */
    int scanSeconds;
    THREAD_PTR scanTimerThread;
    PIPE_PTR scanTimerPipe[2];

    /* thread that listens for ctl requests */
    THREAD_PTR ctlSockThread;
    listHeader ctlClients;
    PIPE_PTR ctlSockPipe[2];

    /* a locked list of known devices */
    LOCK_PTR devsLock;
    listHeader devs;

    /* whether to try and fix the toggle issue on OS X */
    bool fixToggle;

    /* a method for children to communicate back to the list owner */
    PIPE_PTR commPipe[2];

    /* libusb pre 1.0 had problems with threading and this was a
       mechanism to deal with those issues. */
#ifdef LIBUSB_NO_THREADS_OPTION
    bool noThreads;
#endif

    /* list of devices that we are handling */
    deviceList *list;
} serverSettings;

void initServerSettings();
struct argp* baseArgParser();
bool initServer();
void waitOnCommPipe();
char* aliasSummary();
char* deviceSummary();
void cleanupServer();

/* usb ids that we support */
extern usbId usbIds[];

/* global settings object */
extern serverSettings srvSettings;

/* used during shutdown to clean up threads */
void makeParentJoin(THREAD_PTR thread);
