// qtractorOptions.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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
class QObject;


//-------------------------------------------------------------------------
// qtractorOptions - Prototype settings class (singleton).
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

	// Explicit I/O methods.
	void loadOptions();
	void saveOptions();

	// Command line arguments parser.
	bool parse_args(const QStringList& args);
	// Command line usage helper.
	void print_usage(const QString& arg0);

	// Startup supplied session uuid.
	QString sSessionId;

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
	bool    bKeepToolsOnTop;
	int     iDisplayFormat;
	int     iBaseFontSize;

	// Logging options...
	bool    bMessagesLog;
	QString sMessagesLogPath;

	// View options...
	bool    bMenubar;
	bool    bStatusbar;
	bool    bFileToolbar;
	bool    bEditToolbar;
	bool    bTrackToolbar;
	bool    bViewToolbar;
	bool    bOptionsToolbar;
	bool    bTransportToolbar;
	bool    bTimeToolbar;
	bool    bThumbToolbar;
	int     iZoomMode;

	// Transport options...
	bool    bMetronome;
	bool    bFollowPlayhead;
	bool    bAutoBackward;
	bool    bContinuePastEnd;
	int     iTransportMode;

	// Audio options...
	QString sAudioCaptureExt;
	int     iAudioCaptureType;
	int     iAudioCaptureFormat;
	int     iAudioCaptureQuality;
	int     iAudioResampleType;
	bool    bAudioAutoTimeStretch;
	bool    bAudioWsolaTimeStretch;
	bool    bAudioWsolaQuickSeek;
	bool    bAudioPlayerBus;
	bool    bAudioMetroBus;
	bool    bAudioMetronome;

	bool    bAudioMasterAutoConnect;
	bool    bAudioPlayerAutoConnect;
	bool    bAudioMetroAutoConnect;

	// Audio metronome parameters.
	QString sMetroBarFilename;
	float   fMetroBarGain;
	QString sMetroBeatFilename;
	float   fMetroBeatGain;

	// MIDI options...
	int  iMidiCaptureFormat;
	int  iMidiCaptureQuantize;
	int  iMidiQueueTimer;
	bool bMidiPlayerBus;
	bool bMidiControlBus;
	bool bMidiMetroBus;
	bool bMidiMetronome;
	int  iMidiMmcDevice;
	int  iMidiMmcMode;
	int  iMidiSppMode;
	int  iMidiClockMode;

	// MIDI Metronome parameters.
	int iMetroChannel;
	int iMetroBarNote;
	int iMetroBarVelocity;
	int iMetroBarDuration;
	int iMetroBeatNote;
	int iMetroBeatVelocity;
	int iMetroBeatDuration;

	// Default options...
	QString sSessionDir;
	QString sAudioDir;
	QString sMidiDir;
	QString sPresetDir;
	QString sInstrumentDir;
	QString sMidiControlDir;
	QString sMidiSysexDir;

	// Session options.
	QString sSessionExt;
	bool    bSessionTemplate;
	QString sSessionTemplatePath;
	bool    bSessionBackup;
	int     iSessionBackupMode;
	bool	bAutoMonitor;
	int     iSnapPerBeat;
	float   fTempo;
	int     iBeatsPerBar;
	int     iBeatDivisor;
	int     iLoopRecordingMode;

	// Paste-repeat convenient defaults.
	int     iPasteRepeatCount;
	bool    bPasteRepeatPeriod;

	// Plugin search string.
	QString sPluginSearch;
	int     iPluginType;
	bool	bPluginActivate;

	// Automation curve mode default.
	int     iCurveMode;

    // Edit-range options.
    int     iEditRangeOptions;

	// Mouse modifier options.
	bool    bMidButtonModifier;

	// Session auto-save options.
	bool    bAutoSaveEnabled;
	int     iAutoSavePeriod;
	QString sAutoSavePathname;
	QString sAutoSaveFilename;

	// Plug-in paths.
	QStringList ladspaPaths;
	QStringList dssiPaths;
	QStringList vstPaths;
	QStringList lv2Paths;

	QString sLv2PresetDir;

	// Plug-in instrument options.
	bool bAudioOutputBus;
	bool bAudioOutputAutoConnect;

	// Plug-in GUI options.
	bool bOpenEditor;

	// VST dummy plugin scan option.
	bool bDummyVstScan;

	// LV2 plugin specific options.
	bool bLv2DynManifest;

	// Automation preferred resolution (14bit).
	bool bSaveCurve14bit;

	// The instrument file list.
	QStringList instrumentFiles;

	// The MIDI controllers file list.
	QStringList midiControlFiles;

	// Recent file list.
	int iMaxRecentFiles;
	QStringList recentFiles;

	// Tracks view options...
	int  iTrackViewSelectMode;
	bool bTrackViewDropSpan;
	bool bTrackViewSnapZebra;
	bool bTrackViewSnapGrid;
	bool bTrackViewToolTips;
	bool bTrackViewCurveEdit;

	// MIDI Editor options...
	bool bMidiMenubar;
	bool bMidiStatusbar;
	bool bMidiFileToolbar;
	bool bMidiEditToolbar;
	bool bMidiViewToolbar;
	bool bMidiTransportToolbar;
	bool bMidiScaleToolbar;
	bool bMidiThumbToolbar;
	bool bMidiNoteDuration;
	bool bMidiNoteColor;
	bool bMidiValueColor;
	bool bMidiPreview;
	bool bMidiFollow;
	bool bMidiEditMode;
	bool bMidiEditModeDraw;
	int  iMidiZoomMode;
	int  iMidiHorizontalZoom;
	int  iMidiVerticalZoom;
	int  iMidiSnapPerBeat;
	bool bMidiSnapZebra;
	bool bMidiSnapGrid;
	bool bMidiToolTips;
	int  iMidiSnapToScaleKey;
	int  iMidiSnapToScaleType;

	// Meter colors.
	QStringList audioMeterColors;
	QStringList midiMeterColors;

	// Transport view options.
	bool bSyncViewHold;

	// Global persistent user preference options.
	bool bUseNativeDialogs;

	// Run-time special non-persistent options.
	bool bDontUseNativeDialogs;

	// Widget geometry persistence helper prototypes.
	void saveWidgetGeometry(QWidget *pWidget, bool bVisible = false);
	void loadWidgetGeometry(QWidget *pWidget, bool bVisible = false);

	// Combo box history persistence helper prototypes.
	void loadComboBoxHistory(QComboBox *pComboBox, int iLimit = 8);
	void saveComboBoxHistory(QComboBox *pComboBox, int iLimit = 8);

	// Splitter widget sizes persistence helper methods.
	void loadSplitterSizes(QSplitter *pSplitter, QList<int>& sizes);
	void saveSplitterSizes(QSplitter *pSplitter);

	// Action shortcut persistence helper methos.
	void loadActionShortcuts(QObject *pObject);
	void saveActionShortcuts(QObject *pObject);

	// Singleton instance accessor.
	static qtractorOptions *getInstance();

private:

	// Settings member variables.
	QSettings m_settings;

	// The singleton instance.
	static qtractorOptions *g_pOptions;
};


#endif  // __qtractorOptions_h


// end of qtractorOptions.h
