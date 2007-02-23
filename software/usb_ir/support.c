/****************************************************************************
 ** support.c ***************************************************************
 ****************************************************************************
 *
 * Basic functions and definitions for I/O, both for logging and
 * device communication.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#include "base.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#include "pipes.h"
#include "support.h"
#include "iguanaIR.h"

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
static FILE *log = NULL;

/* Exit the application with proper cleanup. */
void dieCleanly(int level)
{
    /* exit with appropriate value */
    if (level == LOG_FATAL)
        exit(1);
    exit(0);
}

void changeLogLevel(int difference)
{
    currentLevel += difference;
    if (currentLevel < LOG_FATAL)
        currentLevel = LOG_FATAL;
}

void openLog(const char *filename)
{
    if (log != NULL)
        fclose(log);
    log = NULL;

    if (strcmp(filename, "-") != 0)
    {
        log = fopen(filename, "a");
        if (log != NULL)
            setlinebuf(log);
    }
}

static FILE* pickStream(int level)
{
    FILE *out = NULL;

    if (level <= currentLevel ||
        level == LOG_ALWAYS)
    {
        /* if logfile is open print to it instead */
        if (log != NULL)
            out = log;
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
            char when[21];
            time_t now;
            struct tm *nowTm;

            /* figure out the timestamp */
            now = time(NULL);
            nowTm = localtime(&now);
            strftime(when, 21, "%b %d %H:%M:%S %Y", nowTm);
            when[20] = '\0';

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
        /* free the format if it ws allocated */
        if (buffer != format)
            free(buffer);
    }
    va_end(list);

    if (level <= LOG_FATAL)
        dieCleanly(level);

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
    }
}
