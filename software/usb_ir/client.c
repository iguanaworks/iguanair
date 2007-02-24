/****************************************************************************
 ** client.c ****************************************************************
 ****************************************************************************
 *
 * Source of the igclient application which should allow users to
 * fully control the Iguanaworks USB IR device.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#include "base.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <popt.h>

#include "iguanaIR.h"

/* not necessary for a client, just helpful for some supporting
 * functions. */
#include "support.h"
#include "list.h"

enum
{
    /* use the upper buye of the short to not overlap with DEV_ commands */
    INTERNAL_SLEEP = 0x100,
    INTERNAL_GETID = 0x101,
    INTERNAL_SETID = 0x102,

    INTERNAL_GETPINS = 0x110,
    INTERNAL_GETOUTPINS,
    INTERNAL_GETPULLPINS,
    INTERNAL_GETOPENPINS,
    INTERNAL_GETSINKPINS,
    INTERNAL_GETHOLDPINS,

    INTERNAL_SETPINS = 0x120,
    INTERNAL_SETOUTPINS,
    INTERNAL_SETPULLPINS,
    INTERNAL_SETOPENPINS,
    INTERNAL_SETSINKPINS,
    INTERNAL_SETHOLDPINS,

    FINAL_CHECK = 0xFFFF
}; 

typedef struct commandSpec
{
    char *text;
    bool internal;
    unsigned short code;
    unsigned char bit;
    bool needsArg, parsePins;
} commandSpec;

static commandSpec supportedCommands[] =
{
    {"final check", false, FINAL_CHECK, 0, false, false},

    {"get version",     false, IG_DEV_GETVERSION,      0,      false, false},
    {"send",            false, IG_DEV_SEND,            0,      true,  false},
    {"receiver on",     false, IG_DEV_RECVON,          0,      false, false},
    {"receiver off",    false, IG_DEV_RECVOFF,         0,      false, false},
    {"get pins",        false, IG_DEV_GETPINS,         0,      false, false},
    {"set pins",        false, IG_DEV_SETPINS,         0,      true,  true},
    {"get buffer size", false, IG_DEV_GETBUFSIZE,      0,      false, false},
    {"write block",     false, IG_DEV_WRITEBLOCK,      0,      true,  false},
    {"execute code",    false, IG_DEV_EXECUTE,         0,      false, false},
    {"bulk pins",       false, IG_DEV_BULKPINS,        0,      true,  false},
    {"get id",          false, IG_DEV_GETID,           0,      false, false},
    {"reset",           false, IG_DEV_RESET,           0,      false, false},

    {"get config 0",    false, IG_DEV_GETCONFIG0,      0,      false, false},
    {"get config 1",    false, IG_DEV_GETCONFIG1,      0,      false, false},
    {"set config 0",    false, IG_DEV_SETCONFIG0,      0,      false, false},
    {"set config 1",    false, IG_DEV_SETCONFIG1,      0,      false, false},

    {"get output pins", true,  INTERNAL_GETOUTPINS, IG_OUTPUT, false, false},
    {"set output pins", true,  INTERNAL_SETOUTPINS, IG_OUTPUT, true,  true},
    {"get pullup pins", true,  INTERNAL_GETPULLPINS, IG_PULLUP, false, false},
    {"set pullup pins", true,  INTERNAL_SETPULLPINS, IG_PULLUP, true,  true},
    {"get open drain pins", true,  INTERNAL_GETOPENPINS, IG_OPEN_DRAIN, false, false},
    {"set open drain pins", true,  INTERNAL_SETOPENPINS, IG_OPEN_DRAIN, true,  true},
    {"get high sink pins", true,INTERNAL_GETSINKPINS, IG_HIGH_SINK, false, false},
    {"set high sink pins", true,INTERNAL_SETSINKPINS, IG_HIGH_SINK, true,  true},
    {"get threshold pins", true,INTERNAL_GETHOLDPINS, IG_THRESHOLD, false, false},
    {"set threshold pins", true,INTERNAL_SETHOLDPINS, IG_THRESHOLD, true,  true},

    {"sleep",           true,  INTERNAL_SLEEP,   0,         true,  false},
    {"internal get id", true,  INTERNAL_GETID,   0,         false, false},
    {"internal set id", true,  INTERNAL_SETID,   0,         true,  false},

    {NULL}
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

    len = strlen(task->command);
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
        task->command = task->spec->text;
        if (task->spec->needsArg)
        {
            unsigned int value;
            if (task->arg == NULL)
                msg = "Missing argument";
            else if (task->spec->parsePins &&
                     ! parseNumber(task->arg, &value))
                msg = "Could no parse argument";
        }
        else if (! task->spec->needsArg && task->arg != NULL)
            msg = "Unnecessary argument";
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
    if (parseNumber(pins, &value))
    {
        int x;
        for(x = 0; x < IG_PIN_COUNT; x++)
        {
            if (value & (1 << x))
                pinState[x] |= setting;
            else
                pinState[x] &= ~setting;
        }
    }
}

static void receiveResponse(PIPE_PTR conn, igtask *cmd, int timeout)
{
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
                message(LOG_NORMAL,
                        "%s: failed: %s\n", cmd->spec->text, strerror(errno));
            /* failure means stop */
            timeout = -1;
        }
        else
        {
            void *data;
            unsigned int length, x;
            unsigned char code;

            /* need to translate the data, and can then
             * toss the response */
            data = iguanaRemoveData(response, &length);
            code = iguanaCode(response);
            iguanaFreePacket(response);

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
                message(LOG_NORMAL, "%s: success", cmd->spec->text);
                switch(code)
                {
                case IG_DEV_GETVERSION:
                    message(LOG_NORMAL, ": version=%d", *(int*)data);
                    break;

                case IG_DEV_GETPINS:
                    message(LOG_NORMAL,
                            ": 0x%2.2x", iguanaDataToPinSpec(data));
                    break;

                case IG_DEV_GETCONFIG0:
                    memcpy(pinState, data, IG_PIN_COUNT / 2);
                    break;

                case IG_DEV_GETCONFIG1:
                    memcpy(pinState + (IG_PIN_COUNT / 2), data, IG_PIN_COUNT / 2);
                    break;

                case IG_DEV_GETID:
                    message(LOG_NORMAL, ": id=%s", (char*)data);
                    break;

                case IG_DEV_GETBUFSIZE:
                    message(LOG_NORMAL, ": size=%d", *(unsigned char*)data);
                    break;
                }
                /* success means stop */
                timeout = -1;
            }
            message(LOG_NORMAL, "\n");

            free(data);
        }
    }
}

static void performTask(PIPE_PTR conn, igtask *cmd);

/* valid because of pigeon hole principal */
#define DATA_BUFFER_BASE 0x7C

static unsigned int assembleData(char *dst, const char *src, unsigned int len, unsigned char target)
{
    unsigned int x, y = 0;
    for(x = 0; x < len; x++)
    {
        dst[y++] = 0x55;
        dst[y++] = target++;
        dst[y++] = src[x];
    }

    return y;
}

static void* writeBlocks(const char *label)
{
    unsigned int len, x, y;
    char *data, *dummy;

    len = 4 + strlen(label);
    if (len > 16)
    {
        message(LOG_ERROR, "Label is too long, truncating to 12 bytes.\n");
        len = 16;
    }

    dummy = malloc(len + 1);
    dummy[0] = IG_CTL_START;
    dummy[1] = IG_CTL_START;
    dummy[2] = (char)IG_CTL_FROMDEV;
    dummy[3] = IG_DEV_GETID;
    strncpy(dummy + 4, label, len - 4);
    dummy[len] = '\0';
    label = dummy;

    data = malloc(68);
    memset(data, 0, 68);
    data[0] = 0x7f;

    y = 4;
    for(x = 0; x < 2; x++)
    {
        if ((x + 1) * 8 > len)
        {
            int amount;
            char buffer[8] = { 0 };

            amount = len % 8;
            y += assembleData(data + y, label + x * 8, amount,
                              DATA_BUFFER_BASE);
            y += assembleData(data + y, buffer, 8 - amount,
                              DATA_BUFFER_BASE + amount);
        }
        else
            y += assembleData(data + y, label + x * 8, 8, DATA_BUFFER_BASE);

        data[y++] = 0x57;
        data[y++] = DATA_BUFFER_BASE;
        data[y++] = 0x7C;
        data[y++] = 0x00;
        data[y++] = 0x68;
    }
    data[y++] = 0x7F;

    return data;
}

/* some requests are handled without sending requests to the server */
static void handleInternalTask(igtask *cmd, PIPE_PTR conn)
{
    if (cmd->spec->code == INTERNAL_SLEEP)
    {
        float seconds;
        char dummy;
        if (sscanf(cmd->arg, "%f%c", &seconds, &dummy) == 1)
        {
            receiveResponse(conn, cmd, (int)(seconds * 1000));
            message(LOG_NORMAL, "%s (%.3f): success\n", cmd->command, seconds);
        }
        else
            message(LOG_ERROR, "failed to parse sleep time.\n");
    }
    else if (cmd->spec->code == INTERNAL_GETID)
    {
        igtask subtask;
        subtask.command = "get id";
        subtask.isSubTask = true;
        findTaskSpec(&subtask);
        performTask(conn, &subtask);
    }
    else if (cmd->spec->code == INTERNAL_SETID)
    {
        igtask subtask;
        subtask.command = "write block";
        subtask.isSubTask = true;
        findTaskSpec(&subtask);
        subtask.arg = writeBlocks(cmd->arg);
        performTask(conn, &subtask);
    }
    else if ((cmd->spec->code & INTERNAL_GETPINS) == INTERNAL_GETPINS)
    {
        message(LOG_NORMAL, "%s: success: 0x%2.2x\n",
                cmd->command, getSetting(cmd->spec->bit, pinState));
    }
    else if ((cmd->spec->code & INTERNAL_SETPINS) == INTERNAL_SETPINS)
    {
        setSetting(cmd->spec->bit, cmd->arg, pinState);
        message(LOG_NORMAL, "%s: success\n", cmd->command);        
    }
    else
        message(LOG_FATAL, "Unhandled internal task: %s\n", cmd->spec->text);
}

static void performTask(PIPE_PTR conn, igtask *cmd)
{
    if (cmd->spec->internal)
        handleInternalTask(cmd, conn);
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

            /* translate cmd->pins */
            if (parseNumber(cmd->arg, &value))
            {
                if (value > 0xFF)
                    message(LOG_ERROR, "Only %d pins are available, invalid pin specification.\n", IG_PIN_COUNT);
                else
                    result = iguanaPinSpecToData(value, &data);
            }
            break;
        }

        case IG_DEV_SEND:
            /* read pulse data from cmd->arg */
            result = iguanaReadPulseFile(cmd->arg, &data);
            result *= sizeof(uint32_t);
            break;

        case IG_DEV_WRITEBLOCK:
            if (cmd->isSubTask)
            {
                /* this frees data allocated in the caller */
                data = cmd->arg;
                result = 68;
            }
            /* read block from cmd->arg */
            else if (iguanaReadBlockFile(cmd->arg, &data))
                result = 68;
            break;

        case IG_DEV_BULKPINS:
        {
            unsigned int x;
            data = malloc(64);
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
            ((unsigned char*)data)[0] = x * 4;
            
            break;
        }
        }

        if (result < 0)
            message(LOG_ERROR,
                    "Failed to pack data: %s\n", strerror(errno));
        else
        {
            iguanaPacket request = NULL;

            request = iguanaCreateRequest(cmd->spec->code, result, data);
            if (request == NULL)
                message(LOG_ERROR,
                        "Out of memory allocating request.\n");
            else if (! iguanaWriteRequest(request, conn))
                message(LOG_ERROR,
                        "Failed to write request to server.\n");
            else
                receiveResponse(conn, cmd, 10000);

            /* release any allocated data buffers (including
             * data ptr) */
            iguanaFreePacket(request);
        }
    }
}

static void performQueuedTasks(PIPE_PTR conn)
{
    igtask *cmd;

    memset(pinState, 0, IG_PIN_COUNT);
    while((cmd = (igtask*)removeFirstItem(&tasks)) != NULL)
    {
        if (checkTask(cmd))
            performTask(conn, cmd);
        free(cmd);
    }
}

static void enqueueTask(char *text, const char *arg)
{
    igtask *cmd;

    cmd = (igtask*)malloc(sizeof(igtask));
    if (cmd != NULL)
    {
        cmd->command = text;
        cmd->arg = arg;
        insertItem(&tasks, NULL, (itemHeader*)cmd);
    }
}

static void enqueueTaskById(unsigned short code, const char *arg)
{
    unsigned int x;
    for(x = 0; supportedCommands[x].text != NULL; x++)
        if (code == supportedCommands[x].code)
        {
            enqueueTask(supportedCommands[x].text, arg);
            break;
        }

    if (supportedCommands[x].text == NULL)
        message(LOG_FATAL, "enqueueTaskById failed on code %d\n", code);
}



static struct poptOption options[] =
{
    /* general options */
    { "device", 'd', POPT_ARG_STRING, NULL, 'd', "Specify the device to connect with (by index or id).", "number" },
    { "interactive", '\0', POPT_ARG_NONE, NULL, 'i', "Use the client interactively.", NULL },
    { "log-file", 'l', POPT_ARG_STRING, NULL, 'l', "Specify a log file (defaults to \"-\").", "filename" },
    { "quiet", 'q', POPT_ARG_NONE, NULL, 'q', "Reduce the verbosity.", NULL },
    { "verbose", 'v', POPT_ARG_NONE, NULL, 'v', "Increase the verbosity.", NULL },

    /* device commands */
    { "get-version", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETVERSION, "Return the version of the device.", NULL },
    { "send", '\0', POPT_ARG_STRING, NULL, IG_DEV_SEND, "Send the pulses and spaces from a file.", "filename" },
    { "receiver-on", '\0', POPT_ARG_NONE, NULL, IG_DEV_RECVON, "Enable the receiver on the usb device.", NULL },
    { "receiver-off", '\0', POPT_ARG_NONE, NULL, IG_DEV_RECVOFF, "Disable the receiver on the usb device.", NULL },
    { "get-pins", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETPINS, "Get the pin values.", NULL },
    { "set-pins", '\0', POPT_ARG_STRING, NULL, IG_DEV_SETPINS, "Set the pin values.", "values" },
    { "get-buffer-size", '\0', POPT_ARG_NONE, NULL, IG_DEV_GETBUFSIZE, "Find out the size of the RAM buffer used for sends and receives.", NULL },
    { "write-block", '\0', POPT_ARG_STRING, NULL, IG_DEV_WRITEBLOCK, "Write the block specified in the file.", "filename" },
    { "execute", '\0', POPT_ARG_NONE, NULL, IG_DEV_EXECUTE, "Execute code starting at address 0x1fc0 on the device.", "address" },
    { "lcd-text", '\0', POPT_ARG_STRING, NULL, IG_DEV_BULKPINS, "Send a bulk transfer of pin settings to write the argument to an LCD.", "string" },
    { "reset", '\0', POPT_ARG_NONE, NULL, IG_DEV_RESET, "Reset the USB device.", NULL },

    /* commands that actually store and load the pin configuration */
    { "get-pin-config", '\0', POPT_ARG_NONE, NULL, INTERNAL_GETPINS, "Retrieve the internal pin state.", NULL },
    { "set-pin-config", '\0', POPT_ARG_NONE, NULL, INTERNAL_SETPINS, "Store the internal pin state.", NULL },

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
    { "get-id", '\0', POPT_ARG_NONE, NULL, INTERNAL_GETID, "Fetch the unique id from the USB device.", NULL },
    { "set-id", '\0', POPT_ARG_STRING, NULL, INTERNAL_SETID, "Set the unique id from the USB device.", NULL },

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
        case IG_DEV_SEND:
        case IG_DEV_RECVON:
        case IG_DEV_RECVOFF:
        case IG_DEV_GETPINS:
        case IG_DEV_SETPINS:
        case IG_DEV_GETBUFSIZE:
        case IG_DEV_WRITEBLOCK:
        case IG_DEV_EXECUTE:
        case IG_DEV_BULKPINS:
        case IG_DEV_RESET:

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
        case INTERNAL_GETID:
        case INTERNAL_SETID:
            enqueueTaskById(x, poptGetOptArg(poptCon));
            break;

        case INTERNAL_GETPINS:
            enqueueTask("get config 0", NULL);
            enqueueTask("get config 1", NULL);
            break;

        case INTERNAL_SETPINS:
            enqueueTask("set config 0", NULL);
            enqueueTask("set config 1", NULL);
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

    /* line buffer the output */
    setlinebuf(stdout);
    setlinebuf(stderr);

    /* connect first */
    conn = iguanaConnect(device);
    if (conn == INVALID_PIPE)
        message(LOG_ERROR,
                "Failed to connect to iguanaIR daemon: %s\n", strerror(errno));
    else
    {
        igtask cmd;

        /* handle all requests */
        if (interactive)
        {
            do
            {
                char line[512], *argument;

                performQueuedTasks(conn);
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
            } while(1);

            retval = 0;
        }
        else if (firstItem(&tasks) == NULL)
            message(LOG_ERROR, "No tasks specified.\n");
        else
        {
            performQueuedTasks(conn);
            retval = 0;
        }

        cmd.command = "final check";
        findTaskSpec(&cmd);
        receiveResponse(conn, &cmd, 0);
    }

    if (conn >= 0)
        iguanaClose(conn);

    return retval;
}
