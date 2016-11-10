/****************************************************************************
 ** libusb.c ****************************************************************
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
#include <errno.h>

#ifdef HAVE_USB_10_LIBUSB_H
#include <libusb-1.0/libusb.h>
#else
#include <libusb.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

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
    struct libusb_device_handle *device;

    /* read and write endpoints (set by higher layer) */
    const struct libusb_endpoint_descriptor *epIn, *epOut;

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

    /* ids that are in this list */
    usbId *ids;

    /* callback when creating a device */
    deviceFunc newDev;

    /* just describe the devices or claim them? */
    bool describe;

    /* if claiming attempt to unbind other drivers */
    bool force;
} usbDeviceList;

#define handleFromInfoPtr(ptr) (usbDevice*)((char*)ptr - offsetof(usbDevice, info))

static void setError(usbDevice *handle, char *error, int usbError)
{
    if (handle != NULL)
    {
        handle->error = error;
        switch(usbError)
        {
        case LIBUSB_SUCCESS:
            handle->usbError = "Success";
            errno = 0;
            break;

        case LIBUSB_ERROR_IO:
            handle->usbError = "Input/output error";
            errno = EIO;
            break;

        case LIBUSB_ERROR_INVALID_PARAM:
            handle->usbError = "Invalid parameter";
            errno = EINVAL;
            break;

        case LIBUSB_ERROR_ACCESS:
            handle->usbError = "Access denied";
            errno = EPERM;
            break;

        case LIBUSB_ERROR_NO_DEVICE:
            handle->usbError = "No such device";
            errno = ENXIO;
            break;

        case LIBUSB_ERROR_NOT_FOUND:
            handle->usbError = "Entity not found";
            errno = ENOENT;
            break;

        case LIBUSB_ERROR_BUSY:
            handle->usbError = "Resource busy";
            errno = EBUSY;
            break;

        case LIBUSB_ERROR_TIMEOUT:
            handle->usbError = "Operation timed out";
            errno = ETIMEDOUT;
            break;

        case LIBUSB_ERROR_OVERFLOW:
            handle->usbError = "Overflow";
/*            errno = XXXXX;*/
            break;

        case LIBUSB_ERROR_PIPE:
            handle->usbError = "Pipe error";
            errno = EPIPE;
            break;

        case LIBUSB_ERROR_INTERRUPTED:
            handle->usbError = "System call interrupted";
            errno = EINTR;
            break;

        case LIBUSB_ERROR_NO_MEM:
            handle->usbError = "Insufficient memory";
            errno = ENOMEM;
            break;

        case LIBUSB_ERROR_NOT_SUPPORTED:
            handle->usbError = "Operation not supported or unimplemented";
            errno = ENOSYS;
            break;

        case LIBUSB_ERROR_OTHER:
            handle->usbError = "Other error";
/*            errno = XXXXX;*/
            break;

        default:
            handle->usbError = "Untranslated error.";
            errno = -1;
            break;
        }
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
    int retval, amount;

    if (handle->info.stopped)
        return -(errno = ENXIO);

    retval = libusb_interrupt_transfer(handle->device,
                                       handle->epIn->bEndpointAddress,
                                       buffer, bufSize,
                                       &amount, timeout);
    if (retval < 0)
    {
        setError(handle, "Failed to read (interrupt end point)", retval);
        return retval;
    }

    message(LOG_DEBUG2, "i");
    appendHex(LOG_DEBUG2, buffer, amount);
    return amount;
}

static int interruptSend(deviceInfo *info,
                         void *buffer, int bufSize, int timeout)
{
    usbDevice *handle = handleFromInfoPtr(info);
    int retval, amount;

    message(LOG_DEBUG2, "o");
    appendHex(LOG_DEBUG2, buffer, bufSize);

    setError(handle, NULL, LIBUSB_SUCCESS);
    if (handle->info.stopped)
        return -(errno = ENXIO);
    /* NOTE: when firmware 0205 hangs during a send this call NEVER
       times out meaning that we have NO way to recover in the daemon
       so there's no point in trying to handle it. */
    retval = libusb_interrupt_transfer(handle->device,
                                       handle->epOut->bEndpointAddress,
                                       buffer, bufSize,
                                       &amount, timeout);
    if (retval < 0)
    {
        setError(handle, "Failed to write (interrupt end point)", retval);
        return retval;
    }

    return amount;
}

static void releaseDevice(deviceInfo *info)
{
    usbDevice *handle = handleFromInfoPtr(info);
    if (info != NULL && ! handle->removed)
    {
        int retval;

        /* record the removal */
        handle->removed = true;

        /* close the usb interface and handle */
        setError(handle, NULL, LIBUSB_SUCCESS);
        retval = libusb_release_interface(handle->device, 0);
        if (retval < 0)
            setError(handle, "Failed to release interface", retval);
        else
        {
            libusb_close(handle->device);
            handle->device = NULL;
        }

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

static void claimDevices(deviceList *devList, bool claim, bool force)
{
    ((usbDeviceList*)devList)->describe = ! claim;
    ((usbDeviceList*)devList)->force = force;
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

static bool checkInUse(struct libusb_device *dev, bool describe)
{
    bool retval = false;
    char buffer[PATH_MAX];
    struct dirent *dp;
    DIR *dir;
    int len, devIndex, busIndex;

    devIndex = libusb_get_device_address(dev);
    busIndex = libusb_get_bus_number(dev);
    if (describe)
        message(LOG_NORMAL, "  USB IR device number %d on bus %d:\n", devIndex, busIndex);

    len = sprintf(buffer, "/sys/bus/usb/devices/usb%d", busIndex);
    if ((dir = opendir(buffer)) != NULL)
    {
        while((dp = readdir(dir)) != NULL)
        {
            FILE *numfile;
            int devnum;

            sprintf(buffer + len, "/%s/devnum", dp->d_name);
            numfile = fopen(buffer, "r");
            if (numfile != NULL &&
                fscanf(numfile, "%d", &devnum) == 1 &&
                devnum == devIndex)
            {
                char link[PATH_MAX] = {0};
                retval = true;

                len += strlen(dp->d_name) + 1;
                sprintf(buffer + len, "/%s:1.0/driver", dp->d_name);
                if (readlink(buffer, link, PATH_MAX) != -1)
                {
                    if (describe)
                    {
                        char *slash;
                        slash = strrchr(link, '/');
                        if (slash == NULL)
                            message(LOG_NORMAL, "    driver link: %s\n", link);
                        else if (strcmp(slash, "/usbfs") == 0)
                            message(LOG_NORMAL, "    claimed by usbfs (likely igdaemon via libusb)\n");
                        else
                        {
                            message(LOG_NORMAL, "    claimed by kernel driver '%s'\n", slash + 1);
                            message(LOG_INFO, "Release command:\n      echo '%s:1.0' > '/sys/bus/usb/devices/usb5/%s/%s:1.0/driver/unbind'\n", dp->d_name, dp->d_name, dp->d_name);
                        }
                    }
                    else
                    {
                        /* attempt to force an unbind of the current driver */
                        FILE *output;
                        message(LOG_INFO, "Attempting to unbind current driver from %s\n", dp->d_name);
                        strcat(buffer, "/unbind");
                        if ((output = fopen(buffer, "w")) == NULL)
                        {
                            message(LOG_ERROR, "Failed to unbind %s: %d\n", dp->d_name, errno);
                            retval = false;
                        }
                        else
                        {
                            fprintf(output, "%s:1.0\n", dp->d_name);
                            fclose(output);
                        }
                    }
                }
                else if (errno == ENOENT)
                    message(LOG_NORMAL, "    currently unclaimed\n");
                else
                    message(LOG_NORMAL, "    failed to detect current state: %d\n", errno);
                break;
            }
        }
        closedir(dir);
    }

    return retval;
}

static bool claimDevice(struct libusb_device *dev, usbId *id, usbDeviceList *list, usbDevice *devPos)
{
    int retval;
    bool success = false;
    usbDevice *newDev = NULL;

    newDev = (usbDevice*)malloc(sizeof(usbDevice));
    memset(newDev, 0, sizeof(usbDevice));

    /* basic stuff */
    newDev->info.type = *id;
    newDev->busIndex = libusb_get_bus_number(dev);
    newDev->devIndex = libusb_get_device_address(dev);

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
    if ((retval = libusb_open(dev, &newDev->device)) != LIBUSB_SUCCESS)
        setError(newDev, "Failed to open usb device", retval);
    else
    {
        /* prime errno to control this loop */
        errno = 0;
        do
        {
            /* try to force an unbind but only once */
            if (errno == EBUSY && (! list->force || ! checkInUse(dev, false)))
                break;

            /* try to configure device and claim the interface */
            if ((retval = libusb_set_configuration(newDev->device, 1)) < 0)
                setError(newDev, "Failed to set device configuration", retval);
            else if ((retval = libusb_claim_interface(newDev->device, 0)) < 0)
                setError(newDev, "libusb_claim_interface failed 0", retval);
            else
            {
                insertItem(&list->deviceList,
                           (itemHeader*)devPos,
                           (itemHeader*)newDev);

                if (list->newDev != NULL)
                    list->newDev(&newDev->info);

                success = true;
                break;
            }
        }
        while(errno == EBUSY);
    }

    /* grab error if there was one */
    if (!success)
    {
        printError(LOG_ERROR, "  updateDeviceList failed", &newDev->info);
        if (errno == EBUSY)
            message(LOG_ERROR,
                    "Check device status with igdaemon --devices\n");

        if (newDev->device != NULL)
            libusb_close(newDev->device);
        free(newDev);
    }
    return success;
}

static bool updateDeviceList(deviceList *devList)
{
    usbDeviceList *list = (usbDeviceList*)devList;
    struct libusb_device *dev, **usbList;
    unsigned int pos, count = 0, newCount = 0;
    usbDevice *devPos;
    ssize_t listSize, listPos;

    /* fedora 19 seems to process udev triggers before the device is ready */
    usleep(1000);

    /* initialize usb TODO: should call libusb_exit at the end? */
    libusb_init(NULL);

    /* the next two return counts of busses and devices respectively */
    listSize = libusb_get_device_list(NULL, &usbList);

    /* search for the first device we find */
    for(listPos = 0; listPos < listSize; listPos++)
    {
        struct libusb_device_descriptor descriptor;

        dev = usbList[listPos];
        libusb_get_device_descriptor(dev, &descriptor);

        for(pos = 0; list->ids[pos].idVendor != INVALID_VENDOR; pos++)
            /* continue if we are not examining the correct device */
            if (descriptor.idVendor  == list->ids[pos].idVendor &&
                descriptor.idProduct == list->ids[pos].idProduct)
            {
                int busIndex;

                /* couldn't find the bus index as a number anywhere */
                busIndex = libusb_get_bus_number(dev);

                /* found a device instance, now find position in
                 * current list */
                devPos = (usbDevice*)firstItem(&list->deviceList);
                setError(devPos, NULL, LIBUSB_SUCCESS);
                while(devPos != NULL &&
                      (devPos->busIndex < busIndex ||
                       (devPos->busIndex == busIndex &&
                        devPos->devIndex < libusb_get_device_address(dev))))
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
                    devPos->devIndex != libusb_get_device_address(dev))
                {
                    if (list->describe)
                        checkInUse(dev, true);
                    else if (claimDevice(dev, list->ids + pos, list, devPos))
                        /* count how many devices we added */
                        newCount++;

                    /* keep a count of the number of devices */
                    count++;
                }
            }
    }
    libusb_free_device_list(usbList, 0); /* deref devices */

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
            sleep(100);
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
    struct libusb_device *dev;
    struct libusb_config_descriptor *cdesc;
    const struct libusb_interface_descriptor *idesc;

    dev = libusb_get_device(handle->device);
    libusb_get_config_descriptor(dev, 0, &cdesc);

    /* sanity checks that we're looking at an acceptable device */
    if (/*dev->descriptor.bNumConfigurations != 1 || TODO: ?*/
        cdesc->bNumInterfaces != 1 ||
        cdesc->interface[0].num_altsetting != 1)
        return false;

    idesc = &cdesc->interface[0].altsetting[0];
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
         LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN &&
        (handle->epIn->bmAttributes &
         LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT &&
        (handle->epOut->bEndpointAddress &
         LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT &&
        (handle->epOut->bmAttributes &
         LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT)
        return true;

    return false;
}

static int clearHalt(deviceInfo *info, unsigned int ep)
{
    usbDevice *handle = handleFromInfoPtr(info);
    switch (ep)
    {
    case EP_IN:
        return libusb_clear_halt(handle->device,
                                 handle->epIn->bEndpointAddress);

    case EP_OUT:
        return libusb_clear_halt(handle->device,
                                 handle->epOut->bEndpointAddress);
    }
    return -1;
}

static int resetDevice(deviceInfo *info)
{
    usbDevice *handle = handleFromInfoPtr(info);
    return libusb_reset_device(handle->device);
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
    claimDevices,
    updateDeviceList,
    stopDevices,
    releaseDevices,
    printError
};

driverImpl* getImplementation()
{
    return &impl_libusbpre1;
}
