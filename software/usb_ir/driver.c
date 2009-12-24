#include "iguanaIR.h"
#include "compat.h"

#include "stdio.h"
#include "string.h"
#include "limits.h"
#include "sys/types.h"

#include "support.h"
#include "driverapi.h"

/* will hold driver-supplied function pointers */
static driverImpl *implementation = NULL;

#ifdef _WIN32
bool findDriverDir(char *path)
{
    HMODULE hModule = GetModuleHandle("iguanaIR.dll");
    if (hModule != NULL &&
        GetModuleFileName(hModule, path, PATH_MAX) > 0)
    {
        strrchr(path, PATH_SEP)[0] = '\0';
        return true;
    }
    return false;
}

#else
bool findDriverDir(char *path)
{
    void *library;
    unsigned long start, target, end;
    FILE *maps;
    char buffer[256], *object;

    library = loadLibrary("libiguanaIR.so");
    target = (unsigned long)dlsym(library, "iguanaClose");

    maps = fopen("/proc/self/maps", "r");
    while(fgets(buffer, 256, maps) != NULL)
        if (sscanf(buffer, "%lx-%lx", &start, &end) == 2 &&
            start < target && target < end)
        {
            object = strrchr(buffer, ' ') + 1;
            strrchr(buffer, '/')[1] = '\0';
            strcpy(path, object);
            strcat(path, "iguanaIR");
            return true;
        }

    return false;
}
#endif

bool loadDriver(char *path)
{
    char *ext;
    void *library;
    driverImpl* (*getImplementation)();

    ext = strrchr(path, '.');
    if (ext != NULL &&
        strcmp(ext, DYNLIB_EXT) == 0 &&
        (library = loadLibrary(path)) != NULL &&
        (*(void**)(&getImplementation) = getFuncAddress(library,
                                                        "getImplementation")) != NULL)
        return (implementation = getImplementation()) != NULL;

    return false;
}

bool checkDriver(const char *root, const char *name)
{
    bool retval = false;
    char driver[PATH_MAX];

    /* combine the root with the name */
#ifdef WIN32
    if (name[1] == ':')
#else
    if (name[0] == '/')
#endif
        strcpy(driver, name);
    else
    {
        strcpy(driver, root);
        /* make sure we append a / (or \) if need be */
        if (driver[strlen(driver) - 1] != PATH_SEP)
        {
            char sep[2] = { PATH_SEP, '\0' };
            strcat(driver, sep);
        }
        strcat(driver, name);
    }

    /* attempt to load the driver */
    if (loadDriver(driver))
        retval = true;
    else if (strrchr(name, '.')  == NULL &&
             strrchr(name, PATH_SEP)  == NULL)
    {
        strcat(driver, DYNLIB_EXT);
        retval = loadDriver(driver);
    }

    if (retval)
        message(LOG_INFO, "Loaded driver: %s\n", driver);
    return retval;
}

/* search for shared objects in the drivers directory and use the
   first that will load. */
bool findDriver(const char *path, const char **preferred, bool onlyPreferred)
{
    DIR_HANDLE dir = NULL;
    char buffer[PATH_MAX];
    int x;

    /* check through the preferred list */
    if (preferred != NULL)
        for(x = 0; preferred[x] != NULL; x++)
            if (checkDriver(path, preferred[x]))
                return true;

    /* check through all files in the path if allowed */
    strcpy(buffer, path);
    while(! onlyPreferred &&
          (dir = findNextFile(dir, buffer)) != NULL)
        if (checkDriver(path, buffer))
            return true;

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
