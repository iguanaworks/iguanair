/****************************************************************************
 ** logging.c ***************************************************************
 ****************************************************************************
 *
 * Basic functions and definitions for logging.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */
#include "logging.h"
#include "compat.h"
#include "version.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <argp.h>

static char *msgPrefixes[] =
{
    "FATAL: ",
    "ERROR: ",
    "WARNING: ",
    "", /* NORMAL gets no prefix */
    "INFO: ",
    "DEBUG: ",
    "DEBUG2: ",
    "DEBUG3: "
};

/* logging variables */
static loggingImpl logImpl = {NULL,NULL,NULL};
static int currentLevel = LOG_NORMAL;
static FILE *logFile = NULL;

static struct argp_option options[] =
{
    { NULL, 0, NULL, 0, "Logging options:", LOG_GROUP },
    { "log-file",    'l',           "FILE",   0, "Specify a log file (defaults to \"-\").", LOG_GROUP },
    { "quiet",       'q',           NULL,     0, "Reduce the verbosity.",                   LOG_GROUP },
    { "verbose",     'v',           NULL,     0, "Increase the verbosity.",                 LOG_GROUP },
    { "log-level",   ARG_LOG_LEVEL, "NUM",    0, "Set the verbosity directly.",             LOG_GROUP },

    /* argp seems to recognize this and move it to the end */
    { "version",     'V',           NULL,     0, "Print the build and version numbers.",    MSC_GROUP },

    {0}
};

static error_t parseOption(int key, char *arg, struct argp_state *state)
{
    switch(key)
    {
    /* Logging options */
    case 'l':
        openLog(arg);
        break;

    case 'q':
        changeLogLevel(-1);
        break;

    case 'v':
        changeLogLevel(+1);
        break;

    case ARG_LOG_LEVEL:
    {
        char *end;
        long int res = strtol(arg, &end, 0);
        if (arg[0] == '\0' || end[0] != '\0' || res < LOG_FATAL || res > LOG_DEBUG3 )
        {
            fprintf(stderr, "Log level requires a numeric argument between %d and %d\n",
                    LOG_FATAL, LOG_DEBUG3);
            return ARGP_HELP_STD_ERR;
        }
        else
            setLogLevel(res);
        break;
    }

    case 'V':
        printf("Software version: %s\n", IGUANAIR_VER_STR("igdaemon"));
        exit(0);
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp parser = {
    options,
    parseOption
};

static FILE* pickStream(int level)
{
    FILE *out = NULL;

    if (level <= currentLevel ||
        level == LOG_ALWAYS)
    {
        /* if logfile is open print to it instead */
        if (logFile != NULL)
            out = logFile;
        else if (level <= LOG_WARN)
            out = stderr;
        else
            out = stdout;
    }

    return out;
}

static bool wouldOutput_internal(int level)
{
    return pickStream(level) != NULL;
}

/* do the actual work of printing */
static int vaMessage_internal(int level, char *format, va_list list)
{
    int retval = 0;
    FILE *out;

    out = pickStream(level);
    if (out != NULL)
    {
        char *buffer;
        if (level != LOG_ALWAYS && level != LOG_NORMAL)
        {
            char when[22];
            time_t now;
            struct tm *nowTm;

            /* figure out the timestamp */
            now = time(NULL);
            nowTm = localtime(&now);
            strftime(when, 22, "%b %d %H:%M:%S %Y ", nowTm);
            when[21] = '\0';

            buffer = (char*)malloc(strlen(when) + \
                                   strlen(msgPrefixes[level]) + \
                                   strlen(format) + 1);
            if (buffer == NULL)
            {
                perror("FATAL: message format malloc failed");
                return -ENOMEM;
            }
            sprintf(buffer, "%s%s%s", when, msgPrefixes[level], format);
        }
        else
            buffer = format;

#ifdef ANDROID
        retval = vprintf(buffer, list);
#else
        retval = vfprintf(out, buffer, list);
#endif
        /* flushing the log file after each write */
        if (out == logFile)
            fflush(logFile);
        /* free the format if it ws allocated */
        if (buffer != format)
            free(buffer);
    }

    /* die at callers request */
    assert(level > LOG_FATAL);

    return retval;
}

static void appendHex_internal(int level, void *location, unsigned int length)
{
    FILE *out;
    int retval = 0;

    out = pickStream(level);
    if (out != NULL)
    {
        unsigned int x;

        retval = fprintf(out, "0x");
        for(x = 0; x < length; x++)
            retval += fprintf(out, "%2.2x", ((unsigned char*)location)[x]);
        retval += fprintf(out, "\n");
        /* flushing the log file after each write */
        if (out == logFile)
            fflush(logFile);
    }
}

void initLogSystem(const loggingImpl *impl)
{
    if (impl == NULL)
    {
        static loggingImpl staticLogImpl = { wouldOutput_internal,
                                             vaMessage_internal,
                                             appendHex_internal };
        logImpl = staticLogImpl;
    }
    else
        logImpl = *impl;
}

loggingImpl* logImplementation()
{
    return &logImpl;
}

struct argp* logArgParser()
{
    return &parser;
}

void changeLogLevel(int difference)
{
    currentLevel += difference;
    if (currentLevel < LOG_FATAL)
        currentLevel = LOG_FATAL;
}

void setLogLevel(int value)
{
    changeLogLevel(value - currentLevel);
}

void openLog(const char *filename)
{
    if (logFile != NULL)
        fclose(logFile);
    logFile = NULL;

    if (strcmp(filename, "-") != 0)
    {
        logFile = fopen(filename, "a");
        if (logFile != NULL)
            setlinebuf(logFile);
    }
}

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
