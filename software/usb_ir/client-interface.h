/****************************************************************************
 ** igdaemon.h **************************************************************
 ****************************************************************************
 *
 * A couple functions used to interface with Iguanaworks USB devices.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#ifndef _IGDAEMON_
#define _IGDAEMON_

typedef struct client
{
    /* we keep a list of clients for each device */
    itemHeader header;

    /* which iguana device is this associated with? */
    iguanaDev *idev;

    /* for communication with the client */
    PIPE_PTR fd;

    /* whether recv messages should be returned to this client */
    int receiving;

    /* protocol version that should be used with this client */
    uint16_t version;

#ifdef WIN32
    /* used in the win32 driver to keep track of overlapped actions */
    OVERLAPPED over;
#endif
} client;

/* API used by the daemon/service code */
void listenToClients(iguanaDev *idev);
void releaseClient(client *target);
bool handleReader(iguanaDev *idev);
void clientConnected(PIPE_PTR clientFd, iguanaDev *idev);
bool handleClient(client *me);
void setAlias(iguanaDev *idev, bool deleteAll, const char *alias);

/* the worker thread has to check the id at startup */
void getID(iguanaDev *idev);
/* start a thread to handle a single device instance */
void startWorker(deviceInfo *info);
/* terminate and join with each child thread */
bool reapAllChildren(deviceList *list);

#endif
