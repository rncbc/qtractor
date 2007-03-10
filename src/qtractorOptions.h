// qtractorOptions.h
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

#ifndef __qtractorOptions_h
#define __qtractorOptions_h

#include <QSettings>
#include <QStringList>

class QWidget;
class QComboBox;
class QSplitter;


//-------------------------------------------------------------------------
// qtractorOptions - Prototype settings class.
//

class qtractorOptions
{
public:

	// Constructor.
	qtractorOptions();
	// Default destructor.
	~qtractorOptions();

	// The settings object accessor.
	QSettings& settings();

	// Command line arguments parser.
	bool parse_args(int argc, char **argv);
	// Command line usage helper.
	void print_usage(const char *arg0);

	// Startup supplied session file.
	QString sSessionFile;

	// Display options...
	QString sMessagesFont;
	bool    bMessagesLimit;
	int     iMessagesLimitLines;
	bool    bConfirmRemove;
	bool    bStdoutCapture;
	bool    bCompletePath;
	bool    bPeakAutoRemove;
	bool    bTransportTime;

	// View options...
	bool    bMenubar;
	bool    bStatusbar;
	bool    bFileToolbar;
	bool    bEditToolbar;
	bool    bTrackToolbar;
	bool    bViewToolbar;
	bool    bTransportToolbar;
	bool    bTimeToolbar;
	bool    bThumbToolbar;

	// Transport options...
	bool    bFollowPlayhead;

	// Default options...
	QString sSessionDir;
	QString sAudioDir;
	QString sMidiDir;
	QString sInstrumentDir;

	// Plugin search string.
	QString sPluginSearch;

	// The instrument file list.
	QStringList instrumentFiles;

	// Recent file list.
	int iMaxRecentFiles;
	QStringList recentFiles;

	// Tracks view options...
	int iTrackViewSelectMode;

	// Audio options...
	QString sCaptureExt;
	int iCaptureType;
	int iCaptureFormat;
	int iCaptureQuality;
	int iResampleType;

	// Widget geometry persistence helper prototypes.
	void saveWidgetGeometry(QWidget *pWidget);
	void loadWidgetGeometry(QWidget *pWidget);

	// Combo box history persistence helper prototypes.
	void loadComboBoxHistory(QComboBox *pComboBox, int iLimit = 8);
	void saveComboBoxHistory(QComboBox *pComboBox, int iLimit = 8);

	// Splitter widget sizes persistence helper methods.
	void loadSplitterSizes(QSplitter *pSplitter, QList<int>& sizes);
	void saveSplitterSizes(QSplitter *pSplitter);

private:

	// Settings member variables.
	QSettings m_settings;
};


#endif  // __qtractorOptions_h


// end of qtractorOptions.h
