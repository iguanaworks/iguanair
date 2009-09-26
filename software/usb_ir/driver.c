#include "iguanaIR.h"
#include "compat.h"

#include "driverapi.h"

/* drivers are currently compiled in, and this is the list */
extern driverImpl impl_libusbpre1;

/* this will contain pointers to the driver we choose */
static driverImpl *implementation = NULL;

bool findDriver()
{
    implementation = &impl_libusbpre1;
    return true;
}

void printError(int level, char *msg, deviceInfo *info)
{
    implementation->printError(level, msg, info);
}

bool findDeviceEndpoints(deviceInfo *info, int *maxPacketSize)
{
    return implementation->findDeviceEndpoints(info, maxPacketSize);
}

int interruptRecv(deviceInfo *info, void *buffer, int bufSize, int timeout)
{
    return implementation->interruptRecv(info, buffer, bufSize, timeout);
}

int interruptSend(deviceInfo *info, void *buffer, int bufSize, int timeout)
{
    return implementation->interruptSend(info, buffer, bufSize, timeout);
}

int clearHalt(deviceInfo *info, unsigned int ep)
{
    return implementation->clearHalt(info, ep);
}

int resetDevice(deviceInfo *info)
{
    return implementation->resetDevice(info);
}

void getDeviceLocation(deviceInfo *info, uint8_t loc[2])
{
    implementation->getDeviceLocation(info, loc);
}

void releaseDevice(deviceInfo *info)
{
    implementation->releaseDevice(info);
}

void freeDevice(deviceInfo *info)
{
    implementation->freeDevice(info);
}

deviceList* prepareDeviceList(usbId *ids, deviceFunc ndf)
{
    return implementation->prepareDeviceList(ids, ndf);
}

bool updateDeviceList(deviceList *devList)
{
    return implementation->updateDeviceList(devList);
}

unsigned int stopDevices(deviceList *devList)
{
    return implementation->stopDevices(devList);
}

unsigned int releaseDevices(deviceList *devList)
{
    return implementation->releaseDevices(devList);
}
