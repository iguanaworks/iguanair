/****************************************************************************
 ** sendFormat.c ************************************************************
 ****************************************************************************
 *
 * Implementation of functions used to prepare data for sending.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include "sendFormat.h"
#include "iguanaIR.h"
#include "compat.h"
#include "device-interface.h"

#include <string.h>
#include <stdlib.h>

#define DEBUG_TRANSMIT_BUFFER 0

int pulsesToIguanaSend(int carrier,
                       uint32_t *sendCode, int length,
                       unsigned char **results, int compress)
{
    int x, codeLength = 0, inSpace = 0;
    uint32_t lastCycles = 0;

    /* prepare/clear the output buffer */
    if (results != NULL)
        *results = NULL;

    /* convert each pulse */
    for(x = 0; x < length; x++)
    {
        uint32_t cycles, numBytes;

/* occasionally useful for debugging transmission issues */
#if DEBUG_TRANSMIT_BUFFER
        fprintf(stderr, "%3d ", x);
        if (x % 2)
            fprintf(stderr, "space ");
        else
            fprintf(stderr, "pulse ");
        fprintf(stderr, "%5d ", sendCode[x] & IG_PULSE_MASK);
#endif

        cycles = (uint32_t)((sendCode[x] & IG_PULSE_MASK) /
                            1000000.0 * carrier + 0.5);
        numBytes = (cycles / MAX_DATA_BYTE) + 1;
        cycles %= MAX_DATA_BYTE;
        if (cycles == 0)
        {
            cycles = MAX_DATA_BYTE;
            numBytes -= 1;
        }

        if (compress == COMPRESS_VER1 && inSpace == 0)
        {
            if (cycles != MAX_DATA_BYTE &&
                cycles == lastCycles &&
                x + 1 < length)
                numBytes = 0;
            lastCycles = cycles;
        }

#if DEBUG_TRANSMIT_BUFFER
        fprintf(stderr, " cycles=%3d numBytes=%3d ", cycles, numBytes);
#endif

        if (numBytes)
        {
            if (inSpace)
                cycles |= STATE_MASK;

            /* store the codes to return to the user if requested */
            if (results != NULL)
            {
                /* allocate space as we go */
                *results = realloc(*results,
                                   sizeof(char) * (codeLength + numBytes));

                /* populate the buffer with max bytes */
                memset(*results + codeLength,
                       LENGTH_MASK | (inSpace * STATE_MASK),
                       numBytes - 1);

                /* store the last byte
                   (cast is alright due to %= MAX_DATA_BYTE) */
                (*results)[codeLength + numBytes - 1] = (unsigned char)cycles;
            }

#if DEBUG_TRANSMIT_BUFFER
            fprintf(stderr, " buf(");
            for(k=0; k<numBytes; k++)
                fprintf(stderr, "%3d ",codes[codeLength+k]);
            fprintf(stderr, ")");
#endif

            /* sum up the total bytes */
            codeLength += numBytes;
        }

#if DEBUG_TRANSMIT_BUFFER
        fprintf(stderr, "\n");
#endif
        inSpace ^= 1;
    }

    return codeLength;
}

