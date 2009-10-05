#include "iguanaIR.h"
#include "compat.h"

#include "stdio.h"
#include "string.h"
#include "limits.h"
#include "sys/types.h"
#include "dirent.h"

#include "support.h"
#include "driverapi.h"

/* will hold driver-supplied function pointers */
static driverImpl *implementation = NULL;

/* guessing the following 2 functions will need to be re-implemented
   on other platforms */
#include <dlfcn.h>

static bool findDriversDir(char *path)
{
    char *pos;
    pid_t pid;

    pid = getpid();
    path[PATH_MAX - 1] = '\0';
    if (snprintf(path, PATH_MAX - 1, "/proc/%d/exe", pid) <= 0 ||
        readlink(path, path, PATH_MAX - 1) <= 0)
        return false;

    pos = strrchr(path, '/');
    strcpy(pos + 1, "drivers/");
    return true;
}

bool loadDriver(char *path)
{
    char *ext;
    void *library;
    driverImpl* (*getImplementation)();

    ext = strrchr(path, '.');
    if (ext != NULL &&
        strcmp(ext, ".so") == 0 &&
        (library = dlopen(path, RTLD_LAZY | RTLD_LOCAL)) != NULL &&
        (*(void**)(&getImplementation) = dlsym(library,
                                               "getImplementation")) != NULL)
        return (implementation = getImplementation()) != NULL;

    return false;
}

/* search for shared objects in the drivers directory and use the
   first that will load. */
bool findDriver()
{
    char path[PATH_MAX];
    DIR *dir = NULL;
    struct dirent *dent;

    if (findDriversDir(path) &&
        (dir = opendir(path)) != NULL)
        while((dent = readdir(dir)) != NULL)
        {
            char soPath[PATH_MAX];
            strcpy(soPath, path);
            strcat(soPath, dent->d_name);
            if (loadDriver(soPath))
            {
                message(LOG_INFO, "Loaded driver: %s\n", soPath);
                return true;
            }
        }
    return false;
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
