/****************************************************************************
 ** protocol.c **************************************************************
 ****************************************************************************
 *
 * The specification of the Iguanaworks device protocol.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#include "base.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "iguanaIR.h"
#include "pipes.h"
#include "support.h"
#include "usbclient.h"
#include "dataPackets.h"
#include "protocol.h"

/* internal protocol constants */
enum
{
    /* used in calls to check message */
    MAX_PACKET_SIZE = 8,
    MIN_CODE_LENGTH = 4,
    CODE_OFFSET     = 3,

    /* protocol control codes */
    /*IG_CTL_START      = 0x0000,*/
    CTL_TODEV      = 0xCD,
    /*CTL_FROMDEV    = 0xDC,*/
    IG_DEV_TO_MASK    = 0x0F,
    IG_DEV_FROM_MASK  = 0xF0,
    IG_DEV_ANY_CODE   = 0x00,

    /* constants for packetType table */
    NO_PAYLOAD  = 0xFF,
    ANY_PAYLOAD = 0x00,

    /* terminate the payload with this */
    CTL_ENDDATA    = 0x00,

    /* masks for encoding and decoding the usb data */
    STATE_MASK  = 0x80,
    LENGTH_MASK = 0x7F,
    CARRIER_RATE = 38000,
    MAX_PULSE_LENGTH = 1023, /* before translation */

    /* highest value of a data byte in the IR code protocol */
    MAX_DATA_BYTE = 127
};

typedef struct packetType
{
    unsigned char code;
    unsigned char direction;
    int outData;
    bool ack;
    int inData;
} packetType;

typedef struct versionedType
{
    int start, end;
    packetType type;
} versionedType;

/* packet type information */
static versionedType types[] =
{
    {0, 0, {IG_DEV_GETVERSION, CTL_TODEV,   NO_PAYLOAD,  true, 2}},
    {0, 0, {IG_DEV_SEND,       CTL_TODEV,   ANY_PAYLOAD, true, NO_PAYLOAD}},
    {0, 0, {IG_DEV_RECVON,     CTL_TODEV,   NO_PAYLOAD,  true, NO_PAYLOAD}},
    {0, 0, {IG_DEV_RECVOFF,    CTL_TODEV,   NO_PAYLOAD,  true, NO_PAYLOAD}},

    /* 1 bit per pin of state */
    {0, 0, {IG_DEV_GETPINS,    CTL_TODEV,   NO_PAYLOAD,  true, 2}},
    {0, 0, {IG_DEV_SETPINS,    CTL_TODEV,   2,           true, NO_PAYLOAD}},

    /* 1 byte per pin, in the register format */
    {0, 0, {IG_DEV_GETCONFIG0, CTL_TODEV,   NO_PAYLOAD,  true, 4}},
    {0, 0, {IG_DEV_SETCONFIG0, CTL_TODEV,   4,           true, NO_PAYLOAD}},
    {0, 0, {IG_DEV_GETCONFIG1, CTL_TODEV,   NO_PAYLOAD,  true, 4}},
    {0, 0, {IG_DEV_SETCONFIG1, CTL_TODEV,   4,           true, NO_PAYLOAD}},

    /* supporting functions */
    {0, 0, {IG_DEV_GETBUFSIZE, CTL_TODEV,   NO_PAYLOAD,  true,  1}},
    {0, 0, {IG_DEV_WRITEBLOCK, CTL_TODEV,   68,          true,  NO_PAYLOAD}},
    {0, 0, {IG_DEV_EXECUTE,    CTL_TODEV,   NO_PAYLOAD,  false, NO_PAYLOAD}},
    {2, 2, {IG_DEV_BULKPINS,   CTL_TODEV,   64,          true,  NO_PAYLOAD}},
    {3, 0, {IG_DEV_BULKPINS,   CTL_TODEV,   ANY_PAYLOAD, true,  NO_PAYLOAD}},
    {0, 0, {IG_DEV_GETID,      CTL_TODEV,   NO_PAYLOAD,  true,  12}},
    {0, 0, {IG_DEV_RESET,      CTL_TODEV,   NO_PAYLOAD,  false, NO_PAYLOAD}},

    /* "from device" codes */
    {0, 0, {IG_DEV_RECV,    IG_CTL_FROMDEV, NO_PAYLOAD,  false, ANY_PAYLOAD}},
    {0, 0, {IG_DEV_BIGSEND, IG_CTL_FROMDEV, NO_PAYLOAD,  false, NO_PAYLOAD}},
    {0, 0, {IG_DEV_BIGRECV, IG_CTL_FROMDEV, NO_PAYLOAD,  false, ANY_PAYLOAD}},

    /* terminate the list */
    {0}
};

static void queueDataPacket(iguanaDev *idev, dataPacket *current)
{
    message(LOG_DEBUG3, "Notifying of packet.\n");

    EnterCriticalSection(&idev->listLock);
    if (current->code & IG_DEV_FROM_MASK)
    {
        insertItem(&idev->recvList, NULL, (itemHeader*)current);
        if (! notify(idev->readerPipe[WRITE]))
            message(LOG_ERROR, "Failed to signal primary thread.\n");        
    }
    else
    {
        idev->response = current;
        if (! notify(idev->responsePipe[WRITE]))
            message(LOG_ERROR, "Failed to signal primary thread.\n");
    }
    LeaveCriticalSection(&idev->listLock);
}

static bool sendData(iguanaDev *idev,
                     const void *buffer, int size, bool addTerminator)
{
    bool retval = true;
    unsigned int x, count, lastSize;
    char *lastPacket = NULL;

    /* compute the packet count and size of last one */
    count = size / idev->maxPacketSize;
    lastSize = size % idev->maxPacketSize;

    /* append the terminator to the last packet is this is a send */
    if (addTerminator)
    {
        lastPacket = (char*)malloc(lastSize + 1);
        memcpy(lastPacket,
               (char*)buffer + count * idev->maxPacketSize, lastSize);
        lastPacket[lastSize++] = CTL_ENDDATA;
    }
    else if (lastSize > 0)
        lastPacket = (char*)buffer + count * idev->maxPacketSize;

    for(x = 0; x < count; x++)
        /* ensure we send idev->maxPacketSize bytes */
        if (interruptSend(idev->usbDev,
                          (char*)buffer + x * idev->maxPacketSize,
                          idev->maxPacketSize) != idev->maxPacketSize)
        {
            printError(LOG_ERROR, "failed to write data packet", idev->usbDev);
            retval = false;
            break;
        }

    /* send the last packet (with at least the data terminator) */
    if (retval && lastPacket != NULL &&
        interruptSend(idev->usbDev, lastPacket, lastSize) != lastSize)
    {
        printError(LOG_ERROR,
                   "failed to write final data packet", idev->usbDev);
        retval = false;
    }

    if (addTerminator)
        free(lastPacket);

    return retval;
}

/* find the appropriate type entry based on the code and the version */
static packetType* findTypeEntry(unsigned char code, uint16_t version)
{
    unsigned int x;

    for(x = 0; types[x].type.code != IG_DEV_ANY_CODE; x++)
        if (types[x].type.code == code &&
            types[x].start <= version &&
            (types[x].end >= version || types[x].end == 0))
            return &(types[x].type);

    return NULL;
}

static bool payloadMatch(unsigned char spec, unsigned char length)
{
    return ((spec == NO_PAYLOAD && length == 0) ||
            spec == ANY_PAYLOAD || spec == length);
}

bool deviceTransaction(iguanaDev *idev,      /* required */
                       dataPacket *request,  /* required */
                       dataPacket **response) /* optional */
{
    bool retval = false;
    packetType *type;

    type = findTypeEntry(request->code, idev->version);
    errno = EINVAL;
    if (type == NULL)
        message(LOG_ERROR,
                "Unknown packet type in request: 0x%x\n", request->code);
    else if (type->direction != CTL_TODEV)
        message(LOG_ERROR, "Cannot request to send a FROMDEV packet.\n");
    else if (! payloadMatch(type->outData, request->dataLen))
        message(LOG_ERROR,
                "Request size does not match type specification (%d != %d)\n",
                request->dataLen, type->outData);
    else if (type->inData != NO_PAYLOAD && response == NULL)
        message(LOG_ERROR,
                "Response NULL, but type specifies a return payload\n");
    else
    {
        unsigned char msg[MAX_PACKET_SIZE] = {IG_CTL_START, IG_CTL_START,
                                              CTL_TODEV};
        uint64_t then, now;
        int length = MIN_CODE_LENGTH, result, sent = 0;

#ifdef LIBUSB_NO_THREADS
        bool unlocked = false;
#endif

        /* finish creating the packet */
        msg[CODE_OFFSET] = request->code;
        if (request->code == IG_DEV_GETID)
            msg[CODE_OFFSET] = IG_DEV_EXECUTE;

        if (request->code != IG_DEV_SEND &&
            request->code != IG_DEV_BULKPINS)
        {
            sent = request->dataLen;
            /* this is only used to get addresses in WRITEBLOCK */
            if (sent > 4)
                sent = 4;
            memcpy(msg + MIN_CODE_LENGTH, request->data, sent);
            length += sent;
        }
        /* as of version 3 SEND and BULKPINS require a length argument */
        else if (idev->version >= 3)
            msg[length++] = request->dataLen;

#ifdef LIBUSB_NO_THREADS
        /* force the reader to give up the lock */
        idev->needToWrite = true;
        EnterCriticalSection(&idev->devLock);
        idev->needToWrite = false;
#endif

        /* time the transfer */
        then = microsSinceX();
        result = interruptSend(idev->usbDev, msg, length);
        /* error if we were not able to write ALL the data */
        if (result != length)
            printError(LOG_ERROR,
                       "failed to write control packet", idev->usbDev);
        /* if there is more data need to transmit the data stream
         * before releasing the devLock */
        else if (request->dataLen > sent &&
                 ! sendData(idev,
                            request->data + sent, request->dataLen - sent,
                            idev->version < 3 && request->code == IG_DEV_SEND))
            message(LOG_ERROR, "Failed to send IR data.\n");
        /* if no ack is necessary then return success now */
        else if (! type->ack)
            retval = true;
        else
        {
            int amount;

#ifdef LIBUSB_NO_THREADS
            /* unlock as soon as possible after all data has been sent */
            LeaveCriticalSection(&idev->devLock);
            unlocked = true;
#endif

            /* using sendTimeout to ensure reader has necessary time
             * to recieve the ack */
            amount = notified(idev->responsePipe[READ],
                              idev->usbDev->list->sendTimeout);
            if (amount < 0)
                message(LOG_ERROR, "Failed to read control ack\n");
            else if (amount > 0)
            {
                dataPacket *pos;

                EnterCriticalSection(&idev->listLock);
                pos = idev->response;

                errno = EINVAL;
                if (pos->code != request->code)
                    message(LOG_ERROR,
                            "Bad ack for send: %d != %d\n",
                            pos->code, request->code);
                else if (! payloadMatch(type->inData, pos->dataLen))
                    message(LOG_ERROR, "Response size does not match specification (%d != %d)\n", pos->dataLen, type->inData);
                else
                {
                    if (pos->dataLen > 0)
                    {
                        *response = pos;
                        pos = NULL;
                    }

                    /* how long did this all take? */
                    now = microsSinceX();
                    message(LOG_INFO,
                            "Transaction: 0x%x (%d microseconds)\n",
                            request->code, now - then);
                    retval = true;
                }

                freeDataPacket(pos);
                idev->response = NULL;
                LeaveCriticalSection(&idev->listLock);
            }
            else
                message(LOG_ERROR, "No response from device.\n");
        }

#ifdef LIBUSB_NO_THREADS
        /* certain errors may result in unlocking here */
        if (!unlocked)
            LeaveCriticalSection(&idev->devLock);
#endif
    }

    return retval;
}

dataPacket* removeNextPacket(iguanaDev *idev)
{
    dataPacket *retval;

    EnterCriticalSection(&idev->listLock);

    /* snag the first one off the list (NULL on empty) */
    retval = (dataPacket*)removeFirstItem(&idev->recvList);

    /* debug logging */
    if (retval == NULL)
        message(LOG_DEBUG, "Returning NULL data packet\n");
    else
        message(LOG_DEBUG, "Returning data packet (0x%x, %d byte payload)\n",
                retval->code, retval->dataLen);

    LeaveCriticalSection(&idev->listLock);

    return retval;
}

void handleIncomingPackets(iguanaDev *idev)
{
    unsigned char *buffer = NULL;
    int length = 0;
    dataPacket *current = NULL;

    /* allocate space for receiving */
    buffer = (unsigned char*)malloc(idev->maxPacketSize);
    if (buffer == NULL)
        message(LOG_ERROR, "Out of memory allocating receive buffer.\n");
    else
        /* read and handle packets forever */
        while(length >= 0 &&
              ! idev->quitRequested)
        {
#ifdef LIBUSB_NO_THREADS
            /* writer will set a flag if it need to perform a device
             * transaction because otherwise we can spin and keep
             * getting the lock before the writer is scheduled */
            if (idev->needToWrite)
                SwitchToThread();
            EnterCriticalSection(&idev->devLock);
#endif

            /* wait for data to arrive */
            length = interruptRecv(idev->usbDev, buffer, idev->maxPacketSize);

            if (length < 0)
            {
                /* loop on timeouts */
                if (errno == EAGAIN || errno == ETIMEDOUT)
                    length = 0;
                /* (somewhat) quietly clean up on disconnect */
                else if (errno == ENODEV)
                {
                    message(LOG_INFO, "Device %d unplugged\n", idev->usbDev->id);
                    break;
                }
                else if (errno != EINVAL)
                    /* log the usb error associated with the problem */
                    printError(LOG_ERROR,
                               "can't read from USB device", idev->usbDev);
            }
            else if (length == 0)
                message(LOG_DEBUG, "0 length recv on %d.\n", idev->usbDev->id);
            else /* if (length > 0)*/
            {
                /* now we need to store a dataPacket */
                current = (dataPacket*)malloc(sizeof(dataPacket));
                if (current == NULL)
                    message(LOG_FATAL, "Out of memory for data packet.\n");
                else
                {
                    unsigned char *dataStart;
                    packetType *type = NULL;

                    /* initialize the data packet */
                    memset(current, 0, sizeof(dataPacket));

                    /* see if we got a control packet */
                    if (length >= MIN_CODE_LENGTH &&
                        buffer[0] == IG_CTL_START &&
                        buffer[1] == IG_CTL_START &&
                        buffer[2] == IG_CTL_FROMDEV)
                    {
                        /* any remaining part of the packet is data */
                        current->code = buffer[CODE_OFFSET];
                        current->dataLen = length - MIN_CODE_LENGTH;
                        dataStart = buffer + MIN_CODE_LENGTH;

                        message(LOG_DEBUG,
                                "Received ctl header: 0x%x\n", current->code);
                    }
                    else
                    {
                        /* all other data is a receive */
                        current->code = IG_DEV_RECV;
                        /* NOTE: last byte is the fill level */
                        current->dataLen = length - 1;
                        dataStart = buffer;
 
                        message(LOG_DEBUG,
                                "Data without ctl header assuming IG_DEV_RECV.\n");
                    }

                    /* if the type demands more data then read it here */
                    type = findTypeEntry(current->code, idev->version);
                    if (type->inData != NO_PAYLOAD &&
                        type->inData > current->dataLen)
                    {
                        current->data = (unsigned char*)malloc(type->inData);
                        memcpy(current->data, dataStart, current->dataLen);

                        while(type->inData > current->dataLen)
                        {
                            length = interruptRecv(idev->usbDev,
                                                   buffer,
                                                   idev->maxPacketSize);
/* TODO: timeouts should never happen, but can cause a CRASH */
                            memcpy(current->data + current->dataLen,
                                   buffer, length);
                            current->dataLen += length;
                        }
                    }
                    else
                    {
                        /* store the data from the packet */
                        current->data = (unsigned char*)malloc(current->dataLen);
                        memcpy(current->data, dataStart, current->dataLen);
                    }

                    /* hate to special case it, but must \0 terminate it */
                    if (current->code == IG_DEV_GETID &&
                        current->data[current->dataLen - 1] != '\0')
                    {
                        char *temp;
                        temp = (char*)malloc(current->dataLen + 1);
                        strncpy(temp, (char*)current->data, current->dataLen);
                        temp[current->dataLen] = '\0';
                        free(current->data);
                        current->data = (unsigned char*)temp;
                    }

                    queueDataPacket(idev, current);
                    current = NULL;
                }
            }

#ifdef LIBUSB_NO_THREADS
            /* unlock the device between reads */
            LeaveCriticalSection(&idev->devLock);
#endif
        }

    /* signal worker thread that the reader is exiting */
    closePipe(idev->readerPipe[WRITE]);
}

/* set dev_ep_in and dev_ep_out to the in/out endpoints of the given
 * device. returns 1 on success, 0 on failure. */
bool findDeviceEndpoints(iguanaDev *idev)
{
    struct usb_device *dev;
    struct usb_interface_descriptor *idesc;

    dev = usb_device(getDevHandle(idev->usbDev));

    if (dev->descriptor.bNumConfigurations != 1 ||
        dev->config[0].bNumInterfaces != 1 ||
        dev->config[0].interface[0].num_altsetting != 1)
        return false;

    idesc = &dev->config[0].interface[0].altsetting[0];
    if (idesc->bNumEndpoints != 2)
        return false;

    /* grab the pointers */
    idev->usbDev->epIn = &idesc->endpoint[0];
    idev->usbDev->epOut = &idesc->endpoint[1];

    /* set the max packet size to the minimum of in and out */
    idev->maxPacketSize = idesc->endpoint[0].wMaxPacketSize;
    if (idev->maxPacketSize > idesc->endpoint[1].wMaxPacketSize)
        idev->maxPacketSize = idesc->endpoint[1].wMaxPacketSize;

    /* check the pointer targets */
    if ((idev->usbDev->epIn->bEndpointAddress &
         USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN &&
        (idev->usbDev->epIn->bmAttributes &
         USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_INTERRUPT &&
        (idev->usbDev->epOut->bEndpointAddress &
         USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_OUT &&
        (idev->usbDev->epOut->bmAttributes &
         USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_INTERRUPT)
        return true;

    return false;
}

uint32_t* iguanaDevToPulses(unsigned char *code, int *length)
{
    int x, codeLength = 0, inSpace = 0;
    uint32_t *retval;

    /* allocate space for the deciphered code */
    retval = (uint32_t*)malloc(sizeof(uint32_t) * *length);
    retval[0] = 0;
    for(x = 0; x < *length + 1; x++)
    {
        if (x > 0 &&
            (((code[x] & STATE_MASK) != inSpace) ||
             ((code[x] & LENGTH_MASK) + retval[codeLength] > 
              IG_PULSE_MASK) ||
             x == *length))
        {
            retval[codeLength] = (retval[codeLength] << 6) / 3;

            if (! inSpace)
                retval[codeLength] |= IG_PULSE_BIT;
            codeLength++;

            if (x == *length)
                break;
            retval[codeLength] = 0;
        }

        if ((code[x] & LENGTH_MASK) == 0)
            retval[codeLength] += MAX_PULSE_LENGTH + 1;
        else
            retval[codeLength] += (code[x] & LENGTH_MASK) + 1;
        inSpace = code[x] & STATE_MASK;
    }

    if (codeLength > 1)
        message(LOG_DEBUG,
                "iguanaDevToPulses: allocated %d, used %d\n",
                *length, codeLength);

    *length = codeLength * sizeof(uint32_t);
    return retval;
}

unsigned char* pulsesToIguanaSend(uint32_t *sendCode, int *length)
{
    int x, codeLength = 0, inSpace = 0;
    unsigned char *codes = NULL;

    /* convert each pulse */
    for(x = 0; x < *length; x++)
    {
        uint32_t cycles, numBytes;
        cycles = (uint32_t)((sendCode[x] & IG_PULSE_MASK) / 
                            1000000.0 * CARRIER_RATE + 0.5);
        numBytes = (cycles / MAX_DATA_BYTE) + 1;
        cycles %= MAX_DATA_BYTE;

        /* allocate space as we go */
        codes = realloc(codes, sizeof(char) * (codeLength + numBytes));

        /* populate the buffer with max bytes */
        memset(codes + codeLength,
               LENGTH_MASK | (inSpace * STATE_MASK),
               numBytes - 1);
        if (inSpace)
            cycles |= STATE_MASK;

        /* store the last byte */
        codes[codeLength + numBytes - 1] = cycles;
        codeLength += numBytes;

        inSpace ^= 1;
    }

    *length = codeLength;
    return codes;
}
