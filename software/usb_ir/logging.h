#pragma once

#include "iguanaIR.h"
#include <stdarg.h>

enum
{
    /* message levels */
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARN,
    LOG_NORMAL,
    LOG_INFO,
    LOG_DEBUG,
    LOG_DEBUG2,
    LOG_DEBUG3,

    LOG_ALWAYS,

    /* argp related logging defines */
    ARG_LOG_LEVEL = 0x100,
    LOG_GROUP = 1,
    MSC_GROUP
};

/* for passing to drivers */
typedef struct loggingImpl
{
    bool (*wouldOutput)(int level);
    int (*vaMessage)(int level, char *format, va_list list);
    void (*appendHex)(int level, void *location, unsigned int length);
} loggingImpl;

/* functions for configuring logging */
void initLogSystem(const loggingImpl *impl);
loggingImpl* logImplementation();
struct argp* logArgParser();
void changeLogLevel(int difference);
void setLogLevel(int value);
void openLog(const char *filename);

/* fuctions for outputting log lines */
bool wouldOutput(int level);
int message(int level, char *format, ...);
void appendHex(int level, void *location, unsigned int length);

/* internal to the logging subsystem */
int vaMessage(int level, char *format, va_list list);
