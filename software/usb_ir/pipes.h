#ifndef _PIPES_
#define _PIPES_

#ifdef WIN32
  #define PIPE_PTR HANDLE
  #define INVALID_PIPE NULL

  bool createPipePair(PIPE_PTR *pair);
  int readPipe(PIPE_PTR fd, void *buf, size_t count);
  int writePipe(PIPE_PTR fd, const void *buf, size_t count);
  int closePipe(PIPE_PTR fd);
  PIPE_PTR acceptClient(PIPE_PTR *server);
#else
  #define PIPE_PTR int
  #define INVALID_PIPE -1

  #include <sys/un.h>
  #include <sys/socket.h>

  #define createPipePair pipe
  #define readPipe read
  #define writePipe write
  #define closePipe close
  #define acceptClient(a) accept((a), NULL, NULL);
#endif

/* functions dealing with the server sockets */
void socketName(const char *name, char *buffer, unsigned int length);
PIPE_PTR startListening(const char *name, const char *alias);
void stopListening(PIPE_PTR fd, const char *name, const char *alias);
PIPE_PTR connectToPipe(const char *name);

int readBytes(PIPE_PTR fd, int timeout,
              char *buffer, int size);

/* used for notification of packet arrival */
int notified(PIPE_PTR fd, int timeout);
bool notify(PIPE_PTR fd);

typedef struct fdSets
{
    int max;
    fd_set next;
    fd_set in, err;
} fdSets;

int checkFD(PIPE_PTR fd, fdSets *fds);

#endif
