/****************************************************************************
 ** client.c ****************************************************************
 ****************************************************************************
 *
 * Source of the igclient application which should allow users to
 * fully control the Iguanaworks USB IR device.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "iguanaIR.h"
#include "compat.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <popt.h>
#ifdef WIN32
    #include "popt-fix.h"
#else
    #include <arpa/inet.h>
#endif

/* not necessary for a client, just helpful for some supporting
 * functions. */
#include "support.h"
#include "list.h"

enum
{
    /* use the upper buye of the short to not overlap with DEV_ commands */
    INTERNAL_SLEEP = 0x100,

    INTERNAL_GETCONFIG = 0x110,
    INTERNAL_GETOUTPINS,
    INTERNAL_GETPULLPINS,
    INTERNAL_GETOPENPINS,
    INTERNAL_GETSINKPINS,
    INTERNAL_GETHOLDPINS,

    INTERNAL_SETCONFIG = 0x120,
    INTERNAL_SETOUTPINS,
    INTERNAL_SETPULLPINS,
    INTERNAL_SETOPENPINS,
    INTERNAL_SETSINKPINS,
    INTERNAL_SETHOLDPINS,

    FINAL_CHECK = 0xFFFF,

    /* valid because we use the last 8 bytes of RAM as packet scratch space */
    PACKET_BUFFER_BASE = 0xF8
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
static bool interactive = false, recvOn = false;
static int logLevelTemp = 0;
static int deviceFeatures = 0;

static bool performTask(PIPE_PTR conn, igtask *cmd);

bool parseNumber(const char *text, unsigned int *value)
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

static bool receiveResponse(PIPE_PTR conn, igtask *cmd, int timeout)
{
    bool retval = false;
    uint64_t end;

    /* read the start and add the timeout */
    end = microsSinceX() + timeout * 1000;

    while(timeout >= 0)
    {
        iguanaPacket response;

        /* determine the time remaining */
        if (microsSinceX() >= end)
            break;

        response = iguanaReadResponse(conn, timeout);
        if (iguanaResponseIsError(response))
        {
            if (errno != 110 || (cmd->spec->code != FINAL_CHECK &&
                                 cmd->spec->code != INTERNAL_SLEEP))
                message(LOG_NORMAL, "%s: failed: %d: %s\n", cmd->spec->text,
                        errno, translateError(errno));
            /* failure means stop */
            timeout = -1;
        }
        else
        {
            void *data;
            unsigned int length, x;
            unsigned char code;

            /* need to translate the data, and can then toss the
             * response */
            data = iguanaRemoveData(response, &length);
            code = iguanaCode(response);

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
                    char buf[13];
                    strncpy(buf, data, 12);
                    buf[12] = '\0';
                    message(LOG_NORMAL, ": id=%s", buf);
                    break;
                }

                case IG_DEV_SENDSIZE:
                {
                    message(LOG_NORMAL, ": size=%d", *(uint16_t*)data);
                }
                }

                /* success means stop */
                timeout = -1;
                retval = true;
            }
            if (! cmd->isSubTask)
                message(LOG_NORMAL, "\n");

            free(data);
        }

        /* free successes or errors */
        iguanaFreePacket(response);
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

        case IG_DEV_RESEND:
            /* read pulse data from cmd->arg */
            data = malloc(sizeof(uint32_t));
            if (parseNumber(cmd->arg, ((uint32_t*)data)))
                result = sizeof(uint32_t);
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
        {
            iguanaPacket request = NULL;

            request = iguanaCreateRequest((unsigned char)cmd->spec->code,
                                          result, data);
            if (request == NULL)
                message(LOG_ERROR,
                        "Out of memory allocating request.\n");
            else if (! iguanaWriteRequest(request, conn))
                message(LOG_ERROR,
                        "Failed to write request to server.\n");
            else if (receiveResponse(conn, cmd, 10000))
                retval = true;

            /* release any allocated data buffers (including
             * data ptr) */
            iguanaFreePacket(request);
        }
    }

    return retval;
}

static void freeTask(igtask *cmd)
{
    /* explicitly cast off the const qualifier */
#ifndef WIN32
    free((char*)cmd->arg);
#endif
    free(cmd);
}

static int performQueuedTasks(PIPE_PTR conn)
{
    igtask *cmd;
    int failed = 0;

    memset(pinState, 0, IG_PIN_COUNT);
    while((cmd = (igtask*)removeFirstItem(&tasks)) != NULL)
    {
        if (checkTask(cmd) && ! performTask(conn, cmd))
            failed++;
        freeTask(cmd);
    }

    return failed;
}

static igtask* enqueueTask(char *text, const char *arg)
{
    igtask *cmd;

    cmd = (igtask*)malloc(sizeof(igtask));
    if (cmd != NULL)
    {
        cmd->command = text;
        cmd->arg = arg;
        insertItem(&tasks, NULL, (itemHeader*)cmd);
    }
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

    message(LOG_FATAL, "enqueueTaskById failed on code %d\n", code);
    return NULL;
}

static struct poptOption options[] =
{
    /* general options */
    { "device", 'd', POPT_ARG_STRING, NULL, 'd', "Specify the device to connect with (by index or id).", "number" },
    { "interactive", '\0', POPT_ARG_NONE, NULL, 'i', "Use the client interactively.", NULL },
    { "log-file", 'l', POPT_ARG_STRING, NULL, 'l', "Specify a log file (defaults to \"-\").", "filename" },
    { "quiet", 'q', POPT_ARG_NONE, NULL, 'q', "Reduce the verbosity.", NULL },
    { "verbose", 'v', POPT_ARG_NONE, NULL, 'v', "Increase the verbosity.", NULL },
    { "log-level", '\0', POPT_ARG_INT, &logLevelTemp, 'e', "Set the verbosity.", NULL },

    /* device commands */
    { "get-version", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETVERSION, "Return the version of the device firmware.", NULL },
    { "get-features", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETFEATURES, "Return the features associated w/ this device.", NULL },
    { "send", '\0', POPT_ARG_STRING, NULL, IG_DEV_SEND, "Send the pulses and spaces from a file.", "filename" },
    { "resend", '\0', POPT_ARG_INT, NULL, IG_DEV_RESEND, "Send the pulses and spaces already in the device buffer after a delay", "microsecond delay" },
    { "encoded-size", '\0', POPT_ARG_STRING, NULL, IG_DEV_SENDSIZE, "Check the encodes size of the pulses and spaces from a file.", "filename" },
    { "receiver-on", '\0', POPT_ARG_NONE, NULL, IG_DEV_RECVON, "Enable the receiver on the usb device.", NULL },
    { "receiver-off", '\0', POPT_ARG_NONE, NULL, IG_DEV_RECVOFF, "Disable the receiver on the usb device.", NULL },
    { "get-pins", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETPINS, "Get the pin values.", NULL },
    { "set-pins", '\0', POPT_ARG_STRING, NULL, IG_DEV_SETPINS, "Set the pin values.", "values" },
    { "get-buffer-size", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETBUFSIZE, "Find out the size of the RAM buffer used for sends and receives.", NULL },
    { "write-block", '\0', POPT_ARG_STRING, NULL, IG_DEV_WRITEBLOCK, "Write the block specified in the file.", "filename" },
    { "execute", '\0', POPT_ARG_NONE, NULL, IG_DEV_EXECUTE, "Execute code starting at address 0x1fc0 on the device.", "address" },
    { "lcd-text", '\0', POPT_ARG_STRING, NULL, IG_DEV_PINBURST, "Send a bulk transfer of pin settings to write the argument to an LCD.", "string" },
    { "reset", '\0', POPT_ARG_NONE, NULL, IG_DEV_RESET, "Reset the USB device.", NULL },
    { "get-channels", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETCHANNELS, "Check which channels are used during transmits.", NULL },
    { "set-channels", '\0', POPT_ARG_STRING, NULL, IG_DEV_SETCHANNELS, "Set which channels are used during transmits.", "channels" },
    { "get-carrier", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETCARRIER, "Check the carrier frequency for transmits.", NULL },
    { "set-carrier", '\0', POPT_ARG_STRING, NULL, IG_DEV_SETCARRIER, "Set the carrier frequency for transmits.", "carrier (Hz)" },

    /* commands that actually store and load the pin configuration */
    { "get-pin-config", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETPINCONFIG, "Retrieve the internal pin state.", NULL },
    { "set-pin-config", '\0', POPT_ARG_NONE, NULL, IG_DEV_SETPINCONFIG, "Store the internal pin state.", NULL },

    /* internal commands */
    { "get-output-pins", '\0', POPT_ARG_NONE, NULL, INTERNAL_GETOUTPINS, "Check which pins are set to be outputs.", NULL },
    { "set-output-pins", '\0', POPT_ARG_STRING, NULL, INTERNAL_SETOUTPINS, "Set which pins should be outputs.", "pins" },
    { "get-pullup-pins", '\0', POPT_ARG_NONE, NULL, INTERNAL_GETPULLPINS, "Check which pins are set to be pullups.", NULL },
    { "set-pullup-pins", '\0', POPT_ARG_STRING, NULL, INTERNAL_SETPULLPINS, "Set which pins should be pullups.", "pins" },
    { "get-open-drain-pins", '\0', POPT_ARG_NONE, NULL, INTERNAL_GETOPENPINS, "Check which pins are set to be open drains.", NULL },
    { "set-open-drain-pins", '\0', POPT_ARG_STRING, NULL, INTERNAL_SETOPENPINS, "Set which pins should be open drains.", "pins" },
    { "get-high-sink-pins", '\0', POPT_ARG_NONE, NULL, INTERNAL_GETSINKPINS, "Check which pins are set to be high sinks.", NULL },
    { "set-high-sink-pins", '\0', POPT_ARG_STRING, NULL, INTERNAL_SETSINKPINS, "Set which pins should be high sinks.", "pins" },
    { "get-threshold-pins", '\0', POPT_ARG_NONE, NULL, INTERNAL_GETHOLDPINS, "Check which pins are set to be thresholds.", NULL },
    { "set-threshold-pins", '\0', POPT_ARG_STRING, NULL, INTERNAL_SETHOLDPINS, "Set which pins should be thresholds.", "pins" },
    { "sleep", '\0', POPT_ARG_INT, NULL, INTERNAL_SLEEP, "Sleep for X seconds.", "seconds" },
    { "get-id", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETID, "Fetch the unique id from the USB device.", NULL },
    { "set-id", '\0', POPT_ARG_STRING, NULL, IG_DEV_SETID, "Set the unique id from the USB device.", NULL },
    { "get-location", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETLOCATION, "Fetch the bus and device indices for the USB device.", NULL },
    { "repeater-on", '\0', POPT_ARG_NONE, NULL, IG_DEV_REPEATER, "Put the device into a mode that repeats signals.", NULL },

#ifndef WIN32
    POPT_AUTOHELP
#endif
    POPT_TABLEEND
};

static void exitOnOptError(poptContext poptCon, char *msg)
{
    message(LOG_ERROR, msg, poptBadOption(poptCon, 0));
    poptPrintHelp(poptCon, stderr, 0);
    exit(1);
}

int main(int argc, const char **argv)
{
    const char **leftOvers, *device = "0";
    int x = 0, retval = 1;
    PIPE_PTR conn = INVALID_PIPE;
    poptContext poptCon;
    igtask *junk;

#ifdef WIN32
    const char *temp;
    temp = strrchr(argv[0], '\\');
    if (temp == NULL)
        programName = argv[0];
    else
        programName = temp + 1;
#endif

    poptCon = poptGetContext(NULL, argc, argv, options, 0);
    if (argc < 2)
    {
        poptPrintUsage(poptCon, stderr, 0);
        exit(1);
    }

    while(x != -1)
    {
        switch(x = poptGetNextOpt(poptCon))
        {
        /* device commands */
        case IG_DEV_GETVERSION:
        case IG_DEV_GETFEATURES:
        case IG_DEV_SEND:
        case IG_DEV_RESEND:
        case IG_DEV_SENDSIZE:
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
        case IG_DEV_GETID:
        case IG_DEV_SETID:
        case IG_DEV_GETLOCATION:
        case IG_DEV_REPEATER:

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
            enqueueTaskById(x, poptGetOptArg(poptCon));
            break;

        case IG_DEV_GETPINCONFIG:
#if 0
            enqueueTask("get config 0", NULL);
            enqueueTask("get config 1", NULL);
#else
            enqueueTaskById(x, poptGetOptArg(poptCon));
#endif
            break;

        case IG_DEV_SETPINCONFIG:
#if 0
            enqueueTask("set config 0", NULL);
            enqueueTask("set config 1", NULL);
#else
            enqueueTaskById(x, poptGetOptArg(poptCon));
#endif
            break;

        /* handling of the normal command arguments */
        case 'd':
            device = poptGetOptArg(poptCon);
            break;

        case 'i':
            interactive = true;
            break;

        case 'l':
            openLog(poptGetOptArg(poptCon));
            break;

        case 'q':
            changeLogLevel(-1);
            break;

        case 'v':
            changeLogLevel(+1);
            break;

        case 'e':
            setLogLevel(logLevelTemp);
            break;

        /* Error handling starts here */
        case POPT_ERROR_NOARG:
            exitOnOptError(poptCon, "Missing argument for '%s'\n");
            break;

        case POPT_ERROR_BADNUMBER:
            exitOnOptError(poptCon, "Need a number instead of '%s'\n");
            break;

        case POPT_ERROR_BADOPT:
            if (strcmp(poptBadOption(poptCon, 0), "-h") == 0)
            {
                poptPrintHelp(poptCon, stdout, 0);
                exit(0);
            }
            exitOnOptError(poptCon, "Unknown option '%s'\n");
            break;

        case -1:
            break;
        default:
            message(LOG_FATAL,
                    "Unexpected return value from popt: %d:%s\n",
                    x, poptStrerror(x));
            break;
        }
    }

    /* what if we have extra parameters? */
    leftOvers = poptGetArgs(poptCon);
    if (leftOvers != NULL && leftOvers[0] != NULL)
    {
        message(LOG_ERROR, "Unknown argument '%s'\n", leftOvers[0]);
        poptPrintHelp(poptCon, stderr, 0);
        exit(1);
    }
    poptFreeContext(poptCon);

    /* line buffer the output */
    setlinebuf(stdout);
    setlinebuf(stderr);

    /* connect first */
    conn = iguanaConnect(device);
    if (conn == INVALID_PIPE)
    {
#ifdef WIN32
        /* windows does not set errno correctly on failure */
        errno = GetLastError();
#endif
        if (errno == 2)
            message(LOG_ERROR, "Failed to connect to iguanaIR daemon: %s: is the daemon running and the device connected?\n",
                    translateError(errno));
        else if (errno != 0)
            message(LOG_ERROR, "Failed to connect to iguanaIR daemon: %s\n",
                    translateError(errno));
    }
    else
    {
        igtask cmd;

        /* handle all requests */
        if (interactive)
        {
            retval = 0;

            do
            {
                char line[512], *argument;

                retval += performQueuedTasks(conn);
                message(LOG_NORMAL, "igclient> ");
                if (fgets(line, 512, stdin) == NULL)
                {
                    message(LOG_NORMAL, "\n");
                    break;
                }
                line[strlen(line) - 1] = '\0';

                argument = strchr(line, ':');
                if (argument != NULL)
                {
                    argument[0] = '\0';
                    argument += strspn(argument + 1, " \r\n\t") + 1;
                }
                enqueueTask(line, argument);
            } while(true);
        }
        else if (firstItem(&tasks) == NULL)
            message(LOG_ERROR, "No tasks specified.\n");
        else
            retval = performQueuedTasks(conn);

        cmd.command = "final check";
        findTaskSpec(&cmd);
        /* cleanup, failure means nothing */
        receiveResponse(conn, &cmd, 0);
    }

    if (conn >= 0)
        iguanaClose(conn);

    /* delete any left over tasks on error */
    while((junk = (igtask*)removeFirstItem(&tasks)) != NULL)
        freeTask(junk);

    return retval;
}
