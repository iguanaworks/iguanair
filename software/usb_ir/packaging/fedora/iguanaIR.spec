# shorten a couple paths for later
%define usbir software/usb_ir
%define lircdrv software/lirc-drv-iguanair

# Don't add provides for python .so files
%define __provides_exclude_from %{python_sitearch}/.*\.so$

Name:             iguanaIR
# TODO: dynamically grab the version, maybe during make-dist.sh
Version:          1.2.0
Release:          1%{?dist}
Summary:          Driver for Iguanaworks USB IR transceiver
Group:            System Environment/Daemons
License:          GPLv2 and LGPLv2
URL:              https://www.iguanaworks.net
Source0:          https://www.iguanaworks.net/downloads/%{name}-%{version}.tar.gz
Requires:         libusb1 udev systemd udev
BuildRequires:    cmake libusb1-devel systemd-devel
%if 0%{?el7}
BuildRequires:    udev
%else
BuildRequires:    systemd-udev
%endif
Requires(post):   /usr/bin/install /sbin/chkconfig
Requires(pre):    /usr/sbin/useradd
Requires(preun):  /sbin/chkconfig /sbin/service /bin/rmdir
Requires(postun): /usr/sbin/userdel

%description
This package provides igdaemon and igclient, the programs necessary to
control the Iguanaworks USB IR transceiver.


%package devel
Summary: Library and header files for iguanaIR
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
The development files needed to interact with the iguanaIR igdaemon are
included in this package.


%package -n python-iguanaIR
Group: System Environment/Daemons
Summary: Python module for Iguanaworks USB IR transceiver
Requires: %{name} = %{version}-%{release}, python >= 2.4
BuildRequires: python-devel swig
Provides: %{name}-python = %{version}

%description -n python-iguanaIR
This package provides the swig-generated Python module for interfacing
with the Iguanaworks USB IR transceiver.


%if ! 0%{?el7}
%package -n python3-iguanaIR
Group: System Environment/Daemons
Summary: Python module for Iguanaworks USB IR transceiver
Requires: %{name} = %{version}-%{release}, python3
BuildRequires: python3-devel swig

%description -n python3-iguanaIR
This package provides the swig-generated Python module for interfacing
with the Iguanaworks USB IR transceiver.
%endif

%package reflasher
Group: System Environment/Daemons
Summary: Reflasher for Iguanaworks USB IR transceiver
Requires: %{name}-python = %{version}
BuildArch: noarch

%description reflasher
This package provides the reflasher/testing script and assorted firmware
versions for the Iguanaworks USB IR transceiver.  If you have no idea
what this means, you do not need it.


%if ! 0%{?el7}
%package -n lirc-drv-iguanair
Group: System Environment/Daemons
Summary: LIRC driver for Iguanaworks USB IR transceiver
Requires: %{name} = %{version}, lirc-core
BuildRequires: lirc-devel

%description -n lirc-drv-iguanair
This package provides the lirc driver for the Iguanaworks USB IR
transceiver.
%endif



%prep
%setup -q -n %{name}-%{version}

%build
pushd %{usbir}
mkdir -p build
cd build
cmake -DPYVER=%{pyver} -DLIBDIR=%{_libdir} -DBOOTSTRAP_DIR=`pwd`/../bootstrap ..
make %{?_smp_mflags}
popd
%if ! 0%{?el7}
pushd %{lircdrv}
make
%endif

%install
rm -rf "${RPM_BUILD_ROOT}"
make DESTDIR="${RPM_BUILD_ROOT}" -C %{usbir}/build install
%if ! 0%{?el7}
make DESTDIR="${RPM_BUILD_ROOT}" -C %{lircdrv} install
%endif



# must create the user and group before files are put down
%pre
#TODO: stupid to not support the long versions
# TODO: do NOT specify the uid!
/usr/sbin/useradd -r -M -c "Iguanaworks IR Daemon" -d / -s /sbin/nologin iguanair 2>/dev/null || true
#/usr/sbin/useradd --system -M --comment "Iguanaworks IR Daemon" --home / --shell /sbin/nologin iguanair 2>/dev/null || true

# must add the service after the files are placed
%post
/sbin/chkconfig --add %{name}

# before the files are removed stop the service
%preun
if [ $1 = 0 ]; then
  /sbin/service %{name} stop > /dev/null 2>&1 || true
  /sbin/chkconfig --del %{name}
fi

# after the files are removed nuke the user and group
%postun
if [ $1 = 0 ]; then
  /usr/sbin/userdel iguanair
fi



%files
%defattr(-,root,root,-)
%doc %{usbir}/AUTHORS
%doc %{usbir}/LICENSE
%doc %{usbir}/LICENSE-LGPL
%doc %{usbir}/WHY
%doc %{usbir}/README.txt
%doc %{usbir}/ChangeLog
%doc %{usbir}/examples
%{_bindir}/igdaemon
%{_bindir}/igclient
%{_bindir}/iguanaIR-rescan
%{_libdir}/lib*.so*
%{_libdir}/%{name}/*.so
/usr/lib/systemd/system/%{name}.service
# makes .rpmsave
%config /etc/default/%{name}
# makes .rpmnew
#%config(noreplace) /etc/default/%{name}
/usr/lib/udev/rules.d/80-%{name}.rules
/usr/share/man/man1/igclient.1.gz
/usr/share/man/man1/%{name}-rescan.1.gz
/usr/share/man/man8/igdaemon.8.gz
/usr/lib/tmpfiles.d/iguanair.conf


%files devel
%{_includedir}/%{name}.h
%{_libdir}/lib%{name}.so


%files -n python-iguanaIR
%{_libdir}/python2.*/site-packages/*


%if ! 0%{?el7}
%files -n python3-iguanaIR
%{_libdir}/python3.*/site-packages/*
%endif


%files reflasher
%{_datadir}/%{name}-reflasher/
%{_bindir}/%{name}-reflasher
/usr/share/man/man1/%{name}-reflasher.1.gz


%if ! 0%{?el7}
%files -n lirc-drv-iguanair
%doc %{lircdrv}/iguanair.txt
%doc %{lircdrv}/LICENSE
/etc/modprobe.d/60-blacklist-kernel-iguanair.conf
%{_libdir}/lirc/plugins/iguanair.so
/usr/share/doc/lirc/plugindocs/iguanair.html
/usr/share/lirc/configs/iguanair.conf
%endif



%changelog
* Sat Nov 20 2010 Joseph Dunn <jdunn@iguanaworks.net> 1.0-1
- Significant release to try and get back on track.

* Fri Jun 27 2008 Joseph Dunn <jdunn@iguanaworks.net> 0.96-1
- Bug fix release.

* Thu Mar 27 2008 Joseph Dunn <jdunn@iguanaworks.net> 0.95-1
- Decided to do another release to fix a udev problem.

* Sun Mar 23 2008 Joseph Dunn <jdunn@iguanaworks.net> 0.94-1
- Better windows support, a pile of bugs fixed.  Works with newer
  firmwares (version 0x102) including frequency and channel support
  with or without LIRC.

* Sat Mar 10 2007 Joseph Dunn <jdunn@iguanaworks.net> 0.31-1
- First release with tentative win32 and darwin support.  Darwin needs
  some work, and windows needs to interface with applications.

* Thu Feb 1 2007 Joseph Dunn <jdunn@iguanaworks.net> 0.30-1
- Added a utility to change the frequency on firmware version 3, and
  had to make iguanaRemoveData accessible to python code.

* Sun Jan 21 2007 Joseph Dunn <jdunn@iguanaworks.net> 0.29-1
- Last currently known problem in the driver.  Using clock_gettime
  instead of gettimeofday to avoid clock rollbacks.

* Sun Dec 31 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.26-1
- Happy New Years! and a bugfix.  Long standing bug that caused the
  igdaemon to hang is fixed.

* Sun Dec 10 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.25-1
- The socket specification accept a path instead of just an index or
  label.

* Wed Dec 6 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.24-1
- Fixes bad argument parsing in igdaemon, and the init script *should*
  work for fedora and debian now.

* Wed Oct 18 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.19-1
- A real release has been made, and we will try to keep track of version
  numbers a bit better now.

* Sat Sep 23 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.10-1
- Preparing for a real release.

* Tue Jul 11 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.9-1
- Switch to using udev instead of hotplug.

* Mon Jul 10 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.8-1
- Version number bumps, and added python support and package.

* Mon Mar 27 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.5-1
- Version number bump.

* Mon Mar 20 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.4-1
- Version number bump.

* Tue Mar 07 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.3-1
- Packaged a client library, and header file.

* Tue Mar 07 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.2-2
- Added support for chkconfig

* Tue Mar 07 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.2-1
- Added files for hotplug.

* Tue Mar 07 2006 Joseph Dunn <jdunn@iguanaworks.net> 0.1-1
- Initial RPM spec file.
