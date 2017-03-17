@ECHO OFF

:: assuming visual studio 10
SET KEY_VS_DIR=HKCU\Software\Microsoft\VisualStudio\10.0_Config\Setup\VS
SET VAL_VS_DIR=ProductDir

:: read path to visual studio from the registry
FOR /f "tokens=1,2,*" %%a IN ('reg.exe query %KEY_VS_DIR% /v %VAL_VS_DIR% ^| FINDSTR "%VAL_VS_DIR%"') DO SET VSDIR=%%c

:: call vcvars32.bat to set various convenience environment variables
CALL "%VSDIR%VC\bin\vcvars32.bat"  > nul 2>&1

:: explicitly output the LIB variable so that the parent batch file can use it
@ECHO %LIB%