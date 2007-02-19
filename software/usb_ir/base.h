#ifndef _BASE_
#define _BASE_

#ifdef WIN32
  typedef int bool;
  enum
  {
      false,
      true
  };

  typedef unsigned char u_int8_t; 

  #include <winsock.h>

  #define PIPE_PTR HANDLE
  #define getpid GetCurrentProcessId
#else
  #include <stdbool.h>
  #include <stdint.h>
  #include <unistd.h>
  #include <pthread.h>

  #define PIPE_PTR int
#endif

#endif
