/****************************************************************************
 ** direct.h ****************************************************************
 ****************************************************************************
 *
 * Export header for the library that controls hardware communication.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the LGPL version 2.1.
 * See LICENSE-LGPL for license details.
 */

#pragma once

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
