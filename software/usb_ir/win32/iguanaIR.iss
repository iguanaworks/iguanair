[Setup]
AppName=IguanaIR
AppVerName=IguanaIR 0.30
DefaultDirName={pf}\IguanaIR
DefaultGroupName=IguanaIR
;#Compression=bzip"

;#OutputDir="{ini:inno.ini,GLOBAL,Dir|Debug}"
OutputBaseFilename=IguanaIR-0.30

[Files]
; compiled from C sources
Source: "Debug\igdaemon.exe"; DestDir: "{app}\igdaemon.exe"; Flags: ignoreversion
Source: "Debug\igclient.exe"; DestDir: "{app}\igclient.exe"; Flags: ignoreversion
Source: "Debug\iguanaIR.dll"; DestDir: "{app}\iguanaIR.dll"; Flags: ignoreversion
;Source: "{ini:inno.ini,GLOBAL,Dir|Debug}\igdaemon.exe"; DestDir: "{app}\igdaemon.exe"; Flags: ignoreversion
;Source: "{ini:inno.ini,GLOBAL,Dir|Debug}\igclient.exe"; DestDir: "{app}\igclient.exe"; Flags: ignoreversion
;Source: "{ini:inno.ini,GLOBAL,Dir|Debug}\iguanaIR.dll"; DestDir: "{app}\iguanaIR.dll"; Flags: ignoreversion

; created by libusb's tools
Source: "libusb-win32/iguanaIR.cat"; DestDir: "{app}\iguanaIR.cat"; Flags: ignoreversion
Source: "libusb-win32/iguanaIR_x64.cat"; DestDir: "{app}\iguanaIR_x64.cat"; Flags: ignoreversion
Source: "libusb-win32/iguanaIR.inf"; DestDir: "{app}\iguanaIR.inf"; Flags: ignoreversion

; libusb libraries
Source: "libusb-win32/libusb0.dll"; DestDir: "{app}\libusb0.dll"; Flags: ignoreversion
Source: "libusb-win32/libusb0.sys"; DestDir: "{app}\libusb0.sys"; Flags: ignoreversion

; popt libraries
Source: "popt/popt1.dll"; DestDir: "{app}\popt1.dll"; Flags: ignoreversion
Source: "popt/libiconv-2.dll"; DestDir: "{app}\libiconv-21.dll"; Flags: ignoreversion
Source: "popt/libintl-2.dll"; DestDir: "{app}\libintl-2.dll"; Flags: ignoreversion

; stuff for the menus
[Icons]
Name: "{group}\IguanaIR Client"; Filename: "{app}\igclient.exe --get-version"
Name: "{group}\Uninstall IguanaIR"; Filename: "{uninstallexe}"

