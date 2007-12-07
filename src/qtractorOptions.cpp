// qtractorOptions.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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
// qtractorOptions - Prototype settings structure.
//

// Constructor.
qtractorOptions::qtractorOptions (void)
	: m_settings(QTRACTOR_DOMAIN, QTRACTOR_TITLE)
{
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
	m_settings.endGroup();

	// Transport options group.
	m_settings.beginGroup("/Transport");
	bMetronome       = m_settings.value("/Metronome", false).toBool();
	bFollowPlayhead  = m_settings.value("/FollowPlayhead", true).toBool();
	bContinuePastEnd = m_settings.value("/ContinuePastEnd", true).toBool();
	m_settings.endGroup();

	// Audio rendering options group.
	m_settings.beginGroup("/Audio");
	sAudioCaptureExt     = m_settings.value("/CaptureExt", AUDIO_DEFAULT_EXT).toString();
	iAudioCaptureType    = m_settings.value("/CaptureType", 0).toInt();
	iAudioCaptureFormat  = m_settings.value("/CaptureFormat", 0).toInt();
	iAudioCaptureQuality = m_settings.value("/CaptureQuality", 4).toInt();
	iAudioResampleType   = m_settings.value("/ResampleType", 2).toInt();
	bAudioQuickSeek      = m_settings.value("/QuickSeek", false).toBool();
	m_settings.endGroup();

	// MIDI rendering options group.
	m_settings.beginGroup("/Midi");
	iMidiCaptureFormat = m_settings.value("/CaptureFormat", 1).toInt();
	m_settings.endGroup();

	// Metronome options group.
	m_settings.beginGroup("/Metronome");
	iMetroChannel      = m_settings.value("/Channel", 9).toInt();
	iMetroBarNote      = m_settings.value("/BarNote", 76).toInt();
	iMetroBarVelocity  = m_settings.value("/BarVelocity", 96).toInt();
	iMetroBarDuration  = m_settings.value("/BarDuration", 24).toInt();
	iMetroBeatNote     = m_settings.value("/BeatNote", 77).toInt();
	iMetroBeatVelocity = m_settings.value("/BeatVelocity", 64).toInt();
	iMetroBeatDuration = m_settings.value("/BeatDuration", 16).toInt();
	m_settings.endGroup();

	m_settings.endGroup(); // Options group.

	// Last but not least, get the default directories.
	m_settings.beginGroup("/Default");
	sSessionDir    = m_settings.value("/SessionDir").toString();
	sAudioDir      = m_settings.value("/AudioDir").toString();
	sMidiDir       = m_settings.value("/MidiDir").toString();
	sInstrumentDir = m_settings.value("/InstrumentDir").toString();
	sPluginSearch  = m_settings.value("/PluginSearch").toString();
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
	m_settings.endGroup();

	// Transport options group.
	m_settings.beginGroup("/Transport");
	m_settings.setValue("/Metronome", bMetronome);
	m_settings.setValue("/FollowPlayhead", bFollowPlayhead);
	m_settings.setValue("/ContinuePastEnd", bContinuePastEnd);
	m_settings.endGroup();

	// Audio redndering options group.
	m_settings.beginGroup("/Audio");
	m_settings.setValue("/CaptureExt", sAudioCaptureExt);
	m_settings.setValue("/CaptureType", iAudioCaptureType);
	m_settings.setValue("/CaptureFormat", iAudioCaptureFormat);
	m_settings.setValue("/CaptureQuality", iAudioCaptureQuality);
	m_settings.setValue("/ResampleType", iAudioResampleType);
	m_settings.setValue("/QuickSeek", bAudioQuickSeek);
	m_settings.endGroup();

	// MIDI rendering options group.
	m_settings.beginGroup("/Midi");
	m_settings.setValue("/CaptureFormat", iMidiCaptureFormat);
	m_settings.endGroup();

	// Metronome options group.
	m_settings.beginGroup("/Metronome");
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
	m_settings.setValue("/SessionDir", sSessionDir);
	m_settings.setValue("/AudioDir", sAudioDir);
	m_settings.setValue("/MidiDir", sMidiDir);
	m_settings.setValue("/InstrumentDir", sInstrumentDir);
	m_settings.setValue("/PluginSearch", sPluginSearch);
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
	m_settings.endGroup();

	m_settings.endGroup();
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
		"  -?, --help\n\tShow help about command line options\n\n"
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

		if (sArg == "-?" || sArg == "--help") {
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
	// Add current text as latest item...
	const QString& sCurrentText = pComboBox->currentText();
	int iCount = pComboBox->count();
	for (int i = 0; i < iCount; i++) {
		const QString& sText = pComboBox->itemText(i);
		if (sText == sCurrentText) {
			pComboBox->removeItem(i);
			iCount--;
			break;
		}
	}
	while (iCount >= iLimit)
		pComboBox->removeItem(--iCount);
	pComboBox->insertItem(0, sCurrentText);
	iCount++;

	// Save combobox list to configuration settings file...
	m_settings.beginGroup("/History/" + pComboBox->objectName());
	for (int i = 0; i < iCount; i++) {
		const QString& sText = pComboBox->itemText(i);
		if (sText.isEmpty())
			break;
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


// end of qtractorOptions.cpp
