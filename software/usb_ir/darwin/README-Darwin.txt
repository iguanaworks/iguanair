This file contains notes for building and installing on Darwin (OS X).
Please refer to the README.txt file for general documentation.

* PREREQUISITES

The following packages are required for a full build:

- libusb (preferrably version 1.0 or above but both libusb-1.0 and libusb work)
- SWIG with Python support.

Assuming that one is using Darwin port, one needs to install the libusb, swig, and swig-python ports.

* BUILDING AND INSTALLING

Configuration and building are done using either Autoconf or the Xcode projects
included in the IguanaUSBIR workspace.

The Xcode project is configured to depend on Mac ports (so /opt/local is used
for building) and assumes that installation is done under /usr/local. If that
is not the case, edit the settings in Build Settings as well as the compiler
flags in Build Phases.

The Xcode project builds against static librares so that one can redistribute
the Iguana IR products without worrying about their dependencies.

The Makefiles use the variable $(ARCHFLAGS) to select which architectures to
build for. By default, ARCHFLAGS is set to "-arch i386 -arch x86_64" (very
portable as you can tell). This means that the libraries one links against
must match, or the flags must be changed.

To build with the Autoconf framework, one simply needs to run the ./configure
script for configuration. As usual, non standard directories need to be
provided to configure, so a Darwin port installation could be configured
through:

  % CPPFLAGS=-I/opt/local/include LDFLAGS=-L/opt/local/lib ./configure -prefix=/opt/local

if one wanted to install in /opt/local.

* CAVEATS

Because there is no concept of a system-wide dynamic library search path in
Darwin, one must install the libraries and Python extension (if built) before
one can use the command line tools and Python scripts. If an installation
has already been made, the installed libraries, and not the ones just built,
are used even with the tools and scripts in the build directory.

* DRIVER

The driver is a user daemon. A launchtcl(8) property list is installed by
default in the /Library/LaunchDaemons/ directory. The daemon should be set
to run by:

  % launchctl load -w /Library/LaunchDaemons/net.iguanaworks.igdaemon.plist

If one desires to uninstall the daemon, it should also be prevented from
running, by issuing the following command:

  % launchctl unload -w /Library/LaunchDaemons/net.iguanaworks.igdaemon.plist

