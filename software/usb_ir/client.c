/****************************************************************************
 ** client.c ****************************************************************
 ****************************************************************************
 *
 * Source of the igclient application which should allow users to
 * fully control the IguanaWorks USB IR device.
 *
 * Copyright (C) 2017, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "iguanaIR.h"
#include "version.h"
#include <argp.h>
#include "compat.h"

/* not necessary for any client application but helpful for some
   supporting functions in this case. */
#include "logging.h"
#include "list.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef WIN32
  #include <arpa/inet.h>
#endif

/* uncomment the following line to check if a blocking client causes
   problems in the daemon.
#define TEST_BLOCKING_CLIENT
//*/

enum
{
    /* use the upper byte of the short to not overlap with DEV_ commands */
    INTERNAL_SLEEP = 0x100,

    /* getters grouped together */
    INTERNAL_GETCONFIG = 0x110,
    INTERNAL_GETOUTPINS,
    INTERNAL_GETPULLPINS,
    INTERNAL_GETOPENPINS,
    INTERNAL_GETSINKPINS,
    INTERNAL_GETHOLDPINS,

    /* setters grouped together */
    INTERNAL_SETCONFIG = 0x120,
    INTERNAL_SETOUTPINS,
    INTERNAL_SETPULLPINS,
    INTERNAL_SETOPENPINS,
    INTERNAL_SETSINKPINS,
    INTERNAL_SETHOLDPINS,

    /* defines for using argp */
    GEN_GROUP = MSC_GROUP,
    DEV_GROUP,
    HELP_GROUP,

    /* substitutes to avoid odd characters in --help */
    ARGP_OFFSET = 0x200,
    OFFSET_RESEND      = ARGP_OFFSET + IG_DEV_RESEND,
    OFFSET_GETID       = ARGP_OFFSET + IG_DEV_GETID,
    OFFSET_SETID       = ARGP_OFFSET + IG_DEV_SETID,
    OFFSET_GETLOCATION = ARGP_OFFSET + IG_DEV_GETLOCATION,
    OFFSET_REPEATER    = ARGP_OFFSET + IG_DEV_REPEATER,
    OFFSET_SENDSIZE    = ARGP_OFFSET + IG_DEV_SENDSIZE,
    OFFSET_LISTALIASES = ARGP_OFFSET + IG_DEV_LISTALIASES,
    OFFSET_GETADDRESS  = ARGP_OFFSET + IG_DEV_GETADDRESS,
    OFFSET_LISTDEVS    = ARGP_OFFSET + IG_CTL_LISTDEVS,
    OFFSET_DEVADDR     = ARGP_OFFSET + IG_CTL_DEVADDR,

    /* used to check the receive buffer is empty in the end */
    FINAL_CHECK = 0xFFFF,

    /* valid because we use the last 8 bytes of RAM as packet scratch space */
    PACKET_BUFFER_BASE = 0xF8,

    /* match these to the CTL commands that we support */
    IG_FIRST_CTLCMD = IG_CTL_LISTDEVS,
    IG_LAST_CTLCMD  = IG_CTL_DEVADDR
};

/* declare and initialize the parameters structure */
struct parameters
{
    const char *device;
    bool listDevs;
} params = {
    NULL,
    false
};

typedef struct commandSpec
{
    char *text;
    bool internal;
    unsigned short code;
    unsigned char bit;
    bool parsePins;
} commandSpec;

static commandSpec supportedCommands[] =
{
    {"final check", false, FINAL_CHECK, 0, false},

    {"all devices",     false, IG_CTL_LISTDEVS, 0, false},
    {"device address",  false, IG_CTL_DEVADDR,  0, false},

    {"get version",     false, IG_DEV_GETVERSION,      0,      false},
    {"write block",     false, IG_DEV_WRITEBLOCK,      0,      false},
    {"checksum block",  false, IG_DEV_WRITEBLOCK,      0,      false},
    {"reset",           false, IG_DEV_RESET,           0,      false},

    {"get features",    false, IG_DEV_GETFEATURES,     0,      false},
    {"get buffer size", false, IG_DEV_GETBUFSIZE,      0,      false},
    {"receiver on",     false, IG_DEV_RECVON,          0,      false},
    {"raw receiver on", false, IG_DEV_RAWRECVON,       0,      false},
    {"receiver off",    false, IG_DEV_RECVOFF,         0,      false},
    {"send",            false, IG_DEV_SEND,            0,      false},
    {"resend",          false, IG_DEV_RESEND,          0,      false},
    {"all aliases",     false, IG_DEV_LISTALIASES,     0,      false},
    {"get address",     false, IG_DEV_GETADDRESS,      0,      false},
    {"encoded size",    false, IG_DEV_SENDSIZE,        0,      false},
    {"get channels",    false, IG_DEV_GETCHANNELS,     0,      false},
    {"set channels",    false, IG_DEV_SETCHANNELS,     0,      true},
    {"get carrier",     false, IG_DEV_GETCARRIER,      0,      false},
    {"set carrier",     false, IG_DEV_SETCARRIER,      0,      false},

    {"get pin config",  false, IG_DEV_GETPINCONFIG,    0,      false},
    {"get config 0",    false, IG_DEV_GETCONFIG0,      0,      false},
    {"get config 1",    false, IG_DEV_GETCONFIG1,      0,      false},
    {"set pin config",  false, IG_DEV_SETPINCONFIG,    0,      false},
    {"set config 0",    false, IG_DEV_SETCONFIG0,      0,      false},
    {"set config 1",    false, IG_DEV_SETCONFIG1,      0,      false},

    {"get pins",        false, IG_DEV_GETPINS,         0,      false},
    {"set pins",        false, IG_DEV_SETPINS,         0,      true},
    {"pin burst",       false, IG_DEV_PINBURST,        0,      false},
    {"execute code",    false, IG_DEV_EXECUTE,         0,      false},
    {"get id",          false, IG_DEV_GETID,           0,      false},
    {"set id",          false, IG_DEV_SETID,           0,      false},
    {"get location",    false, IG_DEV_GETLOCATION,     0,      false},
    {"repeater on",     false, IG_DEV_REPEATER,        0,      false},

    {"get output pins",     true, INTERNAL_GETOUTPINS,  IG_OUTPUT,     false},
    {"set output pins",     true, INTERNAL_SETOUTPINS,  IG_OUTPUT,     true},
    {"get pullup pins",     true, INTERNAL_GETPULLPINS, IG_PULLUP,     false},
    {"set pullup pins",     true, INTERNAL_SETPULLPINS, IG_PULLUP,     true},
    {"get open drain pins", true, INTERNAL_GETOPENPINS, IG_OPEN_DRAIN, false},
    {"set open drain pins", true, INTERNAL_SETOPENPINS, IG_OPEN_DRAIN, true},
    {"get high sink pins",  true, INTERNAL_GETSINKPINS, IG_HIGH_SINK,  false},
    {"set high sink pins",  true, INTERNAL_SETSINKPINS, IG_HIGH_SINK,  true},
    {"get threshold pins",  true, INTERNAL_GETHOLDPINS, IG_THRESHOLD,  false},
    {"set threshold pins",  true, INTERNAL_SETHOLDPINS, IG_THRESHOLD,  true},

    {"sleep",           true,  INTERNAL_SLEEP,   0,         false},

    {NULL,false,0,0,false}
};

typedef struct igtask
{
    /* for keeping a list of them */
    itemHeader header;

    /* populated by the user */
    const char *command, *arg;

    /* populated by checkTask */
    commandSpec *spec;

    /* rarely we need to know if this is a subtask */
    bool isSubTask;
} igtask;

/* globals */
static listHeader tasks;
static unsigned char pinState[IG_PIN_COUNT];
static bool recvOn = false;
static int deviceFeatures = 0;

static bool parseNumber(const char *text, unsigned int *value)
{
    bool retval = false;
    char c;

    if (sscanf(text, "0x%x%c", value, &c) == 1 ||
        sscanf(text, "%u%c", value, &c) == 1)
        retval = true;
    /* look for binary */
    else if (text[0] == 'b')
    {
        int x;
        retval = true;
        *value = 0;
        for(x = 1; text[x] != '\0'; x++)
            if (text[x] != '0' && text[x] != '1')
            {
                message(LOG_ERROR, "%c is not in binary.\n", text[x]);
                retval = false;
                break;
            }
            else
                *value = (*value << 1) + text[x] - '0';
    }
    else
        message(LOG_ERROR, "Unable to parse number: %s\n", text);

    return retval;
}

static bool findTaskSpec(igtask *task)
{
    unsigned int x, len;
    char *msg = NULL;

    /* start with no type specified */
    task->spec = NULL;

    len = (unsigned int)strlen(task->command);
    for(x = 0; supportedCommands[x].text != NULL; x++)
    {
        commandSpec *spec;
        spec = supportedCommands + x;
        if (strncmp(spec->text, task->command, len) == 0)
        {
            if (task->spec != NULL)
            {
                msg = "Ambiguous request";
                task->spec = NULL;
                break;
            }
            else
                task->spec = spec;
        }
    }

    if (msg == NULL &&
        task->spec == NULL)
        msg = "Invalid request";

    if (msg != NULL)
    {
        message(LOG_NORMAL, "%s: failed: %s\n", task->command, msg);
        return false;
    }

    return true;
}

static bool checkTask(igtask *task)
{
    bool retval = false;
    char *msg = NULL;

    if (findTaskSpec(task))
    {
        unsigned int value;
        task->command = task->spec->text;
        if (task->spec->parsePins && ! parseNumber(task->arg, &value))
            msg = "Could not parse argument";
    }

    if (msg != NULL)
        message(LOG_NORMAL, "%s: failed: %s\n", task->command, msg);
    else
        retval = true;

    return retval;
}

static unsigned char getSetting(unsigned int setting,
                                unsigned char *pinState)
{
    int x;
    unsigned char value = 0;
    for(x = 0; x < IG_PIN_COUNT; x++)
        value += (pinState[x] & setting) << x;
    return value;
}

static void setSetting(unsigned int setting, const char *pins,
                       unsigned char *pinState)
{
    unsigned int value;
    int x;

    if (parseNumber(pins, &value))
        for(x = 0; x < IG_PIN_COUNT; x++)
        {
            if (value & (1 << x))
                pinState[x] |= setting;
            else
                pinState[x] &= ~setting;
        }
}

static bool processResponse(unsigned char code, igtask *cmd, unsigned int length, void *data)
{
    bool retval = false;
    unsigned int x;

    if (code == IG_DEV_RECV)
            {
                length /= sizeof(uint32_t);
                message(LOG_NORMAL, "received %d signal(s):", length);
                for(x = 0; x < length; x++)
                {
                    uint32_t value;
                    value = ((uint32_t*)data)[x];
                    if (value & IG_PULSE_BIT)
                    {
                        value &= IG_PULSE_MASK;
                        message(LOG_NORMAL, "\n  pulse: ");
                    }
                    else
                        message(LOG_NORMAL, "\n  space: ");
                    message(LOG_NORMAL, "%d", value);
                }
                /* a receive does not end anything, try again */
            }
            else
            {
                if (! cmd->isSubTask)
                    message(LOG_NORMAL, "%s: success", cmd->spec->text);
                switch(code)
                {
                case IG_CTL_LISTDEVS:
                case IG_CTL_DEVADDR:
                case IG_DEV_LISTALIASES:
                    if (data == NULL)
                        message(LOG_NORMAL, ": no devices");
                    else
                        message(LOG_NORMAL, ": %s", (char*)data);
                    break;

                case IG_DEV_GETADDRESS:
                    message(LOG_NORMAL, ": %s", (char*)data);
                    break;

                case IG_DEV_GETVERSION:
                    message(LOG_NORMAL, ": version=0x%4.4x",
                            (((unsigned char*)data)[1] << 8 | \
                             ((unsigned char*)data)[0]));
                    break;

                case IG_DEV_GETFEATURES:
                    deviceFeatures = ((char*)data)[0];
                    if (! cmd->isSubTask)
                        message(LOG_NORMAL,
                                ": features=0x%x", deviceFeatures);
                    break;

                case IG_DEV_GETBUFSIZE:
                    message(LOG_NORMAL, ": size=%d", *(unsigned char*)data);
                    break;

                case IG_DEV_GETCHANNELS:
                    message(LOG_NORMAL, ": channels=0x%2.2x", *(char*)data);
                    break;

                case IG_DEV_GETCARRIER:
                    message(LOG_NORMAL,
                            ": carrier=%dHz", ntohl(*(uint32_t*)data));
                    break;

                case IG_DEV_GETLOCATION:
                    message(LOG_NORMAL,
                            ": location=%d:%d",
                            ((uint8_t*)data)[0], ((uint8_t*)data)[1]);
                    break;

                case IG_DEV_SETCARRIER:
                    message(LOG_NORMAL,
                            ": carrier=%dHz", ntohl(*(uint32_t*)data));
                    break;

                case IG_DEV_GETPINS:
                    message(LOG_NORMAL, ": 0x%2.2x",
                            iguanaDataToPinSpec(data,
                                                deviceFeatures & IG_SLOT_DEV));
                    break;

                case IG_DEV_GETPINCONFIG:
                    memcpy(pinState, data, IG_PIN_COUNT);
                    break;

                case IG_DEV_GETCONFIG0:
                    memcpy(pinState, data, IG_PIN_COUNT / 2);
                    break;

                case IG_DEV_GETCONFIG1:
                    memcpy(pinState + (IG_PIN_COUNT / 2), data, IG_PIN_COUNT / 2);
                    break;

                case IG_DEV_GETID:
                {
                    char buf[13] = {0};
                    strncpy(buf, data, 12);
                    message(LOG_NORMAL, ": id=%s", buf);
                    break;
                }

                case IG_DEV_SENDSIZE:
                {
                    message(LOG_NORMAL, ": size=%d", *(uint16_t*)data);
                }
                }

                retval = true;
            }
            if (! cmd->isSubTask)
                message(LOG_NORMAL, "\n");

            return retval;
}

static bool receiveResponse(PIPE_PTR conn, igtask *cmd, int timeout)
{
    bool retval = false;
#ifdef TEST_BLOCKING_CLIENT
    bool inSleep;
#endif
    uint64_t end;

    /* read the start and add the timeout */
    end = microsSinceX() + timeout * 1000;

#ifdef TEST_BLOCKING_CLIENT
    inSleep = timeout != 10000 && timeout != 0;
#endif
    while(timeout >= 0)
    {
        iguanaPacket response = NULL;
        uint64_t now = microsSinceX();

        /* try not to wait past the computed end, do wait at least once */
#ifdef TEST_BLOCKING_CLIENT
        if (inSleep)
            Sleep(timeout);
        else
#endif
        {
            if (now > end)
                timeout = 0;
            else
                timeout = (uint32_t)(end - now) / 1000;
            response = iguanaReadResponse(conn, timeout);
        }
        if (iguanaResponseIsError(response))
        {
            if ((errno != ETIMEDOUT && errno != EIO) || (cmd->spec->code != INTERNAL_SLEEP && cmd->spec->code != FINAL_CHECK))
                message(LOG_NORMAL, "%s: failed: %d: %s\n", cmd->spec->text,
                        errno, translateError(errno));
            /* failure means stop */
            timeout = -1;
        }
        else
        {
            void *data;
            unsigned int length;

            /* translate the data and then toss the response */
            data = iguanaRemoveData(response, &length);

            retval = processResponse(iguanaCode(response), cmd, length, data);
            /* success means stop looping, but do the coming cleanup */
            if (retval)
                timeout = -1;

            free(data);
        }

        /* free successes or errors */
        iguanaFreePacket(response);

        /* break out when we hit the end */
        if (microsSinceX() > end)
            break;
    }

    return retval;
}

/* some requests are handled without sending requests to the server */
static bool handleInternalTask(igtask *cmd, PIPE_PTR conn)
{
    bool retval = false;

    if (cmd->spec->code == INTERNAL_SLEEP)
    {
        float seconds;
        char dummy;
        if (sscanf(cmd->arg, "%f%c", &seconds, &dummy) != 1)
            message(LOG_ERROR, "failed to parse sleep time.\n");
        else if (seconds < 0)
            message(LOG_ERROR, "sleep time cannot be negative.\n");
        else if (receiveResponse(conn, cmd, (int)(seconds * 1000)))
        {
            message(LOG_NORMAL, "%s (%.3f): success\n", cmd->command, seconds);
            retval = true;
        }
    }
    else if ((cmd->spec->code & INTERNAL_GETCONFIG) == INTERNAL_GETCONFIG)
    {
        message(LOG_NORMAL, "%s: success: 0x%2.2x\n",
                cmd->command, getSetting(cmd->spec->bit, pinState));
        retval = true;
    }
    else if ((cmd->spec->code & INTERNAL_SETCONFIG) == INTERNAL_SETCONFIG)
    {
        setSetting(cmd->spec->bit, cmd->arg, pinState);
        message(LOG_NORMAL, "%s: success\n", cmd->command);
        retval = true;
    }
    else
        message(LOG_FATAL, "Unhandled internal task: %s\n", cmd->spec->text);

    return retval;
}

static bool transaction(igtask *cmd, PIPE_PTR conn, int amt, void *data)
{
    bool retval = false;
    iguanaPacket request = NULL;

    request = iguanaCreateRequest((unsigned char)cmd->spec->code, amt, data);
    if (request == NULL)
        message(LOG_ERROR, "Out of memory allocating request.\n");
    else if (! iguanaWriteRequest(request, conn))
        message(LOG_ERROR, "Failed to write request to server.\n");
    else if (receiveResponse(conn, cmd, 10000))
        retval = true;

    /* release allocated data buffers (including data ptr) */
    iguanaFreePacket(request);
    return retval;
}

static bool performTask(PIPE_PTR conn, igtask *cmd)
{
    bool retval = false;

    if (cmd->spec->internal)
        retval = handleInternalTask(cmd, conn);
    else
    {
        int result = 0;
        void *data = NULL;

        switch(cmd->spec->code)
        {
        case IG_CTL_DEVADDR:
            result = strlen(cmd->arg) + 1;
            data = strdup(cmd->arg);
            break;

        case IG_DEV_RECVON:
            recvOn = true;
            break;

        case IG_DEV_RECVOFF:
            recvOn = false;
            break;

        case IG_DEV_SETCHANNELS:
        {
            unsigned int value;

            errno = EINVAL;
            result = -1;
            /* translate cmd->pins */
            if (parseNumber(cmd->arg, &value))
            {
                unsigned char mask = 0xFF;
                if (deviceFeatures & IG_SLOT_DEV)
                    mask >>= 2;
                else
                    mask >>= 4;
                message(LOG_INFO, "Mask is now: 0x%x\n", mask);

                if (cmd->spec->code == IG_DEV_SETCHANNELS &&
                    value != (mask & value))
                    message(LOG_ERROR, "Too many channels specified.\n");
                else
                {
                    data = malloc(1);
                    ((char*)data)[0] = (char)value;
                    result = 1;
                }
            }
            break;
        }

        case IG_DEV_SETCARRIER:
        {
            uint32_t value;

            errno = EINVAL;
            result = -1;
            /* translate cmd->pins */
            if (parseNumber(cmd->arg, &value))
            {
                if (cmd->spec->code == IG_DEV_SETCARRIER &&
                         (value < 25000 || value > 150000))
                    message(LOG_ERROR, "Carrier frequency must be between 25 and 150 kHz.\n");
                    else
                {
                    data = malloc(4);
                    *(uint32_t *)data = htonl(value);
                    result = 4;
                }
            }
            break;
        }

        case IG_DEV_SETPINCONFIG:
            result = IG_PIN_COUNT;
            data = malloc(sizeof(pinState));
            memcpy(data, pinState, sizeof(pinState));
            break;

        case IG_DEV_SETCONFIG0:
            result = IG_PIN_COUNT / 2;
            data = malloc(sizeof(pinState) / 2);
            memcpy(data, pinState, sizeof(pinState) / 2);
            break;

        case IG_DEV_SETCONFIG1:
            result = IG_PIN_COUNT / 2;
            data = malloc(sizeof(pinState) / 2);
            memcpy(data, pinState + (IG_PIN_COUNT / 2), sizeof(pinState) / 2);
            break;

        case IG_DEV_SETPINS:
        {
            unsigned int value;

            errno = EINVAL;
            result = -1;
            /* translate cmd->pins */
            if (parseNumber(cmd->arg, &value))
            {
                if (value > 0xFF)
                    message(LOG_ERROR, "Only %d pins are available, invalid pin specification.\n", IG_PIN_COUNT);
                else
                    result = iguanaPinSpecToData(value, &data,
                                                 deviceFeatures & IG_SLOT_DEV);
            }
            break;
        }

        case IG_DEV_SEND:
        case IG_DEV_SENDSIZE:
            /* read pulse data from cmd->arg */
            result = iguanaReadPulseFile(cmd->arg, &data);
            result *= sizeof(uint32_t);
            break;

        case IG_DEV_SETID:
            result = strlen(cmd->arg) + 1;
            if (result > 13)
                message(LOG_WARN, "Label is too long and will be truncated to 12 characters.\n");
            data = strdup(cmd->arg);
            break;

        case IG_DEV_WRITEBLOCK:
            if (iguanaReadBlockFile(cmd->arg, &data))
                result = 68;
            break;

        case IG_DEV_PINBURST:
        {
            unsigned int x;
            data = (void*)malloc(64);
            result = 64;

            for(x = 0; x < 15 && cmd->arg[x] != '\0'; x++)
            {
                unsigned char *b, c;
                b = (unsigned char*)data + x * 4 + 1;
                c = cmd->arg[x];

                b[0] = (c & 0xF0) | 0x0D;
                b[1] = (c & 0xF0) | 0x0C;
                b[2] = (c & 0x0F) << 4 | 0x0D;
                b[3] = (c & 0x0F) << 4 | 0x0C;
            }
            ((unsigned char*)data)[0] = (unsigned char)(x * 4);

            break;
        }
        }

        if (result < 0)
            message(LOG_ERROR,
                    "Failed to pack data: %s\n", translateError(errno));
        else
            retval = transaction(cmd, conn, result, data);
    }

    return retval;
}

bool connectToDaemon(const char *name, PIPE_PTR *conn)
{
    if ((*conn = iguanaConnect(name)) == INVALID_PIPE)
    {
#ifdef WIN32
        /* windows does not set errno correctly on failure */
        errno = GetLastError();
#endif
        if (errno == 2)
        {
            if (name != NULL && strcmp("ctl", name) == 0)
                message(LOG_ERROR, "Failed to connect to iguanaIR daemon: %s: is the daemon running?\n",
                        translateError(errno));
            else
                message(LOG_ERROR, "Failed to connect to iguanaIR daemon: %s: is the daemon running and the device connected?\n",
                        translateError(errno));
        }
        else if (errno != 0)
            message(LOG_ERROR, "Failed to connect to iguanaIR daemon at %s: %s\n",
                    name, translateError(errno));
    }
    else
        return true;
    return false;
}

static int performQueuedTasks()
{
    igtask *cmd;
    int failed = 0;
    PIPE_PTR devConn = INVALID_PIPE, ctlConn = INVALID_PIPE;

    memset(pinState, 0, IG_PIN_COUNT);
    while((cmd = (igtask*)removeFirstItem(&tasks)) != NULL)
    {
        if (checkTask(cmd))
        {
            PIPE_PTR *conn = &devConn;
            const char *name = params.device;
            if (cmd->spec->code >= IG_FIRST_CTLCMD && cmd->spec->code <= IG_LAST_CTLCMD)
            {
                conn = &ctlConn;
                name = "ctl";
            }

            if ((*conn == INVALID_PIPE && ! connectToDaemon(name, conn)) || ! performTask(*conn, cmd))
                failed++;
        }
        free(cmd);
    }

    iguanaClose(devConn);
    iguanaClose(ctlConn);

    return failed;
}

static igtask* enqueueTask(char *text, const char *arg)
{
    igtask *cmd;

    cmd = (igtask*)malloc(sizeof(igtask));
    if (cmd == NULL)
        message(LOG_FATAL, "igtask malloc failed: %s\n", translateError(errno));
    memset(cmd, 0, sizeof(igtask));
    cmd->command = text;
    cmd->arg = arg;
    insertItem(&tasks, NULL, (itemHeader*)cmd);
    return cmd;
}

static igtask* enqueueTaskById(unsigned short code, const char *arg)
{
    unsigned int x;

    if (code == IG_DEV_SETCHANNELS ||
        code == IG_DEV_GETPINS ||
        code == IG_DEV_SETPINS)
    {
        igtask *cur = (igtask*)tasks.head;
        for(; cur; cur = (igtask*)cur->header.next)
            if (strcmp(cur->command,
                       supportedCommands[IG_DEV_GETFEATURES].text) == 0)
                break;

        /* since pci slot-cover devices have 6 channels we
           need the feature information in order to tell what
           channel mask to use. */
        if (cur == NULL)
        {
            cur = enqueueTaskById(IG_DEV_GETFEATURES, NULL);
            cur->isSubTask = true;
        }
    }

    for(x = 0; supportedCommands[x].text != NULL; x++)
        if (code == supportedCommands[x].code)
            return enqueueTask(supportedCommands[x].text, arg);

    message(LOG_FATAL, "enqueueTaskById failed on code 0x%x\n", code);
    return NULL;
}

static struct argp_option options[] = {
    { NULL, 0, NULL, 0, "General options:", GEN_GROUP },
    { "all-devices", OFFSET_LISTDEVS, NULL,     0, "List all devices known to the daemon.",   GEN_GROUP },
    { "dev-address", OFFSET_DEVADDR,  "ALIAS",  0, "Ask the daemon for an alias' address.",   GEN_GROUP },
    { "device",      'd',             "DEVICE", 0, "Specify the target device index or id.",  GEN_GROUP },
    { "sleep",       INTERNAL_SLEEP,  "NUM",    0, "Sleep for NUM seconds.",                  GEN_GROUP },

    /* device commands */
    { NULL, 0, NULL, 0, "Device commands:", DEV_GROUP },
    { "get-version",     IG_DEV_GETVERSION,  NULL,       0, "Return the version of the device firmware.",                    DEV_GROUP },
    { "get-features",    IG_DEV_GETFEATURES, NULL,       0, "Return the features associated w/ this device.",                DEV_GROUP },
    { "send",            IG_DEV_SEND,        "FILE",     0, "Send the pulses and spaces from a file.",                       DEV_GROUP },
    { "resend",          OFFSET_RESEND,      "DELAY",    0, "Resend the contents of the device buffer after DELAY seconds.", DEV_GROUP },
    { "all-aliases",     OFFSET_LISTALIASES, NULL,       0, "List all the valid names for this device.",                     DEV_GROUP },
    { "get-address",     OFFSET_GETADDRESS,  NULL,       0, "Return the base address for a device.",                         DEV_GROUP },
    { "encoded-size",    OFFSET_SENDSIZE,    "FILE",     0, "Check the encodes size of the pulses and spaces from a file.",  DEV_GROUP },
    { "receiver-on",     IG_DEV_RECVON,      NULL,       0, "Enable the receiver on the usb device.",                        DEV_GROUP },
    { "receiver-off",    IG_DEV_RECVOFF,     NULL,       0, "Disable the receiver on the usb device.",                       DEV_GROUP },
    { "get-pins",        IG_DEV_GETPINS,     NULL,       0, "Get the pin values.",                                           DEV_GROUP },
    { "set-pins",        IG_DEV_SETPINS,     "PINS",     0, "Set the pin values.",                                           DEV_GROUP },
    { "get-buffer-size", IG_DEV_GETBUFSIZE,  NULL,       0, "Check onboard RAM send/receive buffer size.",                   DEV_GROUP },
    { "write-block",     IG_DEV_WRITEBLOCK,  "FILE",     0, "Write the block specified in the file.",                        DEV_GROUP },
    { "execute",         IG_DEV_EXECUTE,     NULL,       0, "Execute code starting at address 0x1fc0 on the device.",        DEV_GROUP },
    { "lcd-text",        IG_DEV_PINBURST,    "STR",      0, "Send bulk transfers of pin settings to display STR on an LCD.", DEV_GROUP },
    { "reset",           IG_DEV_RESET,       NULL,       0, "Reset the USB device.",                                         DEV_GROUP },
    { "get-channels",    IG_DEV_GETCHANNELS, NULL,       0, "Check which channels are used during transmits.",               DEV_GROUP },
    { "set-channels",    IG_DEV_SETCHANNELS, "CHANNELS", 0, "Set which channels are used during transmits.",                 DEV_GROUP },
    { "get-carrier",     IG_DEV_GETCARRIER,  NULL,       0, "Check the carrier frequency for transmits.",                    DEV_GROUP },
    { "set-carrier",     IG_DEV_SETCARRIER, "HZ",        0, "Set the carrier frequency for transmits.",                      DEV_GROUP },

    /* commands that actually store and load the pin configuration */
    { "get-pin-config", IG_DEV_GETPINCONFIG, NULL, 0, "Retrieve the internal pin state.", DEV_GROUP },
    { "set-pin-config", IG_DEV_SETPINCONFIG, NULL, 0, "Store the internal pin state.",    DEV_GROUP },

    /* internal commands from here on down (i.e. handled within the app w/o communication w the device) */
    /* pin configuration commands */
    { "get-output-pins",     INTERNAL_GETOUTPINS,  NULL,   0, "Check which pins are set to be outputs.",     DEV_GROUP },
    { "set-output-pins",     INTERNAL_SETOUTPINS,  "PINS", 0, "Set which pins should be outputs.",           DEV_GROUP },
    { "get-pullup-pins",     INTERNAL_GETPULLPINS, NULL,   0, "Check which pins are set to be pullups.",     DEV_GROUP },
    { "set-pullup-pins",     INTERNAL_SETPULLPINS, "PINS", 0, "Set which pins should be pullups.",           DEV_GROUP },
    { "get-open-drain-pins", INTERNAL_GETOPENPINS, NULL,   0, "Check which pins are set to be open drains.", DEV_GROUP },
    { "set-open-drain-pins", INTERNAL_SETOPENPINS, "PINS", 0, "Set which pins should be open drains.",       DEV_GROUP },
    { "get-high-sink-pins",  INTERNAL_GETSINKPINS, NULL,   0, "Check which pins are set to be high sinks.",  DEV_GROUP },
    { "set-high-sink-pins",  INTERNAL_SETSINKPINS, "PINS", 0, "Set which pins should be high sinks.",        DEV_GROUP },
    { "get-threshold-pins",  INTERNAL_GETHOLDPINS, NULL,   0, "Check which pins are set to be thresholds.",  DEV_GROUP },
    { "set-threshold-pins",  INTERNAL_SETHOLDPINS, "PINS", 0, "Set which pins should be thresholds.",        DEV_GROUP },

    /* other application internal commands */
    { "get-id",              OFFSET_GETID,         NULL,   0, "Fetch the unique id from the USB device.",             DEV_GROUP },
    { "set-id",              OFFSET_SETID,         "STR",  0, "Set the unique id from the USB device.",               DEV_GROUP },
    { "get-location",        OFFSET_GETLOCATION,   NULL,   0, "Fetch the bus and device indices for the USB device.", DEV_GROUP },
    { "repeater-on",         OFFSET_REPEATER,      NULL,   0, "Put the device into a mode that repeats signals.",     DEV_GROUP },

    { NULL, 0, NULL, 0, "Help related options:", HELP_GROUP },

    /* end of table */
    {0}
};

static error_t parseOption(int key, char *arg, struct argp_state *state)
{
    switch(key)
    {
    /* device commands */
    case IG_DEV_GETVERSION:
    case IG_DEV_GETFEATURES:
    case IG_DEV_SEND:
    case IG_DEV_RECVON:
    case IG_DEV_RECVOFF:
    case IG_DEV_GETPINS:
    case IG_DEV_SETPINS:
    case IG_DEV_GETBUFSIZE:
    case IG_DEV_WRITEBLOCK:
    case IG_DEV_RESET:
    case IG_DEV_GETCHANNELS:
    case IG_DEV_SETCHANNELS:
    case IG_DEV_GETCARRIER:
    case IG_DEV_SETCARRIER:
    case IG_DEV_PINBURST:
    case IG_DEV_EXECUTE:

    /* internal commands */
    case INTERNAL_GETOUTPINS:
    case INTERNAL_SETOUTPINS:
    case INTERNAL_GETPULLPINS:
    case INTERNAL_SETPULLPINS:
    case INTERNAL_GETOPENPINS:
    case INTERNAL_SETOPENPINS:
    case INTERNAL_GETSINKPINS:
    case INTERNAL_SETSINKPINS:
    case INTERNAL_GETHOLDPINS:
    case INTERNAL_SETHOLDPINS:
    case INTERNAL_SLEEP:
        enqueueTaskById((unsigned short)key, arg);
        break;

    /* a few codes must be offset since they are printable characters otherwise */
    case OFFSET_RESEND:
        enqueueTaskById(INTERNAL_SLEEP, arg);
    case OFFSET_GETID:
    case OFFSET_SETID:
    case OFFSET_GETLOCATION:
    case OFFSET_REPEATER:
    case OFFSET_SENDSIZE:
    case OFFSET_LISTALIASES:
    case OFFSET_GETADDRESS:
    case OFFSET_LISTDEVS:
    case OFFSET_DEVADDR:
        enqueueTaskById((unsigned short)(key - ARGP_OFFSET), arg);
        break;

    case IG_DEV_GETPINCONFIG:
#if 0
        enqueueTask("get config 0", NULL);
        enqueueTask("get config 1", NULL);
#else
        enqueueTaskById((unsigned short)key, arg);
#endif
        break;

    case IG_DEV_SETPINCONFIG:
#if 0
        enqueueTask("set config 0", NULL);
        enqueueTask("set config 1", NULL);
#else
        enqueueTaskById((unsigned short)key, arg);
#endif
        break;

    /* handling of the normal command arguments */
    case 'd':
        if (strlen(arg) == 0)
            params.device = NULL;
        else
            params.device = arg;
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp parser = {
    options,
    parseOption,
    NULL,
    "This program is an interface to and example of the IguanaIR API\n",
    NULL,
    NULL,
    NULL
};

int main(int argc, char **argv)
{
    PIPE_PTR conn = INVALID_PIPE;
    int retval = 1;
    igtask *junk;
    struct argp_child children[2];
    logSettings settings = INIT_LOG_SETTINGS;

    initializeLogging(&settings);

    /* include the log argument parser */
    memset(children, 0, sizeof(struct argp_child) * 2);
    children[0].argp = logArgParser();
    parser.children = children;

    /* parse the cmd line args */
    argp_parse(&parser, argc, argv, 0, NULL, NULL);

    /* line buffer the output */
#ifndef ANDROID
    setlinebuf(stdout);
    setlinebuf(stderr);
#endif

    /* issue any ctl commands before connecting to devices */
    if (firstItem(&tasks) == NULL)
        message(LOG_ERROR, "No tasks specified.\n");
    else
    {
        igtask cmd;

        /* handle all requests */
        retval = performQueuedTasks();

        cmd.command = "final check";
        findTaskSpec(&cmd);
        /* cleanup, failure means nothing */
        receiveResponse(conn, &cmd, 0);
    }

    if (conn >= 0)
        iguanaClose(conn);

    /* delete any left over tasks on error */
    while((junk = (igtask*)removeFirstItem(&tasks)) != NULL)
        free(junk);

    return retval;
}
