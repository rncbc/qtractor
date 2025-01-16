cmake_minimum_required (VERSION 3.15)

project (Qtractor
  VERSION 1.5.2
  DESCRIPTION "An Audio/MIDI multi-track sequencer"
  HOMEPAGE_URL "https://qtractor.org"
  LANGUAGES C CXX)

set (PROJECT_TITLE "${PROJECT_NAME}")
string (TOLOWER "${PROJECT_TITLE}" PROJECT_NAME)

set (PROJECT_COPYRIGHT "Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.")
set (PROJECT_DOMAIN    "rncbc.org")

execute_process (
  COMMAND git describe --tags --dirty --abbrev=6
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
  RESULT_VARIABLE GIT_DESCRIBE_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if (GIT_DESCRIBE_RESULT EQUAL 0)
  set (GIT_VERSION "${GIT_DESCRIBE_OUTPUT}")
  string (REGEX REPLACE "^[^0-9]+" "" GIT_VERSION "${GIT_VERSION}")
  string (REGEX REPLACE "-g"   "git." GIT_VERSION "${GIT_VERSION}")
  string (REGEX REPLACE "[_|-]"   "." GIT_VERSION "${GIT_VERSION}")
  execute_process (
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REVPARSE_OUTPUT
    RESULT_VARIABLE GIT_REVPARSE_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (GIT_REVPARSE_RESULT EQUAL 0 AND NOT GIT_REVPARSE_OUTPUT STREQUAL "main")
    set (GIT_VERSION "${GIT_VERSION} [${GIT_REVPARSE_OUTPUT}]")
  endif ()
  set (PROJECT_VERSION "${GIT_VERSION}")
endif ()


if (CMAKE_BUILD_TYPE MATCHES "Debug")
  set (CONFIG_DEBUG 1)
  set (CONFIG_BUILD_TYPE "debug")
else ()
  set (CONFIG_DEBUG 0)
  set (CONFIG_BUILD_TYPE "release")
  set (CMAKE_BUILD_TYPE "Release")
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
option (CONFIG_VST2 "Enable VST2 plug-in support (default=yes)" 1)

option (CONFIG_VESTIGE "Enable VeSTige header support (default=yes)" 1)

# Enable VST3 support.
option (CONFIG_VST3 "Enable VST3 plug-in support (default=yes)" 1)

# Enable LV2 support.
option (CONFIG_LV2 "Enable LV2 plug-in support (default=yes)" 1)

option (CONFIG_LV2_EVENT "Enable LV2 plug-in MIDI/Event support (default=no)" 0)

option (CONFIG_LV2_ATOM "Enable LV2 plug-in MIDI/Atom support (default=yes)" 1)

option (CONFIG_LV2_CVPORT "Enable LV2 plug-in CVPort support (DUMMY) (default=yes)" 1)

option (CONFIG_LV2_WORKER "Enable LV2 plug-in Worker/schedule support (default=yes)" 1)

option (CONFIG_LV2_UI "Enable LV2 plug-in UI support (default=yes)" 1)

option (CONFIG_LV2_EXTERNAL_UI "Enable LV2 plug-in External UI support (default=yes)" 1)

option (CONFIG_LV2_STATE "Enable LV2 plug-in State support (default=yes)" 1)

option (CONFIG_LV2_STATE_FILES "Enable LV2 plug-in State Files support (default=yes)" 1)

option (CONFIG_LV2_STATE_MAKE_PATH "Enable LV2 plug-in State Make Path support (default=no)" 0)

option (CONFIG_LV2_STATE_FREE_PATH "Enable LV2 plug-in State Free Path support (default=yes)" 1)

option (CONFIG_LV2_PROGRAMS "Enable LV2 plug-in Programs support (default=yes)" 1)

option (CONFIG_LV2_MIDNAM "Enable LV2 plug-in MIDNAM support (default=yes)" 1)

option (CONFIG_LV2_PRESETS "Enable LV2 plug-in Presets support (default=yes)" 1)

option (CONFIG_LV2_PATCH "Enable LV2 plug-in Patch support (default=yes)" 1)

option (CONFIG_LV2_PORT_EVENT "Enable LV2 plug-in Port-event support (default=yes)" 1)

option (CONFIG_LV2_PORT_CHANGE_REQUEST "Enable LV2 plug-in Port-change request support (default=yes)" 1)

option (CONFIG_LV2_TIME "Enable LV2 plug-in Time support (default=yes)" 1)

option (CONFIG_LV2_TIME_POSITION "Enable LV2 plug-in Time/position support (default=yes)" 1)

option (CONFIG_LV2_OPTIONS "Enable LV2 plug-in Options support (default=yes)" 1)

option (CONFIG_LV2_BUF_SIZE "Enable LV2 plug-in Buf-size support (default=yes)" 1)

option (CONFIG_LV2_PARAMETERS "Enable LV2 plug-in Parameters support (default=yes)" 1)

option (CONFIG_LV2_UI_TOUCH "Enable LV2 plug-in UI Touch interface support (default=yes)" 1)

option (CONFIG_LV2_UI_REQ_VALUE "Enable LV2 plug-in UI Request-value support (default=yes)" 1)

option (CONFIG_LV2_UI_IDLE "Enable LV2 plug-in UI Idle interface support (default=yes)" 1)

option (CONFIG_LV2_UI_SHOW "Enable LV2 plug-in UI Show interface support (default=yes)" 1)

option (CONFIG_LV2_UI_GTK2 "Enable LV2 plug-in UI GTK2 native support (default=yes)" 1)

option (CONFIG_LV2_UI_GTKMM2 "Enable LV2 plug-in UI GTKMM2 native support (default=yes)" 1)

option (CONFIG_LV2_UI_X11 "Enable LV2 plug-in UI X11 native support (default=yes)" 1)

# Enable CLAP support.
option (CONFIG_CLAP "Enable CLAP plug-in support (default=yes)" 1)

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

# Enable Wayland support option.
option (CONFIG_WAYLAND "Enable Wayland support (NOT RECOMMENDED) (default=no)" 0)

# Enable Qt6 build preference.
option (CONFIG_QT6 "Enable Qt6 build (default=yes)" 1)


# Fix for new CMAKE_REQUIRED_LIBRARIES policy.
if (POLICY CMP0075)
  cmake_policy (SET CMP0075 NEW)
endif ()

# Check for Qt...
if (CONFIG_QT6)
  find_package (Qt6 QUIET)
  if (NOT Qt6_FOUND)
    set (CONFIG_QT6 0)
  endif ()
endif ()

if (CONFIG_QT6)
  find_package (QT QUIET NAMES Qt6)
else ()
  find_package (QT QUIET NAMES Qt5)
endif ()

find_package (Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Xml Svg)

if (NOT CONFIG_QT6)
  find_package (Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS X11Extras)
endif ()

if (CONFIG_XUNIQUE)
  find_package (Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
endif ()

find_package (Qt${QT_VERSION_MAJOR}LinguistTools)

include (CheckIncludeFile)
include (CheckIncludeFiles)
include (CheckIncludeFileCXX)
include (CheckStructHasMember)
include (CheckFunctionExists)
include (CheckLibraryExists)
include (CheckTypeSize)

# Checks for libraries.
if (WIN32)
  check_function_exists (lroundf CONFIG_ROUND)
else ()
  find_library (MATH_LIBRARY m)
  if (MATH_LIBRARY)
    set (CMAKE_REQUIRED_LIBRARIES "${MATH_LIBRARY};${CMAKE_REQUIRED_LIBRARIES}")
  # link_libraries (${MATH_LIBRARY})
    check_function_exists (lroundf CONFIG_ROUND)
  else ()
    message (FATAL_ERROR "*** math library not found.")
  endif ()
  find_library (DL_LIBRARY dl)
  if (NOT DL_LIBRARY)
    message (FATAL_ERROR "*** dl library not found.")
  endif ()
endif ()

# Check for IEEE 32bit float optimizations.
set (CONFIG_FLOAT32 1)

# Check for SSE optimization.
if (CONFIG_SSE)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^i[3456]86")
    check_include_file (xmmintrin.h HAVE_XMMINTRIN_H)
    if (HAVE_XMMINTRIN_H)
      add_compile_options (-msse -mfpmath=sse)
    endif ()
  endif ()
endif ()

add_compile_options (-ffast-math)

# Checks for header files.
if (UNIX AND NOT APPLE)
  check_include_files ("fcntl.h;unistd.h;signal.h" HAVE_SIGNAL_H)
endif ()


# Check for LADSPA headers.
if (CONFIG_LADSPA)
  check_include_file (ladspa.h HAVE_LADSPA_H)
  if (NOT HAVE_LADSPA_H)
    set (CONFIG_LADSPA 0)
  endif ()
endif ()

# Check for DSSI headers.
if (CONFIG_DSSI)
  check_include_files ("stddef.h;dssi.h" HAVE_DSSI_H)
  if (NOT HAVE_DSSI_H)
    set (CONFIG_DSSI 0)
  endif ()
endif ()

# Check for VST headers.
if (CONFIG_VST2 AND NOT CONFIG_VST2SDK)
  check_include_file (aeffectx.h HAVE_AEFFECTX_H)
  if (NOT HAVE_AEFFECTX_H AND CONFIG_VESTIGE)
    set (VESTIGE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src/vestige)
    set (CMAKE_REQUIRED_INCLUDES "${VESTIGE_INCLUDES};${CMAKE_REQUIRED_INCLUDES}")
    include_directories (${VESTIGE_INCLUDES})
    check_include_file (vestige.h HAVE_VESTIGE_H)
    if (NOT HAVE_VESTIGE_H)
      set (CONFIG_VESTIGE 0)
    endif ()
    set (CONFIG_VST2 ${CONFIG_VESTIGE})
  else ()
    set (CONFIG_VESTIGE 0)
  endif ()
endif ()


# Find package modules
include (FindPkgConfig)

# Check for JACK libraries.
pkg_check_modules (JACK REQUIRED IMPORTED_TARGET jack>=0.100.0)
if (JACK_FOUND)
  find_library (JACK_LIBRARY NAMES ${JACK_LIBRARIES} HINTS ${JACK_LIBDIR})
endif ()
if (JACK_LIBRARY)
  set (CONFIG_LIBJACK 1)
  set (CMAKE_REQUIRED_LIBRARIES "${JACK_LIBRARY};${CMAKE_REQUIRED_LIBRARIES}")
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
  set (CONFIG_LIBJACK 0)
endif ()

# Check for ALSA libraries.
pkg_check_modules (ALSA REQUIRED IMPORTED_TARGET alsa)
if (ALSA_FOUND)
  find_library (ALSA_LIBRARY NAMES ${ALSA_LIBRARIES} HINTS ${ALSA_LIBDIR})
endif ()
if (ALSA_LIBRARY)
  set (CONFIG_LIBASOUND 1)
 #set (CMAKE_REQUIRED_LIBRARIES "${ALSA_LIBRARY};${CMAKE_REQUIRED_LIBRARIES}")
else ()
  message (FATAL_ERROR "*** ALSA library not found.")
  set (CONFIG_LIBASOUND 0)
endif ()

# Check for SNDFILE libraries.
pkg_check_modules (SNDFILE REQUIRED IMPORTED_TARGET sndfile)
if (SNDFILE_FOUND)
  find_library (SNDFILE_LIBRARY NAMES ${SNDFILE_LIBRARIES} HINTS ${SNDFILE_LIBDIR})
endif ()
if (SNDFILE_LIBRARY)
  set (CONFIG_LIBSNDFILE 1)
 #set (CMAKE_REQUIRED_LIBRARIES "${SNDFILE_LIBRARY};${CMAKE_REQUIRED_LIBRARIES}")
else ()
  message (FATAL_ERROR "*** SNDFILE library not found.")
  set (CONFIG_LIBSNDFILE 0)
endif ()

# Check for VORBIS libraries.
if (CONFIG_LIBVORBIS)
  pkg_check_modules (VORBIS IMPORTED_TARGET vorbis)
  if (NOT VORBIS_FOUND)
    message (WARNING "*** VORBIS library not found.")
    set (CONFIG_LIBVORBIS 0)
  endif ()
endif ()

if (CONFIG_LIBVORBIS)
  pkg_check_modules (VORBISENC IMPORTED_TARGET vorbisenc)
  pkg_check_modules (VORBISFILE IMPORTED_TARGET vorbisfile)
  pkg_check_modules (OGG IMPORTED_TARGET ogg)
endif ()

# Check for MAD libraries.
if (CONFIG_LIBMAD)
  pkg_check_modules (MAD IMPORTED_TARGET mad)
  if (NOT MAD_FOUND)
    message (WARNING "*** MAD library not found.")
    set (CONFIG_LIBMAD 0)
  endif ()
endif ()

# Check for SAMPLERATE libraries.
if (CONFIG_LIBSAMPLERATE)
  pkg_check_modules (SAMPLERATE IMPORTED_TARGET samplerate)
  if (NOT SAMPLERATE_FOUND)
    message (WARNING "*** SAMPLERATE library not found.")
    set (CONFIG_LIBSAMPLERATE 0)
  endif ()
endif ()

# Check for RUBBERBAND libraries.
if (CONFIG_LIBRUBBERBAND)
  pkg_check_modules (RUBBERBAND IMPORTED_TARGET rubberband)
  if (NOT RUBBERBAND_FOUND)
    message (WARNING "*** RUBBERBAND library not found.")
    set (CONFIG_LIBRUBBERBAND 0)
  endif ()
endif ()

# Check for AUBIO libraries.
if (CONFIG_LIBAUBIO)
  pkg_check_modules (AUBIO IMPORTED_TARGET aubio>=0.4.1)
  if (NOT AUBIO_FOUND)
    message (WARNING "*** AUBIO library not found.")
    set (CONFIG_LIBAUBIO 0)
  endif ()
endif ()

# Check for LIBLO libraries.
if (CONFIG_LIBLO)
  pkg_check_modules (LIBLO IMPORTED_TARGET liblo)
  if (NOT LIBLO_FOUND)
    message (WARNING "*** LIBLO library not found.")
    set (CONFIG_LIBLO 0)
  endif ()
endif ()

# Check for ZLIB libraries.
if (CONFIG_LIBZ)
  pkg_check_modules (ZLIB IMPORTED_TARGET zlib)
  if (NOT ZLIB_FOUND)
    message (WARNING "*** ZLIB library not found.")
    set (CONFIG_LIBZ 0)
  endif ()
endif ()

# Check for VST3 SDK.
if (CONFIG_VST3 AND NOT CONFIG_VST3SDK)
  set (CONFIG_VST3SDK ${CMAKE_CURRENT_SOURCE_DIR}/src/vst3)
endif ()

if (CONFIG_VST3 OR CONFIG_LV2_UI_X11)
  pkg_check_modules (XCB xcb)
  if (NOT XCB_FOUND)
    message (WARNING "*** XCB library not found.")
  endif ()
endif ()

if (CONFIG_VST3)
  find_library (PTHREAD_LIBRARY pthread)
  if (NOT PTHREAD_LIBRARY)
    message (WARNING "*** pthread library not found.")
  endif ()
endif ()

# Check for LV2 support.
if (CONFIG_LV2)
  pkg_check_modules (LV2 lv2)
  if (LV2_FOUND)
    include_directories (${LV2_INCLUDE_DIRS})
    # Check for LV2 old/new headers style.
    check_include_file (lv2/urid/urid.h HAVE_LV2_URID_H)
    if (NOT HAVE_LV2_URID_H)
      check_include_file (lv2/lv2plug.in/ns/ext/urid/urid.h HAVE_OLD_LV2_URID_H)
      if (NOT HAVE_OLD_LV2_URID_H)
        set (CONFIG_LV2 0)
      else ()
        set (CONFIG_LV2_OLD_HEADERS 1)
      endif ()
    else ()
      set (CONFIG_LV2_OLD_HEADERS 0)
    endif ()
  endif ()
  if (NOT CONFIG_LV2)
    message (WARNING "*** LV2 SDK not found.")
  endif ()
endif ()

# Check for LV2 headers.
if (CONFIG_LV2)
  set (LV2_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src/lv2)
  set (CMAKE_REQUIRED_INCLUDES "${LV2_INCLUDES};${CMAKE_REQUIRED_INCLUDES}")
  include_directories (${LV2_INCLUDES})
  if (CONFIG_LV2_OLD_HEADERS)
    set (CMAKE_REQUIRED_DEFINITIONS "-DCONFIG_LV2_OLD_HEADERS;${CMAKE_REQUIRED_DEFINITIONS}")
  endif ()
else ()
  set (CONFIG_LIBLILV 0)
  set (CONFIG_LV2_UI 0)
endif ()

if (CONFIG_LV2_UI)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file(lv2/lv2plug.in/ns/extensions/ui/ui.h HAVE_LV2_UI_H)
  else ()
    check_include_file(lv2/ui/ui.h HAVE_LV2_UI_H)
  endif ()
  if (NOT HAVE_LV2_UI_H)
    set (CONFIG_LV2_UI 0)
  endif ()
  if (CONFIG_LV2_UI)
    # Check for LV2 external UI instance access.
    if (CONFIG_LV2_EXTERNAL_UI)
      if (CONFIG_LV2_OLD_HEADERS)
        check_include_file (lv2/lv2plug.in/ns/ext/instance-access/instance-access.h HAVE_LV2_INSTANCE_ACCESS_H)
      else ()
        check_include_file (lv2/instance-access/instance-access.h HAVE_LV2_INSTANCE_ACCESS_H)
      endif ()
      if (NOT HAVE_LV2_INSTANCE_ACCESS_H)
        set (CONFIG_LV2_EXTERNAL_UI 0)
      endif ()
    endif ()
  endif ()
endif ()

if (NOT CONFIG_LV2_UI)
  set (CONFIG_LIBSUIL 0)
  set (CONFIG_LV2_EXTERNAL_UI 0)
  set (CONFIG_LV2_UI_TOUCH 0)
  set (CONFIG_LV2_UI_REQ_VALUE 0)
  set (CONFIG_LV2_UI_IDLE 0)
  set (CONFIG_LV2_UI_SHOW 0)
  set (CONFIG_LV2_UI_GTK2 0)
  set (CONFIG_LV2_UI_GTKMM2 0)
  set (CONFIG_LV2_UI_X11 0)
endif ()

# Check for optional LILV library.
if (CONFIG_LIBLILV)
  pkg_check_modules (LILV IMPORTED_TARGET lilv-0)
  if (LILV_FOUND)
    find_library (LILV_LIBRARY NAMES ${LILV_LIBRARIES} HINTS ${LILV_LIBDIR})
  endif ()
  if (LILV_LIBRARY)
    set (CONFIG_LIBLILV 1)
    set (CMAKE_REQUIRED_LIBRARIES "${LILV_LIBRARY};${CMAKE_REQUIRED_LIBRARIES}")
    check_function_exists (lilv_file_uri_parse CONFIG_LILV_FILE_URI_PARSE)
    check_function_exists (lilv_world_unload_resource CONFIG_LILV_WORLD_UNLOAD_RESOURCE)
  else ()
    message (WARNING "*** LILV library not found.")
    set (CONFIG_LIBLILV 0)
  endif ()
endif ()

if (NOT CONFIG_LIBLILV)
  set (CONFIG_LV2 0)
  set (CONFIG_LV2_EVENT 0)
  set (CONFIG_LV2_ATOM 0)
  set (CONFIG_LV2_CVPORT 0)
  set (CONFIG_LV2_WORKER 0)
  set (CONFIG_LV2_UI 0)
  set (CONFIG_LV2_EXTERNAL_UI 0)
  set (CONFIG_LV2_STATE 0)
  set (CONFIG_LV2_STATE_FILES 0)
  set (CONFIG_LV2_STATE_MAKE_PATH 0)
  set (CONFIG_LV2_STATE_FREE_PATH 0)
  set (CONFIG_LV2_PROGRAMS 0)
  set (CONFIG_LV2_MIDNAM 0)
  set (CONFIG_LV2_PRESETS 0)
  set (CONFIG_LV2_PATCH 0)
  set (CONFIG_LV2_PORT_EVENT 0)
  set (CONFIG_LV2_PORT_CHANGE_REQUEST 0)
  set (CONFIG_LV2_TIME 0)
  set (CONFIG_LV2_TIME_POSITION 0)
  set (CONFIG_LV2_OPTIONS 0)
  set (CONFIG_LV2_BUF_SIZE 0)
  set (CONFIG_LV2_PARAMETERS 0)
  set (CONFIG_LV2_UI_TOUCH 0)
  set (CONFIG_LV2_UI_REQ_VALUE 0)
  set (CONFIG_LV2_UI_IDLE 0)
  set (CONFIG_LV2_UI_SHOW 0)
  set (CONFIG_LV2_UI_GTK2 0)
  set (CONFIG_LV2_UI_GTKMM2 0)
  set (CONFIG_LV2_UI_X11 0)
  set (CONFIG_LIBSUIL 0)
endif ()

# Disable libsuil upon Qt >= 6.0.0 ...
if (CONFIG_LIBSUIL AND QT_VERSION_MAJOR GREATER_EQUAL 6)
  set (CONFIG_LIBSUIL 0)
endif ()

# Check for LV2 new UI instantiation availability (libsuil).
if (CONFIG_LIBSUIL)
  pkg_check_modules (SUIL IMPORTED_TARGET suil-0)
  if (SUIL_FOUND)
    find_library (SUIL_LIBRARY NAMES ${SUIL_LIBRARIES} HINTS ${SUIL_LIBDIR})
  endif ()
  if (SUIL_LIBRARY)
    set (CONFIG_LIBSUIL 1)
    set (CMAKE_REQUIRED_LIBRARIES "${SUIL_LIBRARY};${CMAKE_REQUIRED_LIBRARIES}")
    check_function_exists (suil_instance_get_handle CONFIG_SUIL_INSTANCE_GET_HANDLE)
  else ()
    message (WARNING "*** SUIL library not found.")
    set (CONFIG_LIBSUIL 0)
  endif ()
endif ()

if (CONFIG_LV2_EVENT)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/event/event.h HAVE_LV2_EVENT_H)
  else ()
    check_include_file (lv2/event/event.h HAVE_LV2_EVENT_H)
  endif ()
  if (NOT HAVE_LV2_EVENT_H)
    set (CONFIG_LV2_EVENT 0)
  endif ()
endif ()

if (CONFIG_LV2_ATOM)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/atom/atom.h HAVE_LV2_ATOM_H)
  else ()
    check_include_file (lv2/atom/atom.h HAVE_LV2_ATOM_H)
  endif ()
  if (NOT HAVE_LV2_ATOM_H)
    set (CONFIG_LV2_ATOM 0)
  endif ()
  set (CONFIG_LV2_ATOM_FORGE_OBJECT ${CONFIG_LV2_ATOM})
  set (CONFIG_LV2_ATOM_FORGE_KEY ${CONFIG_LV2_ATOM})
endif ()

if (NOT CONFIG_LV2_ATOM)
  set (CONFIG_LV2_ATOM_FORGE_OBJECT 0)
  set (CONFIG_LV2_ATOM_FORGE_KEY 0)
  set (CONFIG_LV2_STATE 0)
  set (CONFIG_LV2_OPTIONS 0)
  set (CONFIG_LV2_TIME_POSITION 0)
  set (CONFIG_LV2_PORT_EVENT 0)
endif ()

if (CONFIG_LV2_WORKER)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/worker/worker.h HAVE_LV2_WORKER_H)
  else ()
    check_include_file (lv2/worker/worker.h HAVE_LV2_WORKER_H)
  endif ()
  if (NOT HAVE_LV2_WORKER_H)
    set (CONFIG_LV2_WORKER 0)
  endif ()
endif ()

if (CONFIG_LV2_STATE)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/state/state.h HAVE_LV2_STATE_H)
  else ()
    check_include_file (lv2/state/state.h HAVE_LV2_STATE_H)
  endif ()
  if (NOT HAVE_LV2_STATE_H)
    set (CONFIG_LV2_STATE 0)
  endif ()
endif ()

if (NOT CONFIG_LV2_STATE)
  set (CONFIG_LV2_STATE_FILES 0)
  set (CONFIG_LV2_STATE_MAKE_PATH 0)
  set (CONFIG_LV2_STATE_FREE_PATH 0)
  set (CONFIG_LV2_PRESETS 0)
  set (CONFIG_LV2_PATCH 0)
  set (CONFIG_LV2_TIME 0)
endif ()

if (CONFIG_LV2_STATE_FREE_PATH)
  if (CONFIG_LV2_OLD_HEADERS)
    set (CMAKE_EXTRA_INCLUDE_FILES "lv2/lv2plug.in/ns/ext/state/state.h")
  else ()
    set (CMAKE_EXTRA_INCLUDE_FILES "lv2/state/state.h")
  endif ()
  check_type_size (LV2_State_Free_Path LV2_STATE_FREE_PATH)
  if (NOT HAVE_LV2_STATE_FREE_PATH)
    set (CONFIG_LV2_STATE_FREE_PATH 0)
  endif ()
endif ()

if (CONFIG_LV2_PROGRAMS)
  check_include_file (lv2_programs.h HAVE_LV2_PROGRAMS_H)
  if (NOT HAVE_LV2_PROGRAMS_H)
    set (CONFIG_LV2_PROGRAMS 0)
  endif ()
endif ()

if (CONFIG_LV2_MIDNAM)
  check_include_file (lv2_midnam.h HAVE_LV2_MIDNAM_H)
  if (NOT HAVE_LV2_MIDNAM_H)
    set (CONFIG_LV2_MIDNAM 0)
  endif ()
endif ()

if (CONFIG_LV2_PRESETS)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/presets/presets.h HAVE_LV2_PRESETS_H)
  else ()
    check_include_file (lv2/presets/presets.h HAVE_LV2_PRESETS_H)
  endif ()
  if (NOT HAVE_LV2_PRESETS_H)
    set (CONFIG_LV2_PRESETS 0)
  endif ()
endif ()

if (CONFIG_LV2_PATCH)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/patch/patch.h HAVE_LV2_PATCH_H)
  else ()
    check_include_file (lv2/patch/patch.h HAVE_LV2_PATCH_H)
  endif ()
  if (NOT HAVE_LV2_PATCH_H)
    set (CONFIG_LV2_PATCH 0)
  endif ()
endif ()

if (CONFIG_LV2_TIME)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/time/time.h HAVE_LV2_TIME_H)
  else ()
    check_include_file (lv2/time/time.h HAVE_LV2_TIME_H)
  endif ()
  if (NOT HAVE_LV2_TIME_H)
    set (CONFIG_LV2_TIME 0)
  endif ()
endif ()

if (NOT CONFIG_LV2_TIME)
  set (CONFIG_LV2_TIME_POSITION 0)
endif ()

if (CONFIG_LV2_OPTIONS)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/options/options.h HAVE_LV2_OPTIONS_H)
  else ()
    check_include_file (lv2/options/options.h HAVE_LV2_OPTIONS_H)
  endif ()
  if (NOT HAVE_LV2_OPTIONS_H)
    set (CONFIG_LV2_OPTIONS 0)
  endif ()
endif ()

if (NOT CONFIG_LV2_OPTIONS)
  set (CONFIG_LV2_BUF_SIZE 0)
endif ()

if (CONFIG_LV2_BUF_SIZE)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/buf-size/buf-size.h HAVE_LV2_BUF_SIZE_H)
  else ()
    check_include_file (lv2/buf-size/buf-size.h HAVE_LV2_BUF_SIZE_H)
  endif ()
  if (NOT HAVE_LV2_BUF_SIZE_H)
    set (CONFIG_LV2_BUF_SIZE 0)
  endif ()
endif ()

if (CONFIG_LV2_PARAMETERS)
  if (CONFIG_LV2_OLD_HEADERS)
    check_include_file (lv2/lv2plug.in/ns/ext/parameters/parameters.h HAVE_LV2_PARAMETERS_H)
  else ()
    check_include_file (lv2/parameters/parameters.h HAVE_LV2_PARAMETERS_H)
  endif ()
  if (NOT HAVE_LV2_PARAMETERS_H)
    set (CONFIG_LV2_PARAMETERS 0)
  endif ()
endif ()

if (CONFIG_LV2_PORT_CHANGE_REQUEST)
  check_include_file (lv2_port_change_request.h HAVE_LV2_PORT_CHANGE_REQUEST_H)
  if (NOT HAVE_LV2_PORT_CHANGE_REQUEST_H)
    set (CONFIG_LV2_PORT_CHANGE_REQUEST 0)
  endif ()
endif ()

if (CONFIG_LV2_UI_TOUCH)
  if (CONFIG_LV2_OLD_HEADERS)
    check_struct_has_member ("LV2UI_Touch" touch lv2/lv2plug.in/ns/extensions/ui/ui.h HAVE_LV2_UI_TOUCH)
  else ()
    check_struct_has_member ("LV2UI_Touch" touch lv2/ui/ui.h HAVE_LV2_UI_TOUCH)
  endif ()
  if (NOT HAVE_LV2_UI_TOUCH)
    set (CONFIG_LV2_UI_TOUCH 0)
  endif ()
endif ()

if (CONFIG_LV2_UI_REQ_VALUE)
  if (CONFIG_LV2_OLD_HEADERS)
    check_struct_has_member ("LV2UI_Request_Value" request lv2/lv2plug.in/ns/extensions/ui/ui.h HAVE_LV2_UI_REQ_VALUE)
  else ()
    check_struct_has_member ("LV2UI_Request_Value" request lv2/ui/ui.h HAVE_LV2_UI_REQ_VALUE)
  endif ()
  if (NOT HAVE_LV2_UI_REQ_VALUE)
    set (CONFIG_LV2_UI_REQ_VALUE_FAKE 1)
  endif ()
endif ()

if (CONFIG_LV2_UI_GTK2)
  pkg_check_modules (GTK2 gtk+-2.0)
  if (NOT GTK2_FOUND)
    message (WARNING "*** GTK2 libraries not found.")
    set (CONFIG_LV2_UI_GTK2 0)
    set (CONFIG_LV2_UI_GTKMM2 0)
  endif ()
endif ()

if (CONFIG_LV2_UI_GTKMM2)
  pkg_check_modules (GTKMM2 gtkmm-2.4)
  if (NOT GTKMM2_FOUND)
    message (WARNING "*** GTKMM2 libraries not found.")
    set (CONFIG_LV2_UI_GTKMM2 0)
  endif ()
endif ()


add_subdirectory (src)


# Finally check whether Qt is statically linked.
if (QT_FEATURE_static)
  set(QT_VERSION "${QT_VERSION}-static")
endif ()

# Configuration status
macro (SHOW_OPTION text value)
  if (${value})
    message ("${text}: yes")
  else ()
    message ("${text}: no")
  endif ()
endmacro ()

message   ("\n  ${PROJECT_TITLE} ${PROJECT_VERSION} (Qt ${QT_VERSION})")
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
show_option ("  VST2 plug-in support . . . . . . . . . . . . . . ." CONFIG_VST2)
show_option ("  VST3 plug-in support . . . . . . . . . . . . . . ." CONFIG_VST3)
show_option ("  CLAP plug-in support . . . . . . . . . . . . . . ." CONFIG_CLAP)
show_option ("  LV2 plug-in support  . . . . . . . . . . . . . . ." CONFIG_LV2)
show_option ("  LV2 plug-in support (liblilv)  . . . . . . . . . ." CONFIG_LIBLILV)
show_option ("  LV2 plug-in UI support . . . . . . . . . . . . . ." CONFIG_LV2_UI)
show_option ("  LV2 plug-in UI support (libsuil) . . . . . . . . ." CONFIG_LIBSUIL)
show_option ("  LV2 plug-in External UI support  . . . . . . . . ." CONFIG_LV2_EXTERNAL_UI)
show_option ("  LV2 plug-in MIDI/Event support (DEPRECATED)  . . ." CONFIG_LV2_EVENT)
show_option ("  LV2 plug-in MIDI/Atom support  . . . . . . . . . ." CONFIG_LV2_ATOM)
show_option ("  LV2 plug-in Worker/Schedule support  . . . . . . ." CONFIG_LV2_WORKER)
show_option ("  LV2 plug-in State support  . . . . . . . . . . . ." CONFIG_LV2_STATE)
show_option ("  LV2 plug-in State Files support  . . . . . . . . ." CONFIG_LV2_STATE_FILES)
show_option ("  LV2 plug-in State Make Path support (DANGEROUS)  ." CONFIG_LV2_STATE_MAKE_PATH)
show_option ("  LV2 plug-in State Free Path support  . . . . . . ." CONFIG_LV2_STATE_FREE_PATH)
show_option ("  LV2 plug-in Programs support . . . . . . . . . . ." CONFIG_LV2_PROGRAMS)
show_option ("  LV2 plug-in MIDNAM support . . . . . . . . . . . ." CONFIG_LV2_MIDNAM)
show_option ("  LV2 plug-in Presets support  . . . . . . . . . . ." CONFIG_LV2_PRESETS)
show_option ("  LV2 plug-in Patch support  . . . . . . . . . . . ." CONFIG_LV2_PATCH)
show_option ("  LV2 plug-in Port-event support . . . . . . . , . ." CONFIG_LV2_PORT_EVENT)
show_option ("  LV2 plug-in Port-change request (EXPERIMENTAL) . ." CONFIG_LV2_PORT_CHANGE_REQUEST)
show_option ("  LV2 plug-in Time support . . . . . . . . . . . . ." CONFIG_LV2_TIME)
show_option ("  LV2 plug-in Time/position support  . . . . . . . ." CONFIG_LV2_TIME_POSITION)
show_option ("  LV2 plug-in Options support  . . . . . . . . . . ." CONFIG_LV2_OPTIONS)
show_option ("  LV2 plug-in Buf-size support . . . . . . . . . . ." CONFIG_LV2_BUF_SIZE)
show_option ("  LV2 plug-in Parameters support . . . . . . . . . ." CONFIG_LV2_PARAMETERS)
show_option ("  LV2 plug-in UI Touch interface support . . . . . ." CONFIG_LV2_UI_TOUCH)
show_option ("  LV2 plug-in UI Request-value support . . . . . . ." CONFIG_LV2_UI_REQ_VALUE)
show_option ("  LV2 plug-in UI Idle interface support  . . . . . ." CONFIG_LV2_UI_IDLE)
show_option ("  LV2 plug-in UI Show interface support  . . . . . ." CONFIG_LV2_UI_SHOW)
show_option ("  LV2 plug-in UI GTK2 native support . . . . . . . ." CONFIG_LV2_UI_GTK2)
show_option ("  LV2 plug-in UI GTKMM2 native support . . . . . . ." CONFIG_LV2_UI_GTKMM2)
show_option ("  LV2 plug-in UI X11 native support  . . . . . . . ." CONFIG_LV2_UI_X11)
message     ("")
show_option ("  JACK Session support . . . . . . . . . . . . . . ." CONFIG_JACK_SESSION)
show_option ("  JACK Latency support . . . . . . . . . . . . . . ." CONFIG_JACK_LATENCY)
show_option ("  JACK Metadata support  . . . . . . . . . . . . . ." CONFIG_JACK_METADATA)
message     ("")
show_option ("  Non/New Session Management (NSM) support . . . . ." CONFIG_NSM)
message     ("")
show_option ("  VeSTige header support . . . . . . . . . . . . . ." CONFIG_VESTIGE)
show_option ("  Unique/Single instance support . . . . . . . . . ." CONFIG_XUNIQUE)
show_option ("  Gradient eye-candy . . . . . . . . . . . . . . . ." CONFIG_GRADIENT)
show_option ("  Debugger stack-trace (gdb) . . . . . . . . . . . ." CONFIG_STACKTRACE)
message   ("\n  Install prefix . . . . . . . . . . . . . . . . . .: ${CONFIG_PREFIX}\n")
