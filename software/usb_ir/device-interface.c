/****************************************************************************
 ** protocol.c **************************************************************
 ****************************************************************************
 *
 * The specification of the Iguanaworks device protocol.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "iguanaIR.h"
#include "compat.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pipes.h"
#include "support.h"
#include "driver.h"
#include "dataPackets.h"
#include "device-interface.h"
#include "protocol-versions.h"

/* internal protocol constants */
enum
{
    /* used in calls to check message parameters */
    MAX_PACKET_SIZE = 8,
    MIN_CTL_LENGTH  = 4,
    CODE_OFFSET     = 3,

    /* control packet constants */
    CTL_START      = 0x0000,
    CTL_TODEV      = 0xCD,
    CTL_FROMDEV    = 0xDC,

    /* constants for packetType table */
    NO_PAYLOAD     = 0xFF,
    ANY_PAYLOAD    = 0x00,
    IG_DEV_ANYCODE = 0x00
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
    /* exchanging the versions of the client and server */
    {0, 0, {IG_EXCH_VERSIONS, CTL_TODEV, 2, true, 2}},

    /* use the upper version bound to list the MAX_VERSION */
    {0, 0, {IG_DEV_GETVERSION, CTL_TODEV,   NO_PAYLOAD,  true, 2}},

    /* device functionality */
    {0x0101, 0, {IG_DEV_GETFEATURES, CTL_TODEV, NO_PAYLOAD, true,ANY_PAYLOAD}},
    {0, 0, {IG_DEV_SEND,           CTL_TODEV, ANY_PAYLOAD, true, NO_PAYLOAD}},
    {0x0307, 0, {IG_DEV_RESEND,    CTL_TODEV,           4, true, NO_PAYLOAD}},
    {0, 0, {IG_DEV_SENDSIZE,       CTL_TODEV, ANY_PAYLOAD, true, 2}},
    {0, 0, {IG_DEV_RECVON,         CTL_TODEV,  NO_PAYLOAD, true, NO_PAYLOAD}},
    {0x0101, 0, {IG_DEV_RAWRECVON, CTL_TODEV,  NO_PAYLOAD, true, NO_PAYLOAD}},
    {0, 0, {IG_DEV_RECVOFF,        CTL_TODEV,  NO_PAYLOAD, true, NO_PAYLOAD}},

    /* 1 bit per pin of state */
    {0,     3, {IG_DEV_GETPINS,    CTL_TODEV,   NO_PAYLOAD, true, 2}},
    {0x101, 0, {IG_DEV_GETPINS,    CTL_TODEV,   NO_PAYLOAD, true, 2}},
    {0,     3, {IG_DEV_SETPINS,    CTL_TODEV,   2,          true, NO_PAYLOAD}},
    {0x101, 0, {IG_DEV_SETPINS,    CTL_TODEV,   2,          true, NO_PAYLOAD}},

    /* 1 byte per pin, in the register format */
    {0, 0x003, {IG_DEV_GETPINCONFIG, CTL_TODEV, NO_PAYLOAD, true, 8}},
    {0, 0x003, {IG_DEV_SETPINCONFIG, CTL_TODEV, 8,          true, NO_PAYLOAD}},
    {0x101, 0, {IG_DEV_GETPINCONFIG, CTL_TODEV, NO_PAYLOAD, true, 8}},
    {0x101, 0, {IG_DEV_SETPINCONFIG, CTL_TODEV, 8,          true, NO_PAYLOAD}},
    {0, 0x003, {IG_DEV_GETCONFIG0,   CTL_TODEV, NO_PAYLOAD, true, 4}},
    {0, 0x003, {IG_DEV_SETCONFIG0,   CTL_TODEV, 4,          true, NO_PAYLOAD}},
    {0, 0x003, {IG_DEV_GETCONFIG1,   CTL_TODEV, NO_PAYLOAD, true, 4}},
    {0, 0x003, {IG_DEV_SETCONFIG1,   CTL_TODEV, 4,          true, NO_PAYLOAD}},

    /* supporting functions */
    {0, 0, {IG_DEV_GETBUFSIZE,  CTL_TODEV,   NO_PAYLOAD,  true,  1}},
    {0, 0x1FF, {IG_DEV_WRITEBLOCK,  CTL_TODEV,   68,      true,  NO_PAYLOAD}},
    {0x200, 0, {IG_DEV_WRITEBLOCK,  CTL_TODEV,   68,      true,  2}},
    {0x200, 0, {IG_DEV_CHECKSUM,    CTL_TODEV,   1,       true,  2}},
    {0, 0, {IG_DEV_EXECUTE,     CTL_TODEV,   NO_PAYLOAD,  false, NO_PAYLOAD}},
    {2, 2, {IG_DEV_PINBURST,    CTL_TODEV,   64,          true,  NO_PAYLOAD}},
    {3, 0, {IG_DEV_PINBURST,    CTL_TODEV,   ANY_PAYLOAD, true,  NO_PAYLOAD}},
    {0, 0, {IG_DEV_GETID,       CTL_TODEV,   NO_PAYLOAD,  true,  12}},
    {0, 0x1FF, {IG_DEV_SETID,   CTL_TODEV,   ANY_PAYLOAD, true,  NO_PAYLOAD}},
    {0x200, 0, {IG_DEV_SETID,   CTL_TODEV,   ANY_PAYLOAD, true,  2}},
    {0, 0, {IG_DEV_IDSOFF,      CTL_TODEV,   NO_PAYLOAD,  true,  NO_PAYLOAD}},
    {0, 0, {IG_DEV_IDSON,       CTL_TODEV,   NO_PAYLOAD,  true,  NO_PAYLOAD}},
    {0, 0, {IG_DEV_IDSTATE,     CTL_TODEV,   NO_PAYLOAD,  true,  1}},
    {0x306, 0, {IG_DEV_REPEATER, CTL_TODEV,  NO_PAYLOAD,  true,  NO_PAYLOAD}},
    {0, 0, {IG_DEV_RESET,       CTL_TODEV,   NO_PAYLOAD,  false, NO_PAYLOAD}},
    {4, 0, {IG_DEV_GETCHANNELS, CTL_TODEV,   NO_PAYLOAD,  true,  1}},
    {4, 0, {IG_DEV_SETCHANNELS, CTL_TODEV,   1,           true,  NO_PAYLOAD}},
    {0x101, 0, {IG_DEV_GETCARRIER, CTL_TODEV, NO_PAYLOAD, true,  4}},
    {0x101, 0, {IG_DEV_SETCARRIER, CTL_TODEV, 4,          true,  4}},
    {0, 0,    {IG_DEV_GETLOCATION, CTL_TODEV, NO_PAYLOAD, true,  2}},

    /* "from device" codes */
    {0, 0, {IG_DEV_RECV,     CTL_FROMDEV, NO_PAYLOAD,  false, ANY_PAYLOAD}},
    {0, 0, {IG_DEV_OVERSEND, CTL_FROMDEV, NO_PAYLOAD,  false, NO_PAYLOAD}},
    {0, 0, {IG_DEV_OVERRECV, CTL_FROMDEV, NO_PAYLOAD,  false, ANY_PAYLOAD}},

    /* invalid argument reply */
    {0x101, 0, {IG_DEV_INVALID_ARG, CTL_TODEV, NO_PAYLOAD, false, NO_PAYLOAD}},

    /* terminate the list */
    {0, 0, {IG_DEV_ANYCODE, 0, 0, false, 0}}
};

static void queueDataPacket(iguanaDev *idev, dataPacket *current, bool fromDev)
{
    message(LOG_DEBUG3,
            "Notifying of packet: type = 0x%2.2x\n", current->code);

    EnterCriticalSection(&idev->listLock);
    if (fromDev)
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

static void flushToDevResponsePackets(iguanaDev *idev)
{
    char byte;
    /* A 0 timeout performs a poll and never waits */
    while (readPipeTimed(idev->responsePipe[READ], &byte, 1, 0) == 1)
    {
        freeDataPacket(idev->response);
        idev->response = NULL;
        message(LOG_ERROR, "Flushed extraneous CTL_TODEV response.\n");
    }
}

static bool sendData(iguanaDev *idev,
                     const void *buffer, int size, bool addTerminator)
{
    bool retval = true;
    int x, count, lastSize;
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
        lastPacket[lastSize++] = 0x00;
    }
    else if (lastSize > 0)
        lastPacket = (char*)buffer + count * idev->maxPacketSize;

    for(x = 0; x < count; x++)
        /* ensure we send idev->maxPacketSize bytes */
        if (interruptSend(idev->usbDev,
                          (char*)buffer + x * idev->maxPacketSize,
                          idev->maxPacketSize,
                          idev->settings->sendTimeout) != idev->maxPacketSize)
        {
            printError(LOG_ERROR, "failed to write data packet", idev->usbDev);
            retval = false;
            break;
        }

    /* send the last packet (with at least the data terminator) */
    if (retval && lastPacket != NULL &&
        interruptSend(idev->usbDev, lastPacket, lastSize,
                      idev->settings->sendTimeout) != lastSize)
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

    for(x = 0; types[x].type.code != IG_DEV_ANYCODE; x++)
    {
/*
        if (types[x].type.code == code)
            fprintf(stderr, "---- 0x%x 0x%x, 0x%x 0x%x\n", code, version, types[x].start, types[x].end);
*/

        if (types[x].type.code == code &&
            types[x].start <= version &&
            (types[x].end >= version || types[x].end == 0))
        {
/*
            fprintf(stderr, "---- 0x%x 0x%x, 0x%x 0x%x\n", code, version, types[x].start, types[x].end);
*/
            return &(types[x].type);
        }
    }

    return NULL;
}

static bool payloadMatch(unsigned char spec, unsigned char length)
{
    return ((spec == NO_PAYLOAD && length == 0) ||
            spec == ANY_PAYLOAD ||
            spec == length);
}

/* There are some magic numbers in this function, and here are the
   explanations:

   Clock is running at 24 Mhz
   24000000 cycles/second

   Want a 38 kHz carrier:
   38000 peaks/second = 76000 transitions/second

   24000000 / 76000 = 315.8 cycles / transition

   Each loop has overhead (counted from code lines):
   5 + 5 + 7 + 6 + 6 + 7 + (5 + 7) + (5 + 7) + 5 = 65

   Break down the remaining delay into components or 7 and 4:
   316 - 65 = 251 = 7 * 1 + 4 * 61

   Compute the number of bytes to jump for each delay:
   delay 7 ==> 2 bytes
   delay 4 ==> 1 byte
   total of 4 delays of 7 in code
   total of 120 delays of 4 in code

   Final values needed for the transmission:
   delay 7 * (4 - 1) = 6 bytes
   delay 4 * (120 - 61) = 59 bytes
   FINAL: delay (6, 59)
*/
static void computeCarrierDelays(uint32_t carrier, unsigned char *delays,
                                 uint8_t loopCycles)
{
    unsigned char sevens = 0, fours;
    unsigned int cycles;

    /* Compute the cycles for any specified frequency.  This requires
       dividing the length of time of a pulse in the requested
       frequency by the length of time in a cycle at the current clock
       speed.
    */
    cycles = (int)(((1.0 / carrier) / (1.0 / 24000000) / 2) + 0.5);

    /* Divide the computed values into 4 and 7 clock components.  Try
       the highest number of 4s, and then count down until we hit
       something that is divisible by 7.  We use 4s as the main
       counter specifically because the delay 4 actually requires less
       space on the flash for a given delay.
    */
    cycles -= loopCycles;

    /* TODO: this next line is too magical */
    sevens = (4 - (cycles % 4)) % 4;
    fours = (unsigned char)((cycles - sevens * 7) / 4);

    /* NOTE: We will never need more than 4 7s due to the properties
       of mathmatical groups. */

    /* store byte offsets for transmission */
    delays[0] = (4 - sevens) * 2;
    delays[1] = (110 - fours) * 1;
}

/* total of 12 bytes will be read from the device when the constructed
   code is called. */
static void* generateIDBlock(const char *label, uint16_t version)
{
    unsigned int len, x, y;
    unsigned char *data;

    /* must allocate the data since we free it later */
    data = malloc(68);
    /* fill the page with halt commands */
    memset(data, 0x30, 68);
    /* which page get's written? and clear the first packet */
    data[0] = 0x7F;
    data[1] = data[2] = data[3] = 0;
    len = 4;

    if (label != NULL && strlen(label))
    {
        unsigned char buf[MAX_PACKET_SIZE * 2] = { CTL_START,   CTL_START,
                                                   CTL_FROMDEV, IG_DEV_GETID },
            packet_start, send_address;
        bool size_in_A = false;

        /* translate the code for the device */
        if (! translateDevice(buf + CODE_OFFSET, version, false))
            message(LOG_ERROR, "Failed to translate GETID code for device.\n");
        if (version >= 0x101)
        {
            /* use the same buffer we use for all control packets */
            packet_start = 0xF8;
            send_address = 0x94;
        }
        else
        {
            /* due to size of buffer these 8 bytes are always in it */
            packet_start = 0x7C;
            send_address = 0x68;
            size_in_A = true;
        }

        /* can only generate 12 bytes worth of transmission data */
        if (strlen(label) > 12)
            message(LOG_ERROR, "Label is too long, truncating to 12 bytes.\n");

        /* construct the bytes that will travel over the wire */
        strncpy((char*)buf + 4, label, 12);

        /* assemble the code to transmit the bytes in 2 packets */
        for(x = 0; x < 2; x++)
        {
            /* record 8 bytes of the label*/
            for(y = 0; y < 8; y++)
            {
                data[len++] = 0x55;
                data[len++] = (unsigned char)(packet_start + y);
                data[len++] = buf[x * 8 + y];
            }
            /* load the packet size into A (or X) */
            if (size_in_A)
                data[len++] = 0x50;
            else
                data[len++] = 0x57;
            data[len++] = 0x08;
            /* load packet location into X (or A) */
            if (size_in_A)
                data[len++] = 0x57;
            else
                data[len++] = 0x50;
            data[len++] = packet_start;
            /* lcall write_data */
            data[len++] = 0x7C;
            data[len++] = 0x00;
            data[len++] = send_address;
        }
    }
    /* put in a trailing ret */
    data[len++] = 0x7F;

    return data;
}

bool checkVersion(iguanaDev *idev)
{
    dataPacket request = DATA_PACKET_INIT, *response = NULL;
    bool retval = false, getVersion = false;

    /* Seems necessary, but means that we lose the interface.....
#ifdef WIN32
    request.code = IG_DEV_RESET;
    if (! deviceTransaction(idev, &request, &response))
        message(LOG_ERROR, "Failed to to do a soft reset.\n");
#endif
        */

    request.code = IG_DEV_GETVERSION;
    /* Due to a firmware mistake related to repeater mode loader
       version 0x300 ignores the first get packet seen after a cold
       boot.  That first packet kicks the device out of repeater mode
       and the second will be answered. */
    if (deviceTransaction(idev, &request, &response))
        getVersion = true;
    /* try a second time to get the version */
    if (! getVersion &&
        ! deviceTransaction(idev, &request, &response))
        message(LOG_ERROR, "Failed to get version.\n");
    else
    {
        if (response->dataLen != 2)
            message(LOG_ERROR, "Incorrectly sized version response.\n");
        else
        {
            idev->version = (response->data[1] << 8) + response->data[0];

            message(LOG_INFO, "Found device version 0x%x\n", idev->version);

            /* ensure we have an acceptable version by checking a hard
               coded mechanism to see if the version is supported.

               check for old firmware first, then the reflasher, then
               the initial bootloader.
            */
            if ((idev->version >= 1 && idev->version <= 4) ||
                idev->version == 0xFF00 ||
                (idev->version >= 0x0100 && idev->version < 0x0400))
                retval = true;
            else
                message(LOG_ERROR,
                        "Unsupported hardware version 0x%x\n", idev->version);
        }

        freeDataPacket(response);
    }

    return retval;
}

bool checkFeatures(iguanaDev *idev, unsigned char targetSet)
{
    /* only ask for features from devices w a body */
    if ((! (idev->version & 0x00FF)) ||
        (! (idev->version & 0xFF00)))
        return false;

    /* try and get the features if we haven't already */
    if (idev->features == UNKNOWN_FEATURES)
    {
        dataPacket request = DATA_PACKET_INIT, *response = NULL;

        request.code = IG_DEV_GETFEATURES;
        if (! deviceTransaction(idev, &request, &response))
            message(LOG_INFO, "Failed to get device features.\n");
        else
        {
            /* save the returned data and free the buffer */
            idev->features = response->data[0];
            if (response->dataLen > 1)
                idev->cycles = response->data[1];
            freeDataPacket(response);
        }
    }

    if (idev->features != UNKNOWN_FEATURES)
    {
        if (targetSet == UNKNOWN_FEATURES)
            return true;
        return idev->features == targetSet;
    }

    return false;
}

packetType* checkIncomingProtocol(iguanaDev *idev, dataPacket *request,
                                  bool nullResponse)
{
    packetType *type;

    type = findTypeEntry(request->code, idev->version);
    errno = EINVAL;
    if (type == NULL)
        message(LOG_ERROR,
                "Unknown packet type in request: 0x%x\n", request->code);
    else if (type->direction != CTL_TODEV)
        message(LOG_ERROR, "Cannot request to send a FROMDEV packet (0x%x %x %x).\n",
                request->code, CTL_TODEV, type->direction);
    else if (! payloadMatch((unsigned char)type->outData, (unsigned char)request->dataLen))
        message(LOG_ERROR,
                "Request size does not match type specification (%d != %d)\n",
                request->dataLen, type->outData);
    else if (type->inData != NO_PAYLOAD && nullResponse)
        message(LOG_ERROR,
                "Response NULL, but type specifies a return payload\n");
    else
        return type;

    return NULL;
}

bool oldPinConfig(iguanaDev *idev,       /* required */
                  dataPacket *request,   /* required */
                  dataPacket **response) /* optional */
{
    unsigned char *data, x, code;

    code = request->code;
    if (code == IG_DEV_SETPINCONFIG)
        data = request->data;
    else
        data = (unsigned char*)malloc(8);
    for(x = 0; x < 2; x++)
    {
        if (code == IG_DEV_SETPINCONFIG)
        {
            request->code = IG_DEV_SETCONFIG0 + x * 2;
            request->dataLen = 4;
            request->data = data + x * 4;
        }
        else
            request->code = IG_DEV_GETCONFIG0 + x * 2;

        if (! deviceTransaction(idev, request, response) ||
            iguanaResponseIsError(*response))
            break;

        if (code == IG_DEV_GETPINCONFIG)
        {
            memcpy(data + x * 4, (*response)->data, 4);
            if (x == 0)
                iguanaFreePacket(*response);
            else
            {
                free((*response)->data);
                (*response)->code = code;
                (*response)->dataLen = 8;
                (*response)->data = data;
            }
        }
    }
    
    if (code == IG_DEV_SETPINCONFIG)
        request->data = data;
    if (x == 2)
        return true;
    return false;
}

bool deviceTransaction(iguanaDev *idev,       /* required */
                       dataPacket *request,   /* required */
                       dataPacket **response) /* optional */
{
    bool retval = false;
    packetType *type;

    /* For old devices setting pin configs actually becomes 2
       requests, awkward, but the way it has to be to support old
       firmware. */
    if (idev->version <= 3 && 
        (request->code == IG_DEV_SETPINCONFIG ||
         request->code == IG_DEV_GETPINCONFIG))
        return oldPinConfig(idev, request, response);

    type = checkIncomingProtocol(idev, request, response == NULL);
    if (type)
    {
        unsigned char msg[MAX_PACKET_SIZE] = {CTL_START, CTL_START, CTL_TODEV};
        uint64_t then, now;
        int length = MIN_CTL_LENGTH, result, sent = 0;

#ifdef LIBUSB_NO_THREADS
        bool unlocked = false;
#endif

        /* possibly change the code, then translate for the device */
        switch(request->code)
        {
        case IG_DEV_GETID:
            msg[CODE_OFFSET] = IG_DEV_EXECUTE;
            break;
            
        case IG_DEV_SETID:
        {
            unsigned char *block;
            block = generateIDBlock((char*)request->data, idev->version);
            free(request->data);
            request->data = block;
            request->dataLen = 68;

            msg[CODE_OFFSET] = IG_DEV_WRITEBLOCK;
            break;
        }

        default:
            msg[CODE_OFFSET] = request->code;
            break;
        }
        if (! translateDevice(msg + CODE_OFFSET, idev->version, false))
            message(LOG_ERROR, "Failed to translate code for device.\n");

        /* SEND and PINBURST do not get their data packed into the
           request packet, unlike everything else. */
        if (request->code != IG_DEV_SEND &&
            request->code != IG_DEV_RESEND &&
            request->code != IG_DEV_PINBURST &&
            request->code != IG_DEV_REPEATER)
        {
            if (request->code != IG_DEV_SETPINCONFIG)
            {
                sent = request->dataLen;
                /* this is only used to get addresses in WRITEBLOCK */
                if (sent > 4)
                    sent = 4;
                memcpy(msg + MIN_CTL_LENGTH, request->data, sent);
                length += sent;
            }
        }
        /* as of version 3 SEND and PINBURST require a length argument */
        else if (idev->version >= 3)
        {
            msg[length++] = (unsigned char)request->dataLen;

            /* select which channels to transmit on */
            if (request->code == IG_DEV_SEND ||
                request->code == IG_DEV_RESEND ||
                request->code == IG_DEV_REPEATER)
            {
                msg[length++] = idev->channels;

                /* is the carrier frequency finally adjustable? */
                if ((idev->version & 0x00FF) &&
                    (idev->version & 0xFF00))
                {
                    /* for a long time the cycle count has remained stable: */
                    uint8_t loopCycles = 5 + 5 + 7 + 6 + 6 + 7 + \
                                         (5 + 7) + (5 + 7) + 5;
                    /* we can use the cycle count provided by the
                       firmware with body-4 */
                    if ((idev->version & 0x00FF) >= 0x0004 &&
                        checkFeatures(idev, UNKNOWN_FEATURES))
                        loopCycles = idev->cycles;

                    /* compute the delay length off the carrier */
                    computeCarrierDelays(idev->carrier, msg + length,
                                         loopCycles);
                    length += 2;
                }
            }
        }


#ifdef LIBUSB_NO_THREADS_OPTION
        if (idev->libusbNoThreads)
#endif
#ifdef LIBUSB_NO_THREADS
        {
            /* force the reader to give up the lock */
            idev->needToWrite = true;
            EnterCriticalSection(&idev->devLock);
            idev->needToWrite = false;
        }
#endif

        /* flush any extraneous CTL_TODEV responses */
        flushToDevResponsePackets(idev);
        /* time the transfer */
        then = microsSinceX();
        result = interruptSend(idev->usbDev, msg, length,
                               idev->settings->sendTimeout);
        /* error if we were not able to write ALL the data */
        if (result != length)
            printError(LOG_ERROR,
                       "failed to write control packet", idev->usbDev);
        /* if there is more data need to transmit the data stream
           before releasing the devLock */
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

#ifdef LIBUSB_NO_THREADS_OPTION
            if (idev->libusbNoThreads)
#endif
#ifdef LIBUSB_NO_THREADS
            {
                /* unlock as soon as possible after all data has been sent */
                LeaveCriticalSection(&idev->devLock);
                unlocked = true;
            }
#endif

            /* using sendTimeout to ensure reader has necessary time
               to recieve the ack */
            amount = notified(idev->responsePipe[READ],
                              idev->settings->sendTimeout);
            if (amount < 0)
                message(LOG_ERROR, "Failed to read control ack: %s\n",
			translateError(errno));
            else if (amount > 0)
            {
                dataPacket *pos;

                EnterCriticalSection(&idev->listLock);
                pos = idev->response;

                /* un-translate the SETID/WRITEBLOCK codes */
                if (request->code == IG_DEV_SETID &&
                    pos->code == IG_DEV_WRITEBLOCK)
                    pos->code = IG_DEV_SETID;

                errno = EINVAL;
                if (pos->code != IG_DEV_INVALID_ARG)
                {
                    if (pos->code != request->code)
                        message(LOG_ERROR,
                                "Bad ack for send: 0x%x != 0x%x\n",
                                pos->code, request->code);
                    else if (! payloadMatch((unsigned char)type->inData, (unsigned char)pos->dataLen))
                        message(LOG_ERROR, "Response size does not match specification (version 0x%x: %d != %d)\n", idev->version, pos->dataLen,type->inData);
                    else
                    {
                        /* store the retrieved response */
                        if (response != NULL)
                            *response = pos;
                        else
                            freeDataPacket(pos);
                        pos = NULL;

                        /* how long did this all take? */
                        now = microsSinceX();
                        message(LOG_INFO,
                                "Transaction: 0x%x (%lld microseconds)\n",
                                request->code, now - then);
                        retval = true;
                    }
                }

                freeDataPacket(pos);
                idev->response = NULL;
                LeaveCriticalSection(&idev->listLock);
            }
            else
            {
                message(LOG_INFO,
                        "Timeout while waiting for response from device.\n");
                errno = ETIMEDOUT;
            }
        }

#ifdef LIBUSB_NO_THREADS_OPTION
        if (idev->libusbNoThreads)
#endif
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
        message(LOG_DEBUG2, "Returning data packet (0x%x, %d byte payload)\n",
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
#ifdef LIBUSB_NO_THREADS_OPTION
            if (idev->libusbNoThreads)
#endif
#ifdef LIBUSB_NO_THREADS
            {
                /* writer will set a flag if it need to perform a device
                 * transaction because otherwise we can spin and keep
                 * getting the lock before the writer is scheduled */
                if (idev->needToWrite)
                    SwitchToThread();
                EnterCriticalSection(&idev->devLock);
            }
#endif

            /* wait for data to arrive */
            length = interruptRecv(idev->usbDev, buffer, idev->maxPacketSize,
                                   idev->settings->recvTimeout);

            if (length < 0)
            {
                /* loop on timeouts */
                if (errno == EAGAIN || errno == USB_ETIMEDOUT)
                    length = 0;
                /* (somewhat) quietly clean up on disconnect */
                else if (errno == ENODEV)
                {
                    message(LOG_INFO,
                            "Device %d unplugged\n", idev->usbDev->id);
                    break;
                }
                /* (somewhat) quietly released during shutdown */
                else if (idev->usbDev->stopped)
                {
                    message(LOG_INFO,
                            "Device %d released\n", idev->usbDev->id);
                    break;
                }
                else /* if (errno != EINVAL) */
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
                    if (length >= MIN_CTL_LENGTH &&
                        buffer[0] == CTL_START &&
                        buffer[1] == CTL_START &&
                        buffer[2] == CTL_FROMDEV)
                    {
                        /* any remaining part of the packet is data */
                        current->code = buffer[CODE_OFFSET];
                        current->dataLen = length - MIN_CTL_LENGTH;
                        dataStart = buffer + MIN_CTL_LENGTH;

                        message(LOG_DEBUG,
                                "Received ctl header: 0x%x\n", current->code);

                        /* translate the incoming packet code */
                        if (! translateDevice(&current->code,
                                              idev->version, true))
                            message(LOG_ERROR,
                                    "Failed to translate code for device.\n");

                        /* log incoming errors to the igdaemon output */
                        if (current->code == IG_DEV_OVERRECV)
                            message(LOG_WARN, "Error received from device %d: Receive too long.\n", idev->usbDev->id);
                        else if (current->code == IG_DEV_OVERSEND)
                            message(LOG_WARN, "Error received from device %d: Transmit too long.\n", idev->usbDev->id);
                    }
                    else
                    {
                        /* all other data is a receive */
                        current->code = IG_DEV_RECV;
                        /* NOTE: last byte is the fill level */
                        current->dataLen = length - 1;
                        dataStart = buffer;
 
                        message(LOG_DEBUG2,
                                "Data without ctl header assuming IG_DEV_RECV.\n");
/* TODO: DEBUG: sleep here to test overflow on the device
                        sleep(3);
*/
                    }

                    /* if the type demands more data then read it here */
                    type = findTypeEntry(current->code, idev->version);
                    if (type == NULL)
                    {
                        message(LOG_ERROR, "Unknown packet type received from device: 0x%x\n", current->code);
                        /* still store the rest of the packet */
                        current->data = (unsigned char*)malloc(current->dataLen);
                        memcpy(current->data, dataStart, current->dataLen);
                    }
                    else if (type->inData != NO_PAYLOAD &&
                             type->inData > current->dataLen)
                    {
                        current->data = (unsigned char*)malloc(type->inData);
                        memcpy(current->data, dataStart, current->dataLen);

                        while(type->inData > current->dataLen)
                        {
                            length = interruptRecv(idev->usbDev,
                                                   buffer,
                                                   idev->maxPacketSize,
                                                  idev->settings->recvTimeout);
                            /* timeouts should never happen, but handle it */
                            if (length > 0 && length <= idev->maxPacketSize)
                            {
                                memcpy(current->data + current->dataLen,
                                       buffer, length);
                                current->dataLen += length;
                            }
                            else
                            {
                                message(LOG_ERROR,
                                        "Invalid length %d\n", length);
                                break;
                            }
                        }
                    }
                    else
                    {
                        /* store the data from the packet */
                        current->data = (unsigned char*)malloc(current->dataLen);
                        memcpy(current->data, dataStart, current->dataLen);
                    }

                    queueDataPacket(idev, current,
                                    ! type || type->direction != CTL_TODEV);
                    current = NULL;
                }
            }

#ifdef LIBUSB_NO_THREADS_OPTION
            if (idev->libusbNoThreads)
#endif
#ifdef LIBUSB_NO_THREADS
                /* unlock the device between reads */
                LeaveCriticalSection(&idev->devLock);
#endif
        }

    /* release the buffer */
    free(buffer);

    /* signal worker thread that the reader is exiting */
#if DEBUG
printf("CLOSE %d %s(%d)\n", idev->readerPipe[WRITE], __FILE__, __LINE__);
#endif
    closePipe(idev->readerPipe[WRITE]);
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
            (x == *length||
             ((code[x] & STATE_MASK) != inSpace) ||
             ((code[x] & LENGTH_MASK) + retval[codeLength] > IG_PULSE_MASK)))
        {
            retval[codeLength] = (retval[codeLength] << 6) / 3;

            if (! inSpace)
                retval[codeLength] |= IG_PULSE_BIT;
            codeLength++;

            if (x == *length)
                break;
            retval[codeLength] = 0;
        }

        /* increase by the maximum pulse length + 1 */
        if ((code[x] & LENGTH_MASK) == 0)
            retval[codeLength] += 1023 + 1;
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
