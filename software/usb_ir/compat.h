/****************************************************************************
 ** compat.h ****************************************************************
 ****************************************************************************
 *
 * Basic includes and definitions to make this code work across
 * various platforms.
 *
 * Copyright (C) 2009, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */

#ifndef _COMPAT_
#define _COMPAT_

#include "config.h"

#ifdef WIN32
    /* The old functions CAN be used safely... stop the warnings. */
    #pragma warning( disable : 4996 )

    /* must be at least 0x0500 to get HDEVNOTIFY */
    #ifdef _WIN32_WINNT
        #undef _WIN32_WINNT
    #endif
    #define _WIN32_WINNT 0x0500
    #include <windows.h>

    typedef unsigned char uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned int uint32_t;
    typedef unsigned long long uint64_t;

    /* taken from libusb-win32/src/error.h */
    #define USB_ETIMEDOUT 116
    #define ETIMEDOUT ERROR_TIMEOUT

    #define getpid GetCurrentProcessId
    #define setlinebuf(a)
    #define snprintf _snprintf

    /* thread defines */
    #define THREAD_PTR HANDLE
    #define INVALID_THREAD_PTR NULL
    bool startThread(THREAD_PTR *handle, void* (*target)(void*), void *arg);
    void joinThread(THREAD_PTR *handle, void **exitVal);
    #define CURRENT_THREAD_PTR OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId())

    /* lock defines */
    #define LOCK_PTR CRITICAL_SECTION

    /* windows has no way to flag specific variables as unused */
    #define UNUSED(a) a

    /* defines for dealing with the file system and libraries */
    #define PATH_MAX MAX_PATH
    #define PATH_SEP '\\'
    #define DYNLIB_EXT ".dll"
    typedef HANDLE DIR_HANDLE;

    /* functions to dynamically load drivers */
    #define loadLibrary(name) LoadLibrary(name)
    #define getFuncAddress    GetProcAddress

#else
    #include <stdint.h>
    #include <unistd.h>
    /* need __USE_GNU to get pthread_yield */
    #define __USE_GNU
    #include <pthread.h>
    #undef __USE_GNU
    #include <dirent.h>
    #include <dlfcn.h>

    /* because it's different in windows */
    #define USB_ETIMEDOUT ETIMEDOUT
    #define Sleep(a) usleep((a) * 1000)

    /* thread defines */
    #define THREAD_PTR pthread_t
    #define INVALID_THREAD_PTR 0
    #define startThread(a, b, c) (pthread_create((a), NULL, (b), (c)) == 0)
    #define joinThread(a,b) (void)pthread_join((a), (b))
    #define CURRENT_THREAD_PTR pthread_self()

    /* lock defines */
    #define LOCK_PTR pthread_mutex_t
    #define InitializeCriticalSection(a) pthread_mutex_init((a), NULL)
    #define EnterCriticalSection pthread_mutex_lock
    #define LeaveCriticalSection pthread_mutex_unlock

    /* gcc 3.3 has problems with __attribute__ ((unused)) on variables */
    #if (__GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 4))
        #define UNUSED(a) a
    #else
        #define UNUSED(a) a __attribute__((unused))
    #endif

    /* defines for dealing with the file system and libraries */
    #define PATH_SEP '/'
    #if __APPLE__
        #define SwitchToThread() pthread_yield_np()
        #define DYNLIB_EXT ".dylib"
    #else
        #define SwitchToThread() pthread_yield()
        #define DYNLIB_EXT ".so"
    #endif
    typedef DIR* DIR_HANDLE;

    /* functions to dynamically load drivers */
    #define loadLibrary(name) dlopen((name), RTLD_LAZY | RTLD_LOCAL)
    #define getFuncAddress    dlsym

#endif

/* a few functions must be implemented in each OS */
uint64_t microsSinceX();
bool setNonBlocking(PIPE_PTR pipe);
char* translateError(int errnum);
DIR_HANDLE findNextFile(DIR_HANDLE hFind, char *buffer);

#endif
