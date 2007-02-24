/****************************************************************************
 ** protocol.h **************************************************************
 ****************************************************************************
 *
 * Definitions for the Iguanaworks device protocol.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#ifndef _PROTOCOL_
#define _PROTOCOL_

#include "list.h"

/* forward declaration */
struct dataPacket;

typedef struct iguanaDev
{
    /* used to pass data from the reader to the worker, closed to
     * notify worker to terminate */
    PIPE_PTR readerPipe[2];
    PIPE_PTR responsePipe[2];

    /* maximum packet size for send and recv */
    int maxPacketSize;

    /* usb device pointer */
    struct usbDevice *usbDev;

    /* so we can join with the reader thread */
    THREAD_PTR worker, reader;
    bool quitRequested;

    /* all data will go in this list as it's read */
    listHeader recvList;
    struct dataPacket *response;

    /* how many clients are currently receiving? */
    unsigned int receiverCount;

    /* must lock the list of received packets */
    LOCK_PTR listLock;

    /* need to know what protocol version to support. */
    uint16_t version;

    /* might as well keep the list of clients here */
    listHeader clientList;

#ifdef LIBUSB_NO_THREADS
    /* if necessary lock access to the underlying usb device */
    LOCK_PTR devLock;
    bool needToWrite;
#endif
} iguanaDev;

/* do a transfer and receive the response */
bool deviceTransaction(iguanaDev *idev,
                       struct dataPacket *request,
                       struct dataPacket **response);

/* for using data on the packet list */
struct dataPacket* removeNextPacket(iguanaDev *idev);

/* read incoming data into the buffer */
void handleIncomingPackets(iguanaDev *idev);

/* dunno where else to put this */
bool findDeviceEndpoints(iguanaDev *idev);

/* translation of data for transmission */
uint32_t* iguanaDevToPulses(unsigned char *code, int *length);
unsigned char* pulsesToIguanaSend(uint32_t *sendCode, int *length);

#endif
