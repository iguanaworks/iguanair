Name:           iguanaIR-reflasher
Version:        0.1
Release:        1%{?dist}
Summary:        Updates IguanaWorks USB IR devices

Group:          Applications/Tools
License:        GPL2
URL:            http://iguanaworks.net
Source0:        reflasher-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires:       iguanaIR-python >= 0.91

%description
This package contains the Python reflashing script and firmware
necessary to upgrade IguanaWorks USB IR transceivers.  If you have no
idea what this means, you don't need it.


%prep
rm -rf reflasher-%{version}
tar -xjf reflasher-0.1.tar.bz2
cd reflasher


%build
cd reflasher
make %{?_smp_mflags}


%install
cd reflasher
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
/usr/lib/iguanaIR-reflasher
/usr/bin/iguanaIR-reflasher
%doc



%changelog
