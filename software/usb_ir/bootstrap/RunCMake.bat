@ECHO OFF

REM make the output window pause if not run from a console
set SCRIPT=%0
@echo %SCRIPT:~0,1% | findstr /l " > NUL
if %ERRORLEVEL% EQU 0 set PAUSE_ON_CLOSE=1

REM remember where we come from
SET BOOTSTRAP_DIR=%~dp0

REM check that we were run from a CMake-able directory
if NOT EXIST CMakeLists.txt (
  echo No CMakeLists.txt found in local directory:
  echo     %CD%
  if "%CD%\" == "%BOOTSTRAP_DIR%" (
    echo   Did you delete the "Start in:" setting from the shortcut to this batch file?
  )
  GOTO EXIT
)

REM load up some defaults
SET BUILDDIR=build
SET PYPATH=C:\Python27
SET QTPATH=C:\qt\4.6.4\bin
SET CMAKE="C:\Program Files (x86)\CMake 2.8\bin\cmake.exe"
SET GENERATOR="Visual Studio 10"
SET JOM="%BOOTSTRAP_DIR%\jom.exe"

REM override the defaults w use settings
if EXIST settings.bat (
  call settings.bat
)

REM for cross compiles we need to use a toolchain file
if NOT "%CSPATH%" == "" (
  if EXIST "%CSPATH%\NUL" (
    echo "CodeSourcery path is not a directory: CSPATH=%CSPATH%"
    GOTO EXIT
  )
  SET CHAIN=-DCMAKE_MAKE_PROGRAM=%JOM% -DCMAKE_TOOLCHAIN_FILE="%BOOTSTRAP_DIR%\CS-gcc-cross.toolchain"
  SET GENERATOR="NMake Makefiles JOM"

  REM not strictly necessary but makes the otuput cleaner
  if "%LIBPATH%" == "" (
    CALL "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"
  )
)

REM create the build path
mkdir %BUILDDIR% 2>NUL
cd %BUILDDIR%
SET PATH=%PYPATH%;%QTPATH%;%PATH%
if NOT EXIST %CMAKE% (
    echo CMake not found!  Please check the CMAKE variable in the settings.bat file.
    GOTO EXIT
)
%CMAKE% %CHAIN% -G %GENERATOR% ..
cd ..

REM only regular VS projects need a shortcut made or DLLs copied
if NOT %GENERATOR% == "Visual Studio 10" GOTO EXIT

REM TODO: only copy the ones we actually need...........
REM copy over the dlls from the Qt directory into debug and release
echo Copying Qt Debug/ DLLs....
mkdir Debug
SET ext=d4
for %%N in (phonon QtCore QtGui QtNetwork QtSql QtWebKit QtXml QtXmlPatterns QtOpenGL) do copy %QTPATH%\%%Nd4.dll Debug
echo Copying Qt Release/ DLLs....
mkdir Release
SET ext=4
for %%N in (phonon QtCore QtGui QtNetwork QtSql QtWebKit QtXml QtXmlPatterns QtOpenGL) do copy %QTPATH%\%%N4.dll Release

REM copy OpenSSL dlls into debug and release
echo Copying OpenSSL DLLs....
for %%N in (libeay32 ssleay32) do copy %QTPATH%\..\..\openssl\bin\%%N.dll Debug
for %%N in (libeay32 ssleay32) do copy %QTPATH%\..\..\openssl\bin\%%N.dll Release

:EXIT
REM pause if necessary
echo Exit: %EXITVAL%
if defined PAUSE_ON_CLOSE pause
REM exit /b %EXITVAL%
