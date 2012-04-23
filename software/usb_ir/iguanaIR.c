/****************************************************************************
 ** iguanaIR.c **************************************************************
 ****************************************************************************
 *
 * Client API to the igdaemon for controlling USB devices.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */
#include "iguanaIR.h"
#include "compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pipes.h"
#include "support.h"
#include "dataPackets.h"

PIPE_PTR iguanaConnect_real(const char *name, unsigned int protocol)
{
    PIPE_PTR conn = INVALID_PIPE;

    if (protocol != IG_PROTOCOL_VERSION)
        message(LOG_ERROR, "Client application was not built against a protocol-compatible library (%d != %d).  Aborting connect iguanaConnect.\n", protocol, IG_PROTOCOL_VERSION);
    else
    {
        conn = connectToPipe(name);
        if (conn != INVALID_PIPE)
        {
            uint16_t clientVersion = IG_PROTOCOL_VERSION;
            dataPacket *request;

            /* check versions of the client and server */
            request = iguanaCreateRequest(IG_EXCH_VERSIONS, 2, &clientVersion);
            if (request &&
                iguanaWriteRequest(request, conn))
            {
                dataPacket *response;
                response = iguanaReadResponse(conn, 10000);
                if (iguanaResponseIsError(response))
                {
                    message(LOG_ERROR, "Server did not understand version request, aborting.  Is the igdaemon is up to date?\n");
                    iguanaClose(conn);
                    errno = 0;
                    conn = INVALID_PIPE;
                }
                freeDataPacket(response);
            }
            request->data = NULL;
            freeDataPacket(request);
        }
    }

    return conn;
}

/* Interesting way to force old clients to connect and reveal a
   version 0 protocol. */
#undef iguanaConnect
PIPE_PTR iguanaConnect(const char *name)
{
    return iguanaConnect_real(name, 0);
}

void iguanaClose(PIPE_PTR connection)
{
#if DEBUG
printf("CLOSE %d %s(%d)\n", connection, __FILE__, __LINE__);
#endif
    closePipe(connection);
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
    unsigned char *retval = NULL;
    dataPacket *packet = (dataPacket*)pkt;

    if (packet != NULL)
    {
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
    }

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

bool iguanaWriteRequest(const iguanaPacket request, PIPE_PTR connection)
{
    if (writeDataPacket((dataPacket*)request, connection))
        return 1;
    return 0;
}

iguanaPacket iguanaReadResponse(PIPE_PTR connection, unsigned int timeout)
{
    dataPacket *response = NULL;

    if (connection != INVALID_PIPE)
    {
        response = (dataPacket*)malloc(sizeof(dataPacket));
        if (response != NULL &&
            ! readDataPacket(response, connection, timeout))
            {
                free(response);
                response = NULL;
            }
    }
    else
        errno = EPIPE;

    return response;
}

bool iguanaResponseIsError(const iguanaPacket response)
{
    int retval = 1;
    dataPacket *packet = (dataPacket*)response;

    errno = EIO;
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
    char buffer[MAX_LINE], *line, inSpace = 1;
    FILE *input;

    /* start with no pulses, then realloc as needed */
    *pulses = NULL;

    /* open the file and read it line by line */
    errno = EINVAL;
    input = fopen(filename, "r");
    if (input != NULL)
    {
        int lineNumber = 0;
        while(fgets(buffer, MAX_LINE, input))
        {
            char *temp;
            int value;
            bool discard = false;

            line = buffer;
            success = false;
            lineNumber++;

            /* allocate space for one more (not it's not efficient) */
            *pulses = realloc(*pulses, sizeof(uint32_t) * (count + 1));
            if (*pulses == NULL)
                break;

            /* ignore anything after a # in the line */
            temp = strchr(line, '#');
            if (temp != NULL)
                temp[0] = '\0';

            /* skip blank lines (or comments that got truncated) */
            line += strspn(line, " \t\r\n");
            if (line[0] == '\0')
                continue;

            /* try to read the pulse or space (in a couple formats) */
            if (sscanf(line, "pulse %d", &value) == 1 ||
                sscanf(line, "pulse: %d", &value) == 1)
            {
                if (! inSpace)
                {
                    ((uint32_t*)(*pulses))[count - 1] += value;
                    message(LOG_WARN,
                            "Combining pulses in pulse/space file %s(%d)\n",
                            filename, lineNumber);
                    discard = true;
                }
            }
            else if (sscanf(line, "space %d", &value) == 1 ||
                     sscanf(line, "space: %d", &value) == 1)
            {
                /* ignore any leading spaces */
                if (count == 0)
                {
                    message(LOG_INFO, "Discarding leading space.\n");
                    discard = true;
                }
                else if (inSpace)
                {
                    ((uint32_t*)(*pulses))[count - 1] += value;
                    message(LOG_WARN,
                            "Combining spaces in pulse/space file %s(%d)\n",
                            filename, lineNumber);
                    discard = true;
                }
            }
            /* A simple list of numbers is also acceptable.  I believe
               this was to support some sort of LIRC raw codes. */
            else if (sscanf(line, "%d", &value) != 1)
            {
                message(LOG_WARN,
                       "Skipping unparsable line in pulse/space file %s(%d)\n",
                        filename, lineNumber);
                discard = true;
            }

            /* bump to the next code */
            if (! discard)
            {
                if (inSpace)
                    value |= IG_PULSE_BIT;
                ((uint32_t*)(*pulses))[count++] = value;
                inSpace ^= 1;
            }
            success = true;
        }

        fclose(input);
    }

    /* free the buffer on failure */
    if (! success)
    {
        free(*pulses);
        count = -1;
    }
    /* trim a trailing space */
    else if (inSpace)
        count--;

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

int iguanaPinSpecToData(unsigned int value, void **data, bool slotDev)
{
    int retval = -1;

    *data = malloc(2);
    if (data != NULL)
    {
        if (slotDev)
        {
            ((char*)*data)[0] = value & 0xFF;
            ((char*)*data)[1] = 0;
        }
        /* In non-slot devices the GPIO pins are divided between 2
           ports so we divide them into 2 different bytes here. */
        else
        {
            ((char*)*data)[0] = (char)(value & 0x0F);
            ((char*)*data)[1] = (char)((value & 0xF0) >> 4);
        }
        retval = 2;
    }

    return retval;
}

unsigned char iguanaDataToPinSpec(const void *data, bool slotDev)
{
    if (slotDev)
        return ((char*)data)[0];
    else
        /* In non-slot devices the GPIO pins are divided between 2
           ports so we recombine the bytes here. */
        return (((char*)data)[0] & 0x0F) |
              ((((char*)data)[1] & 0x0F) << 4);
}
