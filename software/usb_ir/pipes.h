#ifndef _PIPES_
#define _PIPES_

#ifdef WIN32
  int createPipePair(PIPE_PTR *pair);
  int readPipe(PIPE_PTR fd, void *buf, size_t count);
  int writePipe(PIPE_PTR fd, const void *buf, size_t count);
  int closePipe(PIPE_PTR);
#else
  #include <sys/un.h>
  #include <sys/socket.h>

  #define createPipePair pipe
  #define readPipe read
  #define writePipe write
  #define closePipe close
#endif

#endif
