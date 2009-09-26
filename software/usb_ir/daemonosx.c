/*! @file       daemonosx.c
    @author     Kyle J. McKay
    @brief      Mac OS X iguanaIR daemon hot plug in support
    @details
      This file provides hot plug in support for the iguanaIR daemon on
      Mac OS X.  It spawns a thread and waits for notification that an
      iguanaIR device has been plugged in at which point it raises a SIGHUP
      so that the standard igdaemon hot plug in processing will take place.
      
    @note
      Without this support the reflasher will not work as the reset operation
      during programming results in a disconnect and a reconnect on Mac OS X.

<b>
      Copyright (C) 2009 Kyle J. McKay
\n    All rights reserved.
</b>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFDictionary.h>

#include "iguanaIR.h"
#include "driver.h"

extern int daemon_osx_support(const usbId *);

static void *osx_thread(const usbId *);

int daemon_osx_support(const usbId *ids)
{
  int err;
  pthread_t osx_thread_id;
  pthread_attr_t attrs;

  pthread_attr_init(&attrs);
  pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
  err = pthread_create(
    &osx_thread_id, &attrs, (void *(*)(void *))osx_thread, (void *)ids);
  pthread_attr_destroy(&attrs);
  return err;
}

static void arm_notification(io_iterator_t iterator)
{
  io_object_t object;
  
  while ((object = IOIteratorNext(iterator)) != 0) {
    IOObjectRelease(object);
  }
}

static void osx_match_device(void *refcon __attribute__((unused)),
                             io_iterator_t iterator)
{
  /* notify iguanaIR daemon so that it rescans for new devices */
  raise(SIGHUP);
  /* re-arm the notification */
  arm_notification(iterator);
}

static void *osx_thread(const usbId *ids)
{
  IONotificationPortRef ioport = IONotificationPortCreate(kIOMasterPortDefault);
  CFRunLoopSourceRef rlsource = IONotificationPortGetRunLoopSource(ioport);
  CFMutableDictionaryRef matchDict = IOServiceMatching(kIOUSBDeviceClassName);

  if (ids && matchDict) {
    for (; ids->idVendor != INVALID_VENDOR; ++ids) {
      SInt32 vendor = (SInt32) ids->idVendor;
      SInt32 product = (SInt32) ids->idProduct;
      io_iterator_t iter;

      CFDictionarySetValue(matchDict, CFSTR(kUSBVendorName),
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vendor));
      CFDictionarySetValue(matchDict, CFSTR(kUSBProductName),
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &product));
      CFRetain(matchDict);
      IOServiceAddMatchingNotification(ioport, kIOMatchedNotification,
        matchDict, osx_match_device, NULL, &iter);
      arm_notification(iter);
    }
    CFRelease(matchDict);
  }

  CFRunLoopAddSource(CFRunLoopGetCurrent(), rlsource, kCFRunLoopDefaultMode);
  CFRunLoopRun();
  return NULL;
}
