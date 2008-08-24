/****************************************************************************
 ** dataPackets.c ***********************************************************
 ****************************************************************************
 *
 * Functions for handling data packets (aka iguanaPackets)
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
#include <errno.h>
#include <time.h>

#include "pipes.h"
#include "support.h"
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
            packet->data = malloc(packet->dataLen);
            if (packet->data != NULL)
            {
                unsigned int timePassed;

                now = microsSinceX();
                timePassed = (unsigned int)(now - then) / 1000;
#if 0
                /* Leaving this for now, though it should not be
                    * necessary ever again */
                if (timePassed > timeout)
                {
                    fprintf(stderr, "TIMEOUT ON PAYLOAD!!! (%u > %u)\n",
                            timePassed, timeout);
                    timePassed = timeout;
                }
#endif
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

bool writeDataPacket(dataPacket *packet, PIPE_PTR fd)
{
    bool retval = false;
    int result;

    result = writePipe(fd, packet, sizeof(dataPacket));
    if (result == sizeof(dataPacket))
    {
        if (packet->dataLen > 0)
        {
            result = writePipe(fd, packet->data, packet->dataLen);
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
