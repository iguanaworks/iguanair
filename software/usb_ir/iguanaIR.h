/****************************************************************************
 ** iguanaIR.h **************************************************************
 ****************************************************************************
 *
 * Declaration of the client interface to the igdaemon.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#ifndef _IGUANA_IR_
#define _IGUANA_IR_

/* the protocol between the client and server must be versioned */
#define IG_PROTOCOL_VERSION 1

/* hate including headers from headers, but we need a bool */
#ifdef WIN32
    typedef int bool;
    enum
    {
        false,
        true
    };

    #ifdef IGUANAIR_EXPORTS
        #define IGUANAIR_API __declspec(dllexport)
    #else
        #define IGUANAIR_API __declspec(dllimport)
    #endif

    #define PIPE_PTR HANDLE
    #define INVALID_PIPE NULL
#else
    #include "stdbool.h"

    #ifdef IGUANAIR_EXPORTS
        #define IGUANAIR_API __attribute__((visibility("default")))
    #else
        #define IGUANAIR_API
    #endif

    #define PIPE_PTR int
    #define INVALID_PIPE -1
#endif

/* NOTE: all IR timings will be in microseconds and packed in uint32_t
 * arrays */

enum
{
    /* Let the client and daemon check their versions.  should be the
       first thing sent by the client. */
    IG_EXCH_VERSIONS = 0xFE,

    /* used in response packets */
    IG_DEV_ERROR      = 0x00,

    /* FILE:loader.inc bootloader functions */
    IG_DEV_GETVERSION  = 0x01,
    IG_DEV_WRITEBLOCK  = 0x02,
    IG_DEV_CHECKSUM    = 0x03,
    IG_DEV_INVALID_ARG = 0x04,
    IG_DEV_RESET       = 0xFF,

    /* FILE:body.inc standard "body" functions */
    IG_DEV_GETFEATURES  = 0x10,
    IG_DEV_GETBUFSIZE   = 0x11,
    IG_DEV_RECVON       = 0x12,
    IG_DEV_RAWRECVON    = 0x13,
    IG_DEV_RECVOFF      = 0x14,
    IG_DEV_SEND         = 0x15,
    IG_DEV_GETCHANNELS  = 0x16,
    IG_DEV_SETCHANNELS  = 0x17,
    IG_DEV_GETPINCONFIG = 0x18,
    IG_DEV_SETPINCONFIG = 0x19,
    IG_DEV_GETPINS      = 0x1A,
    IG_DEV_SETPINS      = 0x1B,
    IG_DEV_BULKPINS     = 0x1C,
    IG_DEV_EXECUTE      = 0x1D,
    IG_DEV_GETID        = 0x1E,

    /* FILE:body.inc packets initiated by the device */
    IG_DEV_RECV         = 0x30,
    IG_DEV_OVERRECV     = 0x31,
    IG_DEV_OVERSEND     = 0x32,

    /* for interpretting codes */
    IG_PULSE_BIT  = 0x01000000,
    IG_PULSE_MASK = 0x00FFFFFF,

    /* to handle raw signal data */
    IG_RAWSPACE_BIT  = 0x80,
    IG_RAWSPACE_MASK = 0x7F,

    /* a couple protocol enums are needed by client.c */
    IG_CTL_START      = 0x0000,
    IG_CTL_FROMDEV    = 0xDC,

    /* device feature flags, 0 means old style device */
    IG_HAS_LEDS    = 0x01,
    IG_HAS_BOTH    = 0x02,
    IG_HAS_SOCKETS = 0x04,
    IG_HAS_LCD     = 0x08,

    /* used to include support for old protocol */
    IG_DEV_GETCONFIG0 = 0x07,
    IG_DEV_SETCONFIG0 = 0x08,
    IG_DEV_GETCONFIG1 = 0x09,
    IG_DEV_SETCONFIG1 = 0x0A
};

/* manage a connection to the server */
#define iguanaConnect(a) iguanaConnect_real(a, IG_PROTOCOL_VERSION)
IGUANAIR_API PIPE_PTR iguanaConnect_real(const char *name,
                                         unsigned int protocol);
IGUANAIR_API void iguanaClose(PIPE_PTR connection);

/* requests and responses are represented by opaque handles, so
 * functions are needed to access these objects */
typedef void* iguanaPacket;
IGUANAIR_API iguanaPacket iguanaCreateRequest(unsigned char code,
                                              unsigned int dataLength,
                                              void *data);
IGUANAIR_API unsigned char* iguanaRemoveData(iguanaPacket pkt,
                                             unsigned int *dataLength);
IGUANAIR_API unsigned char iguanaCode(const iguanaPacket pkt);
IGUANAIR_API void iguanaFreePacket(iguanaPacket pkt);

/* for communication with the server */
IGUANAIR_API bool iguanaWriteRequest(const iguanaPacket request,
                                     PIPE_PTR connection);
IGUANAIR_API iguanaPacket iguanaReadResponse(PIPE_PTR connection,
                                             unsigned int timeout);
IGUANAIR_API bool iguanaResponseIsError(const iguanaPacket response);

/* a few helper functions for dealing with function arguments */
IGUANAIR_API int iguanaReadPulseFile(const char *filename, void **pulses);
IGUANAIR_API int iguanaReadBlockFile(const char *filename, void **data);
IGUANAIR_API int iguanaPinSpecToData(unsigned int value, void **data);
IGUANAIR_API unsigned char iguanaDataToPinSpec(const void *data);

/* The following enum is for configuring various settings for GPIO
 * pins.  An explanation of each value, from low to high bit, is
 * below (all default to 0):
 *
 * bit: name:          notes
 *  0   output enable 
 *  1   pullup enable
 *  2   open drain
 *  3   high sink      only pins 0 and 1, ignored elsewhere
 *  4   ttl threshold  0 = CMOS, 1 = TTL
 *  5   unused
 *  6   unused
 *  7   unused
*/
enum
{
    IG_OUTPUT     = 1,
    IG_PULLUP     = 2,
    IG_OPEN_DRAIN = 4,
    IG_HIGH_SINK  = 8,
    IG_THRESHOLD  = 16,
    /* unused */
    /* unused */
    /* unused */

    /* number of GPIO pins on our device */
    IG_PIN_COUNT = 8
};

#endif
