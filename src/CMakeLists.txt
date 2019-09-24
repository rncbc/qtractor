set(NAME qtractor)

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

configure_file (cmake_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

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
  qtractorClip.h
  qtractorClipCommand.h
  qtractorClipSelect.h
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
  qtractorFifoBuffer.h
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
  qtractorVstPlugin.h
  qtractorZipFile.h
  qtractorWsolaTimeStretcher.h
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
  qtractorClip.cpp
  qtractorClipCommand.cpp
  qtractorClipSelect.cpp
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
  qtractorVstPlugin.cpp
  qtractorWsolaTimeStretcher.cpp
  qtractorZipFile.cpp
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
  qtractorBusForm.ui
  qtractorClipForm.ui
  qtractorConnectForm.ui
  qtractorEditRangeForm.ui
  qtractorExportForm.ui
  qtractorInstrumentForm.ui
  qtractorMainForm.ui
  qtractorMidiControlForm.ui
  qtractorMidiControlObserverForm.ui
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
  translations/qtractor_pt.ts
  translations/qtractor_ru.ts
)

qt5_add_translation ( QM_FILES ${TRANSLATIONS} )
add_custom_target( translations ALL DEPENDS ${QM_FILES} )

qt5_wrap_ui (UI_SOURCES ${FORMS})
qt5_wrap_cpp (MOC_SOURCES ${HEADERS})
qt5_add_resources (QRC_SOURCES ${RESOURCES})

if (WIN32)
  set (RC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/win32/${NAME}.rc)
  set (RES_FILE ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.res.obj)
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
  set (ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/images/${NAME}.icns)
  list (APPEND SOURCES ${ICON_FILE})
  set (MACOSX_BUNDLE_ICON_FILE ${NAME}.icns)
  set_source_files_properties (${ICON_FILE} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
endif ()


add_executable (${NAME}
  ${UI_SOURCES}
  ${MOC_SOURCES}
  ${QRC_SOURCES}
  ${SOURCES}
)

add_executable (${NAME}_plugin_scan qtractor_plugin_scan.cpp)

set_target_properties (${NAME} PROPERTIES CXX_STANDARD 11)
set_target_properties (${NAME}_plugin_scan PROPERTIES CXX_STANDARD 11)

if (WIN32)
  set_target_properties (${NAME} PROPERTIES WIN32_EXECUTABLE true)
  set_target_properties (${NAME}_plugin_scan PROPERTIES WIN32_EXECUTABLE true)
endif ()

if (APPLE)
  set_target_properties (${NAME} PROPERTIES MACOSX_BUNDLE true)
  set_target_properties (${NAME}_plugin_scan PROPERTIES MACOSX_BUNDLE true)
endif ()

target_link_libraries (${NAME} PRIVATE Qt5::Widgets Qt5::Xml)
target_link_libraries (${NAME}_plugin_scan PRIVATE Qt5::Core)

if (CONFIG_XUNIQUE)
  target_link_libraries (${NAME} PRIVATE Qt5::Network)
endif ()

if (CONFIG_LIBJACK)
  target_link_libraries (${NAME} PRIVATE ${JACK_LIBRARIES})
endif ()

if (CONFIG_LIBASOUND)
  target_link_libraries (${NAME} PRIVATE ${ALSA_LIBRARIES})
endif ()

if (CONFIG_LIBSNDFILE)
  target_link_libraries (${NAME} PRIVATE ${SNDFILE_LIBRARIES})
endif ()

if (CONFIG_LIBVORBIS)
  target_link_libraries (${NAME} PRIVATE ${VORBIS_LIBRARIES})
endif ()

if (CONFIG_LIBMAD)
  target_link_libraries (${NAME} PRIVATE ${MAD_LIBRARIES})
endif ()

if (CONFIG_LIBSAMPLERATE)
  target_link_libraries (${NAME} PRIVATE ${SAMPLERATE_LIBRARIES})
endif ()

if (CONFIG_LIBRUBBERBAND)
  target_link_libraries (${NAME} PRIVATE ${RUBBERBAND_LIBRARIES})
endif ()

if (CONFIG_LIBAUBIO)
  target_link_libraries (${NAME} PRIVATE ${AUBIO_LIBRARIES})
endif ()

if (CONFIG_LIBLO)
  target_link_libraries (${NAME} PRIVATE ${LIBLO_LIBRARIES})
endif ()

if (CONFIG_LIBZ)
  target_link_libraries (${NAME} PRIVATE ${ZLIB_LIBRARIES})
endif ()

if (CONFIG_LIBLILV)
  target_link_libraries (${NAME} PRIVATE ${LILV_LIBRARIES})
endif ()

if (CONFIG_LIBSUIL)
  target_link_libraries (${NAME} PRIVATE ${SUIL_LIBRARIES})
endif ()

if (CONFIG_VST)
  target_link_libraries (${NAME} PRIVATE Qt5::X11Extras)
endif ()


if (UNIX AND NOT APPLE)
  install (TARGETS ${NAME} RUNTIME
     DESTINATION ${CMAKE_INSTALL_BINDIR})
  install (TARGETS ${NAME}_plugin_scan RUNTIME
     DESTINATION ${CMAKE_INSTALL_LIBDIR}/${NAME})
  install (FILES ${QM_FILES}
     DESTINATION ${CMAKE_INSTALL_DATADIR}/${NAME}/translations)
  install (FILES ${NAME}.desktop
     DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install (FILES images/${NAME}.png
     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps)
  install (FILES appdata/${NAME}.appdata.xml
     DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  install (FILES mimetypes/${NAME}.xml
     DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
  install (FILES mimetypes/application-x-${NAME}-session.png
       mimetypes/application-x-${NAME}-template.png
       mimetypes/application-x-${NAME}-archive.png
     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/mimetypes)
  install (FILES mimetypes/application-x-${NAME}-session.svg
       mimetypes/application-x-${NAME}-template.svg
       mimetypes/application-x-${NAME}-archive.svg
     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/mimetypes)
endif ()

if (WIN32)
   install (TARGETS ${NAME} RUNTIME
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   install (TARGETS ${NAME}_plugin_scan RUNTIME
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   install (FILES ${QM_FILES}
     DESTINATION ${CMAKE_INSTALL_DATADIR}/${NAME}/translations)
endif ()

