/****************************************************************************
 ** usbclient.c *************************************************************
 ****************************************************************************
 *
 * Lowest level interface to the USB devices.  
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "../iguanaIR.h"
#include "../compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <usb.h>
#include <errno.h>

#include "../pipes.h"
#include "../support.h"
#include "../driverapi.h"

#include "../list.h"

typedef struct usbDevice
{
    /* fields for the linked list of devices */
    /* MUST be listed first for casting */
    itemHeader header;

    /* identifiers from the USB bus */
    uint8_t busIndex, devIndex;

    /* handle(s) to the actual device */
    struct usb_dev_handle *device;

    /* read and write endpoints (set by higher layer) */
    struct usb_endpoint_descriptor *epIn, *epOut;

    /* usbclient and libusb errors */
    char *error, *usbError;

    /* set when device is logically removed from list */
    bool removed;

    deviceInfo info;
} usbDevice;

typedef struct usbDeviceList
{
    /* for keeping the list of devices */
    listHeader deviceList;

    /* count makes life easier */
    unsigned int count;

    /* id generator */
    unsigned int nextId;

    /* ids that are in this list */
    usbId *ids;

    /* callback when creating a device */
    deviceFunc newDev;
} usbDeviceList;

#define handleFromInfoPtr(ptr) (usbDevice*)((char*)ptr - offsetof(usbDevice, info))




static void setError(usbDevice *handle, char *error)
{
    if (handle != NULL)
    {
        /* clear error codes */
        handle->error = error;
        if (error != NULL)
            handle->usbError = usb_strerror();
    }
}

static void printError(int level, char *msg, deviceInfo *info)
{
    usbDevice *handle = handleFromInfoPtr(info);
    if (msg != NULL)
        if (info == NULL || handle->error == NULL)
            message(level, "%s\n", msg);
        else if (handle->usbError == NULL)
            message(level, "%s: %s\n", msg, handle->error);
        else
            message(level,
                    "%s: %s: %s\n", msg, handle->error, handle->usbError);
    else if (info != NULL && handle->error != NULL)
        if (handle->usbError == NULL)
            message(level, "%s\n", handle->error);
        else
            message(level, "%s: %s\n", handle->error, handle->usbError);
    else
        message(level, "No error recorded\n");
}

static int interruptRecv(deviceInfo *info,
                         void *buffer, int bufSize, int timeout)
{
    usbDevice *handle = handleFromInfoPtr(info);
    int retval;

    if (handle->info.stopped)
        return -(errno = ENXIO);

    retval = usb_interrupt_read(handle->device,
                                handle->epIn->bEndpointAddress,
                                buffer, bufSize,
                                timeout);
    if (retval < 0)
    {
        setError(handle, "Failed to read (interrupt end point)");
        /* libusb does not reliably set errno */
        if (retval < -1 || !errno)
            errno = -retval;
    }
    else
    {
        message(LOG_DEBUG2, "i");
        appendHex(LOG_DEBUG2, buffer, retval);
    }

    return retval;
}

static int interruptSend(deviceInfo *info,
                         void *buffer, int bufSize, int timeout)
{
    usbDevice *handle = handleFromInfoPtr(info);
    int retval;

    message(LOG_DEBUG2, "o");
    appendHex(LOG_DEBUG2, buffer, bufSize);

    setError(handle, NULL);
    if (handle->info.stopped)
        return -(errno = ENXIO);
    /* NOTE: when firmware 0205 hangs during a send this call NEVER
       times out meaning that we have NO way to recover in the daemon
       so there's no point in trying to handle it. */
    retval = usb_interrupt_write(handle->device,
                                 handle->epOut->bEndpointAddress,
                                 buffer, bufSize,
                                 timeout);
    if (retval < 0)
        setError(handle, "Failed to write (interrupt end point)");

    return retval;
}

static void releaseDevice(deviceInfo *info)
{
    usbDevice *handle = handleFromInfoPtr(info);
    if (info != NULL && ! handle->removed)
    {
        /* record the removal */
        handle->removed = true;

        /* close the usb interface and handle */
        setError(handle, NULL);
        if (usb_release_interface(handle->device, 0) < 0 && errno != ENODEV)
            setError(handle, "Failed to release interface");
/* TODO: valgrind notes an error here, but this shuts down quickly... */
        else if (usb_close(handle->device) < 0)
            setError(handle, "Failed to close device");
        else
            handle->device = NULL;

        /* print errors from the usb closes */
        if (handle->error != NULL)
            printError(LOG_ERROR, NULL, &handle->info);

        /* remove the device from the list */
        removeItem((itemHeader*)handle);
    }
}

static void freeDevice(deviceInfo *info)
{
    usbDevice *handle = handleFromInfoPtr(info);
    free(handle);
}

/* the list must be allocated here where the struct is defined */
static deviceList* prepareDeviceList(usbId *ids, deviceFunc ndf)
{
    usbDeviceList *list;
    list = (usbDeviceList*)malloc(sizeof(usbDeviceList));
    if (list != NULL)
    {
        memset(list, 0, sizeof(usbDeviceList));
        list->ids = ids;
        list->newDev = ndf;
    }
    return list;
}

/* increment the id for each item in the list */
static bool findId(itemHeader *item, void *userData)
{
    unsigned int *id = (unsigned int*)userData;
    usbDevice *usbDev = (usbDevice*)item;

    if (! usbDev->removed && usbDev->info.id == *id)
        (*id)++;
    return true;
}

static bool updateDeviceList(deviceList *devList)
{
    usbDeviceList *list = (usbDeviceList*)devList;
    struct usb_bus *bus;
    struct usb_device *dev;
    unsigned int pos, count = 0, newCount = 0;
    usbDevice *devPos;

    /* initialize usb */
    usb_init();

    /* the next two return counts of busses and devices respectively */
    usb_find_busses();
    usb_find_devices();

    /* search for the first device we find */
    for (bus = usb_get_busses(); bus; bus = bus->next)
        for (dev = bus->devices; dev; dev = dev->next)
            for(pos = 0; list->ids[pos].idVendor != INVALID_VENDOR; pos++)
                /* continue if we are not examining the correct device */
                if (dev->descriptor.idVendor  == list->ids[pos].idVendor &&
                    dev->descriptor.idProduct == list->ids[pos].idProduct)
                {
                    int busIndex;

                    /* couldn't find the bus index as a number anywhere */
                    busIndex = atoi(bus->dirname);

                    /* found a device instance, now find position in
                     * current list */
                    devPos = (usbDevice*)firstItem(&list->deviceList);
                    setError(devPos, NULL);
                    while(devPos != NULL &&
                          (devPos->busIndex < busIndex ||
                           (devPos->busIndex == busIndex &&
                            devPos->devIndex < LIBUSB_DEVNUM(dev))))
                        /* used to release devices here, since they
                         * are no longer used, however, this races
                         * with reinsertion of the device, and
                         * therefore reuse of the ID.  Additionally,
                         * unplugs are detected now, so it is no
                         * longer necessary. */
                        devPos = (usbDevice*)devPos->header.next;

                    /* append or insert a new device */
                    if (devPos == NULL ||
                        devPos->busIndex != busIndex ||
                        devPos->devIndex != LIBUSB_DEVNUM(dev))
                    {
                        bool success = false;
                        usbDevice *newDev = NULL;
                        newDev = (usbDevice*)malloc(sizeof(usbDevice));
                        memset(newDev, 0, sizeof(usbDevice));

                        /* basic stuff */
                        newDev->info.type = list->ids[pos];
                        newDev->busIndex = (uint8_t)busIndex;
                        newDev->devIndex = LIBUSB_DEVNUM(dev);

                        /* determine the id (reusing if possible) */
                        newDev->info.id = 0;
                        while(true)
                        {
                            unsigned int prev = newDev->info.id;
                            forEach(&list->deviceList,
                                    findId, &newDev->info.id);
                            if (prev == newDev->info.id)
                                break;
                        }

                        /* open a handle to the usb device */
                        if ((newDev->device = usb_open(dev)) == NULL)
                            setError(newDev, "Failed to open usb device");
                        else if (usb_set_configuration(newDev->device, 1) < 0)
                            setError(newDev, "Failed to set device configuration");
                        else if (! dev->config)
                            setError(newDev, "Failed to receive device descriptors");
                        /* claim the interface */
                        else if (usb_claim_interface(newDev->device, 0) < 0)
                            setError(newDev, "usb_claim_interface failed 0");
                        else
                        {
                            insertItem(&list->deviceList,
                                       (itemHeader*)devPos,
                                       (itemHeader*)newDev);
                            success = true;
                        }

                        /* grab error if there was one */
                        if (!success)
                        {
                            if (errno == EBUSY)
                                message(LOG_ERROR,
                                        "Is igdaemon already running?\n");
                            message(LOG_ERROR, "  trying to claim usb:%d:%d\n",
                                    busIndex, LIBUSB_DEVNUM(dev));
                            printError(LOG_ERROR, "  updateDeviceList failed",
                                       &newDev->info);

                            if (newDev->device != NULL)
                                usb_close(newDev->device);
                            free(newDev);
                            return false;
                        }
                        else if (list->newDev != NULL)
                            list->newDev(&newDev->info);

                        /* count how many devices we added */
                        newCount++;
                    }

                    /* keep a count of the number of devices */
                    count++;
                }

    if (wouldOutput(LOG_DEBUG) && newCount > 0)
    {
        unsigned int index = 0;

        message(LOG_DEBUG, "Handling %d device(s):\n", count);
        devPos = (usbDevice*)list->deviceList.head;

        for(; devPos; devPos = (usbDevice*)devPos->header.next)
            message(LOG_DEBUG,
                    "  %d) usb:%d.%d id=%d addr=%p\n", index++,
                    devPos->busIndex, devPos->devIndex,
                    devPos->info.id, (void*)devPos);
    }

    return true;
}

static bool setStopped(itemHeader *item, void UNUSED(*userData))
{
    usbDevice *head = (usbDevice*)item;
    head->info.stopped = true;
    return true;
}

static unsigned int stopDevices(deviceList *devList)
{
    usbDeviceList *list = (usbDeviceList*)devList;
    unsigned int count = list->deviceList.count;

    forEach(&list->deviceList, setStopped, NULL);

    return count;
}

static unsigned int releaseDevices(deviceList *devList)
{
    usbDeviceList *list = (usbDeviceList*)devList;
    unsigned int count = list->deviceList.count;
    usbDevice *head, *prev = NULL;

    /* loop, but if head does not change then sleep a bit */
    while((head = (usbDevice*)firstItem(&list->deviceList)) != NULL)
    {
        if (head != prev)
            releaseDevice(&head->info);
        else
            Sleep(100);
        prev = head;
    }

    /* illegal to access the list after this call */
    free(list);
    return count;
}

/* set dev_ep_in and dev_ep_out to the in/out endpoints of the given
 * device. returns 1 on success, 0 on failure. */
static bool findDeviceEndpoints(deviceInfo *info, int *maxPacketSize)
{
    usbDevice *handle = handleFromInfoPtr(info);
    struct usb_device *dev;
    struct usb_interface_descriptor *idesc;

    dev = usb_device(handle->device);

    /* sanity checks that we're looking at an acceptable device */
    if (dev->descriptor.bNumConfigurations != 1 ||
        dev->config[0].bNumInterfaces != 1 ||
        dev->config[0].interface[0].num_altsetting != 1)
        return false;

    idesc = &dev->config[0].interface[0].altsetting[0];
    if (idesc->bNumEndpoints != 2)
        return false;

    /* grab the pointers */
    handle->epIn = &idesc->endpoint[0];
    handle->epOut = &idesc->endpoint[1];

    /* set the max packet size to the minimum of in and out */
    *maxPacketSize = idesc->endpoint[0].wMaxPacketSize;
    if (*maxPacketSize > idesc->endpoint[1].wMaxPacketSize)
        *maxPacketSize = idesc->endpoint[1].wMaxPacketSize;

    /* check the pointer targets */
    if ((handle->epIn->bEndpointAddress &
         USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN &&
        (handle->epIn->bmAttributes &
         USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_INTERRUPT &&
        (handle->epOut->bEndpointAddress &
         USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_OUT &&
        (handle->epOut->bmAttributes &
         USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_INTERRUPT)
        return true;

    return false;
}

static int clearHalt(deviceInfo *info, unsigned int ep)
{
    usbDevice *handle = handleFromInfoPtr(info);
    switch (ep)
    {
    case EP_IN:
        return usb_clear_halt(handle->device, 
                              handle->epIn->bEndpointAddress);

    case EP_OUT:
        return usb_clear_halt(handle->device, 
                              handle->epOut->bEndpointAddress);
    }
    return -1;
}

static int resetDevice(deviceInfo *info)
{
    usbDevice *handle = handleFromInfoPtr(info);
    return usb_reset(handle->device);
}

static void getDeviceLocation(deviceInfo *info, uint8_t loc[2])
{
    usbDevice *handle = handleFromInfoPtr(info);
    loc[0] = handle->busIndex;
    loc[1] = handle->devIndex;
}

driverImpl impl_libusbpre1 = {
    findDeviceEndpoints,
    interruptRecv,
    interruptSend,
    clearHalt,
    resetDevice,
    getDeviceLocation,
    releaseDevice,
    freeDevice,
    prepareDeviceList,
    updateDeviceList,
    stopDevices,
    releaseDevices,
    printError
};

driverImpl* getImplementation()
{
    return &impl_libusbpre1;
}
