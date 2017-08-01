/****************************************************************************
 ** driver.h ****************************************************************
 ****************************************************************************
 *
 * Header for the lowest level interface to the USB device.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#pragma once

#include "direct.h"
#include "devicebase.h"

/* call this first to enable logging in this layer */
struct logSettings;
DIRECT_API void initializeDriverLayer(struct logSettings *globalSettings);

/* remaining function calls are illegal until this returns true */
DIRECT_API bool findDriver(const char *path, const char **preferred, bool onlyPreferred);

/* initialization and cleanup */
DIRECT_API bool initializeDriver();
DIRECT_API void cleanupDriver();

/* wrapped usb methods */
DIRECT_API bool findDeviceEndpoints(deviceInfo *info, int *maxPacketSize);
DIRECT_API int interruptRecv(deviceInfo *info, void *buffer, int bufSize, int timeout);
DIRECT_API int interruptSend(deviceInfo *info, void *buffer, int bufSize, int timeout);
DIRECT_API int clearHalt(deviceInfo *info, unsigned int ep);
DIRECT_API int resetDevice(deviceInfo *info);

/* miscellaneous helper functions */
DIRECT_API void getDeviceLocation(deviceInfo *info, uint8_t loc[2]);

/* release a single device (during destruction) */
DIRECT_API void releaseDevice(deviceInfo *info);
DIRECT_API void freeDevice(deviceInfo *info);

/* methods of a device list */
DIRECT_API deviceList* prepareDeviceList(usbId *ids, deviceFunc ndf);
DIRECT_API void claimDevices(deviceList *devList, bool claim, bool force);
DIRECT_API bool updateDeviceList(deviceList *devList);
DIRECT_API unsigned int stopDevices(deviceList *devList);
DIRECT_API unsigned int releaseDevices(deviceList *devList);

/* dump errors to a stream */
DIRECT_API void printError(int level, char *msg, deviceInfo *info);
