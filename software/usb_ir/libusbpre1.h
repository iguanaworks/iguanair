#ifndef _LIB_USB_PRE1_H_
#define _LIB_USB_PRE1_H_

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

#endif
