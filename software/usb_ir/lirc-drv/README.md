## IguanaIR LIRC driver README

### Status

This driver used to be part of the LIRC sources. In order to improve
the maintenance it has been moved to Iguanaworks, Inc which is the
primary hardware and software vendor.

From 0.9.4 the driver is available at: https://github.com/iguanaworks/iguanair

As a transition step the same files are available here during the 0.9.4
cycle. These files are a complete lirc driver for iguanair.


### Build and install

Building requires lirc >= 0.9.4. If you are using a packaged version you
probably need to have the -devel and -doc packages installed. Note that
the downstream sources at https://github.com/iguanaworks/iguanair-lirc
are preferred and will be the only option in next release.


To build and install the driver together with the support files:

    $ make
    $ sudo make install

Verifying the driver after make install:

     $ lirc-lsplugins -q iguanair
     ---   /usr/lib64/lirc/plugins/iguanair.so

You could also find the driver docs in the manual. The path varies, but
in a packaged version typically /usr/share/doc/lirc/lirc.org/html/index.html.


### Driver info

This driver supports both receiving and sending (blasting) using
the IguanaIR devices from http://iguanaworks.net/ir

The LIRC driver works on top of the IguanaIR low-level userspace
driver which provides a socket interface. It is available from
the same source, and also packaged for many distributions
including Debian, Fedora and Arch Linux.

The --device argument to the driver is a socket designator created
by the low-level driveri. It defaults to '0' which implies
*/run/iguanair/0*.

The kernel has built-in support for these devices in the
iguanair module. This can be used instead of this driver in
many cases. If the userspace driver should be used, the
kernel driver must be blacklisted. A sample blacklist file
is shipped with this driver.
