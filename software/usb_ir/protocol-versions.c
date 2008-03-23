/****************************************************************************
 ** compatibility.c *********************************************************
 ****************************************************************************
 *
 * This file provides functions to allow the driver to support older
 * versions of the protocol.  Currently this means supporting the
 * current and original versions although more could be added.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include "iguanaIR.h"
#include "compat.h"

#include "support.h"
#include "protocol-versions.h"

/* mapping from old values to new ones */
uint8_t equivalence[][2] = {
    /* "to device" codes */
    { IG_DEV_GETVERSION , IG_DEV_GETVERSION }, /* no change */
    { IG_DEV_SEND       , 0x02 },
    { IG_DEV_RECVON     , 0x03 },
    { IG_DEV_RECVOFF    , 0x04 },
    { IG_DEV_GETPINS    , 0x05 },
    { IG_DEV_SETPINS    , 0x06 },
    { IG_DEV_GETCONFIG0 , IG_DEV_GETCONFIG0 }, /* replaced */
    { IG_DEV_SETCONFIG0 , IG_DEV_SETCONFIG0 }, /* replaced */
    { IG_DEV_GETCONFIG1 , IG_DEV_GETCONFIG1 }, /* replaced */
    { IG_DEV_SETCONFIG1 , IG_DEV_SETCONFIG1 }, /* replaced */
    { IG_DEV_GETBUFSIZE , 0x0B },
    { IG_DEV_WRITEBLOCK , 0x0C },
    { IG_DEV_EXECUTE    , 0x0D },
    { IG_DEV_PINBURST   , 0x0E },
    { IG_DEV_GETID      , 0x0F },
    { IG_DEV_RESET      , IG_DEV_RESET },      /* no change */
    { IG_DEV_SETCHANNELS, 0x11 },

    /* "from device" codes */
    { IG_DEV_RECV       , 0x10 },
    { IG_DEV_OVERRECV   , 0x20 },
    { IG_DEV_OVERSEND   , 0x30 },

    /* used in response packets and as the terminator */
    { IG_DEV_ERROR     , IG_DEV_ERROR },       /* no change */
};

bool translateClient(uint8_t *code, uint16_t version, bool fromClient)
{
    bool retval = false;
    int x, dir = 0;

    if (fromClient)
        dir ^= 1;

    switch(version)
    {
    case 0:
        /* special case the one we use prior to knowing the versions */
        if (*code == IG_EXCH_VERSIONS)
            retval = true;
        else
            for(x = 0; equivalence[x][0] != IG_DEV_ERROR; x++)
                if (*code == equivalence[x][dir])
                {
                    *code = equivalence[x][dir ^ 1];
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

bool translateDevice(uint8_t *code, uint16_t version, bool fromDevice)
{
    if (version <= 4)
        version = 0;
    else
        version = 1;

    return translateClient(code, version, fromDevice);
}
