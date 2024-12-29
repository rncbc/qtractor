#
# spec file for package qtractor
#
# Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

Summary:	An Audio/MIDI multi-track sequencer
Name:		qtractor
Version:	1.5.1
Release:	8.1
License:	GPL-2.0-or-later
Group:		Productivity/Multimedia/Sound/Midi
Source:		%{name}-%{version}.tar.gz
URL:		https://qtractor.org/
#Packager:	rncbc.org

%if 0%{?fedora_version} >= 34 || 0%{?suse_version} > 1500 || ( 0%{?sle_version} == 150200 && 0%{?is_opensuse} )
%define qt_major_version  6
%else
%define qt_major_version  5
%endif

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
BuildRequires:	qtbase6.8-static >= 6.8
BuildRequires:	qttools6.8-static
BuildRequires:	qttranslations6.8-static
BuildRequires:	qtsvg6.8-static
%else
BuildRequires:	cmake(Qt6LinguistTools)
BuildRequires:	pkgconfig(Qt6Core)
BuildRequires:	pkgconfig(Qt6Gui)
BuildRequires:	pkgconfig(Qt6Widgets)
BuildRequires:	pkgconfig(Qt6Svg)
BuildRequires:	pkgconfig(Qt6Xml)
BuildRequires:	pkgconfig(Qt6Network)
%endif
%else
BuildRequires:	cmake(Qt5LinguistTools)
BuildRequires:	pkgconfig(Qt5Core)
BuildRequires:	pkgconfig(Qt5Gui)
BuildRequires:	pkgconfig(Qt5Widgets)
BuildRequires:	pkgconfig(Qt5Svg)
BuildRequires:	pkgconfig(Qt5Xml)
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
source /opt/qt6.8-static/bin/qt6.8-static-env.sh
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


%files
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
%dir %{_datadir}/%{name}/instruments
%dir %{_datadir}/%{name}/palette
%dir %{_datadir}/%{name}/audio
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
%{_datadir}/%{name}/audio/metro_*.wav
%{_datadir}/%{name}/palette/*.conf
%{_datadir}/%{name}/instruments/Standard*.ins


%changelog
* Mon Dec 30 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.5.1
- An(other) End-of-Year'24 Release.
* Mon Dec 16 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.5.0
- An End-of-Year'24 Release.
* Fri Nov  1 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.4.0
- A Halloween'24 Release.
* Fri Oct  4 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.3.0
- An Early-Fall'24 Release.
* Thu Aug 29 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.2.0
- A Mid-Summer'24 Release.
* Mon Aug  5 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.1.1
- A Summer'24 Hotfix Release.
* Fri Aug  2 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.1.0
- A Summer'24 Release.
* Fri Jun 21 2024 Rui Nuno Capela <rncbc@rncbc.org> 1.0.0
- An Unthinkable Release.
