/****************************************************************************
 ** igdaemon.h **************************************************************
 ****************************************************************************
 *
 * A couple functions used to interface with Iguanaworks USB devices.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */

#ifndef _IGDAEMON_
#define _IGDAEMON_

typedef struct receiveInfo
{
    struct dataPacket *packet;
    bool translated;
} receiveInfo;

typedef struct client
{
    itemHeader header;

    /* which iguana device is this associated with? */
    iguanaDev *idev;

    /* for communication with the client */
    PIPE_PTR fd;

    /* whether recv-related messages should be returned to this
     * client */
    int receiving;

    /* protocol version that should be used with this client */
    uint16_t version;
} client;

typedef bool (*handleReaderFunc)(iguanaDev *idev);
typedef void (*clientConnectedFunc)(PIPE_PTR who, iguanaDev *idev);
typedef bool (*handleClientFunc)(client *me);

void listenToClients(char *name, char *alias, iguanaDev *idev,
                     handleReaderFunc handleReader,
                     clientConnectedFunc clientConnected,
                     handleClientFunc handleClient);

/* start a thread to handle a single device instance */
void startWorker(usbDevice *dev);
/* terminate and join with each child thread */
bool reapAllChildren(usbDeviceList *list);

extern bool readLabels;

#endif
