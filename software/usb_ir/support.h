/****************************************************************************
 ** support.h ***************************************************************
 ****************************************************************************
 *
 * Basic supporting functions needed by the Iguanaworks tools.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */
#pragma once

#include "logging.h"

enum
{
    /* other constants */
    MAX_LINE = 1024,
    CTL_INDEX = 0xFF,

    /* for use with readPipe */
    READ  = 0,
    WRITE = 1
};
