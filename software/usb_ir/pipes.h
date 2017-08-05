/****************************************************************************
 ** pipes.h *****************************************************************
 ****************************************************************************
 *
 * Header for the Unix/Named pipe interfaces.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */

#pragma once

enum
{
    /* pipe pair indices */
    READ  = 0,
    WRITE = 1,

    WAIT_FOREVER = 0x7FFFFFFF
};

#ifdef WIN32
  bool createPipePair(PIPE_PTR *pair);
  int readPipe(PIPE_PTR fd, void *buf, int count);
  int writePipe(PIPE_PTR fd, const void *buf, int count);
  #define closePipe CloseHandle
  PIPE_PTR acceptClient(PIPE_PTR *server);

#else
  #include <sys/socket.h>
  #include <sys/un.h>

  #define createPipePair(a) (pipe(a) == 0)
  #define readPipe read
  #define writePipe write
  #define closePipe close
  #define acceptClient(a) accept((a), NULL, NULL);
#endif

/* functions managing server sockets */
PIPE_PTR createServerPipe(const char *name, char **addrStr);
void closeServerPipe(PIPE_PTR fd, const char *name);
void setAlias(const char *target, bool deleteAll, const char *alias);

void socketName(const char *name, char *buffer, unsigned int length);
PIPE_PTR connectToPipe(const char *name);

/* read/write with timeouts */
int readPipeTimed(PIPE_PTR fd, void *buffer, int size, int timeout);
int writePipeTimed(PIPE_PTR fd, const void *buffer, int size, int timeout);

/* used for notification of packet arrival */
int notified(PIPE_PTR fd, int timeout);
bool notify(PIPE_PTR fd);
