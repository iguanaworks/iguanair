#pragma once

#include "devicebase.h"
#include <stdint.h>

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
    /* initialization and cleanup */
    bool (*initializeDriver)();
    void (*cleanupDriver)();

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
    void (*claimDevices)(deviceList *devList, bool claim, bool force);
    bool (*updateDeviceList)(deviceList *devList);
    unsigned int (*stopDevices)(deviceList *devList);
    unsigned int (*releaseDevices)(deviceList *devList);

    /* dump errors to stream */
    void (*printError)(int level, char *msg, deviceInfo *info);

} driverImpl;

struct logSettings;
DRIVER_API driverImpl* getImplementation(struct logSettings *globalSettings);
