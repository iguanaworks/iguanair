#ifndef _SEND_FORMAT_H_
#define _SEND_FORMAT_H_

#include "direct.h"
#include <stdint.h>

enum
{
    /* highest value of a data byte in the IR code protocol */
    MAX_DATA_BYTE = 127,

    /* we support version 0 and 1, but ver 2 is not implemented */
    COMPRESS_VER0 = 0,
    COMPRESS_VER1,
    COMPRESS_VER2
};

DIRECT_API int pulsesToIguanaSend(int carrier,
                                  uint32_t *sendCode, int length,
                                  unsigned char **results, int compress);

#endif
