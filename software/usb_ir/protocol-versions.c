/****************************************************************************
 ** protocol-versions.c *****************************************************
 ****************************************************************************
 *
 * This file provides functions to allow the driver to support older
 * versions of the protocol.  Currently this means supporting the
 * current and original versions although more could be added.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include "iguanaIR.h"
#include "compat.h"

#include "logging.h"
#include "protocol-versions.h"

typedef uint8_t codeMap[][2];

/* "to device" code translations for protocol v0 */
static codeMap codeMap0 = {
    { IG_DEV_ERROR      , 0x00 }, /* no change */
    { IG_DEV_GETVERSION , 0x01 }, /* no change */
    { IG_DEV_SEND       , 0x02 },
    { IG_DEV_RECVON     , 0x03 },
    { IG_DEV_RECVOFF    , 0x04 },
    { IG_DEV_GETPINS    , 0x05 },
    { IG_DEV_SETPINS    , 0x06 },
    { IG_DEV_GETCONFIG0 , 0x07 }, /* replaced */
    { IG_DEV_SETCONFIG0 , 0x08 }, /* replaced */
    { IG_DEV_GETCONFIG1 , 0x09 }, /* replaced */
    { IG_DEV_SETCONFIG1 , 0x0A }, /* replaced */
    { IG_DEV_GETBUFSIZE , 0x0B },
    { IG_DEV_WRITEBLOCK , 0x0C },
    { IG_DEV_EXECUTE    , 0x0D },
    { IG_DEV_PINBURST   , 0x0E },
    { IG_DEV_GETID      , 0x0F },
    { IG_DEV_SETCHANNELS, 0x11 },
    { IG_DEV_RECV       , 0x10 },
    { IG_DEV_OVERRECV   , 0x20 },
    { IG_DEV_OVERSEND   , 0x30 },
    { IG_DEV_RESET      , 0xFF }, /* never changes and end of list */
};

static codeMap *codeMaps[] = {
    &codeMap0
};

bool translateProtocol(uint8_t *code, uint16_t protocolVersion, bool toVersion)
{
    bool retval = false;

    /* special case the protocol we use prior to knowing the versions */
    if (protocolVersion == IG_PROTOCOL_VERSION || *code == IG_EXCH_VERSIONS)
        retval = true;
    else if (protocolVersion > IG_PROTOCOL_VERSION)
        message(LOG_ERROR, "Cannot translate protocols > %d\n", IG_PROTOCOL_VERSION);
    else
    {
        int x, dir = toVersion ? 0 : 1;
        for(x = 0; (*codeMaps[protocolVersion])[x][0] != IG_DEV_RESET; x++)
            if (*code == (*codeMaps[protocolVersion])[x][dir])
            {
                *code = (*codeMaps[protocolVersion])[x][dir ^ 1];
                retval = true;
                break;
            }
    }

    return retval;
}

bool translateDevice(uint8_t *code, uint16_t deviceVersion, bool toVersion)
{
    int protocolVersion = IG_PROTOCOL_VERSION;
    if (deviceVersion <= 4)
        protocolVersion = 0;
    return translateProtocol(code, protocolVersion, toVersion);
}
