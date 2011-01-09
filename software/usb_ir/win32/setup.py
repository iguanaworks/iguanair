#***************************************************************************
#** setup.py ***************************************************************
# **************************************************************************
# *
# * Inno Setup configuration.
# *
# * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
# * Author: Joseph Dunn <jdunn@iguanaworks.net>
# *
# * Distributed under the GPL version 2.
# * See LICENSE for license details.
# */

import sys
import os

import win32api

exitval = 1
if len(sys.argv) != 2:
    sys.stderr.write('Need a source directory')
else:
    vars = {
        'dir' : sys.argv[1],
        'version' : 'UNKNOWN',
        'pyver'   : '%d%d' % (sys.version_info[0], sys.version_info[1]),
        'sys32'   : win32api.GetSystemDirectory()
    }
    ISSPath = 'iguanaIR.iss'

    input = open('../packaging/fedora/iguanaIR.spec', 'r')
    for line in input:
        if line.startswith('Version:'):
            vars['version'] = line.split()[1]

    out = open(ISSPath, 'w')
    out.write("""
[Setup]
AppName=IguanaIR
AppVerName=IguanaIR %(version)s
DefaultDirName={pf}\IguanaIR
DefaultGroupName=IguanaIR
;#Compression=bzip"

OutputDir=%(dir)s
OutputBaseFilename=iguanaIR-%(version)s

[Files]
; compiled from C sources
Source: "%(dir)s/igdaemon.exe";      DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/igclient.exe";      DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/iguanaIR.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/driver-libusb.dll"; DestDir: "{app}"; Flags: ignoreversion

; compiled python APIs
Source: "%(dir)s/_iguanaIR.pyd";     DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/iguanaIR.py";       DestDir: "{app}"; Flags: ignoreversion

; executable, zip file, python library, et al for py2exe applications
Source: "%(dir)s/library.zip";            DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/python%(pyver)s.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/iguanaIR-reflasher.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/hex/*.hex";              DestDir: "{app}/hex"; Flags: ignoreversion

; popt libraries
Source: "popt/popt1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "popt/libiconv-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "popt/libintl-2.dll"; DestDir: "{app}"; Flags: ignoreversion

; the rest are created by libusb's tools
Source: "libusb-win32/iguanaIR.cat"; DestDir: "{app}"; Flags: ignoreversion
Source: "libusb-win32/iguanaIR.inf"; DestDir: "{app}"; Flags: ignoreversion

; libusb libraries for various archs
Source: "libusb-win32/x86/libusb0_x86.dll"; DestDir: "{app}/x86"; Flags: ignoreversion
Source: "libusb-win32/x86/libusb0.sys"; DestDir: "{app}/x86"; Flags: ignoreversion
Source: "libusb-win32/amd64/libusb0.dll"; DestDir: "{app}/amd64"; Flags: ignoreversion
Source: "libusb-win32/amd64/libusb0.sys"; DestDir: "{app}/amd64"; Flags: ignoreversion
Source: "libusb-win32/ia64/libusb0.dll"; DestDir: "{app}/ia64"; Flags: ignoreversion
Source: "libusb-win32/ia64/libusb0.sys"; DestDir: "{app}/ia64"; Flags: ignoreversion

; embed the sub installer we need
Source: "vcredist_x86.exe"; DestDir: "{app}"; Flags: ignoreversion deleteafterinstall

; stuff for the menus
[Icons]
Name: "{group}\IguanaIR Client"; Filename: "{app}\igclient.exe"
Name: "{group}\Uninstall IguanaIR"; Filename: "{uninstallexe}"

; install the daemon
[Run]
Filename: "{app}/vcredist_x86.exe"; Parameters: "/q"; Flags: runhidden
Filename: "{app}/igdaemon.exe"; Parameters: "--installinf"; Flags: runhidden
Filename: "{app}/igdaemon.exe"; Parameters: "--regsvc"; Flags: runhidden
Filename: "{app}/igdaemon.exe"; Parameters: "--startsvc"; Flags: runhidden
;Filename: "{app}/README.TXT"; Description: "View the README file"; Flags: postinstall shellexec skipifsilent

; remove the daemon on uninstall
[UninstallRun]
Filename: "{app}/igdaemon.exe"; Parameters: "--stopsvc"; Flags: runhidden
Filename: "{app}/igdaemon.exe"; Parameters: "--unregsvc"; Flags: runhidden

; large code block to stop the service during the installation
[Code]
type
	SERVICE_STATUS = record
    	dwServiceType				: cardinal;
    	dwCurrentState				: cardinal;
    	dwControlsAccepted			: cardinal;
    	dwWin32ExitCode				: cardinal;
    	dwServiceSpecificExitCode	: cardinal;
    	dwCheckPoint				: cardinal;
    	dwWaitHint					: cardinal;
	end;
	HANDLE = cardinal;

const
	SERVICE_QUERY_CONFIG		= $1;
	SERVICE_CHANGE_CONFIG		= $2;
	SERVICE_QUERY_STATUS		= $4;
	SERVICE_START				= $10;
	SERVICE_STOP				= $20;
	SERVICE_ALL_ACCESS			= $f01ff;
	SC_MANAGER_ALL_ACCESS		= $f003f;
	SERVICE_WIN32_OWN_PROCESS	= $10;
	SERVICE_WIN32_SHARE_PROCESS	= $20;
	SERVICE_WIN32				= $30;
	SERVICE_INTERACTIVE_PROCESS = $100;
	SERVICE_BOOT_START          = $0;
	SERVICE_SYSTEM_START        = $1;
	SERVICE_AUTO_START          = $2;
	SERVICE_DEMAND_START        = $3;
	SERVICE_DISABLED            = $4;
	SERVICE_DELETE              = $10000;
	SERVICE_CONTROL_STOP		= $1;
	SERVICE_CONTROL_PAUSE		= $2;
	SERVICE_CONTROL_CONTINUE	= $3;
	SERVICE_CONTROL_INTERROGATE = $4;
	SERVICE_STOPPED				= $1;
	SERVICE_START_PENDING       = $2;
	SERVICE_STOP_PENDING        = $3;
	SERVICE_RUNNING             = $4;
	SERVICE_CONTINUE_PENDING    = $5;
	SERVICE_PAUSE_PENDING       = $6;
	SERVICE_PAUSED              = $7;

// #######################################################################################
// nt based service utilities
// #######################################################################################
function OpenSCManager(lpMachineName, lpDatabaseName: string; dwDesiredAccess :cardinal): HANDLE;
external 'OpenSCManagerA@advapi32.dll stdcall';

function OpenService(hSCManager :HANDLE;lpServiceName: string; dwDesiredAccess :cardinal): HANDLE;
external 'OpenServiceA@advapi32.dll stdcall';

function CloseServiceHandle(hSCObject :HANDLE): boolean;
external 'CloseServiceHandle@advapi32.dll stdcall';

function CreateService(hSCManager :HANDLE;lpServiceName, lpDisplayName: string;dwDesiredAccess,dwServiceType,dwStartType,dwErrorControl: cardinal;lpBinaryPathName,lpLoadOrderGroup: String; lpdwTagId : cardinal;lpDependencies,lpServiceStartName,lpPassword :string): cardinal;
external 'CreateServiceA@advapi32.dll stdcall';

function DeleteService(hService :HANDLE): boolean;
external 'DeleteService@advapi32.dll stdcall';

function StartNTService(hService :HANDLE;dwNumServiceArgs : cardinal;lpServiceArgVectors : cardinal) : boolean;
external 'StartServiceA@advapi32.dll stdcall';

function ControlService(hService :HANDLE; dwControl :cardinal;var ServiceStatus :SERVICE_STATUS) : boolean;
external 'ControlService@advapi32.dll stdcall';

function QueryServiceStatus(hService :HANDLE;var ServiceStatus :SERVICE_STATUS) : boolean;
external 'QueryServiceStatus@advapi32.dll stdcall';

function QueryServiceStatusEx(hService :HANDLE;ServiceStatus :SERVICE_STATUS) : boolean;
external 'QueryServiceStatus@advapi32.dll stdcall';

function OpenServiceManager() : HANDLE;
begin
	if UsingWinNT() = true then begin
		Result := OpenSCManager('','ServicesActive',SC_MANAGER_ALL_ACCESS);
		if Result = 0 then
			MsgBox('the servicemanager is not available', mbError, MB_OK)
	end
	else begin
			MsgBox('only nt based systems support services', mbError, MB_OK)
			Result := 0;
	end
end;

function IsServiceInstalled(ServiceName: string) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then begin
		hService := OpenService(hSCM,ServiceName,SERVICE_QUERY_CONFIG);
        if hService <> 0 then begin
            Result := true;
            CloseServiceHandle(hService)
		end;
        CloseServiceHandle(hSCM)
	end
end;

function StartService(ServiceName: string) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then begin
		hService := OpenService(hSCM,ServiceName,SERVICE_START);
        if hService <> 0 then begin
        	Result := StartNTService(hService,0,0);
            CloseServiceHandle(hService)
		end;
        CloseServiceHandle(hSCM)
	end;
end;

function StopService(ServiceName: string) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
	Status	: SERVICE_STATUS;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then begin
		hService := OpenService(hSCM,ServiceName,SERVICE_STOP);
        if hService <> 0 then begin
        	Result := ControlService(hService,SERVICE_CONTROL_STOP,Status);
            CloseServiceHandle(hService)
		end;
        CloseServiceHandle(hSCM)
	end;
end;

function IsServiceRunning(ServiceName: string) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
	Status	: SERVICE_STATUS;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then begin
		hService := OpenService(hSCM,ServiceName,SERVICE_QUERY_STATUS);
    	if hService <> 0 then begin
			if QueryServiceStatus(hService,Status) then begin
				Result :=(Status.dwCurrentState = SERVICE_RUNNING)
        	end;
            CloseServiceHandle(hService)
		    end;
        CloseServiceHandle(hSCM)
	end
end;

// #######################################################################################
// version functions
// #######################################################################################
function CheckVersion(Filename : string;hh,hl,lh,ll : integer) : boolean;
var
	VersionMS	: cardinal;
	VersionLS	: cardinal;
	CheckMS		: cardinal;
	CheckLS		: cardinal;
begin
	if GetVersionNumbers(Filename,VersionMS,VersionLS) = false then begin
		Result := false
	end else begin
		CheckMS := (hh shl $10) or hl;
		CheckLS := (lh shl $10) or ll;
		Result := (VersionMS > CheckMS) or ((VersionMS = CheckMS) and (VersionLS >= CheckLS));
	end;
end;

function NeedVCRedistUpdate() : boolean;
begin
	Result := (CheckVersion('msvcm90.dll',9,0,21022,8) = false)
         or (CheckVersion('msvcp90.dll',9,0,21022,8) = false)
         or (CheckVersion('msvcr90.dll',9,0,21022,8) = false);
end;

function InitializeSetup(): boolean;
begin
	if IsServiceInstalled('igdaemon') and IsServiceRunning('igdaemon') then begin
		  StopService('igdaemon')
  end

//  if NeedVCRedistUpdate() then
//    MsgBox('Need the redist update', mbError, MB_OK)

	Result := true
end;

""" % vars)
    out.close()

    try:
        import ctypes
    except ImportError:
        try:
            import win32api
        except ImportError:
            import os
            os.startfile(self.pathname)
        else:
            print "Ok, using win32api."
            win32api.ShellExecute(0, "compile",
                                  ISSPath,
                                  None,
                                  None,
                                  0)
    else:
        print "Cool, you have ctypes installed."
        res = ctypes.windll.shell32.ShellExecuteA(0, "compile",
                                                  ISSPath,
                                                  None,
                                                  None,
                                                  0)
        if res < 32:
            raise RuntimeError, "ShellExecute failed, error %d" % res
    exitval = 0

sys.exit(exitval)
