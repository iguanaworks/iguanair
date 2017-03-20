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
static bool findDriverDir(char *path)
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

#elif __APPLE__
#include <mach-o/dyld.h>

static bool findDriverDir(char *path)
{
    uint32_t size = PATH_MAX;
    /* TODO: because this call does not give an absolute path it could
       be longer than PATH_MAX so we should call again on failure w a
       dynamically allocated buffer */
    if (_NSGetExecutablePath(path, &size) == 0)
    {
        char *slash = strrchr(path, '/');
        if (slash != NULL)
        {
            slash[0] = '\0';
            strcat(path, "/drivers");
            return true;
        }
    }
    return false;
}

#elif __FreeBSD__
#include <sys/user.h>
#include <stdlib.h>
#include <libutil.h>

static bool findDriverDir(char *path)
{
    bool retval = false;
    void *library;
    uintmax_t target;
    struct kinfo_vmentry *freep, *kve;
    int i, cnt;

    library = loadLibrary("libiguanaIR" DYNLIB_EXT);
    target = (uintmax_t)dlsym(library, "iguanaClose");

    freep = kinfo_getvmmap(getpid(), &cnt);
    if (freep != NULL)
    {
        for (i = 0; i < cnt; i++)
        {
            kve = &freep[i];
            if (kve->kve_path[0] != '\0' &&
                kve->kve_start <= target && target < kve->kve_end)
            {
                char *slash;
                slash = strrchr(kve->kve_path, '/');
                if (slash != NULL)
                {
                    strcpy(slash + 1, "iguanaIR");
                    strcpy(path, kve->kve_path);
                    retval = true;
                    break;
                }
            }
        }
        free(freep);
    }
    return retval;
}

#else /* on Linux use /proc/self/maps */
static bool findDriverDir(char *path)
{
    void *library;
    unsigned long start, target, end;
    FILE *maps;
    char buffer[80 + PATH_MAX], *object;

    library = loadLibrary("libiguanaIR" DYNLIB_EXT);
    target = (unsigned long)dlsym(library, "iguanaClose");

    maps = fopen("/proc/self/maps", "r");
    while(fgets(buffer, 80 + PATH_MAX, maps) != NULL)
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
    loggingImpl logImpl = { wouldOutput, vaMessage, appendHex };
    char *ext;
    void *library;
    driverImpl* (*getImplementation)();

    ext = strrchr(path, '.');
    if (ext != NULL &&
        strcmp(ext, DYNLIB_EXT) == 0 &&
        (library = loadLibrary(path)) != NULL &&
        (*(void**)(&getImplementation) = getFuncAddress(library,
                                                        "getImplementation")) != NULL)
        return (implementation = getImplementation(&logImpl)) != NULL;

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
    else if (root != NULL)
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
    else
        strcpy(driver, name);

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
bool findDriver(const char *driverDir, const char **preferred, bool onlyPreferred)
{
    int x;

    /* try to find the driver directory if none was provided */
    if (driverDir == NULL)
    {
        char expectedDir[PATH_MAX];
        if (findDriverDir(expectedDir))
            driverDir = expectedDir;
        else
            /* fall back on something reasonable per OS */
#ifdef _WIN32
            driverDir = ".";
#else
  #if __LP64__
        if (access("/usr/lib64", F_OK) == 0)
            driverDir = "/usr/lib64/iguanaIR";
        else
  #endif
            driverDir = "/usr/lib/iguanaIR";
#endif
    }
    message(LOG_DEBUG, "  drvDir: %s\n", driverDir);

    /* check through the preferred list */
    if (preferred != NULL)
        for(x = 0; preferred[x] != NULL; x++)
            if (checkDriver(driverDir, preferred[x]) || checkDriver(NULL, preferred[x]))
                return true;

    /* if allowed check through files in the driverDir */
    if (! onlyPreferred)
    {
        DIR_HANDLE dir = NULL;
        char buffer[PATH_MAX];

        strcpy(buffer, driverDir);
        while((dir = findNextFile(dir, buffer)) != NULL)
            if (checkDriver(driverDir, buffer))
                return true;
    }

    return false;
}

bool initializeDriver()
{
    return implementation->initializeDriver();
}

void cleanupDriver()
{
    if (implementation != NULL && implementation->cleanupDriver != NULL)
        implementation->cleanupDriver();
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

void claimDevices(deviceList *devList, bool claim, bool force)
{
    if (implementation->claimDevices != NULL)
        implementation->claimDevices(devList, claim, force);
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
