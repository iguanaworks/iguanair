/****************************************************************************
 ** protocol.h **************************************************************
 ****************************************************************************
 *
 * Definitions for the Iguanaworks device protocol.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#ifndef _PROTOCOL_
#define _PROTOCOL_

#include "list.h"
#include "config.h"

enum
{
    /* masks for encoding and decoding the usb data */
    STATE_MASK  = 0x80,
    LENGTH_MASK = 0x7F,

    /* other internal constants */
    UNKNOWN_FEATURES = 0xFF
};

/* forward declaration */
struct dataPacket;

typedef struct deviceSettings
{
    /* a method for children to communicate back to the list owner */
    PIPE_PTR childPipe[2];

    /* time outs for send and receive */
    unsigned int recvTimeout;
    unsigned int sendTimeout;
} deviceSettings;

typedef struct iguanaDev
{
    /* used to pass data from the reader to the worker, closed to
     * notify worker to terminate */
    PIPE_PTR readerPipe[2];
    PIPE_PTR responsePipe[2];

    /* maximum packet size for send and recv */
    int maxPacketSize;

    /* usb device pointer */
    struct deviceInfo *usbDev;

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

    /* sometimes we need to know the feature set */
    unsigned char features, cycles;

    /* what channels should we use in transmit? default 0 ==> ALL*/
    unsigned char channels;

    /* what carrier frequency should we transmit at?  default 38k */
    unsigned int carrier;

    /* might as well keep the list of clients here */
    listHeader clientList;

    /* link to the global settings object */
    deviceSettings *settings;

#ifdef LIBUSB_NO_THREADS_OPTION
    bool libusbNoThreads;
#endif

#ifdef LIBUSB_NO_THREADS
    /* if necessary lock access to the underlying usb device */
    LOCK_PTR devLock;
    bool needToWrite;
#endif
} iguanaDev;

/* use the protocol table to see if the version is supported */
bool checkVersion(iguanaDev *idev);

/* check that the device features match the targetSet */
bool checkFeatures(iguanaDev *idev, unsigned char targetSet);

/* check that the client is using the proper protocol */
struct packetType* checkIncomingProtocol(iguanaDev *idev,
                                         struct dataPacket *request,
                                         bool nullResponse);

/* do a transfer and receive the response */
bool deviceTransaction(iguanaDev *idev,
                       struct dataPacket *request,
                       struct dataPacket **response);

/* for using data on the packet list */
struct dataPacket* removeNextPacket(iguanaDev *idev);

/* read incoming data into the buffer */
void handleIncomingPackets(iguanaDev *idev);

/* translation of data for transmission */
uint32_t* iguanaDevToPulses(unsigned char *code, int *length);

#endif
