/****************************************************************************
 ** support.c ***************************************************************
 ****************************************************************************
 *
 * Basic functions and definitions for I/O, both for logging and
 * device communication.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */
#include "iguanaIR.h"
#include "compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#include "support.h"

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
static int currentLevel = LOG_NORMAL;
static FILE *logFile = NULL;

void changeLogLevel(int difference)
{
    currentLevel += difference;
    if (currentLevel < LOG_FATAL)
        currentLevel = LOG_FATAL;
}

void setLogLevel(int value)
{
    currentLevel = LOG_NORMAL;
    changeLogLevel(value);
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

bool wouldOutput(int level)
{
    return pickStream(level) != NULL;
}

/* Print a message to a certain debug level */
int message(int level, char *format, ...)
{
    va_list list;
    int retval = 0;
    FILE *out;

    va_start(list, format);
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
                perror("FATAL: message format malloc failedx");
                exit(2);
            }
            sprintf(buffer, "%s%s%s", when, msgPrefixes[level], format);
        }
        else
            buffer = format;

        retval = vfprintf(out, buffer, list);
        /* flushing the log file after each write */
        if (out == logFile)
            fflush(logFile);
        /* free the format if it ws allocated */
        if (buffer != format)
            free(buffer);
    }
    va_end(list);

    if (level <= LOG_FATAL)
        exit(1);

    return retval;
}

void appendHex(int level, void *location, unsigned int length)
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

/* TODO: need to get this out of the library */
#include "pipes.h"

static PIPE_PTR parentPipe;

void setParentPipe(PIPE_PTR pp)
{
    parentPipe = pp;
}

void makeParentJoin()
{
    THREAD_PTR thread;
    thread = CURRENT_THREAD_PTR;
    writePipe(parentPipe, &thread, sizeof(THREAD_PTR));
}
