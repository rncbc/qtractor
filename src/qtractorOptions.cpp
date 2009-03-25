// qtractorOptions.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorOptions.h"

#include <QWidget>
#include <QComboBox>
#include <QSplitter>
#include <QAction>
#include <QList>

#include <QTextStream>


// Supposed to be determinant as default audio file type
// (effective only for capture/record)
#ifdef CONFIG_LIBVORBIS
#define AUDIO_DEFAULT_EXT "ogg"
#else
#define AUDIO_DEFAULT_EXT "wav"
#endif


//-------------------------------------------------------------------------
// qtractorOptions - Prototype settings structure (pseudo-singleton).
//

// Singleton instance pointer.
qtractorOptions *qtractorOptions::g_pOptions = NULL;

// Singleton instance accessor (static).
qtractorOptions *qtractorOptions::getInstance (void)
{
	return g_pOptions;
}


// Constructor.
qtractorOptions::qtractorOptions (void)
	: m_settings(QTRACTOR_DOMAIN, QTRACTOR_TITLE)
{
	// Pseudo-singleton reference setup.
	g_pOptions = this;

	// And go into general options group.
	m_settings.beginGroup("/Options");

	// Load display options...
	m_settings.beginGroup("/Display");
	sMessagesFont   = m_settings.value("/MessagesFont").toString();
	bMessagesLimit  = m_settings.value("/MessagesLimit", true).toBool();
	iMessagesLimitLines = m_settings.value("/MessagesLimitLines", 1000).toInt();
	bConfirmRemove  = m_settings.value("/ConfirmRemove", true).toBool();
	bStdoutCapture  = m_settings.value("/StdoutCapture", true).toBool();
	bCompletePath   = m_settings.value("/CompletePath", true).toBool();
	bPeakAutoRemove = m_settings.value("/PeakAutoRemove", true).toBool();
	bKeepToolsOnTop = m_settings.value("/KeepToolsOnTop", true).toBool();
	iDisplayFormat  = m_settings.value("/DisplayFormat", 1).toInt();
	iMaxRecentFiles = m_settings.value("/MaxRecentFiles", 5).toInt();
	iBaseFontSize   = m_settings.value("/BaseFontSize", 0).toInt();
	m_settings.endGroup();

	// Load logging options...
	m_settings.beginGroup("/Logging");
	bMessagesLog     = m_settings.value("/MessagesLog", false).toBool();
	sMessagesLogPath = m_settings.value("/MessagesLogPath", "qtractor.log").toString();
	m_settings.endGroup();

	// And go into view options group.
	m_settings.beginGroup("/View");
	bMenubar        = m_settings.value("/Menubar", true).toBool();
	bStatusbar      = m_settings.value("/Statusbar", true).toBool();
	bFileToolbar    = m_settings.value("/FileToolbar", true).toBool();
	bEditToolbar    = m_settings.value("/EditToolbar", true).toBool();
	bTrackToolbar   = m_settings.value("/TrackToolbar", true).toBool();
	bViewToolbar    = m_settings.value("/ViewToolbar", true).toBool();
	bOptionsToolbar = m_settings.value("/OptionsToolbar", true).toBool();
	bTransportToolbar = m_settings.value("/TransportToolbar", true).toBool();
	bTimeToolbar    = m_settings.value("/TimeToolbar", true).toBool();
	bThumbToolbar   = m_settings.value("/ThumbToolbar", true).toBool();
	iZoomMode       = m_settings.value("/ZoomMode", 1).toInt();
	m_settings.endGroup();

	// Transport options group.
	m_settings.beginGroup("/Transport");
	bMetronome       = m_settings.value("/Metronome", false).toBool();
	bFollowPlayhead  = m_settings.value("/FollowPlayhead", true).toBool();
	bAutoBackward    = m_settings.value("/AutoBackward", false).toBool();
	bContinuePastEnd = m_settings.value("/ContinuePastEnd", true).toBool();
	m_settings.endGroup();

	// Audio rendering options group.
	m_settings.beginGroup("/Audio");
	sAudioCaptureExt     = m_settings.value("/CaptureExt", AUDIO_DEFAULT_EXT).toString();
	iAudioCaptureType    = m_settings.value("/CaptureType", 0).toInt();
	iAudioCaptureFormat  = m_settings.value("/CaptureFormat", 0).toInt();
	iAudioCaptureQuality = m_settings.value("/CaptureQuality", 4).toInt();
	iAudioResampleType   = m_settings.value("/ResampleType", 2).toInt();
	bAudioAutoTimeStretch = m_settings.value("/AutoTimeStretch", true).toBool();
	bAudioTimeStretch    = m_settings.value("/TimeStretch", true).toBool();
	bAudioQuickSeek      = m_settings.value("/QuickSeek", false).toBool();
	bAudioPlayerBus      = m_settings.value("/PlayerBus", false).toBool();
	bAudioMetroBus       = m_settings.value("/MetroBus", false).toBool();
	bAudioMetronome      = m_settings.value("/Metronome", false).toBool();
	m_settings.endGroup();

	// MIDI rendering options group.
	m_settings.beginGroup("/Midi");
	iMidiCaptureFormat = m_settings.value("/CaptureFormat", 1).toInt();
	iMidiCaptureQuantize = m_settings.value("/CaptureQuantize", 0).toInt();
	bMidiControlBus    = m_settings.value("/ControlBus", false).toBool();
	bMidiMetroBus      = m_settings.value("/MetroBus", false).toBool();
	bMidiMetronome     = m_settings.value("/Metronome", true).toBool();
	m_settings.endGroup();

	// Metronome options group.
	m_settings.beginGroup("/Metronome");
	// Audio metronome...
	sMetroBarFilename  = m_settings.value("/BarFilename").toString();
	sMetroBeatFilename = m_settings.value("/BeatFilename").toString();
	// MIDI metronome...
	iMetroChannel      = m_settings.value("/Channel", 9).toInt();
	iMetroBarNote      = m_settings.value("/BarNote", 76).toInt();
	iMetroBarVelocity  = m_settings.value("/BarVelocity", 96).toInt();
	iMetroBarDuration  = m_settings.value("/BarDuration", 48).toInt();
	iMetroBeatNote     = m_settings.value("/BeatNote", 77).toInt();
	iMetroBeatVelocity = m_settings.value("/BeatVelocity", 64).toInt();
	iMetroBeatDuration = m_settings.value("/BeatDuration", 24).toInt();
	m_settings.endGroup();

	m_settings.endGroup(); // Options group.

	// Last but not least, get the defaults.
	m_settings.beginGroup("/Default");
	bSessionTemplate = m_settings.value("/SessionTemplate", false).toBool();
	sSessionTemplatePath = m_settings.value("/SessionTemplatePath").toString();
	sSessionDir    = m_settings.value("/SessionDir").toString();
	sAudioDir      = m_settings.value("/AudioDir").toString();
	sMidiDir       = m_settings.value("/MidiDir").toString();
	sPresetDir     = m_settings.value("/PresetDir").toString();
	sInstrumentDir = m_settings.value("/InstrumentDir").toString();
	bAutoMonitor   = m_settings.value("/AutoMonitor", true).toBool();
	iSnapPerBeat   = m_settings.value("/SnapPerBeat", 4).toInt();
	fTempo   = float(m_settings.value("/Tempo", 120.0).toDouble());
	iBeatsPerBar   = m_settings.value("/BeatsPerBar", 4).toInt();
	iBeatDivisor   = m_settings.value("/BeatDivisor", 2).toInt();
	iPasteRepeatCount = m_settings.value("/PasteRepeatCount", 2).toInt();
	bPasteRepeatPeriod = m_settings.value("/PasteRepeatPeriod", false).toInt();
	sPluginSearch  = m_settings.value("/PluginSearch").toString();
	iPluginType    = m_settings.value("/PluginType", 1).toInt();
	bPluginActivate = m_settings.value("/PluginActivate", false).toBool();
	m_settings.endGroup();

	// Plug-in paths.
	m_settings.beginGroup("/Plugins");
	ladspaPaths = m_settings.value("/LadspaPaths").toStringList();
	dssiPaths   = m_settings.value("/DssiPaths").toStringList();
	vstPaths    = m_settings.value("/VstPaths").toStringList();
	bAudioOutputBus = m_settings.value("/AudioOutputBus", false).toBool();
	bDummyVstScan = m_settings.value("/DummyVstScan", true).toBool();
	m_settings.endGroup();

	// Instrument file list.
	const QString sFilePrefix = "/File%1";
	int iFile = 0;
	instrumentFiles.clear();
	m_settings.beginGroup("/InstrumentFiles");
	for (;;) {
		QString sFilename = m_settings.value(
			sFilePrefix.arg(++iFile)).toString();
		if (sFilename.isEmpty())
		    break;
		instrumentFiles.append(sFilename);
	}
	m_settings.endGroup();

	// Recent file list.
	iFile = 0;
	recentFiles.clear();
	m_settings.beginGroup("/RecentFiles");
	while (iFile < iMaxRecentFiles) {
		QString sFilename = m_settings.value(
			sFilePrefix.arg(++iFile)).toString();
		if (!sFilename.isEmpty())
			recentFiles.append(sFilename);
	}
	m_settings.endGroup();

	// Tracks widget settings.
	m_settings.beginGroup("/Tracks");
	iTrackViewSelectMode = m_settings.value("/TrackViewSelectMode", 0).toInt();
	bTrackViewDropSpan   = m_settings.value("/TrackViewDropSpan", false).toBool();
	m_settings.endGroup();

	// MIDI options group.
	m_settings.beginGroup("/MidiEditor");

	m_settings.beginGroup("/View");
	bMidiMenubar     = m_settings.value("/Menubar", true).toBool();
	bMidiStatusbar   = m_settings.value("/Statusbar", true).toBool();
	bMidiFileToolbar = m_settings.value("/FileToolbar", true).toBool();
	bMidiEditToolbar = m_settings.value("/EditToolbar", true).toBool();
	bMidiViewToolbar = m_settings.value("/ViewToolbar", true).toBool();
	bMidiTransportToolbar = m_settings.value("/TransportToolbar", false).toBool();
	bMidiNoteDuration = m_settings.value("/NoteDuration", true).toBool();
	bMidiNoteColor   = m_settings.value("/NoteColor", false).toBool();
	bMidiValueColor  = m_settings.value("/ValueColor", false).toBool();
	bMidiPreview     = m_settings.value("/Preview", true).toBool();
	bMidiFollow      = m_settings.value("/Follow", false).toBool();
	bMidiEditMode    = m_settings.value("/EditMode", false).toBool();
	iMidiZoomMode    = m_settings.value("/ZoomMode", 3).toInt();
	iMidiHorizontalZoom = m_settings.value("/HorizontalZoom", 100).toInt();
	iMidiVerticalZoom = m_settings.value("/VerticalZoom", 100).toInt();
	m_settings.endGroup();

	// Meter colors.
	m_settings.beginGroup("/Colors");
	audioMeterColors = m_settings.value("/AudioMeter").toStringList();
	midiMeterColors  = m_settings.value("/MidiMeter").toStringList();
	m_settings.endGroup();

	m_settings.endGroup();
}


// Default Destructor.
qtractorOptions::~qtractorOptions (void)
{
	// Make program version available in the future.
	m_settings.beginGroup("/Program");
	m_settings.setValue("/Version", QTRACTOR_VERSION);
	m_settings.endGroup();

	// And go into general options group.
	m_settings.beginGroup("/Options");

	// Save display options.
	m_settings.beginGroup("/Display");
	m_settings.setValue("/MessagesFont", sMessagesFont);
	m_settings.setValue("/MessagesLimit", bMessagesLimit);
	m_settings.setValue("/MessagesLimitLines", iMessagesLimitLines);
	m_settings.setValue("/ConfirmRemove", bConfirmRemove);
	m_settings.setValue("/StdoutCapture", bStdoutCapture);
	m_settings.setValue("/CompletePath", bCompletePath);
	m_settings.setValue("/PeakAutoRemove", bPeakAutoRemove);
	m_settings.setValue("/KeepToolsOnTop", bKeepToolsOnTop);
	m_settings.setValue("/DisplayFormat", iDisplayFormat);
	m_settings.setValue("/MaxRecentFiles", iMaxRecentFiles);
	m_settings.setValue("/BaseFontSize", iBaseFontSize);
	m_settings.endGroup();

	// Save logging options...
	m_settings.beginGroup("/Logging");
	m_settings.setValue("/MessagesLog", bMessagesLog);
	m_settings.setValue("/MessagesLogPath", sMessagesLogPath);
	m_settings.endGroup();

	// View options group.
	m_settings.beginGroup("/View");
	m_settings.setValue("/Menubar", bMenubar);
	m_settings.setValue("/Statusbar", bStatusbar);
	m_settings.setValue("/FileToolbar", bFileToolbar);
	m_settings.setValue("/EditToolbar", bEditToolbar);
	m_settings.setValue("/TrackToolbar", bTrackToolbar);
	m_settings.setValue("/ViewToolbar", bViewToolbar);
	m_settings.setValue("/OptionsToolbar", bOptionsToolbar);
	m_settings.setValue("/TransportToolbar", bTransportToolbar);
	m_settings.setValue("/TimeToolbar", bTimeToolbar);
	m_settings.setValue("/ThumbToolbar", bThumbToolbar);
	m_settings.setValue("/ZoomMode", iZoomMode);
	m_settings.endGroup();

	// Transport options group.
	m_settings.beginGroup("/Transport");
	m_settings.setValue("/Metronome", bMetronome);
	m_settings.setValue("/FollowPlayhead", bFollowPlayhead);
	m_settings.setValue("/AutoBackward", bAutoBackward);
	m_settings.setValue("/ContinuePastEnd", bContinuePastEnd);
	m_settings.endGroup();

	// Audio redndering options group.
	m_settings.beginGroup("/Audio");
	m_settings.setValue("/CaptureExt", sAudioCaptureExt);
	m_settings.setValue("/CaptureType", iAudioCaptureType);
	m_settings.setValue("/CaptureFormat", iAudioCaptureFormat);
	m_settings.setValue("/CaptureQuality", iAudioCaptureQuality);
	m_settings.setValue("/ResampleType", iAudioResampleType);
	m_settings.setValue("/AutoTimeStretch", bAudioAutoTimeStretch);
	m_settings.setValue("/TimeStretch", bAudioTimeStretch);
	m_settings.setValue("/QuickSeek", bAudioQuickSeek);
	m_settings.setValue("/PlayerBus", bAudioPlayerBus);
	m_settings.setValue("/MetroBus", bAudioMetroBus);
	m_settings.setValue("/Metronome", bAudioMetronome);
	m_settings.endGroup();

	// MIDI rendering options group.
	m_settings.beginGroup("/Midi");
	m_settings.setValue("/CaptureFormat", iMidiCaptureFormat);
	m_settings.setValue("/CaptureQuantize", iMidiCaptureQuantize);
	m_settings.setValue("/ControlBus", bMidiControlBus);
	m_settings.setValue("/MetroBus", bMidiMetroBus);
	m_settings.setValue("/Metronome", bMidiMetronome);
	m_settings.endGroup();

	// Metronome options group.
	m_settings.beginGroup("/Metronome");
	// Audio metronome...
	m_settings.setValue("/BarFilename",  sMetroBarFilename);
	m_settings.setValue("/BeatFilename", sMetroBeatFilename);
	// MIDI metronome...
	m_settings.setValue("/Channel", iMetroChannel);
	m_settings.setValue("/BarNote", iMetroBarNote);
	m_settings.setValue("/BarVelocity", iMetroBarVelocity);
	m_settings.setValue("/BarDuration", iMetroBarDuration);
	m_settings.setValue("/BeatNote", iMetroBeatNote);
	m_settings.setValue("/BeatVelocity", iMetroBeatVelocity);
	m_settings.setValue("/BeatDuration", iMetroBeatDuration);
	m_settings.endGroup();

	m_settings.endGroup(); // Options group.

	// Default directories.
	m_settings.beginGroup("/Default");
	m_settings.setValue("/SessionTemplate", bSessionTemplate);
	m_settings.setValue("/SessionTemplatePath", sSessionTemplatePath);
	m_settings.setValue("/SessionDir", sSessionDir);
	m_settings.setValue("/AudioDir", sAudioDir);
	m_settings.setValue("/MidiDir", sMidiDir);
	m_settings.setValue("/PresetDir", sPresetDir);
	m_settings.setValue("/InstrumentDir", sInstrumentDir);
	m_settings.setValue("/AutoMonitor", bAutoMonitor);
	m_settings.setValue("/SnapPerBeat", iSnapPerBeat);
	m_settings.setValue("/Tempo", double(fTempo));
	m_settings.setValue("/BeatsPerBar", iBeatsPerBar);
	m_settings.setValue("/BeatDivisor", iBeatDivisor);
	m_settings.setValue("/PasteRepeatCount", iPasteRepeatCount);
	m_settings.setValue("/PasteRepeatPeriod", bPasteRepeatPeriod);
	m_settings.setValue("/PluginSearch", sPluginSearch);
	m_settings.setValue("/PluginType", iPluginType);
	m_settings.setValue("/PluginActivate", bPluginActivate);
	m_settings.endGroup();

	// Plug-in paths.
	m_settings.beginGroup("/Plugins");
	m_settings.setValue("/LadspaPaths", ladspaPaths);
	m_settings.setValue("/DssiPaths", dssiPaths);
	m_settings.setValue("/VstPaths", vstPaths);
	m_settings.setValue("/AudioOutputBus", bAudioOutputBus);
	m_settings.setValue("/DummyVstScan", bDummyVstScan);
	m_settings.endGroup();

	// Instrument file list.
	const QString sFilePrefix = "/File%1";
	int iFile = 0;
	m_settings.beginGroup("/InstrumentFiles");
	QStringListIterator iter1(instrumentFiles);
    while (iter1.hasNext())
		m_settings.setValue(sFilePrefix.arg(++iFile), iter1.next());
    // Cleanup old entries, if any...
    while (!m_settings.value(sFilePrefix.arg(++iFile)).isNull())
        m_settings.remove(sFilePrefix.arg(iFile));
	m_settings.endGroup();

	// Recent file list.
	iFile = 0;
	m_settings.beginGroup("/RecentFiles");
	QStringListIterator iter2(recentFiles);
    while (iter2.hasNext())
		m_settings.setValue(sFilePrefix.arg(++iFile), iter2.next());
	m_settings.endGroup();

	// Tracks widget settings.
	m_settings.beginGroup("/Tracks");
	m_settings.setValue("/TrackViewSelectMode", iTrackViewSelectMode);
	m_settings.setValue("/TrackViewDropSpan", bTrackViewDropSpan);
	m_settings.endGroup();

	// MIDI Editor options group.
	m_settings.beginGroup("/MidiEditor");

	m_settings.beginGroup("/View");
	m_settings.setValue("/Menubar", bMidiMenubar);
	m_settings.setValue("/Statusbar", bMidiStatusbar);
	m_settings.setValue("/FileToolbar", bMidiFileToolbar);
	m_settings.setValue("/EditToolbar", bMidiEditToolbar);
	m_settings.setValue("/ViewToolbar", bMidiViewToolbar);
	m_settings.setValue("/TransportToolbar", bMidiTransportToolbar);
	m_settings.setValue("/NoteDuration", bMidiNoteDuration);
	m_settings.setValue("/NoteColor", bMidiNoteColor);
	m_settings.setValue("/ValueColor", bMidiValueColor);
	m_settings.setValue("/Preview", bMidiPreview);
	m_settings.setValue("/Follow", bMidiFollow);
	m_settings.setValue("/EditMode", bMidiEditMode);
	m_settings.setValue("/ZoomMode", iMidiZoomMode);
	m_settings.setValue("/HorizontalZoom", iMidiHorizontalZoom);
	m_settings.setValue("/VerticalZoom", iMidiVerticalZoom);
	m_settings.endGroup();

	// Meter colors.
	m_settings.beginGroup("/Colors");
	m_settings.setValue("/AudioMeter", audioMeterColors);
	m_settings.setValue("/MidiMeter", midiMeterColors);
	m_settings.endGroup();

	m_settings.endGroup();

	// Pseudo-singleton reference shut-down.
	g_pOptions = NULL;
}


//-------------------------------------------------------------------------
// Settings accessor.
//

QSettings& qtractorOptions::settings (void)
{
	return m_settings;
}


//-------------------------------------------------------------------------
// Command-line argument stuff.
//

// Help about command line options.
void qtractorOptions::print_usage ( const char *arg0 )
{
	QTextStream out(stderr);
	out << QObject::tr(
		"Usage: %1 [options] [session-file]\n\n"
		QTRACTOR_TITLE " - " QTRACTOR_SUBTITLE "\n\n"
		"Options:\n\n"
		"  -h, --help\n\tShow help about command line options\n\n"
		"  -v, --version\n\tShow version information\n\n")
		.arg(arg0);
}


// Parse command line arguments into m_settings.
bool qtractorOptions::parse_args ( int argc, char **argv )
{
	QTextStream out(stderr);
	const QString sEol = "\n\n";
	int iCmdArgs = 0;

	for (int i = 1; i < argc; i++) {

		if (iCmdArgs > 0) {
			sSessionFile += ' ';
			sSessionFile += argv[i];
			iCmdArgs++;
			continue;
		}

		QString sVal;
		QString sArg = argv[i];
		int iEqual = sArg.indexOf('=');
		if (iEqual >= 0) {
			sVal = sArg.right(sArg.length() - iEqual - 1);
			sArg = sArg.left(iEqual);
		}
		else if (i < argc)
			sVal = argv[i + 1];

		if (sArg == "-h" || sArg == "--help") {
			print_usage(argv[0]);
			return false;
		}
		else if (sArg == "-v" || sArg == "--version") {
			out << QObject::tr("Qt: %1\n").arg(qVersion());
			out << QObject::tr(QTRACTOR_TITLE ": %1\n").arg(QTRACTOR_VERSION);
			return false;
		}
		else {
			// If we don't have one by now,
			// this will be the startup sesion file...
			sSessionFile += sArg;
			iCmdArgs++;
		}
	}

	// Alright with argument parsing.
	return true;
}


//---------------------------------------------------------------------------
// Widget geometry persistence helper methods.

void qtractorOptions::loadWidgetGeometry ( QWidget *pWidget )
{
	// Try to restore old form window positioning.
	if (pWidget) {
		QPoint fpos;
		QSize  fsize;
		bool bVisible;
		m_settings.beginGroup("/Geometry/" + pWidget->objectName());
		fpos.setX(m_settings.value("/x", -1).toInt());
		fpos.setY(m_settings.value("/y", -1).toInt());
		fsize.setWidth(m_settings.value("/width", -1).toInt());
		fsize.setHeight(m_settings.value("/height", -1).toInt());
		bVisible = m_settings.value("/visible", false).toBool();
		m_settings.endGroup();
		if (fpos.x() > 0 && fpos.y() > 0)
			pWidget->move(fpos);
		if (fsize.width() > 0 && fsize.height() > 0)
			pWidget->resize(fsize);
		else
			pWidget->adjustSize();
		if (bVisible)
			pWidget->show();
		else
			pWidget->hide();
	}
}


void qtractorOptions::saveWidgetGeometry ( QWidget *pWidget )
{
	// Try to save form window position...
	// (due to X11 window managers ideossincrasies, we better
	// only save the form geometry while its up and visible)
	if (pWidget) {
		m_settings.beginGroup("/Geometry/" + pWidget->objectName());
		bool bVisible = pWidget->isVisible();
		const QPoint& fpos  = pWidget->pos();
		const QSize&  fsize = pWidget->size();
		m_settings.setValue("/x", fpos.x());
		m_settings.setValue("/y", fpos.y());
		m_settings.setValue("/width", fsize.width());
		m_settings.setValue("/height", fsize.height());
		m_settings.setValue("/visible", bVisible);
		m_settings.endGroup();
	}
}


//---------------------------------------------------------------------------
// Combo box history persistence helper implementation.

void qtractorOptions::loadComboBoxHistory ( QComboBox *pComboBox, int iLimit )
{
	// Load combobox list from configuration settings file...
	m_settings.beginGroup("/History/" + pComboBox->objectName());

	if (m_settings.childKeys().count() > 0) {
		pComboBox->setUpdatesEnabled(false);
		pComboBox->setDuplicatesEnabled(false);
		pComboBox->clear();
		for (int i = 0; i < iLimit; i++) {
			const QString& sText = m_settings.value(
				"/Item" + QString::number(i + 1)).toString();
			if (sText.isEmpty())
				break;
			pComboBox->addItem(sText);
		}
		pComboBox->setUpdatesEnabled(true);
	}

	m_settings.endGroup();
}


void qtractorOptions::saveComboBoxHistory ( QComboBox *pComboBox, int iLimit )
{
	int iCount = pComboBox->count();

	// Add current text as latest item...
	const QString& sCurrentText = pComboBox->currentText();
	if (!sCurrentText.isEmpty()) {
		int i = pComboBox->findText(sCurrentText);
		if (i >= 0) {
			pComboBox->removeItem(i);
			iCount--;
		}
		pComboBox->insertItem(0, sCurrentText);
		iCount++;
	}

	// Take care of item limit...
	while (iCount > iLimit)
		pComboBox->removeItem(--iCount);

	// Save combobox list to configuration settings file...
	m_settings.beginGroup("/History/" + pComboBox->objectName());
	for (int i = 0; i < iCount; i++) {
		const QString& sText = pComboBox->itemText(i);
		if (sText.isEmpty())
			continue;
		m_settings.setValue("/Item" + QString::number(i + 1), sText);
	}
	m_settings.endGroup();
}


//---------------------------------------------------------------------------
// Splitter widget sizes persistence helper methods.

void qtractorOptions::loadSplitterSizes ( QSplitter *pSplitter,
	QList<int>& sizes )
{
	// Try to restore old splitter sizes...
	if (pSplitter) {
		m_settings.beginGroup("/Splitter/" + pSplitter->objectName());
		QStringList list = m_settings.value("/sizes").toStringList();
		if (!list.isEmpty()) {
			sizes.clear();
			QStringListIterator iter(list);
			while (iter.hasNext())
				sizes.append(iter.next().toInt());
		}
		pSplitter->setSizes(sizes);
		m_settings.endGroup();
	}
}


void qtractorOptions::saveSplitterSizes ( QSplitter *pSplitter )
{
	// Try to save current splitter sizes...
	if (pSplitter) {
		m_settings.beginGroup("/Splitter/" + pSplitter->objectName());
		QStringList list;
		QList<int> sizes = pSplitter->sizes();
		QListIterator<int> iter(sizes);
		while (iter.hasNext())
			list.append(QString::number(iter.next()));
		if (!list.isEmpty())
			m_settings.setValue("/sizes", list);
		m_settings.endGroup();
	}
}


//---------------------------------------------------------------------------
// Action shortcut persistence helper methos.
void qtractorOptions::loadActionShortcuts ( QObject *pObject )
{
	m_settings.beginGroup("/Shortcuts/" + pObject->objectName());

	QList<QAction *> actions = pObject->findChildren<QAction *> ();
	QListIterator<QAction *> iter(actions);
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		if (pAction->objectName().isEmpty())
			continue;
		const QString& sKey = '/' + pAction->objectName();
		const QString& sValue = m_settings.value('/' + sKey).toString();
		if (sValue.isEmpty())
			continue;
		pAction->setShortcut(QKeySequence(sValue));
	}

	m_settings.endGroup();
}


void qtractorOptions::saveActionShortcuts ( QObject *pObject )
{
	m_settings.beginGroup("/Shortcuts/" + pObject->objectName());

	QList<QAction *> actions = pObject->findChildren<QAction *> ();
	QListIterator<QAction *> iter(actions);
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		if (pAction->objectName().isEmpty())
			continue;
		const QString& sKey = '/' + pAction->objectName();
		const QString& sValue = QString(pAction->shortcut());
		if (!sValue.isEmpty())
			m_settings.setValue(sKey, sValue);
		else
		if (m_settings.contains(sKey))
			m_settings.remove(sKey);
	}

	m_settings.endGroup();
}


// end of qtractorOptions.cpp
