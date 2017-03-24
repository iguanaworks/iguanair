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
#pragma once

bool translateClient(uint8_t *code, uint16_t version, bool fromClient);
bool translateDevice(uint8_t *code, uint16_t version, bool fromDevice);
