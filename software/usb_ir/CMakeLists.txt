cmake_minimum_required(VERSION 2.6)
project(IguanaIR C)

# shut up CMake complaints on Darwin
If(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
EndIf()

include(${CMAKE_SOURCE_DIR}/bootstrap/CMakeMacros/Basics.txt)
include(${CMAKE_SOURCE_DIR}/bootstrap/CMakeMacros/GitVersion.txt)

# Make sure to change the value in Changelog as well
Set(FULLVER 1.2.1)

# SONAME links, unrelated to the package version.
SET(SO_VER 0)
Set(SO_REV 4)
Set(SO_REL 0)

# Our Android support is probably junk at this point, but keeping it
# in another file for reference
#Set(TARGET_ANDROID true)
If(TARGET_ANDROID)
  include(Android.cmake)  
EndIf()

# Default the prefix if it was not set
If(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  Set(CMAKE_INSTALL_PREFIX /usr CACHE PATH "Default prefix to /usr" FORCE)
EndIf()

# if we're installing into /usr then etc is /etc, otherwise put it in ${PREFIX}
If("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
  Set(ETC_PARENT /)
Else()
  Set(ETC_PARENT ${CMAKE_INSTALL_PREFIX})
EndIf()

# set a couple variables early
Set(BASESRC compat.h logging.c logging.h)
Set(PIPESRC pipes.h)

If("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  add_subdirectory(win32/argp-standalone-1.3)
  Set(SWIG_EXECUTABLE C:\\swigwin-2.0.9\\swig.exe) # TODO: we should find instead of hard coding this
  Set(PYTHON_INCLUDE_DIR "C:/Python27/include") # TODO: we should find instead of hard coding this

  List(APPEND BASESRC win32/compat-windows.c)
  List(APPEND PIPESRC win32/pipes.c)
  Set(DAEMONSRC win32/service.c)
  Set(DAEMONLIBS setupapi) # for INF installation
  Set(BASELIBS ws2_32) # for htonl
  include_directories(${CMAKE_SOURCE_DIR} ${ARGPDIR})
  Set(CMAKE_REQUIRED_INCLUDES ${CMAKE_SOURCE_DIR}/win32/libusb-1.0-win32/include
                              ${CMAKE_SOURCE_DIR}/win32/libusb-win32/include)
					  
Else()
  include(CheckLibraryExists)
  include(CheckFunctionExists)

  # set variables common to all Unix-like systems
  Set(DAEMONSRC daemon.c)
  List(APPEND BASESRC compat-unix.c)
  Set(CMAKE_REQUIRED_FLAGS "-I/usr/include")
  add_c_flag(-pedantic -g -O2)

  # see if we have clock_gettime possibly in rt
  check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME)
  If(NOT HAVE_CLOCK_GETTIME)
    check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
    If(NOT HAVE_CLOCK_GETTIME)
      check_function_exists(mach_absolute_time HAVE_MACH_ABSOLUTE_TIME)
    EndIf()
  Else()
    List(APPEND BASELIBS rt)
  EndIf()

  If("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" OR "${CMAKE_SYSTEM_NAME}" STREQUAL "GNU")
    # flag that this was a recognized system
    List(APPEND PIPESRC pipes.c)

    If("${CMAKE_SYSTEM_NAME}" STREQUAL "GNU")
      # this makes it build on GNU Mach, but there's no USB support....
      add_definitions(-DPATH_MAX=1024)
    EndIf()

    # default the LIBDIR if it was not set
    If("${LIBDIR}" STREQUAL "")
      get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
      If(LIB64)
        Set(LIBDIR "lib64")
      EndIf()
    EndIf()

    # default the Linux settings files into /etc
    install(DIRECTORY files/base/etc
            USE_SOURCE_PERMISSIONS
            DESTINATION ${ETC_PARENT})

    # non-Android Linux systems need to link to dl and pthread
    If(NOT TARGET_ANDROID)
      Set(DAEMONLIBS pthread dl)
    EndIf()

    # we rely on PkgConfig to find udev and systemd
    find_package(PkgConfig REQUIRED)

    # Use pkg-config to find udev
    pkg_check_modules(UDEV udev)
    If(UDEV_FOUND)
      execute_process(COMMAND pkg-config --variable udevdir udev
                      OUTPUT_VARIABLE UDEVDIR
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
    EndIf()

    # Use pkg-config to find systemd
    pkg_check_modules(SYSTEMD systemd)
    If(SYSTEMD_FOUND)
      execute_process(COMMAND pkg-config --variable systemdsystemunitdir systemd
                      OUTPUT_VARIABLE SYSTEMD_UNITDIR
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
      execute_process(COMMAND pkg-config --variable tmpfilesdir systemd
                      OUTPUT_VARIABLE TMPFILESDIR
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
    Else()
      # fall back on checking for an old systemd (Ubuntu14.04 et al)
      pkg_check_modules(OLDSYSTEMD libsystemd-daemon)
    EndIf()
  ElseIf("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
    # flag that this was a recognized system
    List(APPEND PIPESRC pipes.c)

    # argp and pthread must be explicitly linked and argp defaults to /usr/local
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
    Set(DAEMONLIBS pthread util)
    Set(ARGPLIB argp)

  ElseIf("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    # flag that this was a recognized system
    List(APPEND PIPESRC pipes.c)

    # pull in hotplug and the frameworks it requires
    Set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework IOKit")
    List(APPEND DAEMONSRC darwin/hotplug.c)

    # add the /opt paths for includes and linking and explicitly link to argp
    include_directories(/opt/local/include)
    link_directories(/opt/local/lib)
    Set(ARGPLIB argp)

    # make sure the coming libusb-1.0 search looks in the /opt path as well
    Set(CMAKE_REQUIRED_FLAGS "-I/opt/local/include")
  EndIf()
EndIf()

If("${PIPESRC}" STREQUAL "pipes.h")
  message(FATAL_ERROR "Unrecognized CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
EndIf()

# finally set the LIBDIR if it has not already been set
If("${LIBDIR}" STREQUAL "")
  Set(LIBDIR "lib")
EndIf()

# check for SOME libusb header now that include paths are set
include(CheckIncludeFile)
CHECK_INCLUDE_FILE("usb.h" HAVE_USB_H)
#CHECK_INCLUDE_FILE("libusb.h" HAVE_LIBUSB_H)
CHECK_INCLUDE_FILE("libusb-1.0/libusb.h" HAVE_USB_10_LIBUSB_H)
If(NOT HAVE_USB_H AND NOT HAVE_LIBUSB_H AND NOT HAVE_USB_10_LIBUSB_H)
  message(FATAL_ERROR "No version of the libusb header files were found.")
EndIf()

# Make config.h based on what cmake found and include it's path
configure_file("${CMAKE_SOURCE_DIR}/config.h.in" config.h)
include_directories(${CMAKE_BINARY_DIR})

# write out version.h w every call to build
GitVersionH("${CMAKE_SOURCE_DIR}/version.h.in")

# build the user library
add_library(iguanaIR SHARED ${PIPESRC} ${BASESRC}
            iguanaIR.c iguanaIR.h dataPackets.c dataPackets.h)
target_link_libraries(iguanaIR ${BASELIBS} ${ARGPLIB})
set_property(TARGET iguanaIR
             APPEND PROPERTY COMPILE_DEFINITIONS IGUANAIR_EXPORTS)
set_target_properties(iguanaIR PROPERTIES
                      VERSION "${SO_VER}.${SO_REV}.${SO_REL}"
                      SOVERSION ${SO_VER})
install(TARGETS iguanaIR
        DESTINATION ${LIBDIR})

# TODO: should have this just build early w/o an explicit dep?
add_dependencies(iguanaIR VersionH)

# build the library that can be used to communicate with the devices
add_library(directIguanaIR SHARED
  driver.c driver.h driverapi.h
  sendFormat.c sendFormat.h
  ${BASESRC})
target_link_libraries(directIguanaIR
                      ${DAEMONLIBS} ${BASELIBS} ${ARGPLIB})
set_property(TARGET directIguanaIR
             APPEND PROPERTY COMPILE_DEFINITIONS DIRECT_EXPORTS)
set_target_properties(directIguanaIR PROPERTIES
                      VERSION "${SO_VER}.${SO_REV}.${SO_REL}"
                      SOVERSION ${SO_VER})
install(TARGETS directIguanaIR
        DESTINATION ${LIBDIR})

# TODO: should have this just build early w/o an explicit dep?
add_dependencies(directIguanaIR VersionH)

# build the service/server that controls hardware
add_executable(igdaemon ${DAEMONSRC} ${PIPESRC} ${BASESRC}
  server.c server.h
  client-interface.c client-interface.h
  device-interface.c device-interface.h
  list.c list.h protocol-versions.c protocol-versions.h
  dataPackets.c dataPackets.h)
target_link_libraries(igdaemon directIguanaIR
                      ${DAEMONLIBS} ${BASELIBS} ${ARGPLIB})
install(TARGETS igdaemon DESTINATION bin)

# build all the drivers
add_subdirectory(drivers)

# build igclient
add_executable(igclient ${BASESRC} client.c list.c list.h)
target_link_libraries(igclient iguanaIR ${BASELIBS} ${ARGPLIB})
install(TARGETS igclient DESTINATION bin)

# see if we have python and swig
If("${CMAKE_ARCH}" STREQUAL "arm")
  Message(STATUS "Skipping Python bits on ARM.")
Else()
  find_package(SWIG)

  ForEach(PYVER 2 3)
    add_subdirectory(python python${PYVER})
  EndForEach(PYVER)

  If(SWIG_FOUND AND FOUND_SOME_PYTHON)
    message(STATUS "Python and SWIG found; will build Python bindings ${CMAKE_INSTALL_PREFIX}")

    # install the reflasher script and hex files
    install(FILES files/python/usr/bin/iguanaIR-reflasher
            DESTINATION bin)
    install(DIRECTORY files/python/usr/share/iguanaIR-reflasher
            DESTINATION share
            PATTERN "body-0.hex" EXCLUDE
            PATTERN "loader-0.hex" EXCLUDE)
    install(PROGRAMS files/python/usr/share/iguanaIR-reflasher/iguanaIR-reflasher
            DESTINATION share/iguanaIR-reflasher)

    # force creation of a symlink
    install(CODE "
      execute_process(COMMAND ln -sf
                              ../share/iguanaIR-reflasher/iguanaIR-reflasher
                              \$ENV{DESTDIR}/usr/bin/iguanaIR-reflasher)
    ")
  Else()
    Message(STATUS "Skipping python build")
  EndIf()
EndIf()

# install all the other bits and pieces under different OSes
If("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  find_package(PythonInterp)
  add_custom_target(BuildReleasePackage
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/py2exe-setup.py py2exe
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/setup.py
                    DEPENDS igdaemon igclient usbdrv usbpre1drv argp _iguanaIR2)
Else()
  If(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    install(DIRECTORY files/base/etc
            USE_SOURCE_PERMISSIONS
            DESTINATION ${ETC_PARENT})
  EndIf()

  install(DIRECTORY files/base/usr/share/man
          USE_SOURCE_PERMISSIONS
          DESTINATION share)
  install(FILES iguanaIR.h
          DESTINATION include)

  If(OLDSYSTEMD_FOUND)
    install(DIRECTORY files/systemd/lib files/systemd/usr
            USE_SOURCE_PERMISSIONS
            DESTINATION ${ETC_PARENT})
  Else()
    If(UDEV_FOUND)
      install(FILES files/systemd/lib/udev/rules.d/80-iguanaIR.rules
              DESTINATION ${UDEVDIR}/rules.d)
    EndIf()

    If(SYSTEMD_FOUND)
      install(FILES files/systemd/lib/systemd/system/iguanaIR.service
              DESTINATION ${SYSTEMD_UNITDIR})
      install(FILES files/systemd/usr/lib/tmpfiles.d/iguanair.conf
              DESTINATION ${TMPFILESDIR})
      install(PROGRAMS files/systemd/usr/bin/iguanaIR-rescan
              DESTINATION bin)
    ElseIf("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
      message(STATUS "No initscript or iguanaIR-rescan installed in OS X.")
    Else()
      install(PROGRAMS files/sysV/usr/bin/iguanaIR-rescan
              DESTINATION bin)
      install(DIRECTORY files/sysV/etc
              USE_SOURCE_PERMISSIONS
              DESTINATION ${ETC_PARENT})
    EndIf()
  EndIf()
EndIf()
