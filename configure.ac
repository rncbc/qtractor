# Process this file with autoconf to produce a configure script.
AC_INIT(Qtractor, 0.0.1.516, rncbc@rncbc.org, qtractor)

AC_CONFIG_SRCDIR(src/qtractorMainForm.ui)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES(Makefile qtractor.pro qtractor.spec)

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
  [ac_debug="debug"],
  [ac_debug="release"])
AC_SUBST(ac_debug)

if test "x$ac_debug" = "xdebug"; then
   AC_DEFINE(CONFIG_DEBUG, 1, [Define if debugging is enabled.])
fi

# Enable libvorbis(file) availability.
AC_ARG_ENABLE(libvorbis,
  AC_HELP_STRING([--disable-libvorbis], [disable libvorbis interface (default=no)]),
  [ac_libvorbis="no"],
  [ac_libvorbis="yes"])

# Enable libmad availability.
AC_ARG_ENABLE(libmad,
  AC_HELP_STRING([--disable-libmad], [disable libmad interface (default=no)]),
  [ac_libmad="no"],
  [ac_libmad="yes"])

# Enable libsamplerate availability.
AC_ARG_ENABLE(libsamplerate,
  AC_HELP_STRING([--disable-libsamplerate], [disable libsamplerate interface (default=no)]),
  [ac_libsamplerate="no"],
  [ac_libsamplerate="yes"])

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
for X in $ac_with_paths; do
  if test -d $X/include; then
    CFLAGS="$CFLAGS -I$X/include"
    CPPFLAGS="$CPPFLAGS -I$X/include"
    ac_incpath="$ac_incpath $X/include"
  fi
  if test -d $X/lib64; then
    LIBS="$LIBS -L$X/lib64"
    ac_libs="$ac_libs -L$X/lib64"
  fi
  if test -d $X/lib; then
    LIBS="$LIBS -L$X/lib"
    ac_libs="$ac_libs -L$X/lib"
  fi
done

AC_CACHE_CHECK([for Qt library version >= 4.1],
  ac_qtlib_version, [
  AC_TRY_LINK([#include "Qt/qglobal.h"], [
#if QT_VERSION < 0x040100
#error Qt library 4.1 or greater required.
#endif
    ],
    ac_qtlib_version="yes", [
      echo "no; Qt 4.1 or greater is required"
      exit
    ])
])

# A common error message:
ac_qtdir_errmsg="not found in current PATH. Maybe QT development environment isn't available (qt-devel)."

# Check for Qt qmake utility.
AC_PATH_PROG(ac_qmake, qmake, [no], ${PATH})
if test "x$ac_qmake" = "xno"; then
   AC_MSG_ERROR([qmake $ac_qtdir_errmsg])
fi
AC_SUBST(ac_qmake)

# Check for Qt moc utility.
AC_PATH_PROG(ac_moc, moc, [no], ${PATH})
if test "x$ac_moc" = "xno"; then
   AC_MSG_ERROR([moc $ac_qtdir_errmsg])
fi
AC_SUBST(ac_moc)

# Check for Qt uic utility.
AC_PATH_PROG(ac_uic, uic, [no], ${PATH})
if test "x$ac_uic" = "xno"; then
   AC_MSG_ERROR([uic $ac_qtdir_errmsg])
fi
AC_SUBST(ac_uic)

# Checks for libraries.
AC_CHECK_LIB(m, main)
AC_CHECK_LIB(X11, main)
AC_CHECK_LIB(Xext, main)
AC_CHECK_LIB($ac_qtlib, main)

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
AC_CHECK_LIB(sndfile, main, [ac_sndfile_lib="yes"], [ac_sndfile_lib="no"])
if test "x$ac_sndfile_lib" = "xno"; then
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

AC_SUBST(ac_libs)
AC_SUBST(ac_incpath)

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

# Check for ladspa headers.
AC_CHECK_HEADER(ladspa.h, [ac_ladspa_h="yes"], [ac_ladspa_h="no"])
if test "x$ac_ladspa_h" = "xno"; then
	AC_MSG_ERROR([ladspa.h header file not found.])
fi

# Checks for typedefs, structures, and compiler characteristics.
# AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS(system)

# Finally produce a configure header file and the makefiles.
AC_OUTPUT

# make clean > /dev/null 2>&1

echo ""
echo "Now type 'make', followed by 'make install' as root."
echo ""

