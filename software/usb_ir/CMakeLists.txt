cmake_minimum_required(VERSION 2.6)
project(IguanaIR C)

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
	
	List(APPEND BASELIBS ws2_32)
    Set(PIPESRC win32/pipes.c)
	Set(DAEMONSRC win32/service.c)
	Set(DAEMONLIBS setupapi)
    List(APPEND BASESRC win32/compat-windows.c)
	include_directories(${CMAKE_SOURCE_DIR} ${ARGPDIR} ${CMAKE_SOURCE_DIR}/win32/libusb-win32/include)

ElseIf("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    Set(PIPESRC pipes.c)
	Set(DAEMONSRC daemon.c)
	Set(DAEMONLIBS pthread dl)

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
    List(APPEND BASELIBS rt)
Else()
	message(FATAL_ERROR "Unrecognized CMAKE_SYSTEM_NAME")
EndIf()

# let the user know what platform was detected
message("Generator: ${CMAKE_GENERATOR}")
message("Platform:  ${CMAKE_SYSTEM_NAME} on ${CMAKE_SYSTEM_PROCESSOR}")


# build the user library
add_library(iguanaIR SHARED
                     iguanaIR.c dataPackets.c ${PIPESRC}
                     ${BASESRC})
set_property(TARGET iguanaIR
             APPEND PROPERTY COMPILE_DEFINITIONS IGUANAIR_EXPORTS)
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
  find_package(PythonLibs)
  If(SWIG_FOUND AND PYTHONLIBS_FOUND)
    message("Python and SWIG found; will build Python bindings")

    # pull in the swig macros
    include(${SWIG_USE_FILE})

    include_directories(${PYTHON_INCLUDE_PATH} ${CMAKE_SOURCE_DIR})
    swig_add_module(iguanaIR python iguanaIR.i)
    swig_link_libraries(iguanaIR iguanaIR ${BASELIBS} ${PYTHON_LIBRARIES})
	If(CMAKE_COMPILER_IS_GNUCC)
      set_source_files_properties(${swig_generated_file_fullname}
                                  PROPERTIES COMPILE_FLAGS "-Wno-long-long -Wno-unused-value")
    EndIf()

    # install the python support
    install(FILES ${CMAKE_BINARY_DIR}/_iguanaIR.so
                  ${CMAKE_BINARY_DIR}/iguanaIR.py
            DESTINATION ${LIBDIR}/python2.7/site-packages)

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
  EndIf()
EndIf()

# make the config.h based on what we found
configure_file(config.h.in ${CMAKE_SOURCE_DIR}/config.h)

# install all the other bits and pieces
install(DIRECTORY files/base/etc files/base/lib files/base/usr
        DESTINATION /
        PATTERN ".svn" EXCLUDE)
install(PROGRAMS files/base/etc/init.d/iguanaIR
        DESTINATION /etc/init.d)
install(PROGRAMS files/base/usr/bin/iguanaIR-rescan
        DESTINATION /usr/bin)
install(FILES iguanaIR.h
        DESTINATION /usr/include)

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
