%define debug_packages %{nil}
%define debug_package %{nil}

%define kernel %(uname -r)
%define destdir /lib/modules/%{kernel}

Name:		slidesx-energymeter
Version:	1.0
Release:	1%{?dist}
Summary:	Provides the driver for the MEGWARE SlideSX USB Energy Meter

License:	GPL
Group:		System Environment/Kernel
Source:		slidesx-energymeter.tar.gz
BuildArch:	x86_64

%description
This RPM provides the driver for the MEGWARE SlideSX USB Energy Meter. Measured values are exported to user space via sysfs.

%prep
%setup -q

%build
make

%install
mkdir -p $RPM_BUILD_ROOT/%{destdir}
mv slidesx-energymeter.ko $RPM_BUILD_ROOT/%{destdir}
mkdir -p $RPM_BUILD_ROOT/etc/modules-load.d
echo "slidesx-energymeter" >> $RPM_BUILD_ROOT/etc/modules-load.d/slidesx-energymeter.conf

%files
%defattr(-,root,root,-)
%{destdir}/slidesx-energymeter.ko
/etc/modules-load.d/slidesx-energymeter.conf

%post
depmod -ae %{kernel}

%changelog
* Thu Feb 23 2017 Sebastian Siegert <sebastian.siegert@megware.com>
- Initial Release

