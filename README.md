# IguanaIR software / firmware project README
===========================================

This project provides the software and firmware used with the
IguanaWorks usbir hardware.  This hardware is used to communicate via
infrared signals between remote controls, computer systems, and
consumer electronics.  The hardware is most commonly used along side
LIRC or WinLirc by individuals building customized media centers or
similar projects, but people have also controlled glowing juggling
balls or used these devices in university projects.

This code currently supports multiple Linux distributions on PCs or
embeded platforms, such as the Raspberry Pi, along with Windows 7
through Windows 10.  Windows XP and Vista should also work, but are
not explicitly supported at this time.  This software includes
firmware update tools, command line testing tools, and Python (version
2 or 3) and C programming APIs for people who want to write their own
code to interact with devices controlled by infrared signals.

Alternatively, a Linux kernel driver also supports the IguanaWorks
usbir hardware and does not require or use the software provided here.

With either this software, or the kernel driver, Linux users commonly
rely on LIRC to decode the infrared signals into a more user-friendly
format such as "TV Power" or "Heater On":

  http://www.lirc.org/

Windows users usually rely on WinLIRC for signal decoding and coupled
with plugins for frameworks such as Girder or EventGhost these
configurations can tie infrared signals into home automation:

  http://winlirc.sourceforge.net/
  https://www.promixis.com/girder.php
  http://www.eventghost.org/



The hardware can be purchased from our website and bulk discounts are
available:

  http://www.iguanaworks.net

The IguanaWorks website also provides Windows binaries, frequently
asked questions, and a support site to contact the developers with
questions, issues, or descriptions of interesting projects.

Thank you for your interest.
 -The IguanaWorks Team
