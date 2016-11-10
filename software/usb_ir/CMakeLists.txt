cmake_minimum_required(VERSION 2.6)
project(IguanaIR C)

find_package(PkgConfig REQUIRED)

# Make sure to change the value in Changelog as well
Set(FULLVER 1.1.2)

#
#  SONAME links,  not related to the package version.
#
Set(SO_REV 0)
SET(SO_CURRENT 0)
Set(SO_AGE 3)

Set(SOVERSION 0)

#Set(TARGET_ANDROID true)

If(TARGET_ANDROID)
  Message("(badly) Targeting the Android platform")

  # define the entry point for the android executables
  Set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --entry=_start")

  # skip the full RPATH for the build tree since it confuses the link command
  SET(CMAKE_SKIP_BUILD_RPATH TRUE)

  # shut up some warnings
  Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-long-long -Wno-variadic-macros")

  # drastically change the include paths
  Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc -fPIC -DANDROID")
  include_directories("/mnt/btrfs/android-ndk-r8e/platforms/android-3/arch-arm/usr/include")
  include_directories("/mnt/btrfs/android-ndk-r8e/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86_64/lib/gcc/arm-linux-androideabi/4.4.3/include")

  # drastically mess with the linking as well
  Set(ANDROID_LINKER_FLAGS "--dynamic-linker /system/bin/linker -nostdlib -rpath /system/lib -L/mnt/btrfs/too-big/droid-system/system/lib")

  Set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ANDROID_LINKER_FLAGS} /mnt/btrfs/android-ndk-r8e/platforms/android-3/arch-arm/usr/lib/crtbegin_dynamic.o /mnt/btrfs/android-ndk-r8e/platforms/android-3/arch-arm/usr/lib/crtend_android.o")
  Set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ANDROID_LINKER_FLAGS}")

  # drop -rdynamic AND always link to libc and libdl
  Set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-lc -ldl")

  # override the actual build commands
  Set(CMAKE_C_LINK_EXECUTABLE "/opt/arm-2012.03/bin/arm-none-linux-gnueabi-ld <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
  Set(CMAKE_C_CREATE_SHARED_LIBRARY "/opt/arm-2012.03/bin/arm-none-linux-gnueabi-ld <CMAKE_SHARED_LIBRARY_C_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> -soname <TARGET_SONAME> -o <TARGET> <OBJECTS> -lc <LINK_LIBRARIES>")

  # we need our own argp
  add_subdirectory(win32/argp-standalone-1.3)
  include_directories(${ARGPDIR})

EndIf()

include(CheckIncludeFile)
set(CMAKE_REQUIRED_FLAGS "-I/usr/include")
CHECK_INCLUDE_FILE("limits.h" HAVE_SYSLIMITS_H)
CHECK_INCLUDE_FILE("libusb-1.0/libusb.h" HAVE_USB_10_LIBUSB_H)
include(CheckFunctionExists)

# default the LIBDIR if it was not set
If("${LIBDIR}" STREQUAL "")
  Set(LIBDIR "/usr/lib")
EndIf()

# set a couple variables early
Set(BASESRC support.c)

If("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
	add_subdirectory(win32/argp-standalone-1.3)
	Set(SWIG_EXECUTABLE C:\\swigwin-2.0.9\\swig.exe)
    Set(PYTHON_INCLUDE_DIR "C:/Python27/include")

	List(APPEND BASELIBS ws2_32)
    Set(PIPESRC win32/pipes.c)
	Set(DAEMONSRC win32/service.c)
	Set(DAEMONLIBS setupapi)
    List(APPEND BASESRC win32/compat-windows.c)
	include_directories(${CMAKE_SOURCE_DIR} ${ARGPDIR} ${CMAKE_SOURCE_DIR}/win32/libusb-win32/include)

ElseIf("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")

    # Checking if systemd/udev is around...
    pkg_check_modules(UDEV udev)
    if ("${UDEV_FOUND}" EQUAL 1)
        execute_process(COMMAND pkg-config --variable udevdir udev
            OUTPUT_VARIABLE UDEVDIR)
        string(REGEX REPLACE "\n$" "" UDEVDIR ${UDEVDIR})
        string(STRIP UDEVDIR ${UDEVDIR})
    else()
        set(UDEVDIR "")
    endif()
    pkg_check_modules(SYSTEMD systemd)
    if ("${SYSTEMD_FOUND}" EQUAL 1)
        execute_process(COMMAND pkg-config --variable systemdsystemunitdir systemd
                        OUTPUT_VARIABLE SYSTEMD_UNITDIR)
        string(REGEX REPLACE "\n$" "" SYSTEMD_UNITDIR ${SYSTEMD_UNITDIR})
        string(STRIP SYSTEMD_UNITDIR ${SYSTEMD_UNITDIR})
        execute_process(COMMAND pkg-config --variable tmpfilesdir systemd
                        OUTPUT_VARIABLE TMPFILESDIR)
        string(REGEX REPLACE "\n$" "" TMPFILESDIR ${TMPFILESDIR})
        string(STRIP TMPFILESDIR ${TMPFILESDIR})
    endif()

    Set(PIPESRC pipes.c)
        Set(DAEMONSRC daemon.c)
    If(NOT TARGET_ANDROID)
      Set(DAEMONLIBS pthread dl)
    EndIf()

    Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall -pedantic")

    # see if we have clock_gettime in time.h
    Set(CMAKE_REQUIRED_LIBRARIES rt)
    check_function_exists(clock_gettime USE_CLOCK_GETTIME)
    Unset(CMAKE_REQUIRED_LIBRARIES)

    # default all symbols to hidden under gcc
# TODO: if we set the visibility correctly then the drivers cannot
#  find symbols from the main library... need to fix that somehow
#    Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

    List(APPEND BASESRC compat-unix.c)
    If(NOT TARGET_ANDROID)
      List(APPEND BASELIBS rt)
    EndIf()
ElseIf("${CMAKE_SYSTEM_NAME}" MATCHES "BSD")
    # see if we have clock_gettime in time.h
    Set(CMAKE_REQUIRED_LIBRARIES rt)
    check_function_exists(clock_gettime USE_CLOCK_GETTIME)
    Unset(CMAKE_REQUIRED_LIBRARIES)
    List(APPEND BASESRC compat-unix.c)
    Set(PIPESRC pipes.c)
    Set(DAEMONSRC daemon.c)
    Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall -pedantic -I/usr/local/include")
    Set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
    Set(ARGPLIB "-largp")
    Set(PTHREADLIB "-lpthread")
Else()
    message(FATAL_ERROR "Unrecognized CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
EndIf()

# let the user know what platform was detected
message("Generator: ${CMAKE_GENERATOR}")
message("Platform:  ${CMAKE_SYSTEM_NAME} on ${CMAKE_SYSTEM_PROCESSOR}")


# build the user library
add_library(iguanaIR SHARED
                     iguanaIR.c dataPackets.c ${PIPESRC}
                     ${BASESRC})
target_link_libraries(iguanaIR ${BASELIBS})
set_target_properties(iguanaIR PROPERTIES
                      VERSION ${FULLVER} SOVERSION ${SOVERSION})
set_property(TARGET iguanaIR
             APPEND PROPERTY COMPILE_DEFINITIONS IGUANAIR_EXPORTS)
set_property(TARGET iguanaIR
             PROPERTY VERSION "${SO_CURRENT}.${SO_AGE}.${SO_REV}")
set_property(TARGET iguanaIR
             PROPERTY SOVERSION ${SOVERSION})
install(TARGETS iguanaIR
        DESTINATION ${LIBDIR})

# build the service/server that controls hardware
#+LISTENER=unixlistener.o
#+#LISTENER=tcplistener.o
#+igdaemon: daemon.o $(LISTENER)
add_executable(igdaemon ${DAEMONSRC}
               server.c client-interface.c device-interface.c driver.c
               list.c protocol-versions.c ${PIPESRC} dataPackets.c ${BASESRC})
set_property(TARGET igdaemon
             APPEND PROPERTY COMPILE_DEFINITIONS SUPPORT_EXPORTS)
set_property(TARGET igdaemon
             PROPERTY LINK_FLAGS  "${PTHREADLIB}")
target_link_libraries(igdaemon iguanaIR
                               ${DAEMONLIBS} ${BASELIBS} ${ARGPLIB})
install(TARGETS igdaemon DESTINATION /usr/bin)


# build all the drivers
add_subdirectory(drivers)

# build igclient to test our API
add_executable(igclient client.c list.c ${BASESRC})
set_property(TARGET igclient
             APPEND PROPERTY COMPILE_DEFINITIONS SUPPORT_EXPORTS)
     set_property(TARGET igclient PROPERTY LINK_FLAGS "${ARGPLIB}")
target_link_libraries(igclient iguanaIR ${BASELIBS} ${ARGPLIB})
install(TARGETS igclient DESTINATION /usr/bin)


# see if we have python and swig
If("${CMAKE_ARCH}" STREQUAL "arm")
  Message("Skipping Python bits on ARM.")
Else()
  find_package(SWIG)
  find_package(PythonLibs ${PYVER})
  If(SWIG_FOUND AND PYTHONLIBS_FOUND)
    message(STATUS "Found Python version ${PYTHONLIBS_VERSION_STRING} (override with -DPYVER=2|3)")
    message(STATUS "Python and SWIG found; will build Python bindings")

    # ensure we have PYTHON_INCLUDE_DIR even on older CMake
    If("${PYTHON_INCLUDE_DIR}" STREQUAL "")
      Set(PYTHON_INCLUDE_DIR "${PYTHON_INCLUDE_PATH}")
    EndIf()

    # compute the version info we need to find site-packages
    # for cmake < 2.8.8 (without PYTHONLIBS_VERSION_STRING) default to 2.7
    Set(PYMAJMINVER "2.7")
    If(NOT "${PYTHONLIBS_VERSION_STRING}" STREQUAL "")
      string(SUBSTRING ${PYTHONLIBS_VERSION_STRING} 0 3 PYMAJMINVER)
    EndIf()

    # pull in the swig macros
    include(${SWIG_USE_FILE})

    include_directories(${PYTHON_INCLUDE_DIR} ${CMAKE_SOURCE_DIR})
    swig_add_module(iguanaIR python iguanaIR.i)
    swig_link_libraries(iguanaIR iguanaIR ${BASELIBS} ${PYTHON_LIBRARIES})
    If(CMAKE_COMPILER_IS_GNUCC)
        set_source_files_properties(${swig_generated_file_fullname}
        PROPERTIES COMPILE_FLAGS "-Wno-long-long -Wno-unused-value")
    EndIf()

    # install the python support
    install(TARGETS _iguanaIR
            DESTINATION ${LIBDIR}/python${PYMAJMINVER}/site-packages)
    install(FILES ${CMAKE_BINARY_DIR}/iguanaIR.py
            DESTINATION ${LIBDIR}/python${PYMAJMINVER}/site-packages)

    # install the reflasher script and hex files
    install(DIRECTORY files/python/usr
            DESTINATION /
            PATTERN ".svn" EXCLUDE
            PATTERN "body-0.hex" EXCLUDE
            PATTERN "loader-0.hex" EXCLUDE)
    install(PROGRAMS files/python/usr/share/iguanaIR-reflasher/iguanaIR-reflasher
            DESTINATION /usr/share/iguanaIR-reflasher)

    # force creation of a symlink
    install(CODE "
      execute_process(COMMAND ln -sf
                              ../share/iguanaIR-reflasher/iguanaIR-reflasher
                              \$ENV{DESTDIR}/usr/bin/iguanaIR-reflasher)
    ")
  Else()
    Message("    Skipping python build")
  EndIf()
EndIf()


# Create a version string based on git info: hash, modififed
# (adds :M if uncommited stuff) and a timestamp.
execute_process(COMMAND git log -1 --format=format:%h
    OUTPUT_VARIABLE GIT_HASH
    ERROR_QUIET)
execute_process(COMMAND  git diff-index --quiet HEAD --
    RESULT_VARIABLE GIT_STATUS
    ERROR_QUIET)
execute_process(COMMAND git log -1 --format=format:%ci
    OUTPUT_VARIABLE GIT_WHEN
    ERROR_QUIET)
if (${GIT_STATUS} EQUAL 0)
    Set(GIT_MOD "")
else()
    set(GIT_MOD ":M")
endif()

# Make config.h, release.h and version.h based on what we found
configure_file(config.h.in ${CMAKE_SOURCE_DIR}/config.h)
configure_file(release.h.in ${CMAKE_SOURCE_DIR}/release.h)
if (NOT "x_${GIT_HASH}" STREQUAL "x_")
    set(GIT_VERSION "${GIT_HASH}${GIT_MOD}-${GIT_WHEN}")
    string(REGEX REPLACE ":[^:]+$" "" GIT_WHEN ${GIT_WHEN})
    configure_file(version.h.in ${CMAKE_SOURCE_DIR}/version.h)
elseif (NOT EXISTS ${CMAKE_SOURCE_DIR}/version.h)
    set(GIT_VERSION "(no git data available)")
    configure_file(version.h.in ${CMAKE_SOURCE_DIR}/version.h)
endif()

# install all the other bits and pieces
install(DIRECTORY files/base/etc files/base/usr
        USE_SOURCE_PERMISSIONS
        DESTINATION /)
if ("${UDEV_FOUND}" EQUAL 1)
    install(DIRECTORY files/base/lib/udev/rules.d
        DESTINATION ${UDEVDIR})
endif()
install(FILES iguanaIR.h
        DESTINATION /usr/include)
if ("${SYSTEMD_FOUND}" EQUAL 1)
    install(FILES files/systemd/lib/systemd/system/iguanaIR.service
            DESTINATION ${SYSTEMD_UNITDIR})
    install(FILES  files/base/lib/udev/rules.d/80-iguanaIR.rules
            DESTINATION ${UDEVDIR}/rules.d)
    install(FILES files/systemd/lib/tmpfiles.d/iguanair.conf
            DESTINATION ${TMPFILESDIR})
    install(DIRECTORY files/systemd/usr
            USE_SOURCE_PERMISSIONS
            DESTINATION /)
else()
    install(DIRECTORY files/sysV/usr files/sysV/etc
            USE_SOURCE_PERMISSIONS
            DESTINATION /)
endif()


##########################################
#install: all
#ifdef DARWIN
#        install -D --mode 644 osx/com.iguana.igdaemon.plist \
#                              /Library/LaunchDaemons/com.iguana.igdaemon.plist
#        launchctl load -w /Library/LaunchDaemons/com.iguana.igdaemon.plist
#endif
##########################################


If("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
  find_package(PythonInterp)
  add_custom_target(BuildReleasePackage
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/py2exe-setup.py py2exe
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/setup.py
                    DEPENDS igdaemon igclient usbpre1 argp _iguanaIR)
EndIf()
