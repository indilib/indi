%global forgeurl https://github.com/xsnrg/indi/

Name: indi
Version: 1.8.6.git
Release: 1%{?dist}
Summary: Instrument Neutral Distributed Interface

License: LGPLv2+ and GPLv2+
# See COPYRIGHT file for a description of the licenses and files covered

Obsoletes: libindi
Provides: libindi = %{version}-%{release}

%forgemeta -i

URL: http://www.indilib.org
Source0: %{forgesource} 

BuildRequires: cmake
BuildRequires: libfli-devel
BuildRequires: libnova-devel
BuildRequires: qt5-qtbase-devel
BuildRequires: systemd
BuildRequires: libogg-devel
BuildRequires: libtheora-devel
BuildRequires: fftw-devel
BuildRequires: gmock-devel

BuildRequires: pkgconfig(fftw3)
BuildRequires: pkgconfig(cfitsio)
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(gsl)
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
%forgesetup
# For Fedora we want to put udev rules in %{_udevrulesdir}
sed -i 's|/lib/udev/rules.d|%{_udevrulesdir}|g' CMakeLists.txt
chmod -x drivers/telescope/pmc8driver.h
chmod -x drivers/telescope/pmc8driver.cpp

%build
# This package tries to mix and match PIE and PIC which is wrong and will
# trigger link errors when LTO is enabled.
# Disable LTO
%define _lto_cflags %{nil}

%cmake .
make VERBOSE=1 %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

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

%changelog
* Thu Jul 09 2020 Sergio Pascual <sergiopr@fedoraproject.org> 1.8.5-1
- New upstream source (1.8.5)

* Wed Jul 08 2020 Jeff Law <law@redhat.com> 1.8.1-4
- Disable LTO

* Sun Mar 01 2020 Sergio Pascual <sergiopr@fedoraproject.org> 1.8.1-3
- Patch stime in arm (bz#1799588)

* Wed Jan 29 2020 Fedora Release Engineering <releng@fedoraproject.org> - 1.8.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_32_Mass_Rebuild

* Sun Oct 20 2019 Christian Dersch <lupinix@fedoraproject.org> - 1.8.1-1
- new version

* Tue Aug 20 2019 Susi Lehtola <jussilehtola@fedoraproject.org> - 1.7.7-3
- Rebuilt for GSL 2.6.

* Thu Jul 25 2019 Fedora Release Engineering <releng@fedoraproject.org> - 1.7.7-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_31_Mass_Rebuild

* Sun Apr 28 2019 Christian Dersch <lupinix@mailbox.org> - 1.7.7-1
- new version

* Fri Feb 01 2019 Fedora Release Engineering <releng@fedoraproject.org> - 1.7.4-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Wed Jan 23 2019 Björn Esser <besser82@fedoraproject.org> - 1.7.4-3
- Append curdir to CMake invokation. (#1668512)

* Tue Jul 31 2018 Florian Weimer <fweimer@redhat.com> - 1.7.4-2
- Rebuild with fixed binutils

* Sun Jul 29 2018 Christian Dersch <lupinix@mailbox.org> - 1.7.4-1
- new version

* Fri Jul 13 2018 Fedora Release Engineering <releng@fedoraproject.org> - 1.7.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Thu Jun 21 2018 Sergio Pascual <sergiopr@fedoraproject.org> 1.7.2-2
- Patch udev rule to remove plugdev (bz #1577332)

* Sat May 26 2018 Christian Dersch <lupinix@mailbox.org> - 1.7.2-1
- new version

* Fri Feb 23 2018 Christian Dersch <lupinix@mailbox.org> - 1.6.2-3
- rebuilt for cfitsio 3.420 (so version bump)

* Wed Feb 07 2018 Fedora Release Engineering <releng@fedoraproject.org> - 1.6.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Mon Jan 08 2018 Christian Dersch <lupinix@mailbox.org> - 1.6.2-1
- new version

* Mon Jan 08 2018 Christian Dersch <lupinix@mailbox.org> - 1.6.1-1
- new version

* Tue Jan 02 2018 Christian Dersch <lupinix@fedoraproject.org> - 1.6.0-1
- new version
- split shared libraries into -libs subpackage, to be multiarch clean

* Sat Oct 07 2017 Christian Dersch <lupinix@mailbox.org> - 1.5.0-1
- new version

* Sat Aug 05 2017 Christian Dersch <lupinix@mailbox.org> - 1.4.1-4
- Rebuild (gsl)

* Thu Aug 03 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.4.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Wed Jul 26 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.4.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Mon Feb 27 2017 Christian Dersch <lupinix@mailbox.org> - 1.4.1-1
- new version

* Sun Feb 26 2017 Christian Dersch <lupinix@mailbox.org> - 1.4.0-1
- new version

* Fri Feb 10 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Thu Dec 15 2016 derschc <lupinix@mailbox.org> - 1.3.1-1
- new version

* Sat May 07 2016 Christian Dersch <lupinix@mailbox.org> - 1.2.0-3
- Unified spec for Fedora and EPEL

* Mon Feb 22 2016 Orion Poplawski <orion@cora.nwra.com> - 1.2.0-2
- Rebuild for gsl 2.1

* Tue Feb 02 2016 Christian Dersch <lupinix@mailbox.org> - 1.2.0-1
- new version

* Mon Sep 07 2015 Christian Dersch <lupinix@fedoraproject.org> 1.1.0-1
- New upstream source (1.1.0)
- New BR: curl-devel
- New BR: systemd (for udevrulesdir macro)

* Mon Feb 23 2015 Sergio Pascual <sergiopr@fedoraproject.org> 1.0.0-1
- New upstream source (1.0.0)

* Mon Oct 06 2014 Sergio Pascual <sergiopr@fedoraproject.org> 0.9.9-1
- New upstream source (0.9.9)
- Removed patches ported upstream

* Sun Aug 17 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.8.1-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Mon Jul 28 2014 Peter Robinson <pbrobinson@fedoraproject.org> 0.9.8.1-6
- rebuild (libnova)

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.8.1-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Mon May 12 2014 Marcin Juszkiewicz <mjuszkiewicz@redhat.com> 0.9.8.1-4
- Add AArch64 definitions where needed.

* Fri May 09 2014 Sergio Pascual <sergiopr@fedoraproject.org> 0.9.8.1-3
- Plugin directory has to be arch-dependent

* Sun Apr 27 2014 Christian Dersch <chrisdersch@gmail.com> 0.9.8.1-2
- Fix wrong upstream version

* Thu Apr 24 2014 Sergio Pascual <sergiopr@fedoraproject.org> 0.9.8.1-1
- New upstream source (0.9.8.1)

* Tue Dec 03 2013 Sergio Pascual <sergiopr@fedoraproject.org> 0.9.7-1
- New upstream source (0.9.7)

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.6-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Wed Jul 17 2013 Sergio Pascual <sergiopr@fedoraproject.org> 0.9.6-5
- rebuild (cfitsio 3.350)

* Fri Mar 22 2013 Rex Dieter <rdieter@fedoraproject.org> 0.9.6-4
- rebuild (cfitsio)

* Wed Mar 20 2013 Rex Dieter <rdieter@fedoraproject.org> 0.9.6-3
- rebuild (cfitsio)

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.6-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Fri Dec 07 2012 Sergio Pascual <sergiopr at fedoraproject.org> - 0.9.6-1
- New upstream source
- Added udev rules (in wrong directory)
- Fixed FSF previous address bug, new appear

* Tue Jan 24 2012 Sergio Pascual <sergiopr at fedoraproject.org> - 0.9-1
- New upstream source
- All instruments created (solves #653690)
- Library does not call exit()
- Library does not build require boost-devel

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.8-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Fri Jul 15 2011 Sergio Pascual <sergiopr at fedoraproject.org> - 0.8-1
- New upstream source
- Submitted a bug upstream, address of FSF is incorrect in some files

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.6.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Fri Nov 26 2010 Sergio Pascual <sergiopr at fedoraproject.org> - 0.6.2-2
- Adding manually telescopes missing (bz #653690)

* Thu Jul 29 2010 Sergio Pascual <sergiopr at fedoraproject.org> - 0.6.2-1
- New upstream source (bz #618776)
- Bz #564842 fixed upstream, patch removed
- With pkgconfig file

* Wed Feb 17 2010 Sergio Pascual <sergiopr at fedoraproject.org> - 0.6-11
- Added missing -lm in indi_sbig_stv. Fixes bz #564842

* Fri Jan 08 2010 Sergio Pascual <sergiopr at fedoraproject.org> - 0.6-10
- EVR bump, rebuilt with new libnova

* Tue Dec 22 2009 Sergio Pascual <sergiopr at fedoraproject.org> - 0.6-9
- Static library moved to its own subpackage

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.6-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.6-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Wed Feb 18 2009 Sergio Pascual <sergiopr at fedoraproject.org> -  0.6-6
- Provides libindi-static

* Tue Feb 17 2009 Sergio Pascual <sergiopr at fedoraproject.org> -  0.6-5
- Need to provide the static library libindidriver.a to build indi-apogee

* Sat Feb 14 2009 Sergio Pascual <sergiopr at fedoraproject.org> -  0.6-4
- Fixed patch to find cfitsio

* Sat Feb 14 2009 Sergio Pascual <sergiopr at fedoraproject.org> -  0.6-3
- Patch to detect cfitsio in all architectures

* Fri Feb 06 2009 Sergio Pascual <sergiopr at fedoraproject.org> -  0.6-2
- Commands (rm, make) instead of macros
- Upstream bug about licenses (GPLv2 missing)
- Upstream bug about libindi calling exit

* Mon Jan 26 2009 Sergio Pascual <sergiopr at fedoraproject.org> -  0.6-1
- First version


