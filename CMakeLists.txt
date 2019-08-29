project(QTRACTOR)

cmake_minimum_required(VERSION 3.1)

set (VERSION "0.9.10")

set (CONFIG_VERSION ${VERSION})
execute_process (
  COMMAND git describe --tags --dirty --abbrev=6
  OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
  RESULT_VARIABLE GIT_DESCRIBE_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if (GIT_DESCRIBE_RESULT EQUAL 0)
  set (VERSION "${GIT_DESCRIBE_OUTPUT}")
  string (REGEX REPLACE "^[^_]+"   "" VERSION "${VERSION}")
  string (REGEX REPLACE "^[_vV]+"  "" VERSION "${VERSION}")
  string (REGEX REPLACE "-g"   "git." VERSION "${VERSION}")
  string (REGEX REPLACE "[_|-]+"  "." VERSION "${VERSION}")
  execute_process (
    COMMAND git rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE GIT_REVPARSE_OUTPUT
    RESULT_VARIABLE GIT_REVPARSE_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (GIT_REVPARSE_RESULT EQUAL 0 AND NOT GIT_REVPARSE_OUTPUT STREQUAL "master")
    set (VERSION "${VERSION} [${GIT_REVPARSE_OUTPUT}]")
  endif ()
endif ()

set (PACKAGE_NAME "Qtractor")
set (PACKAGE_VERSION "${VERSION}")
set (PACKAGE_BUGREPORT "rncbc@rncbc.org")
set (PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set (PACKAGE_TARNAME "qtractor")

set (CONFIG_BUILD_VERSION "${PACKAGE_VERSION}")

if (CMAKE_BUILD_TYPE)
  set (CONFIG_BUILD_TYPE ${CMAKE_BUILD_TYPE})
else ()
  set (CONFIG_BUILD_TYPE "release")
endif ()

set (CONFIG_DEBUG 0)
if (CONFIG_BUILD_TYPE MATCHES "debug")
  set (CONFIG_DEBUG 1)
endif ()

set (CONFIG_PREFIX "${CMAKE_INSTALL_PREFIX}")

include (GNUInstallDirs)
set (CONFIG_BINDIR  "${CONFIG_PREFIX}/${CMAKE_INSTALL_BINDIR}")
set (CONFIG_LIBDIR  "${CONFIG_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set (CONFIG_DATADIR "${CONFIG_PREFIX}/${CMAKE_INSTALL_DATADIR}")
set (CONFIG_MANDIR  "${CONFIG_PREFIX}/${CMAKE_INSTALL_MANDIR}")


# Enable libvorbis(file) availability.
option (CONFIG_LIBVORBIS "Enable libvorbis interface (default=yes)" 1)

# Enable libmad availability.
option (CONFIG_LIBMAD "Enable libmad interface (default=yes)" 1)

# Enable libsamplerate availability.
option (CONFIG_LIBSAMPLERATE "Enable libsamplerate interface (default=yes)" 1)

# Enable librubberband availability.
option (CONFIG_LIBRUBBERBAND "Enable librubberband interface (default=yes)" 1)

# Enable libaudio support.
option (CONFIG_LIBAUBIO "Enable libaubio interface support (default=yes)" 1)

# Enable liblo availability.
option (CONFIG_LIBLO "Enable liblo interface (default=yes)" 1)

# Enable libz availability.
option (CONFIG_LIBZ "Enable libz interface (default=yes)" 1)

# Enable LILV support.
option (CONFIG_LIBLILV "Enable LILV interface support (default=yes)" 1)

# Enable SUIL support.
option (CONFIG_LIBSUIL "Enable SUIL interface support (default=yes)" 1)

# Enable SSE optimization.
option (CONFIG_SSE "Enable SSE optimization (default=yes)" 1)

# Enable LADSPA support.
option (CONFIG_LADSPA "Enable LADSPA plug-in support (default=yes)" 1)

# Enable DSSI support.
option (CONFIG_DSSI "Enable DSSI plug-in support (default=yes)" 1)

# Enable VST support.
option (CONFIG_VST "Enable VST plug-in support (default=yes)" 1)

option (CONFIG_VESTIGE "Enable VeSTige header support (default=yes)" 1)

# Enable LV2 support.
option (CONFIG_LV2 "Enable LV2 plug-in support (default=yes)" 1)

option (CONFIG_LV2_EVENT "Enable LV2 plug-in MIDI/Event support (default=no)" 0)

option (CONFIG_LV2_ATOM "Enable LV2 plug-in MIDI/Atom support (default=yes)" 1)

option (CONFIG_LV2_WORKER "Enable LV2 plug-in Worker/schedule support (default=yes)" 1)

option (CONFIG_LV2_UI "Enable LV2 plug-in UI support (default=yes)" 1)

option (CONFIG_LV2_EXTERNAL_UI "Enable LV2 plug-in External UI support (default=yes)" 1)

option (CONFIG_LV2_STATE "Enable LV2 plug-in State support (default=yes)" 1)

option (CONFIG_LV2_STATE_FILES "Enable LV2 plug-in State Files support (default=yes)" 1)

option (CONFIG_LV2_STATE_MAKE_PATH "Enable LV2 plug-in State Make Path support (default=no)" 0)

option (CONFIG_LV2_PROGRAMS "Enable LV2 plug-in Programs support (default=yes)" 1)

option (CONFIG_LV2_PRESETS "Enable LV2 plug-in Presets support (default=yes)" 1)

option (CONFIG_LV2_PATCH "Enable LV2 plug-in Patch support (default=yes)" 1)

option (CONFIG_LV2_TIME "Enable LV2 plug-in Time support (default=yes)" 1)

option (CONFIG_LV2_TIME_POSITION "Enable LV2 plug-in Time/position support (default=yes)" 1)

option (CONFIG_LV2_OPTIONS "Enable LV2 plug-in Options support (default=yes)" 1)

option (CONFIG_LV2_BUF_SIZE "Enable LV2 plug-in Buf-size support (default=yes)" 1)

option (CONFIG_LV2_UI_TOUCH "Enable LV2 plug-in UI Touch interface support (default=yes)" 1)

option (CONFIG_LV2_UI_IDLE "Enable LV2 plug-in UI Idle interface support (default=yes)" 1)

option (CONFIG_LV2_UI_SHOW "Enable LV2 plug-in UI Show interface support (default=yes)" 1)

option (CONFIG_LV2_UI_GTK2 "Enable LV2 plug-in UI GTK2 native support (default=yes)" 1)

option (CONFIG_LV2_UI_X11 "Enable LV2 plug-in UI X11 native support (default=yes)" 1)

# Enable JACK session support.
option (CONFIG_JACK_SESSION "Enable JACK session support (default=yes)" 1)

# Enable JACK latency support.
option (CONFIG_JACK_LATENCY "Enable JACK latency support (default=yes)" 1)

# Enable JACK metadata support.
option (CONFIG_JACK_METADATA "Enable JACK metadata support (default=yes)" 1)

# Enable NSM support.
option (CONFIG_NSM "Enable NSM support (default=yes)" 1)

# Enable unique/single instance.
option (CONFIG_XUNIQUE "Enable unique/single instance (default=no)" 0)

# Enable gradient eye_candy.
option (CONFIG_GRADIENT "Enable gradient eye-candy (default=yes)" 1)

# Enable debugger stack_trace option (assumes --enable-debug).
option (CONFIG_STACKTRACE "Enable debugger stack-trace (default=no)" 0)


# Fix for new CMAKE_REQUIRED_LIBRARIES policy.
if (POLICY CMP0075)
  cmake_policy (SET CMP0075 NEW)
endif ()

# Check for Qt
find_package (Qt5 REQUIRED COMPONENTS Core Gui Widgets Xml)

if (CONFIG_XUNIQUE)
  find_package (Qt5 REQUIRED COMPONENTS Network)
endif ()

find_package (Qt5LinguistTools)

include (CheckIncludeFile)
include (CheckIncludeFiles)
include (CheckIncludeFileCXX)
include (CheckFunctionExists)
include (CheckLibraryExists)

# Checks for libraries.
if (WIN32)
  check_function_exists (lroundf CONFIG_ROUND)
else ()
  find_library (MATH_LIBRARY m)
  # Check for round math function.
  if (MATH_LIBRARY)
    set (CMAKE_REQUIRED_LIBRARIES "${MATH_LIBRARY};${CMAKE_REQUIRED_LIBRARIES}")
    check_function_exists (lroundf CONFIG_ROUND)
  else ()
    message (FATAL_ERROR "*** math library not found.")
  endif ()
endif ()

# Check for IEEE 32bit float optimizations.
set (CONFIG_FLOAT32 1)

# Check for SSE optimization.
if (CONFIG_SSE)
   add_compile_options (-msse -mfpmath=sse -ffast-math)
endif ()

# Checks for header files.
if (UNIX AND NOT APPLE)
  check_include_files ("fcntl.h;unistd.h;signal.h" HAVE_SIGNAL_H)
endif ()


# Check for LADSPA headers.
if (CONFIG_LADSPA)
  check_include_file (ladspa.h CONFIG_LADSPA)
endif ()

# Check for DSSI headers.
if (CONFIG_DSSI)
  check_include_file (dssi.h CONFIG_DSSI)
endif ()

# Check for VST headers.
if (CONFIG_VST)
  check_include_file (aeffectx.h CONFIG_VST)
  if (NOT CONFIG_VST AND CONFIG_VESTIGE)
     include_directories (${CMAKE_CURRENT_SOURCE_DIR}/src/vestige)
     check_include_file (vestige.h CONFIG_VESTIGE)
     set (CONFIG_VST ${CONFIG_VESTIGE})
  else ()
     set (CONFIG_VESTIGE 0)
  endif ()
endif ()

# Check for LV2 headers.
if (CONFIG_LV2)
  include_directories (${CMAKE_CURRENT_SOURCE_DIR}/src/lv2)
endif ()

# Find package modules
find_package (PkgConfig REQUIRED)

# Check for JACK libraries.
pkg_check_modules (JACK REQUIRED jack>=0.100.0)
set (CONFIG_LIBJACK ${JACK_FOUND})
if (CONFIG_LIBJACK)
  include_directories (${JACK_INCLUDE_DIRS})
  link_directories (${JACK_LIBRARY_DIRS})
# link_libraries (${JACK_LIBRARIES})
  set (CMAKE_REQUIRED_LIBRARIES "${JACK_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  # Check for JACK session event callback availability.
  if (CONFIG_JACK_SESSION)
    check_function_exists (jack_set_session_callback CONFIG_JACK_SESSION)
  endif ()
  # Check for (new) JACK latency support availability.
  if (CONFIG_JACK_LATENCY)
    check_function_exists (jack_port_get_latency_range CONFIG_JACK_LATENCY)
  endif ()
  # Check for JACK metadata support availability.
  if (CONFIG_JACK_METADATA)
    check_function_exists (jack_get_property CONFIG_JACK_METADATA)
  endif ()
  # Check for jack_set_port_rename_callback.
  check_function_exists (jack_set_port_rename_callback CONFIG_JACK_PORT_RENAME)
else ()
  message (FATAL_ERROR "*** JACK library not found.")
endif ()

# Check for ALSA libraries.
pkg_check_modules (ALSA REQUIRED alsa)
set (CONFIG_LIBASOUND ${ALSA_FOUND})
if (CONFIG_LIBASOUND)
  include_directories (${ALSA_INCLUDE_DIRS})
  link_directories (${ALSA_LIBRARY_DIRS})
  link_libraries (${ALSA_LIBRARIES})
# set (CMAKE_REQUIRED_LIBRARIES "${ALSA_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
else ()
  message (FATAL_ERROR "*** ALSA library not found.")
endif ()

# Check for SNDFILE libraries.
pkg_check_modules (SNDFILE REQUIRED sndfile)
set (CONFIG_LIBSNDFILE ${SNDFILE_FOUND})
if (CONFIG_LIBSNDFILE)
  include_directories (${SNDFILE_INCLUDE_DIRS})
  link_directories (${SNDFILE_LIBRARY_DIRS})
  link_libraries (${SNDFILE_LIBRARIES})
# set (CMAKE_REQUIRED_LIBRARIES "${SNDFILE_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
else ()
  message (FATAL_ERROR "*** SNDFILE library not found.")
endif ()

# Check for VORBIS libraries.
if (CONFIG_LIBVORBIS)
  pkg_check_modules (VORBIS vorbis)
  set (CONFIG_LIBVORBIS ${VORBIS_FOUND})
  if (CONFIG_LIBVORBIS)
    include_directories (${VORBIS_INCLUDE_DIRS})
    link_directories (${VORBIS_LIBRARY_DIRS})
    link_libraries (${VORBIS_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${VORBIS_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  else ()
    message (WARNING "*** VORBIS library not found.")
  endif ()
endif ()

if (CONFIG_LIBVORBIS)
  pkg_check_modules (VORBISENC REQUIRED vorbisenc)
  if (VORBISENC_FOUND)
    include_directories (${VORBISENC_INCLUDE_DIRS})
    link_directories (${VORBISENC_LIBRARY_DIRS})
    link_libraries (${VORBISENC_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${VORBISENC_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  endif ()
endif ()

if (CONFIG_LIBVORBIS)
  pkg_check_modules (VORBISFILE REQUIRED vorbisfile)
  if (VORBISFILE_FOUND)
    include_directories (${VORBISFILE_INCLUDE_DIRS})
    link_directories (${VORBISFILE_LIBRARY_DIRS})
    link_libraries (${VORBISFILE_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${VORBISFILE_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  endif ()
endif ()

if (CONFIG_LIBVORBIS)
  pkg_check_modules (OGG REQUIRED ogg)
  if (OGG_FOUND)
    include_directories (${OGG_INCLUDE_DIRS})
    link_directories (${OGG_LIBRARY_DIRS})
    link_libraries (${OGG_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${OGG_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  endif ()
endif ()

# Check for MAD libraries.
if (CONFIG_LIBMAD)
  pkg_check_modules (MAD mad)
  set (CONFIG_LIBMAD ${MAD_FOUND})
  if (CONFIG_LIBMAD)
    include_directories (${MAD_INCLUDE_DIRS})
    link_directories (${MAD_LIBRARY_DIRS})
    link_libraries (${MAD_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${MAD_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  else ()
    message (WARNING "*** MAD library not found.")
  endif ()
endif ()

# Check for SAMPLERATE libraries.
if (CONFIG_LIBSAMPLERATE)
  pkg_check_modules (SAMPLERATE samplerate)
  set (CONFIG_LIBSAMPLERATE ${SAMPLERATE_FOUND})
  if (CONFIG_LIBSAMPLERATE)
    include_directories (${SAMPLERATE_INCLUDE_DIRS})
    link_directories (${SAMPLERATE_LIBRARY_DIRS})
    link_libraries (${SAMPLERATE_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${SAMPLERATE_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  else ()
    message (WARNING "*** SAMPLERATE library not found.")
  endif ()
endif ()

# Check for RUBBERBAND libraries.
if (CONFIG_LIBRUBBERBAND)
  pkg_check_modules (RUBBERBAND rubberband)
  set (CONFIG_LIBRUBBERBAND ${RUBBERBAND_FOUND})
  if (CONFIG_LIBRUBBERBAND)
    include_directories (${RUBBERBAND_INCLUDE_DIRS})
    link_directories (${RUBBERBAND_LIBRARY_DIRS})
    link_libraries (${RUBBERBAND_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${RUBBERBAND_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  else ()
    message (WARNING "*** RUBBERBAND library not found.")
  endif ()
endif ()

# Check for AUBIO libraries.
if (CONFIG_LIBAUBIO)
  pkg_check_modules (AUBIO aubio>=0.4.1)
  set (CONFIG_LIBAUBIO ${AUBIO_FOUND})
  if (CONFIG_LIBAUBIO)
    include_directories (${AUBIO_INCLUDE_DIRS})
    link_directories (${AUBIO_LIBRARY_DIRS})
    link_libraries (${AUBIO_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${AUBIO_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  else ()
    message (WARNING "*** AUBIO library not found.")
  endif ()
endif ()

# Check for LIBLO libraries.
if (CONFIG_LIBLO)
  pkg_check_modules (LIBLO liblo)
  set (CONFIG_LIBLO ${LIBLO_FOUND})
  if (CONFIG_LIBLO)
    include_directories (${LIBLO_INCLUDE_DIRS})
    link_directories (${LIBLO_LIBRARY_DIRS})
    link_libraries (${LIBLO_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${LIBLO_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  else ()
    message (WARNING "*** LIBLO library not found.")
  endif ()
endif ()

# Check for ZLIB libraries.
if (CONFIG_LIBZ)
  pkg_check_modules (ZLIB zlib)
  set (CONFIG_LIBZ ${ZLIB_FOUND})
  if (CONFIG_LIBZ)
    include_directories (${ZLIB_INCLUDE_DIRS})
    link_directories (${ZLIB_LIBRARY_DIRS})
    link_libraries (${ZLIB_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${ZLIB_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  else ()
    message (WARNING "*** ZLIB library not found.")
  endif ()
endif ()

# Check for LV2 support.
if (CONFIG_LV2)
  pkg_check_modules (LV2 lv2)
  set (CONFIG_LV2 ${LV2_FOUND})
  if (CONFIG_LV2)
    include_directories (${LV2_INCLUDE_DIRS})
    link_directories (${LV2_LIBRARY_DIRS})
    link_libraries (${LV2_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${LV2_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  endif ()
endif ()

if (NOT CONFIG_LV2)
  set (CONFIG_LIBLILV 0)
  set (CONFIG_LV2_UI 0)
endif ()

# Check for optional LILV library.
if (CONFIG_LIBLILV)
  pkg_check_modules (LILV lilv-0)
  set (CONFIG_LIBLILV ${LILV_FOUND})
  if (CONFIG_LIBLILV)
    include_directories (${LILV_INCLUDE_DIRS})
    link_directories (${LILV_LIBRARY_DIRS})
  # link_libraries (${LILV_LIBRARIES})
    set (CMAKE_REQUIRED_LIBRARIES "${LILV_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
    check_function_exists (lilv_file_uri_parse CONFIG_LILV_FILE_URI_PARSE)
    check_function_exists (lilv_world_unload_resource CONFIG_LILV_WORLD_UNLOAD_RESOURCE)
  else ()
    message (WARNING "*** LILV library not found.")
  endif ()
endif ()

if (CONFIG_LV2_UI)
  check_include_file(lv2/lv2plug.in/ns/extensions/ui/ui.h CONFIG_LV2_UI)
  if (CONFIG_LV2_UI)
    # Check for LV2 external UI instance access.
    if (CONFIG_LV2_EXTERNAL_UI)
      check_include_file (lv2/lv2plug.in/ns/ext/instance-access/instance-access.h CONFIG_LV2_EXTERNAL_UI)
    endif ()
  endif ()
endif ()

if (NOT CONFIG_LV2_UI)
  set (CONFIG_LIBSUIL 0)
  set (CONFIG_LV2_EXTERNAL_UI 0)
  set (CONFIG_LV2_UI_TOUCH 0)
  set (CONFIG_LV2_UI_IDLE 0)
  set (CONFIG_LV2_UI_SHOW 0)
  set (CONFIG_LV2_UI_GTK2 0)
  set (CONFIG_LV2_UI_X11 0)
endif ()

# Check for optional SUIL library.
if (CONFIG_LIBSUIL)
  pkg_check_modules (SUIL suil-0)
  set (CONFIG_LIBSUIL ${LILV_FOUND})
  if (CONFIG_LIBSUIL)
    include_directories (${SUIL_INCLUDE_DIRS})
    link_directories (${SUIL_LIBRARY_DIRS})
  # link_libraries (${SUIL_LIBRARIES})
    set (CMAKE_REQUIRED_LIBRARIES "${SUIL_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
    check_function_exists(suil_instance_get_handle CONFIG_SUIL_INSTANCE_GET_HANDLE)
    set (CONFIG_LIBSUIL_GTK2_IN_QT5 1)
    set (CONFIG_LIBSUIL_X11_IN_QT5 1)
  else ()
    message (WARNING "*** SUIL library not found.")
  endif ()
endif ()

if (CONFIG_LV2_EVENT)
  check_include_file (lv2/lv2plug.in/ns/ext/event/event.h CONFIG_LV2_EVENT)
endif ()

if (CONFIG_LV2_ATOM)
  check_include_file (lv2/lv2plug.in/ns/ext/atom/atom.h CONFIG_LV2_ATOM)
  set (CONFIG_LV2_ATOM_FORGE_OBJECT ${CONFIG_LV2_ATOM})
  set (CONFIG_LV2_ATOM_FORGE_KEY ${CONFIG_LV2_ATOM})
endif ()

if (NOT CONFIG_LV2_ATOM)
  set (CONFIG_LV2_ATOM_FORGE_OBJECT 0)
  set (CONFIG_LV2_ATOM_FORGE_KEY 0)
  set (CONFIG_LV2_STATE 0)
  set (CONFIG_LV2_OPTIONS 0)
  set (CONFIG_LV2_TIME_POSITION 0)
endif ()

if (CONFIG_LV2_WORKER)
  check_include_file (lv2/lv2plug.in/ns/ext/worker/worker.h CONFIG_LV2_WORKER)
endif ()

if (CONFIG_LV2_STATE)
  check_include_file (lv2/lv2plug.in/ns/ext/state/state.h CONFIG_LV2_STATE)
endif ()

if (NOT CONFIG_LV2_STATE)
  set (CONFIG_LV2_STATE_FILES 0)
  set (CONFIG_LV2_STATE_MAKE_PATH 0)
  set (CONFIG_LV2_PRESETS 0)
  set (CONFIG_LV2_PATCH 0)
endif ()

if (CONFIG_LV2_PROGRAMS)
  check_include_file (lv2_programs.h CONFIG_LV2_PROGRAMS)
endif ()

if (CONFIG_LV2_PRESETS)
  check_include_file (lv2/lv2plug.in/ns/ext/presets/presets.h CONFIG_LV2_PRESETS)
endif ()

if (CONFIG_LV2_PATCH)
  check_include_file (lv2/lv2plug.in/ns/ext/patch/patch.h CONFIG_LV2_PATCH)
endif ()

if (CONFIG_LV2_TIME)
  check_include_file (lv2/lv2plug.in/ns/ext/time/time.h CONFIG_LV2_TIME)
endif ()

if (NOT CONFIG_LV2_TIME)
  set (CONFIG_LV2_TIME_POSITION 0)
endif ()

if (CONFIG_LV2_OPTIONS)
  check_include_file (lv2/lv2plug.in/ns/ext/options/options.h CONFIG_LV2_OPTIONS)
endif ()

if (NOT CONFIG_LV2_OPTIONS)
  set (CONFIG_LV2_BUF_SIZE 0)
endif ()

if (CONFIG_LV2_BUF_SIZE)
  check_include_file (lv2/lv2plug.in/ns/ext/buf-size/buf-size.h CONFIG_LV2_BUF_SIZE)
endif ()

if (CONFIG_LV2_UI_GTK2)
  pkg_check_modules (GTK2 gtk+-2.0)
  set (CONFIG_LV2_UI_GTK2 ${GTK2_FOUND})
  if (CONFIG_LV2_UI_GTK2)
    include_directories (${GTK2_INCLUDE_DIRS})
    link_directories (${GTK2_LIBRARY_DIRS})
    link_libraries (${GTK2_LIBRARIES})
  # set (CMAKE_REQUIRED_LIBRARIES "${GTK2_LIBRARIES};${CMAKE_REQUIRED_LIBRARIES}")
  endif ()
endif ()

# Check for VST/X11 support.
if (CONFIG_VST)
  find_package (Qt5 REQUIRED NO_MODULE COMPONENTS X11Extras)
endif ()


add_subdirectory (src)


configure_file (qtractor.spec.in qtractor.spec IMMEDIATE @ONLY)

install (FILES qtractor.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install (FILES qtractor.fr.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/fr/man1 RENAME qtractor.1)

# Configuration status
macro (SHOW_OPTION text value)
  if (${value})
    message ("${text}: yes")
  else ()
    message ("${text}: no")
  endif ()
endmacro ()


message   ("\n  ${PACKAGE_NAME} ${PACKAGE_VERSION}")
message   ("\n  Build target . . . . . . . . . . . . . . . . . . .: ${CONFIG_BUILD_TYPE}\n")
show_option ("  JACK Audio Connection Kit support  . . . . . . . ." CONFIG_LIBJACK)
show_option ("  ALSA MIDI Sequencer support  . . . . . . . . . . ." CONFIG_LIBASOUND)
show_option ("  General audio file support (libsndfile)  . . . . ." CONFIG_LIBSNDFILE)
show_option ("  Ogg Vorbis audio file support (libvorbis)  . . . ." CONFIG_LIBVORBIS)
show_option ("  MPEG-1 Audio Layer 3 file support (libmad) . . . ." CONFIG_LIBMAD)
show_option ("  Sample-rate conversion support (libsamplerate) . ." CONFIG_LIBSAMPLERATE)
show_option ("  Pitch-shifting support (librubberband) . . . . . ." CONFIG_LIBRUBBERBAND)
show_option ("  Beat-detection support (libaubio)  . . . . . . . ." CONFIG_LIBAUBIO)
show_option ("  OSC service support (liblo)  . . . . . . . . . . ." CONFIG_LIBLO)
show_option ("  Archive/Zip file support (zlib)  . . . . . . . . ." CONFIG_LIBZ)
show_option ("  IEEE 32bit float optimizations . . . . . . . . . ." CONFIG_FLOAT32)
show_option ("  SSE optimization support (x86) . . . . . . . . . ." CONFIG_SSE)
show_option ("  LADSPA plug-in support . . . . . . . . . . . . . ." CONFIG_LADSPA)
show_option ("  DSSI plug-in support . . . . . . . . . . . . . . ." CONFIG_DSSI)
show_option ("  VST plug-in support  . . . . . . . . . . . . . . ." CONFIG_VST)
show_option ("  LV2 plug-in support  . . . . . . . . . . . . . . ." CONFIG_LV2)
show_option ("  LV2 plug-in support (liblilv)  . . . . . . . . . ." CONFIG_LILV)
show_option ("  LV2 plug-in UI support . . . . . . . . . . . . . ." CONFIG_LV2_UI)
show_option ("  LV2 plug-in UI support (libsuil) . . . . . . . . ." CONFIG_SUIL)
show_option ("  LV2 plug-in External UI support  . . . . . . . . ." CONFIG_LV2_EXTERNAL_UI)
show_option ("  LV2 plug-in MIDI/Event support (DEPRECATED)  . . ." CONFIG_LV2_EVENT)
show_option ("  LV2 plug-in MIDI/Atom support  . . . . . . . . . ." CONFIG_LV2_ATOM)
show_option ("  LV2 plug-in Worker/Schedule support  . . . . . . ." CONFIG_LV2_WORKER)
show_option ("  LV2 plug-in State support  . . . . . . . . . . . ." CONFIG_LV2_STATE)
show_option ("  LV2 plug-in State Files support  . . . . . . . . ." CONFIG_LV2_STATE_FILES)
show_option ("  LV2 plug-in State Make Path support (DANGEROUS)  ." CONFIG_LV2_STATE_MAKE_PATH)
show_option ("  LV2 plug-in Programs support . . . . . . . . . . ." CONFIG_LV2_PROGRAMS)
show_option ("  LV2 plug-in Presets support  . . . . . . . . . . ." CONFIG_LV2_PRESETS)
show_option ("  LV2 plug-in Patch support  . . . . . . . . . . . ." CONFIG_LV2_PATCH)
show_option ("  LV2 plug-in Time support . . . . . . . . . . . . ." CONFIG_LV2_TIME)
show_option ("  LV2 plug-in Time/position support  . . . . . . . ." CONFIG_LV2_TIME_POSITION)
show_option ("  LV2 plug-in Options support  . . . . . . . . . . ." CONFIG_LV2_OPTIONS)
show_option ("  LV2 plug-in Buf-size support . . . . . . . . . . ." CONFIG_LV2_BUF_SIZE)
show_option ("  LV2 plug-in UI Touch interface support . . . . . ." CONFIG_LV2_UI_TOUCH)
show_option ("  LV2 plug-in UI Idle interface support  . . . . . ." CONFIG_LV2_UI_IDLE)
show_option ("  LV2 plug-in UI Show interface support  . . . . . ." CONFIG_LV2_UI_SHOW)
show_option ("  LV2 plug-in UI GTK2 native support . . . . . . . ." CONFIG_LV2_UI_GTK2)
show_option ("  LV2 plug-in UI X11 native support  . . . . . . . ." CONFIG_LV2_UI_X11)
message     ("")
show_option ("  JACK Session support . . . . . . . . . . . . . . ." CONFIG_JACK_SESSION)
show_option ("  JACK Latency support . . . . . . . . . . . . . . ." CONFIG_JACK_LATENCY)
show_option ("  JACK Metadata support  . . . . . . . . . . . . . ." CONFIG_JACK_METADATA)
message     ("")
show_option ("  Non Session Management (NSM) support . . . . . . ." CONFIG_NSM)
message     ("")
show_option ("  VeSTige header support . . . . . . . . . . . . . ." CONFIG_VESTIGE)
show_option ("  Unique/Single instance support . . . . . . . . . ." CONFIG_XUNIQUE)
show_option ("  Gradient eye-candy . . . . . . . . . . . . . . . ." CONFIG_GRADIENT)
show_option ("  Debugger stack-trace (gdb) . . . . . . . . . . . ." CONFIG_STACKTRACE)
message   ("\n  Install prefix . . . . . . . . . . . . . . . . . .: ${CMAKE_INSTALL_PREFIX}")
message   ("\nNow type 'make', followed by 'make install' as root.\n")
