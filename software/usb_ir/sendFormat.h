/****************************************************************************
 ** sendFormat.h ************************************************************
 ****************************************************************************
 *
 * Declarations used in send commands within the device protocol.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */

#pragma once

#include "direct.h"
#include <stdint.h>

enum
{
    /* highest value of a data byte in the IR code protocol */
    MAX_DATA_BYTE = 127,

    /* we support version 0 and 1, but ver 2 is not implemented */
    COMPRESS_VER0 = 0,
    COMPRESS_VER1,
    COMPRESS_VER2
};

DIRECT_API int pulsesToIguanaSend(int carrier,
                                  uint32_t *sendCode, int length,
                                  unsigned char **results, int compress);
