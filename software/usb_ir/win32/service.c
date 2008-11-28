/**************************************************************************
 * service.c **************************************************************
 **************************************************************************
 *
 * TODO: DESCRIBE AND DOCUMENT THIS FILE
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include "iguanaIR.h"
#include "compat.h"

#include <windows.h>
#include <stdio.h>
#include <setupapi.h>
#include <dbt.h>
#include <initguid.h>
#include <popt.h>
#include "popt-fix.h"

#include "usbclient.h"
#include "pipes.h"
#include "support.h"
#include "device-interface.h"
#include "client-interface.h"

#define SERVICE_NAME "igdaemon"

/* iguana local variables */
static usbId ids[] = {
    {0x1781, 0x0938}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};
static PIPE_PTR commPipe[2];
#ifdef LIBUSB_NO_THREADS
  static unsigned int recvTimeout = 100;
#else
  static unsigned int recvTimeout = 1000;
#endif
static unsigned int sendTimeout = 1000;
static usbDeviceList list;

/* guids for registering for device notifications */
DEFINE_GUID(GUID_DEVINTERFACE_USB_HUB, 0xf18a0e88, 0xc30c, 0x11d0, 0x88,
            0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8);
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2,
            0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

/* global variables concerning our server state */
static SERVICE_STATUS service_status;
static SERVICE_STATUS_HANDLE service_status_handle;
static HDEVNOTIFY notification_handle_hub, notification_handle_dev;
static HANDLE service_stop_event;

/* just prototypes */
static void WINAPI serviceMain(int argc, char **argv);
static DWORD WINAPI serviceHandler(DWORD code, DWORD event_type,
                                   VOID *event_data, VOID *context);
static void setServiceStatus(DWORD status, DWORD exit_code);
static void registerNotifications(void);
static void unregisterNotifications(void);

static bool registerWithSCM()
{
    bool retval = false;
    SC_HANDLE scm, svc;

    scm = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL)
        message(LOG_ERROR, "Failed to open the SCM: %d\n", GetLastError());
    else
    {
        char path[MAX_PATH + 2];
        GetModuleFileName(NULL, path + 1, MAX_PATH);
        path[0] = '"';
        strcat(path, "\"");

        svc = CreateService(scm, "igdaemon", "Iguanaworks IR Daemon",
                            0, SERVICE_WIN32_OWN_PROCESS,
                            SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path,
                            NULL, NULL, NULL, NULL, "");
        if (svc == NULL)
            message(LOG_ERROR, "Failed to create the service: %d\n", GetLastError());
        else
        {
            retval = true;
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }

    return retval;
}

static bool changeServiceState(unsigned int action)
{
    bool retval = false;
    SC_HANDLE scm, svc;

    scm = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL)
        message(LOG_ERROR, "Failed to open the SCM: %d\n", GetLastError());
    else
    {
        svc = OpenService(scm, "igdaemon", action);
        if (svc == NULL)
            message(LOG_ERROR, "Failed to open the service: %d\n", GetLastError());
        else
        {
            SERVICE_STATUS status;

            switch(action)
            {
            case SERVICE_START:
                if (StartService(svc, 0, NULL) == FALSE)
                    message(LOG_ERROR, "Failed to start the service: %d\n", GetLastError());
                else
                    retval = true;
                break;

            case SERVICE_STOP:
                if (ControlService(svc, SERVICE_CONTROL_STOP, &status) == FALSE)
                    message(LOG_ERROR, "Failed to start the service: %d\n", GetLastError());
                else
                    retval = true;
                break;

            case DELETE:
                if (DeleteService(svc) == FALSE)
                    message(LOG_ERROR, "Failed to delete the service: %d\n", GetLastError());
                else
                    retval = true;
                break;
            }
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }

    return retval;
}

static bool installINF()
{
    char path[MAX_PATH], dir[MAX_PATH], *temp;
    GetModuleFileName(NULL, path, MAX_PATH);

    temp = strrchr(path, '\\');
    if (temp != NULL)
    {
        temp[1] = '\0';
        strcpy(dir, path);
        strcat(path, "iguanaIR.inf");
        if (SetupCopyOEMInf(path, dir, SPOST_PATH, 0,
                            NULL, 0, NULL, NULL))
            return true;
    }

    return false;
}

static struct poptOption options[] =
{
    { "log-file", 'l', POPT_ARG_STRING, NULL, 'l', "Specify a log file (defaults to \"-\").", "filename" },
    { "quiet", 'q', POPT_ARG_NONE, NULL, 'q', "Reduce the verbosity.", NULL },
    { "verbose", 'v', POPT_ARG_NONE, NULL, 'v', "Increase the verbosity.", NULL },

    /* iguanaworks specific stuff */
    { "no-labels", 0, POPT_ARG_NONE, NULL, 'b', "Do not query the Iguanaworks device for its label.", NULL },

    /* Windows specific stuff for controlling the service */
    { "regsvc", 0, POPT_ARG_NONE, NULL, 'r', "Register this executable as the system igdaemon service.", NULL },
    { "unregsvc", 0, POPT_ARG_NONE, NULL, 'u', "Remove the system igdaemon service.", NULL },
    { "startsvc", 0, POPT_ARG_NONE, NULL, 's', "Start the system igdaemon service.", NULL },
    { "stopsvc", 0, POPT_ARG_NONE, NULL, 't', "Stop the system igdaemon service.", NULL },
    { "installinf", 0, POPT_ARG_NONE, NULL, 'i', "Stop the system igdaemon service.", NULL },

    POPT_TABLEEND
};

static void exitOnOptError(poptContext poptCon, char *msg)
{
    message(LOG_ERROR, msg, poptBadOption(poptCon, 0));
    poptPrintHelp(poptCon, stderr, 0);
    exit(1);
}

int main(int argc, char **argv)
{
    const char **leftOvers, *temp;
    int x = 0;
    poptContext poptCon;

    temp = strrchr(argv[0], '\\');
    if (temp == NULL)
        programName = argv[0];
    else
        programName = temp + 1;

    poptCon = poptGetContext(NULL, argc, argv, options, 0);
    while(x != -1)
    {
        switch(x = poptGetNextOpt(poptCon))
        {
        case 'l':
            openLog(poptGetOptArg(poptCon));
            break;

        case 'q':
            changeLogLevel(-1);
            break;

        case 'v':
            changeLogLevel(+1);
            break;

        case 'b':
            readLabels = false;
            break;

        case 'r':
            if (registerWithSCM())
                return 0;
            return 1;

        case 's':
            if (changeServiceState(SERVICE_START))
                return 0;
            return 1;

        case 't':
            if (changeServiceState(SERVICE_STOP))
                return 0;
            return 1;

        case 'u':
            if (changeServiceState(DELETE))
                return 0;
            return 1;

        case 'i':
            if (installINF())
                return 0;
            return 1;

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

    if (! initDeviceList(&list, ids, recvTimeout, sendTimeout, startWorker))
        message(LOG_ERROR, "failed to initialize device list.\n");
    else
    {
        SERVICE_TABLE_ENTRY dispatch_table[] =
        {
            { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)serviceMain },
            { NULL, NULL }
        };

        //openLog("c:\\igdaemon.txt");
        if (StartServiceCtrlDispatcher(dispatch_table) == FALSE)
        {
            DWORD error;
            error = GetLastError();
            /* we could be running as a console application */
            if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            {
                /* populate the device list once */
                if (! updateDeviceList(&list))
                    message(LOG_ERROR, "scan failed.\n");

                /* just block indefinitely */
                fscanf(stdin, "\n");
            }
            else
                return error;
        }
    }
    return 0;
}

/* this function is the main function for the windows service */
static void WINAPI serviceMain(int argc, char **argv)
{
    message(LOG_NORMAL, "igdaemon serviceMain started\n");

    /* open a handle to our server status */
    memset(&service_status, 0, sizeof(service_status));
    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_START_PENDING;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                        SERVICE_ACCEPT_PAUSE_CONTINUE;
    service_status.dwWaitHint = 5000;

    service_status_handle = RegisterServiceCtrlHandlerEx(SERVICE_NAME,
                                                         serviceHandler,
                                                         NULL);
    if (!service_status_handle)
        return;

    /* notify the SCM that we're starting */
    setServiceStatus(SERVICE_START_PENDING, NO_ERROR);

    /* create and watch the stop event */
    service_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (service_stop_event)
    {
        /* register for plug notifications */
        registerNotifications();

        /* populate the device list as soon as we have notifications */
        if (! updateDeviceList(&list))
            message(LOG_ERROR, "scan failed.\n");

        /* tell SCM about our progress and wait for death */
        setServiceStatus(SERVICE_RUNNING, NO_ERROR);
        while(WaitForSingleObject(service_stop_event, INFINITE));
        CloseHandle(service_stop_event);
    }

    /* tell the SCM that we've stopped */
    setServiceStatus(SERVICE_STOPPED, NO_ERROR);
    return;
}

static DWORD WINAPI serviceHandler(DWORD code, DWORD event_type,
                                   VOID *event_data, VOID *context)
{
    switch(code)
    {
    case SERVICE_CONTROL_STOP:
        setServiceStatus(SERVICE_STOP_PENDING, NO_ERROR);
        unregisterNotifications();
        if(service_stop_event)
            SetEvent(service_stop_event);
        break;

    case SERVICE_CONTROL_DEVICEEVENT:
        /* receive one of these on device plug */
        if (event_type == DBT_DEVICEARRIVAL &&
            ! updateDeviceList(&list))
            message(LOG_ERROR, "scan failed.\n");
        break;

    case SERVICE_CONTROL_PAUSE:
        setServiceStatus(SERVICE_PAUSE_PENDING, NO_ERROR);
        unregisterNotifications();
        setServiceStatus(SERVICE_PAUSED, NO_ERROR);
        break;

    case SERVICE_CONTROL_CONTINUE:
        setServiceStatus(SERVICE_CONTINUE_PENDING, NO_ERROR);
        registerNotifications();
        setServiceStatus(SERVICE_RUNNING, NO_ERROR);
        break;

    default:
        message(LOG_ERROR, "Unknown code sent to service handler: %d\n", code);
        break;
    }

    return NO_ERROR;
}

static void setServiceStatus(DWORD status, DWORD exit_code)
{
    service_status.dwCurrentState  = status;
    service_status.dwWin32ExitCode = exit_code;
    SetServiceStatus(service_status_handle, &service_status);
}

/* TODO: need an application with an event loop to have these work outside the service setting */
static void registerNotifications(void)
{
    DEV_BROADCAST_DEVICEINTERFACE dev_if;
    dev_if.dbcc_size = sizeof(dev_if);
    dev_if.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_HUB;
    notification_handle_hub = RegisterDeviceNotification((HANDLE)service_status_handle, &dev_if,
                                                         DEVICE_NOTIFY_SERVICE_HANDLE);

    dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;
    notification_handle_dev = RegisterDeviceNotification((HANDLE)service_status_handle, &dev_if,
                                                         DEVICE_NOTIFY_SERVICE_HANDLE);
}

static void unregisterNotifications(void)
{
    if(notification_handle_hub)
        UnregisterDeviceNotification(notification_handle_hub);
    if(notification_handle_dev)
        UnregisterDeviceNotification(notification_handle_dev);
}

/* helper function for listenToClients */
static HANDLE startOverlappedAction(PIPE_PTR fd, HANDLE event, bool connect)
{
    OVERLAPPED over = { (ULONG_PTR)NULL };

    if (event == NULL)
        event = CreateEvent(NULL, TRUE, FALSE, NULL);
    else
        ResetEvent(event);
    over.hEvent = event;

    if (connect)
        ConnectNamedPipe(fd, &over);
    else
        ReadFile(fd, NULL, 0, NULL, &over);

    return event;
}

/* list to clients connecting to either name or alias, and the idev->reader */
void listenToClients(char *name, char *alias, iguanaDev *idev,
                     handleReaderFunc handleReader,
                     clientConnectedFunc clientConnected,
                     handleClientFunc handleClient)
{
    HANDLE *handles = NULL, listeners[2] = {NULL,NULL};
    OVERLAPPED over;
    char names[2][256];
    int numNames = 1, x;
    bool firstPass = true;

    /* create an array of names to make life simpler */
    socketName(name, names[0], 256);
    if (alias != NULL && alias[0] != '\0')
    {
        socketName(alias, names[1], 256);
        numNames++;
    }

    memset(&over, 0, sizeof(OVERLAPPED));
    while(true)
    {
        int count = 1;
        client *john;

        /* allocate the handles and overlap objects that I need to populate */
        handles = realloc(handles, sizeof(HANDLE) * (1 + numNames + idev->clientList.count));
        if (firstPass)
            handles[0] = startOverlappedAction(idev->readerPipe[READ], NULL, false);

        /* next add events for the named pipes */
        for(x = 0; x < numNames; x++)
        {
            if (firstPass || listeners[x] == NULL)
            {
                listeners[x] = CreateNamedPipe(names[x],
                                               PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                               PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                               PIPE_UNLIMITED_INSTANCES,
                                               64, 64, NMPWAIT_USE_DEFAULT_WAIT, NULL);
                handles[count] = startOverlappedAction(listeners[x], NULL, true);
            }
            count++;
        }

        /* list the current clients last */
        for(john = (client*)idev->clientList.head; john != NULL; john = (client*)john->header.next)
        {
            john->listenData = startOverlappedAction(john->fd, john->listenData, false);
            handles[count++] = john->listenData;
        }

        /* wait for something to happen */
        WaitForMultipleObjects(count, handles, FALSE, INFINITE); 

        /* handle the reader thread sending us things, and on failure quit */
        if (WaitForSingleObject(handles[0], 0) == WAIT_OBJECT_0)
        {
            if (! handleReader(idev))
                break;

            /* Prepare for the next pass */
            ResetEvent(handles[0]);
            startOverlappedAction(idev->readerPipe[READ], handles[0], false);
        }

        /* now accept new clients */
        for(x = 0; x < numNames; x++)
            if (WaitForSingleObject(handles[x + 1], 0) == WAIT_OBJECT_0)
            {
                clientConnected(listeners[x], idev);
                /* create a new pipe instance on the next pass */
                CloseHandle(handles[x + 1]);
                listeners[x] = NULL;
            }

        /* last, handle existing clients */
        for(john = (client*)idev->clientList.head; john != NULL;)
        {
            HANDLE event;
            client *next;

            next = (client*)john->header.next;
            event = john->listenData;
            if (event != NULL &&
                WaitForSingleObject(event, 0) == WAIT_OBJECT_0 &&
                ! handleClient(john))
                CloseHandle(event);

            john = next;
        }
        firstPass = false;
    }
    free(handles);

    for(x = 0; x < numNames; x++)
        if (listeners[x] != NULL)
            CloseHandle(listeners[x]);
}
