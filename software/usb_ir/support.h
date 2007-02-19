/****************************************************************************
 ** support.h ***************************************************************
 ****************************************************************************
 *
 * Basic supporting functions needed by the Iguanaworks tools.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#ifndef _SUPPORT_H_
#define _SUPPORT_H_

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
void dieCleanly(int level);
void changeLogLevel(int difference);
void openLog(const char *filename);
bool wouldOutput(int level);
int message(int level, char *format, ...);
void appendHex(int level, void *location, unsigned int length);

/* functions dealing with the unix sockets */
void socketName(const char *name, char *buffer, unsigned int length);
int startListening(const char *name, const char *alias);
void stopListening(int fd, const char *name, const char *alias);

int readBytes(int fd, int timeout,
              char *buffer, int size);

/* used for notification of packet arrival */
int notified(int fd, int timeout);
bool notify(int fd);

typedef struct fdSets
{
    int max;
    fd_set next;
    fd_set in, err;
} fdSets;

int checkFD(int fd, fdSets *fds);

#endif
