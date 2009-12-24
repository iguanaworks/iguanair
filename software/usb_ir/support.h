/****************************************************************************
 ** support.h ***************************************************************
 ****************************************************************************
 *
 * Basic supporting functions needed by the Iguanaworks tools.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */
#ifndef _SUPPORT_H_
#define _SUPPORT_H_

#ifdef SUPPORT_INCLUDE
    #define SUPPORT_API __declspec(dllexport)
#elif WIN32
    #ifdef SUPPORT_EXPORTS
        #define SUPPORT_API __declspec(dllexport)
    #else
        #define SUPPORT_API __declspec(dllimport)
    #endif
#else
    #ifdef SUPPORT_EXPORTS
        #define SUPPORT_API __attribute__((visibility("default")))
    #else
        #define SUPPORT_API
    #endif
#endif

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

    /* other constants */
    MAX_LINE = 1024,
    CTL_INDEX = 0xFF,

    /* for use with readPipe */
    READ  = 0,
    WRITE = 1
};

/* functions for messages (logging) */
void changeLogLevel(int difference);
void setLogLevel(int value);
void openLog(const char *filename);

/* exported by the igdaemon executable to allow dlopen'd drivers to
   link to these */
SUPPORT_API bool wouldOutput(int level);
SUPPORT_API int message(int level, char *format, ...);
SUPPORT_API void appendHex(int level, void *location, unsigned int length);

/* used during shutdown to clean up threads */
void setParentPipe(PIPE_PTR pp);
void makeParentJoin();

#endif
