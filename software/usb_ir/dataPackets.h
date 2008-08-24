/****************************************************************************
 ** dataPackets.h ***********************************************************
 ****************************************************************************
 *
 * Function declarations for handling data packets.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */
#ifndef _DATA_PACKETS_
#define _DATA_PACKETS_

#include "list.h"

#define DATA_PACKET_INIT {{NULL,NULL,NULL},0,0,NULL}
typedef struct dataPacket
{
    /* used internally for queuing incoming packets*/
    /* MUST be listed first for casting */
    itemHeader header;

    unsigned char code;
    int dataLen;
    unsigned char *data;
} dataPacket;

bool readDataPacket(dataPacket *packet, PIPE_PTR fd, unsigned int timeout);
bool writeDataPacket(dataPacket *packet, PIPE_PTR fd);
void freeDataPacket(dataPacket *packet);

#endif
