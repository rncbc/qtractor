Nebula Quasar - An Audio/MIDI multi-track sequencer
----------------------------------------------------

Nebula Quasar is an audio/MIDI multi-track sequencer application written
in C++ with the Qt framework. This software is made by Nebula Audio.

Target platforms are macOS and Windows, where:
- On macOS: CoreAudio for audio and MIDI infrastructure, supporting 
  AudioUnit (AU), VST, VST3, LV2 and CLAP plug-ins.
- On Windows: WASAPI and WaveRT for audio, supporting native VST, VST3, 
  LV2 and CLAP plug-ins.

This is a port of Qtractor, originally developed for Linux by rncbc aka 
Rui Nuno Capela.


Features
--------

- Multi-track audio and MIDI sequencing and recording.
- Developed on the Qt C++ application and UI framework.
- Uses CoreAudio (macOS) or WASAPI/WaveRT (Windows) as multimedia 
  infrastructures.
- Traditional multi-track tape recorder control paradigm.
- Audio file formats support: OGG (via libvorbis), MP3 (via libmad, 
  playback only), WAV, FLAC, AIFF and many more (via libsndfile).
- Standard MIDI files support (format 0 and 1).
- Non-destructive, non-linear editing.
- Unlimited number of tracks per session/project.
- Unlimited number of overlapping clips per track.
- XML encoded session/project description files (SDI).
- Point-and-click, multi-select, drag-and-drop interaction.
- Unlimited undo/redo.
- Built-in mixer and monitor controls.
- Built-in connection patchbay control and persistence.
- Native VST(2), VST3, LV2 and CLAP plug-ins support.
- AudioUnit (AU) plug-ins support on macOS.
- Unlimited number of plug-ins per track or bus.
- Plug-in presets, programs and configurations support.
- Unlimited audio/MIDI effect send/return inserts per track or bus.
- Loop-recording/takes.
- Audio/MIDI clip fade-in/out, cross-fade.
- Audio/MIDI clip gain/volume, normalize, export.
- Audio/MIDI track and plugin parameter automation.
- Audio clip time-stretching and pitch-shifting.
- Seamless sample-rate conversion.
- Audio/MIDI track export (mix-down, render, merge, freeze).
- Audio/MIDI metronome bar/beat clicks.
- Unlimited tempo/time-signature map.
- Unlimited location/bar markers.
- MIDI clip editor (matrix/piano roll).
- MIDI instrument definitions support.
- MIDI controller mapping/learn/assignment.
- MIDI system exclusive (SysEx) setups.
- Configurable keyboard and MIDI controller shortcuts.


Requirements
------------

- Qt 5 or Qt 6 framework
- CMake 3.15 or higher
- libsndfile for audio file support
- libvorbis for OGG file support (optional)
- libmad for MP3 playback support (optional)
- libsamplerate for sample-rate conversion (optional)
- librubberband for time-stretching and pitch-shifting (optional)

Platform-specific requirements:

macOS:
- Xcode Command Line Tools
- CoreAudio frameworks (included with macOS)
- AudioUnitSDK for AU plug-in support

Windows:
- Microsoft Visual Studio 2019 or later (or MinGW-w64)
- Windows SDK for WASAPI/WaveRT support


Building
--------

### macOS

```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCONFIG_QT6=ON \
  -DCONFIG_LADSPA=OFF \
  -DCONFIG_DSSI=OFF \
  -DCONFIG_AU=ON \
  -DCONFIG_VST2=ON \
  -DCONFIG_VST3=ON \
  -DCONFIG_LV2=ON \
  -DCONFIG_CLAP=ON
make -j$(sysctl -n hw.ncpu)
```

### Windows

```bash
mkdir build && cd build
cmake .. ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCONFIG_QT6=ON ^
  -DCONFIG_LADSPA=OFF ^
  -DCONFIG_DSSI=OFF ^
  -DCONFIG_WASAPI=ON ^
  -DCONFIG_WAVERT=ON ^
  -DCONFIG_VST2=ON ^
  -DCONFIG_VST3=ON ^
  -DCONFIG_LV2=ON ^
  -DCONFIG_CLAP=ON
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
```


Plug-in Support
---------------

### macOS
- AudioUnit (AU) - via Apple AudioUnitSDK
- VST2 - via VeSTige headers or Steinberg VST SDK
- VST3 - via Steinberg VST3 SDK
- LV2 - via LV2 SDK and lilv
- CLAP - via CLAP headers

### Windows
- VST2 - via VeSTige headers or Steinberg VST SDK
- VST3 - via Steinberg VST3 SDK
- LV2 - via LV2 SDK and lilv
- CLAP - via CLAP headers

Note: LADSPA and DSSI plug-in formats are not supported on macOS or Windows 
as they are Linux-specific formats.


License
-------

Nebula Quasar is free, open-source software, distributed under the terms of
the GNU General Public License (GPL) version 2 or later.


Credits
-------

This software is based on Qtractor, originally developed by rncbc aka 
Rui Nuno Capela.

Made by Nebula Audio.
