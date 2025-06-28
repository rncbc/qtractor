# project (qtractor)

set (CMAKE_INCLUDE_CURRENT_DIR ON)

set (CMAKE_AUTOUIC ON)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
  file (REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
endif ()
configure_file (config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set (HEADERS
  qtractor.h
  qtractorAbout.h
  qtractorAtomic.h
  qtractorActionControl.h
  qtractorAudioBuffer.h
  qtractorAudioClip.h
  qtractorAudioConnect.h
  qtractorAudioEngine.h
  qtractorAudioFile.h
  qtractorAudioListView.h
  qtractorAudioMadFile.h
  qtractorAudioMeter.h
  qtractorAudioMonitor.h
  qtractorAudioPeak.h
  qtractorAudioSndFile.h
  qtractorAudioVorbisFile.h
  qtractorClapPlugin.h
  qtractorClip.h
  qtractorClipCommand.h
  qtractorClipSelect.h
  qtractorComboBox.h
  qtractorCommand.h
  qtractorConnect.h
  qtractorConnections.h
  qtractorCtlEvent.h
  qtractorCurve.h
  qtractorCurveCommand.h
  qtractorCurveFile.h
  qtractorCurveSelect.h
  qtractorDocument.h
  qtractorDssiPlugin.h
  qtractorEngine.h
  qtractorEngineCommand.h
  qtractorFileList.h
  qtractorFileListView.h
  qtractorFiles.h
  qtractorFileSystem.h
  qtractorInsertPlugin.h
  qtractorInstrument.h
  qtractorInstrumentMenu.h
  qtractorLadspaPlugin.h
  qtractorList.h
  qtractorLv2Plugin.h
  qtractorLv2Gtk2Plugin.h
  qtractorMessageBox.h
  qtractorMessageList.h
  qtractorMessages.h
  qtractorMeter.h
  qtractorMidiBuffer.h
  qtractorMidiClip.h
  qtractorMidiConnect.h
  qtractorMidiControl.h
  qtractorMidiControlCommand.h
  qtractorMidiControlObserver.h
  qtractorMidiControlTypeGroup.h
  qtractorMidiControlPluginWidget.h
  qtractorMidiControlPlugin.h
  qtractorMidiCursor.h
  qtractorMidiEditor.h
  qtractorMidiEditCommand.h
  qtractorMidiEditEvent.h
  qtractorMidiEditList.h
  qtractorMidiEditSelect.h
  qtractorMidiEditTime.h
  qtractorMidiEditView.h
  qtractorMidiEngine.h
  qtractorMidiEvent.h
  qtractorMidiEventList.h
  qtractorMidiFile.h
  qtractorMidiFileTempo.h
  qtractorMidiListView.h
  qtractorMidiManager.h
  qtractorMidiMeter.h
  qtractorMidiMonitor.h
  qtractorMidiRpn.h
  qtractorMidiSequence.h
  qtractorMidiSysex.h
  qtractorMidiThumbView.h
  qtractorMidiTimer.h
  qtractorMixer.h
  qtractorMmcEvent.h
  qtractorMonitor.h
  qtractorNsmClient.h
  qtractorObserver.h
  qtractorObserverWidget.h
  qtractorOptions.h
  qtractorPlugin.h
  qtractorPluginFactory.h
  qtractorPluginCommand.h
  qtractorPluginListView.h
  qtractorPropertyCommand.h
  qtractorRingBuffer.h
  qtractorRubberBand.h
  qtractorScrollView.h
  qtractorSession.h
  qtractorSessionCommand.h
  qtractorSessionCursor.h
  qtractorSpinBox.h
  qtractorThumbView.h
  qtractorTimeScale.h
  qtractorTimeScaleCommand.h
  qtractorTimeStretcher.h
  qtractorTrack.h
  qtractorTrackButton.h
  qtractorTrackCommand.h
  qtractorTrackList.h
  qtractorTrackTime.h
  qtractorTrackView.h
  qtractorTracks.h
  qtractorVst2Plugin.h
  qtractorVst3Plugin.h
  qtractorZipFile.h
  qtractorWsolaTimeStretcher.h
  qtractorAuxSendIOMatrixForm.h
  qtractorBusForm.h
  qtractorClipForm.h
  qtractorConnectForm.h
  qtractorEditRangeForm.h
  qtractorExportForm.h
  qtractorInstrumentForm.h
  qtractorMainForm.h
  qtractorMidiControlForm.h
  qtractorMidiControlObserverForm.h
  qtractorMidiEditorForm.h
  qtractorMidiSysexForm.h
  qtractorMidiToolsForm.h
  qtractorOptionsForm.h
  qtractorPaletteForm.h
  qtractorPasteRepeatForm.h
  qtractorPluginForm.h
  qtractorPluginSelectForm.h
  qtractorSessionForm.h
  qtractorShortcutForm.h
  qtractorTakeRangeForm.h
  qtractorTempoAdjustForm.h
  qtractorTimeScaleForm.h
  qtractorTrackForm.h
)

set (SOURCES
  qtractor.cpp
  qtractorActionControl.cpp
  qtractorAudioBuffer.cpp
  qtractorAudioClip.cpp
  qtractorAudioConnect.cpp
  qtractorAudioEngine.cpp
  qtractorAudioFile.cpp
  qtractorAudioListView.cpp
  qtractorAudioMadFile.cpp
  qtractorAudioMeter.cpp
  qtractorAudioMonitor.cpp
  qtractorAudioPeak.cpp
  qtractorAudioSndFile.cpp
  qtractorAudioVorbisFile.cpp
  qtractorClapPlugin.cpp
  qtractorClip.cpp
  qtractorClipCommand.cpp
  qtractorClipSelect.cpp
  qtractorComboBox.cpp
  qtractorCommand.cpp
  qtractorConnect.cpp
  qtractorConnections.cpp
  qtractorDocument.cpp
  qtractorCurve.cpp
  qtractorCurveCommand.cpp
  qtractorCurveFile.cpp
  qtractorCurveSelect.cpp
  qtractorDssiPlugin.cpp
  qtractorEngine.cpp
  qtractorEngineCommand.cpp
  qtractorFileList.cpp
  qtractorFileListView.cpp
  qtractorFiles.cpp
  qtractorFileSystem.cpp
  qtractorInsertPlugin.cpp
  qtractorInstrument.cpp
  qtractorInstrumentMenu.cpp
  qtractorLadspaPlugin.cpp
  qtractorLv2Plugin.cpp
  qtractorLv2Gtk2Plugin.cpp
  qtractorMessageBox.cpp
  qtractorMessageList.cpp
  qtractorMessages.cpp
  qtractorMeter.cpp
  qtractorMidiClip.cpp
  qtractorMidiConnect.cpp
  qtractorMidiControl.cpp
  qtractorMidiControlCommand.cpp
  qtractorMidiControlObserver.cpp
  qtractorMidiControlTypeGroup.cpp
  qtractorMidiControlPluginWidget.cpp
  qtractorMidiControlPlugin.cpp
  qtractorMidiCursor.cpp
  qtractorMidiEditor.cpp
  qtractorMidiEditCommand.cpp
  qtractorMidiEditEvent.cpp
  qtractorMidiEditList.cpp
  qtractorMidiEditSelect.cpp
  qtractorMidiEditTime.cpp
  qtractorMidiEditView.cpp
  qtractorMidiEngine.cpp
  qtractorMidiEventList.cpp
  qtractorMidiFile.cpp
  qtractorMidiFileTempo.cpp
  qtractorMidiListView.cpp
  qtractorMidiManager.cpp
  qtractorMidiMeter.cpp
  qtractorMidiMonitor.cpp
  qtractorMidiRpn.cpp
  qtractorMidiSequence.cpp
  qtractorMidiThumbView.cpp
  qtractorMidiTimer.cpp
  qtractorMixer.cpp
  qtractorMmcEvent.cpp
  qtractorNsmClient.cpp
  qtractorObserver.cpp
  qtractorObserverWidget.cpp
  qtractorOptions.cpp
  qtractorPlugin.cpp
  qtractorPluginFactory.cpp
  qtractorPluginCommand.cpp
  qtractorPluginListView.cpp
  qtractorRubberBand.cpp
  qtractorScrollView.cpp
  qtractorSession.cpp
  qtractorSessionCommand.cpp
  qtractorSessionCursor.cpp
  qtractorSpinBox.cpp
  qtractorThumbView.cpp
  qtractorTimeScale.cpp
  qtractorTimeScaleCommand.cpp
  qtractorTimeStretcher.cpp
  qtractorTrack.cpp
  qtractorTrackButton.cpp
  qtractorTrackCommand.cpp
  qtractorTrackList.cpp
  qtractorTrackTime.cpp
  qtractorTrackView.cpp
  qtractorTracks.cpp
  qtractorVst2Plugin.cpp
  qtractorVst3Plugin.cpp
  qtractorWsolaTimeStretcher.cpp
  qtractorZipFile.cpp
  qtractorAuxSendIOMatrixForm.cpp
  qtractorBusForm.cpp
  qtractorClipForm.cpp
  qtractorConnectForm.cpp
  qtractorEditRangeForm.cpp
  qtractorExportForm.cpp
  qtractorInstrumentForm.cpp
  qtractorMainForm.cpp
  qtractorMidiControlForm.cpp
  qtractorMidiControlObserverForm.cpp
  qtractorMidiEditorForm.cpp
  qtractorMidiSysexForm.cpp
  qtractorMidiToolsForm.cpp
  qtractorOptionsForm.cpp
  qtractorPaletteForm.cpp
  qtractorPasteRepeatForm.cpp
  qtractorPluginForm.cpp
  qtractorPluginSelectForm.cpp
  qtractorSessionForm.cpp
  qtractorShortcutForm.cpp
  qtractorTakeRangeForm.cpp
  qtractorTempoAdjustForm.cpp
  qtractorTimeScaleForm.cpp
  qtractorTrackForm.cpp
)

set (FORMS
  qtractorAuxSendIOMatrixForm.ui
  qtractorBusForm.ui
  qtractorClipForm.ui
  qtractorConnectForm.ui
  qtractorEditRangeForm.ui
  qtractorExportForm.ui
  qtractorInstrumentForm.ui
  qtractorMainForm.ui
  qtractorMidiControlForm.ui
  qtractorMidiControlObserverForm.ui
  qtractorMidiControlPluginWidget.ui
  qtractorMidiEditorForm.ui
  qtractorMidiSysexForm.ui
  qtractorMidiToolsForm.ui
  qtractorOptionsForm.ui
  qtractorPaletteForm.ui
  qtractorPasteRepeatForm.ui
  qtractorPluginForm.ui
  qtractorPluginSelectForm.ui
  qtractorSessionForm.ui
  qtractorShortcutForm.ui
  qtractorTakeRangeForm.ui
  qtractorTempoAdjustForm.ui
  qtractorTimeScaleForm.ui
  qtractorTrackForm.ui
)

set (RESOURCES
  qtractor.qrc
)

set (TRANSLATIONS
  translations/qtractor_cs.ts
  translations/qtractor_de.ts
  translations/qtractor_es.ts
  translations/qtractor_fr.ts
  translations/qtractor_it.ts
  translations/qtractor_ja.ts
  translations/qtractor_pt_BR.ts
  translations/qtractor_ru.ts
  translations/qtractor_uk.ts
)

if (QT_VERSION VERSION_LESS 5.15.0)
  qt5_add_translation (QM_FILES ${TRANSLATIONS})
else ()
  qt_add_translation (QM_FILES ${TRANSLATIONS})
endif ()

add_custom_target (translations ALL DEPENDS ${QM_FILES})

if (WIN32)
  set (RC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/win32/${PROJECT_NAME}.rc)
  set (RES_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.res.obj)
  find_program (WINDRES_EXECUTABLE NAMES windres mingw32-windres i686-mingw32-windres)
  if (MINGW)
    exec_program (${WINDRES_EXECUTABLE}
      ARGS "-i ${RC_FILE} -o ${RES_FILE} --include-dir=${CMAKE_CURRENT_SOURCE_DIR}/images")
    list (APPEND SOURCES ${RES_FILE})
  else ()
    list (APPEND SOURCES ${RC_FILE})
  endif ()
endif ()

if (APPLE)
  set (ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/images/${PROJECT_NAME}.icns)
  list (APPEND SOURCES ${ICON_FILE})
  set (MACOSX_BUNDLE_ICON_FILE ${PROJECT_NAME}.icns)
  set_source_files_properties (${ICON_FILE} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
endif ()

if (CONFIG_VST3SDK)
  add_definitions(-DNDEBUG)
  set (VST3SDK_SOURCES
    ${CONFIG_VST3SDK}/base/source/baseiids.cpp
  # ${CONFIG_VST3SDK}/base/source/fstring.cpp
    ${CONFIG_VST3SDK}/base/source/fobject.cpp
    ${CONFIG_VST3SDK}/base/thread/source/flock.cpp
    ${CONFIG_VST3SDK}/public.sdk/source/common/commoniids.cpp
    ${CONFIG_VST3SDK}/public.sdk/source/vst/vstinitiids.cpp
    ${CONFIG_VST3SDK}/pluginterfaces/base/coreiids.cpp
    ${CONFIG_VST3SDK}/pluginterfaces/base/funknown.cpp
  )
endif ()


add_executable (${PROJECT_NAME}
  ${HEADERS}
  ${SOURCES}
  ${FORMS}
  ${RESOURCES}
  ${VST3SDK_SOURCES}
)

# Add some debugger flags.
if (CONFIG_DEBUG AND UNIX AND NOT APPLE)
  set (CONFIG_DEBUG_OPTIONS -g -fsanitize=address -fno-omit-frame-pointer)
  target_compile_options (${PROJECT_NAME} PRIVATE ${CONFIG_DEBUG_OPTIONS})
  target_link_options (${PROJECT_NAME} PRIVATE ${CONFIG_DEBUG_OPTIONS})
endif ()

add_executable (${PROJECT_NAME}_plugin_scan qtractor_plugin_scan.cpp ${VST3SDK_SOURCES})

set_target_properties (${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties (${PROJECT_NAME}_plugin_scan PROPERTIES CXX_STANDARD 17)

if (WIN32)
  set_target_properties (${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE true)
  set_target_properties (${PROJECT_NAME}_plugin_scan PROPERTIES WIN32_EXECUTABLE true)
endif ()

if (APPLE)
  set_target_properties (${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE true)
  set_target_properties (${PROJECT_NAME}_plugin_scan PROPERTIES MACOSX_BUNDLE true)
endif ()

target_link_libraries (${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Xml Qt${QT_VERSION_MAJOR}::Svg)
target_link_libraries (${PROJECT_NAME}_plugin_scan PRIVATE Qt${QT_VERSION_MAJOR}::Core)

if (NOT CONFIG_QT6)
  target_link_libraries (${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::X11Extras)
endif ()

if (CONFIG_XUNIQUE)
  target_link_libraries (${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Network)
endif ()

if (UNIX AND NOT APPLE)
  if (DL_LIBRARY)
    target_link_libraries (${PROJECT_NAME} PRIVATE ${DL_LIBRARY})
    target_link_libraries (${PROJECT_NAME}_plugin_scan PRIVATE ${DL_LIBRARY})
  endif ()
endif ()

if (CONFIG_LIBJACK)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::JACK)
endif ()

if (CONFIG_LIBASOUND)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::ALSA)
endif ()

if (CONFIG_LIBSNDFILE)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::SNDFILE)
endif ()

if (CONFIG_LIBVORBIS)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::VORBIS)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::VORBISENC)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::VORBISFILE)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::OGG)
endif ()

if (CONFIG_LIBMAD)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::MAD)
endif ()

if (CONFIG_LIBSAMPLERATE)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::SAMPLERATE)
endif ()

if (CONFIG_LIBRUBBERBAND)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::RUBBERBAND)
endif ()

if (CONFIG_LIBAUBIO)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::AUBIO)
endif ()

if (CONFIG_LIBLO)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::LIBLO)
endif ()

if (CONFIG_LIBZ)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::ZLIB)
endif ()

if (CONFIG_LIBLILV)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::LILV)
endif ()

if (CONFIG_LIBSUIL)
  target_link_libraries (${PROJECT_NAME} PRIVATE PkgConfig::SUIL)
endif ()

if (CONFIG_LV2)
  target_include_directories (${PROJECT_NAME} PRIVATE ${LV2_INCLUDE_DIRS})
  target_link_directories (${PROJECT_NAME} PRIVATE ${LV2_LIBRARY_DIRS})
  target_link_libraries (${PROJECT_NAME} PRIVATE ${LV2_LIBRARIES})
  target_include_directories (${PROJECT_NAME}_plugin_scan PRIVATE ${LV2_INCLUDE_DIRS})
  target_link_directories (${PROJECT_NAME}_plugin_scan PRIVATE ${LV2_LIBRARY_DIRS})
  target_link_libraries (${PROJECT_NAME}_plugin_scan PRIVATE ${LV2_LIBRARIES})
endif ()

if (CONFIG_LV2_UI_GTK2)
  target_include_directories (${PROJECT_NAME} PRIVATE ${GTK2_INCLUDE_DIRS})
  target_link_directories (${PROJECT_NAME} PRIVATE ${GTK2_LIBRARY_DIRS})
  target_link_libraries (${PROJECT_NAME} PRIVATE ${GTK2_LIBRARIES})
endif ()

if (CONFIG_LV2_UI_GTKMM2)
  target_include_directories (${PROJECT_NAME} PRIVATE ${GTKMM2_INCLUDE_DIRS})
  target_link_directories (${PROJECT_NAME} PRIVATE ${GTKMM2_LIBRARY_DIRS})
  target_link_libraries (${PROJECT_NAME} PRIVATE ${GTKMM2_LIBRARIES})
endif ()

if (CONFIG_VST2 AND CONFIG_VST2SDK AND NOT CONFIG_VESTIGE)
  target_include_directories (${PROJECT_NAME} PRIVATE ${CONFIG_VST2SDK})
  target_include_directories (${PROJECT_NAME}_plugin_scan PRIVATE ${CONFIG_VST2SDK})
endif ()

if (CONFIG_VST3 AND CONFIG_VST3SDK)
  target_include_directories (${PROJECT_NAME} PRIVATE ${CONFIG_VST3SDK})
  target_include_directories (${PROJECT_NAME}_plugin_scan PRIVATE ${CONFIG_VST3SDK})
  if (PTHREAD_LIBRARY)
    target_link_libraries (${PROJECT_NAME} PRIVATE ${PTHREAD_LIBRARY})
    target_link_libraries (${PROJECT_NAME}_plugin_scan PRIVATE ${PTHREAD_LIBRARY})
  endif ()
endif ()

if (CONFIG_CLAP)
  target_include_directories (${PROJECT_NAME} PRIVATE clap/include)
  target_include_directories (${PROJECT_NAME}_plugin_scan PRIVATE clap/include)
endif ()

if (XCB_FOUND)
  target_include_directories (${PROJECT_NAME} PRIVATE ${XCB_INCLUDE_DIRS})
  target_link_directories (${PROJECT_NAME} PRIVATE ${XCB_LIBRARY_DIRS})
  target_link_libraries (${PROJECT_NAME} PRIVATE ${XCB_LIBRARIES})
endif ()


if (UNIX AND NOT APPLE)
  install (TARGETS ${PROJECT_NAME} RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR})
  install (TARGETS ${PROJECT_NAME}_plugin_scan RUNTIME
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})
  install (FILES ${QM_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/translations)
  install (FILES images/${PROJECT_NAME}.png
    RENAME org.rncbc.${PROJECT_NAME}.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps)
  install (FILES images/${PROJECT_NAME}.svg
    RENAME org.rncbc.${PROJECT_NAME}.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
  install (FILES appdata/org.rncbc.${PROJECT_NAME}.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install (FILES appdata/org.rncbc.${PROJECT_NAME}.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  install (FILES mimetypes/org.rncbc.${PROJECT_NAME}.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
  install (FILES mimetypes/org.rncbc.${PROJECT_NAME}.application-x-${PROJECT_NAME}-session.png
      mimetypes/org.rncbc.${PROJECT_NAME}.application-x-${PROJECT_NAME}-template.png
      mimetypes/org.rncbc.${PROJECT_NAME}.application-x-${PROJECT_NAME}-archive.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/mimetypes)
  install (FILES mimetypes/org.rncbc.${PROJECT_NAME}.application-x-${PROJECT_NAME}-session.svg
      mimetypes/org.rncbc.${PROJECT_NAME}.application-x-${PROJECT_NAME}-template.svg
      mimetypes/org.rncbc.${PROJECT_NAME}.application-x-${PROJECT_NAME}-archive.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/mimetypes)
  install (FILES man1/${PROJECT_NAME}.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
  install (FILES man1/${PROJECT_NAME}.fr.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/fr/man1 RENAME ${PROJECT_NAME}.1)
  install (FILES audio/metro_bar.wav audio/metro_beat.wav
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/audio)
  install (FILES instruments/Standard1.ins
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/instruments)
  install (FILES palette/KXStudio.conf palette/Wonton\ Soup.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/palette)
endif ()

if (WIN32)
  install (TARGETS ${PROJECT_NAME} RUNTIME
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  install (TARGETS ${PROJECT_NAME}_plugin_scan RUNTIME
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  install (FILES ${QM_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/translations)
endif ()

