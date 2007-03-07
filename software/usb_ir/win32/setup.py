import sys
import os

exitval = 1
if len(sys.argv) != 2:
    sys.stderr.write('Need a source directory')
else:
    vars = {
        'dir' : sys.argv[1],
        'version' : 'UNKNOWN'
    }
    ISSPath = 'iguanaIR.iss'

    input = open('../iguanaIR.spec', 'r')
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
OutputBaseFilename=IguanaIR-%(version)s

[Files]
; compiled from C sources
Source: "%(dir)s/igdaemon.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/igclient.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "%(dir)s/_iguanaIR.dll"; DestDir: "{app}"; Flags: ignoreversion

; popt libraries
Source: "popt/popt1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "popt/libiconv-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "popt/libintl-2.dll"; DestDir: "{app}"; Flags: ignoreversion

; the rest are created by libusb's tools
Source: "libusb-win32/iguanaIR.cat"; DestDir: "{app}"; Flags: ignoreversion
Source: "libusb-win32/iguanaIR_x64.cat"; DestDir: "{app}"; Flags: ignoreversion
; TODO: the inf file goes where?
Source: "libusb-win32/iguanaIR.inf"; DestDir: "{win}/INF"; Flags: ignoreversion

; libusb libraries
Source: "libusb-win32/libusb0.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "libusb-win32/libusb0.sys"; DestDir: "{app}"; Flags: ignoreversion

; stuff for the menus
[Icons]
Name: "{group}\IguanaIR Client"; Filename: "{app}\igclient.exe"
Name: "{group}\Uninstall IguanaIR"; Filename: "{uninstallexe}"

; install the daemon
[Run]
Filename: "{app}/igdaemon.exe"; Parameters: "--regsvc"; Flags: runhidden
Filename: "{app}/igdaemon.exe"; Parameters: "--startsvc"; Flags: runhidden
;Filename: "{app}/README.TXT"; Description: "View the README file"; Flags: postinstall shellexec skipifsilent
;Filename: "{app}/MYPROG.EXE"; Description: "Launch application"; Flags: postinstall nowait skipifsilent unchecked

; remove the daemon on uninstall
[UninstallRun]
Filename: "{app}/igdaemon.exe"; Parameters: "--stopsvc"; Flags: runhidden
Filename: "{app}/igdaemon.exe"; Parameters: "--unregsvc"; Flags: runhidden

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
