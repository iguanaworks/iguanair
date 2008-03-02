#ifndef _IGUANAIR_EXPORT_
#define _IGUANAIR_EXPORT_

#ifdef WIN32
    #ifdef IGUANAIR_EXPORTS
        #define IGUANAIR_API __declspec(dllexport)
    #else
        #define IGUANAIR_API __declspec(dllimport)
    #endif
#else
    #ifdef IGUANAIR_EXPORTS
        #define IGUANAIR_API __attribute__((visibility("default")))
    #else
        #define IGUANAIR_API
    #endif
#endif

#endif
