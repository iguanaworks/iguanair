/****************************************************************************
 ** dataPackets.c ***********************************************************
 ****************************************************************************
 *
 * Functions for handling data packets (aka iguanaPackets)
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
#include <errno.h>
#include <time.h>

#include "pipes.h"
#include "logging.h"
#include "dataPackets.h"

bool readDataPacket(dataPacket *packet, PIPE_PTR fd, unsigned int timeout)
{
    bool retval = false;
    int result;
    uint64_t then, now;

    then = microsSinceX();
    result = readPipeTimed(fd, (char*)packet, sizeof(dataPacket), timeout);
    if (result == sizeof(dataPacket))
    {
        if (packet->dataLen <= 0)
        {
            packet->data = NULL;
            retval = true;
        }
        else
        {
            packet->data = (unsigned char*)malloc(packet->dataLen);
            if (packet->data != NULL)
            {
                unsigned int timePassed;

                now = microsSinceX();
                timePassed = (unsigned int)(now - then) / 1000;
                if (timePassed <= timeout)
                {
                    result = readPipeTimed(fd, (char*)packet->data,
                                           packet->dataLen,
                                           timeout - timePassed);
                    if (result != packet->dataLen)
                    {
                        free(packet->data);
                        packet->data = NULL;
                    }
                    else
                        retval = true;
                }
            }
        }
    }

    if (result == 0)
        errno = ETIMEDOUT;

    return retval;
}

bool writeDataPacket(const dataPacket *packet, PIPE_PTR fd, unsigned int timeout)
{
    bool retval = false;
    int result;
    uint64_t then;

    then = microsSinceX();
    result = writePipeTimed(fd, packet, sizeof(dataPacket), timeout);
    if (result == sizeof(dataPacket))
    {
        if (packet->dataLen > 0)
        {
            unsigned int elapsed = (unsigned int)(microsSinceX() - then) / 1000;
            if (timeout != WAIT_FOREVER)
            {
                if (timeout > elapsed)
                    timeout -= elapsed;
                else
                    timeout = 0;
            }

            result = writePipeTimed(fd, packet->data, packet->dataLen, timeout);
            if (result == packet->dataLen)
                retval = true;
        }
        else
            retval = true;
    }

    return retval;
}

void freeDataPacket(dataPacket *packet)
{
    if (packet != NULL)
    {
        free(packet->data);
        free(packet);
    }
}

bool packetIsError(const dataPacket *packet)
{
    int retval = true;

    errno = EIO;
    if (packet != NULL)
    {
        if (packet->code != IG_DEV_ERROR)
            retval = false;
        else
            errno = -packet->dataLen;
    }

    return retval;
}
