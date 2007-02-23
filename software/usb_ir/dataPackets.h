/****************************************************************************
 ** dataPackets.h ***********************************************************
 ****************************************************************************
 *
 * Function declarations for handling data packets.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#ifndef _DATA_PACKETS_
#define _DATA_PACKETS_

#include "list.h"

#define DATA_PACKET_INIT {{NULL,NULL},0,0,NULL}
typedef struct dataPacket
{
    /* used internally for queuing incoming packets*/
    /* MUST be listed first for casting */
    itemHeader header;

    unsigned char code;
    int dataLen;
    unsigned char *data;
} dataPacket;

void freeDataPacket(dataPacket *packet);

bool readDataPacket(dataPacket *packet, PIPE_PTR fd, unsigned int timeout);
bool writeDataPacket(dataPacket *packet, PIPE_PTR fd);

#endif
