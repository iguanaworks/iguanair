The documentation for this driver is kindly described as "a little
sparse" in reality a better term is "basically non-existent", but
we're trying to remedy the situation as time permits.

Additional documentation can be found online at:
http://iguanaworks.net/projects/IguanaIR

The USB driver is written as a daemon process for a couple of reasons.
First of all, in-kernel USB programming is more complicated than using
the libusb library.  Secondly, it's not just a driver in lircd
primarily because the device has capabilities that extend beyond
simple IR transceiving, thanks to the GPIO pins.  By the way, if you
have an interesting idea about what we could do with 8 digital general
purpose IO pins, let us know through email.  Currently, we are
investigating using them to control a character LCD panel, and
possibly turn on the PC from the remote control.

Some important files in this distribution:
AUTHORS, LICENSE, README.txt
    Our contact information, the license (GPLv2) for this software,
    and this file.

client.c and daemon.c:
    These files contain the main() for the client and driver daemon
    and are compiled into igclient and igdaemon, respectively.

dataPackets.{h,c}:
    Defines the data packets used internally in the driver to hold
    command requests and responses from the USB device.

iguanadev.{h,c}:
    These files should wrap all accesses to the USB device(s) in the
    daemon process in cooperation with the protocol files.

protocol.{h,c}:
    Protocol files describe the protocol used to communicate with the
    USB device, including expected data sizes, and changes with
    different device versions.

iguanaIR.{h,c,i}:
    The iguanaIR files define the application interface (API) to the
    libiguanaIR.so library.  This interface should be used by the
    client applications, such as igdaemon, to interact with the
    igdaemon.  Additionally, the .i file provides swig definitions for
    interacting with the igdaemon using Python.

iguanaIR.init and iguanaIR.gentoo.init:
    These are init scripts that should start the igdaemon at boot time
    as the iguanair user.  The first should work for Fedora or Debian
    based distros, and while the second is aimed at Gentoo's init
    system.

iguanaIR.options:
    Stored in /etc/default/iguanaIR this file is read by the init
    scripts to get default igdaemon command line options.

iguanaIR.rules:
    Used by the udev subsystem to properly set ownership of the USB
    devices.  So far, most problems that we've encountered with the
    USB hardware has been tracable to udev difficulties.

initLCD:
    A python script that we use for testing the GPIO pins with an LCD
    screen.  Yes, it works, and fairly well, but we still need more
    testing before we sell the LCD screens or cables to connect to
    them.

list.{h,c}:
    A small linked list implementation used in the driver and client.

Makefile and mymake.info:
    A huge makefile that does more than we need, and the build
    information for this specific project.

support.{h,c}:
    Supporting functions for the client and daemon.

testdata/:
    This directory contains data files that we use for testing the USB
    (and serial) devices.  Unless you have the same devices as we do,
    this directory is probably of little use.

usbclient.{h,c}:
    These contain some basic wrapper functions for wrapping relatively
    low-level libusb calls.
