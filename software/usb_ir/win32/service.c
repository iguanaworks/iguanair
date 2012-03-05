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
#include <aclapi.h>
#include <setupapi.h>
#include <dbt.h>
#include <initguid.h>
#include <popt.h>
#include "popt-fix.h"

#include "driver.h"
#include "pipes.h"
#include "support.h"
#include "device-interface.h"
#include "client-interface.h"
#include "server.h"

#define SERVICE_NAME "igdaemon"

/* iguana local variables */
static PIPE_PTR commPipe[2];
static deviceList *list;

/* we keep a global list of aliases and  */
#define MAX_DEVICES 64
char aliases[MAX_DEVICES][MAX_PATH] = {{'\0'}};
HANDLE listeners[MAX_DEVICES][2] = {{NULL, NULL}};
CRITICAL_SECTION aliasLock;

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
        message(LOG_ERROR, "Failed to open the SCM: %s\n", translateError(GetLastError()));
    else
    {
        svc = OpenService(scm, "igdaemon", action);
        if (svc == NULL)
            message(LOG_ERROR, "Failed to open the service: %s\n",
                    translateError(GetLastError()));
        else
        {
            SERVICE_STATUS status;

            switch(action)
            {
            case SERVICE_START:
                if (StartService(svc, 0, NULL) == FALSE)
                    message(LOG_ERROR, "Failed to start the service: %s\n",
                            translateError(GetLastError()));
                else
                    retval = true;
                break;

            case SERVICE_STOP:
                if (ControlService(svc, SERVICE_CONTROL_STOP, &status) == FALSE)
                    message(LOG_ERROR, "Failed to stop the service: %s\n",
                            translateError(GetLastError()));
                else
                    retval = true;
                break;

            case DELETE:
                if (DeleteService(svc) == FALSE)
                    message(LOG_ERROR, "Failed to delete the service: %s\n",
                            translateError(GetLastError()));
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

enum
{
    /* generic actions */
    ARG_LOG_FILE,
    ARG_QUIETER,
    ARG_LOUDER,
    ARG_LOG_LEVEL,

    /* igdaemon specific actions */
    ARG_FOREGROUND,
    ARG_PID_FILE,
    ARG_NO_IDS,
    ARG_NO_RESCAN,
    ARG_NO_THREADS,
    ARG_DRIVER,
    ARG_ONLY_PREFER,
    ARG_DRIVER_DIR
};

static struct poptOption options[] =
{
    { "log-file", 'l', POPT_ARG_STRING, NULL, 'l', "Specify a log file (defaults to \"-\").", "filename" },
    { "quiet", 'q', POPT_ARG_NONE, NULL, 'q', "Reduce the verbosity.", NULL },
    { "verbose", 'v', POPT_ARG_NONE, NULL, 'v', "Increase the verbosity.", NULL },

    /* iguanaworks specific stuff */
    { "no-auto-rescan", '\0', POPT_ARG_NONE, NULL, 'n', "Do not automatically rescan the USB bus after a device disconnect.", NULL },
    { "no-ids", '\0', POPT_ARG_NONE, NULL, 'b', "Do not query the iguanaworks device for its label.  Try this if fetching the label hangs.", NULL },
    { "no-labels", '\0', POPT_ARG_NONE, NULL, 'b', "DEPRECATED: same as --no-ids", NULL },

    /* Windows specific stuff for controlling the service */
    { "regsvc", 0, POPT_ARG_NONE, NULL, 'r', "Register this executable as the system igdaemon service.", NULL },
    { "unregsvc", 0, POPT_ARG_NONE, NULL, 'u', "Remove the system igdaemon service.", NULL },
    { "startsvc", 0, POPT_ARG_NONE, NULL, 's', "Start the system igdaemon service.", NULL },
    { "stopsvc", 0, POPT_ARG_NONE, NULL, 't', "Stop the system igdaemon service.", NULL },
    { "installinf", 0, POPT_ARG_NONE, NULL, 'i', "Manually install the INF for the device.", NULL },

    /* options specific to the drivers */
    { "driver", 'd', POPT_ARG_STRING, NULL, ARG_DRIVER, "Use this driver in preference to others.  This command can be used multiple times.", "preferred driver" },
    { "only-preferred", '\0', POPT_ARG_NONE, NULL, ARG_ONLY_PREFER, "Use only drivers specified by the --driver option.", "only preferred drivers" },
    { "driver-dir", '\0', POPT_ARG_STRING, NULL, ARG_DRIVER_DIR, "Specify the location of driver objects.", "driver directory" },

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

    /* initialize the settings for the server process */
    InitializeCriticalSection(&aliasLock);
    initServerSettings(startWorker);

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

        case 'n':
            autoRescan = false;
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

        /* driver options */
        case ARG_DRIVER:
            srvSettings.preferred = (const char**)realloc((void*)srvSettings.preferred, sizeof(char*) * (srvSettings.preferredCount + 1));
            srvSettings.preferred[srvSettings.preferredCount - 1] = poptGetOptArg(poptCon);
            srvSettings.preferred[srvSettings.preferredCount++] = NULL;
            break;

        case ARG_ONLY_PREFER:
            srvSettings.onlyPreferred = true;
            break;

        case ARG_DRIVER_DIR:
            srvSettings.driverDir = poptGetOptArg(poptCon);
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

    if ((list = initServer()) == NULL)
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
                if (! updateDeviceList(list))
                    message(LOG_ERROR, "scan failed.\n");

                /* just block indefinitely */
                fscanf(stdin, "\n");
            }
            else
                return error;
        }
    }

    DeleteCriticalSection(&aliasLock);
    return 0;
}

/* this function is the main function for the windows service */
static void WINAPI serviceMain(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

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
        if (! updateDeviceList(list))
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
    UNUSED(context);
    UNUSED(event_data);

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
            ! updateDeviceList(list))
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
void listenToClients(iguanaDev *idev,
                     handleReaderFunc handleReader,
                     clientConnectedFunc clientConnected,
                     handleClientFunc handleClient)
{
    HANDLE *handles = NULL;
    OVERLAPPED over;
    int x;
    bool firstPass = true;

    /* get any intial ID from the device */
    getID(idev);

    memset(&over, 0, sizeof(OVERLAPPED));
    while(true)
    {
        int count = 1;
        client *john;

        /* allocate the handles and overlap objects that I need to populate */
        handles = realloc(handles, sizeof(HANDLE) * (1 + 2 + idev->clientList.count));
        if (firstPass)
            handles[0] = startOverlappedAction(idev->readerPipe[READ], NULL, false);

        /* next add events for the named pipes */
        for(x = 0; x < 2; x++)
        {
            /* handle the case there there is no alias */
            if (x == 1 && aliases[idev->usbDev->id][0] == '\0')
                break;

            EnterCriticalSection(&aliasLock);
            if (firstPass || listeners[idev->usbDev->id][x] == NULL)
            {
                char path[PATH_MAX], name[4];
                PSID pEveryoneSID = NULL, pAdminSID = NULL;
                PACL pACL = NULL;
                PSECURITY_DESCRIPTOR pSD = NULL;
                EXPLICIT_ACCESS ea[2];
                SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
                SECURITY_ATTRIBUTES sa;
                HKEY hkSub = NULL;

                // Create a well-known SID for the Everyone group.
                if(!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                                 SECURITY_WORLD_RID,
                                 0, 0, 0, 0, 0, 0, 0,
                                 &pEveryoneSID))
                {
                    message(LOG_ERROR, "AllocateAndInitializeSid Error %s\n",
                            translateError(GetLastError()));
                    goto Cleanup;
                }

                // Initialize an EXPLICIT_ACCESS structure for an ACE.
                // The ACE will allow Everyone read/write access to the pipe
                ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESS));
                ea[0].grfAccessPermissions = FILE_GENERIC_WRITE | FILE_GENERIC_READ;
                ea[0].grfAccessMode = SET_ACCESS;
                ea[0].grfInheritance= NO_INHERITANCE;
                ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
                ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
                ea[0].Trustee.ptstrName  = (LPTSTR) pEveryoneSID;

                // Create a new ACL that contains the new ACEs.
                if (ERROR_SUCCESS != SetEntriesInAcl(1, ea, NULL, &pACL))
                {
                    message(LOG_ERROR, "SetEntriesInAcl Error %s\n",
                            translateError(GetLastError()));
                    goto Cleanup;
                }

                // Initialize a security descriptor.  
                pSD = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, 
                             SECURITY_DESCRIPTOR_MIN_LENGTH); 
                if (NULL == pSD) 
                { 
                    message(LOG_ERROR, "LocalAlloc Error %s\n",
                            translateError(GetLastError()));
                    goto Cleanup; 
                } 
             
                if (!InitializeSecurityDescriptor(pSD,
                        SECURITY_DESCRIPTOR_REVISION)) 
                {  
                    message(LOG_ERROR, "InitializeSecurityDescriptor Error %s\n",
                            translateError(GetLastError()));
                    goto Cleanup; 
                } 
             
                // Add the ACL to the security descriptor. 
                if (!SetSecurityDescriptorDacl(pSD, 
                        TRUE,     // bDaclPresent flag   
                        pACL, 
                        FALSE))   // not a default DACL 
                {  
                    message(LOG_ERROR, "SetSecurityDescriptorDacl Error %s\n",
                            translateError(GetLastError()));
                    goto Cleanup; 
                } 

                // Initialize a security attributes structure.
                sa.nLength = sizeof (SECURITY_ATTRIBUTES);
                sa.lpSecurityDescriptor = pSD;
                sa.bInheritHandle = FALSE;

                /* prepare the name/path variables */
                switch(x)
                {
                default:
                case 0:
                    sprintf(name, "%d", idev->usbDev->id);
                    socketName(name, path, PATH_MAX);
                    break;

                case 1:
                    socketName(aliases[idev->usbDev->id], path, PATH_MAX);
                    break;
                }

                // Use the security attributes to set the security descriptor
                listeners[idev->usbDev->id][x] = CreateNamedPipe(path,
                                                       PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                                       PIPE_UNLIMITED_INSTANCES,
                                                       64, 64, NMPWAIT_USE_DEFAULT_WAIT, &sa);
                handles[count] = startOverlappedAction(listeners[idev->usbDev->id][x], NULL, true);

            Cleanup:
                if (pEveryoneSID) 
                    FreeSid(pEveryoneSID);
                if (pAdminSID) 
                    FreeSid(pAdminSID);
                if (pACL) 
                    LocalFree(pACL);
                if (pSD) 
                    LocalFree(pSD);
                if (hkSub) 
                    RegCloseKey(hkSub);
            }
            LeaveCriticalSection(&aliasLock);
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
        EnterCriticalSection(&aliasLock);
        for(x = 0; x < 2; x++)
        {
            /* handle the case there there is no alias */
            if (x == 1 && aliases[idev->usbDev->id][0] == '\0')
                break;

            if (WaitForSingleObject(handles[x + 1], 0) == WAIT_OBJECT_0)
            {
                clientConnected(listeners[idev->usbDev->id][x], idev);

                /* create a new pipe instance on the next pass */
                CloseHandle(handles[x + 1]);
                listeners[idev->usbDev->id][x] = NULL;
            }
        }
        LeaveCriticalSection(&aliasLock);

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

	// "disconnect" and release the unconnected pipes
    EnterCriticalSection(&aliasLock);
    for(x = 0; x < 2; x++)
        if (listeners[idev->usbDev->id][x] != NULL)
		{
			// crashes without the disconnect call here
			DisconnectNamedPipe(listeners[idev->usbDev->id][x]);
            CloseHandle(listeners[idev->usbDev->id][x]);
		}
    LeaveCriticalSection(&aliasLock);
}

void setAlias(unsigned int id, const char *alias)
{
    EnterCriticalSection(&aliasLock);
    if (listeners[id][1] != NULL)
    {
        CloseHandle(listeners[id][1]);
        listeners[id][1] = NULL;
    }
    if (alias != NULL)
        strcpy(aliases[id], alias);
	else
		aliases[id][0] = '\0';
    LeaveCriticalSection(&aliasLock);
}
