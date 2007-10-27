/****************************************************************************
 ** compatibility.c *********************************************************
 ****************************************************************************
 *
 * This file provides functions to allow the driver to support older 
 * versions of the protocol. 
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include <stdint.h>
#include "iguanaIR.h"
#include "support.h"
#include "dataPackets.h"
#include "compatibility.h"

uint8_t equivalence[][2] = {
    /* used in response packets */
    { IG_DEV_ERROR     , IG_DEV_ERROR },

    /* "to device" codes */
    { IG_DEV_GETVERSION , IG_DEV_GETVERSION },
    { IG_DEV_SEND       , 0x02 },
    { IG_DEV_RECVON     , 0x03 },
    { IG_DEV_RECVOFF    , 0x04 },
    { IG_DEV_GETPINS    , IG_DEV_GETPINS },
    { IG_DEV_SETPINS    , IG_DEV_SETPINS },
    { IG_DEV_GETCONFIG0 , IG_DEV_GETCONFIG0 },
    { IG_DEV_SETCONFIG0 , IG_DEV_SETCONFIG0 },
    { IG_DEV_GETCONFIG1 , IG_DEV_GETCONFIG1 },
    { IG_DEV_SETCONFIG1 , IG_DEV_SETCONFIG1 },
    { IG_DEV_GETBUFSIZE , 0x0B },
    { IG_DEV_WRITEBLOCK , 0x0C },
    { IG_DEV_EXECUTE    , IG_DEV_EXECUTE },
    { IG_DEV_BULKPINS   , IG_DEV_BULKPINS },
    { IG_DEV_GETID      , IG_DEV_GETID },
    { IG_DEV_RESET      , IG_DEV_RESET },
    { IG_DEV_SETCHANNELS, 0x11 },
    { IG_DEV_RAWRECVON  , IG_DEV_RAWRECVON },

    /* "from device" codes */
    { IG_DEV_RECV       , 0x10 },
    { IG_DEV_OVERRECV   , IG_DEV_OVERRECV },
    { IG_DEV_OVERSEND   , IG_DEV_OVERSEND },

    /* used as the terminator too */
    { IG_EXCH_VERSIONS , IG_EXCH_VERSIONS },
};

bool translateClient(dataPacket *packet, uint16_t version, bool fromClient)
{
    bool retval = false;
    int x, dir = 0;

    if (fromClient)
        dir ^= 1;

    switch(version)
    {
    case 0:
        if (packet->code == IG_EXCH_VERSIONS)
            retval = true;
        else
            for(x = 0; equivalence[x][0] != IG_EXCH_VERSIONS; x++)
                if (packet->code == equivalence[x][dir])
                {
                    packet->code = equivalence[x][dir ^ 1];
                    retval = true;
                    break;
                }

        break;

    case 1:
        retval = true;
        break;

    default:
        message(LOG_ERROR, "Unrecognized protocol version %d\n", version);
        break;
    }

    return retval;
}

bool translateDevice(dataPacket *packet, uint16_t version, bool fromDevice)
{
    if (version <= 4)
        version = 0;
    else
        version = 1;

    return translateClient(packet, version, fromDevice);
}
