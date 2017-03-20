If(TARGET_ANDROID)
  Message("Will build libusb for android")
  add_subdirectory(libusb-android/libusb)
  add_subdirectory(libusb-compat-android/libusb)
  include_directories(libusb-compat-android/libusb)
EndIf()

Set(BASESRCS loggingWrap.c ../list.c)

If("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  Set(USBPRE1LIB ${CMAKE_SOURCE_DIR}/win32/libusb-win32/lib/msvc/libusb.lib)
  List(APPEND BASESRCS ../win32/pipes.c)
Else()
  Set(USBPRE1LIB usb)
  If(NOT "${CMAKE_ARCH}" STREQUAL "arm")
    Set(COMPILE_NEW true)
    Set(EXCLUDE EXCLUDE_FROM_ALL)
    Set(NEEDED OPTIONAL)
  EndIf()
EndIf()
# OS X : TODO: necessary?
#Set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")

If(COMPILE_NEW)
  # build a driver that uses libusb version 1.0 or later
  add_library(usbdrv SHARED libusb.c ${BASESRCS})
  set_property(TARGET usbdrv
               APPEND PROPERTY COMPILE_DEFINITIONS DRIVER_EXPORTS)
  If(HAVE_USB_10_LIBUSB_H)
    target_link_libraries(usbdrv usb-1.0)
  Else()
    target_link_libraries(usbdrv usb)
  EndIf()

  install(TARGETS usbdrv
          DESTINATION ${LIBDIR}/iguanaIR)
EndIf()

# optionally build a driver that uses libusb pre version 1.0
add_library(usbpre1drv SHARED ${EXCLUDE} libusbpre1.c ${BASESRCS})
set_property(TARGET usbpre1drv
             APPEND PROPERTY COMPILE_DEFINITIONS DRIVER_EXPORTS)
target_link_libraries(usbpre1drv ${USBPRE1LIB})
If(TARGET_ANDROID)
  target_link_libraries(usbpre1drv usb-compat-android)
EndIf()
If(NOT "${NEEDED}" STREQUAL "OPTIONAL")
  install(TARGETS usbpre1drv
          DESTINATION ${LIBDIR}/iguanaIR
          ${NEEDED})
EndIf()

