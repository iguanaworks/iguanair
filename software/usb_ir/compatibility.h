/****************************************************************************
 ** compatibility.h *********************************************************
 ****************************************************************************
 *
 * TODO: DESCRIBE AND DOCUMENT THIS FILE
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#ifndef _COMPATIBILITY_
#define _COMPATIBILITY_

bool translateClient(dataPacket *packet, uint16_t version, bool copyTo);
bool translateDevice(dataPacket *packet, uint16_t version, bool copyTo);

#endif
