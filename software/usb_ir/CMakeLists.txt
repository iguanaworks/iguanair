cmake_minimum_required(VERSION 2.6)
project(IguanaIR C)

include(CheckFunctionExists)

# set a couple variables early
Set(BASESRC support.c)

If("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    # see if we have clock_gettime in time.h
    Set(CMAKE_REQUIRED_LIBRARIES rt)
    check_function_exists(clock_gettime USE_CLOCK_GETTIME)
    Unset(CMAKE_REQUIRED_LIBRARIES)

    # default all symbols to hidden under gcc
#    Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

    List(APPEND BASESRC compat-unix.c)
    List(APPEND BASELIBS rt)
EndIf()

# let the user know what platform was detected
message("Generator: ${CMAKE_GENERATOR}")
message("Platform:  ${CMAKE_SYSTEM_NAME} on ${CMAKE_SYSTEM_PROCESSOR}")


# build the user library
add_library(iguanaIR SHARED
                     iguanaIR.c dataPackets.c pipes.c
                     ${BASESRC})
set_property(TARGET iguanaIR
             APPEND PROPERTY COMPILE_DEFINITIONS IGUANAIR_EXPORTS)
install(TARGETS iguanaIR
        DESTINATION /usr/lib)


# build the service/server that controls hardware
add_executable(igdaemon daemon.c
               server.c client-interface.c device-interface.c driver.c
               list.c protocol-versions.c pipes.c dataPackets.c ${BASESRC})
#target_link_libraries(igdaemon
target_link_libraries(igdaemon iguanaIR
                               pthread dl popt ${BASELIBS})
install(TARGETS igdaemon DESTINATION /usr/bin)


# build all the drivers
add_subdirectory(drivers)

# build igclient to test our API
add_executable(igclient client.c list.c ${BASESRC})
target_link_libraries(igclient iguanaIR popt ${BASELIBS})
install(TARGETS igclient DESTINATION /usr/bin)


# see if we have python and swig
find_package(SWIG)
find_package(PythonLibs)
If (SWIG_FOUND AND PYTHONLIBS_FOUND)
    message("Python and SWIG; will build Python bindings")

    INCLUDE(${SWIG_USE_FILE})

    INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
    INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
    SWIG_ADD_MODULE(iguanaIR python iguanaIR.i iguanaIR.c)

    # install the python support
    install(FILES ${CMAKE_BINARY_DIR}/_iguanaIR.so
                  ${CMAKE_BINARY_DIR}/iguanaIR.py
            DESTINATION /usr/lib/python2.7/dist-packages)

    # install the reflasher script and hex files
    install(PROGRAMS reflasher/iguanaIR-reflasher
            DESTINATION /usr/bin)
    install(DIRECTORY reflasher/hex
            DESTINATION /usr/lib/iguanaIR-reflasher)
EndIf()

# make the config.h based on what we found
configure_file(config.h.in.cmake ${CMAKE_SOURCE_DIR}/config.h)







##########################################
# install all the other bits and pieces
#.PHONY: install
#install: all
#        install -d $(DESTDIR)/$(BINDIR)
#        install igclient $(DESTDIR)/$(BINDIR)/igclient
#        install igdaemon $(DESTDIR)/$(BINDIR)/igdaemon
#        install -d $(DESTDIR)/$(LIBDIR)/iguanaIR
#        install $(LIBNAME) $(DESTDIR)/$(LIBDIR)/$(LIBNAME)
#        install drivers/libusbpre1.$(SHAREEXT) $(DESTDIR)/$(LIBDIR)/iguanaIR/libusbpre1.$(SHAREEXT)
#        install drivers/libusb.$(SHAREEXT) $(DESTDIR)/$(LIBDIR)/iguanaIR/libusb.$(SHAREEXT)
#        install -d $(DESTDIR)$(prefix)/include
#        install $(srcdir)/iguanaIR.h $(DESTDIR)$(prefix)/include/iguanaIR.h
#ifdef DARWIN
#        install -D --mode 644 osx/com.iguana.igdaemon.plist \
#                              /Library/LaunchDaemons/com.iguana.igdaemon.plist
#        launchctl load -w /Library/LaunchDaemons/com.iguana.igdaemon.plist
#else
#        install -D $(srcdir)/iguanaIR.init $(DESTDIR)/etc/init.d/iguanaIR
#        install -D --mode 644 $(srcdir)/iguanaIR.options $(DESTDIR)/etc/default/iguanaIR
#        install -D --mode 644 $(srcdir)/udev/iguanaIR.rules \
#                              $(DESTDIR)/lib/udev/rules.d/80-iguanaIR.rules
#        install --mode 755 $(SETOWNER) -d $(DESTDIR)/lib/udev/devices/iguanaIR
#        install --mode 755 $(SETOWNER) -d $(DESTDIR)/dev/iguanaIR
#endif
##########################################
