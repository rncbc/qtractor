// qtractorMainForm.h
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

#ifndef __qtractorMainForm_h
#define __qtractorMainForm_h

#include "ui_qtractorMainForm.h"


// Forward declarations...
class qtractorOptions;
class qtractorSession;
class qtractorSessionEvent;
class qtractorSyncEvent;
class qtractorTracks;
class qtractorThumbView;
class qtractorCommand;
class qtractorCommandList;
class qtractorInstrumentList;
class qtractorFiles;
class qtractorMessages;
class qtractorConnections;
class qtractorMixer;
class qtractorMmcEvent;
class qtractorCtlEvent;
class qtractorMidiControl;
class qtractorTimeSpinBox;
class qtractorTempoSpinBox;
class qtractorTempoCursor;

class qtractorMidiEditorForm;
class qtractorMidiEditor;

class qtractorMidiControlObserver;
class qtractorSubject;

class qtractorNsmClient;

class QLabel;
class QComboBox;
class QProgressBar;
class QSocketNotifier;
class QActionGroup;
class QPalette;


//----------------------------------------------------------------------------
// qtractorMainForm -- UI wrapper form.

class qtractorMainForm : public QMainWindow
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMainForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorMainForm();

	static qtractorMainForm *getInstance();

	void setup(qtractorOptions *pOptions);

	qtractorTracks *tracks() const;
	qtractorFiles *files() const;
	qtractorConnections *connections() const;
	qtractorMixer *mixer() const;
	qtractorThumbView *thumbView() const;

	QString sessionName(const QString& sFilename) const;
	QString untitledName(void) const;

	void updateTransportTime(unsigned long iPlayHead);

	void appendMessages(const QString& s);
	void appendMessagesColor(const QString& s, const QString& c);
	void appendMessagesText(const QString& s);
	void appendMessagesError(const QString& s);

	void addEditorForm(qtractorMidiEditorForm *pEditorForm);
	void removeEditorForm(qtractorMidiEditorForm *pEditorForm);

	QMenu *editMenu() const
		{ return m_ui.editMenu; }
	QMenu *trackMenu() const
		{ return m_ui.trackMenu; }
	QMenu *trackCurveMenu() const
		{ return m_ui.trackCurveMenu; }
	QMenu *trackCurveModeMenu() const
		{ return m_ui.trackCurveModeMenu; }

	QProgressBar *progressBar() const
		{ return m_pProgressBar; }

	void contextMenuEvent(QContextMenuEvent *pEvent);

	void dropEvent(QDropEvent *pDropEvent);

	int rolling() const;

public slots:

	void fileNew();
	void fileOpen();
	void fileOpenRecent();
	void fileSave();
	void fileSaveAs();
	void fileProperties();
	void fileExit();

	void editUndo();
	void editRedo();
	void editCut();
	void editCopy();
	void editPaste();
	void editPasteRepeat();
	void editDelete();
	void editSelectModeClip();
	void editSelectModeRange();
	void editSelectModeRect();
	void editSelectModeCurve();
	void editSelectAll();
	void editSelectNone();
	void editSelectInvert();
	void editSelectTrack();
	void editSelectTrackRange();
	void editSelectRange();
	void editInsertRange();
	void editInsertTrackRange();
	void editRemoveRange();
	void editRemoveTrackRange();
	void editSplit();

	void trackAdd();
	void trackRemove();
	void trackProperties();
	void trackInputs();
	void trackOutputs();
	void trackStateRecord(bool bOn);
	void trackStateMute(bool bOn);
	void trackStateSolo(bool bOn);
	void trackStateMonitor(bool bOn);
	void trackNavigateFirst();
	void trackNavigatePrev();
	void trackNavigateNext();
	void trackNavigateLast();
	void trackNavigateNone();
	void trackMoveTop();
	void trackMoveUp();
	void trackMoveDown();
	void trackMoveBottom();
	void trackHeightUp();
	void trackHeightDown();
	void trackHeightReset();
	void trackAutoMonitor(bool bOn);
	void trackImportAudio();
	void trackImportMidi();
	void trackExportAudio();
	void trackExportMidi();
	void trackCurveSelect(bool On);
	void trackCurveSelect(QAction *pAction, bool bOn = true);
	void trackCurveMode(QAction *pAction);
	void trackCurveProcess(bool bOn);
	void trackCurveCapture(bool bOn);
	void trackCurveLocked(bool bOn);
	void trackCurveLogarithmic(bool bOn);
	void trackCurveColor();
	void trackCurveClear();
	void trackCurveProcessAll(bool bOn);
	void trackCurveCaptureAll(bool bOn);
	void trackCurveLockedAll(bool bOn);
	void trackCurveClearAll();

	void clipNew();
	void clipEdit();
	void clipUnlink();
	void clipSplit();
	void clipMerge();
	void clipNormalize();
	void clipTempoAdjust();
	void clipRangeSet();
	void clipLoopSet();
	void clipImport();
	void clipExport();
	void clipToolsQuantize();
	void clipToolsTranspose();
	void clipToolsNormalize();
	void clipToolsRandomize();
	void clipToolsResize();
	void clipToolsRescale();
	void clipToolsTimeshift();
	void clipTakeSelect(QAction *pAction);
	void clipTakeFirst();
	void clipTakePrev();
	void clipTakeNext();
	void clipTakeLast();
	void clipTakeReset();
	void clipTakeRange();

	void viewMenubar(bool bOn);
	void viewStatusbar(bool bOn);
	void viewToolbarFile(bool bOn);
	void viewToolbarEdit(bool bOn);
	void viewToolbarTrack(bool bOn);
	void viewToolbarView(bool bOn);
	void viewToolbarOptions(bool bOn);
	void viewToolbarTransport(bool bOn);
	void viewToolbarTime(bool bOn);
	void viewToolbarThumb(bool bOn);
	void viewFiles(bool bOn);
	void viewMessages(bool bOn);
	void viewConnections(bool bOn);
	void viewMixer(bool bOn);
	void viewZoomIn();
	void viewZoomOut();
	void viewZoomReset();
	void viewZoomHorizontal();
	void viewZoomVertical();
	void viewZoomAll();
	void viewSnapZebra(bool bOn);
	void viewSnapGrid(bool bOn);
	void viewToolTips(bool bOn);
	void viewSnap();
	void viewRefresh();
	void viewInstruments();
	void viewControllers();
	void viewBuses();
	void viewTempoMap();
	void viewOptions();

	void transportBackward();
	void transportRewind();
	void transportFastForward();
	void transportForward();
	void transportLoop();
	void transportLoopSet();
	void transportStop();
	void transportPlay();
	void transportRecord();
	void transportPunch();
	void transportPunchSet();
	void transportMetro();
	void transportFollow();
	void transportAutoBackward();
	void transportContinue();
	void transportPanic();

	void helpShortcuts();
	void helpAbout();
	void helpAboutQt();

	void stabilizeForm();

	void addAudioFile(const QString& sFilename);
	void addMidiFile(const QString& sFilename);

	void selectionNotifySlot(qtractorMidiEditor *pMidiEditor);
	void changeNotifySlot(qtractorMidiEditor *pMidiEditor);
	void updateNotifySlot(unsigned int flags);
	void dirtyNotifySlot();

protected slots:

	void timerSlot();

	void peakNotify();
	void alsaNotify();

	void audioShutNotify();
	void audioXrunNotify();
	void audioPortNotify();
	void audioBuffNotify();
	void audioSessNotify(void *pvSessionArg);
	void audioSyncNotify(unsigned long iPlayHead);

	void midiMmcNotify(const qtractorMmcEvent& mmce);
	void midiCtlNotify(const qtractorCtlEvent& ctle);
	void midiSppNotify(int iSppCmd, unsigned short iSongPos);
	void midiClkNotify(float fTempo);

	void updateRecentFilesMenu();
	void updateTrackMenu();
	void updateCurveMenu();
	void updateCurveModeMenu();
	void updateClipMenu();
	void updateTakeMenu();
	void updateTakeSelectMenu();
	void updateExportMenu();
	void updateZoomMenu();
	void updateSnapMenu();

	void selectAudioFile(const QString& sFilename, int iTrackChannel, bool bSelect);
	void activateAudioFile(const QString& sFilename, int iTrackChannel = -1);

	void selectMidiFile(const QString& sFilename, int iTrackChannel, bool bSelect);
	void activateMidiFile(const QString& sFilename, int iTrackChannel = -1);

	void trackSelectionChanged();
	void mixerSelectionChanged();

	void transportTimeFormatChanged(int iDisplayFormat);
	void transportTimeChanged(unsigned long iPlayHead);
	void transportTimeFinished();

	void transportTempoChanged(float fTempo,
		unsigned short iBeatsPerBar, unsigned short iBeatDivisor);
	void transportTempoFinished();

	void snapPerBeatChanged(int iSnap);
	void contentsChanged();

	void transportTempoContextMenu(const QPoint& pos);

	void handle_sigusr1();
	void handle_sigterm();

	void openNsmSession();
	void saveNsmSession();

	void showNsmSession();
	void hideNsmSession();

protected:

	void closeEvent(QCloseEvent *pCloseEvent);
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);

	void showEvent(QShowEvent *pShowEvent);
	void hideEvent(QHideEvent *pHideEvent);

	bool queryClose();

	bool newSession();
	bool openSession();
	bool saveSession(bool bPrompt);
	bool editSession();
	bool closeSession();

	bool loadSessionFileEx(
		const QString& sFilename, bool bTemplate, bool bUpdate);
	bool loadSessionFile(const QString& sFilename);

	bool saveSessionFileEx(
		const QString& sFilename, bool bTemplate, bool bUpdate);
	bool saveSessionFile(const QString& sFilename);

	bool startSession();
	bool checkRestartSession();

	bool setPlaying(bool bPlaying);
	bool setRecording(bool bRecording);

	int setRolling(int iRolling);

	void setLocate(unsigned long iLocate);
	void setShuttle(float fShuttle);
	void setStep(int iStep);

	void setTrack(int scmd, int iTrack, bool bOn);

	void setSongPos(unsigned short iSongPos);

	void updateSessionPre();
	void updateSessionPost();

	void updateRecentFiles(const QString& sFilename);
	void updatePeakAutoRemove();
	void updateMessagesFont();
	void updateMessagesLimit();
	void updateMessagesCapture();
	void updateDisplayFormat();
	void updatePluginPaths();
	void updateTransportMode();
	void updateMidiControlModes();
	void updateAudioPlayer();
	void updateMidiQueueTimer();
	void updateMidiPlayer();
	void updateMidiControl();
	void updateAudioMetronome();
	void updateMidiMetronome();
	void updateSyncViewHold();

	void updateContents(qtractorMidiEditor *pMidiEditor, bool bRefresh);
	void updateDirtyCount(bool bDirtyCount);

	void trackCurveSelectMenuAction(QMenu *pMenu,
		qtractorMidiControlObserver *pObserver, qtractorSubject *pCurrentSubject) const;

	bool trackCurveSelectMenuReset(QMenu *pMenu) const;
	bool trackCurveModeMenuReset(QMenu *pMenu) const;

	void saveNsmSessionEx(bool bSaveReply);

	bool autoSaveOpen();
	void autoSaveReset();
	void autoSaveSession();
	void autoSaveClose();

private:

	// The Qt-designer UI struct...
	Ui::qtractorMainForm m_ui;

	// Instance variables...
	qtractorOptions *m_pOptions;
	qtractorSession *m_pSession;
	qtractorFiles *m_pFiles;
	qtractorMessages *m_pMessages;
	qtractorConnections *m_pConnections;
	qtractorMixer *m_pMixer;
	qtractorTracks *m_pTracks;
	QString m_sFilename;
	int m_iUntitled;
	int m_iDirtyCount;
	int m_iBackupCount;
	QSocketNotifier *m_pUsr1Notifier;
	QSocketNotifier *m_pTermNotifier;
	QActionGroup *m_pSelectModeActionGroup;
	qtractorTimeSpinBox *m_pTimeSpinBox;
	qtractorTempoSpinBox *m_pTempoSpinBox;
	QComboBox *m_pSnapPerBeatComboBox;
	QProgressBar *m_pProgressBar;
	qtractorThumbView *m_pThumbView;
	qtractorTempoCursor *m_pTempoCursor;
	qtractorMidiControl *m_pMidiControl;
	qtractorNsmClient *m_pNsmClient;
	QString m_sNsmFile;
	QString m_sNsmExt;
	bool m_bNsmDirty;
	unsigned long m_iPlayHead;
	int m_iPeakTimer;
	int m_iPlayTimer;
	int m_iIdleTimer;
	int m_iTransportUpdate;
	int m_iTransportRolling;
	bool m_bTransportPlaying;
	float m_fTransportShuttle;
	int m_iTransportStep;
	int m_iXrunCount;
	int m_iXrunSkip;
	int m_iXrunTimer;
	int m_iAudioRefreshTimer;
	int m_iMidiRefreshTimer;
	int m_iPlayerTimer;
	int m_iAutoSaveTimer;
	int m_iAutoSavePeriod;

	// Status bar item indexes
	enum {
		StatusName    = 0,   // Active session track caption.
		StatusMod     = 1,   // Current session modification state.
		StatusRec     = 2,   // Current session recording state.
		StatusMute    = 3,   // Current session muting state.
		StatusSolo    = 4,   // Current session soloing state.
		StatusLoop    = 5,   // Current session looping state.
		StatusTime    = 6,   // Current session length time.
		StatusRate    = 7,   // Current session sample rate.
		StatusItems   = 8    // Number of status items.
	};

	QLabel *m_statusItems[StatusItems];

	// Palette status item indexes
	enum {
		PaletteNone   = 0,   // Default background color.
		PaletteRed    = 1,   // Current session recording state (red).
		PaletteYellow = 2,   // Current session muting state (yellow).
		PaletteCyan   = 3,   // Current session soloing state (cyan).
		PaletteGreen  = 4,   // Current session looping state (green).
		PaletteItems  = 5    // Number of palette items.
	};

	QPalette *m_paletteItems[PaletteItems];

	// View/Snap-to-beat actions (for shortcuts access)
	QList<QAction *> m_snapPerBeatActions;

	// Name says it all...
	QList<qtractorMidiEditorForm *> m_editors;

	// Kind-of singleton reference.
	static qtractorMainForm *g_pMainForm;
};


#endif	// __qtractorMainForm_h


// end of qtractorMainForm.h
