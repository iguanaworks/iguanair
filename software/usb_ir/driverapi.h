#ifndef _DRIVER_API_
#define _DRIVER_API_

#include "devicebase.h"

#ifdef WIN32
    #ifdef DRIVER_EXPORTS
        #define DRIVER_API __declspec(dllexport)
    #else
        #define DRIVER_API __declspec(dllimport)
    #endif
#else
    #ifdef DRIVER_EXPORTS
        #define DRIVER_API __attribute__((visibility("default")))
    #else
        #define DRIVER_API
    #endif
#endif

typedef struct driverImpl
{
    /* wrapped usb methods */
    bool (*findDeviceEndpoints)(deviceInfo *info, int *maxPacketSize);
    int (*interruptRecv)(deviceInfo *info,
                         void *buffer, int bufSize, int timeout);
    int (*interruptSend)(deviceInfo *info,
                         void *buffer, int bufSize, int timeout);
    int (*clearHalt)(deviceInfo *info, unsigned int ep);
    int (*resetDevice)(deviceInfo *info);

    /* miscellaneous helper functions */
    void (*getDeviceLocation)(deviceInfo *info, uint8_t loc[2]);

    /* release a single device (during destruction) */
    void (*releaseDevice)(deviceInfo *info);
    void (*freeDevice)(deviceInfo *info);

    /* methods of a device list */
    deviceList* (*prepareDeviceList)(usbId *ids, deviceFunc ndf);
    bool (*updateDeviceList)(deviceList *devList);
    unsigned int (*stopDevices)(deviceList *devList);
    unsigned int (*releaseDevices)(deviceList *devList);

    /* dump errors to stream */
    void (*printError)(int level, char *msg, deviceInfo *info);

} driverImpl;

DRIVER_API driverImpl* getImplementation();

#endif
