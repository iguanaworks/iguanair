/****************************************************************************
 ** protocol-versions.h *****************************************************
 ****************************************************************************
 *
 * Declaration of a couple functions used to translate current
 * commands to and from older protocol versions.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#pragma once

bool translateProtocol(uint8_t *code, uint16_t protocolVersion, bool toVersion);
bool translateDevice(uint8_t *code, uint16_t deviceVersion, bool toVersion);
