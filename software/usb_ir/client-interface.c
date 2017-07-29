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
#include "compat.h"
#include "sendFormat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <signal.h>

#ifndef WIN32
    #include <arpa/inet.h>
#endif

#include "pipes.h"
#include "dataPackets.h"
#include "driver.h"
#include "device-interface.h"
#include "client-interface.h"
#include "protocol-versions.h"
#include "server.h"

/* small structure passed through a void* for tellReceivers. */
typedef struct receiveInfo
{
    struct dataPacket *packet;
    bool translated;
} receiveInfo;

static bool handleClientRequest(dataPacket *request, client *target)
{
    bool retval = false;
    dataPacket *response = NULL;
    int compressVersion;

    /* translate the newly read data packet code */
    if (! translateClient(&request->code, target->version, true))
        return false;

    /* return false if the incoming packet does not match the protocol */
    if (checkIncomingProtocol(target->idev, request, false) == NULL)
        return false;

    /* figure out what version of the compression we support */
    compressVersion = COMPRESS_VER0;
    if (target->idev != NULL && (target->idev->version & 0xFF) >= 0x08)
        compressVersion = COMPRESS_VER1;

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

    case IG_CTL_LISTDEVS:
        request->data = (unsigned char*)deviceSummary();
        if (request->data == NULL)
            request->dataLen = 0;
        else
            request->dataLen = strlen((char*)request->data) + 1;
        retval = true;
        break;

    case IG_DEV_GETFEATURES:
        /* shortcut the request if possible */
        if (checkFeatures(target->idev, UNKNOWN_FEATURES))
        {
            request->data = (unsigned char*)malloc(1);
            request->data[0] = target->idev->features;
            request->dataLen = 1;
            retval = true;
        }
        else
            message(LOG_ERROR, "checkFeatures failed: %s\n", translateError(errno));
        break;

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
        if (checkFeatures(target->idev, IG_SLOT_DEV))
            request->data[0] = target->idev->channels >> 2;
        else
            request->data[0] = target->idev->channels >> 4;
        request->dataLen = 1;
        retval = true;
        break;

    case IG_DEV_SETCHANNELS:
        if (checkFeatures(target->idev, IG_SLOT_DEV))
            target->idev->channels = request->data[0] << 2;
        else
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
        srvSettings.readLabels = false;
        retval = true;
        break;

    case IG_DEV_IDSON:
        srvSettings.readLabels = true;
        retval = true;
        break;

    case IG_DEV_GETLOCATION:
        request->data = (unsigned char*)malloc(2);
        getDeviceLocation(target->idev->usbDev, (uint8_t*)request->data);
        request->dataLen = 2;
        retval = true;
        break;

    case IG_DEV_IDSTATE:
        request->data = (unsigned char*)malloc(1);
        request->data[0] = (unsigned char)srvSettings.readLabels;
        request->dataLen = 1;
        retval = true;
        break;

    case IG_DEV_SEND:
    {
        unsigned char *codes;
        request->dataLen /= sizeof(uint32_t);
        request->dataLen = pulsesToIguanaSend(target->idev->carrier,
                                              (uint32_t*)request->data,
                                              request->dataLen,
                                              &codes,
                                              compressVersion);
        free(request->data);
        request->data = codes;
        break;
    }

    case IG_DEV_SENDSIZE:
    {
        /* translate the passed signals into codes that the device
           will understand, but just compute the length of the encoded
           signal */
        request->dataLen /= sizeof(uint32_t);
        request->dataLen = pulsesToIguanaSend(target->idev->carrier,
                                              (uint32_t*)request->data,
                                              request->dataLen,
                                              NULL,
                                              compressVersion);

        /* return the computed size */
        request->data = (unsigned char*)realloc(request->data,
                                                sizeof(uint16_t));
        ((uint16_t*)request->data)[0] = request->dataLen;
        request->dataLen = sizeof(uint16_t);
        retval = true;
        break;
    }

    case IG_DEV_LISTALIASES:
        request->data = (unsigned char*)aliasSummary(target->idev);
        if (request->data == NULL)
        {
            request->dataLen = 0;
            message(LOG_ERROR, "Avoiding a crash, but no device can have NO aliases.\n");
        }
        else
            request->dataLen = strlen((char*)request->data) + 1;
        retval = true;
        break;

    case IG_DEV_GETADDRESS:
        request->data = (unsigned char*)strdup(target->idev->addrStr);
        request->dataLen = strlen((char*)request->data) + 1;
        retval = true;
        break;
    }

    if (retval)
        message(LOG_INFO,
                "Request handled within daemon: 0x%x\n", request->code);
    else if (target->idev == NULL)
        message(LOG_ERROR, "Unknown request from ctl interface.\n");
    else if (! deviceTransaction(target->idev, request, &response))
    {
        if (request->code == IG_DEV_RESET)
        {
            clearHalt(target->idev->usbDev, EP_IN);
            if (resetDevice(target->idev->usbDev) != 0)
                message(LOG_ERROR, "Hard reset failed\n");
            else
                retval = true;
        }
        else
            /* TODO: changes the errno to 9 sometimes.  need to store the error somewhere before this */
            message(LOG_ERROR,
                    "Device transaction (0x%2.2x) failed\n", request->code);
    }
    else
        retval = true;

    /* This is separate from the above else to accommodate internally
       handled commands. */
    if (retval)
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
    }

    /* translate the newly read data packet code before returning */
    if (! translateClient(&request->code, target->version, false))
        retval = false;

    return retval;
}

void releaseClient(client *target)
{
    closePipe(target->fd);
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", target->fd, __FILE__, __LINE__);
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

bool handleClient(client *me)
{
    bool retval = true;
    dataPacket request;

    if (! readDataPacket(&request, me->fd, srvSettings.devSettings.recvTimeout))
    {
        releaseClient(me);
        retval = false;
    }
    else
    {
        if (! handleClientRequest(&request, me))
        {
            message(LOG_ERROR,
                    "handleClientRequest(0x%2.2x) failed with: %d (%s)\n",
                    request.code, errno, translateError(errno));
            request.code = IG_DEV_ERROR;
            request.dataLen = -errno;
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

        /* for SETID calls we need to do a GETID to then update the
           aliases correctly */
        if (request.code == IG_DEV_SETID)
            getID(me->idev);
        free(request.data);
    }

    return retval;
}

void getID(iguanaDev *idev)
{
    char buf[13] = {0};
    uint8_t loc[2];
    dataPacket request = DATA_PACKET_INIT, *response = NULL;

    /* add an alias based on bus location */
    getDeviceLocation(idev->usbDev, loc);
    sprintf(buf, "%d-%d", loc[0], loc[1]);
    setAlias(idev, true, buf);
    free(idev->locAlias);
    idev->locAlias = strdup(buf);

    if (! srvSettings.readLabels ||
        /* reflasher and loader-only devices have no id */
        idev->version == 0x00FF || (idev->version & 0x00FF) == 0x0000)
        return;

    request.code = IG_DEV_GETID;
    /* NOTE: trigger a dummy call because in early (pre 5) body
       firmware the first call fails. */
    if ((idev->version & 0xFF00) &&
        (idev->version & 0x00FF) < 0x05)
        deviceTransaction(idev, &request, &response);

    /* use any alias the user has set */
    if (! deviceTransaction(idev, &request, &response))
        message(LOG_INFO,
                "Failed to get id.  Device may not have one assigned.\n");
    else
    {
        /* make sure we properly terminate the id in case someone sets
           one of maximum length */
        memset(buf, 0, 13);
        strncpy(buf, (char*)response->data, 12);
        setAlias(idev, false, buf);
        free(idev->userAlias);
        idev->userAlias = strdup(buf);
        freeDataPacket(response);
    }
}

static void joinWithReader(iguanaDev *idev)
{
    void *exitVal;

    /* signal then join with the reader */
    idev->quitRequested = true;
    joinThread(idev->reader, &exitVal);
}

void clientConnected(PIPE_PTR clientFd, listHeader *clientList, iguanaDev *idev)
{
#if DEBUG
message(LOG_WARN, "OPEN %d %s(%d)\n", clientFd, __FILE__, __LINE__);
#endif

    if (clientFd == INVALID_PIPE)
        message(LOG_ERROR,
                "Error accepting client: %s\n", translateError(errno));
    else
    {
        client *newClient;

        newClient = (client*)malloc(sizeof(client));
        if (newClient == NULL)
            message(LOG_ERROR, "Out of memory allocating client struct.");
        else if (! setNonBlocking(clientFd))
            message(LOG_ERROR,
                    "Failed to set client socket to non-blocking mode.\n");
        else
        {
            memset(newClient, 0, sizeof(client));
            newClient->idev = idev;
            newClient->receiving = 0;
            newClient->fd = clientFd;
            insertItem(clientList, NULL, (itemHeader*)newClient);
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
        {
            message(LOG_ERROR, "Failed to send packet to receiver: %d: %s\n",
                    errno, translateError(errno));
            if (errno == 11)
                message(LOG_WARN, "A client application may not be closing connections to the daemon.\n");
        }
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

bool handleReader(iguanaDev *idev)
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
    THREAD_PTR thread;

#ifndef WIN32
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        message(LOG_ERROR, "Failed when setting SIG_IGN for SIGPIPE.\n");
#endif

    message(LOG_INFO, "Worker %d starting\n", idev->usbDev->id);
    if (checkVersion(idev))
    {
        char name[4];

        /* add this device to the list of devices */
        EnterCriticalSection(&srvSettings.devsLock);
        insertItem(&srvSettings.devs, NULL, (itemHeader*)idev);
        LeaveCriticalSection(&srvSettings.devsLock);

        /* start the listener */
        sprintf(name, "%d", idev->usbDev->id);
        listenToClients(name, &idev->clientList, idev);

        /* Close some of the pipes but leave one to mark when the
           device reader exits. */
        closePipe(idev->readerPipe[READ]);
        closePipe(idev->responsePipe[READ]);
        closePipe(idev->responsePipe[WRITE]);
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", idev->readerPipe[READ], __FILE__, __LINE__);
message(LOG_WARN, "CLOSE %d %s(%d)\n", idev->responsePipe[READ], __FILE__, __LINE__);
message(LOG_WARN, "CLOSE %d %s(%d)\n", idev->responsePipe[WRITE], __FILE__, __LINE__);
#endif

        /* remove this from the active device list */
        EnterCriticalSection(&srvSettings.devsLock);
        removeItem((itemHeader*)idev);
        LeaveCriticalSection(&srvSettings.devsLock);
    }

    /* log the shutdown and grab a copy of the thread id for later */
    message(LOG_INFO, "Worker %d exiting\n", idev->usbDev->id);
    thread = idev->worker;

    /* release resources for reader and usb device */
    joinWithReader(idev);
    releaseDevice(idev->usbDev);
    freeDevice(idev->usbDev);
    free(idev->locAlias);
    free(idev->userAlias);
    free(idev);

    /* tell the parent thread to go ahead and reclaim our resources */
    makeParentJoin(thread);

    return NULL;
}

void startWorker(deviceInfo *info)
{
    iguanaDev *idev = NULL;

    idev = (iguanaDev*)malloc(sizeof(iguanaDev));
    if (idev == NULL)
        message(LOG_ERROR,
                "Out of memory allocating iguanaDev for %d\n, dev->id");
    else
    {
        memset(idev, 0, sizeof(iguanaDev));
        idev->features = UNKNOWN_FEATURES;
        idev->settings = (deviceSettings*)info->type.data;
        idev->carrier = 38000;
        InitializeCriticalSection(&idev->listLock);
#ifdef LIBUSB_NO_THREADS_OPTION
        idev->libusbNoThreads = srvSettings.noThreads;
#endif
#ifdef LIBUSB_NO_THREADS
        InitializeCriticalSection(&idev->devLock);
#endif
        if (! createPipePair(idev->readerPipe))
            message(LOG_ERROR,
                    "Failed to create readPipe for %d: %s\n",
                    info->id, translateError(errno));
        else if (! createPipePair(idev->responsePipe))
            message(LOG_ERROR,
                    "Failed to create responsePipe for %d: %s\n",
                    info->id, translateError(errno));
        else
        {
#if DEBUG
message(LOG_WARN, "OPEN %d %s(%d)\n", idev->readerPipe[READ],    __FILE__, __LINE__);
message(LOG_WARN, "OPEN %d %s(%d)\n", idev->readerPipe[WRITE],   __FILE__, __LINE__);
message(LOG_WARN, "OPEN %d %s(%d)\n", idev->responsePipe[READ],  __FILE__, __LINE__);
message(LOG_WARN, "OPEN %d %s(%d)\n", idev->responsePipe[WRITE], __FILE__, __LINE__);
#endif

            /* this must be set before the call to findDeviceEndpoints */
            idev->usbDev = info;

            if (! findDeviceEndpoints(idev->usbDev, &idev->maxPacketSize))
                message(LOG_ERROR,
                        "Failed find device endpoints for %d\n", info->id);
            else if (! startThread(&idev->reader,
                                   (void *(*)(void*))handleIncomingPackets,
                                   idev))
                message(LOG_ERROR,
                        "Failed to create reader thread %d\n", info->id);
            else
            {
                if (! startThread(&idev->worker, workLoop, idev))
                    message(LOG_ERROR,
                            "Failed to create worker thread %d\n", info->id);
                else
                    /* return on success to skip cleanup */
                    return;

                joinWithReader(idev);
            }
        }
        free(idev);
    }
    releaseDevice(info);
}

bool reapAllChildren(deviceList *list)
{
    unsigned int x;

    /* stop all the readers and thereby the workers */
    x = stopDevices(list);
    while(x > 0)
    {
        void *exitVal;
        int result;
        THREAD_PTR child;

        /* NOTE: using 2*recv timeout to allow readers to exit. TODO: we might want to move this to server.c  */
        result = readPipeTimed(srvSettings.commPipe[READ],
                               (char*)&child, sizeof(THREAD_PTR),
                               2 * srvSettings.devSettings.recvTimeout);
        /* no one ready in time, break out */
        if (result == 0)
        {
            message(LOG_WARN, "failed to join %d device threads\n");
            break;
        }
        /* confirm that we read a full THREAD_PTR */
        else if (result != sizeof(THREAD_PTR))
        {
            message(LOG_ERROR, "failed while reaping worker thread.\n");
            return false;
        }
        /* try to join with the requested worker thread */
        else if (child != INVALID_THREAD_PTR)
        {
            joinThread(child, &exitVal);
            x--;
            message(LOG_DEBUG, "Reaped worker: %p\n", child);
        }
        /* discard the scan/quit/etc. command and read again */
        else
            result = readPipe(srvSettings.commPipe[READ], &child, sizeof(THREAD_PTR));
    }
    releaseDevices(list);

    return true;
}
