# qtractor.pro
#
TARGET = qtractor

TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .

include(src.pri)

#DEFINES += DEBUG

HEADERS += config.h \
	qtractorAbout.h \
	qtractorAtomic.h \
	qtractorAudioBuffer.h \
	qtractorAudioClip.h \
	qtractorAudioConnect.h \
	qtractorAudioEngine.h \
	qtractorAudioFile.h \
	qtractorAudioListView.h \
	qtractorAudioMadFile.h \
	qtractorAudioMeter.h \
	qtractorAudioMonitor.h \
	qtractorAudioPeak.h \
	qtractorAudioSndFile.h \
	qtractorAudioVorbisFile.h \
	qtractorClip.h \
	qtractorClipCommand.h \
	qtractorClipFadeFunctor.h \
	qtractorClipSelect.h \
	qtractorCommand.h \
	qtractorConnect.h \
	qtractorConnections.h \
	qtractorCtlEvent.h \
	qtractorCurve.h \
	qtractorCurveCommand.h \
	qtractorCurveFile.h \
	qtractorDocument.h \
	qtractorDssiPlugin.h \
	qtractorEngine.h \
	qtractorEngineCommand.h \
	qtractorFifoBuffer.h \
	qtractorFileList.h \
	qtractorFileListView.h \
	qtractorFiles.h \
	qtractorInsertPlugin.h \
	qtractorInstrument.h \
	qtractorLadspaPlugin.h \
	qtractorList.h \
	qtractorLv2Plugin.h \
	qtractorMessages.h \
	qtractorMeter.h \
	qtractorMidiBuffer.h \
	qtractorMidiClip.h \
	qtractorMidiConnect.h \
	qtractorMidiControl.h \
	qtractorMidiControlCommand.h \
	qtractorMidiControlObserver.h \
	qtractorMidiCursor.h \
	qtractorMidiEditor.h \
	qtractorMidiEditCommand.h \
	qtractorMidiEditEvent.h \
	qtractorMidiEditList.h \
	qtractorMidiEditSelect.h \
	qtractorMidiEditTime.h \
	qtractorMidiEditView.h \
	qtractorMidiEngine.h \
	qtractorMidiEvent.h \
	qtractorMidiEventItemDelegate.h \
	qtractorMidiEventList.h \
	qtractorMidiEventListModel.h \
	qtractorMidiEventListView.h \
	qtractorMidiFile.h \
	qtractorMidiFileTempo.h \
	qtractorMidiListView.h \
	qtractorMidiMeter.h \
	qtractorMidiMonitor.h \
	qtractorMidiSequence.h \
	qtractorMidiSysex.h \
	qtractorMidiTimer.h \
	qtractorMixer.h \
	qtractorMmcEvent.h \
	qtractorMonitor.h \
	qtractorObserver.h \
	qtractorObserverWidget.h \
	qtractorOptions.h \
	qtractorPlugin.h \
	qtractorPluginCommand.h \
	qtractorPluginListView.h \
	qtractorPropertyCommand.h \
	qtractorRingBuffer.h \
	qtractorRubberBand.h \
	qtractorScrollView.h \
	qtractorSession.h \
	qtractorSessionCommand.h \
	qtractorSessionCursor.h \
	qtractorSessionDocument.h \
	qtractorSpinBox.h \
	qtractorThumbView.h \
	qtractorTimeScale.h \
	qtractorTimeScaleCommand.h \
	qtractorTimeStretch.h \
	qtractorTimeStretcher.h \
	qtractorTrack.h \
	qtractorTrackButton.h \
	qtractorTrackCommand.h \
	qtractorTrackList.h \
	qtractorTrackTime.h \
	qtractorTrackView.h \
	qtractorTracks.h \
	qtractorVstPlugin.h \
	qtractorZipFile.h \
	qtractorBusForm.h \
	qtractorClipForm.h \
	qtractorConnectForm.h \
	qtractorExportForm.h \
	qtractorInstrumentForm.h \
	qtractorMainForm.h \
	qtractorMidiControlForm.h \
	qtractorMidiControlObserverForm.h \
	qtractorMidiEditorForm.h \
	qtractorMidiSysexForm.h \
	qtractorMidiToolsForm.h \
	qtractorOptionsForm.h \
	qtractorPasteRepeatForm.h \
	qtractorPluginForm.h \
	qtractorPluginSelectForm.h \
	qtractorSessionForm.h \
	qtractorShortcutForm.h \
	qtractorTakeRangeForm.h \
	qtractorTempoAdjustForm.h \
	qtractorTimeScaleForm.h \
	qtractorTrackForm.h

SOURCES += \
	qtractor.cpp \
	qtractorAudioBuffer.cpp \
	qtractorAudioClip.cpp \
	qtractorAudioConnect.cpp \
	qtractorAudioEngine.cpp \
	qtractorAudioFile.cpp \
	qtractorAudioListView.cpp \
	qtractorAudioMadFile.cpp \
	qtractorAudioMeter.cpp \
	qtractorAudioMonitor.cpp \
	qtractorAudioPeak.cpp \
	qtractorAudioSndFile.cpp \
	qtractorAudioVorbisFile.cpp \
	qtractorClip.cpp \
	qtractorClipCommand.cpp \
	qtractorClipFadeFunctor.cpp \
	qtractorClipSelect.cpp \
	qtractorCommand.cpp \
	qtractorConnect.cpp \
	qtractorConnections.cpp \
	qtractorDocument.cpp \
	qtractorCurve.cpp \
	qtractorCurveCommand.cpp \
	qtractorCurveFile.cpp \
	qtractorDssiPlugin.cpp \
	qtractorEngine.cpp \
	qtractorEngineCommand.cpp \
	qtractorFileList.cpp \
	qtractorFileListView.cpp \
	qtractorFiles.cpp \
	qtractorInsertPlugin.cpp \
	qtractorInstrument.cpp \
	qtractorLadspaPlugin.cpp \
	qtractorLv2Plugin.cpp \
	qtractorMessages.cpp \
	qtractorMeter.cpp \
	qtractorMidiBuffer.cpp \
	qtractorMidiClip.cpp \
	qtractorMidiConnect.cpp \
	qtractorMidiControl.cpp \
	qtractorMidiControlCommand.cpp \
	qtractorMidiControlObserver.cpp \
	qtractorMidiCursor.cpp \
	qtractorMidiEditor.cpp \
	qtractorMidiEditCommand.cpp \
	qtractorMidiEditEvent.cpp \
	qtractorMidiEditList.cpp \
	qtractorMidiEditSelect.cpp \
	qtractorMidiEditTime.cpp \
	qtractorMidiEditView.cpp \
	qtractorMidiEngine.cpp \
	qtractorMidiEventItemDelegate.cpp \
	qtractorMidiEventList.cpp \
	qtractorMidiEventListModel.cpp \
	qtractorMidiEventListView.cpp \
	qtractorMidiFile.cpp \
	qtractorMidiFileTempo.cpp \
	qtractorMidiListView.cpp \
	qtractorMidiMeter.cpp \
	qtractorMidiMonitor.cpp \
	qtractorMidiSequence.cpp \
	qtractorMidiTimer.cpp \
	qtractorMixer.cpp \
	qtractorMmcEvent.cpp \
	qtractorObserver.cpp \
	qtractorObserverWidget.cpp \
	qtractorOptions.cpp \
	qtractorPlugin.cpp \
	qtractorPluginCommand.cpp \
	qtractorPluginListView.cpp \
	qtractorRubberBand.cpp \
	qtractorScrollView.cpp \
	qtractorSession.cpp \
	qtractorSessionCommand.cpp \
	qtractorSessionCursor.cpp \
	qtractorSessionDocument.cpp \
	qtractorSpinBox.cpp \
	qtractorThumbView.cpp \
	qtractorTimeScale.cpp \
	qtractorTimeScaleCommand.cpp \
	qtractorTimeStretch.cpp \
	qtractorTimeStretcher.cpp \
	qtractorTrack.cpp \
	qtractorTrackButton.cpp \
	qtractorTrackCommand.cpp \
	qtractorTrackList.cpp \
	qtractorTrackTime.cpp \
	qtractorTrackView.cpp \
	qtractorTracks.cpp \
	qtractorVstPlugin.cpp \
	qtractorZipFile.cpp \
	qtractorBusForm.cpp \
	qtractorClipForm.cpp \
	qtractorConnectForm.cpp \
	qtractorExportForm.cpp \
	qtractorInstrumentForm.cpp \
	qtractorMainForm.cpp \
	qtractorMidiControlForm.cpp \
	qtractorMidiControlObserverForm.cpp \
	qtractorMidiEditorForm.cpp \
	qtractorMidiSysexForm.cpp \
	qtractorMidiToolsForm.cpp \
	qtractorOptionsForm.cpp \
	qtractorPasteRepeatForm.cpp \
	qtractorPluginForm.cpp \
	qtractorPluginSelectForm.cpp \
	qtractorSessionForm.cpp \
	qtractorShortcutForm.cpp \
	qtractorTakeRangeForm.cpp \
	qtractorTempoAdjustForm.cpp \
	qtractorTimeScaleForm.cpp \
	qtractorTrackForm.cpp

FORMS += \
	qtractorBusForm.ui \
	qtractorClipForm.ui \
	qtractorConnectForm.ui \
	qtractorExportForm.ui \
	qtractorInstrumentForm.ui \
	qtractorMainForm.ui \
	qtractorMidiControlForm.ui \
	qtractorMidiControlObserverForm.ui \
	qtractorMidiEditorForm.ui \
	qtractorMidiSysexForm.ui \
	qtractorMidiToolsForm.ui \
	qtractorOptionsForm.ui \
	qtractorPasteRepeatForm.ui \
	qtractorPluginForm.ui \
	qtractorPluginSelectForm.ui \
	qtractorSessionForm.ui \
	qtractorShortcutForm.ui \
	qtractorTakeRangeForm.ui \
	qtractorTempoAdjustForm.ui \
	qtractorTimeScaleForm.ui \
	qtractorTrackForm.ui

RESOURCES += \
	qtractor.qrc

TRANSLATIONS += \
	translations/qtractor_cs.ts \
	translations/qtractor_fr.ts \
	translations/qtractor_it.ts \
	translations/qtractor_ru.ts

unix {

	# variables
	OBJECTS_DIR = .obj
	MOC_DIR     = .moc
	UI_DIR      = .ui

	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}

	BINDIR = $$PREFIX/bin
	DATADIR = $$PREFIX/share
	LOCALEDIR = $(localedir)

	DEFINES += DATADIR=\"$$DATADIR\"

	!isEmpty(LOCALEDIR) {
		DEFINES += LOCALEDIR=\"$$LOCALEDIR\"
	}

	# make install
	INSTALLS += target desktop icon

	target.path = $$BINDIR

	desktop.path = $$DATADIR/applications
	desktop.files += $${TARGET}.desktop

	icon.path = $$DATADIR/icons/hicolor/32x32/apps
	icon.files += images/$${TARGET}.png 
}

# XML/DOM support
QT += xml
