%global forgeurl https://github.com/indilib/indi.git

Name: indi
Version: 2.2.5.git
Release: %(T=%{_topdir}; mkdir -p $T 2>/dev/null; [ -f $T/.release-stamp-%{name} ] || date -u +%%Y%%m%%d%%H%%M%%S > $T/.release-stamp-%{name} 2>/dev/null; cat $T/.release-stamp-%{name} 2>/dev/null || date -u +%%Y%%m%%d%%H%%M%%S)%{?dist}
Summary: Instrument Neutral Distributed Interface

License: LGPLv2+ and GPLv2+
# See COPYRIGHT file for a descriptions of the licenses and files covered

Provides: libindi = %{version}-%{release}

URL: http://www.indilib.org
Source0: https://github.com/indilib/%{name}/archive/master.tar.gz

BuildRequires: cmake
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: libnova-devel
BuildRequires: qt5-qtbase-devel
BuildRequires: systemd
BuildRequires: libogg-devel
BuildRequires: libtheora-devel
BuildRequires: fftw-devel
BuildRequires: libev-devel
BuildRequires: gmock-devel
BuildRequires: rtl-sdr-devel

BuildRequires: pkgconfig(fftw3)
BuildRequires: pkgconfig(cfitsio)
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(gsl)
BuildRequires: pkgconfig(erfa)
BuildRequires: pkgconfig(libjpeg)
BuildRequires: pkgconfig(libusb-1.0)
BuildRequires: pkgconfig(zlib)

Requires: %{name}-libs%{?_isa} = %{version}-%{release}

%description
INDI is a distributed control protocol designed to operate
astronomical instrumentation. INDI is small, flexible, easy to parse,
and scalable. It supports common DCS functions such as remote control,
data acquisition, monitoring, and a lot more.


%package devel
Summary: Libraries, includes, etc. used to develop an application with %{name}
Requires: %{name}-libs%{?_isa} = %{version}-%{release}
Requires: %{name}-static%{?_isa} = %{version}-%{release}

%description devel
These are the header files needed to develop a %{name} application


%package libs
Summary: INDI shared libraries

%description libs
These are the shared libraries of INDI.


%package static
Summary: Static libraries, includes, etc. used to develop an application with %{name}
Requires: %{name}-libs%{?_isa} = %{version}-%{release}

%description static
Static library needed to develop a %{name} application

%prep
%autosetup -p1 -n %{name}-master

# For Fedora we want to put udev rules in {_udevrulesdir}
sed -i 's|/lib/udev/rules.d|%{_udevrulesdir}|g' CMakeLists.txt
chmod -x drivers/telescope/pmc8driver.h
chmod -x drivers/telescope/pmc8driver.cpp


%build
# This package tries to mix and match PIE and PIC which is wrong and will
# trigger link errors when LTO is enabled.
# Disable LTO
%define _lto_cflags %{nil}

# TODO: fix pre-existing GCC -Wstringop-truncation warnings in
# libs/indibase/webcam/v4l2_base.cpp (strncpy bound == destination size),
# then remove -DFIX_WARNINGS=OFF to build with -Werror like CI does.
%cmake -DFIX_WARNINGS=OFF
%cmake_build

%install
%cmake_install

%ldconfig_scriptlets libs

%files
%license COPYING.BSD COPYING.GPL COPYING.LGPL COPYRIGHT LICENSE
%doc AUTHORS ChangeLog NEWS README
%{_bindir}/*
%{_datadir}/indi
%{_udevrulesdir}/*.rules

%files libs
%license COPYING.BSD COPYING.GPL COPYING.LGPL COPYRIGHT LICENSE
%{_libdir}/*.so.*
%{_libdir}/indi/MathPlugins

%files devel
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc

%files static
%{_libdir}/*.a
