#include <stdlib.h>
#include <stdio.h>

#include "iguanaIR.h"

int main(int argc, char **argv)
{
    /* connect to a device, in this case the first device found by igdaemon */
    PIPE_PTR conn = iguanaConnect("0");
    if (conn == INVALID_PIPE)
        perror("iguanaConnect failed");
    else
    {
        iguanaPacket req, resp;

        /* check the version just to demonstrate the server/client protocol */
        req = iguanaCreateRequest(IG_DEV_GETVERSION, 0, NULL);
        if (! iguanaWriteRequest(req, conn))
            perror("iguanaWriteRequest failed");
        else
        {
            /* wait up to 1000 milliseconds for a response */
            resp = iguanaReadResponse(conn, 1000);
            if (iguanaResponseIsError(resp))
                perror("iguanaReadResponse errored");
            else
            {
                /* the get version request returns 2 version bytes */
                unsigned int len;
                unsigned char *buffer = iguanaRemoveData(resp, &len);
                printf("Firmware version 0x%02x\n", *((short*)buffer));
                free(buffer);
            }
            iguanaFreePacket(resp);
        }
        iguanaFreePacket(req);

        /* the sendable data should be an unsigned int array
           containing the pulse/space data with the pulses OR'd with
           IG_PULSE_BIT.  I believe it must start and end with a pulse
           as well.  All lengths are in microseconds. */
        unsigned int buffer[] = {
            8000 | IG_PULSE_BIT,
            1000,
            8000 | IG_PULSE_BIT,
            100,
            800 | IG_PULSE_BIT,
            100,
            800 | IG_PULSE_BIT,
            100,
            8000 | IG_PULSE_BIT,
            1000,
            8000 | IG_PULSE_BIT,
        };

        req = iguanaCreateRequest(IG_DEV_SEND,
                                  sizeof(unsigned int) * 11, buffer);
        if (! iguanaWriteRequest(req, conn))
            perror("iguanaWriteRequest failed");
        else
        {
            resp = iguanaReadResponse(conn, 1000);
            if (iguanaResponseIsError(resp))
                perror("iguanaReadResponse errored");
            else
                /* send just gives back success or failure */
                printf("Send successful.\n");
            iguanaFreePacket(resp);
        }

        /* because we did not dynamically allocate the buffer we need
           to remove it before freeing the packet it was added to. */
        iguanaRemoveData(req, NULL);
        iguanaFreePacket(req);
    }

    return 0;
}

/*
Expected output:

[jdunn@margarita usb_ir]$ ./example
Firmware version 0x306
Send successful.


igdaemon -nvvv output:

Jul 19 09:59:52 2011 INFO: Found client using protocol version 1
Jul 19 09:59:52 2011 INFO: Request handled within daemon: 0xfe
Jul 19 09:59:52 2011 DEBUG2: o0x0000cd01
Jul 19 09:59:52 2011 DEBUG2: i0x0000dc010603
Jul 19 09:59:52 2011 DEBUG: Received ctl header: 0x1
Jul 19 09:59:52 2011 INFO: Transaction: 0x1 (11061 microseconds)
Jul 19 09:59:52 2011 DEBUG2: o0x0000cd1513000631
Jul 19 09:59:52 2011 DEBUG2: o0x7f7f32a67f7f3284
Jul 19 09:59:52 2011 DEBUG2: o0x1e841e847f7f32a6
Jul 19 09:59:52 2011 DEBUG2: o0x7f7f32
Jul 19 09:59:52 2011 DEBUG2: i0x0000dc15
Jul 19 09:59:52 2011 DEBUG: Received ctl header: 0x15
Jul 19 09:59:52 2011 INFO: Transaction: 0x15 (71557 microseconds)
*/
