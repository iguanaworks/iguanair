/****************************************************************************
 ** client-interface.c ******************************************************
 ****************************************************************************
 *
 * Implements a translation and checking layer for data coming from
 * client applications to the igdaemon driver.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "iguanaIR.h"
#include "compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#ifndef WIN32
    #include <arpa/inet.h>
#endif

#include "support.h"
#include "pipes.h"
#include "dataPackets.h"
#include "device-interface.h"
#include "usbclient.h"
#include "client-interface.h"
#include "protocol-versions.h"

enum
{
    /* highest value of a data byte in the IR code protocol */
    MAX_DATA_BYTE = 127
};

/* small structure passed through a void* for tellReceivers. */
typedef struct receiveInfo
{
    struct dataPacket *packet;
    bool translated;
} receiveInfo;

bool readLabels = true;

static unsigned char* pulsesToIguanaSend(int carrier,
                                         uint32_t *sendCode, int *length)
{
    int x, codeLength = 0, inSpace = 0;
    unsigned char *codes = NULL;

    /* convert each pulse */
    for(x = 0; x < *length; x++)
    {
        uint32_t cycles, numBytes;

/* occasionally useful for debugging transmission issues */
#if 0
        fprintf(stderr, "%3d ", x);
        if (x % 2)
            fprintf(stderr, "space ");
        else
            fprintf(stderr, "pulse ");
        fprintf(stderr, "%d\n", sendCode[x] & IG_PULSE_MASK);
#endif

        cycles = (uint32_t)((sendCode[x] & IG_PULSE_MASK) / 
                            1000000.0 * carrier + 0.5);
        numBytes = (cycles / MAX_DATA_BYTE) + 1;
        cycles %= MAX_DATA_BYTE;
        if (cycles == 0)
        {
            cycles = MAX_DATA_BYTE;
            numBytes -= 1;
        }

        if (numBytes)
        {
            /* allocate space as we go */
            codes = realloc(codes, sizeof(char) * (codeLength + numBytes));

            /* populate the buffer with max bytes */
            memset(codes + codeLength,
                   LENGTH_MASK | (inSpace * STATE_MASK),
                   numBytes - 1);
            if (inSpace)
                cycles |= STATE_MASK;

            /* store the last byte (cast is alright due to %= MAX_DATA_BYTE) */
            codes[codeLength + numBytes - 1] = (unsigned char)cycles;
            codeLength += numBytes;
        }

        inSpace ^= 1;
    }

    *length = codeLength;
    return codes;
}

static bool handleClientRequest(dataPacket *request, client *target)
{
    bool retval = false;
    dataPacket *response = NULL;

    /* translate the newly read data packet code */
    if (! translateClient(&request->code, target->version, true))
        return false;

    /* return false if the incoming packet does not match the protocol */
    if (checkIncomingProtocol(target->idev, request, false) == NULL)
        return false;

    switch(request->code)
    {
    /* this should be the first packet sent by new clients */
    case IG_EXCH_VERSIONS:
    {
        uint16_t *version = ((uint16_t*)(request->data));
        message(LOG_INFO,
                "Found client using protocol version %d\n", *version);
        target->version = *version;
        *version = IG_PROTOCOL_VERSION;
        retval = true;
        break;
    }

    case IG_DEV_RECVON:
    case IG_DEV_RAWRECVON:
        request->code = IG_DEV_RECVON;
        if (target->idev->receiverCount > 0)
            retval = true;
        break;

    case IG_DEV_RECVOFF:
        target->receiving = 0;
        if (target->idev->receiverCount > 0)
        {
            target->idev->receiverCount--;
            if (target->idev->receiverCount > 0)
                retval = true;
        }
        break;

    case IG_DEV_GETCHANNELS:
        request->data = (unsigned char*)malloc(1);
        request->data[0] = target->idev->channels >> 4;
        request->dataLen = 1;
        retval = true;
        break;

    case IG_DEV_SETCHANNELS:
        target->idev->channels = request->data[0] << 4;
        retval = true;
        break;

    case IG_DEV_GETCARRIER:
        request->data = (unsigned char*)malloc(4);
        *(uint32_t*)request->data = htonl(target->idev->carrier);
        request->dataLen = 4;
        retval = true;
        break;

    case IG_DEV_SETCARRIER:
        target->idev->carrier = ntohl(*(uint32_t*)request->data);

        if (target->idev->carrier > 150000)
        {
            message(LOG_WARN,
                    "Frequency higher than 150Khz.  Using 150kHz.\n");
            target->idev->carrier = 150000;
        }
        else if (target->idev->carrier < 25000)
        {
            message(LOG_WARN,
                    "Frequency lower than 25Khz.  Using 25kHz.\n");
            target->idev->carrier = 25000;
        }
        *(uint32_t*)request->data = htonl(target->idev->carrier);

        retval = true;
        break;

    case IG_DEV_IDSOFF:
        readLabels = false;
        retval = true;
        break;

    case IG_DEV_IDSON:
        readLabels = true;
        retval = true;
        break;

    case IG_DEV_SEND:
    {
        unsigned char *codes;
        request->dataLen /= sizeof(uint32_t);
        codes = pulsesToIguanaSend(target->idev->carrier,
                                   (uint32_t*)request->data,
                                   &request->dataLen);
        free(request->data);
        request->data = codes;
        break;
    }
    }

    if (retval)
        message(LOG_INFO,
                "Request handled within daemon: 0x%x\n", request->code);
    else if (! deviceTransaction(target->idev, request, &response))
    {
        if (request->code == IG_DEV_RESET)
        {
            if (usb_reset(getDevHandle(target->idev->usbDev)) != 0)
                message(LOG_ERROR, "Hard reset failed\n");
            else
                retval = true;
        }
        else
            message(LOG_ERROR,
                    "Device transaction (0x%2.2x) failed\n", request->code);
    }
    else
    {
        /* need to know which clients to contact on a receive */
        if (request->code == IG_DEV_RECVON)
        {
            target->idev->receiverCount++;
            target->receiving = request->code;
        }

        if (response != NULL)
        {
            /* pack data for the client */
            free(request->data);
            request->data = response->data;
            request->dataLen = response->dataLen;
            free(response);
        }

        retval = true;
    }

    /* translate the newly read data packet code before returning */
    if (! translateClient(&request->code, target->version, false))
        return false;

    return retval;
}

static void releaseClient(client *target)
{
    closePipe(target->fd);
#if DEBUG
printf("CLOSE %d %s(%d)\n", target->fd, __FILE__, __LINE__);
#endif

    if (target->receiving)
    {
        target->idev->receiverCount--;
        if (target->idev->receiverCount == 0)
        {
            dataPacket request = DATA_PACKET_INIT;
            message(LOG_INFO,
                    "No more receivers, turning off the receiver.\n");

            request.code = IG_DEV_RECVOFF;
            if (! deviceTransaction(target->idev, &request, NULL))
                message(LOG_ERROR, "Failed to disable the receiver.\n");
        }
    }

    free(removeItem((itemHeader*)target));
}

static bool handleClient(client *me)
{
    bool retval = true;
    dataPacket request;

    if (! readDataPacket(&request, me->fd,
                         me->idev->usbDev->list->recvTimeout))
    {
        releaseClient(me);
        retval = false;
    }
    else
    {
        if (! handleClientRequest(&request, me))
        {
            request.code = IG_DEV_ERROR;
            request.dataLen = -errno;
            message(LOG_ERROR,
                    "handleClientRequest(0x%2.2x) failed with: %d (%s)\n",
                    request.code, errno, strerror(errno));
        }

        if (! writeDataPacket(&request, me->fd))
        {
            message(LOG_INFO, "FAILED to write packet back to client: 0x%x\n",
                    request.code);
            releaseClient(me);
            retval = false;
        }
        else
        {
            dataPacket *packet = &request;
            message(LOG_DEBUG3, "Successfully wrote packet: ");
            appendHex(LOG_DEBUG3, (char*)packet + offsetof(dataPacket, code),
                      offsetof(dataPacket, data) - offsetof(dataPacket, code));
            if (packet->dataLen > 0)
                appendHex(LOG_DEBUG3, packet->data, packet->dataLen);
        }

        free(request.data);
    }

    return retval;
}

static char* getID(iguanaDev *idev)
{
    dataPacket request = DATA_PACKET_INIT, *response = NULL;
    char *retval = NULL;

    request.code = IG_DEV_GETID;
    if (! deviceTransaction(idev, &request, &response))
        message(LOG_INFO,
                "Failed to get id.  Device may not have one assigned.\n");
    else
    {
        retval = (char*)iguanaRemoveData(response, NULL);
        freeDataPacket(response);
    }

    return retval;
}

static void joinWithReader(iguanaDev *idev)
{
    void *exitVal;

    /* signal then join with the reader */
    idev->quitRequested = true;
    joinThread(idev->reader, &exitVal);
}

static void clientConnected(PIPE_PTR clientFd, iguanaDev *idev)
{
#if DEBUG
printf("OPEN %d %s(%d)\n", clientFd, __FILE__, __LINE__);
#endif

    if (clientFd == INVALID_PIPE)
        message(LOG_ERROR, "Error accepting client: %s\n", strerror(errno));
    else
    {
        client *newClient;

        newClient = (client*)malloc(sizeof(client));
        if (newClient == NULL)
            message(LOG_ERROR, "Out of memory allocating client struct.");
        else
        {
            /*
            int flags;

            flags = fcntl(clientFd, F_GETFL);
            if (flags == -1)
                message(LOG_ERROR,
                        "Failed read status flags for client socket.\n");
            else if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) == -1)
                message(LOG_ERROR,
                        "Failed to set client socket to non-blocking mode.\n");
            else*/
            {
                memset(newClient, 0, sizeof(client));
                newClient->idev = idev;
                newClient->receiving = 0;
                newClient->fd = clientFd;
                insertItem(&idev->clientList, NULL, (itemHeader*)newClient);
            }
        }
    }
}

static bool tellReceivers(itemHeader *item, void *userData)
{
    client *me = (client*)item;
    receiveInfo *info = (receiveInfo*)userData;

    if ((me->receiving == IG_DEV_RECVON    &&   info->translated) ||
        (me->receiving == IG_DEV_RAWRECVON && ! info->translated))
    {
        /* translate the packet code before returning it */
        if (! translateClient(&info->packet->code, me->version, false))
            return false;

        if (! writeDataPacket(info->packet, me->fd))
            message(LOG_ERROR, "Failed to send packet to receiver: %d: %s\n",
                    errno, strerror(errno));
        else
        {
            message(LOG_DEBUG3, "Sent receivers: ");
            appendHex(LOG_DEBUG3,
                      (char*)info->packet + offsetof(dataPacket, code),
                      offsetof(dataPacket, data) - offsetof(dataPacket, code));
            if (info->packet->dataLen > 0)
                appendHex(LOG_DEBUG3,
                          info->packet->data, info->packet->dataLen);
        }

        /* translate the packet code BACK before continuing */
        if (! translateClient(&info->packet->code, me->version, true))
            return false;
    }

    return true;
}

static bool handleReader(iguanaDev *idev)
{
    bool retval = false;
    char byte;

    switch(readPipe(idev->readerPipe[READ], &byte, 1))
    {
    case -1:
        message(LOG_ERROR, "Error reading from readPipe.\n");
        break;

    /* check for removal of usb device (or shutdown) */
    case 0:
        break;

    case 1:
    {
        dataPacket *packet;
        receiveInfo info;

        packet = removeNextPacket(idev);
        switch(packet->code)
        {
        case IG_DEV_RECV:
        {
            uint32_t *pulses;

            /* inform any users that want raw receive data */
            info.packet = packet;
            info.translated = false;
            forEach(&idev->clientList, tellReceivers, &info);

            /* translate, then tell interested users about the data */
            pulses = iguanaDevToPulses(packet->data, &packet->dataLen);
            free(packet->data);
            packet->data = (unsigned char*)pulses;

            info.translated = true;
            forEach(&idev->clientList, tellReceivers, &info);
            break;
        }

        case IG_DEV_OVERRECV:
            message(LOG_ERROR, "Receive too large from USB device.\n");
            info.packet = packet;
            info.translated = false;
            forEach(&idev->clientList, tellReceivers, &info);
            break;

/*
        case IG_DEV_BIGSEND:
            message(LOG_ERROR, "Send too large for USB device.\n");
            break;
*/

        default:
            message(LOG_ERROR,
                    "Unexpected code (0x%x) with %d data bytes from usb\n",
                    packet->code, packet->dataLen);
            if (packet->dataLen != 0)
                appendHex(LOG_ERROR, packet->data, packet->dataLen);
        }

        freeDataPacket(packet);
        retval = true;
        break;
    }
    }
    return retval;
}

static void* workLoop(void *instance)
{
    iguanaDev *idev = (iguanaDev*)instance;

#ifndef WIN32
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        message(LOG_ERROR, "Failed when setting SIG_IGN for SIGPIPE.\n");
#endif

    message(LOG_INFO, "Worker %d starting\n", idev->usbDev->id);
    if (checkVersion(idev))
    {
        char name[4], *alias = NULL;

        snprintf(name, 4, "%d", idev->usbDev->id);
        if (readLabels)
            alias = getID(idev);

        listenToClients(name, alias, idev,
                        handleReader, clientConnected, handleClient);

        /* Close some of the pipes.  Leave one to note when the device
           reader exits. */
        closePipe(idev->readerPipe[READ]);
        closePipe(idev->responsePipe[READ]);
        closePipe(idev->responsePipe[WRITE]);
#if DEBUG
printf("CLOSE %d %s(%d)\n", idev->readerPipe[READ], __FILE__, __LINE__);
printf("CLOSE %d %s(%d)\n", idev->responsePipe[READ], __FILE__, __LINE__);
printf("CLOSE %d %s(%d)\n", idev->responsePipe[WRITE], __FILE__, __LINE__);
#endif
    }

    /* release resources for reader and usb device */
    joinWithReader(idev);
    releaseDevice(idev->usbDev);

    message(LOG_INFO, "Worker %d exiting\n", idev->usbDev->id);
    if (writePipe(idev->usbDev->list->childPipe[WRITE],
                  &idev->worker, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "Failed to write thread id to childPipe.\n");

    /* now actually free the malloc'd data */
    free(idev->usbDev);

    /* go ahead and free the idev since the thread id has been written
       to the main application thread for reaping. */
    free(idev);
    makeParentJoin();

    return NULL;
}

void startWorker(usbDevice *dev)
{
    iguanaDev *idev = NULL;

    idev = (iguanaDev*)malloc(sizeof(iguanaDev));
    if (idev == NULL)
        message(LOG_ERROR,
                "Out of memory allocating iguanaDev for %d\n, dev->id");
    else
    {
        memset(idev, 0, sizeof(iguanaDev));
        idev->carrier = 38000;
        InitializeCriticalSection(&idev->listLock);
#ifdef LIBUSB_NO_THREADS
        InitializeCriticalSection(&idev->devLock);
#endif
        if (! createPipePair(idev->readerPipe))
            message(LOG_ERROR,
                    "Failed to create readPipe for %d: %s\n",
                    dev->id, strerror(errno));
        else if (! createPipePair(idev->responsePipe))
            message(LOG_ERROR,
                    "Failed to create responsePipe for %d: %s\n",
                    dev->id, strerror(errno));
        else
        {
#if DEBUG
printf("OPEN %d %s(%d)\n", idev->readerPipe[0], __FILE__, __LINE__);
printf("OPEN %d %s(%d)\n", idev->readerPipe[1], __FILE__, __LINE__);
printf("OPEN %d %s(%d)\n", idev->responsePipe[0], __FILE__, __LINE__);
printf("OPEN %d %s(%d)\n", idev->responsePipe[1], __FILE__, __LINE__);
#endif

            /* this must be set before the call to findDeviceEndpoints */
            idev->usbDev = dev;

            if (! findDeviceEndpoints(idev))
                message(LOG_ERROR,
                        "Failed find device endpoints for %d\n", dev->id);
            else if (! startThread(&idev->reader,
                                   (void *(*)(void*))handleIncomingPackets,
                                   idev))
                message(LOG_ERROR,
                        "Failed to create reader thread %d\n", dev->id);
            else
            {
                if (! startThread(&idev->worker, workLoop, idev))
                    message(LOG_ERROR,
                            "Failed to create worker thread %d\n", dev->id);
                else
                    /* return on success to skip cleanup */
                    return;

                joinWithReader(idev);
            }
        }
        free(idev);
    }
    releaseDevice(dev);
}

bool reapAllChildren(usbDeviceList *list)
{
    unsigned int x;
    /* stop all the readers and thereby the workers */
    x = releaseDevices(list);

    for(; x > 0; x--)
    {
        void *exitval;
        int result;
        THREAD_PTR child;

        /* NOTE: using 2*recv timeout to allow readers to exit. */
        result = readPipeTimed(list->childPipe[READ],
                               (char*)&child, sizeof(THREAD_PTR),
                               2 * list->recvTimeout);
        /* no one ready */
        if (result == 0)
            break;
        /* try to join with the worker thread */
        else if (result != sizeof(THREAD_PTR))
        {
            message(LOG_ERROR, "failed while reaping worker thread.\n");
            return false;
        }
        else
        {
            joinThread(child, &exitval);
            message(LOG_DEBUG, "Reaped child: %p\n", child);
        }
    }

    return true;
}
