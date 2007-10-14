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

#include <usb.h>
#include "list.h"

typedef struct usbId
{
    unsigned short idVendor;
    unsigned short idProduct;
} usbId;

#define INVALID_VENDOR 0
#define END_OF_USB_ID_LIST {INVALID_VENDOR,0}

typedef struct usbDevice
{
    /* fields for the linked list of devices */
    /* MUST be listed first for casting */
    itemHeader header;
    struct usbDeviceList *list;

    /* unique id (counter) */
    unsigned int id;

    /* what device id did it match? */
    usbId type;

    /* identifiers from the USB bus */
    u_int8_t busIndex, devIndex;

    /* handle(s) to the actual device */
    struct usb_dev_handle *device;

    /* read and write endpoints (set by higher layer) */
    struct usb_endpoint_descriptor *epIn, *epOut;

    /* usbclient and libusb errors */
    char *error, *usbError;

    /* set when device is logically removed from list */
    bool removed;
} usbDevice;

/* called when a new device is found by a list */
typedef void (*deviceFunc)(usbDevice *dev);

typedef struct usbDeviceList
{
    /* for keeping the list of devices */
    listHeader deviceList;

    /* a method for children to communicate back to the list owner */
    PIPE_PTR childPipe[2];

    /* count makes life easier */
    unsigned int count;

    /* id generator */
    unsigned int nextId;

    /* time outs for send and receive */
    unsigned int recvTimeout;
    unsigned int sendTimeout;

    /* ids that are in this list */
    usbId *ids;

    /* callback when creating a device */
    deviceFunc newDev;
} usbDeviceList;

/* device methods */
/* dump errors to stream */
void printError(int level, char *msg, usbDevice *handle);
/* "simplifying" send and recv wrappers with logging */
int interruptRecv(usbDevice *handle, void *buffer, int bufSize);
int interruptSend(usbDevice *handle, void *buffer, int bufSize);
/* release a single device (during destruction) */
void releaseDevice(usbDevice *handle);
/* rarely higher levels need the device handle */
#define getDevHandle(a) ((a)->device)

/* methods of a device list */
bool initDeviceList(usbDeviceList *list, usbId *ids,
                    unsigned int recvTimeout, unsigned int sendTimeout,
                    deviceFunc ndf);
bool updateDeviceList(usbDeviceList *list);
unsigned int releaseDevices(usbDeviceList *list);

#endif
