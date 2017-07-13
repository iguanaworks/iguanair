#ifndef _DIRECT_H_
#define _DIRECT_H_

#ifdef WIN32
    #ifdef DIRECT_EXPORTS
        #define DIRECT_API __declspec(dllexport)
    #else
        #define DIRECT_API __declspec(dllimport)
    #endif
#else
    #ifdef DIRECT_EXPORTS
        #define DIRECT_API __attribute__((visibility("default")))
    #else
        #define DIRECT_API
    #endif
#endif

#endif
