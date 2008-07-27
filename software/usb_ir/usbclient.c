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
#include "iguanaIR.h"
#include "compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <usb.h>
#include <errno.h>

#include "pipes.h"
#include "support.h"
#include "usbclient.h"

/* We want to use the constantly incrementing devnum if it exists.
 * This functionality was introduced in version 0.1.9 of libusb.
 */
#if LIBUSB_VERSION < 109
  #define DEVNUM(dev) (dev->bus->location)
#else
  #define DEVNUM(dev) (dev->devnum)
#endif

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

void printError(int level, char *msg, usbDevice *handle)
{
    if (msg != NULL)
        if (handle == NULL || handle->error == NULL)
            message(level, "%s\n", msg);
        else if (handle->usbError == NULL)
            message(level, "%s: %s\n", msg, handle->error);
        else
            message(level,
                    "%s: %s: %s\n", msg, handle->error, handle->usbError);
    else if (handle != NULL && handle->error != NULL)
        if (handle->usbError == NULL)
            message(level, "%s\n", handle->error);
        else
            message(level, "%s: %s\n", handle->error, handle->usbError);
    else
        message(level, "No error recorded\n");
}

int interruptRecv(usbDevice *handle, void *buffer, int bufSize)
{
    int retval;

    retval = usb_interrupt_read(handle->device,
                                handle->epIn->bEndpointAddress,
                                buffer, bufSize,
                                handle->list->recvTimeout);
    if (retval < 0)
    {
        setError(handle, "Failed to read (interrupt end point)");
#ifdef WIN32
        /* windows is not setting errno, and is instead returning the error */
        errno = -retval;
#endif
    }
    else
    {
        message(LOG_DEBUG2, "i");
        appendHex(LOG_DEBUG2, buffer, retval);
    }

    return retval;
}

int interruptSend(usbDevice *handle, void *buffer, int bufSize)
{
    int retval;

    message(LOG_DEBUG2, "o");
    appendHex(LOG_DEBUG2, buffer, bufSize);

    setError(handle, NULL);
    retval = usb_interrupt_write(handle->device,
                                 handle->epOut->bEndpointAddress,
                                 buffer, bufSize,
                                 handle->list->sendTimeout);
    if (retval < 0)
        setError(handle, "Failed to write (interrupt end point)");

    return retval;
}

void releaseDevice(usbDevice *handle)
{
    if (handle != NULL && ! handle->removed)
    {
        /* record the removal */
        handle->removed = true;

        /* close the usb interface and handle */
        setError(handle, NULL);
        if (usb_release_interface(handle->device, 0) < 0 && errno != ENODEV)
            setError(handle, "Failed to release interface");
        else if (usb_close(handle->device) < 0)
            setError(handle, "Failed to close device");

        /* print errors from the usb closes */
        if (handle->error != NULL)
            printError(LOG_ERROR, NULL, handle);

        /* remove the device from the list */
        removeItem((itemHeader*)handle);
    }
}

bool initDeviceList(usbDeviceList *list, usbId *ids,
                    unsigned int recvTimeout, unsigned int sendTimeout,
                    deviceFunc ndf)
{
    bool retval = false;

    memset(list, 0, sizeof(usbDeviceList));
    if (createPipePair(list->childPipe))
    {
#if DEBUG
printf("OPEN %d %s(%d)\n", list->childPipe[0], __FILE__, __LINE__);
printf("OPEN %d %s(%d)\n", list->childPipe[1], __FILE__, __LINE__);
#endif

        list->ids = ids;
        list->recvTimeout = recvTimeout;
        list->sendTimeout = sendTimeout;
        list->newDev = ndf;
        retval = true;
    }

    return retval;
}

/* increment the id for each item in the list */
static bool findId(itemHeader *item, void *userData)
{
    unsigned int *id = (unsigned int*)userData;
    usbDevice *usbDev = (usbDevice*)item;

    if (! usbDev->removed && usbDev->id == *id)
        (*id)++;
    return true;
}

bool updateDeviceList(usbDeviceList *list)
{
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
                            devPos->devIndex < DEVNUM(dev))))
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
                        devPos->devIndex != DEVNUM(dev))
                    {
                        bool success = false;
                        usbDevice *newDev = NULL;
                        newDev = (usbDevice*)malloc(sizeof(usbDevice));
                        memset(newDev, 0, sizeof(usbDevice));

                        /* basic stuff */
                        newDev->list = list;
                        newDev->busIndex = busIndex;
                        newDev->devIndex = DEVNUM(dev);

                        /* determine the id (reusing if possible) */
                        newDev->id = 0;
                        while(true)
                        {
                            unsigned int prev = newDev->id;
                            forEach(&list->deviceList, findId, &newDev->id);
                            if (prev == newDev->id)
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
                            message(LOG_ERROR, "  trying to claim usb:%d:%d\n", busIndex, DEVNUM(dev));
                            printError(LOG_ERROR,
                                       "  updateDeviceList failed", newDev);

                            if (newDev->device != NULL)
                                usb_close(newDev->device);
                            free(newDev);
                            return false;
                        }
                        else if (list->newDev != NULL)
                            list->newDev(newDev);

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
                    devPos->id, (void*)devPos);
    }

    return true;
}

unsigned int releaseDevices(usbDeviceList *list)
{
    unsigned int count = list->deviceList.count;
    usbDevice *head;

    while((head = (usbDevice*)firstItem(&list->deviceList)) != NULL)
        releaseDevice(head);

    return count;
}
