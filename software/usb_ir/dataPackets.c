/****************************************************************************
 ** dataPackets.c ***********************************************************
 ****************************************************************************
 *
 * Functions for handling data packets (aka iguanaPackets)
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
#include <time.h>

#include "support.h"
#include "dataPackets.h"

void freeDataPacket(dataPacket *packet)
{
    if (packet != NULL)
    {
        free(packet->data);
        free(packet);
    }
}

bool readDataPacket(dataPacket *packet, int fd, unsigned int timeout)
{
    bool retval = false;
    int result;
    struct timespec start, now;

    result = clock_gettime(CLOCK_MONOTONIC, &start);
    if (result == 0)
        result = readBytes(fd, timeout,
                           (char*)packet, sizeof(dataPacket));
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

                result = clock_gettime(CLOCK_MONOTONIC, &now);
                if (result == 0)
                {
                    timePassed = (now.tv_sec - start.tv_sec) * 1000 + 
                                 (now.tv_nsec - start.tv_nsec) / 1000000;

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
                        result = readBytes(fd, timeout - timePassed,
                                           (char*)packet->data,
                                           packet->dataLen);
                }

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

    if (result == 0)
        errno = ETIMEDOUT;
    
    return retval;
}

bool writeDataPacket(dataPacket *packet, int fd)
{
    bool retval = false;
    int result;

    result = write(fd, packet, sizeof(dataPacket));
    if (result == sizeof(dataPacket))
    {
        if (packet->dataLen > 0)
        {
            result = write(fd, packet->data, packet->dataLen);
            if (result == packet->dataLen)
                retval = true;
        }
        else
            retval = true;
    }

    return retval;
}
