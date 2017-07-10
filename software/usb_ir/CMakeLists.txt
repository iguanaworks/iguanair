cmake_minimum_required(VERSION 2.6)
project(IguanaIR C)

# shut up CMake complaints on Darwin
If(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
EndIf()

include(${BOOTSTRAP_DIR}/CMakeMacros/Basics.txt)
include(${BOOTSTRAP_DIR}/CMakeMacros/GitVersion.txt)

# Make sure to change the value in Changelog as well
Set(FULLVER 1.1.2) # TODO: pull from version.h or directly from git at cmake time?

# SONAME links, unrelated to the package version.
SET(SO_CURRENT 0)
Set(SO_AGE 3)
Set(SO_REV 0)
Set(SOVERSION 0)

# Our Android support is probably junk at this point, but keeping it
# in another file for reference
#Set(TARGET_ANDROID true)
If(TARGET_ANDROID)
  include(Android.cmake)  
EndIf()

# default the LIBDIR if it was not set
If("${LIBDIR}" STREQUAL "")
  Set(LIBDIR "/usr/lib")
EndIf()
# TODO: installation should go to lib64 on some platforms, but not sure this handles it now

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
  include_directories(${CMAKE_SOURCE_DIR} ${ARGPDIR} ${CMAKE_SOURCE_DIR}/win32/libusb-win32/include)

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

    # non-Android Linux systems need to link to dl and pthread
    If(NOT TARGET_ANDROID)
      Set(DAEMONLIBS pthread dl)
    EndIf()

    # we rely on PkgConfig to find udev and systemd
    find_package(PkgConfig REQUIRED)

    # Use pkg-config to find udev
    pkg_check_modules(UDEV udev)
    if(UDEV_FOUND)
      execute_process(COMMAND pkg-config --variable udevdir udev
                      OUTPUT_VARIABLE UDEVDIR
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()

    # Use pkg-config to find systemd
    pkg_check_modules(SYSTEMD systemd)
    if (SYSTEMD_FOUND)
      execute_process(COMMAND pkg-config --variable systemdsystemunitdir systemd
                      OUTPUT_VARIABLE SYSTEMD_UNITDIR
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
      execute_process(COMMAND pkg-config --variable tmpfilesdir systemd
                      OUTPUT_VARIABLE TMPFILESDIR
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
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

# check that we have assorted headers
include(CheckIncludeFile)
CHECK_INCLUDE_FILE("libusb-1.0/libusb.h" HAVE_USB_10_LIBUSB_H)

# Make config.h based on what cmake found and include it's path
configure_file("${CMAKE_SOURCE_DIR}/config.h.in" config.h)
include_directories(${CMAKE_BINARY_DIR})

# write out version.h w every call to build
GitVersionH("${CMAKE_SOURCE_DIR}/version.h.in")

# build the user library
add_library(iguanaIR SHARED ${PIPESRC} ${BASESRC}
            iguanaIR.c iguanaIR.h dataPackets.c dataPackets.h)
target_link_libraries(iguanaIR ${BASELIBS} ${ARGPLIB})
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
# TODO: should have this just build early w/o an explicit dep?
add_dependencies(iguanaIR VersionH)

# build the library that can be used to communicate with the devices
add_library(directIguanaIR SHARED
  driver.c driver.h driverapi.h
  ${BASESRC})
#set_property(TARGET directIguanaIR
#             APPEND PROPERTY COMPILE_DEFINITIONS SUPPORT_EXPORTS)
set_property(TARGET directIguanaIR
             APPEND PROPERTY COMPILE_DEFINITIONS DIRECT_EXPORTS)
target_link_libraries(directIguanaIR iguanaIR
                      ${DAEMONLIBS} ${BASELIBS} ${ARGPLIB})

# build the service/server that controls hardware
add_executable(igdaemon ${DAEMONSRC} ${PIPESRC} ${BASESRC}
  server.c server.h
  client-interface.c client-interface.h
  device-interface.c device-interface.h
  driver.c driver.h driverapi.h
  list.c list.h protocol-versions.c protocol-versions.h
  dataPackets.c dataPackets.h)
set_property(TARGET igdaemon
             APPEND PROPERTY COMPILE_DEFINITIONS SUPPORT_EXPORTS)
#target_link_libraries(igdaemon iguanaIR directIguanaIR
target_link_libraries(igdaemon iguanaIR
                      ${DAEMONLIBS} ${BASELIBS} ${ARGPLIB})
install(TARGETS igdaemon DESTINATION /usr/bin)

# build all the drivers
add_subdirectory(drivers)

# build igclient
add_executable(igclient ${BASESRC} client.c list.c list.h)
set_property(TARGET igclient
             APPEND PROPERTY COMPILE_DEFINITIONS SUPPORT_EXPORTS)
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

# install all the other bits and pieces under different OSes
If("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  find_package(PythonInterp)
  add_custom_target(BuildReleasePackage
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/py2exe-setup.py py2exe
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/setup.py
                    DEPENDS igdaemon igclient usbpre1drv argp _iguanaIR)
Else()
  install(DIRECTORY files/base/etc files/base/usr
          USE_SOURCE_PERMISSIONS
          DESTINATION /)
  install(FILES iguanaIR.h
          DESTINATION /usr/include)

  If(UDEV_FOUND)
    install(DIRECTORY files/base/lib/udev/rules.d
            DESTINATION ${UDEVDIR})
  EndIf()

  If(SYSTEMD_FOUND)
    install(FILES files/systemd/lib/systemd/system/iguanaIR.service
            DESTINATION ${SYSTEMD_UNITDIR})
    install(FILES  files/base/lib/udev/rules.d/80-iguanaIR.rules
            DESTINATION ${UDEVDIR}/rules.d)
    install(FILES files/systemd/lib/tmpfiles.d/iguanair.conf
            DESTINATION ${TMPFILESDIR})
    install(DIRECTORY files/systemd/usr
            USE_SOURCE_PERMISSIONS
            DESTINATION /)
  Else()
    install(DIRECTORY files/sysV/usr files/sysV/etc
            USE_SOURCE_PERMISSIONS
            DESTINATION /)
  EndIf()
EndIf()
