/****************************************************************************
 ** iguanaIR.c **************************************************************
 ****************************************************************************
 *
 * Client API to the igdaemon for controlling USB devices.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
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
#include <limits.h>

#include "pipes.h"
#include "logging.h"
#include "dataPackets.h"

#define OLD_IGSOCK_NAME "/dev/iguanaIR/"

enum
{
    MAX_LINE = 1024
};

PIPE_PTR iguanaConnect_internal(const char *name, unsigned int protocol, bool checkVersion);

char* iguanaListDevices()
{
    char *retval = NULL;
    PIPE_PTR conn = iguanaConnect_internal("ctl", IG_PROTOCOL_VERSION, true);
    if (conn != INVALID_PIPE)
    {
        dataPacket *response,
            *request = iguanaCreateRequest(IG_CTL_LISTDEVS, 0, NULL);
        if (iguanaTransaction(conn, (iguanaPacket)request, (iguanaPacket*)&response))
        {
            if (response->data != NULL)
                retval = strdup((char*)response->data);
            freeDataPacket(response);
        }
        freeDataPacket(request);
    }

    return retval;
}

PIPE_PTR iguanaConnect_internal(const char *name, unsigned int protocol, bool checkVersion)
{
    PIPE_PTR conn = INVALID_PIPE;

    if (protocol != IG_PROTOCOL_VERSION)
        message(LOG_ERROR, "Client application was not built against a protocol-compatible library (%d != %d).  Aborting connect.\n", protocol, IG_PROTOCOL_VERSION);
    else
    {
        const char *target = name;
        if (target == NULL)
            target = "0";

        conn = connectToPipe(target);
        if (conn == INVALID_PIPE)
        {
            if (name == NULL)
            {
                /* since we failed to connect to the default device
                   check in with the daemon about getting a different
                   device name */
                char name[8] = {0};
                const char *text = iguanaListDevices();
                if (text != NULL)
                {
                    strncpy(name, text + 2, strchr(text, ',') - (text + 2));
                    conn = iguanaConnect_internal(name, protocol, true);
                }
                else
                    errno = ENOENT;
            }
            else if (strncmp(name, OLD_IGSOCK_NAME, strlen(OLD_IGSOCK_NAME)) == 0)
            {
                char buffer[PATH_MAX] = IGSOCK_NAME;
                strcat(buffer, name + 14);
                message(LOG_WARN, "Client application failed to connect to a socket in /dev.  The proper location is now in /var/run.  Please update your paths accordingly.  Re-trying with corrected path: %s\n", buffer);
                return iguanaConnect_internal(buffer, protocol, true);
            }
        }
        else if (checkVersion)
        {
            uint16_t clientVersion = IG_PROTOCOL_VERSION;
            dataPacket *request = iguanaCreateRequest(IG_EXCH_VERSIONS, 2, &clientVersion);
            if (! iguanaTransaction(conn, (iguanaPacket)request, NULL))
            {
                message(LOG_ERROR, "Server did not understand version request, aborting.  Is the igdaemon is up to date?\n");
                iguanaClose(conn);
                errno = 0;
                conn = INVALID_PIPE;
            }
            request->data = NULL;
            freeDataPacket(request);
        }
    }

    return conn;
}

PIPE_PTR iguanaConnect_real(const char *name, unsigned int protocol)
{
    return iguanaConnect_internal(name, protocol, true);
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
    if (connection == INVALID_PIPE)
        message(LOG_DEBUG3, "iguanaClose called on invalid pipe.\n");
    else
    {
#if DEBUG
message(LOG_WARN, "CLOSE %d %s(%d)\n", connection, __FILE__, __LINE__);
#endif
        closePipe(connection);
    }
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
    return packetIsError((dataPacket*)response);
}

bool iguanaTransaction(PIPE_PTR connection, const iguanaPacket request,
                       iguanaPacket *response)
{
    bool retval = false;
    dataPacket *req = (dataPacket*)request;

    /* check versions of the client and server */
    if (req &&
        iguanaWriteRequest(req, connection))
    {
        dataPacket *result = iguanaReadResponse(connection, 10000);
        if (iguanaResponseIsError(result))
            freeDataPacket(result);
        else
        {
            if (response != NULL)
                *(dataPacket**)response = result;
            else
                freeDataPacket(result);
            retval = true;
        }
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
            {
                success = true;
                continue;
            }

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
                    discard = true;
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
