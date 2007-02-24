#include <windows.h>
#include <stdio.h>

#include <dbt.h>
#include <initguid.h>


#include "iguanaIR.h"
#include "base.h"
#include "pipes.h"
#include "support.h"
#include "protocol.h"
#include "iguanadev.h"
#include "igdaemon.h"
#include "usbclient.h"

#define SERVICE_NAME "igdaemon"

/* local variables */
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








DEFINE_GUID(GUID_DEVINTERFACE_USB_HUB, 0xf18a0e88, 0xc30c, 0x11d0, 0x88,
            0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8);


DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2,
            0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);


static SERVICE_STATUS service_status;
static SERVICE_STATUS_HANDLE service_status_handle;
static HDEVNOTIFY notification_handle_hub, notification_handle_dev;

static HANDLE service_stop_event;


static void WINAPI usb_service_main(int argc, char **argv);
static DWORD WINAPI usb_service_handler(DWORD code, DWORD event_type,
                                        VOID *event_data, VOID *context);

static void usb_service_run(int argc, char **argv);
static void usb_service_set_status(DWORD status, DWORD exit_code);
static void usb_register_notifications(void);
static void usb_unregister_notifications(void);

int main(int argc, char **argv)
{
    usbDeviceList list;

    changeLogLevel(+3);

    if (! initDeviceList(&list, ids, recvTimeout, sendTimeout, startWorker))
        message(LOG_ERROR, "failed to initialize device list.\n");
    else if (! updateDeviceList(&list))
        message(LOG_ERROR, "scan failed.\n");
    else
    {
        SERVICE_TABLE_ENTRY dispatch_table[] =
        {
            { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)usb_service_main },
            { NULL, NULL }
        };

        BOOL res = StartServiceCtrlDispatcher(dispatch_table);
        DWORD error;
        error = GetLastError();
        res =  TRUE;
    }
    return 0;
}


static void WINAPI usb_service_main(int argc, char **argv)
{
    memset(&service_status, 0, sizeof(service_status));

    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_START_PENDING;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                        SERVICE_ACCEPT_PAUSE_CONTINUE;

    service_status.dwWaitHint = 5000;

    service_status_handle = RegisterServiceCtrlHandlerEx(SERVICE_NAME,
                                                         usb_service_handler,
                                                         NULL);

    if (!service_status_handle)
        return;

    usb_register_notifications();
    usb_service_run(argc, argv);

    return;
}

static DWORD WINAPI usb_service_handler(DWORD code, DWORD event_type,
                                        VOID *event_data, VOID *context)
{
    switch(code)
    {
    case SERVICE_CONTROL_STOP:
        usb_service_set_status(SERVICE_STOP_PENDING, NO_ERROR);
        usb_unregister_notifications();

        if(service_stop_event)
            SetEvent(service_stop_event);
        break;

    case SERVICE_CONTROL_DEVICEEVENT:
        if(event_type == DBT_DEVICEARRIVAL)
        {
            usb_unregister_notifications();
            fprintf(stderr, "plugged\n");
            usb_register_notifications();
        }
        usb_service_set_status(SERVICE_RUNNING, NO_ERROR);
        break;

    case SERVICE_CONTROL_PAUSE:
        usb_service_set_status(SERVICE_PAUSE_PENDING, NO_ERROR);
        usb_unregister_notifications();
        usb_service_set_status(SERVICE_PAUSED, NO_ERROR);
        break;

    case SERVICE_CONTROL_CONTINUE:
        usb_service_set_status(SERVICE_CONTINUE_PENDING, NO_ERROR);
        usb_register_notifications();
        usb_service_set_status(SERVICE_RUNNING, NO_ERROR);
        break;

    default:
      ;
    }

    return NO_ERROR;
}

static void usb_service_run(int argc, char **argv)
{
    usb_service_set_status(SERVICE_START_PENDING, NO_ERROR);
    service_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);

    if(!service_stop_event)
    {
        usb_service_set_status(SERVICE_STOPPED, NO_ERROR);
        return;
    }

    usb_service_set_status(SERVICE_RUNNING, NO_ERROR);

    while(WaitForSingleObject(service_stop_event, INFINITE));

    CloseHandle(service_stop_event);
    usb_service_set_status(SERVICE_STOPPED, NO_ERROR);
}

static void usb_service_set_status(DWORD status, DWORD exit_code)
{
    service_status.dwCurrentState  = status;
    service_status.dwWin32ExitCode = exit_code;

    SetServiceStatus(service_status_handle, &service_status);
}

static void usb_register_notifications(void)
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

static void usb_unregister_notifications(void)
{
    if(notification_handle_hub)
        UnregisterDeviceNotification(notification_handle_hub);
    if(notification_handle_dev)
        UnregisterDeviceNotification(notification_handle_dev);
}



HANDLE startOverlappedAction(PIPE_PTR fd, HANDLE event, bool connect)
{
    OVERLAPPED over = { NULL };

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
    unsigned int numNames = 1;
    bool firstPass = true;

    /* create an array of names to make life simpler */
    socketName(name, names[0], 256);
    if (alias != NULL && alias[0] != '\0')
    {
        socketName(alias, names[0], 256);
        numNames++;
    }

    memset(&over, 0, sizeof(OVERLAPPED));
    while(true)
    {
        int count = 1, x, numClients = 0;
        client *john;

        /* allocate the handles and overlap objects that I need to populate */
        handles = realloc(handles, sizeof(HANDLE) * (1 + numNames + idev->clientList.count));
        if (firstPass)
            handles[0] = startOverlappedAction(idev->readPipe[READ], NULL, false);

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
        for(john = idev->clientList.head; john != NULL; john = john->header.next)
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
            startOverlappedAction(idev->readPipe[READ], handles[0], false);
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
        for(john = idev->clientList.head; john != NULL;)
        {
            HANDLE event;
            client *next;

            next = john->header.next;
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
}
