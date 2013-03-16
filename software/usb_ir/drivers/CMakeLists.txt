# TODO: only expose the specific API!
Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=default")
#Set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")


If(NOT "${CMAKE_ARCH}" STREQUAL "arm")
  # build a driver that uses libusb version 1.0 or later
  add_library(usb SHARED libusb.c ../list.c)
  target_link_libraries(usb -lusb-1.0 iguanaIR)
  install(TARGETS usb
          DESTINATION ${LIBDIR}/iguanaIR)

  Set(EXCLUDE EXCLUDE_FROM_ALL)
  Set(NEEDED OPTIONAL)
EndIf()


# optionally build a driver that uses libusb pre version 1.0
add_library(usbpre1 SHARED ${EXCLUDE} libusbpre1.c ../list.c)
target_link_libraries(usbpre1 -lusb)
install(TARGETS usbpre1
        DESTINATION ${LIBDIR}/iguanaIR
        ${NEEDED})
