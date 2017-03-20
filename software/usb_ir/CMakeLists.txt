cmake_minimum_required(VERSION 2.6)
project(IguanaIR C)

include(${BOOTSTRAP_DIR}/CMakeMacros/Basics.txt)

# Make sure to change the value in Changelog as well
Set(FULLVER 1.1.2)

# SONAME links, unrelated to the package version.
SET(SO_CURRENT 0)
Set(SO_AGE 3)
Set(SO_REV 0)
Set(SOVERSION 0) # TODO: duplicative?

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
Set(BASESRC support.c)

If("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  add_subdirectory(win32/argp-standalone-1.3)
  Set(SWIG_EXECUTABLE C:\\swigwin-2.0.9\\swig.exe) # TODO: we should find instead of hard coding this
  Set(PYTHON_INCLUDE_DIR "C:/Python27/include") # TODO: we should find instead of hard coding this

  List(APPEND BASELIBS ws2_32)
  Set(PIPESRC win32/pipes.c)
  Set(DAEMONSRC win32/service.c)
  Set(DAEMONLIBS setupapi)
  List(APPEND BASESRC win32/compat-windows.c)
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/win32 ${ARGPDIR} ${CMAKE_SOURCE_DIR}/win32/libusb-win32/include)

Else()
  include(CheckLibraryExists)
  include(CheckFunctionExists)

  # set variables common to all Unix-like systems
  Set(DAEMONSRC daemon.c)
  List(APPEND BASESRC compat-unix.c)
  Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall -pedantic")
  
  If("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" OR "${CMAKE_SYSTEM_NAME}" STREQUAL "GNU")
    # flag that this was a recognized system
    Set(PIPESRC pipes.c)

    If("${CMAKE_SYSTEM_NAME}" STREQUAL "GNU")
      # this makes it build on GNU Mach, but there's not USB support....
      add_definitions(-DPATH_MAX=1024)
    EndIf()

    find_package(PkgConfig REQUIRED)

    # non-Android Linux systems need to link to dl and pthread
    If(NOT TARGET_ANDROID)
      Set(DAEMONLIBS pthread dl)
    EndIf()

    # Use pkg-config to find udev
    pkg_check_modules(UDEV udev)
    if ("${UDEV_FOUND}" EQUAL 1)
      execute_process(COMMAND pkg-config --variable udevdir udev
                      OUTPUT_VARIABLE UDEVDIR)
      string(REGEX REPLACE "\n$" "" UDEVDIR ${UDEVDIR})
      string(STRIP UDEVDIR ${UDEVDIR})
    else()
      set(UDEVDIR "")
    endif()

    # Use pkg-config to find systemd
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

    # see if we have clock_gettime possibly in rt
    check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME)
    If(NOT HAVE_CLOCK_GETTIME)
      check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
    Else()
      List(APPEND BASELIBS rt)
    EndIf()
    
    # default all symbols to hidden
    Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

  ElseIf("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
    # flag that this was a recognized system
    Set(PIPESRC pipes.c)

    # try to find clock_gettime for subsecond timing
    check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)

    # argp and pthread must be explicitly linked and argp defaults to /usr/local
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
    Set(DAEMONLIBS pthread util)
    Set(ARGPLIB argp)

  ElseIf("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    # flag that this was a recognized system, and add os specific code
    Set(PIPESRC pipes.c)
    List(APPEND DAEMONSRC darwin/hotplug.c)

    # TODO: we might be able to move the clock_gettime check out if Darwin is ok w it
    # see if we have mach_absolute_time
    check_function_exists(mach_absolute_time HAVE_MACH_ABSOLUTE_TIME)

    # TODO: why does macports need the -I at the end of this cflags?
    Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/opt/local/include")
    Set(ARGPLIB argp)
#    Set(DAEMONLIBS pthread) ok or no?

    #    include_directories(/opt/local/include)
    include_directories(/opt/local/include/libusb-1.0)
    link_directories(/opt/local/lib)
  EndIf()
EndIf()

If("${PIPESRC}" STREQUAL "")
  message(FATAL_ERROR "Unrecognized CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
EndIf()

include(CheckIncludeFile)
set(CMAKE_REQUIRED_FLAGS "-I/usr/include")
CHECK_INCLUDE_FILE("limits.h" HAVE_SYSLIMITS_H)
CHECK_INCLUDE_FILE("libusb-1.0/libusb.h" HAVE_USB_10_LIBUSB_H)

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
add_executable(igdaemon ${DAEMONSRC}
               server.c client-interface.c device-interface.c driver.c
               list.c protocol-versions.c ${PIPESRC} dataPackets.c ${BASESRC})
set_property(TARGET igdaemon
             APPEND PROPERTY COMPILE_DEFINITIONS SUPPORT_EXPORTS)
target_link_libraries(igdaemon iguanaIR
                      ${DAEMONLIBS} ${BASELIBS} ${ARGPLIB})
install(TARGETS igdaemon DESTINATION /usr/bin)


# build all the drivers
add_subdirectory(drivers)

# build igclient to test our API
add_executable(igclient client.c list.c ${BASESRC})
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

# Make config.h and version.h based on what we found
configure_file(config.h.in ${CMAKE_SOURCE_DIR}/config.h)
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
install(FILES iguanaIR.h
        DESTINATION /usr/include)
if ("${UDEV_FOUND}" EQUAL 1)
  install(DIRECTORY files/base/lib/udev/rules.d
          DESTINATION ${UDEVDIR})
endif()
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

If("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  find_package(PythonInterp)
  add_custom_target(BuildReleasePackage
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/py2exe-setup.py py2exe
                    COMMAND ${PYTHON_INTERP} ${CMAKE_SOURCE_DIR}/win32/setup.py
                    DEPENDS igdaemon igclient usbpre1drv argp _iguanaIR)
EndIf()
