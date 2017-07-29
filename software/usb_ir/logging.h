#pragma once

#include "iguanaIR.h"
#include <stdarg.h>
#include <stdio.h>

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

    /* argp related logging defines */
    LOG_GROUP = 1,
    MSC_GROUP
};

typedef struct logSettings
{
    int level;
    FILE *log;
} logSettings;

#define INIT_LOG_SETTINGS { LOG_NORMAL, NULL }

/* functions for configuring logging */
void initializeLogging(logSettings *globalSsettings);
logSettings* currentLogSettings();
struct argp* logArgParser();

/* fuctions for outputting log lines */
bool wouldOutput(int level);
int message(int level, char *format, ...);
void appendHex(int level, void *location, unsigned int length);
