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
#include "../version.h"
#include <argp.h>
#include "compat.h"

#include <windows.h>
#include <stdio.h>
#include <aclapi.h>
#include <setupapi.h>
#include <dbt.h>
#include <initguid.h>

#include "driver.h"
#include "pipes.h"
#include "support.h"
#include "device-interface.h"
#include "client-interface.h"
#include "server.h"

#define SERVICE_NAME "igdaemon"

/* iguana local variables */
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
		// translate the requested access as necessary
		unsigned int access = action;
		if (access == SERVICE_CONTROL_PAUSE ||
			access == SERVICE_CONTROL_CONTINUE)
			access = SERVICE_PAUSE_CONTINUE;

		// open a handle to the specific service
        svc = OpenService(scm, "igdaemon", access);
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

			case SERVICE_CONTROL_PAUSE:
                if (ControlService(svc, SERVICE_CONTROL_PAUSE, &status) == FALSE)
                    message(LOG_ERROR, "Failed to pause the service: %s\n",
                            translateError(GetLastError()));
                else
                    retval = true;
                break;

			case SERVICE_CONTROL_CONTINUE:
                if (ControlService(svc, SERVICE_CONTROL_CONTINUE, &status) == FALSE)
                    message(LOG_ERROR, "Failed to continue the service: %s\n",
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
	ARG_RESCAN = LAST_BASE_ARG
};

static struct argp_option options[] =
{
    /* Windows specific stuff for controlling the service */
    { NULL, 0, NULL, 0, "Windows specific options:" },
    { "regsvc",     'r',        NULL, 0, "Register this executable as the system igdaemon service.",   GROUP0 },
    { "unregsvc",   'u',        NULL, 0, "Remove the system igdaemon service.",                        GROUP0 },
    { "startsvc",   's',        NULL, 0, "Start the system igdaemon service.",                         GROUP0 },
    { "stopsvc",    't',        NULL, 0, "Stop the system igdaemon service.",                          GROUP0 },

    { "rescan",     ARG_RESCAN, NULL, 0, "Trigger a rescan by pausing and then resuming the service.", GROUP0 },
    { "installinf", 'i',        NULL, 0, "Manually install the INF for the device.",                   GROUP0 },

    /* end of table */
    {0}
};

static error_t parseOption(int key, char *arg, struct argp_state *state)
{
	int *retval = (int*)state->input;
    switch(key)
    {
    case 'r':
        if (registerWithSCM())
            *retval = 0;
		else
	        *retval = 1;
		break;

    case 's':
        if (changeServiceState(SERVICE_START))
            *retval = 0;
		else
	        *retval = 1;
		break;

    case 't':
        if (changeServiceState(SERVICE_STOP))
            *retval = 0;
		else
	        *retval = 1;
		break;

    case ARG_RESCAN:
        if (changeServiceState(SERVICE_CONTROL_PAUSE) &&
            changeServiceState(SERVICE_CONTROL_CONTINUE))
            *retval = 0;
		else
	        *retval = 1;
		break;

    case 'u':
        if (changeServiceState(DELETE))
            *retval = 0;
		else
	        *retval = 1;
		break;

    case 'i':
        if (installINF())
            *retval = 0;
		else
	        *retval = 1;
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
    "This daemon acts as a user-space driver controlling access to IguanaIR devices.\n",
    NULL,
    NULL,
    NULL
};

static void waitCommPipe(HANDLE stopEvent)
{
    while(stopEvent == NULL || WaitForSingleObject(stopEvent, 0) == WAIT_TIMEOUT)
    {
        THREAD_PTR child = INVALID_THREAD_PTR;
        if (readPipeTimed(srvSettings.commPipe[READ], (char*)&child, sizeof(THREAD_PTR), 250) == sizeof(THREAD_PTR))
        {
            void *exitVal;
            joinThread(child, &exitVal);
        }
        else if (errno != ERROR_TIMEOUT)
            message(LOG_ERROR, "Failure reading from commPipe in service\n");
    }
}

int main(int argc, char **argv)
{
	int retval = -1;
    struct argp_child children[2];

	/* initialize the settings for the server process */
    InitializeCriticalSection(&aliasLock);
    initServerSettings(startWorker);

    /* grab the base arguments from server.c */
    memset(children, 0, sizeof(struct argp_child) * 2);
    children[0].argp = baseArgParser();
    children[0].group = 0;
    parser.children = children;

    /* parse the cmd line args */
    argp_parse(&parser, argc, argv, 0, NULL, &retval);
	if (retval != -1)
		return retval;

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

                /* block waiting for commPipe commands */
                waitCommPipe(NULL);
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
        waitCommPipe(service_stop_event);
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
        if (! updateDeviceList(list))
            message(LOG_ERROR, "scan failed.\n");
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
static void startOverlappedAction(PIPE_PTR fd, OVERLAPPED *over, bool connect)
{
    if (over->hEvent == NULL)
        over->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    else
        ResetEvent(over->hEvent);

    if (connect)
        ConnectNamedPipe(fd, over);
    else
        ReadFile(fd, NULL, 0, NULL, over);
}

/* listen to clients connecting to either name or alias, and the idev->reader */
void listenToClients(iguanaDev *idev)
{
    HANDLE *handles = NULL;
    OVERLAPPED over[3];
    int x;
    client *john;
    bool firstPass = true;

    /* get any intial ID from the device */
    getID(idev);

    /* clear the overlapped structures */
    memset(over, 0, sizeof(OVERLAPPED) * 3);
    while(true)
    {
        int count = 1;

        /* allocate the handles and overlap objects that I need to populate */
        handles = (HANDLE*)realloc(handles, sizeof(HANDLE) * (1 + 2 + idev->clientList.count));
        if (firstPass)
		{
            startOverlappedAction(idev->readerPipe[READ], over + 0, false);
            handles[0] = over[0].hEvent;
		}

        /* next add events for the named pipes */
        for(x = 0; x < 2; x++)
        {
            /* handle the case there there is no alias */
            if (x == 1 && aliases[idev->usbDev->id][0] == '\0')
            {
                handles[x + 1] = NULL;
                break;
            }

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
                if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
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
                pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, 
                                                       SECURITY_DESCRIPTOR_MIN_LENGTH);
                if (NULL == pSD) 
                { 
                    message(LOG_ERROR, "LocalAlloc Error %s\n",
                            translateError(GetLastError()));
                    goto Cleanup; 
                } 
             
                if (! InitializeSecurityDescriptor(pSD,
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
                sa.nLength = sizeof(SECURITY_ATTRIBUTES);
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
                startOverlappedAction(listeners[idev->usbDev->id][x], over + x + 1, true);
                handles[x + 1] = over[x + 1].hEvent;

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
			/* check if an existing overlapped action is still running */
			bool pending = false;
			DWORD bytes;
			if (! GetOverlappedResult(john->fd, &john->over, &bytes, FALSE))
				switch(GetLastError())
				{
				case ERROR_IO_INCOMPLETE:
				/* docs say error should be ERROR_IO_INCOMPLETE, but results say otherwise */
				case ERROR_IO_PENDING:
					pending = true;
					break;

				default:
					break;
				}

			/* Start a new one if need be */
			if (! pending)
			{
				startOverlappedAction(john->fd, &john->over, false);
				handles[count++] = john->over.hEvent;
			}
		}

        /* wait for something to happen */
        WaitForMultipleObjects(count, handles, FALSE, INFINITE); 

        /* handle the reader thread sending us things, and on failure quit */
        if (WaitForSingleObject(handles[0], 0) == WAIT_OBJECT_0)
        {
            if (! handleReader(idev))
                break;

            /* Prepare for the next pass */
            startOverlappedAction(idev->readerPipe[READ], over + 0, false);
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
                CloseHandle(over[x + 1].hEvent);
                over[x + 1].hEvent = NULL;
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
            event = john->over.hEvent;
            if (event != NULL &&
                WaitForSingleObject(event, 0) == WAIT_OBJECT_0 &&
                ! handleClient(john))
                CloseHandle(event);

            john = next;
        }
        firstPass = false;
    }

    /* make sure to close the system level handles (reader and 1 or 2 named pipes) */
    for(x = 0; x < 1 + 2; x++)
        if (handles[x] != NULL)
            CloseHandle(handles[x]);
    free(handles);

    /* and release any connected clients */
    while(idev->clientList.count > 0)
    {
        client *john = (client*)idev->clientList.head;
        if (john->over.hEvent != NULL)
            CloseHandle(john->over.hEvent);
        releaseClient(john);
    }

	/* "disconnect" and release the unconnected pipes */
    EnterCriticalSection(&aliasLock);
    for(x = 0; x < 2; x++)
        if (listeners[idev->usbDev->id][x] != NULL)
		{
			// crashes without the disconnect call here
			DisconnectNamedPipe(listeners[idev->usbDev->id][x]);
            CloseHandle(listeners[idev->usbDev->id][x]);
			listeners[idev->usbDev->id][x] = NULL;
		}
    LeaveCriticalSection(&aliasLock);
}

// TODO: only allows for 1 alias, but alright
void setAlias(iguanaDev *idev, bool deleteAll, const char *alias)
{
    EnterCriticalSection(&aliasLock);
    if (listeners[idev->usbDev->id][1] != NULL)
    {
        CloseHandle(listeners[idev->usbDev->id][1]);
        listeners[idev->usbDev->id][1] = NULL;
    }
    if (alias != NULL)
        strcpy(aliases[idev->usbDev->id], alias);
	else
		aliases[idev->usbDev->id][0] = '\0';
    LeaveCriticalSection(&aliasLock);
}
