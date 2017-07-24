/****************************************************************************
** hw_iguanaIR.c ***********************************************************
****************************************************************************
*
* routines for interfacing with Iguanaworks USB IR devices
*
* Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
*
* Distribute under GPL version 2.
*
*/

#include "lirc_driver.h"
#include "lirc/config.h"
#include "iguanaIR.h"

#include <errno.h>
#include <arpa/inet.h>
#if defined __APPLE__
  #include <sys/wait.h>
  #include <sys/ioctl.h>
#else
  #include <wait.h>
#endif

/* Uncomment the following line to change how we describe devices when
   enumerating
#define DESCRIBE_DEVICES
*/

static const logchannel_t logchannel = LOG_DRIVER;
static int sendConn = -1;
static pid_t child = 0;
static int recvDone = 0;
static int currentCarrier = -1;

static void quitHandler(int sig)
{
    recvDone = 1;
}

static int connectToIgdaemon(const char *device)
{
    if (strcmp(device, "auto") == 0)
        device = NULL;
    return iguanaConnect(device);
}

static bool daemonTransaction(int conn, unsigned char code, void *value, size_t size)
{
    bool retval = false;
    iguanaPacket request, response = NULL;

    request = iguanaCreateRequest(code, size, value);
    if (request != NULL)
    {
        if (iguanaWriteRequest(request, conn))
            response = iguanaReadResponse(conn, 10000);

        iguanaRemoveData(request, NULL);
        iguanaFreePacket(request);
    }

    /* handle success */
    if (! iguanaResponseIsError(response))
        retval = true;
    iguanaFreePacket(response);

    return retval;
}

static void recv_loop(int fd, int notify)
{
    int conn;

    alarm(0);
    signal(SIGTERM, quitHandler);
    /* signal(SIGPIPE, SIG_DFL); */
    signal(SIGINT, quitHandler);
    signal(SIGHUP, SIG_IGN);
    signal(SIGALRM, SIG_IGN);

    /* notify parent by closing notify */
    close(notify);

    conn = connectToIgdaemon(drv.device);
    if (conn != -1)
    {
        iguanaPacket response;
        lirc_t prevCode = -1;

        /* turn on the receiver */
        if (! daemonTransaction(conn, IG_DEV_RECVON, NULL, 0))
        {
            log_error("error when turning receiver on: %s\n", strerror(errno));
        }
        else
            while (!recvDone)
            {
                /* read from device */
                do
                    response = iguanaReadResponse(conn, 1000);
                while (!recvDone && ((response == NULL && errno == ETIMEDOUT) ||
                                     (iguanaResponseIsError(response) && errno == ETIMEDOUT)));

                if (iguanaResponseIsError(response))
                {
                    /* be quiet during exit */
                    if (!recvDone)
                    {
                        log_error("error response: %s\n", strerror(errno));
                    }
                    break;
                }
                else
                {
                    uint32_t* code;
                    unsigned int length, x, y = 0;
                    lirc_t buffer[8];       /* we read 8 bytes max at a time
                                             * from the device, i.e. packet
                                             * can only contain 8
                                             * signals. */

                    /* pull the data off the packet */
                    code = (uint32_t*)iguanaRemoveData(response, &length);
                    length /= sizeof(uint32_t);

                    /* translate the code into lirc_t pulses (and make
                     * sure they don't split across iguana packets. */
                    for (x = 0; x < length; x++)
                    {
                        if (prevCode == -1)
                        {
                            prevCode = (code[x] & IG_PULSE_MASK);
                            if (prevCode > PULSE_MASK)
                                prevCode = PULSE_MASK;
                            if (code[x] & IG_PULSE_BIT)
                                prevCode |= PULSE_BIT;
                        }
                        else if (((prevCode & PULSE_BIT) && (code[x] & IG_PULSE_BIT)) ||
                                 (!(prevCode & PULSE_BIT) && !(code[x] & IG_PULSE_BIT)))
                        {
                            /* can overflow pulse mask, so just set to
                             * largest possible */
                            if ((prevCode & PULSE_MASK) + (code[x] & IG_PULSE_MASK) > PULSE_MASK)
                                prevCode = (prevCode & PULSE_BIT) | PULSE_MASK;
                            else
                                prevCode += code[x] & IG_PULSE_MASK;
                        }
                        else
                        {
                            buffer[y] = prevCode;
                            y++;

                            prevCode = (code[x] & IG_PULSE_MASK);
                            if (prevCode > PULSE_MASK)
                                prevCode = PULSE_MASK;
                            if (code[x] & IG_PULSE_BIT)
                                prevCode |= PULSE_BIT;
                        }
                    }

                    /* write the data and free it */
                    if (y > 0)
                        chk_write(fd,
                                  buffer,
                                  sizeof(lirc_t) * y);
                    free(code);
                }

                iguanaFreePacket(response);
            }
    }

    iguanaClose(conn);
    close(fd);
}

static pid_t dowaitpid(pid_t pid, int* stat_loc, int options)
{
    pid_t retval;

    do
        retval = waitpid(pid, stat_loc, options);
    while (retval == (pid_t)-1 && errno == EINTR);

    return retval;
}

static void killChildProcess()
{
    /* signal the child process to exit */
    if (child > 0 && (kill(child, SIGTERM) == -1 || dowaitpid(child, NULL, 0) != (pid_t)-1))
        child = 0;
}

static int iguana_init()
{
    int recv_pipe[2], retval = 0;

    rec_buffer_init();

    if (pipe(recv_pipe) != 0)
    {
        log_error("couldn't open pipe: %s", strerror(errno));
    }
    else
    {
        int notify[2];

        if (pipe(notify) != 0)
        {
            log_error("couldn't open pipe: %s", strerror(errno));
            close(recv_pipe[0]);
            close(recv_pipe[1]);
        }
        else
        {
            drv.fd = recv_pipe[0];

            child = fork();
            if (child == -1)
            {
                log_error("couldn't fork child process: %s", strerror(errno));
            }
            else if (child == 0)
            {
                close(recv_pipe[0]);
                close(notify[0]);
                recv_loop(recv_pipe[1], notify[1]);
                _exit(0);
            }
            else
            {
                int dummy;

                close(recv_pipe[1]);
                close(notify[1]);
                /* make sure child has set its signal handler to avoid race with iguana_deinit() */
                chk_read(notify[0], &dummy, 1);
                close(notify[0]);
                sendConn = connectToIgdaemon(drv.device);
                if (sendConn == -1)
                {
                    log_error("couldn't open connection to iguanaIR daemon: %s",
                              strerror(errno));
                    killChildProcess();
                }
                else
                    retval = 1;
            }
        }
    }

    return retval;
}

static int iguana_deinit()
{
    /* close the connection to the iguana daemon */
    if (sendConn != -1)
    {
        iguanaClose(sendConn);
        sendConn = -1;
    }

    /* shut down the child reader */
    killChildProcess();

    /* close drv.fd since otherwise we leak open files */
    close(drv.fd);
    drv.fd = -1;

    return child == 0;
}

static char* iguana_rec(struct ir_remote* remotes)
{
    char* retval = NULL;

    if (rec_buffer_clear())
        retval = decode_all(remotes);
    return retval;
}

static int iguana_send(struct ir_remote* remote, struct ir_ncode* code)
{
    int retval = 0;
    uint32_t freq;

    /* set the carrier frequency if necessary */
    freq = htonl(remote->freq);
    if (remote->freq != currentCarrier && remote->freq >= 25000 && remote->freq <= 100000
        && daemonTransaction(sendConn, IG_DEV_SETCARRIER, &freq, sizeof(freq)))
        currentCarrier = remote->freq;

    if (send_buffer_put(remote, code))
    {
        int length, x;
        const lirc_t* signals;
        uint32_t* igsignals;

        length = send_buffer_length();
        signals = send_buffer_data();

        igsignals = (uint32_t*)malloc(sizeof(uint32_t) * length);
        if (igsignals != NULL)
        {
            /* pack the data into a unit32_t array */
            for (x = 0; x < length; x++)
            {
                igsignals[x] = signals[x] & PULSE_MASK;
                if (signals[x] & PULSE_BIT)
                    igsignals[x] |= IG_PULSE_BIT;
            }

            if (daemonTransaction(sendConn, IG_DEV_SEND, igsignals, sizeof(uint32_t) * length))
                retval = 1;
            free(igsignals);
        }
    }

    return retval;
}

#if VERSION_NODOTS >= 1000

static int list_devices(glob_t* devices)
{
    char *devList, *pos;
    glob_t_init(devices);

    pos = devList = iguanaListDevices();
    if (devList != NULL)
    {
        char *start, *idx, type;
        while(pos[0] != '\0')
        {
            type = *pos;
            pos += 2;

            start = pos;
            while(true)
            {
                // TODO: technically the user alias could be 12 bytes of UTF-8
                if (*pos == '\0' || *pos == '|' ||
                    ((type == 'i' || type == 'l') && *pos == ','))
                    break;
                pos++;
            }

            if (pos != start)
            {
                char buf[13] = { '\0' }, *s;

                strncpy(buf, start, pos - start);
#ifdef DESCRIBE_DEVICES
                s = (char*)malloc(strlen(IGSOCK_NAME) + strlen(buf) + 42 + 8);
#else
                s = (char*)malloc(strlen(IGSOCK_NAME) + strlen(buf) + 42 + strlen(IGSOCK_NAME) + 8);
#endif
                sprintf(s, "%s%s", IGSOCK_NAME, buf);

                if (type == 'i')
#ifdef DESCRIBE_DEVICES
                    idx = strdup(buf);
#else
                    idx = strdup(s);
#endif

                /* First word is the device path, the rest optional
                   free-format info on device usable in a UI. */
                switch(type)
                {
                case 'i':
#ifdef DESCRIBE_DEVICES
                    strcat(s, " IguanaWorks USB Infrared device ");
                    strcat(s, idx);
#else
                    strcat(s, " [1781:0938]");
#endif
                    break;

                case 'l':
#ifdef DESCRIBE_DEVICES
                    strcat(s, " Hardware location based alias for device ");
#else
                    strcat(s, " ->");
#endif
                    strcat(s, idx);
                    break;

                case 'u':
#ifdef DESCRIBE_DEVICES
                    strcat(s, " User specified alias for device ");
#else
                    strcat(s, " ->");
#endif
                    strcat(s, idx);
                    break;
                }
                glob_t_add_path(devices, s);
            }

            if (*pos != '\0')
                pos++;
        }
        free(devList);
    }

    return 0;
}

#endif


/*
 * Set the transmit channel(s) bitmask:.
 * Return 0 on success, 4 out-of-range, -1 on other errors. See
 * LIRC_SET_TRANSMITTER in lirc(4)
 */
static int set_transmitters(uint32_t channels)
{
    if (channels > 0x0F)
// TODO: check the device features for the transmitter count to return on bad mask
        return 4;
    else
    {
        uint8_t chans = channels;
        if (! daemonTransaction(sendConn, IG_DEV_SETCHANNELS, &chans, 1))
            return -1;
    }
    return 0;
}


static int drvctl_func(unsigned int cmd, void *arg)
{
    switch (cmd)
    {
#if VERSION_NODOTS >= 1000
    case DRVCTL_GET_DEVICES:
        return list_devices((glob_t*)arg);

    case DRVCTL_FREE_DEVICES:
        drv_enum_free((glob_t*)arg);
        return 0;
#endif

    case LIRC_SET_TRANSMITTER_MASK:
        return set_transmitters(*(uint32_t*)arg);

    default:
        return DRV_ERR_NOT_IMPLEMENTED;
    }
}


static lirc_t readdata(lirc_t timeout)
{
    lirc_t code = 0;
    struct pollfd pfd = {.fd = drv.fd, .events = POLLIN, .revents = 0};
    /* attempt a read with a timeout using select */
    if (poll(&pfd, 1, timeout / 1000) > 0)
    {
        /* if we failed to get data return 0 */
        if (read(drv.fd, &code, sizeof(lirc_t)) <= 0)
            iguana_deinit();
    }
    return code;
}

static const struct driver hw_iguanaIR = {
    .name           = "iguanair",
    .device         = "0",
    .features       = LIRC_CAN_REC_MODE2 |
                      LIRC_CAN_SEND_PULSE |
                      LIRC_CAN_SET_SEND_CARRIER |
                      LIRC_CAN_SET_TRANSMITTER_MASK,
    .send_mode      = LIRC_MODE_PULSE,
    .rec_mode       = LIRC_MODE_MODE2,
    .code_length    = sizeof(int),
    .init_func      = iguana_init,
    .deinit_func    = iguana_deinit,
    .open_func      = default_open,
    .close_func     = default_close,
    .send_func      = iguana_send,
    .rec_func       = iguana_rec,
    .decode_func    = receive_decode,
    .drvctl_func    = drvctl_func,
    .readdata       = readdata,
    .api_version    = 3,
    .driver_version = "0.9.3",
    .info           = "See file://" PLUGINDOCS "/iguanair.html",
    .device_hint    = "drvctl"
};

const struct driver* hardwares[] = {
    &hw_iguanaIR,
    (const struct driver*)NULL
};
