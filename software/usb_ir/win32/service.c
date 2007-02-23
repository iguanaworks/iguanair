#include "base.h"
#include "support.h"
#include "iguanaIR.h"
#include "usbclient.h"

/* local variables */
static usbId ids[] = {
    {0x1781, 0x0938}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};
static PIPE_PTR commPipe[2];
#ifdef LIBUSB_NO_THREADS
static unsigned int recvTimeout = 100;
#else
static unsigned int recvTimeout = 1000;
#endif
static unsigned int sendTimeout = 1000;

int main(int argc, char**argv)
{
    usbDeviceList list;

    changeLogLevel(+3);

    if (! initDeviceList(&list, ids, recvTimeout, sendTimeout, NULL)) //startWorker))
        message(LOG_ERROR, "failed to initialize device list.\n");
    else if (! updateDeviceList(&list))
        message(LOG_ERROR, "scan failed.\n");
    else
    {
        unsigned char buf[8] = { 0x00, 0x00, 0xCD, 0x01 };
        usbDevice *handle;
        struct usb_device *dev;
        struct usb_interface_descriptor *idesc;

        handle = (usbDevice*)list.deviceList.head;
        dev = usb_device(getDevHandle(handle));

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

        printf("Using entry: %p\n", list.deviceList.head);
        interruptSend(handle, buf, 4);
        interruptRecv(handle, buf, 8);
    }

    return 0;
}
