/****************************************************************************
 ** client-interface.h ******************************************************
 ****************************************************************************
 *
 * A couple functions used to interface with IguanaWorks USB devices.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#pragma once

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

/* API implemented by the daemon/service code */
void listenToClients(const char *name, listHeader *clientList, iguanaDev *idev);

/* API used by the daemon/service code */
void releaseClient(client *target);
bool handleReader(iguanaDev *idev);
void clientConnected(PIPE_PTR clientFd, listHeader *clientList, iguanaDev *idev);
bool handleClient(client *me);

/* the worker thread has to check the id at startup */
void getID(iguanaDev *idev);
/* start a thread to handle a single device instance */
void startWorker(deviceInfo *info);
/* terminate and join with each child thread */
bool reapAllChildren(deviceList *list);
