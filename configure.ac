# Process this file with autoconf to produce a configure script.
AC_INIT(Qtractor, 0.4.6.15, rncbc@rncbc.org, qtractor)

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
  AC_HELP_STRING([--enable-vestige], [enable VeSTige header support (default=no)]),
  [ac_vestige="$enableval"],
  [ac_vestige="no"])

# Enable LV2 support.
AC_ARG_ENABLE(lv2,
  AC_HELP_STRING([--enable-lv2], [enable LV2 plug-in support (default=yes)]),
  [ac_lv2="$enableval"],
  [ac_lv2="yes"])

AC_ARG_ENABLE(lv2-event,
  AC_HELP_STRING([--enable-lv2-event], [enable LV2 plug-in event/MIDI support (default=yes)]),
  [ac_lv2_event="$enableval"],
  [ac_lv2_event="yes"])

AC_ARG_ENABLE(lv2-external-ui,
  AC_HELP_STRING([--enable-lv2-external-ui], [enable LV2 plug-in external UI support (default=yes)]),
  [ac_lv2_external_ui="$enableval"],
  [ac_lv2_external_ui="yes"])

AC_ARG_ENABLE(lv2-saverestore,
  AC_HELP_STRING([--enable-lv2-saverestore], [enable LV2 plug-in save/restore support (default=yes)]),
  [ac_lv2_saverestore="$enableval"],
  [ac_lv2_saverestore="yes"])

# Enable JACK session support.
AC_ARG_ENABLE(jack-session,
  AC_HELP_STRING([--jack-session], [enable JACK session support (default=yes)]),
  [ac_jack_session="$enableval"],
  [ac_jack_session="yes"])

# Enable X11 unique/single instance.
AC_ARG_ENABLE(xunique,
  AC_HELP_STRING([--enable-xunique], [enable X11 unique/single instance (default=no)]),
  [ac_xunique="$enableval"],
  [ac_xunique="no"])

# Enable XInitThreads() support.
AC_ARG_ENABLE(xinitthreads,
  AC_HELP_STRING([--enable-xinitthreads], [enable XInitThreads() support (default=no)]),
  [ac_xinitthreads="$enableval"],
  [ac_xinitthreads="no"])

# Enable gradient eye-candy.
AC_ARG_ENABLE(gradient,
  AC_HELP_STRING([--enable-gradient], [enable gradient eye-candy (default=yes)]),
  [ac_gradient="$enableval"],
  [ac_gradient="yes"])

# Enable debugger stack-trace option (assumes --enable-debug).
AC_ARG_ENABLE(stacktrace,
  AC_HELP_STRING([--enable-stacktrace], [enable debugger stack-trace (default=no)]),
  [ac_stacktrace="$enableval"])


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

# Set for alternate libslv2 installation dir.
AC_ARG_WITH(libslv2,
  AC_HELP_STRING([--with-libslv2=PATH], [use alternate libslv2 path]),
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
AC_CACHE_CHECK([for Qt library version >= 4.1],
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

AC_SUBST(ac_cflags)


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

# Check for optional libslv2 library.
if test "x$ac_lv2" = "xyes"; then
   AC_CHECK_LIB(slv2, main, [ac_lv2="yes"], [ac_lv2="no"])
fi
if test "x$ac_lv2" = "xyes"; then
   AC_DEFINE(CONFIG_LIBSLV2, 1, [Define if libslv2 is available.])
   ac_libs="$ac_libs -lslv2"
else
   AC_MSG_WARN([*** slv2 library not found.])
   AC_MSG_WARN([*** LV2 plugin support will be disabled.])
   ac_lv2_event="no"
   ac_lv2_external_ui="no"
   ac_lv2_saverestore="no"
fi

# Check for JACK session event callback availability.
if test "x$ac_jack_session" = "xyes"; then
   AC_CHECK_LIB(jack, jack_set_session_callback, [ac_jack_session="yes"], [ac_jack_session="no"])
fi

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
else
   if test "x$ac_vestige" = "xyes"; then
	  CFLAGS="$CFLAGS -Isrc/vestige"
	  CPPFLAGS="$CPPFLAGS -Isrc/vestige"
	  ac_incpath="$ac_incpath vestige"
   fi
fi
if test "x$ac_vst" = "xyes"; then
   CFLAGS="-D__cdecl= $CFLAGS"
   CPPFLAGS="-D__cdecl= $CPPFLAGS"
   AC_CHECK_HEADER(aeffectx.h, [ac_vst="yes"], [ac_vst="no"])
   if test "x$ac_vst" = "xyes"; then
      AC_DEFINE(CONFIG_VST, 1, [Define if VST header is available.])
   else
      AC_MSG_WARN([*** aeffectx.h header file not found.])
      AC_MSG_WARN([*** VST plug-in support will be disabled.])
      ac_xinitthreads="no"
	  ac_vestige="no"
   fi
else
   ac_xinitthreads="no"
   ac_vestige="no"
fi

# Post-check for VeSTige header.
if test "x$ac_vestige" = "xyes"; then
   AC_DEFINE(CONFIG_VESTIGE, 1, [Define if VeSTige header is available.])
fi

# Check for LV2 headers.
if test -n "$ac_with_lv2"; then
   CFLAGS="-I$ac_with_lv2 $CFLAGS"
   CPPFLAGS="-I$ac_with_lv2 $CPPFLAGS"
   ac_lv2="yes"
fi
if test "x$ac_lv2" = "xyes"; then
   # Check for LV2 related headers (eg. slv2, librdf).
   for X in $ac_with_paths; do
      if test -d $X/include; then
         for Y in slv2 redland rdf; do
            if test -d $X/include/$Y; then
               CFLAGS="-I$X/include/$Y $CFLAGS"
               CPPFLAGS="-I$X/include/$Y $CPPFLAGS"
               ac_incpath="$X/include/$Y $ac_incpath"
            fi
         done
      fi
   done
   # Check for proper headers.
   AC_CHECK_HEADERS(lv2.h slv2.h, [ac_lv2="yes"], [ac_lv2="no"])
fi
if test "x$ac_lv2" = "xyes"; then
   AC_DEFINE(CONFIG_LV2, 1, [Define if LV2 headers are available.])
   if test -n "$ac_with_lv2"; then
      ac_incpath="$ac_with_lv2 $ac_incpath"
   else
      CFLAGS="$CFLAGS -Isrc/lv2"
      CPPFLAGS="$CPPFLAGS -Isrc/lv2"
      ac_incpath="$ac_incpath lv2"
   fi
else
   AC_MSG_WARN([*** LV2 header files not found.])
   AC_MSG_WARN([*** LV2 plug-in support will be disabled.])
   ac_lv2_event="no"
   ac_lv2_external_ui="no"
   ac_lv2_saverestore="no"
fi
if test "x$ac_lv2_event" = "xyes"; then
   AC_CHECK_HEADERS(lv2_event.h lv2_event_helpers.h lv2_uri_map.h,
      [ac_lv2_event="yes"], [ac_lv2_event="no"])
   if test "x$ac_lv2_event" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_EVENT, 1, [Define if LV2 event/MIDI extension is available.])
   else
      AC_MSG_WARN([*** LV2 event extension will be disabled.])
   fi
fi
if test "x$ac_lv2_external_ui" = "xyes"; then
   AC_CHECK_HEADERS(lv2_ui.h lv2_external_ui.h lv2_data_access.h lv2_instance_access.h,
      [ac_lv2_external_ui="yes"],
      [ac_lv2_external_ui="no"],
      [[#ifdef HAVE_LV2_UI_H
        # include <lv2_ui.h>
        #endif
      ]])
   if test "x$ac_lv2_external_ui" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_EXTERNAL_UI, 1, [Define if LV2 external UI extension is available.])
   else
      AC_MSG_WARN([*** LV2 external UI extension will be disabled.])
   fi
fi
if test "x$ac_lv2_saverestore" = "xyes"; then
   AC_CHECK_HEADER(lv2_saverestore.h, [ac_lv2_saverestore="yes"], [ac_lv2_saverestore="no"])
   if test "x$ac_lv2_saverestore" = "xyes"; then
      AC_DEFINE(CONFIG_LV2_SAVERESTORE, 1, [Define if LV2 save/restore extension is available.])
   else
      AC_MSG_WARN([*** LV2 save/restore extension will be disabled.])
   fi
fi

# Check for JACK session headers availability.
if test "x$ac_jack_session" = "xyes"; then
   AC_CHECK_HEADER(jack/session.h, [ac_jack_session="yes"], [ac_jack_session="no"])
   if test "x$ac_jack_session" = "xyes"; then
      AC_DEFINE(CONFIG_JACK_SESSION, 1, [Define if JACK session support is available.])
   else
      AC_MSG_WARN([*** JACK session support will be disabled.])
   fi
fi

# Check for X11 unique/single instance.
if test "x$ac_xunique" = "xyes"; then
   AC_DEFINE(CONFIG_XUNIQUE, 1, [Define if X11 unique/single instance is enabled.])
fi

# Check for XInitThreads() support.
if test "x$ac_xinitthreads" = "xyes"; then
   AC_DEFINE(CONFIG_XINITTHREADS, 1, [Define if XInitThreads() is supported.])
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
echo "  IEEE 32bit float optimizations . . . . . . . . . .: $ac_float32"
echo "  SSE optimization support (x86) . . . . . . . . . .: $ac_sse"
echo "  LADSPA Plug-in support . . . . . . . . . . . . . .: $ac_ladspa"
echo "  DSSI Plug-in support . . . . . . . . . . . . . . .: $ac_dssi"
echo "  VST Plug-in support  . . . . . . . . . . . . . . .: $ac_vst"
echo "  LV2 Plug-in support (libslv2)  . . . . . . . . . .: $ac_lv2"
echo "  LV2 Plug-in Event/MIDI support . . . . . . . . . .: $ac_lv2_event"
echo "  LV2 Plug-in External UI support  . . . . . . . . .: $ac_lv2_external_ui"
echo "  LV2 Plug-in Save/Restore support . . . . . . . . .: $ac_lv2_saverestore"
echo
echo "  JACK Session support (EXPERIMENTAL)  . . . . . . .: $ac_jack_session"
echo
echo "  X11 Unique/Single instance . . . . . . . . . . . .: $ac_xunique"
echo "  XInitThreads() support (DANGEROUS) . . . . . . . .: $ac_xinitthreads"
echo "  Gradient eye-candy . . . . . . . . . . . . . . . .: $ac_gradient"
echo "  VeSTige header support . . . . . . . . . . . . . .: $ac_vestige"
echo "  Debugger stack-trace (gdb) . . . . . . . . . . . .: $ac_stacktrace"
echo
echo "  Install prefix . . . . . . . . . . . . . . . . . .: $ac_prefix"
echo
echo "Now type 'make', followed by 'make install' as root."
echo
