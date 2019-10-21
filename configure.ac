# Process this file with autoconf to produce a configure script.
AC_INIT(Qtractor, 0.9.11, rncbc@rncbc.org, qtractor)

AC_CONFIG_SRCDIR(src/qtractor.cpp)
AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES(Makefile qtractor.spec src/src.pri)
AC_CONFIG_FILES(src/qtractor_plugin_scan.pri)

# Build version string.
AC_CACHE_VAL([ac_cv_build_version], [
   ac_cv_build_version=$(git describe --tags --dirty --abbrev=6 2>/dev/null)
   if test -n "$ac_cv_build_version"; then
      ac_cv_build_version=$(echo $ac_cv_build_version | sed 's/^[[^_]]*[[_v]]//i')
      ac_cv_build_version=$(echo $ac_cv_build_version | sed 's/-g/git./')
      ac_cv_build_version=$(echo $ac_cv_build_version | sed 's/[[_|-]]\+/./g')
      ac_cv_build_version_extra=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
      if test "x$ac_cv_build_version_extra" != "xmaster"; then
         ac_cv_build_version="$ac_cv_build_version [[$ac_cv_build_version_extra]]"
      fi
   else
      ac_cv_build_version=$PACKAGE_VERSION
   fi
])
ac_build_version="$ac_cv_build_version"
AC_DEFINE_UNQUOTED(CONFIG_BUILD_VERSION, ["$ac_build_version"], [Build version string.])

# Sanitized version string.
AC_CACHE_VAL([ac_cv_version], [
   ac_cv_version=$(echo $PACKAGE_VERSION | sed -r 's/^([[0-9|\.]]+).*$/\1/')
])
ac_version="$ac_cv_version"
AC_DEFINE_UNQUOTED(CONFIG_VERSION, ["$ac_version"], [Version string.])
AC_SUBST(ac_version)

# Set default installation prefix.
AC_PREFIX_DEFAULT(/usr/local)
if test "x$prefix" = "xNONE"; then
   prefix=$ac_default_prefix
fi
if test "x$exec_prefix" = "xNONE"; then
   exec_prefix=$prefix
fi
eval ac_prefix=$prefix
AC_SUBST(ac_prefix)
AC_DEFINE_UNQUOTED(CONFIG_PREFIX, ["$ac_prefix"], [Default installation prefix.])

# Set default installation directories.
eval ac_bindir=$bindir
AC_SUBST(ac_bindir)
AC_DEFINE_UNQUOTED(CONFIG_BINDIR, ["$ac_bindir"], [Default executable binary path.])

eval ac_libdir=$libdir
AC_SUBST(ac_libdir)
AC_DEFINE_UNQUOTED(CONFIG_LIBDIR, ["$ac_libdir"], [Default object library path.])

eval datarootdir=$datarootdir
eval ac_datadir=$datadir
AC_SUBST(ac_datadir)
AC_DEFINE_UNQUOTED(CONFIG_DATADIR, ["$ac_datadir"], [Default arch-idependent data path.])

eval ac_mandir=$mandir
AC_SUBST(ac_mandir)
AC_DEFINE_UNQUOTED(CONFIG_MANDIR, ["$ac_mandir"], [Default man page path.])


# Enable debugging argument option.
AC_ARG_ENABLE(debug,
  AS_HELP_STRING([--enable-debug], [enable debugging (default=no)]),
  [ac_debug="$enableval"])

if test "x$ac_debug" = "xyes"; then
   AC_DEFINE(CONFIG_DEBUG, 1, [Define if debugging is enabled.])
   ac_stacktrace="yes"
   ac_debug="debug"
else
   ac_stacktrace="no"
   ac_debug="release"
fi

AC_SUBST(ac_debug)


# Enable libvorbis(file) availability.
AC_ARG_ENABLE(libvorbis,
  AS_HELP_STRING([--enable-libvorbis], [enable libvorbis interface (default=yes)]),
  [ac_libvorbis="$enableval"],
  [ac_libvorbis="yes"])

# Enable libmad availability.
AC_ARG_ENABLE(libmad,
  AS_HELP_STRING([--enable-libmad], [enable libmad interface (default=yes)]),
  [ac_libmad="$enableval"],
  [ac_libmad="yes"])

# Enable libsamplerate availability.
AC_ARG_ENABLE(libsamplerate,
  AS_HELP_STRING([--enable-libsamplerate], [enable libsamplerate interface (default=yes)]),
  [ac_libsamplerate="$enableval"],
  [ac_libsamplerate="yes"])

# Enable librubberband availability.
AC_ARG_ENABLE(librubberband,
  AS_HELP_STRING([--enable-librubberband], [enable librubberband interface (default=yes)]),
  [ac_librubberband="$enableval"],
  [ac_librubberband="yes"])

# Enable libaudio support.
AC_ARG_ENABLE(libaubio,
  AS_HELP_STRING([--enable-libaubio], [enable libaubio interface support (default=yes)]),
  [ac_libaubio="$enableval"],
  [ac_libaubio="yes"])

# Enable liblo availability.
AC_ARG_ENABLE(liblo,
  AS_HELP_STRING([--enable-liblo], [enable liblo interface (default=yes)]),
  [ac_liblo="$enableval"],
  [ac_liblo="yes"])

# Enable libz availability.
AC_ARG_ENABLE(libz,
  AS_HELP_STRING([--enable-libz], [enable libz interface (default=yes)]),
  [ac_libz="$enableval"],
  [ac_libz="yes"])

# Enable LILV support.
AC_ARG_ENABLE(lilv,
  AS_HELP_STRING([--enable-lilv], [enable LILV interface support (default=yes)]),
  [ac_lilv="$enableval"],
  [ac_lilv="yes"])

# Enable SUIL support.
AC_ARG_ENABLE(suil,
  AS_HELP_STRING([--enable-suil], [enable SUIL interface support (default=yes)]),
  [ac_suil="$enableval"],
  [ac_suil="yes"])

# Enable SSE optimization.
AC_ARG_ENABLE(sse,
  AS_HELP_STRING([--enable-sse], [enable SSE optimization (default=yes)]),
  [ac_sse="$enableval"],
  [ac_sse="yes"])

# Enable LADSPA support.
AC_ARG_ENABLE(ladspa,
  AS_HELP_STRING([--enable-ladspa], [enable LADSPA plug-in support (default=yes)]),
  [ac_ladspa="$enableval"],
  [ac_ladspa="yes"])

# Enable DSSI support.
AC_ARG_ENABLE(dssi,
  AS_HELP_STRING([--enable-dssi], [enable DSSI plug-in support (default=yes)]),
  [ac_dssi="$enableval"],
  [ac_dssi="yes"])

# Enable VST support.
AC_ARG_ENABLE(vst,
  AS_HELP_STRING([--enable-vst], [enable VST plug-in support (default=yes)]),
  [ac_vst="$enableval"],
  [ac_vst="yes"])

AC_ARG_ENABLE(vestige,
  AS_HELP_STRING([--enable-vestige], [enable VeSTige header support (default=yes)]),
  [ac_vestige="$enableval"],
  [ac_vestige="yes"])

# Enable LV2 support.
AC_ARG_ENABLE(lv2,
  AS_HELP_STRING([--enable-lv2], [enable LV2 plug-in support (default=yes)]),
  [ac_lv2="$enableval"],
  [ac_lv2="yes"])

AC_ARG_ENABLE(lv2-event,
  AS_HELP_STRING([--enable-lv2-event], [enable LV2 plug-in MIDI/Event support (default=no)]),
  [ac_lv2_event="$enableval"],
  [ac_lv2_event="no"])

AC_ARG_ENABLE(lv2-atom,
  AS_HELP_STRING([--enable-lv2-atom], [enable LV2 plug-in MIDI/Atom support (default=yes)]),
  [ac_lv2_atom="$enableval"],
  [ac_lv2_atom="yes"])

AC_ARG_ENABLE(lv2-worker,
  AS_HELP_STRING([--enable-lv2-worker], [enable LV2 plug-in Worker/schedule support (default=yes)]),
  [ac_lv2_worker="$enableval"],
  [ac_lv2_worker="yes"])

AC_ARG_ENABLE(lv2-ui,
  AS_HELP_STRING([--enable-lv2-ui], [enable LV2 plug-in UI support (default=yes)]),
  [ac_lv2_ui="$enableval"],
  [ac_lv2_ui="yes"])

AC_ARG_ENABLE(lv2-external-ui,
  AS_HELP_STRING([--enable-lv2-external-ui], [enable LV2 plug-in External UI support (default=yes)]),
  [ac_lv2_external_ui="$enableval"],
  [ac_lv2_external_ui="yes"])

AC_ARG_ENABLE(lv2-state,
  AS_HELP_STRING([--enable-lv2-state], [enable LV2 plug-in State support (default=yes)]),
  [ac_lv2_state="$enableval"],
  [ac_lv2_state="yes"])

AC_ARG_ENABLE(lv2-state-files,
  AS_HELP_STRING([--enable-lv2-state-files], [enable LV2 plug-in State Files support (default=yes)]),
  [ac_lv2_state_files="$enableval"],
  [ac_lv2_state_files="yes"])

AC_ARG_ENABLE(lv2-state-make-path,
  AS_HELP_STRING([--enable-lv2-state-make-path], [enable LV2 plug-in State Make Path support (default=no)]),
  [ac_lv2_state_make_path="$enableval"],
  [ac_lv2_state_make_path="no"])

AC_ARG_ENABLE(lv2-programs,
  AS_HELP_STRING([--enable-lv2-programs], [enable LV2 plug-in Programs support (default=yes)]),
  [ac_lv2_programs="$enableval"],
  [ac_lv2_programs="yes"])

AC_ARG_ENABLE(lv2-presets,
  AS_HELP_STRING([--enable-lv2-presets], [enable LV2 plug-in Presets support (default=yes)]),
  [ac_lv2_presets="$enableval"],
  [ac_lv2_presets="yes"])

AC_ARG_ENABLE(lv2-patch,
  AS_HELP_STRING([--enable-lv2-patch], [enable LV2 plug-in Patch support (default=yes)]),
  [ac_lv2_patch="$enableval"],
  [ac_lv2_patch="yes"])

AC_ARG_ENABLE(lv2-time,
  AS_HELP_STRING([--enable-lv2-time], [enable LV2 plug-in Time support (default=yes)]),
  [ac_lv2_time="$enableval"],
  [ac_lv2_time="yes"])

AC_ARG_ENABLE(lv2-time-position,
  AS_HELP_STRING([--enable-lv2-time-position], [enable LV2 plug-in Time/position support (default=yes)]),
  [ac_lv2_time_position="$enableval"],
  [ac_lv2_time_position="yes"])

AC_ARG_ENABLE(lv2-options,
  AS_HELP_STRING([--enable-lv2-options], [enable LV2 plug-in Options support (default=yes)]),
  [ac_lv2_options="$enableval"],
  [ac_lv2_options="yes"])

AC_ARG_ENABLE(lv2-buf-size,
  AS_HELP_STRING([--enable-lv2-buf-size], [enable LV2 plug-in Buf-size support (default=yes)]),
  [ac_lv2_buf_size="$enableval"],
  [ac_lv2_buf_size="yes"])

AC_ARG_ENABLE(lv2-ui-touch,
  AS_HELP_STRING([--enable-lv2-ui-touch], [enable LV2 plug-in UI Touch interface support (default=yes)]),
  [ac_lv2_ui_touch="$enableval"],
  [ac_lv2_ui_touch="yes"])

AC_ARG_ENABLE(lv2-ui-idle,
  AS_HELP_STRING([--enable-lv2-ui-idle], [enable LV2 plug-in UI Idle interface support (default=yes)]),
  [ac_lv2_ui_idle="$enableval"],
  [ac_lv2_ui_idle="yes"])

AC_ARG_ENABLE(lv2-ui-show,
  AS_HELP_STRING([--enable-lv2-ui-show], [enable LV2 plug-in UI Show interface support (default=yes)]),
  [ac_lv2_ui_show="$enableval"],
  [ac_lv2_ui_show="yes"])

AC_ARG_ENABLE(lv2-ui-gtk2,
  AS_HELP_STRING([--enable-lv2-ui-gtk2], [enable LV2 plug-in UI GTK2 native support (default=yes)]),
  [ac_lv2_ui_gtk2="$enableval"],
  [ac_lv2_ui_gtk2="yes"])

AC_ARG_ENABLE(lv2-ui-x11,
  AS_HELP_STRING([--enable-lv2-ui-x11], [enable LV2 plug-in UI X11 native support (default=yes)]),
  [ac_lv2_ui_x11="$enableval"],
  [ac_lv2_ui_x11="yes"])

# Enable JACK session support.
AC_ARG_ENABLE(jack-session,
  AS_HELP_STRING([--enable-jack-session], [enable JACK session support (default=yes)]),
  [ac_jack_session="$enableval"],
  [ac_jack_session="yes"])

# Enable JACK latency support.
AC_ARG_ENABLE(jack-latency,
  AS_HELP_STRING([--enable-jack-latency], [enable JACK latency support (default=yes)]),
  [ac_jack_latency="$enableval"],
  [ac_jack_latency="yes"])

# Enable JACK metadata support.
AC_ARG_ENABLE(jack_metadata,
  AS_HELP_STRING([--enable-jack-metadata], [enable JACK metadata support (default=yes)]),
  [ac_jack_metadata="$enableval"],
  [ac_jack_metadata="yes"])

# Enable NSM support.
AC_ARG_ENABLE(nsm,
  AS_HELP_STRING([--enable-nsm], [enable NSM support (default=yes)]),
  [ac_nsm="$enableval"],
  [ac_nsm="yes"])

# Enable unique/single instance.
AC_ARG_ENABLE(xunique,
  AS_HELP_STRING([--enable-xunique], [enable unique/single instance (default=no)]),
  [ac_xunique="$enableval"],
  [ac_xunique="no"])

# Enable gradient eye-candy.
AC_ARG_ENABLE(gradient,
  AS_HELP_STRING([--enable-gradient], [enable gradient eye-candy (default=yes)]),
  [ac_gradient="$enableval"],
  [ac_gradient="yes"])

# Enable debugger stack-trace option (assumes --enable-debug).
AC_ARG_ENABLE(stacktrace,
  AS_HELP_STRING([--enable-stacktrace], [enable debugger stack-trace (default=no)]),
  [ac_stacktrace="$enableval"])


# Standard installation base dirs.
ac_path=$PATH
ac_pkg_config_path=$PKG_CONFIG_PATH
ac_with_paths=""

# Set for alternate Qt installation dir.
AC_ARG_WITH(qt,
  AS_HELP_STRING([--with-qt=PATH], [use alternate Qt install path]),
  [ac_qt_path="$withval"], [ac_qt_path="no"])

if test "x$ac_qt_path" != "xno"; then
   ac_path="$ac_qt_path/bin:$ac_path"
   ac_pkg_config_path="$ac_qt_path/lib/pkgconfig:$ac_pkg_config_path"
fi

# Set for alternate JACK installation dir.
AC_ARG_WITH(jack,
  AS_HELP_STRING([--with-jack=PATH], [use alternate JACK install path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate ALSA installation dir.
AC_ARG_WITH(alsa,
  AS_HELP_STRING([--with-alsa=PATH], [use alternate ALSA install path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libsndfile installation dir.
AC_ARG_WITH(libsndfile,
  AS_HELP_STRING([--with-libsndfile=PATH], [use alternate libsndfile path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libvorbis installation dir.
AC_ARG_WITH(libvorbis,
  AS_HELP_STRING([--with-libvorbis=PATH], [use alternate libvorbis path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libmad installation dir.
AC_ARG_WITH(libmad,
  AS_HELP_STRING([--with-libmad=PATH], [use alternate libmad path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libsamplerate installation dir.
AC_ARG_WITH(libsamplerate,
  AS_HELP_STRING([--with-libsamplerate=PATH], [use alternate libsamplerate path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate librubberband installation dir.
AC_ARG_WITH(librubberband,
  AS_HELP_STRING([--with-librubberband=PATH], [use alternate librubberband path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libaubio installation dir.
AC_ARG_WITH(libaubio,
  AS_HELP_STRING([--with-libaubio=PATH], [use alternate libaubio path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate liblo installation dir.
AC_ARG_WITH(liblo,
  AS_HELP_STRING([--with-liblo=PATH], [use alternate liblo path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libz installation dir.
AC_ARG_WITH(libz,
  AS_HELP_STRING([--with-libz=PATH], [use alternate libz path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate liblilv installation dir.
AC_ARG_WITH(liblilv,
  AS_HELP_STRING([--with-liblilv=PATH], [use alternate liblilv path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libsuil installation dir.
AC_ARG_WITH(libsuil,
  AS_HELP_STRING([--with-libsuil=PATH], [use alternate libsuil path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate LADSPA include dir.
AC_ARG_WITH(ladspa,
  AS_HELP_STRING([--with-ladspa=PATH], [use alternate LADSPA header path]),
  [ac_with_ladspa="$withval"])

# Set for alternate DSSI include dir.
AC_ARG_WITH(dssi,
  AS_HELP_STRING([--with-dssi=PATH], [use alternate DSSI header path]),
  [ac_with_dssi="$withval"])

# Set for alternate VST include dir.
AC_ARG_WITH(vst,
  AS_HELP_STRING([--with-vst=PATH], [use alternate VST header path]),
  [ac_with_vst="$withval"])

# Set for alternate LV2 include dir.
AC_ARG_WITH(lv2,
  AS_HELP_STRING([--with-lv2=PATH], [use alternate LV2 header path]),
  [ac_with_lv2="$withval"])


# Honor user specified flags.
ac_cflags=$CFLAGS
ac_ldflags=$LDFLAGS


# Checks for programs.
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_CXXCPP

# Checks for languages.
AC_LANG_CPLUSPLUS

# Check for pkg-config.
PKG_PROG_PKG_CONFIG

# Check whether -std=c++11 support is necessary (4 < g++ version < 6).
AC_CACHE_CHECK([for g++ major version], [ac_cv_gxx_version_major], [
   ac_cv_gxx_version_major=$($CC -dumpversion)
   if test -n "$ac_cv_gxx_version_major"; then
      ac_cv_gxx_version_major=$(echo $ac_cv_gxx_version_major | cut -d'.' -f1);
   fi
   ac_gxx_version_major=$(($ac_cv_gxx_version_major + 0))
])
ac_gxx_version_major=$ac_cv_gxx_version_major
if test $ac_gxx_version_major -ge 4 -a $ac_gxx_version_major -lt 6; then
   CPPFLAGS="-std=c++11 $CPPFLAGS"
   ac_cflags="-std=c++11 $ac_cflags"
fi

# Check for proper flags.
ac_arch=$(uname -m)

# Check for install paths and alternatives...
ac_libdirs="lib"

if test "x$ac_arch" = "xx86_64"; then
   ac_libdirs="$ac_libdirs lib64"
fi

CFLAGS="-fPIC $CFLAGS"
CPPFLAGS="-fPIC $CPPFLAGS"

# Prepend alternate dependencies paths.
for X in $ac_with_paths; do
  if test -d $X/bin; then
     ac_path="$X/bin:$ac_path"
  fi
  if test -d $X/include; then
     CFLAGS="-I$X/include $CFLAGS "
     CPPFLAGS="-I$X/include $CPPFLAGS"
     ac_incpath="$X/include $ac_incpath"
  fi
  for Y in $ac_libdirs; do
     if test -d $X/$Y; then
        LIBS="-L$X/$Y $LIBS"
        ac_libs="-L$X/$Y $ac_libs"
        if test -d $X/$Y/pkgconfig; then
           ac_pkg_config_path="$X/$Y/pkgconfig:$ac_pkg_config_path"
        fi
     fi
  done
done

# Set pkg-config path.
if test -n "$ac_pkg_config_path"; then
   export PKG_CONFIG_PATH=$ac_pkg_config_path
fi


# A common error message:
ac_errmsg="not found in current PATH. Maybe QT development environment isn't available."

# Check for qtchooser availability.
AC_PATH_TOOL(ac_qtchooser, qtchooser, [no], $ac_path)
if test -x $ac_qtchooser; then
   export QT_SELECT=5
fi

# Check for proper qmake path/version alternatives.
AC_PATH_TOOL(ac_qmake, qmake, [no], $ac_path)
if test "x$ac_qmake" = "xno"; then
   AC_PATH_TOOL(ac_cv_qmake, qmake-qt5, [no], $ac_path)
   ac_qmake=$ac_cv_qmake
fi

# Check for proper Qt major version.
AC_CACHE_CHECK([for Qt major version], [ac_cv_qt_version_major], [
   ac_cv_qt_version_major=$($ac_qmake -query QT_VERSION | cut -d'.' -f1)
   ac_cv_qt_version_major=$(($ac_cv_qt_version_major + 0))
])
ac_qt_version_major=$ac_cv_qt_version_major
if test $ac_qt_version_major -ne 5; then
	AC_PATH_TOOL(ac_cv_qmake, qmake-qt5, [no], $ac_path)
	ac_qmake=$ac_cv_qmake
fi

if test "x$ac_qmake" = "xno"; then
   AC_MSG_ERROR([qmake-qt5 $ac_errmsg (qt5-devel)])
fi

# Check for proper Qt install path.
AC_CACHE_CHECK([for Qt install path], [ac_cv_qt_install_path], [
   ac_cv_qt_install_path=$($ac_qmake -query QT_INSTALL_BINS)
])
ac_qt_install_path=$ac_cv_qt_install_path
if test -d $ac_qt_install_path; then
   ac_path="$ac_qt_install_path:$ac_path"
fi

# Check it again, now with updated PATH, just in case...
AC_PATH_TOOL(ac_cv_qmake, qmake, [no], $ac_path)
ac_qmake=$ac_cv_qmake
if test "x$ac_qmake" = "xno"; then
   AC_MSG_ERROR([qmake $ac_errmsg])
fi

AC_SUBST(ac_qmake)

AC_CACHE_CHECK([for Qt install headers], [ac_cv_qt_install_headers], [
   ac_cv_qt_install_headers=$($ac_qmake -query QT_INSTALL_HEADERS)
])
ac_qt_install_headers=$ac_cv_qt_install_headers
if test -d $ac_qt_install_headers; then
   CFLAGS="-I$ac_qt_install_headers $CFLAGS "
   CPPFLAGS="-I$ac_qt_install_headers $CPPFLAGS"
   ac_incpath="$ac_qt_install_headers $ac_incpath"
fi

AC_CACHE_CHECK([for Qt install libraries], [ac_cv_qt_install_libs], [
   ac_cv_qt_install_libs=$($ac_qmake -query QT_INSTALL_LIBS)
])
ac_qt_install_libs=$ac_cv_qt_install_libs
if test -d $ac_qt_install_libs; then
   LIBS="-L$ac_qt_install_libs $LIBS"
   ac_libs="-L$ac_qt_install_libs $ac_libs"
fi

# Finally, check for proper Qt version.
AC_CACHE_CHECK([for Qt library version >= 5.1],
   ac_cv_qtversion, [
   AC_TRY_COMPILE([#include "QtCore/qglobal.h"], [
      #if QT_VERSION < 0x050100
      #error Qt library 5.1 or greater required.
      #endif
   ], ac_cv_qtversion="yes", [
      echo "no; Qt 5.1 or greater is required"
      exit 1
   ])
])

# Check for Qt moc utility.
AC_PATH_TOOL(ac_moc, moc, [no], $ac_path)
if test "x$ac_moc" = "xno"; then
   AC_MSG_ERROR([moc $ac_errmsg])
fi
AC_SUBST(ac_moc)

# Check for Qt uic utility.
AC_PATH_TOOL(ac_uic, uic, [no], $ac_path)
if test "x$ac_uic" = "xno"; then
   AC_MSG_ERROR([uic $ac_errmsg])
fi
AC_SUBST(ac_uic)


# Check for Qt lupdate utility.
AC_PATH_TOOL(ac_lupdate, lupdate, [no], $ac_path)
if test "x$ac_lupdate" = "xno"; then
   AC_PATH_TOOL(ac_cv_lupdate, lupdate-qt5, [no], $ac_path)
   if test "x$ac_cv_lupdate" = "xno"; then
      AC_MSG_ERROR([lupdate $ac_errmsg (qt5-linguist)])
   else
      ac_lupdate=$ac_cv_lupdate;
   fi
fi
AC_SUBST(ac_lupdate)

# Check for Qt lrelease utility.
AC_PATH_TOOL(ac_lrelease, lrelease, [no], $ac_path)
if test "x$ac_lrelease" = "xno"; then
   AC_PATH_TOOL(ac_cv_lrelease, lrelease-qt5, [no], $ac_path)
   if test "x$ac_cv_lrelease" = "xno"; then
      AC_MSG_ERROR([lrelease $ac_errmsg (qt5-linguist)])
   else
      ac_lrelease=$ac_cv_lrelease;
   fi
fi
AC_SUBST(ac_lrelease)


# Check for IEEE 32bit float optimizations.
AC_CACHE_CHECK([for IEEE 32bit float optimizations],
   ac_cv_float32, [
   AC_TRY_RUN([
      int main() { return (sizeof(float) == 4 ? 0 : 1); }
   ], ac_cv_float32="yes", ac_cv_float32="no")
])
ac_float32=$ac_cv_float32
if test "x$ac_float32" = "xyes"; then
   AC_DEFINE(CONFIG_FLOAT32, 1, [Define if IEEE 32bit float optimizations are enabled.])
fi

# Check for SSE optimization.
if test "x$ac_sse" = "xyes"; then
   ac_sse_cflags="-msse -mfpmath=sse -ffast-math"
   ac_old_cflags=$CFLAGS
   ac_old_cppflags=$CPPFLAGS
   CFLAGS="$ac_sse_cflags $CFLAGS"
   CPPFLAGS="$ac_sse_cflags $CPPFLAGS"
   AC_CACHE_CHECK([for SSE optimization],
      ac_cv_sse, [
      AC_TRY_RUN([
         #include <xmmintrin.h>
         #if !defined(__SSE__)
         #error SSE optimization disabled.
         #endif
         int main() {
            unsigned int a, b, c, d;
            __asm__ __volatile__ (
              "movl %%ebx, %%esi\n\t" \
              "cpuid\n\t" \
              "xchgl %%ebx, %%esi" \
              : "=a" (a), "=S" (b), "=c" (c), "=d" (d) : "0" (1));
            return ((d & (1 << 25)) ? 0 : 1);
         }
      ], ac_cv_sse="yes", ac_cv_sse="no")
   ])
   ac_sse=$ac_cv_sse
   if test "x$ac_sse" = "xyes"; then
      ac_cflags="$ac_sse_cflags $ac_cflags"
   else
      CPPFLAGS=$ac_old_cppflags
      CFLAGS=$ac_old_cflags
   fi
fi


# Checks for libraries.
AC_CHECK_LIB(m, main)
AC_CHECK_LIB(X11, main)
AC_CHECK_LIB(Xext, main)

# Check for round math function.
AC_CHECK_LIB(m, round, [ac_round="yes"], [ac_round="no"])
if test "x$ac_round" = "xyes"; then
   AC_DEFINE(CONFIG_ROUND, 1, [Define if round is available.])
fi

# Check for JACK libraries.
PKG_CHECK_MODULES([JACK], [jack >= 0.100.0], [ac_libjack="yes"], [ac_libjack="no"])
if test "x$ac_libjack" = "xyes"; then
   AC_DEFINE(CONFIG_LIBJACK, 1, [Define if JACK library is available.])
   ac_cflags="$ac_cflags $JACK_CFLAGS"
   ac_libs="$ac_libs $JACK_LIBS"
   CFLAGS="$CFLAGS $JACK_CFLAGS"
   CPPFLAGS="$CPPFLAGS $JACK_CFLAGS"
   LIBS="$LIBS $JACK_LIBS"
else
   AC_MSG_ERROR([*** JACK library not found.])
fi

# Check for ALSA libraries.
PKG_CHECK_MODULES([ALSA], [alsa], [ac_libasound="yes"], [ac_libasound="no"])
if test "x$ac_libasound" = "xyes"; then
   AC_DEFINE(CONFIG_LIBASOUND, 1, [Define if ALSA library is available.])
   ac_cflags="$ac_cflags $ALSA_CFLAGS"
   ac_libs="$ac_libs $ALSA_LIBS"
else
   AC_MSG_ERROR([*** ALSA library not found.])
fi

# Check for SNDFILE library.
PKG_CHECK_MODULES([SNDFILE], [sndfile], [ac_libsndfile="yes"], [ac_libsndfile="no"])
if test "x$ac_libsndfile" = "xyes"; then
   AC_DEFINE(CONFIG_LIBSNDFILE, 1, [Define if SNDFILE library is available.])
   ac_cflags="$ac_cflags $SNDFILE_CFLAGS"
   ac_libs="$ac_libs $SNDFILE_LIBS"
else
   AC_MSG_ERROR([*** SNDFILE library not found.])
fi

# Check for optional VORBIS libraries.
if test "x$ac_libvorbis" = "xyes"; then
   PKG_CHECK_MODULES([VORBIS], [vorbis], [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_cflags="$ac_cflags $VORBIS_CFLAGS"
      ac_libs="$ac_libs $VORBIS_LIBS"
   else
      AC_MSG_WARN([*** VORBIS library not found.])
   fi
fi
if test "x$ac_libvorbis" = "xyes"; then
   PKG_CHECK_MODULES([VORBISENC], [vorbisenc], [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_cflags="$ac_cflags $VORBISENC_CFLAGS"
      ac_libs="$ac_libs $VORBISENC_LIBS"
   else
      AC_MSG_WARN([*** VORBISENC library not found.])
   fi
fi
if test "x$ac_libvorbis" = "xyes"; then
   PKG_CHECK_MODULES([VORBISFILE], [vorbisfile], [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_cflags="$ac_cflags $VORBISFILE_CFLAGS"
      ac_libs="$ac_libs $VORBISFILE_LIBS"
   else
      AC_MSG_WARN([*** VORBISFILE library not found.])
   fi
fi
if test "x$ac_libvorbis" = "xyes"; then
   PKG_CHECK_MODULES([OGG], [ogg], [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_cflags="$ac_cflags $OGG_CFLAGS"
      ac_libs="$ac_libs $OGG_LIBS"
   else
      AC_MSG_WARN([*** OGG library not found.])
   fi
fi
if test "x$ac_libvorbis" = "xyes"; then
   AC_DEFINE(CONFIG_LIBVORBIS, 1, [Define if VORBIS library is available.])
else
   AC_MSG_WARN([*** Ogg Vorbis audio file support will be disabled.])
fi

# Check for optional MAD library.
if test "x$ac_libmad" = "xyes"; then
   PKG_CHECK_MODULES([MAD], [mad], [ac_libmad="yes"], [ac_libmad="no"])
   if test "x$ac_libmad" = "xyes"; then
      AC_DEFINE(CONFIG_LIBMAD, 1, [Define if MAD library is available.])
      ac_cflags="$ac_cflags $MAD_CFLAGS"
      ac_libs="$ac_libs $MAD_LIBS"
   else
      AC_MSG_WARN([*** MAD library not found.])
      AC_MSG_WARN([*** MPEG Layer III audio file support will be disabled.])
   fi
fi

# Check for optional SAMPLERATE library.
if test "x$ac_libsamplerate" = "xyes"; then
   PKG_CHECK_MODULES([SAMPLERATE], [samplerate], [ac_libsamplerate="yes"], [ac_libsamplerate="no"])
   if test "x$ac_libsamplerate" = "xyes"; then
      AC_DEFINE(CONFIG_LIBSAMPLERATE, 1, [Define if SAMPLERATE library is available.])
      ac_cflags="$ac_cflags $SAMPLERATE_CFLAGS"
      ac_libs="$ac_libs $SAMPLERATE_LIBS"
   else
      AC_MSG_WARN([*** SAMPLERATE library not found.])
      AC_MSG_WARN([*** Sample-rate conversion support will be disabled.])
   fi
fi

# Check for optional RUBBERBAND library.
if test "x$ac_librubberband" = "xyes"; then
   PKG_CHECK_MODULES([RUBBERBAND], [rubberband], [ac_librubberband="yes"], [ac_librubberband="no"])
   if test "x$ac_librubberband" = "xyes"; then
      AC_DEFINE(CONFIG_LIBRUBBERBAND, 1, [Define if RUBBERBAND library is available.])
      ac_cflags="$ac_cflags $RUBBERBAND_CFLAGS"
      ac_libs="$ac_libs $RUBBERBAND_LIBS"
   else
      AC_MSG_WARN([*** RUBBERBAND library not found.])
      AC_MSG_WARN([*** Pitch-shifting support will be disabled.])
   fi
fi

# Check for optional AUBIO library.
if test "x$ac_libaubio" = "xyes"; then
   PKG_CHECK_MODULES([AUBIO], [aubio >= 0.4.1], [ac_libaubio="yes"], [ac_libaubio="no"])
   if test "x$ac_libaubio" = "xyes"; then
      AC_DEFINE(CONFIG_LIBAUBIO, 1, [Define if AUBIO library is available.])
      ac_cflags="$ac_cflags $AUBIO_CFLAGS"
      ac_libs="$ac_libs $AUBIO_LIBS"
   else
      AC_MSG_WARN([*** AUBIO library not found.])
      AC_MSG_WARN([*** Beat-detection support will be disabled.])
   fi
fi

# Check for optional LIBLO library.
if test "x$ac_liblo" = "xyes"; then
   PKG_CHECK_MODULES([LIBLO], [liblo], [ac_liblo="yes"], [ac_liblo="no"])
   if test "x$ac_liblo" = "xyes"; then
      AC_DEFINE(CONFIG_LIBLO, 1, [Define if LIBLO library is available.])
      ac_cflags="$ac_cflags $LIBLO_CFLAGS"
      ac_libs="$ac_libs $LIBLO_LIBS"
   else
      AC_MSG_WARN([*** LIBLO library not found.])
      AC_MSG_WARN([*** OSC service support will be disabled.])
      ac_nsm="no"
   fi
else
   ac_nsm="no"
fi

# Check for optional ZLIB library.
if test "x$ac_libz" = "xyes"; then
   PKG_CHECK_MODULES([ZLIB], [zlib], [ac_libz="yes"], [ac_libz="no"])
   if test "x$ac_libz" = "xyes"; then
      AC_DEFINE(CONFIG_LIBZ, 1, [Define if ZLIB library is available.])
      ac_cflags="$ac_cflags $ZLIB_CFLAGS"
      ac_libs="$ac_libs $ZLIB_LIBS"
   else
      AC_MSG_WARN([*** ZLIB library not found.])
      AC_MSG_WARN([*** Archive/Zip file support will be disabled.])
   fi
fi

# Check for LV2 support.
if test "x$ac_lv2" = "xno"; then
   ac_lilv="no"
   ac_lv2_ui="no"
fi

if test "x$ac_lv2_ui" = "xno"; then
   ac_suil="no"
   ac_lv2_external_ui="no"
   ac_lv2_ui_touch="no"
   ac_lv2_ui_idle="no"
   ac_lv2_ui_show="no"
   ac_lv2_ui_gtk2="no"
   ac_lv2_ui_x11="no"
fi

# Check for optional LILV library.
if test "x$ac_lilv" = "xyes"; then
   PKG_CHECK_MODULES([LILV], [lilv-0], [ac_lilv="yes"], [ac_lilv="no"])
fi
if test "x$ac_lilv" = "xyes"; then
   AC_DEFINE(CONFIG_LIBLILV, 1, [Define if LILV library is available.])
   ac_cflags="$ac_cflags $LILV_CFLAGS"
   ac_libs="$ac_libs $LILV_LIBS"
   CFLAGS="$CFLAGS $LILV_CFLAGS"
   CPPFLAGS="$CPPFLAGS $LILV_CFLAGS"
   LIBS="$LIBS $LILV_LIBS"
else
   AC_MSG_WARN([*** LILV library not found.])
fi

if test "x$ac_lilv" = "xyes"; then
   AC_CHECK_LIB(lilv-0, lilv_file_uri_parse,
      [ac_lilv_file_uri_parse="yes"],
      [ac_lilv_file_uri_parse="no"])
   if test "x$ac_lilv_file_uri_parse" = "xyes"; then
      AC_DEFINE(CONFIG_LILV_FILE_URI_PARSE, 1, [Define if lilv_file_uri_parse is available.])
   fi
   AC_CHECK_LIB(lilv-0, lilv_world_unload_resource,
      [ac_lilv_world_unload_resource="yes"],
      [ac_lilv_world_unload_resource="no"])
   if test "x$ac_lilv_world_unload_resource" = "xyes"; then
      AC_DEFINE(CONFIG_LILV_WORLD_UNLOAD_RESOURCE, 1, [Define if lilv_world_unload_resource is available.])
   fi
fi

# Check for LV2 new UI instantiation availability (libsuil).
if test "x$ac_suil" = "xyes"; then
   PKG_CHECK_MODULES([SUIL], [suil-0], [ac_suil="yes"], [ac_suil="no"])
fi
if test "x$ac_suil" = "xyes"; then
   AC_DEFINE(CONFIG_LIBSUIL, 1, [Define if SUIL library is enabled.])
   ac_cflags="$ac_cflags $SUIL_CFLAGS"
   ac_libs="$ac_libs $SUIL_LIBS"
   CFLAGS="$CFLAGS $SUIL_CFLAGS"
   CPPFLAGS="$CPPFLAGS $SUIL_CFLAGS"
   LIBS="$LIBS $SUIL_LIBS"
else
   AC_MSG_WARN([*** SUIL library will be disabled.])
fi

if test "x$ac_suil" = "xyes"; then
   AC_CHECK_LIB(suil-0, suil_instance_get_handle,
      [ac_suil_instance_get_handle="yes"],
      [ac_suil_instance_get_handle="no"])
   if test "x$ac_suil_instance_get_handle" = "xyes"; then
      AC_DEFINE(CONFIG_SUIL_INSTANCE_GET_HANDLE, 1, [Define if suil_instance_get_handle is available.])
   fi
fi

# Check for LV2 host library (liblilv).
if test "x$ac_lilv" = "xno"; then
   ac_lv2="no"
   ac_lv2_ui="no"
   ac_lv2_event="no"
   ac_lv2_atom="no"
   ac_lv2_worker="no"
   ac_lv2_external_ui="no"
   ac_lv2_state="no"
   ac_lv2_state_files="no"
   ac_lv2_state_make_path="no"
   ac_lv2_programs="no"
   ac_lv2_presets="no"
   ac_lv2_patch="no"
   ac_lv2_time="no"
   ac_lv2_time_position="no";
   ac_lv2_options="no"
   ac_lv2_buf_size="no"
   ac_lv2_ui_touch="no"
   ac_lv2_ui_idle="no"
   ac_lv2_ui_show="no"
   ac_lv2_ui_gtk2="no"
   ac_lv2_ui_x11="no"
fi

# Check for JACK session event callback availability.
if test "x$ac_jack_session" = "xyes"; then
   AC_CHECK_LIB(jack, jack_set_session_callback, [ac_jack_session="yes"], [ac_jack_session="no"])
else
   AC_MSG_WARN([*** JACK session support will be disabled.])
fi

# Check for (new) JACK latency support availability.
if test "x$ac_jack_latency" = "xyes"; then
   AC_CHECK_LIB(jack, jack_port_get_latency_range, [ac_jack_latency="yes"], [ac_jack_latency="no"])
   if test "x$ac_jack_latency" = "xyes"; then
      AC_DEFINE(CONFIG_JACK_LATENCY, 1, [Define if JACK latency support is available.])
   else
      AC_MSG_WARN([*** JACK latency support will be disabled.])
   fi
fi

# Check for JACK metadata support availability.
if test "x$ac_jack_metadata" = "xyes"; then
   AC_CHECK_LIB(jack, jack_get_property, [ac_jack_metadata="yes"], [ac_jack_metadata="no"])
else
   AC_MSG_WARN([*** JACK metadata support will be disabled.])
fi


# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/ioctl.h sys/stat.h unistd.h signal.h)

# Check for LADSPA headers.
if test -n "$ac_with_ladspa"; then
   CFLAGS="-I$ac_with_ladspa $CFLAGS"
   CPPFLAGS="-I$ac_with_ladspa $CPPFLAGS"
   ac_ladspa="yes"
fi
if test "x$ac_ladspa" = "xyes"; then
   AC_CHECK_HEADER(ladspa.h, [ac_ladspa="yes"], [ac_ladspa="no"])
   if test "x$ac_ladspa" = "xyes"; then
      AC_DEFINE(CONFIG_LADSPA, 1, [Define if LADSPA header is available.])
      ac_incpath="$ac_with_ladspa $ac_incpath"
   else
      AC_MSG_WARN([*** ladspa.h header file not found.])
      AC_MSG_WARN([*** LADSPA plug-in support will be disabled.])
   fi
fi

# Check for DSSI headers.
if test -n "$ac_with_dssi"; then
   CFLAGS="-I$ac_with_dssi $CFLAGS"
   CPPFLAGS="-I$ac_with_dssi $CPPFLAGS"
   ac_dssi="yes"
fi
if test "x$ac_dssi" = "xyes"; then
   AC_CHECK_HEADER(dssi.h, [ac_dssi="yes"], [ac_dssi="no"])
   if test "x$ac_dssi" = "xyes"; then
      AC_DEFINE(CONFIG_DSSI, 1, [Define if DSSI header is available.])
      ac_incpath="$ac_with_dssi $ac_incpath"
   else
      AC_MSG_WARN([*** dssi.h header file not found.])
      AC_MSG_WARN([*** DSSI plug-in support will be disabled.])
   fi
fi

# Pre-check for VeSTige header.
if test "$ac_vestige" = "xyes"; then
   ac_vst="yes"
fi

# Check for VST headers.
if test -n "$ac_with_vst"; then
   CFLAGS="-I$ac_with_vst $CFLAGS"
   CPPFLAGS="-I$ac_with_vst $CPPFLAGS"
   ac_incpath="$ac_with_vst $ac_incpath"
   ac_vst="yes"
fi
if test "x$ac_vst" = "xyes"; then
   CFLAGS="-D__cdecl= $CFLAGS"
   CPPFLAGS="-D__cdecl= $CPPFLAGS"
   AC_CACHE_CHECK([for VST headers],
      ac_cv_vst, [
      AC_TRY_LINK([#include <aeffectx.h>], [
         // Checking for VST headers...
      ], ac_cv_vst="yes", ac_cv_vst="no")
   ])
   ac_vst=$ac_cv_vst
   if test "x$ac_vst" = "xno" -a "x$ac_vestige" = "xyes"; then
      CFLAGS="$CFLAGS -Isrc/vestige"
      CPPFLAGS="$CPPFLAGS -Isrc/vestige"
      ac_incpath="$ac_incpath vestige"
      AC_CHECK_HEADER(vestige.h, [ac_vst="yes"], [ac_vst="no"])
   else
      ac_vestige="no"
   fi
   if test "x$ac_vst" = "xyes"; then
      AC_DEFINE(CONFIG_VST, 1, [Define if VST header is available.])
   else
      AC_MSG_WARN([*** aeffectx.h header file not found.])
      AC_MSG_WARN([*** VST plug-in support will be disabled.])
      ac_vestige="no"
   fi
else
   ac_vestige="no"
fi

# Post-check for VeSTige header.
if test "x$ac_vestige" = "xyes"; then
   AC_DEFINE(CONFIG_VESTIGE, 1, [Define if VeSTige header is available.])
fi

# Check for LV2 headers.
if test -n "$ac_with_lv2"; then
   ac_lv2="yes"
fi
if test "x$ac_lv2" = "xyes"; then
   if test -n "$ac_with_lv2"; then
      CFLAGS="-I$ac_with_lv2 $CFLAGS"
      CPPFLAGS="-I$ac_with_lv2 $CPPFLAGS"
      ac_incpath="$ac_with_lv2 $ac_incpath"
   fi
   CFLAGS="$CFLAGS -Isrc/lv2"
   CPPFLAGS="$CPPFLAGS -Isrc/lv2"
   ac_incpath="$ac_incpath lv2"
   PKG_CHECK_MODULES([LV2], [lv2], [ac_lv2="yes"], [ac_lv2="no"])
   if test "x$ac_lv2" = "xyes"; then
      ac_cflags="$ac_cflags $LV2_CFLAGS"
      CFLAGS="$CFLAGS $LV2_CFLAGS"
      CPPFLAGS="$CPPFLAGS $LV2_CFLAGS"
   fi
   if test "x$ac_lv2" = "xyes"; then
      AC_CHECK_HEADERS(
         lv2/lv2plug.in/ns/ext/urid/urid.h,
         [ac_lv2="yes"], [ac_lv2="no"])
   fi
   if test "x$ac_lv2" = "xyes"; then
      AC_DEFINE(CONFIG_LV2, 1, [Define if LV2 headers are available.])
   else
      AC_MSG_WARN([*** LV2 header files not found.])
      AC_MSG_WARN([*** LV2 plug-in support will be disabled.])
      ac_lv2_ui="no"
      ac_lv2_event="no"
      ac_lv2_atom="no"
      ac_lv2_worker="no"
      ac_lv2_external_ui="no"
      ac_lv2_state="no"
      ac_lv2_state_files="no"
      ac_lv2_state_make_path="no"
      ac_lv2_programs="no"
      ac_lv2_presets="no"
      ac_lv2_patch="no"
      ac_lv2_time="no"
      ac_lv2_time_position="no";
      ac_lv2_options="no"
      ac_lv2_buf_size="no"
      ac_lv2_ui_touch="no"
      ac_lv2_ui_idle="no"
      ac_lv2_ui_show="no"
      ac_lv2_ui_gtk2="no"
      ac_lv2_ui_x11="no"
   fi
fi
if test "x$ac_lv2_ui" = "xyes"; then
   AC_CHECK_HEADER(
      lv2/lv2plug.in/ns/extensions/ui/ui.h,
      [ac_lv2_ui="yes"], [ac_lv2_ui="no"])
fi
if test "x$ac_lv2_ui" = "xyes"; then
   AC_DEFINE(CONFIG_LV2_UI, 1, [Define if LV2 UI support is available.])
else
   AC_MSG_WARN([*** LV2 UI support will be disabled.])
   ac_lv2_external_ui="no"
   ac_lv2_ui_touch="no"
   ac_lv2_ui_idle="no"
   ac_lv2_ui_show="no"
   ac_lv2_ui_gtk2="no"
   ac_lv2_ui_x11="no"
fi
if test "x$ac_lv2_event" = "xyes"; then
   AC_CHECK_HEADERS(
      lv2/lv2plug.in/ns/ext/uri-map/uri-map.h \
      lv2/lv2plug.in/ns/ext/event/event.h \
      lv2/lv2plug.in/ns/ext/event/event-helpers.h,
      [ac_lv2_event="yes"], [ac_lv2_event="no"])
   if test "x$ac_lv2_event" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_EVENT, 1, [Define if LV2 Event/MIDI support is available.])
   else
      AC_MSG_WARN([*** LV2 Event/MIDI support will be disabled.])
   fi
fi
if test "x$ac_lv2_atom" = "xyes"; then
   AC_CHECK_HEADERS(lv2_atom_helpers.h \
      lv2/lv2plug.in/ns/ext/atom/atom.h \
      lv2/lv2plug.in/ns/ext/atom/forge.h \
      lv2/lv2plug.in/ns/ext/atom/util.h,
      [ac_lv2_atom="yes"], [ac_lv2_atom="no"])
   if test "x$ac_lv2_atom" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_ATOM, 1, [Define if LV2 Atom/MIDI aupport is available.])
      AC_CACHE_CHECK([for lv2_atom_forge_object],
         ac_cv_lv2_atom_forge_object, [
         AC_TRY_LINK([#include "lv2/lv2plug.in/ns/ext/atom/forge.h"], [
            // Checking for lv2_atom_forge_object...
            LV2_Atom_Forge *forge;
            LV2_Atom_Forge_Frame *frame;
            lv2_atom_forge_object(forge, frame, 0, 101);
         ], ac_cv_lv2_atom_forge_object="yes", ac_cv_lv2_atom_forge_object="no")
      ])
      ac_lv2_atom_forge_object=$ac_cv_lv2_atom_forge_object
      if test "x$ac_lv2_atom_forge_object" = "xyes"; then
         AC_DEFINE(CONFIG_LV2_ATOM_FORGE_OBJECT, 1, [Define if lv2_atom_forge_object is available.])
      fi
      AC_CACHE_CHECK([for lv2_atom_forge_key],
         ac_cv_lv2_atom_forge_key, [
         AC_TRY_LINK([#include "lv2/lv2plug.in/ns/ext/atom/forge.h"], [
            // Checking for lv2_atom_forge_key...
            LV2_Atom_Forge *forge;
            lv2_atom_forge_key(forge, 102);
         ], ac_cv_lv2_atom_forge_key="yes", ac_cv_lv2_atom_forge_key="no")
      ])
      ac_lv2_atom_forge_key=$ac_cv_lv2_atom_forge_key
      if test "x$ac_lv2_atom_forge_key" = "xyes"; then
         AC_DEFINE(CONFIG_LV2_ATOM_FORGE_KEY, 1, [Define if lv2_atom_forge_key is available.])
      fi
   else
      AC_MSG_WARN([*** LV2 Atom/MIDI support will be disabled.])
      ac_lv2_state="no"
      ac_lv2_options="no"
      ac_lv2_time_position="no"
   fi
else
   ac_lv2_state="no"
   ac_lv2_options="no"
   ac_lv2_time_position="no"
fi
if test "x$ac_lv2_worker" = "xyes"; then
   AC_CHECK_HEADER(
      lv2/lv2plug.in/ns/ext/worker/worker.h,
      [ac_lv2_worker="yes"], [ac_lv2_worker="no"])
   if test "x$ac_lv2_worker" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_WORKER, 1, [Define if LV2 Worker/Schedule aupport is available.])
   else
      AC_MSG_WARN([*** LV2 Worker/Schedule support will be disabled.])
   fi
fi
if test "x$ac_lv2_external_ui" = "xyes"; then
   AC_CHECK_HEADER(lv2_external_ui.h, [ac_lv2_external_ui="yes"], [ac_lv2_external_ui="no"])
   if test "x$ac_lv2_external_ui" = "xyes"; then
      AC_CHECK_HEADERS(
         lv2/lv2plug.in/ns/ext/data-access/data-access.h \
         lv2/lv2plug.in/ns/ext/instance-access/instance-access.h,
         [ac_lv2_external_ui="yes"], [ac_lv2_external_ui="no"])
   fi
   if test "x$ac_lv2_external_ui" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_EXTERNAL_UI, 1, [Define if LV2 External UI extension is available.])
   else
      AC_MSG_WARN([*** LV2 External UI extension will be disabled.])
   fi
fi
if test "x$ac_lv2_state" = "xyes"; then
   AC_CHECK_HEADERS(
      lv2/lv2plug.in/ns/ext/atom/atom.h \
      lv2/lv2plug.in/ns/ext/state/state.h,
      [ac_lv2_state="yes"], [ac_lv2_state="no"])
   if test "x$ac_lv2_state" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_STATE, 1, [Define if LV2 State extension is available.])
      if test "x$ac_lv2_state_files" = "xyes"; then
         AC_DEFINE(CONFIG_LV2_STATE_FILES, 1, [Define if LV2 State Files feature is available.])
         if test "x$ac_lv2_state_make_path" = "xyes"; then
            AC_DEFINE(CONFIG_LV2_STATE_MAKE_PATH, 1, [Define if LV2 State Make Path feature is available.])
         fi
      else
         AC_MSG_WARN([*** LV2 State Files feature will be disabled.])
         ac_lv2_state_make_path="no"
      fi
   else
      AC_MSG_WARN([*** LV2 State extension will be disabled.])
      ac_lv2_state_make_path="no"
      ac_lv2_state_files="no"
      ac_lv2_presets="no"
      ac_lv2_patch="no"
   fi
else
   ac_lv2_state_make_path="no"
   ac_lv2_state_files="no"
   ac_lv2_presets="no"
   ac_lv2_patch="no"
fi
if test "x$ac_lv2_programs" = "xyes"; then
   AC_CHECK_HEADER(lv2_programs.h, [ac_lv2_programs="yes"], [ac_lv2_programs="no"])
   if test "x$ac_lv2_programs" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_PROGRAMS, 1, [Define if LV2 Programs extension is available.])
   else
      AC_MSG_WARN([*** LV2 Programs extension will be disabled.])
   fi
fi
if test "x$ac_lv2_presets" = "xyes"; then
   AC_CHECK_HEADER(
      lv2/lv2plug.in/ns/ext/presets/presets.h,
      [ac_lv2_presets="yes"], [ac_lv2_presets="no"])
   if test "x$ac_lv2_presets" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_PRESETS, 1, [Define if LV2 Presets are supported.])
   else
      AC_MSG_WARN([*** LV2 Presets support will be disabled.])
   fi
fi
if test "x$ac_lv2_patch" = "xyes"; then
   AC_CHECK_HEADER(
      lv2/lv2plug.in/ns/ext/patch/patch.h,
      [ac_lv2_patch="yes"], [ac_lv2_patch="no"])
   if test "x$ac_lv2_patch" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_PATCH, 1, [Define if LV2 Patch is supported.])
   else
      AC_MSG_WARN([*** LV2 Patch support will be disabled.])
   fi
fi
if test "x$ac_lv2_time" = "xyes"; then
   AC_CHECK_HEADER(
      lv2/lv2plug.in/ns/ext/time/time.h,
      [ac_lv2_time="yes"], [ac_lv2_time="no"])
   if test "x$ac_lv2_time" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_TIME, 1, [Define if LV2 Time is supported.])
   else
      AC_MSG_WARN([*** LV2 Time support will be disabled.])
      ac_lv2_time_position="no";
   fi
else
   ac_lv2_time_position="no";
fi
if test "x$ac_lv2_options" = "xyes"; then
   AC_CHECK_HEADER(
      lv2/lv2plug.in/ns/ext/options/options.h,
      [ac_lv2_options="yes"], [ac_lv2_options="no"])
   if test "x$ac_lv2_options" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_OPTIONS, 1, [Define if LV2 Options is supported.])
   else
      AC_MSG_WARN([*** LV2 Options support will be disabled.])
      ac_lv2_buf_size="no"
   fi
else
   ac_lv2_buf_size="no"
fi
if test "x$ac_lv2_buf_size" = "xyes"; then
   AC_CHECK_HEADER(
      lv2/lv2plug.in/ns/ext/buf-size/buf-size.h,
      [ac_lv2_buf_size="yes"], [ac_lv2_buf_size="no"])
   if test "x$ac_lv2_buf_size" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_BUF_SIZE, 1, [Define if LV2 Buf-size is supported.])
   else
      AC_MSG_WARN([*** LV2 Buf-size support will be disabled.])
   fi
fi
if test "x$ac_lv2_time_position" = "xyes"; then
   AC_DEFINE(CONFIG_LV2_TIME_POSITION, 1, [Define if LV2 Time/position support is available.])
fi
if test "x$ac_lv2_ui_touch" = "xyes"; then
   AC_CACHE_CHECK([for LV2 UI Touch interface],
      ac_cv_lv2_ui_touch, [
      AC_TRY_LINK([#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"], [
         // Checking for LV2 UI Touch interface...
         LV2UI_Touch touch;
      ], ac_cv_lv2_ui_touch="yes", ac_cv_lv2_ui_touch="no")
   ])
   ac_lv2_ui_touch=$ac_cv_lv2_ui_touch
   if test "x$ac_lv2_ui_touch" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_UI_TOUCH, 1, [Define if LV2 UI Touch interface support is available.])
   else
      AC_MSG_WARN([*** LV2 UI Touch interface support will be disabled.])
   fi
fi
if test "x$ac_lv2_ui_idle" = "xyes"; then
   AC_CACHE_CHECK([for LV2 UI Idle interface],
      ac_cv_lv2_ui_idle, [
      AC_TRY_LINK([#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"], [
         // Checking for LV2 UI Idle interface...
         LV2UI_Idle_Interface idle;
      ], ac_cv_lv2_ui_idle="yes", ac_cv_lv2_ui_idle="no")
   ])
   ac_lv2_ui_idle=$ac_cv_lv2_ui_idle
   if test "x$ac_lv2_ui_idle" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_UI_IDLE, 1, [Define if LV2 UI Idle interface support is available.])
   else
      AC_MSG_WARN([*** LV2 UI Idle interface support will be disabled.])
   fi
fi
if test "x$ac_lv2_ui_show" = "xyes"; then
   AC_CACHE_CHECK([for LV2 UI Show interface],
      ac_cv_lv2_ui_show, [
      AC_TRY_LINK([#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"], [
         // Checking for LV2 UI Show interface...
         LV2UI_Show_Interface show;
      ], ac_cv_lv2_ui_show="yes", ac_cv_lv2_ui_show="no")
   ])
   ac_lv2_ui_show=$ac_cv_lv2_ui_show
   if test "x$ac_lv2_ui_show" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_UI_SHOW, 1, [Define if LV2 UI Show interface support is available.])
   else
      AC_MSG_WARN([*** LV2 UI Show interface support will be disabled.])
   fi
fi

if test "x$ac_suil" = "xyes"; then
   AC_CACHE_CHECK([for LV2 UI GTK2 support (libsuil_gtk2_in_qt5)],
      ac_cv_suil_gtk2_in_qt5, [
      AC_TRY_RUN([
         #include <suil/suil.h>
         #include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
         #ifndef LV2_UI__Qt5UI
         #define LV2_UI__Qt5UI LV2_UI_PREFIX "Qt5UI"
         #endif
         int main() { return !suil_ui_supported(LV2_UI__Qt5UI, LV2_UI__GtkUI); }
      ], ac_cv_suil_gtk2_in_qt5="yes", ac_cv_suil_gtk2_in_qt5="no")
   ])
   if test "x$ac_cv_suil_gtk2_in_qt5" = "xyes"; then
      AC_DEFINE(CONFIG_LIBSUIL_GTK2_IN_QT5, 1, [Define if libsuil_gtk2_in_qt5 is available.])
   #  ac_lv2_ui_gtk2="no"
   fi
fi
if test "x$ac_lv2_ui_gtk2" = "xyes"; then
   PKG_CHECK_MODULES([GTK2], [gtk+-2.0], [ac_lv2_ui_gtk2="yes"], [ac_lv2_ui_gtk2="no"])
fi
if test "x$ac_lv2_ui_gtk2" = "xyes"; then
   AC_DEFINE(CONFIG_LV2_UI_GTK2, 1, [Define if LV2 UI GTK2 native support is available.])
   ac_cflags="$ac_cflags $GTK2_CFLAGS"
   ac_libs="$ac_libs $GTK2_LIBS"
else
   AC_MSG_WARN([*** LV2 UI GTK2 native support will be disabled.])
fi

if test "x$ac_suil" = "xyes"; then
   AC_CACHE_CHECK([for LV2 UI X11 support (libsuil_x11_in_qt5)],
      ac_cv_suil_x11_in_qt5, [
      AC_TRY_RUN([
         #include <suil/suil.h>
         #include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
         #ifndef LV2_UI__Qt5UI
         #define LV2_UI__Qt5UI LV2_UI_PREFIX "Qt5UI"
         #endif
         int main() { return !suil_ui_supported(LV2_UI__Qt5UI, LV2_UI__X11UI); }
      ], ac_cv_suil_x11_in_qt5="yes", ac_cv_suil_x11_in_qt5="no")
   ])
   if test "x$ac_cv_suil_x11_in_qt5" = "xyes"; then
      AC_DEFINE(CONFIG_LIBSUIL_X11_IN_QT5, 1, [Define if libsuil_x11_in_qt5 is available.])
   #  ac_lv2_ui_x11="no"
   fi
fi
if test "x$ac_lv2_ui_x11" = "xyes"; then
   AC_DEFINE(CONFIG_LV2_UI_X11, 1, [Define if LV2 UI X11 native support is available.])
fi

# Check for JACK session headers availability.
if test "x$ac_jack_session" = "xyes"; then
   AC_CHECK_HEADER(jack/session.h, [ac_jack_session="yes"], [ac_jack_session="no"])
   if test "x$ac_jack_session" = "xyes"; then
      AC_DEFINE(CONFIG_JACK_SESSION, 1, [Define if JACK session support is available.])
   else
      AC_MSG_WARN([*** jack/session.h file not found.])
      AC_MSG_WARN([*** JACK session support will be disabled.])
   fi
fi

# Check for JACK metadata headers availability.
if test "x$ac_jack_metadata" = "xyes"; then
   AC_CHECK_HEADER(jack/metadata.h, [ac_jack_metadata="yes"], [ac_jack_metadata="no"])
   if test "x$ac_jack_metadata" = "xyes"; then
      AC_DEFINE(CONFIG_JACK_METADATA, 1, [Define if JACK metadata support is available.])
   else
      AC_MSG_WARN([*** jack/metadata.h file not found.])
      AC_MSG_WARN([*** JACK metadata support will be disabled.])
   fi
fi

# Check for jack_set_port_rename_callback
AC_CHECK_LIB(jack, jack_set_port_rename_callback, [ac_jack_port_rename="yes"], [ac_jack_port_rename="no"])
if test "x$ac_jack_port_rename" = "xyes"; then
   AC_TRY_COMPILE([#include <jack/jack.h>], [
      void port_rename(jack_port_id_t, const char *, const char *, void *);
      jack_set_port_rename_callback(0, port_rename, 0);
   ], ac_jack_port_rename="yes", ac_jack_port_rename="no")
fi
if test "x$ac_jack_port_rename" = "xyes"; then
   AC_DEFINE(CONFIG_JACK_PORT_RENAME, 1, [Define if jack_set_port_rename_callback is available.])
fi

# Check for NSM support.
if test "x$ac_nsm" = "xyes"; then
   AC_DEFINE(CONFIG_NSM, 1, [Define if NSM support is available.])
fi

# Check for VST/X11 support.
if test "x$ac_vst" = "xyes"; then
   PKG_CHECK_MODULES([QT5X11EXTRAS], [Qt5X11Extras], [ac_qt5x11extras="yes"], [ac_qt5x11extras="no"])
fi
if test "x$ac_qt5x11extras" = "xyes"; then
   ac_qx11extras="x11extras"
   # Some recent distros (eg. fedora, debian) require this.
   if test "x$ac_cv_lib_X11_main" = "xyes"; then
      ac_libs="$ac_libs -lX11"
   fi
fi
AC_SUBST(ac_qx11extras)

# Check for unique/single instance support.
if test "x$ac_xunique" = "xyes"; then
   PKG_CHECK_MODULES([QT5NETWORK], [Qt5Network], [ac_qt5network="yes"], [ac_qt5network="no"])
   if test "x$ac_qt5network" = "xyes"; then
      AC_DEFINE(CONFIG_XUNIQUE, 1, [Define if unique/single instance is enabled.])
      ac_qnetwork="network"
   fi
fi
AC_SUBST(ac_qnetwork)

# Check for gradient eye-candy.
if test "x$ac_gradient" = "xyes"; then
   AC_DEFINE(CONFIG_GRADIENT, 1, [Define if gradient eye-candy is enabled.])
fi

# Check for debugging stack-trace.
if test "x$ac_stacktrace" = "xyes"; then
   AC_DEFINE(CONFIG_STACKTRACE, 1, [Define if debugger stack-trace is enabled.])
fi

AC_SUBST(ac_cflags)
AC_SUBST(ac_ldflags)
AC_SUBST(ac_incpath)
AC_SUBST(ac_libs)


# Checks for typedefs, structures, and compiler characteristics.
# AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS(system)

# Finally produce a configure header file and the makefiles.
AC_OUTPUT

# make clean > /dev/null 2>&1

# Output summary message

echo
echo "  $PACKAGE_NAME $ac_build_version"
echo
echo "  Build target . . . . . . . . . . . . . . . . . . .: $ac_debug"
echo
echo "  JACK Audio Connection Kit support  . . . . . . . .: $ac_libjack"
echo "  ALSA MIDI Sequencer support  . . . . . . . . . . .: $ac_libasound"
echo "  General audio file support (libsndfile)  . . . . .: $ac_libsndfile"
echo "  Ogg Vorbis audio file support (libvorbis)  . . . .: $ac_libvorbis"
echo "  MPEG-1 Audio Layer 3 file support (libmad) . . . .: $ac_libmad"
echo "  Sample-rate conversion support (libsamplerate) . .: $ac_libsamplerate"
echo "  Pitch-shifting support (librubberband) . . . . . .: $ac_librubberband"
echo "  Beat-detection support (libaubio)  . . . . . . . .: $ac_libaubio"
echo "  OSC service support (liblo)  . . . . . . . . . . .: $ac_liblo"
echo "  Archive/Zip file support (zlib)  . . . . . . . . .: $ac_libz"
echo "  IEEE 32bit float optimizations . . . . . . . . . .: $ac_float32"
echo "  SSE optimization support (x86) . . . . . . . . . .: $ac_sse"
echo "  LADSPA plug-in support . . . . . . . . . . . . . .: $ac_ladspa"
echo "  DSSI plug-in support . . . . . . . . . . . . . . .: $ac_dssi"
echo "  VST plug-in support  . . . . . . . . . . . . . . .: $ac_vst"
echo "  LV2 plug-in support  . . . . . . . . . . . . . . .: $ac_lv2"
echo "  LV2 plug-in support (liblilv)  . . . . . . . . . .: $ac_lilv"
echo "  LV2 plug-in UI support . . . . . . . . . . . . . .: $ac_lv2_ui"
echo "  LV2 plug-in UI support (libsuil) . . . . . . . . .: $ac_suil"
echo "  LV2 plug-in External UI support  . . . . . . . . .: $ac_lv2_external_ui"
echo "  LV2 plug-in MIDI/Event support (DEPRECATED)  . . .: $ac_lv2_event"
echo "  LV2 plug-in MIDI/Atom support  . . . . . . . . . .: $ac_lv2_atom"
echo "  LV2 plug-in Worker/Schedule support  . . . . . . .: $ac_lv2_worker"
echo "  LV2 plug-in State support  . . . . . . . . . . . .: $ac_lv2_state"
echo "  LV2 plug-in State Files support  . . . . . . . . .: $ac_lv2_state_files"
echo "  LV2 plug-in State Make Path support (DANGEROUS)  .: $ac_lv2_state_make_path"
echo "  LV2 plug-in Programs support . . . . . . . . . . .: $ac_lv2_programs"
echo "  LV2 plug-in Presets support  . . . . . . . . . . .: $ac_lv2_presets"
echo "  LV2 plug-in Patch support  . . . . . . . . . . . .: $ac_lv2_patch"
echo "  LV2 plug-in Time support . . . . . . . . . . . . .: $ac_lv2_time"
echo "  LV2 plug-in Time/position support  . . . . . . . .: $ac_lv2_time_position"
echo "  LV2 plug-in Options support  . . . . . . . . . . .: $ac_lv2_options"
echo "  LV2 plug-in Buf-size support . . . . . . . . . . .: $ac_lv2_buf_size"
echo "  LV2 plug-in UI Touch interface support . . . . . .: $ac_lv2_ui_touch"
echo "  LV2 plug-in UI Idle interface support  . . . . . .: $ac_lv2_ui_idle"
echo "  LV2 plug-in UI Show interface support  . . . . . .: $ac_lv2_ui_show"
echo "  LV2 plug-in UI GTK2 native support . . . . . . . .: $ac_lv2_ui_gtk2"
echo "  LV2 plug-in UI X11 native support  . . . . . . . .: $ac_lv2_ui_x11"
echo
echo "  JACK Session support . . . . . . . . . . . . . . .: $ac_jack_session"
echo "  JACK Latency support . . . . . . . . . . . . . . .: $ac_jack_latency"
echo "  JACK Metadata support  . . . . . . . . . . . . . .: $ac_jack_metadata"
echo
echo "  Non Session Management (NSM) support . . . . . . .: $ac_nsm"
echo
echo "  VeSTige header support . . . . . . . . . . . . . .: $ac_vestige"
echo "  Unique/Single instance support . . . . . . . . . .: $ac_xunique"
echo "  Gradient eye-candy . . . . . . . . . . . . . . . .: $ac_gradient"
echo "  Debugger stack-trace (gdb) . . . . . . . . . . . .: $ac_stacktrace"
echo
echo "  Install prefix . . . . . . . . . . . . . . . . . .: $ac_prefix"
echo
echo "Now type 'make', followed by 'make install' as root."
echo

