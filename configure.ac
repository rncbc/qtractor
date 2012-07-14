# Process this file with utoconf to produce a configure script.
AC_INIT(Qtractor, 0.5.5.8, rncbc@rncbc.org, qtractor)

AC_CONFIG_SRCDIR(src/qtractor.cpp)
AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES(Makefile qtractor.spec src/src.pri src/qtractor.desktop)

# Set default installation prefix.
AC_PREFIX_DEFAULT(/usr/local)
ac_prefix=$prefix
if test "x$ac_prefix" = "xNONE"; then
    ac_prefix=$ac_default_prefix
fi
AC_SUBST(ac_prefix)
AC_DEFINE_UNQUOTED(CONFIG_PREFIX, ["$ac_prefix"], [Default installation prefix.])

# Enable debugging argument option.
AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--enable-debug], [enable debugging (default=no)]),
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
  AC_HELP_STRING([--enable-libvorbis], [enable libvorbis interface (default=yes)]),
  [ac_libvorbis="$enableval"],
  [ac_libvorbis="yes"])

# Enable libmad availability.
AC_ARG_ENABLE(libmad,
  AC_HELP_STRING([--enable-libmad], [enable libmad interface (default=yes)]),
  [ac_libmad="$enableval"],
  [ac_libmad="yes"])

# Enable libsamplerate availability.
AC_ARG_ENABLE(libsamplerate,
  AC_HELP_STRING([--enable-libsamplerate], [enable libsamplerate interface (default=yes)]),
  [ac_libsamplerate="$enableval"],
  [ac_libsamplerate="yes"])

# Enable librubberband availability.
AC_ARG_ENABLE(librubberband,
  AC_HELP_STRING([--enable-librubberband], [enable librubberband interface (default=yes)]),
  [ac_librubberband="$enableval"],
  [ac_librubberband="yes"])

# Enable liblo availability.
AC_ARG_ENABLE(liblo,
  AC_HELP_STRING([--enable-liblo], [enable liblo interface (default=yes)]),
  [ac_liblo="$enableval"],
  [ac_liblo="yes"])

# Enable libz availability.
AC_ARG_ENABLE(libz,
  AC_HELP_STRING([--enable-libz], [enable libz interface (default=yes)]),
  [ac_libz="$enableval"],
  [ac_libz="yes"])

# Enable SLV2 support.
AC_ARG_ENABLE(slv2,
  AC_HELP_STRING([--enable-slv2], [enable SLV2 interface support (default=no)]),
  [ac_slv2="$enableval"],
  [ac_slv2="no"])

# Enable LILV support.
AC_ARG_ENABLE(lilv,
  AC_HELP_STRING([--enable-lilv], [enable LILV interface support (default=yes)]),
  [ac_lilv="$enableval"],
  [ac_lilv="yes"])

# Enable SUIL support.
AC_ARG_ENABLE(suil,
  AC_HELP_STRING([--enable-suil], [enable SUIL interface support (default=yes)]),
  [ac_suil="$enableval"],
  [ac_suil="yes"])

# Enable SSE optimization.
AC_ARG_ENABLE(sse,
  AC_HELP_STRING([--enable-sse], [enable SSE optimization (default=yes)]),
  [ac_sse="$enableval"],
  [ac_sse="yes"])

# Enable LADSPA support.
AC_ARG_ENABLE(ladspa,
  AC_HELP_STRING([--enable-ladspa], [enable LADSPA plug-in support (default=yes)]),
  [ac_ladspa="$enableval"],
  [ac_ladspa="yes"])

# Enable DSSI support.
AC_ARG_ENABLE(dssi,
  AC_HELP_STRING([--enable-dssi], [enable DSSI plug-in support (default=yes)]),
  [ac_dssi="$enableval"],
  [ac_dssi="yes"])

# Enable VST support.
AC_ARG_ENABLE(vst,
  AC_HELP_STRING([--enable-vst], [enable VST plug-in support (default=yes)]),
  [ac_vst="$enableval"],
  [ac_vst="yes"])

AC_ARG_ENABLE(vestige,
  AC_HELP_STRING([--enable-vestige], [enable VeSTige header support (default=yes)]),
  [ac_vestige="$enableval"],
  [ac_vestige="yes"])

# Enable LV2 support.
AC_ARG_ENABLE(lv2,
  AC_HELP_STRING([--enable-lv2], [enable LV2 plug-in support (default=yes)]),
  [ac_lv2="$enableval"],
  [ac_lv2="yes"])

AC_ARG_ENABLE(lv2-event,
  AC_HELP_STRING([--enable-lv2-event], [enable LV2 plug-in MIDI/Event support (default=yes)]),
  [ac_lv2_event="$enableval"],
  [ac_lv2_event="yes"])

AC_ARG_ENABLE(lv2-atom,
  AC_HELP_STRING([--enable-lv2-atom], [enable LV2 plug-in MIDI/Atom support (default=yes)]),
  [ac_lv2_atom="$enableval"],
  [ac_lv2_atom="yes"])

AC_ARG_ENABLE(lv2-worker,
  AC_HELP_STRING([--enable-lv2-worker], [enable LV2 plug-in worker/schedule support (default=yes)]),
  [ac_lv2_worker="$enableval"],
  [ac_lv2_worker="yes"])

AC_ARG_ENABLE(lv2-gtk-ui,
  AC_HELP_STRING([--enable-lv2-gtk-ui], [enable LV2 plug-in GTK UI support (default=yes)]),
  [ac_lv2_gtk_ui="$enableval"],
  [ac_lv2_gtk_ui="yes"])

AC_ARG_ENABLE(lv2-qt4-ui,
  AC_HELP_STRING([--enable-lv2-qt4-ui], [enable LV2 plug-in QT4 UI support (default=yes)]),
  [ac_lv2_qt4_ui="$enableval"],
  [ac_lv2_qt4_ui="yes"])

AC_ARG_ENABLE(lv2-external-ui,
  AC_HELP_STRING([--enable-lv2-external-ui], [enable LV2 plug-in external UI support (default=yes)]),
  [ac_lv2_external_ui="$enableval"],
  [ac_lv2_external_ui="yes"])

AC_ARG_ENABLE(lv2-state,
  AC_HELP_STRING([--enable-lv2-state], [enable LV2 plug-in state support (default=yes)]),
  [ac_lv2_state="$enableval"],
  [ac_lv2_state="yes"])

AC_ARG_ENABLE(lv2-state-files,
  AC_HELP_STRING([--enable-lv2-state-files], [enable LV2 plug-in state files support (default=yes)]),
  [ac_lv2_state_files="$enableval"],
  [ac_lv2_state_files="yes"])

AC_ARG_ENABLE(lv2-programs,
  AC_HELP_STRING([--enable-lv2-programs], [enable LV2 plug-in programs support (default=yes)]),
  [ac_lv2_programs="$enableval"],
  [ac_lv2_programs="yes"])

AC_ARG_ENABLE(lv2-presets,
  AC_HELP_STRING([--enable-lv2-presets], [enable LV2 plug-in presets support (default=yes)]),
  [ac_lv2_presets="$enableval"],
  [ac_lv2_presets="yes"])

AC_ARG_ENABLE(lv2-time,
  AC_HELP_STRING([--enable-lv2-time], [enable LV2 plug-in time/position support (default=yes)]),
  [ac_lv2_time="$enableval"],
  [ac_lv2_time="yes"])

# Enable JACK session support.
AC_ARG_ENABLE(jack-session,
  AC_HELP_STRING([--enable-jack-session], [enable JACK session support (default=yes)]),
  [ac_jack_session="$enableval"],
  [ac_jack_session="yes"])

# Enable JACK latency support.
AC_ARG_ENABLE(jack-latency,
  AC_HELP_STRING([--enable-jack-latency], [enable JACK latency support (default=yes)]),
  [ac_jack_latency="$enableval"],
  [ac_jack_latency="yes"])

# Enable X11 unique/single instance.
AC_ARG_ENABLE(xunique,
  AC_HELP_STRING([--enable-xunique], [enable X11 unique/single instance (default=no)]),
  [ac_xunique="$enableval"],
  [ac_xunique="no"])

# Enable gradient eye-candy.
AC_ARG_ENABLE(gradient,
  AC_HELP_STRING([--enable-gradient], [enable gradient eye-candy (default=yes)]),
  [ac_gradient="$enableval"],
  [ac_gradient="yes"])

# Enable debugger stack-trace option (assumes --enable-debug).
AC_ARG_ENABLE(stacktrace,
  AC_HELP_STRING([--enable-stacktrace], [enable debugger stack-trace (default=no)]),
  [ac_stacktrace="$enableval"])


# Check for new alternative to SLV2 is disabled.
if test "x$ac_lv2" = "xno"; then
   ac_slv2="no"
   ac_lilv="no"
   ac_suil="no"
elif test "x$ac_slv2" = "xyes"; then
   ac_lilv="no"
   ac_suil="no"
elif test "x$ac_lilv" = "xno"; then
   ac_slv2="yes"
   ac_suil="no"
fi


# Standard installation base dirs.
ac_with_paths="/usr /usr/local"

# Some a-la-debian alternatives...
for X in /usr/lib /usr/lib64 /usr/share; do
  for Y in qt qt4; do
    if test -d $X/$Y/bin; then
      ac_with_paths="$ac_with_paths $X/$Y"
    fi
  done
done

# Set for alternate Qt installation dir.
AC_ARG_WITH(qt,
  AC_HELP_STRING([--with-qt=PATH], [use alternate Qt install path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate JACK installation dir.
AC_ARG_WITH(jack,
  AC_HELP_STRING([--with-jack=PATH], [use alternate JACK install path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate ALSA installation dir.
AC_ARG_WITH(alsa,
  AC_HELP_STRING([--with-alsa=PATH], [use alternate ALSA install path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libsndfile installation dir.
AC_ARG_WITH(libsndfile,
  AC_HELP_STRING([--with-libsndfile=PATH], [use alternate libsndfile path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libvorbis installation dir.
AC_ARG_WITH(libvorbis,
  AC_HELP_STRING([--with-libvorbis=PATH], [use alternate libvorbis path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libmad installation dir.
AC_ARG_WITH(libmad,
  AC_HELP_STRING([--with-libmad=PATH], [use alternate libmad path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libsamplerate installation dir.
AC_ARG_WITH(libsamplerate,
  AC_HELP_STRING([--with-libsamplerate=PATH], [use alternate libsamplerate path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate librubberband installation dir.
AC_ARG_WITH(librubberband,
  AC_HELP_STRING([--with-librubberband=PATH], [use alternate librubberband path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate liblo installation dir.
AC_ARG_WITH(liblo,
  AC_HELP_STRING([--with-liblo=PATH], [use alternate liblo path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libz installation dir.
AC_ARG_WITH(libz,
  AC_HELP_STRING([--with-libz=PATH], [use alternate libz path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libslv2 installation dir.
AC_ARG_WITH(libslv2,
  AC_HELP_STRING([--with-libslv2=PATH], [use alternate libslv2 path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate liblilv installation dir.
AC_ARG_WITH(liblilv,
  AC_HELP_STRING([--with-liblilv=PATH], [use alternate liblilv path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate libsuil installation dir.
AC_ARG_WITH(libsuil,
  AC_HELP_STRING([--with-libsuil=PATH], [use alternate libsuil path]),
  [ac_with_paths="$ac_with_paths $withval"])

# Set for alternate LADSPA include dir.
AC_ARG_WITH(ladspa,
  AC_HELP_STRING([--with-ladspa=PATH], [use alternate LADSPA header path]),
  [ac_with_ladspa="$withval"])

# Set for alternate DSSI include dir.
AC_ARG_WITH(dssi,
  AC_HELP_STRING([--with-dssi=PATH], [use alternate DSSI header path]),
  [ac_with_dssi="$withval"])

# Set for alternate VST include dir.
AC_ARG_WITH(vst,
  AC_HELP_STRING([--with-vst=PATH], [use alternate VST header path]),
  [ac_with_vst="$withval"])

# Set for alternate LV2 include dir.
AC_ARG_WITH(lv2,
  AC_HELP_STRING([--with-lv2=PATH], [use alternate LV2 header path]),
  [ac_with_lv2="$withval"])


# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_GCC_TRADITIONAL

# Checks for languages.
AC_LANG_C
AC_LANG_CPLUSPLUS

# Prepend alternate dependencies paths.
ac_path=$PATH
for X in $ac_with_paths; do
  if test -d $X/bin; then
    ac_path="$X/bin:$ac_path"
  fi
  if test -x $X/qmake; then
    ac_path="$X:$ac_path"
  fi
  if test -d $X/include; then
    for Y in qt qt4; do
      if test -d $X/include/$Y; then
        CFLAGS="-I$X/include/$Y $CFLAGS"
        CPPFLAGS="-I$X/include/$Y $CPPFLAGS"
        ac_incpath="$X/include/$Y $ac_incpath"
      fi
    done
    CFLAGS="-I$X/include $CFLAGS"
    CPPFLAGS="-I$X/include $CPPFLAGS"
    ac_incpath="$X/include $ac_incpath"
  fi
  if test -d $X/lib64; then
    LIBS="-L$X/lib64 $LIBS"
    ac_libs="-L$X/lib64 $ac_libs"
  fi
  if test -d $X/lib; then
    LIBS="-L$X/lib $LIBS"
    ac_libs="-L$X/lib $ac_libs"
  fi
done


# Check for proper Qt version.
AC_CACHE_CHECK([for Qt library version >= 4.2],
   ac_cv_qtversion, [
   AC_TRY_LINK([#include "Qt/qglobal.h"], [
      #if QT_VERSION < 0x040200
      #error Qt library 4.2 or greater required.
      #endif
   ], ac_cv_qtversion="yes", [
      echo "no; Qt 4.2 or greater is required"
      exit 1
   ])
])


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
             __asm__ __volatile__
             ("cpuid": "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "a" (1));
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


# A common error message:
ac_errmsg="not found in current PATH. Maybe QT development environment isn't available (qt-devel)."

# Check for Qt qmake utility.
AC_PATH_PROG(ac_qmake, qmake, [no], $ac_path)
if test "x$ac_qmake" = "xno"; then
   AC_MSG_ERROR([qmake $ac_errmsg])
fi
AC_SUBST(ac_qmake)

# Check for Qt moc utility.
AC_PATH_PROG(ac_moc, moc, [no], $ac_path)
if test "x$ac_moc" = "xno"; then
   AC_MSG_ERROR([moc $ac_errmsg])
fi
AC_SUBST(ac_moc)

# Check for Qt uic utility.
AC_PATH_PROG(ac_uic, uic, [no], $ac_path)
if test "x$ac_uic" = "xno"; then
   AC_MSG_ERROR([uic $ac_errmsg])
fi
AC_SUBST(ac_uic)

# Check for Qt lupdate utility.
AC_PATH_PROG(ac_lupdate, lupdate, [no], $ac_path)
if test "x$ac_lupdate" = "xno"; then
   AC_MSG_ERROR([lupdate $ac_errmsg])
fi
AC_SUBST(ac_lupdate)

# Check for Qt lrelease utility.
AC_PATH_PROG(ac_lrelease, lrelease, [no], $ac_path)
if test "x$ac_release" = "xno"; then
   AC_MSG_ERROR([lrelease $ac_errmsg])
fi
AC_SUBST(ac_lrelease)


# Check for pkg-config utility (needed for LV2 GTK UI support).
AC_PATH_PROG(ac_pkg_config, pkg-config, [no], $ac_path)
if test "x$ac_pkg_config" = "xno"; then
   ac_lv2="no"
   ac_slv2="no"
   ac_lilv="no"
   ac_suil="no"
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
AC_CHECK_LIB(jack, main, [ac_libjack="yes"], [ac_libjack="no"])
if test "x$ac_libjack" = "xno"; then
   AC_MSG_ERROR([JACK library not found.])
else
   ac_libs="$ac_libs -ljack"
fi

# Check for ALSA libraries.
AC_CHECK_LIB(asound, main, [ac_libasound="yes"], [ac_libasound="no"])
if test "x$ac_libasound" = "xno"; then
   AC_MSG_ERROR([ALSA library not found.])
else
   ac_libs="$ac_libs -lasound"
fi

# Check for sndfile library.
AC_CHECK_LIB(sndfile, main, [ac_libsndfile="yes"], [ac_libsndfile="no"])
if test "x$ac_libsndfile" = "xno"; then
   AC_MSG_ERROR([sndfile library not found.])
else
   ac_libs="$ac_libs -lsndfile"
fi

# Check for option libvorbis libraries.
if test "x$ac_libvorbis" = "xyes"; then
   AC_CHECK_LIB(ogg, main, [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_libs="$ac_libs -logg"
   else
      AC_MSG_WARN([*** ogg library not found.])
   fi
fi

if test "x$ac_libvorbis" = "xyes"; then
   AC_CHECK_LIB(vorbis, main, [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_libs="$ac_libs -lvorbis"
   else
      AC_MSG_WARN([*** vorbis library not found.])
   fi
fi

if test "x$ac_libvorbis" = "xyes"; then
   AC_CHECK_LIB(vorbisenc, main, [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_libs="$ac_libs -lvorbisenc"
   else
      AC_MSG_WARN([*** vorbisenc library not found.])
   fi
fi

if test "x$ac_libvorbis" = "xyes"; then
   AC_CHECK_LIB(vorbisfile, main, [ac_libvorbis="yes"], [ac_libvorbis="no"])
   if test "x$ac_libvorbis" = "xyes"; then
      ac_libs="$ac_libs -lvorbisfile"
   else
      AC_MSG_WARN([*** vorbisfile library not found.])
   fi
fi

if test "x$ac_libvorbis" = "xyes"; then
   AC_DEFINE(CONFIG_LIBVORBIS, 1, [Define if libvorbis is available.])
else
   AC_MSG_WARN([*** Ogg Vorbis audio file support will be disabled.])
fi

# Check for optional libmad library.
if test "x$ac_libmad" = "xyes"; then
   AC_CHECK_LIB(mad, main, [ac_libmad="yes"], [ac_libmad="no"])
   if test "x$ac_libmad" = "xyes"; then
      AC_DEFINE(CONFIG_LIBMAD, 1, [Define if libmad is available.])
      ac_libs="$ac_libs -lmad"
   else
      AC_MSG_WARN([*** mad library not found.])
      AC_MSG_WARN([*** MPEG Layer III audio file support will be disabled.])
   fi
fi

# Check for optional libsamplerate library.
if test "x$ac_libsamplerate" = "xyes"; then
   AC_CHECK_LIB(samplerate, main, [ac_libsamplerate="yes"], [ac_libsamplerate="no"])
   if test "x$ac_libsamplerate" = "xyes"; then
      AC_DEFINE(CONFIG_LIBSAMPLERATE, 1, [Define if libsamplerate is available.])
      ac_libs="$ac_libs -lsamplerate"
   else
      AC_MSG_WARN([*** samplerate library not found.])
      AC_MSG_WARN([*** Sample-rate conversion support will be disabled.])
   fi
fi

# Check for optional librubberband library.
if test "x$ac_librubberband" = "xyes"; then
   AC_CHECK_LIB(rubberband, main, [ac_librubberband="yes"], [ac_librubberband="no"])
   if test "x$ac_librubberband" = "xyes"; then
      AC_DEFINE(CONFIG_LIBRUBBERBAND, 1, [Define if librubberband is available.])
      ac_libs="$ac_libs -lrubberband"
   else
      AC_MSG_WARN([*** rubberband library not found.])
      AC_MSG_WARN([*** Pitch-shifting support will be disabled.])
   fi
fi

# Check for optional liblo library.
if test "x$ac_liblo" = "xyes"; then
   AC_CHECK_LIB(lo, main, [ac_liblo="yes"], [ac_liblo="no"])
   if test "x$ac_liblo" = "xyes"; then
      AC_DEFINE(CONFIG_LIBLO, 1, [Define if liblo is available.])
      ac_libs="$ac_libs -llo"
   else
      AC_MSG_WARN([*** lo library not found.])
      AC_MSG_WARN([*** OSC service support will be disabled.])
   fi
fi

# Check for optional libz library.
if test "x$ac_libz" = "xyes"; then
   AC_CHECK_LIB(z, main, [ac_libz="yes"], [ac_libz="no"])
   if test "x$ac_libz" = "xyes"; then
      AC_DEFINE(CONFIG_LIBZ, 1, [Define if libz is available.])
      ac_libs="$ac_libs -lz"
   else
      AC_MSG_WARN([*** z library not found.])
      AC_MSG_WARN([*** Archive/Zip file support will be disabled.])
   fi
fi

# Check for optional liblilv library.
if test "x$ac_lilv" = "xyes"; then
   AC_CHECK_LIB(lilv-0, main, [ac_lilv="yes"], [ac_lilv="no"])
fi
if test "x$ac_lilv" = "xyes"; then
   ac_lilv_libs=`$ac_pkg_config --libs lilv-0 2>/dev/null`
   if test -n "$ac_lilv_libs"; then
      ac_libs="$ac_libs $ac_lilv_libs"
      AC_DEFINE(CONFIG_LIBLILV, 1, [Define if liblilv is available.])
      ac_slv2="no"
   else
      AC_MSG_WARN([*** lilv library not found.])
      ac_slv2="yes"
      ac_lilv="no"
      ac_suil="no"
      ac_lv2_presets="no"
      ac_lv2_time="no"
   fi
else
   ac_suil="no"
   ac_lv2_presets="no"
   ac_lv2_time="no"
fi

# Check for optional libslv2 library.
if test "x$ac_slv2" = "xyes"; then
   AC_CHECK_LIB(slv2, main, [ac_slv2="yes"], [ac_slv2="no"])
fi
if test "x$ac_slv2" = "xyes"; then
   ac_slv2_libs=`$ac_pkg_config --libs slv2 2>/dev/null`
   if test -n "$ac_slv2_libs"; then
      ac_libs="$ac_libs $ac_slv2_libs"
      AC_DEFINE(CONFIG_LIBSLV2, 1, [Define if libslv2 is available.])
      ac_lilv="no"
      ac_suil="no"
      ac_lv2_presets="no"
      ac_lv2_time="no"
   else
      AC_MSG_WARN([*** slv2 library not found.])
      AC_MSG_WARN([*** LV2 plugin support will be disabled.])
      ac_slv2="no"
      ac_lv2="no"
      ac_lv2_event="no"
      ac_lv2_atom="no"
      ac_lv2_worker="no"
      ac_lv2_gtk_ui="no"
      ac_lv2_qt4_ui="no"
      ac_lv2_external_ui="no"
      ac_lv2_state="no"
      ac_lv2_state_files="no"
      ac_lv2_programs="no"
      ac_lv2_presets="no"
      ac_lv2_time="no"
   fi
fi

# Check for LV2 new UI instantiation availability (libsuil).
if test "x$ac_suil" = "xyes"; then
   AC_CHECK_LIB(suil-0, main, [ac_suil="yes"], [ac_suil="no"])
fi
if test "x$ac_suil" = "xyes"; then
   ac_suil_libs=`$ac_pkg_config --libs suil-0 2>/dev/null`
   if test -n "$ac_suil_libs"; then
      ac_libs="$ac_libs $ac_suil_libs"
      AC_DEFINE(CONFIG_LIBSUIL, 1, [Define if libsuil is available.])
   else
      AC_MSG_WARN([*** suil library not found.])
      ac_suil="no"
      ac_lv2_gtk_ui="no"
      ac_lv2_qt4_ui="no"
      ac_lv2_external_ui="no"
   fi
elif test "x$ac_lilv" = "xyes"; then
  ac_lv2_gtk_ui="no"
  ac_lv2_qt4_ui="no"
  ac_lv2_external_ui="no"
fi

# Check for either LV2 host library (libslv2 or liblilv).
if test "x$ac_slv2" = "xno" -a "x$ac_lilv" = "xno"; then
   ac_lv2="no"
   ac_lv2_event="no"
   ac_lv2_atom="no"
   ac_lv2_worker="no"
   ac_lv2_gtk_ui="no"
   ac_lv2_qt4_ui="no"
   ac_lv2_external_ui="no"
   ac_lv2_state="no"
   ac_lv2_state_files="no"
   ac_lv2_programs="no"
   ac_lv2_presets="no"
   ac_lv2_time="no"
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

# Some recent distros (eg. fedora, debian) require this.
ac_libs="$ac_libs -lX11"

AC_SUBST(ac_libs)


# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/ioctl.h sys/stat.h unistd.h signal.h)

# Check for JACK headers.
AC_CHECK_HEADER(jack/jack.h, [ac_jack_h="yes"], [ac_jack_h="no"])
if test "x$ac_jack_h" = "xno"; then
   AC_MSG_ERROR([JACK headers not found.])
fi

# Check for ALSA headers.
AC_CHECK_HEADER(alsa/asoundlib.h, [ac_alsa_h="yes"], [ac_alsa_h="no"])
if test "x$ac_alsa_h" = "xno"; then
   AC_MSG_ERROR([ALSA headers not found.])
fi

# Check for sndfile headers.
AC_CHECK_HEADER(sndfile.h, [ac_sndfile_h="yes"], [ac_sndfile_h="no"])
if test "x$ac_sndfile_h" = "xno"; then
   AC_MSG_ERROR([sndfile.h header file not found.])
fi

# Check for vorbisfile headers.
if test "x$ac_libvorbis" = "xyes"; then
   AC_CHECK_HEADER(vorbis/vorbisenc.h, [ac_vorbisenc_h="yes"], [ac_vorbisenc_h="no"])
   if test "x$ac_vorbisenc_h" = "xno"; then
      AC_MSG_ERROR([vorbisenc.h header file not found.])
   fi
   AC_CHECK_HEADER(vorbis/vorbisfile.h, [ac_vorbisfile_h="yes"], [ac_vorbisfile_h="no"])
   if test "x$ac_vorbisfile_h" = "xno"; then
      AC_MSG_ERROR([vorbisfile.h header file not found.])
   fi
fi

# Check for mad headers.
if test "x$ac_libmad" = "xyes"; then
   AC_CHECK_HEADER(mad.h, [ac_mad_h="yes"], [ac_mad_h="no"])
   if test "x$ac_mad_h" = "xno"; then
      AC_MSG_ERROR([mad.h header file not found.])
   fi
fi

# Check for samplerate headers.
if test "x$ac_libsamplerate" = "xyes"; then
   AC_CHECK_HEADER(samplerate.h, [ac_samplerate_h="yes"], [ac_samplerate_h="no"])
   if test "x$ac_samplerate_h" = "xno"; then
      AC_MSG_ERROR([samplerate.h header file not found.])
   fi
fi

# Check for rubberband headers.
if test "x$ac_librubberband" = "xyes"; then
   AC_CHECK_HEADER(rubberband/RubberBandStretcher.h, [ac_rubberband_h="yes"], [ac_rubberband_h="no"])
   if test "x$ac_rubberband_h" = "xno"; then
      AC_MSG_ERROR([RubberBandStretcher.h header file not found.])
   fi
fi

# Check for lo headers.
if test "x$ac_liblo" = "xyes"; then
   AC_CHECK_HEADER(lo/lo.h, [ac_lo_h="yes"], [ac_lo_h="no"])
   if test "x$ac_lo_h" = "xno"; then
      AC_MSG_ERROR([lo.h header file not found.])
   fi
fi

# Check for zlib headers.
if test "x$ac_libz" = "xyes"; then
   AC_CHECK_HEADER(zlib.h, [ac_zlib_h="yes"], [ac_zlib_h="no"])
   if test "x$ac_zlib_h" = "xno"; then
      AC_MSG_ERROR([zlib.h header file not found.])
   fi
fi

# Check for slv2 headers.
if test "x$ac_slv2" = "xyes"; then
   ac_slv2_cflags=`$ac_pkg_config --cflags slv2 2>/dev/null`
   if test -n "$ac_slv2_cflags"; then
      CFLAGS="$ac_slv2_cflags $CFLAGS"
      CPPFLAGS="$ac_slv2_cflags $CPPFLAGS"
   fi
   AC_CHECK_HEADER(slv2/slv2.h, [ac_slv2_h="yes"], [ac_slv2_h="no"])
   if test "x$ac_slv2_h" = "xno"; then
      AC_MSG_ERROR([slv2.h header file not found.])
   else
      ac_cflags="$ac_slv2_cflags $ac_cflags"
   fi
fi

# Check for lilv headers.
if test "x$ac_lilv" = "xyes"; then
   ac_lilv_cflags=`$ac_pkg_config --cflags lilv-0 2>/dev/null`
   if test -n "$ac_lilv_cflags"; then
      CFLAGS="$ac_lilv_cflags $CFLAGS"
      CPPFLAGS="$ac_lilv_cflags $CPPFLAGS"
   fi
   AC_CHECK_HEADER(lilv/lilv.h, [ac_lilv_h="yes"], [ac_lilv_h="no"])
   if test "x$ac_lilv_h" = "xno"; then
      AC_MSG_ERROR([lilv.h header file not found.])
   else
      ac_cflags="$ac_lilv_cflags $ac_cflags"
   fi
fi

# Check for suil headers.
if test "x$ac_suil" = "xyes"; then
   ac_suil_cflags=`$ac_pkg_config --cflags suil-0 2>/dev/null`
   if test -n "$ac_suil_cflags"; then
      CFLAGS="$ac_suil_cflags $CFLAGS"
      CPPFLAGS="$ac_suil_cflags $CPPFLAGS"
   fi
   AC_CHECK_HEADER(suil/suil.h, [ac_suil_h="yes"], [ac_suil_h="no"])
   if test "x$ac_suil_h" = "xno"; then
      AC_MSG_ERROR([suil.h header file not found.])
   else
      ac_cflags="$ac_suil_cflags $ac_cflags"
   fi
fi

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
      AC_CHECK_HEADER(aeffectx.h, [ac_vst="yes"], [ac_vst="no"])
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
   else
      CFLAGS="$CFLAGS -Isrc/lv2"
      CPPFLAGS="$CPPFLAGS -Isrc/lv2"
      ac_incpath="$ac_incpath lv2"
   fi
   AC_CHECK_HEADERS(lv2.h lv2_urid.h lv2_uri_map.h, [ac_lv2="yes"], [ac_lv2="no"])
   if test "x$ac_lv2" = "xyes"; then
      AC_DEFINE(CONFIG_LV2, 1, [Define if LV2 headers are available.])
   else
      AC_MSG_WARN([*** LV2 header files not found.])
      AC_MSG_WARN([*** LV2 plug-in support will be disabled.])
      ac_lv2_event="no"
      ac_lv2_atom="no"
      ac_lv2_worker="no"
      ac_lv2_gtk_ui="no"
      ac_lv2_qt4_ui="no"
      ac_lv2_external_ui="no"
      ac_lv2_state="no"
      ac_lv2_state_files="no"
      ac_lv2_programs="no"
      ac_lv2_presets="no"
      ac_lv2_time="no"
   fi
fi
if test "x$ac_lv2_event" = "xyes"; then
   AC_CHECK_HEADERS(lv2_event.h lv2_event_helpers.h,
      [ac_lv2_event="yes"], [ac_lv2_event="no"])
   if test "x$ac_lv2_event" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_EVENT, 1, [Define if LV2 Event/MIDI support is available.])
   else
      AC_MSG_WARN([*** LV2 Event/MIDI support will be disabled.])
   fi
fi
if test "x$ac_lv2_atom" = "xyes"; then
   AC_CHECK_HEADERS(lv2_atom.h lv2_atom_helpers.h,
      [ac_lv2_atom="yes"], [ac_lv2_atom="no"])
   if test "x$ac_lv2_atom" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_ATOM, 1, [Define if LV2 Atom/MIDI aupport is available.])
   else
      AC_MSG_WARN([*** LV2 Atom/MIDI support will be disabled.])
   fi
fi
if test "x$ac_lv2_worker" = "xyes"; then
   AC_CHECK_HEADER(lv2_worker.h, [ac_lv2_worker="yes"], [ac_lv2_worker="no"])
   if test "x$ac_lv2_worker" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_WORKER, 1, [Define if LV2 Worker/Schedule aupport is available.])
   else
      AC_MSG_WARN([*** LV2 Worker/Schedule support will be disabled.])
   fi
fi
if test "x$ac_lv2_qt4_ui" = "xyes"; then
   AC_DEFINE(CONFIG_LV2_QT4_UI, 1, [Define if LV2 QT4 UI support is available.])
fi
if test "x$ac_lv2_gtk_ui" = "xyes" -a "x$ac_suil" = "xno"; then
   ac_gtk_cflags=`$ac_pkg_config --cflags gtk+-2.0 2>/dev/null`
   ac_gtk_libs=`$ac_pkg_config --libs gtk+-2.0 2>/dev/null`
   if test -n "$ac_gtk_cflags" -a -n "$ac_gtk_libs"; then
      ac_cflags="$ac_cflags $ac_gtk_cflags"
      ac_libs="$ac_libs $ac_gtk_libs"
   else
      AC_MSG_WARN([*** LV2 GTK UI support will be disabled.])
      ac_lv2_gtk_ui="no"
   fi
fi
if test "x$ac_lv2_gtk_ui" = "xyes"; then
   AC_DEFINE(CONFIG_LV2_GTK_UI, 1, [Define if LV2 GTK UI support is available.])
fi
if test "x$ac_lv2_external_ui" = "xyes"; then
   AC_CHECK_HEADERS(lv2_ui.h lv2_external_ui.h lv2_data_access.h lv2_instance_access.h,
      [ac_lv2_external_ui="yes"],
      [ac_lv2_external_ui="no"],
      [[#include <stdint.h>
        #include <lv2.h>
        #ifdef HAVE_LV2_UI_H
        # include <lv2_ui.h>
        #endif
      ]])
   if test "x$ac_lv2_external_ui" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_EXTERNAL_UI, 1, [Define if LV2 External UI extension is available.])
   else
      AC_MSG_WARN([*** LV2 External UI extension will be disabled.])
   fi
fi
if test "x$ac_lv2_state" = "xyes"; then
   AC_CHECK_HEADERS(lv2_atom.h lv2_state.h, [ac_lv2_state="yes"], [ac_lv2_state="no"],
      [[#include <stdlib.h>
        #include <lv2.h>
      ]])
   if test "x$ac_lv2_state" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_STATE, 1, [Define if LV2 State extension is available.])
   else
      AC_MSG_WARN([*** LV2 State extension will be disabled.])
      ac_lv2_state_files="no"
      ac_lv2_presets="no"
   fi
else
   ac_lv2_state_files="no"
   ac_lv2_presets="no"
fi
if test "x$ac_lv2_state_files" = "xyes"; then
    AC_DEFINE(CONFIG_LV2_STATE_FILES, 1, [Define if LV2 State Files feature is available.])
else
    AC_MSG_WARN([*** LV2 State Files feature will be disabled.])
fi
if test "x$ac_lv2_programs" = "xyes"; then
   AC_CHECK_HEADERS(lv2_ui.h lv2_programs.h,
      [ac_lv2_programs="yes"],
      [ac_lv2_programs="no"],
      [[#include <stdint.h>
        #include <lv2.h>
        #ifdef HAVE_LV2_UI_H
        # include <lv2_ui.h>
        #endif
      ]])
   if test "x$ac_lv2_programs" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_PROGRAMS, 1, [Define if LV2 Programs extension is available.])
   else
      AC_MSG_WARN([*** LV2 Programs extension will be disabled.])
   fi
fi
if test "x$ac_lv2_presets" = "xyes"; then
   AC_CHECK_HEADERS(lv2_presets.h, [ac_lv2_presets="yes"], [ac_lv2_presets="no"])
   if test "x$ac_lv2_presets" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_PRESETS, 1, [Define if LV2 Presets are supported.])
   else
      AC_MSG_WARN([*** LV2 Presets will be disabled.])
   fi
fi
if test "x$ac_lv2_time" = "xyes"; then
   AC_CHECK_HEADER(lv2_time.h, [ac_lv2_time="yes"], [ac_lv2_time="no"])
   if test "x$ac_lv2_time" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_TIME, 1, [Define if LV2 Time/position is supported.])
   else
      AC_MSG_WARN([*** LV2 Time support will be disabled.])
   fi
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

# Check for X11 unique/single instance.
if test "x$ac_xunique" = "xyes"; then
   AC_DEFINE(CONFIG_XUNIQUE, 1, [Define if X11 unique/single instance is enabled.])
fi

# Check for gradient eye-candy.
if test "x$ac_gradient" = "xyes"; then
   AC_DEFINE(CONFIG_GRADIENT, 1, [Define if gradient eye-candy is enabled.])
fi

# Check for debugging stack-trace.
if test "x$ac_stacktrace" = "xyes"; then
   AC_DEFINE(CONFIG_STACKTRACE, 1, [Define if debugger stack-trace is enabled.])
fi

AC_SUBST(ac_incpath)
AC_SUBST(ac_cflags)


# Checks for typedefs, structures, and compiler characteristics.
# AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS(system)

# Finally produce a configure header file and the makefiles.
AC_OUTPUT

# make clean > /dev/null 2>&1

# Output summary message

echo
echo "  $PACKAGE_NAME $PACKAGE_VERSION"
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
echo "  OSC service support (liblo)  . . . . . . . . . . .: $ac_liblo"
echo "  Archive/Zip file support (zlib)  . . . . . . . . .: $ac_libz"
echo "  IEEE 32bit float optimizations . . . . . . . . . .: $ac_float32"
echo "  SSE optimization support (x86) . . . . . . . . . .: $ac_sse"
echo "  LADSPA Plug-in support . . . . . . . . . . . . . .: $ac_ladspa"
echo "  DSSI Plug-in support . . . . . . . . . . . . . . .: $ac_dssi"
echo "  VST Plug-in support  . . . . . . . . . . . . . . .: $ac_vst"
echo "  LV2 Plug-in support  . . . . . . . . . . . . . . .: $ac_lv2"
echo "  LV2 Plug-in support (libslv2) (DEPRECATED) . . . .: $ac_slv2"
echo "  LV2 Plug-in support (liblilv)  . . . . . . . . . .: $ac_lilv"
echo "  LV2 Plug-in UI instantiation (libsuil) . . . . . .: $ac_suil"
echo "  LV2 Plug-in QT4 UI support . . . . . . . . . . . .: $ac_lv2_qt4_ui"
echo "  LV2 Plug-in GTK UI support . . . . . . . . . . . .: $ac_lv2_gtk_ui"
echo "  LV2 Plug-in External UI support  . . . . . . . . .: $ac_lv2_external_ui"
echo "  LV2 Plug-in MIDI/Event support . . . . . . . . . .: $ac_lv2_event"
echo "  LV2 Plug-in MIDI/Atom support  . . . . . . . . . .: $ac_lv2_atom"
echo "  LV2 Plug-in Worker/Schedule support  . . . . . . .: $ac_lv2_worker"
echo "  LV2 Plug-in State support  . . . . . . . . . . . .: $ac_lv2_state"
echo "  LV2 Plug-in State Files support (EXPERIMENTAL) . .: $ac_lv2_state_files"
echo "  LV2 Plug-in Programs support . . . . . . . . . . .: $ac_lv2_programs"
echo "  LV2 Plug-in Presets support  . . . . . . . . . . .: $ac_lv2_presets"
echo "  LV2 Plug-in Time support . . . . . . . . . . . . .: $ac_lv2_time"
echo
echo "  JACK Session support . . . . . . . . . . . . . . .: $ac_jack_session"
echo "  JACK Latency support . . . . . . . . . . . . . . .: $ac_jack_latency"
echo
echo "  X11 Unique/Single instance . . . . . . . . . . . .: $ac_xunique"
echo "  VeSTige header support . . . . . . . . . . . . . .: $ac_vestige"
echo "  Gradient eye-candy . . . . . . . . . . . . . . . .: $ac_gradient"
echo "  Debugger stack-trace (gdb) . . . . . . . . . . . .: $ac_stacktrace"
echo
echo "  Install prefix . . . . . . . . . . . . . . . . . .: $ac_prefix"
echo
echo "Now type 'make', followed by 'make install' as root."
echo
