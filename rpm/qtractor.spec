#
# spec file for package qtractor
#
# Copyright (C) 2005-2023, rncbc aka Rui Nuno Capela. All rights reserved.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.
#
# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

%define name    qtractor
%define version 0.9.35
%define release 78.1

%define _prefix	/usr

%if %{defined fedora}
%define debug_package %{nil}
%endif

%if 0%{?fedora_version} >= 34 || 0%{?suse_version} > 1500 || ( 0%{?sle_version} == 150200 && 0%{?is_opensuse} )
%define qt_major_version  6
%else
%define qt_major_version  5
%endif

Summary:	An Audio/MIDI multi-track sequencer
Name:		%{name}
Version:	%{version}
Release:	%{release}
License:	GPL-2.0+
Group:		Productivity/Multimedia/Sound/Midi
Source0:	%{name}-%{version}.tar.gz
URL:		https://qtractor.org/
Packager:	rncbc.org

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

BuildRequires:	coreutils
BuildRequires:	pkgconfig
BuildRequires:	glibc-devel
BuildRequires:	cmake >= 3.15
%if 0%{?sle_version} >= 150200 && 0%{?is_opensuse}
BuildRequires:	gcc10 >= 10
BuildRequires:	gcc10-c++ >= 10
%define _GCC	/usr/bin/gcc-10
%define _GXX	/usr/bin/g++-10
%else
BuildRequires:	gcc >= 10
BuildRequires:	gcc-c++ >= 10
%define _GCC	/usr/bin/gcc
%define _GXX	/usr/bin/g++
%endif
%if 0%{qt_major_version} == 6
%if 0%{?sle_version} == 150200 && 0%{?is_opensuse}
BuildRequires:	qtbase6.5-static >= 6.1
BuildRequires:	qttools6.5-static
BuildRequires:	qttranslations6.5-static
BuildRequires:	qtsvg6.5-static
#BuildRequires:	qtwayland6-static
%else
BuildRequires:	cmake(Qt6LinguistTools)
BuildRequires:	pkgconfig(Qt6Core)
BuildRequires:	pkgconfig(Qt6Gui)
BuildRequires:	pkgconfig(Qt6Widgets)
BuildRequires:	pkgconfig(Qt6Xml)
BuildRequires:	pkgconfig(Qt6Svg)
BuildRequires:	pkgconfig(Qt6Network)
%endif
%else
BuildRequires:	cmake(Qt5LinguistTools)
BuildRequires:	pkgconfig(Qt5Core)
BuildRequires:	pkgconfig(Qt5Gui)
BuildRequires:	pkgconfig(Qt5Widgets)
BuildRequires:	pkgconfig(Qt5Xml)
BuildRequires:	pkgconfig(Qt5Svg)
BuildRequires:	pkgconfig(Qt5Network)
BuildRequires:	pkgconfig(Qt5X11Extras)
%endif
%if %{defined fedora}
BuildRequires:	jack-audio-connection-kit-devel
%else
BuildRequires:	pkgconfig(jack)
%endif
BuildRequires:	pkgconfig(alsa)

%if %{defined fedora}
BuildRequires:	rubberband-devel
%else
BuildRequires:	librubberband-devel
%endif
BuildRequires:	ladspa-devel
BuildRequires:	libmad-devel

BuildRequires:	pkgconfig(sndfile)
BuildRequires:	pkgconfig(liblo)
BuildRequires:	pkgconfig(lv2)
BuildRequires:	pkgconfig(serd-0)
BuildRequires:	pkgconfig(sord-0)
BuildRequires:	pkgconfig(sratom-0)
BuildRequires:	pkgconfig(lilv-0)
%if 0%{qt_major_version} < 6
BuildRequires:	pkgconfig(suil-0)
%endif
BuildRequires:	pkgconfig(vorbis)
BuildRequires:	pkgconfig(samplerate)
BuildRequires:	pkgconfig(dssi)
BuildRequires:	pkgconfig(zlib)
BuildRequires:	pkgconfig(aubio)

BuildRequires:	gtk2-devel
%if %{defined fedora}
BuildRequires:	gtkmm24-devel
%else
BuildRequires:	gtkmm2-devel
%endif

%if 0%{?is_opensuse}
BuildRequires:	libicu-devel
%endif

Requires(post):		desktop-file-utils, shared-mime-info
Requires(postun):	desktop-file-utils, shared-mime-info

%description
Qtractor is an Audio/MIDI multi-track sequencer application
written in C++ with the Qt framework. Target platform will be
Linux, where the Jack Audio Connection Kit (JACK) for audio,
and the Advanced Linux Sound Architecture (ALSA) for MIDI,
are the main infrastructures to evolve as a fairly-featured
Linux Desktop Audio Workstation GUI, specially dedicated to
the personal home-studio.

%prep
%setup -q

%build
%if 0%{?sle_version} == 150200 && 0%{?is_opensuse}
source /opt/qt6.5-static/bin/qt6.5-static-env.sh
%endif
CXX=%{_GXX} CC=%{_GCC} \
cmake -DCMAKE_INSTALL_PREFIX=%{_prefix} -Wno-dev -B build
cmake --build build %{?_smp_mflags}

%install
DESTDIR="%{buildroot}" \
cmake --install build

%post
%mime_database_post
%desktop_database_post

%postun
%mime_database_postun
%desktop_database_postun

%clean
[ -d "%{buildroot}" -a "%{buildroot}" != "/" ] && %__rm -rf "%{buildroot}"

%files
%defattr(-,root,root)
%license LICENSE
%doc README TRANSLATORS ChangeLog
%dir %{_libdir}/%{name}
#dir %{_datadir}/mime
#dir %{_datadir}/mime/packages
#dir %{_datadir}/applications
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/32x32
%dir %{_datadir}/icons/hicolor/32x32/apps
%dir %{_datadir}/icons/hicolor/32x32/mimetypes
%dir %{_datadir}/icons/hicolor/scalable
%dir %{_datadir}/icons/hicolor/scalable/apps
%dir %{_datadir}/icons/hicolor/scalable/mimetypes
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/translations
%dir %{_datadir}/metainfo
#dir %{_datadir}/man
#dir %{_datadir}/man/man1
#dir %{_datadir}/man/fr
#dir %{_datadir}/man/fr/man1
%{_bindir}/%{name}
%{_libdir}/%{name}/%{name}_plugin_scan
%{_datadir}/mime/packages/org.rncbc.%{name}.xml
%{_datadir}/applications/org.rncbc.%{name}.desktop
%{_datadir}/icons/hicolor/32x32/apps/org.rncbc.%{name}.png
%{_datadir}/icons/hicolor/scalable/apps/org.rncbc.%{name}.svg
%{_datadir}/icons/hicolor/32x32/mimetypes/org.rncbc.%{name}.application-x-%{name}*.png
%{_datadir}/icons/hicolor/scalable/mimetypes/org.rncbc.%{name}.application-x-%{name}*.svg
%{_datadir}/%{name}/translations/%{name}_*.qm
%{_datadir}/metainfo/org.rncbc.%{name}.metainfo.xml
%{_datadir}/man/man1/%{name}.1.gz
%{_datadir}/man/fr/man1/%{name}.1.gz

%changelog
* Thu Sep 14 2023 Rui Nuno Capela <rncbc@rncbc.org> 0.9.35
- An End-of-Summer'23'23 Release.
* Thu Jun  8 2023 Rui Nuno Capela <rncbc@rncbc.org> 0.9.34
- A Spring'23 Release.
* Mon Mar 27 2023 Rui Nuno Capela <rncbc@rncbc.org> 0.9.33
- An Early-Spring'23 Hotfix Release.
* Sat Mar 25 2023 Rui Nuno Capela <rncbc@rncbc.org> 0.9.32
- An Early-Spring'23 Release.
* Thu Jan 26 2023 Rui Nuno Capela <rncbc@rncbc.org> 0.9.31
- A Winter'23 Release.
* Fri Dec 30 2022 Rui Nuno Capela <rncbc@rncbc.org> 0.9.30
- An End-of-Year'22 Release.
* Wed Oct  5 2022 Rui Nuno Capela <rncbc@rncbc.org> 0.9.29
- An Early-Autumn'22 Release.
* Sat Sep  3 2022 Rui Nuno Capela <rncbc@rncbc.org> 0.9.28
- A Late-Summer'22 Release.
* Thu Jul  7 2022 Rui Nuno Capela <rncbc@rncbc.org> 0.9.27
- An Early-Summer'22 Release.
* Sat Apr  9 2022 Rui Nuno Capela <rncbc@rncbc.org> 0.9.26
- A Spring'22 Release.
* Sun Jan  9 2022 Rui Nuno Capela <rncbc@rncbc.org> 0.9.25
- A Winter'22 Release.
* Sat Oct 16 2021 Rui Nuno Capela <rncbc@rncbc.org> 0.9.24
- Autumn'21 release.
* Sat Jul 10 2021 Rui Nuno Capela <rncbc@rncbc.org> 0.9.23
- Early-Summer'21 release.
* Fri May 14 2021 Rui Nuno Capela <rncbc@rncbc.org> 0.9.22
- Spring'21 release.
* Thu Mar 18 2021 Rui Nuno Capela <rncbc@rncbc.org> 0.9.21
- End-of-Winter'21 release.
* Fri Feb 12 2021 Rui Nuno Capela <rncbc@rncbc.org> 0.9.20
- Winter'21 release.
* Sun Dec 20 2020 Rui Nuno Capela <rncbc@rncbc.org> 0.9.19
- Winter'20 release.
* Fri Oct 30 2020 Rui Nuno Capela <rncbc@rncbc.org> 0.9.18
- Fall'20 release.
* Tue Sep 15 2020 Rui Nuno Capela <rncbc@rncbc.org> 0.9.17
- End-of-Summer'20 release.
* Fri Aug  7 2020 Rui Nuno Capela <rncbc@rncbc.org> 0.9.16
- Summer'20 release.
* Sat Jun 27 2020 Rui Nuno Capela <rncbc@rncbc.org> 0.9.15
- Early-Summer'20 release.
* Thu May  7 2020 Rui Nuno Capela <rncbc@rncbc.org> 0.9.14
- Mid-Spring'20 release.
* Sat Mar 28 2020 Rui Nuno Capela <rncbc@rncbc.org> 0.9.13
- Spring'20 release.
* Sat Dec 28 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.12
- Winter'19 release.
* Sat Nov  9 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.11
- Mauerfall'30 release.
* Sat Oct 12 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.10
- Autumn'19 beta release.
* Wed Jul 24 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.9
- Summer'19 beta release.
* Fri May 31 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.8
- Spring'19 beta release.
* Tue Apr 16 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.7
- Spring-Break'19 release.
* Wed Mar 20 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.6
- Pre-LAC2019 release frenzy.
* Thu Feb 14 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.5
- Valentines'19 hotfix release.
* Thu Feb  7 2019 Rui Nuno Capela <rncbc@rncbc.org> 0.9.4
- Winter'19 beta release.
* Fri Dec  7 2018 Rui Nuno Capela <rncbc@rncbc.org> 0.9.3
- End of Autumn'18 beta release.
* Sun Sep  9 2018 Rui Nuno Capela <rncbc@rncbc.org> 0.9.2
- Summer'18 beta release.
* Tue May 29 2018 Rui Nuno Capela <rncbc@rncbc.org> 0.9.1
- Pre-LAC2018 release frenzy.
* Thu Mar 22 2018 Rui Nuno Capela <rncbc@rncbc.org> 0.9.0
- Early Spring'18 beta release.
* Tue Jan 30 2018 Rui Nuno Capela <rncbc@rncbc.org> 0.8.6
- Winter'18 beta release.
* Mon Dec  4 2017 Rui Nuno Capela <rncbc@rncbc.org> 0.8.5
- Autumn'17 beta release.
* Wed Sep 20 2017 Rui Nuno Capela <rncbc@rncbc.org> 0.8.4
- End of Summer'17 beta release.
* Fri Jun 30 2017 Rui Nuno Capela <rncbc@rncbc.org> 0.8.3
- The Stickiest Tauon beta release.
* Wed May 10 2017 Rui Nuno Capela <rncbc@rncbc.org> 0.8.2
- A Stickier Tauon beta release.
* Fri Feb 17 2017 Rui Nuno Capela <rncbc@rncbc.org> 0.8.1
- The Sticky Tauon beta release.
* Mon Nov 21 2016 Rui Nuno Capela <rncbc@rncbc.org> 0.8.0
- The Snobbiest Graviton beta release.
* Wed Sep 21 2016 Rui Nuno Capela <rncbc@rncbc.org> 0.7.9
- A Snobbier Graviton beta release.
* Thu Jun 23 2016 Rui Nuno Capela <rncbc@rncbc.org> 0.7.8
- The Snobby Graviton beta release.
* Wed Apr 27 2016 Rui Nuno Capela <rncbc@rncbc.org> 0.7.7
- The Haziest Photon beta release.
* Tue Apr  5 2016 Rui Nuno Capela <rncbc@rncbc.org> 0.7.6
- A Hazier Photon beta release.
* Mon Mar 21 2016 Rui Nuno Capela <rncbc@rncbc.org> 0.7.5
- Hazy Photon beta release.
* Thu Jan 28 2016 Rui Nuno Capela <rncbc@rncbc.org> 0.7.4
- Tackiest Gluon beta release.
* Tue Dec 29 2015 Rui Nuno Capela <rncbc@rncbc.org> 0.7.3
- A Tackier Gluon beta release.
* Thu Dec 10 2015 Rui Nuno Capela <rncbc@rncbc.org> 0.7.2
- Tacky Gluon beta release.
* Fri Oct  9 2015 Rui Nuno Capela <rncbc@rncbc.org> 0.7.1
- Meson Dope beta release.
* Fri Jul 24 2015 Rui Nuno Capela <rncbc@rncbc.org> 0.7.0
- Muon Base beta release.
* Wed May 27 2015 Rui Nuno Capela <rncbc@rncbc.org> 0.6.7
- Lepton Acid beta release.
* Sun Mar 29 2015 Rui Nuno Capela <rncbc@rncbc.org> 0.6.6
- Lazy Tachyon beta release.
* Fri Jan 30 2015 Rui Nuno Capela <rncbc@rncbc.org> 0.6.5
- Fermion Ray beta release.
* Mon Nov 24 2014 Rui Nuno Capela <rncbc@rncbc.org> 0.6.4
- Baryon Throne beta release.
* Mon Sep 22 2014 Rui Nuno Capela <rncbc@rncbc.org> 0.6.3
- Armed Hadron beta release.
* Mon Jul  7 2014 Rui Nuno Capela <rncbc@rncbc.org> 0.6.2
- Boson Walk beta release.
* Tue Apr 29 2014 Rui Nuno Capela <rncbc@rncbc.org> 0.6.1
- Bitsy Sweet beta release.
* Fri Mar 21 2014 Rui Nuno Capela <rncbc@rncbc.org> 0.6.0
- Byte Bald beta release.
* Tue Dec 31 2013 Rui Nuno Capela <rncbc@rncbc.org> 0.5.12
- Mike November release.
* Mon Oct  7 2013 Rui Nuno Capela <rncbc@rncbc.org> 0.5.11
- Lima Oscar release.
* Thu Jul 18 2013 Rui Nuno Capela <rncbc@rncbc.org> 0.5.10
- Kilo Papa release.
* Thu Jun  6 2013 Rui Nuno Capela <rncbc@rncbc.org> 0.5.9
- Juliet Quebec release.
* Tue Mar 19 2013 Rui Nuno Capela <rncbc@rncbc.org> 0.5.8
- India Romeo release.
* Thu Dec 27 2012 Rui Nuno Capela <rncbc@rncbc.org> 0.5.7
- Hotel Sierra release.
* Tue Oct 02 2012 Rui Nuno Capela <rncbc@rncbc.org> 0.5.6
- Golf Tango release.
* Fri Jun 15 2012 Rui Nuno Capela <rncbc@rncbc.org> 0.5.5
- Foxtrot Uniform release.
* Thu Mar 01 2012 Rui Nuno Capela <rncbc@rncbc.org> 0.5.4
- Echo Victor release.
* Wed Dec 28 2011 Rui Nuno Capela <rncbc@rncbc.org> 0.5.3
- Delta Whisky release.
* Fri Dec 16 2011 Rui Nuno Capela <rncbc@rncbc.org> 0.5.2
- Charlie X-ray release.
* Wed Oct 05 2011 Rui Nuno Capela <rncbc@rncbc.org> 0.5.1
- Bravo Yankee release.
* Fri Jul 22 2011 Rui Nuno Capela <rncbc@rncbc.org> 0.5.0
- Alpha Zulu (TYOQA) release.
* Thu May 26 2011 Rui Nuno Capela <rncbc@rncbc.org> 0.4.9
- Final Dudette release.
* Wed Jan 19 2011 Rui Nuno Capela <rncbc@rncbc.org> 0.4.8
- Fiery Demigoddess release.
* Thu Sep 30 2010 Rui Nuno Capela <rncbc@rncbc.org> 0.4.7
- Furious Desertrix release.
* Fri May 21 2010 Rui Nuno Capela <rncbc@rncbc.org> 0.4.6
- Funky Deviless release.
* Sat Jan 23 2010 Rui Nuno Capela <rncbc@rncbc.org> 0.4.5
- A Friskier Demivierge release.
* Sat Jan 16 2010 Rui Nuno Capela <rncbc@rncbc.org> 0.4.4
- Frisky Demivierge release.
* Mon Oct 05 2009 Rui Nuno Capela <rncbc@rncbc.org> 0.4.3
- Fussy Doula release.
* Thu Jun 04 2009 Rui Nuno Capela <rncbc@rncbc.org> 0.4.2
- Flaunty Demoness release.
* Sat Apr 04 2009 Rui Nuno Capela <rncbc@rncbc.org> 0.4.1
- Funky Dominatrix release.
* Fri Mar 13 2009 Rui Nuno Capela <rncbc@rncbc.org> 0.4.0
- Foxy Dryad release.
* Thu Dec 25 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.3.0
- Fluffy Doll release.
* Sun Oct 05 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.2.2
- Flirty Ditz release.
* Sat Aug 30 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.2.1
- Fainty Diva release.
* Fri Jul 18 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.2.0
- Frolic Demoiselle release.
* Fri May 02 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.1.3
- Frugal Damsel release.
* Sun Mar 23 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.1.2
- Frantic Dame release.
* Sat Feb 16 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.1.1
- Futile Duchess release.
* Sat Jan  5 2008 Rui Nuno Capela <rncbc@rncbc.org> 0.1.0
- Frivolous Debutante release.
