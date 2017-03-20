#include "../logging.h"

#include <stdlib.h>
#include <string.h>

static loggingImpl logImpl = {NULL,NULL,NULL};

bool wouldOutput(int level)
{
    if (logImpl.wouldOutput != NULL)
        return logImpl.wouldOutput(level);
    return true;
}

int message(int level, char *format, ...)
{
    int retval = 0;
    if (logImpl.vaMessage != NULL)
    {
        va_list list;
        va_start(list, format);
        retval = logImpl.vaMessage(level, format, list);
        va_end(list);
    }
    return retval;
}

void appendHex(int level, void *location, unsigned int length)
{
    if (logImpl.appendHex != NULL)
        logImpl.appendHex(level, location, length);
}

void setLoggingPtrs(const loggingImpl *impl)
{
    if (impl == NULL)
        memset(&logImpl, 0, sizeof(loggingImpl));
    else
        logImpl = *impl;
}
