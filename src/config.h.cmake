#ifndef CONFIG_H
#define CONFIG_H

/* Define to the full name of this package. */
#cmakedefine PACKAGE_NAME "@PACKAGE_NAME@"

/* Define to the full name and version of this package. */
#cmakedefine PACKAGE_STRING "@PACKAGE_STRING@"

/* Define to the version of this package. */
#cmakedefine PACKAGE_VERSION "@PACKAGE_VERSION@"

/* Define to the address where bug reports for this package should be sent. */
#cmakedefine PACKAGE_BUGREPORT "@PACKAGE_BUGREPORT@"

/* Define to the one symbol short name of this package. */
#cmakedefine PACKAGE_TARNAME "@PACKAGE_TARNAME@"

/* Define to the version of this package. */
#cmakedefine CONFIG_VERSION "@CONFIG_VERSION@"

/* Define to the build version of this package. */
#cmakedefine CONFIG_BUILD_VERSION "@CONFIG_BUILD_VERSION@"

/* Default installation prefix. */
#cmakedefine CONFIG_PREFIX "@CONFIG_PREFIX@"

/* Define to target installation dirs. */
#cmakedefine CONFIG_BINDIR "@CONFIG_BINDIR@"
#cmakedefine CONFIG_LIBDIR "@CONFIG_LIBDIR@"
#cmakedefine CONFIG_DATADIR "@CONFIG_DATADIR@"
#cmakedefine CONFIG_MANDIR "@CONFIG_MANDIR@"

/* Define if debugging is enabled. */
#cmakedefine CONFIG_DEBUG @CONFIG_DEBUG@

/* Define to 1 if you have the <signal.h> header file. */
#cmakedefine HAVE_SIGNAL_H @HAVE_SIGNAL_H@

/* Define if IEEE 32bit float optimizations are enabled. */
#cmakedefine CONFIG_FLOAT32 @CONFIG_FLOAT32@

/* Define if round is available. */
#cmakedefine CONFIG_ROUND @CONFIG_ROUND@

/* Define if JACK library is available. */
#cmakedefine CONFIG_LIBJACK @CONFIG_LIBJACK@

/* Define if ALSA library is available. */
#cmakedefine CONFIG_LIBASOUND @CONFIG_LIBASOUND@

/* Define if SNDFILE library is available. */
#cmakedefine CONFIG_LIBSNDFILE @CONFIG_LIBSNDFILE@

/* Define if VORBIS library is available. */
#cmakedefine CONFIG_LIBVORBIS @CONFIG_LIBVORBIS@

/* Define if MAD library is available. */
#cmakedefine CONFIG_LIBMAD @CONFIG_LIBMAD@

/* Define if SAMPLERATE library is available. */
#cmakedefine CONFIG_LIBSAMPLERATE @CONFIG_LIBSAMPLERATE@

/* Define if RUBBERBAND library is available. */
#cmakedefine CONFIG_LIBRUBBERBAND @CONFIG_LIBRUBBERBAND@

/* Define if AUBIO library is available. */
#cmakedefine CONFIG_LIBAUBIO @CONFIG_LIBAUBIO@

/* Define if LIBLO library is available. */
#cmakedefine CONFIG_LIBLO @CONFIG_LIBLO@

/* Define if ZLIB library is available. */
#cmakedefine CONFIG_LIBZ @CONFIG_LIBZ@

/* Define if LILV library is available. */
#cmakedefine CONFIG_LIBLILV @CONFIG_LIBLILV@

/* Define if lilv_file_uri_parse is available. */
#cmakedefine CONFIG_LILV_FILE_URI_PARSE @CONFIG_LILV_FILE_URI_PARSE@

/* Define if lilv_world_unload_resource is available. */
#cmakedefine CONFIG_LILV_WORLD_UNLOAD_RESOURCE @CONFIG_LILV_WORLD_UNLOAD_RESOURCE@

/* Define if SUIL library is enabled. */
#cmakedefine CONFIG_LIBSUIL @CONFIG_LIBSUIL@

/* Define if suil_instance_get_handle is available. */
#cmakedefine CONFIG_SUIL_INSTANCE_GET_HANDLE @CONFIG_SUIL_INSTANCE_GET_HANDLE@

/* Define if JACK latency support is available. */
#cmakedefine CONFIG_JACK_LATENCY @CONFIG_JACK_LATENCY@

/* Define if LADSPA header is available. */
#cmakedefine CONFIG_LADSPA @CONFIG_LADSPA@

/* Define if DSSI header is available. */
#cmakedefine CONFIG_DSSI @CONFIG_DSSI@

/* Define if VST2 header is available. */
#cmakedefine CONFIG_VST2 @CONFIG_VST2@

/* Define if VeSTige header is available. */
#cmakedefine CONFIG_VESTIGE @CONFIG_VESTIGE@

/* Define if LV2 headers are available. */
#cmakedefine CONFIG_LV2 @CONFIG_LV2@

/* Define if LV2 UI support is available. */
#cmakedefine CONFIG_LV2_UI @CONFIG_LV2_UI@

/* Define if LV2 Event/MIDI support is available. */
#cmakedefine CONFIG_LV2_EVENT @CONFIG_LV2_EVENT@

/* Define if LV2 Atom/MIDI aupport is available. */
#cmakedefine CONFIG_LV2_ATOM @CONFIG_LV2_ATOM@

/* Define if LV2 CVPort aupport is available. (DUMMY) */
#cmakedefine CONFIG_LV2_CVPORT @CONFIG_LV2_CVPORT@

/* Define if lv2_atom_forge_object is available. */
#cmakedefine CONFIG_LV2_ATOM_FORGE_OBJECT @CONFIG_LV2_ATOM_FORGE_OBJECT@

/* Define if lv2_atom_forge_key is available. */
#cmakedefine CONFIG_LV2_ATOM_FORGE_KEY @CONFIG_LV2_ATOM_FORGE_KEY@

/* Define if LV2 Worker/Schedule aupport is available. */
#cmakedefine CONFIG_LV2_WORKER @CONFIG_LV2_WORKER@

/* Define if LV2 External UI extension is available. */
#cmakedefine CONFIG_LV2_EXTERNAL_UI @CONFIG_LV2_EXTERNAL_UI@

/* Define if LV2 State extension is available. */
#cmakedefine CONFIG_LV2_STATE @CONFIG_LV2_STATE@

/* Define if LV2 State Files feature is available. */
#cmakedefine CONFIG_LV2_STATE_FILES @CONFIG_LV2_STATE_FILES@

/* Define if LV2 State Make Path feature is available. */
#cmakedefine CONFIG_LV2_STATE_MAKE_PATH @CONFIG_LV2_STATE_MAKE_PATH@

/* Define if LV2 Programs extension is available. */
#cmakedefine CONFIG_LV2_PROGRAMS @CONFIG_LV2_PROGRAMS@

/* Define if LV2 MIDNAM extension is available. */
#cmakedefine CONFIG_LV2_MIDNAM @CONFIG_LV2_MIDNAM@

/* Define if LV2 Presets are supported. */
#cmakedefine CONFIG_LV2_PRESETS @CONFIG_LV2_PRESETS@

/* Define if LV2 Patch is supported. */
#cmakedefine CONFIG_LV2_PATCH @CONFIG_LV2_PATCH@

/* Define if LV2 Port-event is supported. */
#cmakedefine CONFIG_LV2_PORT_EVENT @CONFIG_LV2_PORT_EVENT@

/* Define if LV2 Time is supported. */
#cmakedefine CONFIG_LV2_TIME @CONFIG_LV2_TIME@

/* Define if LV2 Options is supported. */
#cmakedefine CONFIG_LV2_OPTIONS @CONFIG_LV2_OPTIONS@

/* Define if LV2 Buf-size is supported. */
#cmakedefine CONFIG_LV2_BUF_SIZE @CONFIG_LV2_BUF_SIZE@

/* Define if LV2 Parameters is supported. */
#cmakedefine CONFIG_LV2_PARAMETERS @CONFIG_LV2_PARAMETERS@

/* Define if LV2 Time/position support is available. */
#cmakedefine CONFIG_LV2_TIME_POSITION @CONFIG_LV2_TIME_POSITION@

/* Define if LV2 UI Touch interface support is available. */
#cmakedefine CONFIG_LV2_UI_TOUCH @CONFIG_LV2_UI_TOUCH@

/* Define if LV2 UI Request-value support is available. */
#cmakedefine CONFIG_LV2_UI_REQ_VALUE @CONFIG_LV2_UI_REQ_VALUE@

/* Define if LV2 UI Request-value support is available. (FAKE) */
#cmakedefine CONFIG_LV2_UI_REQ_VALUE_FAKE @CONFIG_LV2_UI_REQ_VALUE_FAKE@

/* Define if LV2 UI Idle interface support is available. */
#cmakedefine CONFIG_LV2_UI_IDLE @CONFIG_LV2_UI_IDLE@

/* Define if LV2 UI Show interface support is available. */
#cmakedefine CONFIG_LV2_UI_SHOW @CONFIG_LV2_UI_SHOW@

/* Define if LV2 UI GTK2 native support is available. */
#cmakedefine CONFIG_LV2_UI_GTK2 @CONFIG_LV2_UI_GTK2@

/* Define if LV2 UI GTKMM2 native support is available. */
#cmakedefine CONFIG_LV2_UI_GTKMM2 @CONFIG_LV2_UI_GTKMM2@

/* Define if LV2 UI X11 native support is available. */
#cmakedefine CONFIG_LV2_UI_X11 @CONFIG_LV2_UI_X11@

/* Define if VST3 plug-in support is avilable. */
#cmakedefine CONFIG_VST3 @CONFIG_VST3@

/* Define if CLAP plug-in support is avilable. */
#cmakedefine CONFIG_CLAP @CONFIG_CLAP@

/* Define if JACK session support is available. */
#cmakedefine CONFIG_JACK_SESSION @CONFIG_JACK_SESSION@

/* Define if JACK metadata support is available. */
#cmakedefine CONFIG_JACK_METADATA @CONFIG_JACK_METADATA@

/* Define if jack_set_port_rename_callback is available. */
#cmakedefine CONFIG_JACK_PORT_RENAME @CONFIG_JACK_PORT_RENAME@

/* Define if NSM support is available. */
#cmakedefine CONFIG_NSM @CONFIG_NSM@

/* Define if unique/single instance is enabled. */
#cmakedefine CONFIG_XUNIQUE @CONFIG_XUNIQUE@

/* Define if gradient eye-candy is enabled. */
#cmakedefine CONFIG_GRADIENT @CONFIG_GRADIENT@

/* Define if debugger stack-trace is enabled. */
#cmakedefine CONFIG_STACKTRACE @CONFIG_STACKTRACE@

/* Define if Wayland is supported (NOT RECOMMENDED) */
#cmakedefine CONFIG_WAYLAND @CONFIG_WAYLAND@

#endif /* CONFIG_H */
