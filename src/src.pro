# qtractor.pro
#
NAME = qtractor

TARGET = $${NAME}
TEMPLATE = app

include(src.pri)

#DEFINES += DEBUG

HEADERS += config.h \
	qtractor.h \
	qtractorAbout.h \
	qtractorAtomic.h \
	qtractorActionControl.h \
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
	qtractorCurveSelect.h \
	qtractorDocument.h \
	qtractorDssiPlugin.h \
	qtractorEngine.h \
	qtractorEngineCommand.h \
	qtractorFifoBuffer.h \
	qtractorFileList.h \
	qtractorFileListView.h \
	qtractorFiles.h \
	qtractorFileSystem.h \
	qtractorInsertPlugin.h \
	qtractorInstrument.h \
	qtractorInstrumentMenu.h \
	qtractorLadspaPlugin.h \
	qtractorList.h \
	qtractorLv2Plugin.h \
	qtractorMessageBox.h \
	qtractorMessageList.h \
	qtractorMessages.h \
	qtractorMeter.h \
	qtractorMidiBuffer.h \
	qtractorMidiClip.h \
	qtractorMidiConnect.h \
	qtractorMidiControl.h \
	qtractorMidiControlCommand.h \
	qtractorMidiControlObserver.h \
	qtractorMidiControlTypeGroup.h \
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
	qtractorMidiEventList.h \
	qtractorMidiFile.h \
	qtractorMidiFileTempo.h \
	qtractorMidiListView.h \
	qtractorMidiManager.h \
	qtractorMidiMeter.h \
	qtractorMidiMonitor.h \
	qtractorMidiRpn.h \
	qtractorMidiSequence.h \
	qtractorMidiSysex.h \
	qtractorMidiThumbView.h \
	qtractorMidiTimer.h \
	qtractorMixer.h \
	qtractorMmcEvent.h \
	qtractorMonitor.h \
	qtractorNsmClient.h \
	qtractorObserver.h \
	qtractorObserverWidget.h \
	qtractorOptions.h \
	qtractorPlugin.h \
	qtractorPluginFactory.h \
	qtractorPluginCommand.h \
	qtractorPluginListView.h \
	qtractorPropertyCommand.h \
	qtractorRingBuffer.h \
	qtractorRubberBand.h \
	qtractorScrollView.h \
	qtractorSession.h \
	qtractorSessionCommand.h \
	qtractorSessionCursor.h \
	qtractorSpinBox.h \
	qtractorThumbView.h \
	qtractorTimeScale.h \
	qtractorTimeScaleCommand.h \
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
	qtractorWsolaTimeStretcher.h \
	qtractorBusForm.h \
	qtractorClipForm.h \
	qtractorConnectForm.h \
	qtractorEditRangeForm.h \
	qtractorExportForm.h \
	qtractorInstrumentForm.h \
	qtractorMainForm.h \
	qtractorMidiControlForm.h \
	qtractorMidiControlObserverForm.h \
	qtractorMidiEditorForm.h \
	qtractorMidiSysexForm.h \
	qtractorMidiToolsForm.h \
	qtractorOptionsForm.h \
	qtractorPaletteForm.h \
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
	qtractorActionControl.cpp \
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
	qtractorCurveSelect.cpp \
	qtractorDssiPlugin.cpp \
	qtractorEngine.cpp \
	qtractorEngineCommand.cpp \
	qtractorFileList.cpp \
	qtractorFileListView.cpp \
	qtractorFiles.cpp \
	qtractorFileSystem.cpp \
	qtractorInsertPlugin.cpp \
	qtractorInstrument.cpp \
	qtractorInstrumentMenu.cpp \
	qtractorLadspaPlugin.cpp \
	qtractorLv2Plugin.cpp \
	qtractorMessageBox.cpp \
	qtractorMessageList.cpp \
	qtractorMessages.cpp \
	qtractorMeter.cpp \
	qtractorMidiClip.cpp \
	qtractorMidiConnect.cpp \
	qtractorMidiControl.cpp \
	qtractorMidiControlCommand.cpp \
	qtractorMidiControlObserver.cpp \
	qtractorMidiControlTypeGroup.cpp \
	qtractorMidiCursor.cpp \
	qtractorMidiEditor.cpp \
	qtractorMidiEditCommand.cpp \
	qtractorMidiEditEvent.cpp \
	qtractorMidiEditList.cpp \
	qtractorMidiEditSelect.cpp \
	qtractorMidiEditTime.cpp \
	qtractorMidiEditView.cpp \
	qtractorMidiEngine.cpp \
	qtractorMidiEventList.cpp \
	qtractorMidiFile.cpp \
	qtractorMidiFileTempo.cpp \
	qtractorMidiListView.cpp \
	qtractorMidiManager.cpp \
	qtractorMidiMeter.cpp \
	qtractorMidiMonitor.cpp \
	qtractorMidiRpn.cpp \
	qtractorMidiSequence.cpp \
	qtractorMidiThumbView.cpp \
	qtractorMidiTimer.cpp \
	qtractorMixer.cpp \
	qtractorMmcEvent.cpp \
	qtractorNsmClient.cpp \
	qtractorObserver.cpp \
	qtractorObserverWidget.cpp \
	qtractorOptions.cpp \
	qtractorPlugin.cpp \
	qtractorPluginFactory.cpp \
	qtractorPluginCommand.cpp \
	qtractorPluginListView.cpp \
	qtractorRubberBand.cpp \
	qtractorScrollView.cpp \
	qtractorSession.cpp \
	qtractorSessionCommand.cpp \
	qtractorSessionCursor.cpp \
	qtractorSpinBox.cpp \
	qtractorThumbView.cpp \
	qtractorTimeScale.cpp \
	qtractorTimeScaleCommand.cpp \
	qtractorTimeStretcher.cpp \
	qtractorTrack.cpp \
	qtractorTrackButton.cpp \
	qtractorTrackCommand.cpp \
	qtractorTrackList.cpp \
	qtractorTrackTime.cpp \
	qtractorTrackView.cpp \
	qtractorTracks.cpp \
	qtractorVstPlugin.cpp \
	qtractorWsolaTimeStretcher.cpp \
	qtractorZipFile.cpp \
	qtractorBusForm.cpp \
	qtractorClipForm.cpp \
	qtractorConnectForm.cpp \
	qtractorEditRangeForm.cpp \
	qtractorExportForm.cpp \
	qtractorInstrumentForm.cpp \
	qtractorMainForm.cpp \
	qtractorMidiControlForm.cpp \
	qtractorMidiControlObserverForm.cpp \
	qtractorMidiEditorForm.cpp \
	qtractorMidiSysexForm.cpp \
	qtractorMidiToolsForm.cpp \
	qtractorOptionsForm.cpp \
	qtractorPaletteForm.cpp \
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
	qtractorEditRangeForm.ui \
	qtractorExportForm.ui \
	qtractorInstrumentForm.ui \
	qtractorMainForm.ui \
	qtractorMidiControlForm.ui \
	qtractorMidiControlObserverForm.ui \
	qtractorMidiEditorForm.ui \
	qtractorMidiSysexForm.ui \
	qtractorMidiToolsForm.ui \
	qtractorOptionsForm.ui \
	qtractorPaletteForm.ui \
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
	translations/qtractor_de.ts \
	translations/qtractor_es.ts \
	translations/qtractor_fr.ts \
	translations/qtractor_it.ts \
	translations/qtractor_ja.ts \
	translations/qtractor_pt.ts \
	translations/qtractor_ru.ts


unix {

	# variables
	OBJECTS_DIR = .obj
	MOC_DIR     = .moc
	UI_DIR      = .ui

	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}

	isEmpty(BINDIR) {
		BINDIR = $${PREFIX}/bin
	}

	isEmpty(DATADIR) {
		DATADIR = $${PREFIX}/share
	}

	#DEFINES += DATADIR=\"$${DATADIR}\"

	# make install
	INSTALLS += target desktop icon appdata \
		icon_scalable mimeinfo mimetypes mimetypes_scalable

	target.path = $${BINDIR}

	desktop.path = $${DATADIR}/applications
	desktop.files += $${NAME}.desktop

	icon.path = $${DATADIR}/icons/hicolor/32x32/apps
	icon.files += images/$${NAME}.png

	icon_scalable.path = $${DATADIR}/icons/hicolor/scalable/apps
	icon_scalable.files += images/$${NAME}.svg

	appdata.path = $${DATADIR}/metainfo
	appdata.files += appdata/$${NAME}.appdata.xml

	mimeinfo.path = $${DATADIR}/mime/packages
	mimeinfo.files += mimetypes/$${NAME}.xml

	mimetypes.path = $${DATADIR}/icons/hicolor/32x32/mimetypes
	mimetypes.files += \
		mimetypes/application-x-$${NAME}-session.png \
		mimetypes/application-x-$${NAME}-template.png \
		mimetypes/application-x-$${NAME}-archive.png

	mimetypes_scalable.path = $${DATADIR}/icons/hicolor/scalable/mimetypes
	mimetypes_scalable.files += \
		mimetypes/application-x-$${NAME}-session.svg \
		mimetypes/application-x-$${NAME}-template.svg \
		mimetypes/application-x-$${NAME}-archive.svg
}

QT += widgets xml
