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
    /* driver location and preference information */
    bool onlyPreferred;
    const char *driverDir,
              **preferred;
    int preferredCount;

    /* timeouts and other device settings */
    deviceSettings devSettings;

    deviceFunc devFunc;
} serverSettings;

void initServerSettings(serverSettings *settings);
deviceList* initServer(serverSettings *settings);

#endif
