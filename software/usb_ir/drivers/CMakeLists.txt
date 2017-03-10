If(TARGET_ANDROID)
  Message("Will build libusb for android")
  add_subdirectory(libusb-android/libusb)
  add_subdirectory(libusb-compat-android/libusb)
  include_directories(libusb-compat-android/libusb)
EndIf()

# TODO: only expose the specific API!
If("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
  Set(USBPRE1LIB ${CMAKE_SOURCE_DIR}/win32/libusb-win32/lib/msvc/libusb.lib)
  Set(SUPPORTSRCS ../win32/pipes.c ../support.c)
Else()
  Set(USBPRE1LIB usb)
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
  add_library(usbdrv SHARED libusb.c ../list.c)
  set_property(TARGET usbdrv
               APPEND PROPERTY COMPILE_DEFINITIONS DRIVER_EXPORTS)
  if("${HAVE_USB_10_LIBUSB_H}" EQUAL 1)
      target_link_libraries(usbdrv usb-1.0)
  else()
      target_link_libraries(usbdrv usb)
  endif()

  install(TARGETS usbdrv
          DESTINATION ${LIBDIR}/iguanaIR)
EndIf()

# optionally build a driver that uses libusb pre version 1.0
add_library(usbpre1drv SHARED ${EXCLUDE} libusbpre1.c ../list.c ${SUPPORTSRCS})
set_property(TARGET usbpre1drv
             APPEND PROPERTY COMPILE_DEFINITIONS DRIVER_EXPORTS)
target_link_libraries(usbpre1drv ${USBPRE1LIB})
If(TARGET_ANDROID)
  target_link_libraries(usbpre1drv usb-compat-android)
  install(TARGETS usbpre1drv
          DESTINATION ${LIBDIR}/iguanaIR
          ${NEEDED})
EndIf()

If("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
  set_target_properties(usbpre1drv PROPERTIES RELEASE_OUTPUT_NAME "driver-libusb")
EndIf()
