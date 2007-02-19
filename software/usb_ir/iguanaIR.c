/****************************************************************************
 ** iguanaIR.c **************************************************************
 ****************************************************************************
 *
 * Client interface to the igdaemon for controlling USB devices.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#include "base.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "pipes.h"
#include "iguanaIR.h"
#include "support.h"
#include "protocol.h"
#include "dataPackets.h"

int iguanaConnect(const char *name)
{
    int retval = -1;
    struct sockaddr_un server;

    /* generate the server address */
    server.sun_family = PF_UNIX;
    socketName(name, server.sun_path, sizeof(server.sun_path));

    /* connect to the server */
    retval = socket(PF_UNIX, SOCK_STREAM, 0);
    if (retval != -1)
    {
        if (connect(retval,
                    (struct sockaddr *)&server,
                    sizeof(struct sockaddr_un)) == -1)
        {
            close(retval);
            retval = -1;
        }
    }

    return retval;
}

void iguanaClose(int connection)
{
    close(connection);
}

void* iguanaCreateRequest(unsigned char code,
                          unsigned int dataLength, void *data)
{
    dataPacket *packet;

    packet = (dataPacket*)malloc(sizeof(dataPacket));
    if (packet != NULL)
    {
        memset(packet, 0, sizeof(dataPacket));
        packet->code = code;
        packet->data = data;
        packet->dataLen = dataLength;
    }

    return packet;
}

unsigned char* iguanaRemoveData(iguanaPacket pkt, unsigned int *dataLength)
{
    unsigned char *retval;
    dataPacket *packet = (dataPacket*)pkt;

    /* store the values ... */
    if (dataLength != NULL)
        *dataLength = packet->dataLen;
    if (packet->dataLen == 0)
        retval = NULL;
    else
        retval = packet->data;

    /* ... and clear them from the packet */
    packet->dataLen = 0;
    packet->data = NULL;

    return retval;
}

unsigned char iguanaCode(const iguanaPacket pkt)
{
    return ((dataPacket*)pkt)->code;
}

void iguanaFreePacket(iguanaPacket pkt)
{
    freeDataPacket((dataPacket*)pkt);
}

bool iguanaWriteRequest(const iguanaPacket request, int connection)
{
    if (writeDataPacket((dataPacket*)request, connection))
        return 1;
    return 0;
}

iguanaPacket iguanaReadResponse(int connection, unsigned int timeout)
{
    dataPacket *response;

    response = (dataPacket*)malloc(sizeof(dataPacket));
    if (response != NULL)
    {
        if (! readDataPacket(response, connection, timeout))
        {
            free(response);
            response = NULL;
        }
    }

    return response;
}

bool iguanaResponseIsError(const iguanaPacket response)
{
    int retval = 1;
    dataPacket *packet = (dataPacket*)response;

    if (packet != NULL)
    {
        if (packet->code != IG_DEV_ERROR)
            retval = 0;
        else
            errno = -packet->dataLen;
    }

    return retval;
}

int iguanaReadPulseFile(const char *filename, void **pulses)
{
    bool success = false;
    int count = 0;
    char line[MAX_LINE], inSpace = 1;
    FILE *input;

    /* start with no pulses, then realloc as needed */
    *pulses = NULL;

    errno = EINVAL;
    input = fopen(filename, "r");
    if (input != NULL)
        while(fgets(line, MAX_LINE, input))
        {
            int value;
            bool discard = false;
            success = false;

            /* allocate space for one more */
            *pulses = realloc(*pulses, sizeof(uint32_t) * (count + 1));
            if (*pulses == NULL)
                break;

            /* try to read the pulse or space (in a couple formats) */
            if (sscanf(line, "pulse %d", &value) == 1)
            {
                if (! inSpace)
                {
                    message(LOG_ERROR,
                            "Illegal pulse reading pulse file %s(%d): %s\n",
                            filename, count + 1, line);
                    break;
                }
            }
            else if (sscanf(line, "space %d", &value) == 1)
            {
                /* never start with a space */
                if (count == 0)
                {
                    message(LOG_INFO, "Discarding leading space.\n");
                    discard = true;
                }
                else if (inSpace)
                {
                    message(LOG_ERROR,
                            "Illegal space reading pulse file %s(%d): %s\n",
                            filename, count + 1, line);
                    break;
                }
            }
            else if (sscanf(line, "%d", &value) != 1)
            {
                message(LOG_ERROR,
                        "Illegal line reading pulse file %s(%d): %s\n",
                        filename, count + 1, line);
                break;
            }

            if (! discard)
            {
                if (inSpace)
                    value |= IG_PULSE_BIT;
                ((uint32_t*)(*pulses))[count++] = value;
            }
            inSpace ^= 1;
            success = true;
        }

    if (! success)
    {
        free(*pulses);
        return -1;
    }

    return count;
}

int iguanaReadBlockFile(const char *filename, void **data)
{
    int retval = 0;
    char *block;
    FILE *input;

    /* we know a block is 64 bytes, plus 4 of a header */
    *data = malloc(69);
    memset(*data, 0, 69);
    block = (char*)*data;

    errno = EINVAL;
    input = fopen(filename, "r");
    if (input != NULL)
        switch(fread(block, 1, 69, input))
        {
        /* by default write the last block */
        case 68:
            if (block[0] == 0)
                block[0] = 0x7F;
            retval = 1;
            break;

        /* too much was read */
        case 69:
            message(LOG_ERROR, "Too much data in block file.\n");
            break;

        default:
            message(LOG_ERROR, "Too little data in block file.\n");
            break;
        }

    return retval;
}

int iguanaPinSpecToData(unsigned int value, void **data)
{
    int retval = -1;

    *data = malloc(2);
    if (data != NULL)
    {
        ((char*)*data)[0] = value & 0x0F;
        ((char*)*data)[1] = (value & 0xF0) >> 4;
        retval = 2;
    }

    return retval;
}

unsigned char iguanaDataToPinSpec(const void *data)
{
    return (((char*)data)[0] & 0x0F) |
          ((((char*)data)[1] & 0x0F) << 4);
}
