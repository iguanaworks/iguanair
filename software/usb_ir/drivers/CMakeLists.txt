# TODO: only expose the specific API!
If("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
  Set(USBPRE1LIB ${CMAKE_SOURCE_DIR}/win32/libusb-win32/lib/msvc/libusb.lib)
  Set(SUPPORTSRCS ../win32/pipes.c ../support.c)
Else()
  Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=default")
  If(NOT "${CMAKE_ARCH}" STREQUAL "arm")
    Set(COMPILE_NEW true)
    Set(EXCLUDE EXCLUDE_FROM_ALL)
    Set(NEEDED OPTIONAL)
  EndIf()
EndIf()
# OS X
#Set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")

If(COMPILE_NEW)
  # build a driver that uses libusb version 1.0 or later
  add_library(usb SHARED libusb.c ../list.c)
  set_property(TARGET usb
               APPEND PROPERTY COMPILE_DEFINITIONS DRIVER_EXPORTS)
  target_link_libraries(usb -lusb-1.0 iguanaIR)
  install(TARGETS usb
          DESTINATION ${LIBDIR}/iguanaIR)
EndIf()

# optionally build a driver that uses libusb pre version 1.0
add_library(usbpre1 SHARED ${EXCLUDE} libusbpre1.c ../list.c ${SUPPORTSRCS})
set_property(TARGET usbpre1
             APPEND PROPERTY COMPILE_DEFINITIONS DRIVER_EXPORTS)
target_link_libraries(usbpre1 ${USBPRE1LIB})
install(TARGETS usbpre1
        DESTINATION ${LIBDIR}/iguanaIR
        ${NEEDED})

If("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
  set_target_properties(usbpre1 PROPERTIES RELEASE_OUTPUT_NAME "driver-libusb")
EndIf()
