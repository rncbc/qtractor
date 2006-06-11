// qtractorOptions.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorOptions.h"

#include <qwidget.h>
#include <qcombobox.h>
#include <qsplitter.h>


//-------------------------------------------------------------------------
// qtractorOptions - Prototype settings structure.
//

// Constructor.
qtractorOptions::qtractorOptions (void)
{
	// Begin master key group.
	m_settings.beginGroup("/qtractor");

	// And go into general options group.
	m_settings.beginGroup("/Options");

	// Load display options...
	m_settings.beginGroup("/Display");
	sMessagesFont   = m_settings.readEntry("/MessagesFont", QString::null);
	bMessagesLimit  = m_settings.readBoolEntry("/MessagesLimit", true);
	iMessagesLimitLines = m_settings.readNumEntry("/MessagesLimitLines", 1000);
	bConfirmRemove  = m_settings.readBoolEntry("/ConfirmRemove", true);
	bStdoutCapture  = m_settings.readBoolEntry("/StdoutCapture", true);
	bCompletePath   = m_settings.readBoolEntry("/CompletePath", true);
	bPeakAutoRemove = m_settings.readBoolEntry("/PeakAutoRemove", true);
	bTransportTime  = m_settings.readBoolEntry("/TransportTime", true);
	iMaxRecentFiles = m_settings.readNumEntry("/MaxRecentFiles", 5);
	m_settings.endGroup();

	// And go into view options group.
	m_settings.beginGroup("/View");
	bMenubar        = m_settings.readBoolEntry("/Menubar", true);
	bStatusbar      = m_settings.readBoolEntry("/Statusbar", true);
	bFileToolbar    = m_settings.readBoolEntry("/FileToolbar", true);
	bEditToolbar    = m_settings.readBoolEntry("/EditToolbar", true);
	bTrackToolbar   = m_settings.readBoolEntry("/TrackToolbar", true);
	bViewToolbar    = m_settings.readBoolEntry("/ViewToolbar", true);
	bTransportToolbar = m_settings.readBoolEntry("/TransportToolbar", true);
	bTimeToolbar    = m_settings.readBoolEntry("/TimeToolbar", true);
	m_settings.endGroup();

	// Transport options group.
	m_settings.beginGroup("/Transport");
	bFollowPlayhead = m_settings.readBoolEntry("/FollowPlayhead", true);
	m_settings.endGroup();

	m_settings.endGroup(); // Options group.

	// Last but not least, get the default directories.
	m_settings.beginGroup("/Default");
	sSessionDir = m_settings.readEntry("/SessionDir", QString::null);
	sAudioDir   = m_settings.readEntry("/AudioDir", QString::null);
	sMidiDir    = m_settings.readEntry("/MidiDir", QString::null);
	sInstrumentDir = m_settings.readEntry("/InstrumentDir", QString::null);
	m_settings.endGroup();

	// Instrument file list.
	const QString sFilePrefix = "/File";
	int iFile = 0;
	instrumentFiles.clear();
	m_settings.beginGroup("/InstrumentFiles");
	for (;;) {
		QString sFilename = m_settings.readEntry(
			sFilePrefix + QString::number(++iFile), QString::null);
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
		QString sFilename = m_settings.readEntry(
			sFilePrefix + QString::number(++iFile), QString::null);
		if (!sFilename.isEmpty())
			recentFiles.append(sFilename);
	}
	m_settings.endGroup();

	// Tracks widget settings.
	m_settings.beginGroup("/Tracks");
	iTrackListWidth = m_settings.readNumEntry("/TrackListWidth", 160);
	m_settings.endGroup();
}


// Default Destructor.
qtractorOptions::~qtractorOptions (void)
{
	// Make program version available in the future.
	m_settings.beginGroup("/Program");
	m_settings.writeEntry("/Version", QTRACTOR_VERSION);
	m_settings.endGroup();

	// And go into general options group.
	m_settings.beginGroup("/Options");

	// Save display options.
	m_settings.beginGroup("/Display");
	m_settings.writeEntry("/MessagesFont", sMessagesFont);
	m_settings.writeEntry("/MessagesLimit", bMessagesLimit);
	m_settings.writeEntry("/MessagesLimitLines", iMessagesLimitLines);
	m_settings.writeEntry("/ConfirmRemove", bStdoutCapture);
	m_settings.writeEntry("/StdoutCapture", bStdoutCapture);
	m_settings.writeEntry("/CompletePath", bCompletePath);
	m_settings.writeEntry("/PeakAutoRemove", bPeakAutoRemove);
	m_settings.writeEntry("/TransportTime", bTransportTime);
	m_settings.writeEntry("/MaxRecentFiles", iMaxRecentFiles);
	m_settings.endGroup();

	// View options group.
	m_settings.beginGroup("/View");
	m_settings.writeEntry("/Menubar", bMenubar);
	m_settings.writeEntry("/Statusbar", bStatusbar);
	m_settings.writeEntry("/FileToolbar", bFileToolbar);
	m_settings.writeEntry("/EditToolbar", bEditToolbar);
	m_settings.writeEntry("/TrackToolbar", bTrackToolbar);
	m_settings.writeEntry("/ViewToolbar", bViewToolbar);
	m_settings.writeEntry("/TransportToolbar", bTransportToolbar);
	m_settings.writeEntry("/TimeToolbar", bTimeToolbar);
	m_settings.endGroup();

	// Transport options group.
	m_settings.beginGroup("/Transport");
	m_settings.writeEntry("/FollowPlayhead", bFollowPlayhead);
	m_settings.endGroup();

	m_settings.endGroup(); // Options group.

	// Default directories.
	m_settings.beginGroup("/Default");
	m_settings.writeEntry("/SessionDir", sSessionDir);
	m_settings.writeEntry("/AudioDir", sAudioDir);
	m_settings.writeEntry("/MidiDir", sMidiDir);
	m_settings.writeEntry("/InstrumentDir", sInstrumentDir);
	m_settings.endGroup();

	// Instrument file list.
	const QString sFilePrefix = "/File";
	QStringList::Iterator iter;
	int iFile = 0;
	m_settings.beginGroup("/InstrumentFiles");
    for (iter = instrumentFiles.begin(); iter != instrumentFiles.end(); iter++)
		m_settings.writeEntry(sFilePrefix + QString::number(++iFile), *iter);
    // Cleanup old entries, if any...
    while (!m_settings.readEntry(sFilePrefix + QString::number(++iFile)).isEmpty())
        m_settings.removeEntry(sFilePrefix + QString::number(iFile));
	m_settings.endGroup();

	// Recent file list.
	iFile = 0;
	m_settings.beginGroup("/RecentFiles");
    for (iter = recentFiles.begin(); iter != recentFiles.end(); iter++)
		m_settings.writeEntry(sFilePrefix + QString::number(++iFile), *iter);
	m_settings.endGroup();

	// Tracks widget settings.
	m_settings.beginGroup("/Tracks");
	m_settings.writeEntry("/TrackListWidth", iTrackListWidth);
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
	const QString sEot = "\n\t";
	const QString sEol = "\n\n";

	fprintf(stderr, QObject::tr("Usage") + ": %s [" + QObject::tr("options") + "] [" +
		QObject::tr("session-file") + "]" + sEol, arg0);
	fprintf(stderr, QTRACTOR_TITLE " - " + QObject::tr(QTRACTOR_SUBTITLE) + sEol);
	fprintf(stderr, QObject::tr("Options") + ":" + sEol);
	fprintf(stderr, "  -?, --help" + sEot +
		QObject::tr("Show help about command line options") + sEol);
	fprintf(stderr, "  -v, --version" + sEot +
		QObject::tr("Show version information") + sEol);
}


// Parse command line arguments into m_settings.
bool qtractorOptions::parse_args ( int argc, char **argv )
{
	const QString sEol = "\n\n";
	int iCmdArgs = 0;

	for (int i = 1; i < argc; i++) {

		if (iCmdArgs > 0) {
			sSessionFile += " ";
			sSessionFile += argv[i];
			iCmdArgs++;
			continue;
		}

		QString sArg = argv[i];
		QString sVal = QString::null;
		int iEqual = sArg.find("=");
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
			fprintf(stderr, "Qt: %s\n", qVersion());
			fprintf(stderr, "qtractor: %s\n", QTRACTOR_VERSION);
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
		m_settings.beginGroup("/Geometry/" + QString(pWidget->name()));
		fpos.setX(m_settings.readNumEntry("/x", -1));
		fpos.setY(m_settings.readNumEntry("/y", -1));
		fsize.setWidth(m_settings.readNumEntry("/width", -1));
		fsize.setHeight(m_settings.readNumEntry("/height", -1));
		bVisible = m_settings.readBoolEntry("/visible", false);
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
		m_settings.beginGroup("/Geometry/" + QString(pWidget->name()));
		bool bVisible = pWidget->isVisible();
		if (bVisible) {
			QPoint fpos  = pWidget->pos();
			QSize  fsize = pWidget->size();
			m_settings.writeEntry("/x", fpos.x());
			m_settings.writeEntry("/y", fpos.y());
			m_settings.writeEntry("/width", fsize.width());
			m_settings.writeEntry("/height", fsize.height());
		}
		m_settings.writeEntry("/visible", bVisible);
		m_settings.endGroup();
	}
}


//---------------------------------------------------------------------------
// Combo box history persistence helper implementation.

void qtractorOptions::loadComboBoxHistory ( QComboBox *pComboBox, int iLimit )
{
	pComboBox->setUpdatesEnabled(false);
	pComboBox->setDuplicatesEnabled(false);
	pComboBox->clear();

	// Load combobox list from configuration settings file...
	m_settings.beginGroup("/History/" + QString(pComboBox->name()));
	for (int i = 0; i < iLimit; i++) {
		const QString& sText = m_settings.readEntry(
			"/Item" + QString::number(i + 1), QString::null);
		if (sText.isEmpty())
			break;
		pComboBox->insertItem(sText);
	}
	m_settings.endGroup();

	pComboBox->setUpdatesEnabled(true);
}


void qtractorOptions::saveComboBoxHistory ( QComboBox *pComboBox, int iLimit )
{
	// Add current text as latest item...
	const QString& sCurrentText = pComboBox->currentText();
	int iCount = pComboBox->count();
	for (int i = 0; i < iCount; i++) {
		const QString& sText = pComboBox->text(i);
		if (sText == sCurrentText) {
			pComboBox->removeItem(i);
			iCount--;
			break;
		}
	}
	while (iCount >= iLimit)
		pComboBox->removeItem(--iCount);
	pComboBox->insertItem(sCurrentText, 0);
	iCount++;

	// Save combobox list to configuration settings file...
	m_settings.beginGroup("/History/" + QString(pComboBox->name()));
	for (int i = 0; i < iCount; i++) {
		const QString& sText = pComboBox->text(i);
		if (sText.isEmpty())
			break;
		m_settings.writeEntry("/Item" + QString::number(i + 1), sText);
	}
	m_settings.endGroup();
}


//---------------------------------------------------------------------------
// Splitter widget sizes persistence helper methods.

void qtractorOptions::loadSplitterSizes ( QSplitter *pSplitter,
	QValueList<int>& sizes )
{
	// Try to restore old splitter sizes...
	if (pSplitter) {
		m_settings.beginGroup("/Splitter/" + QString(pSplitter->name()));
		QStringList list = m_settings.readListEntry("/sizes");
		if (!list.isEmpty()) {
			sizes.clear();
			QStringList::Iterator iter = list.begin();
			while (iter != list.end())
				sizes.append((*iter++).toInt());
		}
		pSplitter->setSizes(sizes);
		m_settings.endGroup();
	}
}


void qtractorOptions::saveSplitterSizes ( QSplitter *pSplitter )
{
	// Try to save current splitter sizes...
	if (pSplitter) {
		m_settings.beginGroup("/Splitter/" + QString(pSplitter->name()));
		QStringList list;
		QValueList<int> sizes = pSplitter->sizes();
		QValueList<int>::Iterator iter = sizes.begin();
		while (iter != sizes.end())
			list.append(QString::number(*iter++));
		if (!list.isEmpty())
			m_settings.writeEntry("/sizes", list);
		m_settings.endGroup();
	}
}


// end of qtractorOptions.cpp
