/****************************************************************************
 ** usbclient.h *************************************************************
 ****************************************************************************
 *
 * Header for the lowest level interface to the USB device.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#ifndef _USBCLIENT_
#define _USBCLIENT_

#include "list.h"

enum
{
    EP_IN,
    EP_OUT,

    INVALID_VENDOR = 0
};

#define END_OF_USB_ID_LIST {INVALID_VENDOR,0,NULL}

typedef struct usbId
{
    unsigned short idVendor;
    unsigned short idProduct;

    /* generic pointer to store info specific to this device type */
    void *data;
} usbId;

typedef struct deviceInfo
{
    /* unique id (counter) */
    unsigned int id;

    /* what device id did it match? */
    usbId type;

    /* set when device is logically stopped prior to removal from list */
    bool stopped;
} deviceInfo;

/* prototype of the function called when a new device is found */
typedef void (*deviceFunc)(deviceInfo *info);

/* dump errors to stream */
void printError(int level, char *msg, deviceInfo *info);

/* wrapped usb methods */
bool findDeviceEndpoints(deviceInfo *info, int *maxPacketSize);
int interruptRecv(deviceInfo *info, void *buffer, int bufSize, int timeout);
int interruptSend(deviceInfo *info, void *buffer, int bufSize, int timeout);
int clearHalt(deviceInfo *info, unsigned int ep);
int usbReset(deviceInfo *info);

/* miscellaneous helper functions */
void getDeviceLocation(deviceInfo *info, uint8_t loc[2]);

/* release a single device (during destruction) */
void releaseDevice(deviceInfo *info);
void freeDevice(deviceInfo *info);

/* methods of a device list */
typedef void deviceList;
deviceList* prepareDeviceList(usbId *ids, deviceFunc ndf);
bool updateDeviceList(deviceList *devList);
unsigned int stopDevices(deviceList *devList);
unsigned int releaseDevices(deviceList *devList);

#endif
