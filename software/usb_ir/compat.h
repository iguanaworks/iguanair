/****************************************************************************
 ** compat.h ****************************************************************
 ****************************************************************************
 *
 * Basic includes and definitions to make this code work across
 * various platforms.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */


#ifndef _COMPAT_
#define _COMPAT_

#include "config.h"
#include "iguanaIR-export.h"

#ifdef WIN32
    /* The old functions CAN be used safely... stop the warnings. */
    #pragma warning( disable : 4996 )

    typedef int bool;
    #define PIPE_PTR HANDLE
    #define INVALID_PIPE NULL

    typedef unsigned char uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned int uint32_t;
    typedef unsigned long long uint64_t;

    /* taken from libusb-win32/src/error.h */
    #define ETIMEDOUT 116

    /* must be at least 0x0500 to get HDEVNOTIFY */
    #ifdef _WIN32_WINNT
        #undef _WIN32_WINNT
    #endif
    #define _WIN32_WINNT 0x0500
    #include <windows.h>

    #define getpid GetCurrentProcessId
    #define setlinebuf(a)
    #define snprintf _snprintf

    /* thread defines */
    #define THREAD_PTR HANDLE
    #define INVALID_THREAD_PTR NULL
    IGUANAIR_API bool startThread(THREAD_PTR *handle, void* (*target)(void*), void *arg);
    IGUANAIR_API void joinThread(THREAD_PTR *handle, void **exitVal);
    #define CURRENT_THREAD_PTR OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId())

    /* lock defines */
    #define LOCK_PTR CRITICAL_SECTION

    /* windows has no way to flag specific variables as unused */
    #define UNUSED(a) a

#else
    #include <stdint.h>
    #include <unistd.h>
    /* need __USE_GNU to get pthread_yield */
    #define __USE_GNU
    #include <pthread.h>
    #undef __USE_GNU

    /* thread defines */
    #define THREAD_PTR pthread_t
    #define INVALID_THREAD_PTR 0
    #define startThread(a, b, c) (pthread_create((a), NULL, (b), (c)) == 0)
    #define joinThread(a,b) (void)pthread_join((a), (b))
    #define CURRENT_THREAD_PTR pthread_self()

    #if __APPLE__
        #define SwitchToThread() pthread_yield_np()
    #else
        #define SwitchToThread() pthread_yield()
    #endif

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

#endif

IGUANAIR_API uint64_t microsSinceX();

#endif
