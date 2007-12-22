Name:           iguanair-reflasher
Version:        0.4
Release:        1%{?dist}
Summary:        Updates IguanaWorks USB IR devices

Group:          Applications/Tools
License:        GPL2
URL:            http://iguanaworks.net
Source0:        %{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:      noarch

Requires:       iguanaIR-python >= 0.91

%description
This package contains the Python reflashing script and firmware
necessary to upgrade IguanaWorks USB IR transceivers.  If you have no
idea what this means, you don't need it.


%prep
%setup -q


%build
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
/usr/lib/%{name}
/usr/bin/%{name}
%doc



%changelog
