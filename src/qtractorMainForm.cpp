// qtractorMainForm.cpp
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

#include "qtractorMainForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorInstrument.h"
#include "qtractorMessages.h"
#include "qtractorFiles.h"
#include "qtractorConnections.h"
#include "qtractorMixer.h"

#include "qtractorTracks.h"
#include "qtractorTrackList.h"
#include "qtractorTrackTime.h"
#include "qtractorTrackView.h"
#include "qtractorThumbView.h"
#include "qtractorSpinBox.h"

#include "qtractorAudioPeak.h"
#include "qtractorAudioBuffer.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorSessionDocument.h"
#include "qtractorSessionCursor.h"

#include "qtractorSessionCommand.h"
#include "qtractorClipCommand.h"

#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"

#include "qtractorSessionForm.h"
#include "qtractorOptionsForm.h"
#include "qtractorConnectForm.h"
#include "qtractorInstrumentForm.h"
#include "qtractorBusForm.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QRegExp>
#include <QUrl>

#include <QSocketNotifier>
#include <QActionGroup>
#include <QStatusBar>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QDateTime>

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QCloseEvent>
#include <QDropEvent>


#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <math.h>


// Timer constant stuff.
#define QTRACTOR_TIMER_MSECS    50
#define QTRACTOR_TIMER_DELAY    200


// Specialties for thread-callback comunication.
#define QTRACTOR_PEAK_EVENT     QEvent::Type(QEvent::User + 1)
#define QTRACTOR_XRUN_EVENT     QEvent::Type(QEvent::User + 2)
#define QTRACTOR_SHUT_EVENT     QEvent::Type(QEvent::User + 3)
#define QTRACTOR_PORT_EVENT     QEvent::Type(QEvent::User + 4)
#define QTRACTOR_BUFF_EVENT     QEvent::Type(QEvent::User + 5)
#define QTRACTOR_MMC_EVENT      QEvent::Type(QEvent::User + 6)


//-------------------------------------------------------------------------
// qtractorMainForm -- Main window form implementation.

// Kind of singleton reference.
qtractorMainForm *qtractorMainForm::g_pMainForm = NULL;

// Constructor.
qtractorMainForm::qtractorMainForm (
	QWidget *pParent, Qt::WFlags wflags ) : QMainWindow(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Pseudo-singleton reference setup.
	g_pMainForm = this;

	// Initialize some pointer references.
	m_pOptions     = NULL;
	m_pSession     = new qtractorSession();
	m_pCommands    = new qtractorCommandList();
	m_pInstruments = new qtractorInstrumentList();

	// All child forms are to be created later, not earlier than setup.
	m_pMessages    = NULL;
	m_pFiles       = NULL;
	m_pMixer       = NULL;
	m_pConnections = NULL;
	m_pTracks      = NULL;

	// To remember last time we've shown the playhead.
	m_iPlayHead = 0;

	// We'll start clean.
	m_iUntitled   = 0;
	m_iDirtyCount = 0;

	m_iPeakTimer = 0;
	m_iPlayTimer = 0;

	m_iTransportUpdate  = 0; 
	m_iTransportDelta   = 0;
	m_iTransportRolling = 0;
	m_fTransportShuttle = 0.0f;
	m_iTransportStep    = 0;

	m_iXrunCount = 0;
	m_iXrunSkip  = 0;
	m_iXrunTimer = 0;

	m_iAudioRefreshTimer = 0;
	m_iMidiRefreshTimer  = 0;

	// Configure the audio engine event handling...
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine) {
		pAudioEngine->setNotifyWidget(this);
		pAudioEngine->setNotifyShutdownType(QTRACTOR_SHUT_EVENT);
		pAudioEngine->setNotifyXrunType(QTRACTOR_XRUN_EVENT);
		pAudioEngine->setNotifyPortType(QTRACTOR_PORT_EVENT);
		pAudioEngine->setNotifyBufferType(QTRACTOR_BUFF_EVENT);
	}

	// Configure the audio file peak factory...
	qtractorAudioPeakFactory *pAudioPeakFactory
		= m_pSession->audioPeakFactory();
	if (pAudioPeakFactory) {
		pAudioPeakFactory->setNotifyWidget(this);
		pAudioPeakFactory->setNotifyPeakType(QTRACTOR_PEAK_EVENT);
	}

	// Configure the MIDI engine event handling...
	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine) {
		pMidiEngine->setNotifyWidget(this);
		pMidiEngine->setNotifyMmcType(QTRACTOR_MMC_EVENT);
	}

#ifdef HAVE_SIGNAL_H
	// Set to ignore any fatal "Broken pipe" signals.
	::signal(SIGPIPE, SIG_IGN);
#endif

	// Get edit selection mode action group up...
//	m_ui.editToolbar->addSeparator();
	m_pSelectModeActionGroup = new QActionGroup(this);
	m_pSelectModeActionGroup->setExclusive(true);
//	m_pSelectModeActionGroup->setUsesDropDown(true);
	m_pSelectModeActionGroup->addAction(m_ui.editSelectModeClipAction);
	m_pSelectModeActionGroup->addAction(m_ui.editSelectModeRangeAction);
	m_pSelectModeActionGroup->addAction(m_ui.editSelectModeRectAction);
//	m_ui.editToolbar->addActions(m_pSelectModeActionGroup->actions());

	// Additional time-toolbar controls...
//	m_ui.timeToolbar->addSeparator();

	// Editable toolbar widgets special palette.
	QPalette pal;
	pal.setColor(QPalette::Base, Qt::black);
	pal.setColor(QPalette::Text, Qt::green);

	// Transport time.
	const QFont& font = qtractorMainForm::font();
	m_pTransportTimeSpinBox = new qtractorSpinBox();
	m_pTransportTimeSpinBox->setTimeScale(m_pSession->timeScale());
	m_pTransportTimeSpinBox->setFont(QFont(font.family(), font.pointSize() + 2));
	m_pTransportTimeSpinBox->setPalette(pal);
//	m_pTransportTimeSpinBox->setAutoFillBackground(true);
	m_pTransportTimeSpinBox->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	m_pTransportTimeSpinBox->setFixedSize(QSize(124, 26));
	m_pTransportTimeSpinBox->setToolTip(tr("Current transport time (playhead)"));
	m_ui.timeToolbar->addWidget(m_pTransportTimeSpinBox);
	m_ui.timeToolbar->addSeparator();

	// Tempo spin-box.
	m_pTempoSpinBox = new QDoubleSpinBox();
	m_pTempoSpinBox->setDecimals(1);
	m_pTempoSpinBox->setMinimum(1.0f);
	m_pTempoSpinBox->setMaximum(1000.0f);
	m_pTempoSpinBox->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	m_pTempoSpinBox->setPalette(pal);
	m_pTempoSpinBox->setToolTip(tr("Tempo (BPM)"));
	m_ui.timeToolbar->addWidget(m_pTempoSpinBox);
	m_ui.timeToolbar->addSeparator();

	// Snap-per-beat combo-box.
	m_pSnapPerBeatComboBox = new QComboBox();
	m_pSnapPerBeatComboBox->setEditable(false);
	m_pSnapPerBeatComboBox->addItem(tr("None"));
	QString sPrefix = tr("Beat");
	m_pSnapPerBeatComboBox->addItem(sPrefix);
	sPrefix += "/%1";
	for (unsigned short iSnapPerBeat = 2; iSnapPerBeat < 64; iSnapPerBeat <<= 1)
		m_pSnapPerBeatComboBox->addItem(sPrefix.arg(iSnapPerBeat));
	m_pSnapPerBeatComboBox->setToolTip(tr("Snap/beat"));
	m_ui.timeToolbar->addWidget(m_pSnapPerBeatComboBox);

	// Track-line thumbnail view...
	m_pThumbView = new qtractorThumbView();
	m_ui.thumbToolbar->addWidget(m_pThumbView);
	m_ui.thumbToolbar->setAllowedAreas(
		Qt::TopToolBarArea | Qt::BottomToolBarArea);

	QObject::connect(m_pTempoSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(tempoChanged()));
	QObject::connect(m_pSnapPerBeatComboBox,
		SIGNAL(activated(int)),
		SLOT(snapPerBeatChanged(int)));

	// Create some statusbar labels...
	QLabel *pLabel;
	const QSize pad(4, 0);
	QPalette *pPalette = new QPalette(statusBar()->palette());
	m_paletteItems[PaletteNone] = pPalette;

	pPalette = new QPalette(statusBar()->palette());
	pPalette->setColor(QPalette::Window, Qt::red);
	m_paletteItems[PaletteRed] = pPalette;

	pPalette = new QPalette(statusBar()->palette());
	pPalette->setColor(QPalette::Window, Qt::yellow);
	m_paletteItems[PaletteYellow] = pPalette;

	pPalette = new QPalette(statusBar()->palette());
	pPalette->setColor(QPalette::Window, Qt::cyan);
	m_paletteItems[PaletteCyan] = pPalette;

	pPalette = new QPalette(statusBar()->palette());
	pPalette->setColor(QPalette::Window, Qt::green);
	m_paletteItems[PaletteGreen] = pPalette;

	// Track status.
	pLabel = new QLabel(tr("Track"));
	pLabel->setAlignment(Qt::AlignLeft);
	pLabel->setToolTip(tr("Current track name"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusName] = pLabel;
	statusBar()->addWidget(pLabel, 2);

	// Session modification status.
	pLabel = new QLabel(tr("MOD"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session modification state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusMod] = pLabel;
	statusBar()->addWidget(pLabel);

	// Session recording status.
	pLabel = new QLabel(tr("REC"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session record state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusRec] = pLabel;
	statusBar()->addWidget(pLabel);

	// Session muting status.
	pLabel = new QLabel(tr("MUTE"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session muting state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusMute] = pLabel;
	statusBar()->addWidget(pLabel);

	// Session soloing status.
	pLabel = new QLabel(tr("SOLO"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session soloing state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusSolo] = pLabel;
	statusBar()->addWidget(pLabel);

	// Session looping status.
	pLabel = new QLabel(tr("LOOP"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session looping state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusLoop] = pLabel;
	statusBar()->addWidget(pLabel);

	// Session length time.
	const QString sTime("00:00:00.000");
	pLabel = new QLabel(sTime);
	pLabel->setAlignment(Qt::AlignRight);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session total time"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusTime] = pLabel;
	statusBar()->addWidget(pLabel);

	// Session sample rate.
	const QString sRate("44100 Hz");
	pLabel = new QLabel(sRate);
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setAutoFillBackground(true);
	pLabel->setToolTip(tr("Session sample rate"));
	m_statusItems[StatusRate] = pLabel;
	statusBar()->addWidget(pLabel);

	// Some actions surely need those
	// shortcuts firmly attached...
	addAction(m_ui.viewMenubarAction);

	// Ah, make it stand right.
	setFocus();

	// UI signal/slot connections...
	QObject::connect(m_ui.fileNewAction,
		SIGNAL(triggered(bool)),
		SLOT(fileNew()));
	QObject::connect(m_ui.fileOpenAction,
		SIGNAL(triggered(bool)),
		SLOT(fileOpen()));
	QObject::connect(m_ui.fileSaveAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSave()));
	QObject::connect(m_ui.fileSaveAsAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSaveAs()));
	QObject::connect(m_ui.filePropertiesAction,
		SIGNAL(triggered(bool)),
		SLOT(fileProperties()));
	QObject::connect(m_ui.fileExitAction,
		SIGNAL(triggered(bool)),
		SLOT(fileExit()));
	QObject::connect(m_ui.editUndoAction,
		SIGNAL(triggered(bool)),
		SLOT(editUndo()));
	QObject::connect(m_ui.editRedoAction,
		SIGNAL(triggered(bool)),
		SLOT(editRedo()));
	QObject::connect(m_ui.editCutAction,
		SIGNAL(triggered(bool)),
		SLOT(editCut()));
	QObject::connect(m_ui.editCopyAction,
		SIGNAL(triggered(bool)),
		SLOT(editCopy()));
	QObject::connect(m_ui.editPasteAction,
		SIGNAL(triggered(bool)),
		SLOT(editPaste()));
	QObject::connect(m_ui.editDeleteAction,
		SIGNAL(triggered(bool)),
		SLOT(editDelete()));
	QObject::connect(m_ui.editSelectModeClipAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectModeClip()));
	QObject::connect(m_ui.editSelectModeRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectModeRange()));
	QObject::connect(m_ui.editSelectModeRectAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectModeRect()));
	QObject::connect(m_ui.editSelectNoneAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectNone()));
	QObject::connect(m_ui.editSelectRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectRange()));
	QObject::connect(m_ui.editSelectTrackAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectTrack()));
	QObject::connect(m_ui.editSelectAllAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectAll()));
	QObject::connect(m_ui.editClipAction,
		SIGNAL(triggered(bool)),
		SLOT(editClip()));
	QObject::connect(m_ui.trackAddAction,
		SIGNAL(triggered(bool)),
		SLOT(trackAdd()));
	QObject::connect(m_ui.trackRemoveAction,
		SIGNAL(triggered(bool)),
		SLOT(trackRemove()));
	QObject::connect(m_ui.trackPropertiesAction,
		SIGNAL(triggered(bool)),
		SLOT(trackProperties()));
	QObject::connect(m_ui.trackImportAudioAction,
		SIGNAL(triggered(bool)),
		SLOT(trackImportAudio()));
	QObject::connect(m_ui.trackImportMidiAction,
		SIGNAL(triggered(bool)),
		SLOT(trackImportMidi()));
	QObject::connect(m_ui.viewMenubarAction,
		SIGNAL(triggered(bool)),
		SLOT(viewMenubar(bool)));
	QObject::connect(m_ui.viewStatusbarAction,
		SIGNAL(triggered(bool)),
		SLOT(viewStatusbar(bool)));
	QObject::connect(m_ui.viewToolbarFileAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarFile(bool)));
	QObject::connect(m_ui.viewToolbarEditAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarEdit(bool)));
	QObject::connect(m_ui.viewToolbarTrackAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTrack(bool)));
	QObject::connect(m_ui.viewToolbarViewAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarView(bool)));
	QObject::connect(m_ui.viewToolbarTransportAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTransport(bool)));
	QObject::connect(m_ui.viewToolbarTimeAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTime(bool)));
	QObject::connect(m_ui.viewToolbarThumbAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarThumb(bool)));
	QObject::connect(m_ui.viewFilesAction,
		SIGNAL(triggered(bool)),
		SLOT(viewFiles(bool)));
	QObject::connect(m_ui.viewMessagesAction,
		SIGNAL(triggered(bool)),
		SLOT(viewMessages(bool)));
	QObject::connect(m_ui.viewConnectionsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewConnections(bool)));
	QObject::connect(m_ui.viewMixerAction,
		SIGNAL(triggered(bool)),
		SLOT(viewMixer(bool)));
	QObject::connect(m_ui.viewRefreshAction,
		SIGNAL(triggered(bool)),
		SLOT(viewRefresh()));
	QObject::connect(m_ui.viewInstrumentsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewInstruments()));
	QObject::connect(m_ui.viewBusesAction,
		SIGNAL(triggered(bool)),
		SLOT(viewBuses()));
	QObject::connect(m_ui.viewOptionsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewOptions()));
	QObject::connect(m_ui.transportBackwardAction,
		SIGNAL(triggered(bool)),
		SLOT(transportBackward()));
	QObject::connect(m_ui.transportRewindAction,
		SIGNAL(triggered(bool)),
		SLOT(transportRewind()));
	QObject::connect(m_ui.transportFastForwardAction,
		SIGNAL(triggered(bool)),
		SLOT(transportFastForward()));
	QObject::connect(m_ui.transportForwardAction,
		SIGNAL(triggered(bool)),
		SLOT(transportForward()));
	QObject::connect(m_ui.transportLoopAction,
		SIGNAL(triggered(bool)),
		SLOT(transportLoop()));
	QObject::connect(m_ui.transportPlayAction,
		SIGNAL(triggered(bool)),
		SLOT(transportPlay()));
	QObject::connect(m_ui.transportRecordAction,
		SIGNAL(triggered(bool)),
		SLOT(transportRecord()));
	QObject::connect(m_ui.transportFollowAction,
		SIGNAL(triggered(bool)),
		SLOT(transportFollow()));
	QObject::connect(m_ui.helpAboutAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAbout()));
	QObject::connect(m_ui.helpAboutQtAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAboutQt()));

	QObject::connect(m_ui.fileMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateRecentFilesMenu()));

	QObject::connect(m_pCommands,
		SIGNAL(updateNotifySignal(bool)),
		SLOT(updateNotifySlot(bool)));

	QObject::connect(m_pTransportTimeSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(transportTimeChanged(unsigned long)));
}


// Destructor.
qtractorMainForm::~qtractorMainForm (void)
{
	// Drop any widgets around (not really necessary)...
	if (m_pMixer)
		delete m_pMixer;
	if (m_pConnections)
		delete m_pConnections;
	if (m_pFiles)
		delete m_pFiles;
	if (m_pMessages)
		delete m_pMessages;
	if (m_pTracks)
		delete m_pTracks;

	//  Free some more still around...
	if (m_pInstruments)
		delete m_pInstruments;
	if (m_pCommands)
		delete m_pCommands;
	if (m_pSession)
		delete m_pSession;

	// Get select mode action group down.
	if (m_pSelectModeActionGroup)
		delete m_pSelectModeActionGroup;

	// Reclaim status items palettes...
	for (int i = 0; i < PaletteItems; i++)
		delete m_paletteItems[i];

	// Pseudo-singleton reference shut-down.
	g_pMainForm = NULL;
}


// kind of singleton reference.
qtractorMainForm *qtractorMainForm::getInstance (void)
{
	return g_pMainForm;
}


// Make and set a proper setup options step.
void qtractorMainForm::setOptions ( qtractorOptions *pOptions )
{
	// We got options?
	m_pOptions = pOptions;

	// Some child forms are to be created right now.
	m_pFiles = new qtractorFiles(this);
	m_pFiles->audioListView()->setRecentDir(m_pOptions->sAudioDir);
	m_pFiles->midiListView()->setRecentDir(m_pOptions->sMidiDir);
	m_pMessages = new qtractorMessages(this);
	m_pConnections = new qtractorConnections(this);
	m_pMixer = new qtractorMixer(this);

	// Make those primordially docked...
	addDockWidget(Qt::RightDockWidgetArea, m_pFiles, Qt::Vertical);
	addDockWidget(Qt::BottomDockWidgetArea, m_pMessages, Qt::Horizontal);

	// Time to create the main session track list view...
	m_pTracks = new qtractorTracks(this);
	// Make it shine :-)
	setCentralWidget(m_pTracks);

	// Set message defaults...
	updateMessagesFont();
	updateMessagesLimit();
	updateMessagesCapture();

	// Track view select mode...
	qtractorTrackView::SelectMode selectMode;
	switch (pOptions->iTrackViewSelectMode) {
	case 2:
		selectMode = qtractorTrackView::SelectRect;
		m_ui.editSelectModeRectAction->setChecked(true);
		break;
	case 1:
		selectMode = qtractorTrackView::SelectRange;
		m_ui.editSelectModeRangeAction->setChecked(true);
		break;
	case 0:
	default:
		selectMode = qtractorTrackView::SelectClip;
		m_ui.editSelectModeClipAction->setChecked(true);
		break;
	}
	m_pTracks->trackView()->setSelectMode(selectMode);

	// Initial decorations toggle state.
	m_ui.viewMenubarAction->setChecked(m_pOptions->bMenubar);
	m_ui.viewStatusbarAction->setChecked(m_pOptions->bStatusbar);
	m_ui.viewToolbarFileAction->setChecked(m_pOptions->bFileToolbar);
	m_ui.viewToolbarEditAction->setChecked(m_pOptions->bEditToolbar);
	m_ui.viewToolbarTrackAction->setChecked(m_pOptions->bTrackToolbar);
	m_ui.viewToolbarViewAction->setChecked(m_pOptions->bViewToolbar);
	m_ui.viewToolbarTransportAction->setChecked(m_pOptions->bTransportToolbar);
	m_ui.viewToolbarTimeAction->setChecked(m_pOptions->bTimeToolbar);
	m_ui.viewToolbarThumbAction->setChecked(m_pOptions->bThumbToolbar);

	m_ui.transportFollowAction->setChecked(m_pOptions->bFollowPlayhead);

	// Initial decorations visibility state.
	viewMenubar(m_pOptions->bMenubar);
	viewStatusbar(m_pOptions->bStatusbar);
	viewToolbarFile(m_pOptions->bFileToolbar);
	viewToolbarEdit(m_pOptions->bEditToolbar);
	viewToolbarTrack(m_pOptions->bTrackToolbar);
	viewToolbarView(m_pOptions->bViewToolbar);
	viewToolbarTransport(m_pOptions->bTransportToolbar);
	viewToolbarTime(m_pOptions->bTimeToolbar);
	viewToolbarThumb(m_pOptions->bThumbToolbar);

	// Restore whole dock windows state.
	QByteArray aDockables = m_pOptions->settings().value(
		"/Layout/DockWindows").toByteArray();
	if (aDockables.isEmpty()) {
		// Some windows are forced initially as is...
		insertToolBarBreak(m_ui.timeToolbar);
		insertToolBarBreak(m_ui.transportToolbar);
	} else {
		// Make it as the last time.
		restoreState(aDockables);
	}

	// Try to restore old window positioning.
	m_pOptions->loadWidgetGeometry(this);
	m_pOptions->loadWidgetGeometry(m_pMixer);
	m_pOptions->loadWidgetGeometry(m_pConnections);

	// Load instrument definition files...
	QStringListIterator iter(m_pOptions->instrumentFiles);
	while (iter.hasNext())
		m_pInstruments->load(iter.next());

	// Primary startup stabilization...
	updateRecentFilesMenu();
	updatePeakAutoRemove();
	updateDisplayFormat();

	// FIXME: This is what it should ever be,
	// make it right from this very moment...
	qtractorAudioFileFactory::setDefaultType(
		m_pOptions->sCaptureExt,
		m_pOptions->iCaptureType,
		m_pOptions->iCaptureFormat,
		m_pOptions->iCaptureQuality);
	// Set default sample-rate converter quality...
	qtractorAudioBuffer::setResampleType(m_pOptions->iResampleType);

	// Change to last known session dir...
	if (!m_pOptions->sSessionDir.isEmpty()) {
		if (!QDir::setCurrent(m_pOptions->sSessionDir)) {
			appendMessagesError(
				tr("Could not set default session directory:\n\n"
				"%1\n\nSorry.").arg(m_pOptions->sSessionDir));
		}
	}

	// Is any session pending to be loaded?
	if (!m_pOptions->sSessionFile.isEmpty()) {
		// Just load the prabably startup session...
		if (loadSessionFile(m_pOptions->sSessionFile))
			m_pOptions->sSessionFile.clear();
	} else {
		// Open up with a new empty session...
		newSession();
	}

	// Final widget slot connections....
	QObject::connect(m_pFiles->toggleViewAction(),
		SIGNAL(triggered(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_pMessages->toggleViewAction(),
		SIGNAL(triggered(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_pMixer->trackRack(),
		SIGNAL(selectionChanged()),
		SLOT(mixerSelectionChanged()));
	QObject::connect(m_pFiles->audioListView(),
		SIGNAL(activated(const QString&)),
		SLOT(activateAudioFile(const QString&)));
	QObject::connect(m_pFiles->midiListView(),
		SIGNAL(activated(const QString&)),
		SLOT(activateMidiFile(const QString&)));
	QObject::connect(m_pFiles->audioListView(),
		SIGNAL(contentsChanged()),
		SLOT(contentsChanged()));
	QObject::connect(m_pFiles->midiListView(),
		SIGNAL(contentsChanged()),
		SLOT(contentsChanged()));
	QObject::connect(m_pTracks->trackList(),
		SIGNAL(selectionChanged()),
		SLOT(trackSelectionChanged()));

	// Other dedicated signal/slot connections...
	QObject::connect(m_pTracks->trackView(),
		SIGNAL(contentsMoving(int,int)),
		m_pThumbView, SLOT(updateThumb()));

	// Make it ready :-)
	statusBar()->showMessage(tr("Ready"), 3000);
 
	// Register the first timer slot.
	QTimer::singleShot(QTRACTOR_TIMER_MSECS, this, SLOT(timerSlot()));
}


// Window close event handlers.
bool qtractorMainForm::queryClose (void)
{
	bool bQueryClose = closeSession();

	// Try to save current general state...
	if (m_pOptions) {
		// Some windows default fonts is here on demand too.
		if (bQueryClose && m_pMessages)
			m_pOptions->sMessagesFont = m_pMessages->messagesFont().toString();
		// Save recent directories...
		if (bQueryClose && m_pFiles) {
			m_pOptions->sAudioDir = m_pFiles->audioListView()->recentDir();
			m_pOptions->sMidiDir = m_pFiles->midiListView()->recentDir();
		}
		// Try to save current positioning.
		if (bQueryClose) {
			// Save decorations state.
			m_pOptions->bMenubar = m_ui.MenuBar->isVisible();
			m_pOptions->bStatusbar = statusBar()->isVisible();
			m_pOptions->bFileToolbar = m_ui.fileToolbar->isVisible();
			m_pOptions->bEditToolbar = m_ui.editToolbar->isVisible();
			m_pOptions->bTrackToolbar = m_ui.trackToolbar->isVisible();
			m_pOptions->bViewToolbar = m_ui.viewToolbar->isVisible();
			m_pOptions->bTransportToolbar = m_ui.transportToolbar->isVisible();
			m_pOptions->bTimeToolbar = m_ui.timeToolbar->isVisible();
			m_pOptions->bThumbToolbar = m_ui.thumbToolbar->isVisible();
			m_pOptions->bFollowPlayhead = m_ui.transportFollowAction->isChecked();
			// Save instrument definition file list...
			m_pOptions->instrumentFiles = m_pInstruments->files();
			// Save the dock windows state.
			m_pOptions->settings().setValue("/Layout/DockWindows", saveState());
			// And the main windows state.
			m_pOptions->saveWidgetGeometry(m_pConnections);
			m_pOptions->saveWidgetGeometry(m_pMixer);
			m_pOptions->saveWidgetGeometry(this);
		}
	}

	return bQueryClose;
}


void qtractorMainForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	if (queryClose())
		pCloseEvent->accept();
	else
		pCloseEvent->ignore();
}


// Window drag-n-drop event handlers.
void qtractorMainForm::dragEnterEvent ( QDragEnterEvent* pDragEnterEvent )
{
	// Accept external drags only...
	if (pDragEnterEvent->source() == NULL
		&& pDragEnterEvent->mimeData()->hasUrls()) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


void qtractorMainForm::dropEvent ( QDropEvent* pDropEvent )
{
	// Accept externally originated drops only...
	if (pDropEvent->source())
		return;

	const QMimeData *pMimeData = pDropEvent->mimeData();
	if (pMimeData->hasUrls()) {
		QString sFilename = pMimeData->urls().first().toLocalFile();
		// Close current session and try to load the new one...
		if (!sFilename.isEmpty() && closeSession())
			loadSessionFile(sFilename);
	}
}


// Custom event handler.
void qtractorMainForm::customEvent ( QEvent *pEvent )
{
#ifdef CONFIG_DEBUG_0
	appendMessages("qtractorMainForm::customEvent("
		+ QString::number((int) pEvent->type()) + ")");
#endif

	switch (pEvent->type()) {
	case QTRACTOR_PEAK_EVENT:
		// A peak file has just been (re)created;
		// try to postpone the event effect a little more...
		if (m_iPeakTimer  < QTRACTOR_TIMER_DELAY)
			m_iPeakTimer += QTRACTOR_TIMER_DELAY;
		break;
	case QTRACTOR_PORT_EVENT:
		// An Audio graph change has just been issued;
		// try to postpone the event effect a little more...
		if (m_iAudioRefreshTimer  < QTRACTOR_TIMER_DELAY)
			m_iAudioRefreshTimer += QTRACTOR_TIMER_DELAY;
		break;
	case QTRACTOR_XRUN_EVENT:
		// An XRUN has just been notified...
		m_iXrunCount++;
		// Skip this one, maybe we're under some kind of storm;
		if (m_iXrunTimer > 0)
			m_iXrunSkip++;
		// Defer the informative effect...
		if (m_iXrunTimer  < QTRACTOR_TIMER_DELAY)
			m_iXrunTimer += QTRACTOR_TIMER_DELAY;
		break;
	case QTRACTOR_SHUT_EVENT:
	case QTRACTOR_BUFF_EVENT:
		// Just in case we were in the middle of something...
		if (m_pSession->isPlaying()) {
			m_ui.transportPlayAction->setChecked(false);
		//	transportPlay(); // Toggle playing!
		}
		// Engine shutdown is on demand...
		m_pSession->close();
		m_pConnections->clear();
		// Send an informative message box...
		appendMessagesError(
			tr("The audio engine has been shutdown.\n\n"
			"Make sure the JACK audio server (jackd)\n"
			"is up and running and then restart session."));
		// Make things just bearable...
		stabilizeForm();
		break;
	case QTRACTOR_MMC_EVENT:
		// MMC event handling...
		mmcEvent(static_cast<qtractorMmcEvent *> (pEvent));
		// Fall thru.
	default:
		break;
	}
}


// Custom MMC event handler.
void qtractorMainForm::mmcEvent ( qtractorMmcEvent *pMmcEvent )
{
	QString sMmcText;
	switch (pMmcEvent->cmd()) {
	case qtractorMmcEvent::STOP:
	case qtractorMmcEvent::PAUSE:
		sMmcText = tr("STOP");
		if (setPlaying(false))
			m_ui.transportPlayAction->setChecked(false);
		break;
	case qtractorMmcEvent::PLAY:
	case qtractorMmcEvent::DEFERRED_PLAY:
		sMmcText = tr("PLAY");
		if (setPlaying(true))
			m_ui.transportPlayAction->setChecked(true);
		break;
	case qtractorMmcEvent::FAST_FORWARD:
		sMmcText = tr("FFWD");
		if (0 >= setRolling(+1))
			m_ui.transportFastForwardAction->setChecked(true);
		break;
	case qtractorMmcEvent::REWIND:
		sMmcText = tr("REW");
		if (setRolling(-1) >= 0)
			m_ui.transportRewindAction->setChecked(true);
		break;
	case qtractorMmcEvent::RECORD_STROBE:
	case qtractorMmcEvent::RECORD_PAUSE:
		sMmcText = tr("REC ON");
		if (setRecording(true)) {
			m_ui.transportRecordAction->setChecked(true);
		} else {
			// Send MMC RECORD_EXIT command immediate reply...
			m_pSession->midiEngine()->sendMmcCommand(
				qtractorMmcEvent::RECORD_EXIT);
		}
		break;
	case qtractorMmcEvent::RECORD_EXIT:
		sMmcText = tr("REC OFF");
		if (setRecording(false))
			m_ui.transportRecordAction->setChecked(false);
		break;
	case qtractorMmcEvent::MMC_RESET:
		sMmcText = tr("RESET");
		setRolling(0);
		break;
	case qtractorMmcEvent::LOCATE:
		sMmcText = tr("LOCATE %1").arg(pMmcEvent->locate());
		setLocate(pMmcEvent->locate());
		break;
	case qtractorMmcEvent::SHUTTLE:
		sMmcText = tr("SHUTTLE %1").arg(pMmcEvent->shuttle());
		setShuttle(pMmcEvent->shuttle());
		break;
	case qtractorMmcEvent::STEP:
		sMmcText = tr("STEP %1").arg(pMmcEvent->step());
		setStep(pMmcEvent->step());
		break;
	case qtractorMmcEvent::MASKED_WRITE:
		switch (pMmcEvent->scmd()) {
		case qtractorMmcEvent::TRACK_RECORD:
			sMmcText = tr("TRACK RECORD %1 %2")
				.arg(pMmcEvent->track())
				.arg(pMmcEvent->isOn());
			break;
		case qtractorMmcEvent::TRACK_MUTE:
			sMmcText = tr("TRACK MUTE %1 %2")
				.arg(pMmcEvent->track())
				.arg(pMmcEvent->isOn());
			break;
		case qtractorMmcEvent::TRACK_SOLO:
			sMmcText = tr("TRACK SOLO %1 %2")
				.arg(pMmcEvent->track())
				.arg(pMmcEvent->isOn());
			break;
		default:
			sMmcText = tr("Unknown sub-command");
			break;
		}
		setTrack(pMmcEvent->scmd(), pMmcEvent->track(), pMmcEvent->isOn());
		break;
	default:
		sMmcText = tr("Not implemented");
		break;
	}

	appendMessages("MMC: " + sMmcText);
	stabilizeForm();
}


// Context menu event handler.
void qtractorMainForm::contextMenuEvent( QContextMenuEvent *pEvent )
{
	stabilizeForm();

	// Primordial edit menu should be available...
	m_ui.editMenu->exec(pEvent->globalPos());
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Brainless public property accessors.

// The global options settings property.
qtractorOptions *qtractorMainForm::options (void) const
{
	return m_pOptions;
}

// The global session reference.
qtractorSession *qtractorMainForm::session (void) const
{
	return m_pSession;
}

// The global session tracks reference.
qtractorTracks *qtractorMainForm::tracks (void) const
{
	return m_pTracks;
}

// The global session file(lists) reference.
qtractorFiles *qtractorMainForm::files (void) const
{
	return m_pFiles;
}

// The global session connections reference.
qtractorConnections *qtractorMainForm::connections (void) const
{
	return m_pConnections;
}

// The global session mixer reference.
qtractorMixer *qtractorMainForm::mixer (void) const
{
	return m_pMixer;
}

// The global undoable command list reference.
qtractorCommandList *qtractorMainForm::commands (void) const
{
	return m_pCommands;
}

// The global instruments repository.
qtractorInstrumentList *qtractorMainForm::instruments (void) const
{
	return m_pInstruments;
}

// The session thumb-view widget accessor.
qtractorThumbView *qtractorMainForm::thumbView (void) const
{
	return m_pThumbView;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Session file stuff.

// Format the displayable session filename.
QString qtractorMainForm::sessionName ( const QString& sFilename )
{
	bool bCompletePath = (m_pOptions && m_pOptions->bCompletePath);
	QString sSessionName = sFilename;
	if (sSessionName.isEmpty())
		sSessionName = tr("Untitled") + QString::number(m_iUntitled);
	else if (!bCompletePath)
		sSessionName = QFileInfo(sSessionName).fileName();
	return sSessionName;
}


// Create a new session file from scratch.
bool qtractorMainForm::newSession (void)
{
	// Check if we can do it.
	if (!closeSession())
		return false;

	// Ok increment untitled count.
	m_iUntitled++;

	// Stabilize form.
	m_sFilename.clear();
	m_iDirtyCount = 0;
	appendMessages(tr("New session: \"%1\".").arg(sessionName(m_sFilename)));

	// Give us what we got, right now...
	updateSession();

	return true;
}


// Open an existing sampler session.
bool qtractorMainForm::openSession (void)
{
	if (m_pOptions == NULL)
		return false;

	// Ask for the filename to open...
	QString sFilename = QFileDialog::getOpenFileName(
		this,                                       // Parent.
		tr("Open Session") + " - " QTRACTOR_TITLE,  // Caption.
		m_pOptions->sSessionDir,                    // Start here.
		tr("Session files") + " (*.qtr)"            // Filter files.
	);

	// Have we cancelled?
	if (sFilename.isEmpty())
		return false;

	// Check if we're going to discard safely the current one...
	if (!closeSession())
		return false;

	// Load it right away.
	return loadSessionFile(sFilename);
}


// Save current sampler session with another name.
bool qtractorMainForm::saveSession ( bool bPrompt )
{
	if (m_pOptions == NULL)
		return false;

	// It must be a session name...
	if (m_pSession->sessionName().isEmpty() && !editSession())
		return false;

	// Suggest a filename, if there's none...
	QString sFilename = m_sFilename;

	if (sFilename.isEmpty()) {
		sFilename = QFileInfo(m_pOptions->sSessionDir,
			m_pSession->sessionName()).filePath();
		bPrompt = true;
	}

	// Ask for the file to save...
	if (bPrompt) {
		// If none is given, assume default directory.
		// Prompt the guy...
		sFilename = QFileDialog::getSaveFileName(
			this,                                       // Parent.
			tr("Save Session") + " - " QTRACTOR_TITLE,  // Caption.
			sFilename,                                  // Start here.
			tr("Session files") + " (*.qtr)"            // Filter files.
		);
		// Have we cancelled it?
		if (sFilename.isEmpty())
			return false;
		// Enforce .qtr extension...
		const QString sExt("qtr");
		if (QFileInfo(sFilename).suffix() != sExt)
			sFilename += '.' + sExt;
		// Check if already exists...
		if (sFilename != m_sFilename && QFileInfo(sFilename).exists()) {
			if (QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("The file already exists:\n\n"
				"\"%1\"\n\n"
				"Do you want to replace it?")
				.arg(sFilename),
				tr("Replace"), tr("Cancel")) > 0)
				return false;
		}
	}

	// Save it right away.
	return saveSessionFile(sFilename);
}


// Edit session properties.
bool qtractorMainForm::editSession (void)
{
	// Session Properties...
	qtractorSessionForm sessionForm(this);
	sessionForm.setSession(m_pSession);
	if (!sessionForm.exec())
		return false;

	// If currently playing, we need to do a stop and go...
	bool bPlaying = m_pSession->isPlaying();
	if (bPlaying)
		m_pSession->lock();

	// Take care of session name changes...
	const QString sOldSessionName = m_pSession->sessionName();
	// And tempo changes too...
	float fOldTempo = m_pSession->tempo();

	// Now, express the change as a undoable command...
	m_pCommands->exec(
		new qtractorPropertyCommand<qtractorSession::Properties> (
			tr("session properties"), m_pSession->properties(),
				sessionForm.properties()));

	// If session name has changed, we'll prompt
	// for correct filename when save is triggered...
	if (m_pSession->sessionName() != sOldSessionName)
		m_sFilename.clear();

	// Restore playback state, if needed...
	if (bPlaying) {
		// On tempo change, the MIDI engine queue needs a reset...
		if (m_pSession->midiEngine() && m_pSession->tempo() != fOldTempo)
			m_pSession->midiEngine()->resetTempo();
		m_pSession->unlock();
		m_iTransportUpdate++;
	}
	
	// Done.
	return true;
}



// Close current session.
bool qtractorMainForm::closeSession (void)
{
	bool bClose = true;

	// Are we dirty enough to prompt it?
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("The current session has been changed:\n\n"
			"\"%1\"\n\n"
			"Do you want to save the changes?")
			.arg(sessionName(m_sFilename)),
			tr("Save"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Save...
			bClose = saveSession(false);
			// Fall thru....
		case 1:     // Discard
			break;
		default:    // Cancel.
			bClose = false;
			break;
		}
	}

	// If we may close it, dot it.
	if (bClose) {
		// Just in case we were in the middle of something...
		if (m_pSession->isPlaying()) {
			m_ui.transportPlayAction->setChecked(false);
			transportPlay(); // Toggle playing!
		}
		// Reset all dependables to default.
		m_pCommands->clear();
		m_pMixer->clear();
		m_pFiles->clear();
		// Close session engines.
		m_pSession->close();
		m_pSession->clear();
		// And last but not least.
		m_pConnections->clear();
		m_pTracks->clear();
		// Reset playhead.
		m_iPlayHead = 0;
		// We're now clean, for sure.
		m_iDirtyCount = 0;
		appendMessages(tr("Session closed."));
	}

	return bClose;
}


// Load a session from specific file path.
bool qtractorMainForm::loadSessionFile ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::loadSessionFile(\"" + sFilename + "\")");
#endif

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Read the file.
	QDomDocument doc("qtractorSession");
	bool bResult = qtractorSessionDocument(
		&doc, m_pSession, m_pFiles).load(sFilename);

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	if (bResult) {
		// We're not dirty anymore.
		updateRecentFiles(sFilename);
		m_iDirtyCount = 0;
	} else {
		// Something went wrong...
		appendMessagesError(
			tr("Session could not be loaded\n"
			"from \"%1\".\n\n"
			"Sorry.").arg(sFilename));
	}

	// Save as default session directory.
	if (m_pOptions)
		m_pOptions->sSessionDir = QFileInfo(sFilename).absolutePath();
	// Stabilize form...
	m_sFilename = sFilename;
	appendMessages(tr("Open session: \"%1\".").arg(sessionName(m_sFilename)));

	// Now we'll try to create (update) the whole GUI session.
	updateSession();
	return bResult;
}


// Save current session to specific file path.
bool qtractorMainForm::saveSessionFile ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::saveSessionFile(\"" + sFilename + "\")");
#endif

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Have we any errors?
	QDomDocument doc("qtractorSession");
	bool bResult = qtractorSessionDocument(
		&doc, m_pSession, m_pFiles).save(sFilename);

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	if (bResult) {
		// We're not dirty anymore.
		updateRecentFiles(sFilename);
		m_iDirtyCount = 0;
	} else {
		// Something went wrong...
		appendMessagesError(
			tr("Session could not be saved\n"
			"to \"%1\".\n\n"
			"Sorry.").arg(sFilename));
	}

	// Save as default session directory.
	if (m_pOptions)
		m_pOptions->sSessionDir = QFileInfo(sFilename).absolutePath();
	// Stabilize form...
	m_sFilename = sFilename;
	appendMessages(tr("Save session: \"%1\".").arg(sessionName(m_sFilename)));
	stabilizeForm();
	return bResult;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- File Action slots.

// Create a new sampler session.
void qtractorMainForm::fileNew (void)
{
	// Of course we'll start clean new.
	newSession();
}


// Open an existing sampler session.
void qtractorMainForm::fileOpen (void)
{
	// Open it right away.
	openSession();
}


// Open a recent file session.
void qtractorMainForm::fileOpenRecent (void)
{
	// Retrive filename index from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction && m_pOptions) {
		int iIndex = pAction->data().toInt();
		if (iIndex >= 0 && iIndex < m_pOptions->recentFiles.count()) {
			QString sFilename = m_pOptions->recentFiles[iIndex];
			// Check if we can safely close the current session...
			if (!sFilename.isEmpty() && closeSession())
				loadSessionFile(sFilename);
		}
	}
}


// Save current sampler session.
void qtractorMainForm::fileSave (void)
{
	// Save it right away.
	saveSession(false);
}


// Save current sampler session with another name.
void qtractorMainForm::fileSaveAs (void)
{
	// Save it right away, maybe with another name.
	saveSession(true);
}


// Edit session properties.
void qtractorMainForm::fileProperties (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::fileProperties()");
#endif

	// Session Properties...
	editSession();
}


// Exit application program.
void qtractorMainForm::fileExit (void)
{
	// Go for close the whole thing.
	close();
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Edit Action slots.

// Undo last action.
void qtractorMainForm::editUndo (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editUndo()");
#endif

	m_pCommands->undo();
}


// Redo last action.
void qtractorMainForm::editRedo (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editRedo()");
#endif

	m_pCommands->redo();
}


// Cut selection to clipboard.
void qtractorMainForm::editCut (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editCut()");
#endif

	// Cut selection...
	if (m_pTracks)
		m_pTracks->cutClipSelect();
}


// Copy selection to clipboard.
void qtractorMainForm::editCopy (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editCopy()");
#endif

	// Copy selection...
	if (m_pTracks)
		m_pTracks->copyClipSelect();

	stabilizeForm();
}


// Paste clipboard contents.
void qtractorMainForm::editPaste (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editPaste()");
#endif

	// Paste selection...
	if (m_pTracks)
		m_pTracks->pasteClipSelect();
}


// Delete selection.
void qtractorMainForm::editDelete (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editDelete()");
#endif

	// Delete selection...
	if (m_pTracks)
		m_pTracks->deleteClipSelect();
}


// Set selection to whole clip mode.
void qtractorMainForm::editSelectModeClip (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editSelectModeClip()");
#endif

	// Select clip mode...
	if (m_pTracks)
		m_pTracks->trackView()->setSelectMode(qtractorTrackView::SelectClip);
	if (m_pOptions)
		m_pOptions->iTrackViewSelectMode = 0;

	stabilizeForm();
}


// Set selection to range mode.
void qtractorMainForm::editSelectModeRange (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editSelectModeRange()");
#endif

	// Select clip mode...
	if (m_pTracks)
		m_pTracks->trackView()->setSelectMode(qtractorTrackView::SelectRange);
	if (m_pOptions)
		m_pOptions->iTrackViewSelectMode = 1;

	stabilizeForm();
}


// Set selection to rectangularmode.
void qtractorMainForm::editSelectModeRect (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editSelectModeRect()");
#endif

	// Select clip mode...
	if (m_pTracks)
		m_pTracks->trackView()->setSelectMode(qtractorTrackView::SelectRect);
	if (m_pOptions)
		m_pOptions->iTrackViewSelectMode = 2;

	stabilizeForm();
}


// Mark all as unselected.
void qtractorMainForm::editSelectNone (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editSelectNone()");
#endif

	// Select Track...
	if (m_pTracks)
		m_pTracks->selectAll(false);

	stabilizeForm();
}


// Mark range as selected.
void qtractorMainForm::editSelectRange (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editSelectRange()");
#endif

	// Select Track...
	if (m_pTracks)
		m_pTracks->selectEditRange();

	stabilizeForm();
}


// Mark track as selected.
void qtractorMainForm::editSelectTrack (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editSelectTrack()");
#endif

	// Select Track...
	if (m_pTracks)
		m_pTracks->selectCurrentTrack();

	stabilizeForm();
}


// Mark all as selected.
void qtractorMainForm::editSelectAll (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editSelectAll()");
#endif

	// Select All...
	if (m_pTracks)
		m_pTracks->selectAll();

	stabilizeForm();
}


// Enter in clip edit mode.
void qtractorMainForm::editClip (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::editClip()");
#endif

	// Start editing the current clip, if any...
	if (m_pTracks)
		m_pTracks->editClip();
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Track Action slots.

// Add a new track to session.
void qtractorMainForm::trackAdd (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::trackAdd()");
#endif

	// Add Track...
	if (m_pTracks)
		m_pTracks->addTrack();
}


// Remove current track from session.
void qtractorMainForm::trackRemove (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::trackAdd()");
#endif

	// Remove Track...
	if (m_pTracks)
		m_pTracks->removeTrack();
}


// Edit track properties on session.
void qtractorMainForm::trackProperties (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::trackProperties()");
#endif

	// Track Properties...
	if (m_pTracks)
		m_pTracks->editTrack();
}


// Import some tracks from Audio file.
void qtractorMainForm::trackImportAudio (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::trackImportAudio()");
#endif

	// Import Audio files into tracks...
	if (m_pTracks) {
		unsigned long iClipStart = m_pSession->editHead();
		m_pTracks->addAudioTracks(m_pFiles->audioListView()->openFileNames(),
			iClipStart);
		m_pTracks->trackView()->ensureVisibleFrame(iClipStart);
	}
}


// Import some tracks from MIDI file.
void qtractorMainForm::trackImportMidi (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::trackImportMidi()");
#endif

	// Import MIDI files into tracks...
	if (m_pTracks) {
		unsigned long iClipStart = m_pSession->editHead();
		m_pTracks->addMidiTracks(m_pFiles->midiListView()->openFileNames(),
			iClipStart);
		m_pTracks->trackView()->ensureVisibleFrame(iClipStart);
	}
}


//-------------------------------------------------------------------------
// qtractorMainForm -- View Action slots.

// Show/hide the main program window menubar.
void qtractorMainForm::viewMenubar ( bool bOn )
{
	if (bOn)
		m_ui.MenuBar->show();
	else
		m_ui.MenuBar->hide();
}


// Show/hide the main program window statusbar.
void qtractorMainForm::viewStatusbar ( bool bOn )
{
	if (bOn)
		statusBar()->show();
	else
		statusBar()->hide();
}


// Show/hide the file-toolbar.
void qtractorMainForm::viewToolbarFile ( bool bOn )
{
	if (bOn) {
		m_ui.fileToolbar->show();
	} else {
		m_ui.fileToolbar->hide();
	}
}


// Show/hide the edit-toolbar.
void qtractorMainForm::viewToolbarEdit ( bool bOn )
{
	if (bOn) {
		m_ui.editToolbar->show();
	} else {
		m_ui.editToolbar->hide();
	}
}


// Show/hide the track-toolbar.
void qtractorMainForm::viewToolbarTrack ( bool bOn )
{
	if (bOn) {
		m_ui.trackToolbar->show();
	} else {
		m_ui.trackToolbar->hide();
	}
}


// Show/hide the view-toolbar.
void qtractorMainForm::viewToolbarView ( bool bOn )
{
	if (bOn) {
		m_ui.viewToolbar->show();
	} else {
		m_ui.viewToolbar->hide();
	}
}


// Show/hide the transport toolbar.
void qtractorMainForm::viewToolbarTransport ( bool bOn )
{
	if (bOn) {
		m_ui.transportToolbar->show();
	} else {
		m_ui.transportToolbar->hide();
	}
}


// Show/hide the time toolbar.
void qtractorMainForm::viewToolbarTime ( bool bOn )
{
	if (bOn) {
		m_ui.timeToolbar->show();
	} else {
		m_ui.timeToolbar->hide();
	}
}


// Show/hide the thumb (track-line)ime toolbar.
void qtractorMainForm::viewToolbarThumb ( bool bOn )
{
	if (bOn) {
		m_ui.thumbToolbar->show();
	} else {
		m_ui.thumbToolbar->hide();
	}
}


// Show/hide the files window view.
void qtractorMainForm::viewFiles ( bool bOn )
{
	if (bOn) {
		m_pFiles->show();
	} else {
		m_pFiles->hide();
	}
}


// Show/hide the messages window logger.
void qtractorMainForm::viewMessages ( bool bOn )
{
	if (bOn) {
		m_pMessages->show();
	} else {
		m_pMessages->hide();
	}
}


// Show/hide the mixer window.
void qtractorMainForm::viewMixer ( bool bOn )
{
	if (m_pOptions)
		m_pOptions->saveWidgetGeometry(m_pMixer);

	if (bOn) {
		m_pMixer->show();
	} else {
		m_pMixer->hide();
	}
}


// Show/hide the connections window.
void qtractorMainForm::viewConnections ( bool bOn )
{
	if (m_pOptions)
		m_pOptions->saveWidgetGeometry(m_pConnections);

	if (bOn) {
		m_pConnections->show();
	} else {
		m_pConnections->hide();
	}
}


// Refresh view display.
void qtractorMainForm::viewRefresh (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::viewRefresh()");
#endif

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Update the whole session view dependables...
	m_pSession->updateTimeScale();
	m_pSession->updateSessionLength();

	if (m_pTracks)
		m_pTracks->updateContents(true);
	if (m_pConnections)
		m_pConnections->refresh();
	if (m_pMixer) {
		m_pMixer->updateBuses();
		m_pMixer->updateTracks();
	}

	m_pThumbView->update();

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	stabilizeForm();
}


// Show instruments dialog.
void qtractorMainForm::viewInstruments (void)
{
	// Just set and show the instruments dialog...
	qtractorInstrumentForm(this).exec();
}


// Show buses dialog.
void qtractorMainForm::viewBuses (void)
{
	// Just set and show the instruments dialog...
	qtractorBusForm(this).exec();
}


// Show options dialog.
void qtractorMainForm::viewOptions (void)
{
	if (m_pOptions == NULL)
		return;

	// Check out some initial nullities(tm)...
	if (m_pOptions->sMessagesFont.isEmpty() && m_pMessages)
		m_pOptions->sMessagesFont = m_pMessages->messagesFont().toString();
	// To track down deferred or immediate changes.
	QString sOldMessagesFont    = m_pOptions->sMessagesFont;
	bool    bOldStdoutCapture   = m_pOptions->bStdoutCapture;
	int     bOldMessagesLimit   = m_pOptions->bMessagesLimit;
	int     iOldMessagesLimitLines = m_pOptions->iMessagesLimitLines;
	bool    bOldCompletePath    = m_pOptions->bCompletePath;
	bool    bOldPeakAutoRemove  = m_pOptions->bPeakAutoRemove;
	int     iOldMaxRecentFiles  = m_pOptions->iMaxRecentFiles;
	int     iOldResampleType    = m_pOptions->iResampleType;
	int     iOldTransportTime   = m_pOptions->iTransportTime;
	// Load the current setup settings.
	qtractorOptionsForm optionsForm(this);
	optionsForm.setOptions(m_pOptions);
	// Show the setup dialog...
	if (optionsForm.exec()) {
		// Check wheather something immediate has changed.
		QString sNeedRestart;
		if (iOldResampleType != m_pOptions->iResampleType) {
			qtractorAudioBuffer::setResampleType(m_pOptions->iResampleType);
			sNeedRestart += tr("session");
		}
		if (( bOldStdoutCapture && !m_pOptions->bStdoutCapture) ||
			(!bOldStdoutCapture &&  m_pOptions->bStdoutCapture)) {
			updateMessagesCapture();
			if (!sNeedRestart.isEmpty())
				sNeedRestart += tr(" or ");
			sNeedRestart += tr("program");
		}
		if (( bOldCompletePath && !m_pOptions->bCompletePath) ||
			(!bOldCompletePath &&  m_pOptions->bCompletePath) ||
			(iOldMaxRecentFiles != m_pOptions->iMaxRecentFiles))
			updateRecentFilesMenu();
		if (( bOldPeakAutoRemove && !m_pOptions->bPeakAutoRemove) ||
			(!bOldPeakAutoRemove &&  m_pOptions->bPeakAutoRemove))
			updatePeakAutoRemove();
		if (sOldMessagesFont != m_pOptions->sMessagesFont)
			updateMessagesFont();
		if (( bOldMessagesLimit && !m_pOptions->bMessagesLimit) ||
			(!bOldMessagesLimit &&  m_pOptions->bMessagesLimit) ||
			(iOldMessagesLimitLines !=  m_pOptions->iMessagesLimitLines))
			updateMessagesLimit();
		if (iOldTransportTime != m_pOptions->iTransportTime)
			updateDisplayFormat();
		// FIXME: This is what it should ever be,
		// make it right from this very moment...
		qtractorAudioFileFactory::setDefaultType(
			m_pOptions->sCaptureExt,
			m_pOptions->iCaptureType,
			m_pOptions->iCaptureFormat,
			m_pOptions->iCaptureQuality);
		// Warn if something will be only effective on next time.
		if (!sNeedRestart.isEmpty()) {
			QMessageBox::information(this,
				tr("Information") + " - " QTRACTOR_TITLE,
				tr("Some settings may be only effective\n"
				"next time you start this %1.").arg(sNeedRestart), tr("OK"));
		}
	}

	// This makes it.
	stabilizeForm();
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Transport Action slots.

// Transport backward.
void qtractorMainForm::transportBackward (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportBackward()");
#endif

	// Make sure session is activated...
	checkRestartSession();

	// Move playhead to edit-tail, head or full session-start.
	unsigned long iPlayHead = m_pSession->playHead();
	if (iPlayHead > m_pSession->editTail() && !m_pSession->isPlaying())
		iPlayHead = m_pSession->editTail();
	else
	if (iPlayHead > m_pSession->editHead())
		iPlayHead = m_pSession->editHead();
	else
		iPlayHead = 0;
	m_pSession->setPlayHead(iPlayHead);
	m_iTransportUpdate++;

	stabilizeForm();
}


// Transport rewind.
void qtractorMainForm::transportRewind (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportRewind()");
#endif

	// Make sure session is activated...
	if (!checkRestartSession())
		return;

	// Toggle rolling backward...
	if (setRolling(-1) < 0) {
		// Send MMC STOP command...
		m_pSession->midiEngine()->sendMmcCommand(
			qtractorMmcEvent::STOP);
	} else {
		// Send MMC REWIND command...
		m_pSession->midiEngine()->sendMmcCommand(
			qtractorMmcEvent::REWIND);
	}

	stabilizeForm();
}


// Transport fast-forward
void qtractorMainForm::transportFastForward (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportFastForward()");
#endif

	// Make sure session is activated...
	if (!checkRestartSession())
		return;

	// Toggle rolling backward...
	if (setRolling(+1) > 0) {
		// Send MMC STOP command...
		m_pSession->midiEngine()->sendMmcCommand(
			qtractorMmcEvent::STOP);
	} else {
		// Send MMC FAST_FORWARD command...
		m_pSession->midiEngine()->sendMmcCommand(
			qtractorMmcEvent::FAST_FORWARD);
	}

	stabilizeForm();
}


// Transport forward
void qtractorMainForm::transportForward (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportForward()");
#endif

	// Make sure session is activated...
	checkRestartSession();

	// Move playhead to edit-head, tail or full session-end.
	unsigned long iPlayHead = m_pSession->playHead();
	if (iPlayHead < m_pSession->editHead())
		iPlayHead = m_pSession->editHead();
	else
	if (iPlayHead < m_pSession->editTail())
		iPlayHead = m_pSession->editTail();
	else
		iPlayHead = m_pSession->sessionLength();
	m_pSession->setPlayHead(iPlayHead);
	m_iTransportUpdate++;

	stabilizeForm();
}


// Transport loop.
void qtractorMainForm::transportLoop (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportLoop()");
#endif

	// Make sure session is activated...
	checkRestartSession();

	// Do the loop switch...
	if (!m_pSession->isLooping()) {
		m_pSession->setLoop(m_pSession->editHead(), m_pSession->editTail());
	} else {
		m_pSession->setLoop(0, 0);
	}

	// Refresh track views...
	if (m_pTracks) {
		m_pTracks->trackTime()->updateContents();
		m_pTracks->trackView()->updateContents();
	}

	// Done with loop switch...
	m_iDirtyCount++;
	stabilizeForm();
}


// Transport play.
void qtractorMainForm::transportPlay (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportPlay()");
#endif

	// Make sure session is activated...
	if (!checkRestartSession())
		return;

	// Toggle playing...
	bool bPlaying = !m_pSession->isPlaying();
	if (setPlaying(bPlaying)) {
		// Send MMC PLAY/STOP command...
		m_pSession->midiEngine()->sendMmcCommand(bPlaying ?
			qtractorMmcEvent::PLAY : qtractorMmcEvent::STOP);
	}

	stabilizeForm();
}


// Transport record.
void qtractorMainForm::transportRecord (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportRecord()");
#endif

	// Make sure session is activated...
	if (!checkRestartSession())
		return;

	// Toggle recording...
	bool bRecording = !m_pSession->isRecording();
	if (setRecording(bRecording)) {
		// Send MMC RECORD_STROBE/EXIT command...
		m_pSession->midiEngine()->sendMmcCommand(bRecording ?
			qtractorMmcEvent::RECORD_STROBE : qtractorMmcEvent::RECORD_EXIT);
	}

	stabilizeForm();
}


// Transport follow playhead
void qtractorMainForm::transportFollow (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::transportFollow()");
#endif

	// Toggle follow-playhead...
	stabilizeForm();
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Help Action slots.

// Show information about the Qt toolkit.
void qtractorMainForm::helpAboutQt (void)
{
	QMessageBox::aboutQt(this);
}


// Show information about application program.
void qtractorMainForm::helpAbout (void)
{
	// Stuff the about box text...
	QString sText = "<p>\n";
	sText += "<b>" QTRACTOR_TITLE " - " + tr(QTRACTOR_SUBTITLE) + "</b><br />\n";
	sText += "<br />\n";
	sText += tr("Version") + ": <b>" QTRACTOR_VERSION "</b><br />\n";
	sText += "<small>" + tr("Build") + ": " __DATE__ " " __TIME__ "</small><br />\n";
#ifdef CONFIG_DEBUG
	sText += "<small><font color=\"red\">";
	sText += tr("Debugging option enabled.");
	sText += "</font></small><br />";
#endif
#ifndef CONFIG_LIBVORBIS
    sText += "<small><font color=\"red\">";
    sText += tr("Ogg Vorbis (libvorbis) file support disabled.");
    sText += "</font></small><br />";
#endif
#ifndef CONFIG_LIBMAD
    sText += "<small><font color=\"red\">";
    sText += tr("MPEG-1 Audio Layer 3 (libmad) file support disabled.");
    sText += "</font></small><br />";
#endif
#ifndef CONFIG_LIBSAMPLERATE
    sText += "<small><font color=\"red\">";
    sText += tr("Sample-rate conversion (libsamplerate) disabled.");
    sText += "</font></small><br />";
#endif
	sText += "<br />\n";
	sText += tr("Website") + ": <a href=\"" QTRACTOR_WEBSITE "\">" QTRACTOR_WEBSITE "</a><br />\n";
	sText += "<br />\n";
	sText += "<small>";
	sText += QTRACTOR_COPYRIGHT "<br />\n";
	sText += "<br />\n";
	sText += tr("This program is free software; you can redistribute it and/or modify it") + "<br />\n";
	sText += tr("under the terms of the GNU General Public License version 2 or later.");
	sText += "</small>";
	sText += "</p>\n";

	QMessageBox::about(this, tr("About") + " " QTRACTOR_TITLE, sText);
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Internal transport stabilization.

bool qtractorMainForm::setPlaying ( bool bPlaying )
{
	// In case of (re)starting playback, send now
	// all tracks MIDI bank select/program changes...
	if (bPlaying)
		m_pSession->setMidiPatch(m_pInstruments);

	// Toggle engine play status...
	m_pSession->setPlaying(bPlaying);
	m_iTransportUpdate++;

	// We must stop certain things...
	if (!bPlaying) {
		// And shutdown recording anyway...
		if (m_pSession->isRecording() && setRecording(false))
			m_ui.transportRecordAction->setChecked(false);
		// Stop transport rolling, immediately...
		setRolling(0);
	}

	// Done with playback switch...
	return true;
}


bool qtractorMainForm::setRecording ( bool bRecording )
{
	// Avoid if no tracks are armed...
	if (m_pSession->recordTracks() < 1)
		return false;

	if (bRecording) {
		// Starting recording: we must have a session name...
		if (m_pSession->sessionName().isEmpty() && !editSession()) {
			m_ui.transportRecordAction->setChecked(false);
			return false;
		}
		// Will start recording...
	} else {
		// Stopping recording: fetch and commit
		// all new clips as a composite command...
		int iUpdate = 0;
		qtractorClipCommand *pClipCommand
			= new qtractorClipCommand(tr("record clip"));
		// For all non-empty clip on record...
		for (qtractorTrack *pTrack = m_pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			if (pClipCommand->addClipRecord(pTrack))
				iUpdate++;
		}
		// Put it in the form of an undoable command...
		if (iUpdate > 0) {
			m_pCommands->exec(pClipCommand);
		} else {
			// The allocated command is unhelpful...
			delete pClipCommand;
			// Try to postpone an overall refresh...
			if (m_iPeakTimer  < QTRACTOR_TIMER_DELAY)
				m_iPeakTimer += QTRACTOR_TIMER_DELAY;
		}
	}

	// Finally, toggle session record status...
	m_pSession->setRecording(bRecording);

	// Done with record switch...
	return true;
}


int qtractorMainForm::setRolling ( int iRolling )
{
	int iOldRolling = m_iTransportRolling;

	// Avoid if recording is armed...
	if (m_pSession->isRecording() || iOldRolling == iRolling)
		iRolling = 0;

	// Where were we?
	if (iOldRolling < 0 && iOldRolling < iRolling)
		m_ui.transportRewindAction->setChecked(false);
	else
	if (iOldRolling > 0 && iOldRolling > iRolling)
		m_ui.transportFastForwardAction->setChecked(false);

	// Set the rolling flag.
	m_iTransportRolling = iRolling;
	m_fTransportShuttle = float(iRolling);
	m_iTransportStep    = 0;

	// We've started something...
	if (m_iTransportRolling)
		m_iTransportUpdate++;

	// Done with rolling switch...
	return iOldRolling;
}


void qtractorMainForm::setLocate ( unsigned long iLocate )
{
	m_pSession->setPlayHead(m_pSession->frameFromLocate(iLocate));
	m_iTransportUpdate++;
}


void qtractorMainForm::setShuttle ( float fShuttle )
{
	float fOldShuttle = m_fTransportShuttle;

	if (fShuttle < 0.0f && fOldShuttle >= 0.0f && setRolling(-1) >= 0)
		m_ui.transportRewindAction->setChecked(true);
	else
	if (fShuttle > 0.0f && 0.0f >= fOldShuttle && 0 >= setRolling(+1))
		m_ui.transportFastForwardAction->setChecked(true);

	m_fTransportShuttle = fShuttle;
	m_iTransportUpdate++;
}


void qtractorMainForm::setStep ( int iStep )
{
	m_iTransportStep += iStep;
	m_iTransportUpdate++;
}


void qtractorMainForm::setTrack ( int scmd, int iTrack, bool bOn )
{
	if (m_pTracks) {
		// Find which ordinal track...
		qtractorTrack *pTrack = m_pTracks->trackList()->track(iTrack);
		if (pTrack) {
			// Set session track mode state...
			switch (qtractorMmcEvent::SubCommand(scmd)) {
			case qtractorMmcEvent::TRACK_RECORD:
				pTrack->setRecord(bOn);
				break;
			case qtractorMmcEvent::TRACK_MUTE:
				pTrack->setMute(bOn);
				break;
			case qtractorMmcEvent::TRACK_SOLO:
				pTrack->setSolo(bOn);
				break;
			default:
				break;
			}
			// Update track-buttons...
			m_pTracks->trackList()->updateTrack(pTrack);
			if (m_pMixer) {
				qtractorMixerStrip *pStrip
					= m_pMixer->trackRack()->findStrip(pTrack->monitor());
				if (pStrip)
					pStrip->updateTrackButtons();
			}
			// Done.
			stabilizeForm();
		}
	}
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Main window stabilization.

void qtractorMainForm::updateTransportTime ( unsigned long iPlayHead )
{
	m_pTransportTimeSpinBox->setValue(iPlayHead, false);
}


void qtractorMainForm::stabilizeForm (void)
{
#ifdef CONFIG_DEBUG_0
	appendMessages("qtractorMainForm::stabilizeForm()");
#endif

	// Update the main application caption...
	QString sSessionName = sessionName(m_sFilename);
	if (m_iDirtyCount > 0)
		sSessionName += ' ' + tr("[modified]");
	setWindowTitle(sSessionName + " - " QTRACTOR_TITLE);

	// Update the main menu state...
	m_ui.fileSaveAction->setEnabled(m_iDirtyCount > 0);

	// Update edit menu state...
	m_pCommands->updateAction(m_ui.editUndoAction, m_pCommands->lastCommand());
	m_pCommands->updateAction(m_ui.editRedoAction, m_pCommands->nextCommand());

	unsigned long iSessionLength = m_pSession->sessionLength();
	bool bEnabled = (m_pTracks && m_pTracks->currentTrack() != NULL);
	bool bSelected = (m_pTracks && m_pTracks->isClipSelected());
	bool bSelectable = (iSessionLength > 0);
	bool bEditable = (m_pTracks && m_pTracks->currentClip() != NULL);

	m_ui.editCutAction->setEnabled(bSelected);
	m_ui.editCopyAction->setEnabled(bSelected);
	m_ui.editPasteAction->setEnabled(m_pTracks && !m_pTracks->isClipboardEmpty());
	m_ui.editDeleteAction->setEnabled(bSelected);

	m_ui.editSelectAllAction->setEnabled(bSelectable);
	m_ui.editSelectTrackAction->setEnabled(bEnabled);
	if (bSelectable)
		bSelectable  = (m_pSession->editHead() < m_pSession->editTail());
	m_ui.editSelectRangeAction->setEnabled(bSelectable);
	m_ui.editSelectNoneAction->setEnabled(bSelected);

	m_ui.editClipAction->setEnabled(bEditable);

	// Update track menu state...
	m_ui.trackRemoveAction->setEnabled(bEnabled);
	m_ui.trackPropertiesAction->setEnabled(bEnabled);
	m_ui.trackImportAudioAction->setEnabled(m_pTracks != NULL);
	m_ui.trackImportMidiAction->setEnabled(m_pTracks != NULL);

	// Update view menu state...
	m_ui.viewFilesAction->setChecked(m_pFiles && m_pFiles->isVisible());
	m_ui.viewMessagesAction->setChecked(m_pMessages && m_pMessages->isVisible());
	m_ui.viewConnectionsAction->setChecked(m_pConnections && m_pConnections->isVisible());
	m_ui.viewMixerAction->setChecked(m_pMixer && m_pMixer->isVisible());

	// Recent files menu.
	m_ui.fileOpenRecentMenu->setEnabled(m_pOptions->recentFiles.count() > 0);

	// Always make the latest message visible.
	if (m_pMessages)
		m_pMessages->flushStdoutBuffer();

	// Session status...
	updateTransportTime(m_pSession->playHead());

	if (m_pTracks && m_pTracks->currentTrack()) {
		m_statusItems[StatusName]->setText(
			m_pTracks->currentTrack()->trackName().simplified());
	} else {
		m_statusItems[StatusName]->clear();
	}

	if (m_iDirtyCount > 0)
		m_statusItems[StatusMod]->setText(tr("MOD"));
	else
		m_statusItems[StatusMod]->clear();

	if (m_pSession->recordTracks() > 0)
		m_statusItems[StatusRec]->setText(tr("REC"));
	else
		m_statusItems[StatusRec]->clear();

	if (m_pSession->muteTracks() > 0)
		m_statusItems[StatusMute]->setText(tr("MUTE"));
	else
		m_statusItems[StatusMute]->clear();

	if (m_pSession->soloTracks() > 0)
		m_statusItems[StatusSolo]->setText(tr("SOLO"));
	else
		m_statusItems[StatusSolo]->clear();

	if (m_pSession->isLooping())
		m_statusItems[StatusLoop]->setText(tr("LOOP"));
	else
		m_statusItems[StatusLoop]->clear();

	m_statusItems[StatusTime]->setText(
		m_pSession->timeFromFrame(iSessionLength));

	m_statusItems[StatusRate]->setText(
		tr("%1 Hz").arg(m_pSession->sampleRate()));

	bool bRecording = (m_pSession->isRecording()
		&& m_pSession->isPlaying()
		&& m_pSession->recordTracks() > 0);
	m_statusItems[StatusRec]->setPalette(*m_paletteItems[
		bRecording ? PaletteRed : PaletteNone]);
	m_statusItems[StatusMute]->setPalette(*m_paletteItems[
		m_pSession->muteTracks() > 0 ? PaletteYellow : PaletteNone]);
	m_statusItems[StatusSolo]->setPalette(*m_paletteItems[
		m_pSession->soloTracks() > 0 ? PaletteCyan : PaletteNone]);
	m_statusItems[StatusLoop]->setPalette(*m_paletteItems[
		m_pSession->isLooping() ? PaletteGreen : PaletteNone]);

	// Transport stuff...
	bEnabled = (!m_pSession->isPlaying() || !m_pSession->isRecording());
	m_ui.transportBackwardAction->setEnabled(bEnabled && m_iPlayHead > 0);
	m_ui.transportRewindAction->setEnabled(bEnabled && m_iPlayHead > 0);
	m_ui.transportFastForwardAction->setEnabled(bEnabled);
	m_ui.transportForwardAction->setEnabled(bEnabled
		&& (m_iPlayHead < iSessionLength
			|| m_iPlayHead < m_pSession->editHead()
			|| m_iPlayHead < m_pSession->editTail()));
	m_ui.transportLoopAction->setEnabled(bEnabled
		&& (m_pSession->isLooping() || bSelectable));
	m_ui.transportRecordAction->setEnabled(m_pSession->recordTracks() > 0);

	// Special record mode settlement.
	m_pTransportTimeSpinBox->setReadOnly(bRecording);
	m_pTempoSpinBox->setReadOnly(bRecording);

	m_pThumbView->update();
	m_pThumbView->updateThumb();
}


// Actually start all session engines.
bool qtractorMainForm::startSession (void)
{
	m_iTransportUpdate  = 0; 
	m_iTransportDelta   = 0;
	m_iTransportRolling = 0;
	m_fTransportShuttle = 0.0f;
	m_iTransportStep    = 0;

	m_iXrunCount = 0;
	m_iXrunSkip  = 0;
	m_iXrunTimer = 0;

	m_iAudioRefreshTimer = 0;
	m_iMidiRefreshTimer  = 0;

	unsigned int iOldSampleRate = m_pSession->sampleRate();

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	bool bResult = m_pSession->open(QTRACTOR_TITLE);
	QApplication::restoreOverrideCursor();

	if (bResult) {
		appendMessages(tr("Session started."));
		// HACK: Special treatment for disparate sample rates,
		// and only for (just loaded) non empty sessions...
		if (m_pSession->sampleRate() != iOldSampleRate
			&& m_pSession->sessionLength() > 0) {
			appendMessagesError(
				tr("The original session sample rate (%1 Hz)\n"
				"is not the same of the current audio engine (%2 Hz).\n\n"
				"Saving and reloading from a new session file\n"
				"is highly recommended.")
				.arg(iOldSampleRate)
				.arg(m_pSession->sampleRate()));
			// We'll doing the conversion right here and right now...
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			m_pSession->updateSampleRate(iOldSampleRate);
			QApplication::restoreOverrideCursor();
			m_iDirtyCount++;
		}
	} else {
		// Uh-oh, we can't go on like this...
		appendMessagesError(
			tr("The audio/MIDI engine could not be started.\n\n"
			"Make sure the JACK audio server (jackd) and/or\n"
			"the ALSA Sequencer kernel module (snd-seq-midi)\n"
			"are up and running and then restart the session."));
	}

	return bResult;
}


// Check and restart session, if applicable.
bool qtractorMainForm::checkRestartSession (void)
{
	// Whether session is currently activated,
	// try to (re)open the whole thing...
	if (!m_pSession->isActivated()) {
		// Save current playhead position, if any...
		unsigned long iPlayHead = m_pSession->playHead();
		// Bail out if can't start it...
		if (!startSession()) {
			// Can go on with no-business...
			m_ui.transportRewindAction->setChecked(false);
			m_ui.transportFastForwardAction->setChecked(false);
			m_ui.transportRecordAction->setChecked(false);
			m_ui.transportPlayAction->setChecked(false);
			stabilizeForm();
			return false;
		}
		// Restore previous playhead position...
		m_pSession->setPlayHead(iPlayHead);
	}

	return true;
}


// Grab and restore current sampler channels session.
void qtractorMainForm::updateSession (void)
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::updateSession()");
#endif

	// Initialize toolbar widgets...
	m_pTempoSpinBox->setValue(m_pSession->tempo());
	m_pSnapPerBeatComboBox->setCurrentIndex(
		qtractorTimeScale::indexFromSnap(m_pSession->snapPerBeat()));

	// Remake loop state...
	m_ui.transportLoopAction->setChecked(m_pSession->isLooping());

	//  Actually (re)start session engines...
	if (startSession()) {
		// (Re)set playhead...
		m_pSession->setPlayHead(0);
		// (Re)initialize MIDI instrument patching...
		m_pSession->setMidiPatch(m_pInstruments);
		// Get on with the special ALSA sequencer notifier...
		if (m_pSession->midiEngine()->alsaNotifier()) {
			QObject::connect(m_pSession->midiEngine()->alsaNotifier(),
				SIGNAL(activated(int)),
				SLOT(alsaNotify()));			
		}
	}

	// Update the session views...
	viewRefresh();

	// Ah, make it stand right.
	if (m_pTracks)
		m_pTracks->trackView()->setFocus();
}


// Update the recent files list and menu.
void qtractorMainForm::updateRecentFiles ( const QString& sFilename )
{
	if (m_pOptions == NULL)
		return;

	// Remove from list if already there (avoid duplicates)
	int iIndex = m_pOptions->recentFiles.indexOf(sFilename);
	if (iIndex >= 0)
		m_pOptions->recentFiles.removeAt(iIndex);
	// Put it to front...
	m_pOptions->recentFiles.push_front(sFilename);
}


// Update the recent files list and menu.
void qtractorMainForm::updateRecentFilesMenu (void)
{
	if (m_pOptions == NULL)
		return;

	// Time to keep the list under limits.
	int iRecentFiles = m_pOptions->recentFiles.count();
	while (iRecentFiles > m_pOptions->iMaxRecentFiles) {
		m_pOptions->recentFiles.pop_back();
		iRecentFiles--;
	}

	// Rebuild the recent files menu...
	m_ui.fileOpenRecentMenu->clear();
	for (int i = 0; i < iRecentFiles; i++) {
		const QString& sFilename = m_pOptions->recentFiles[i];
		if (QFileInfo(sFilename).exists()) {
			QAction *pAction = m_ui.fileOpenRecentMenu->addAction(
				QString("&%1 %2").arg(i + 1).arg(sessionName(sFilename)),
				this, SLOT(fileOpenRecent()));
			pAction->setData(i);
		}
	}
}


// Force update of the peak-files auto-remove mode.
void qtractorMainForm::updatePeakAutoRemove (void)
{
	if (m_pOptions == NULL)
		return;

	qtractorAudioPeakFactory *pAudioPeakFactory
		= m_pSession->audioPeakFactory();
	if (pAudioPeakFactory)
		pAudioPeakFactory->setAutoRemove(m_pOptions->bPeakAutoRemove);	
}


// Update main transport-time display format.
void qtractorMainForm::updateDisplayFormat (void)
{
	if (m_pOptions == NULL)
		return;

	// Main transport display format is due...
	qtractorSpinBox::DisplayFormat displayFormat;
	switch (m_pOptions->iTransportTime) {
	case 2:
		displayFormat = qtractorSpinBox::BBT;
		break;
	case 1:
		displayFormat = qtractorSpinBox::Time;
		break;
	case 0:
	default:
		displayFormat = qtractorSpinBox::Frames;
		break;
	}

	m_pTransportTimeSpinBox->setDisplayFormat(displayFormat);
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Messages window form handlers.

// Messages output methods.
void qtractorMainForm::appendMessages( const QString& s )
{
	if (m_pMessages)
		m_pMessages->appendMessages(s);

	statusBar()->showMessage(s, 3000);
}

void qtractorMainForm::appendMessagesColor( const QString& s, const QString& c )
{
	if (m_pMessages)
		m_pMessages->appendMessagesColor(s, c);

	statusBar()->showMessage(s, 3000);
}

void qtractorMainForm::appendMessagesText( const QString& s )
{
	if (m_pMessages)
		m_pMessages->appendMessagesText(s);
}

void qtractorMainForm::appendMessagesError( const QString& s )
{
	if (m_pMessages)
		m_pMessages->show();

	appendMessagesColor(s.simplified(), "#ff0000");

	QMessageBox::critical(this,
		tr("Error") + " - " QTRACTOR_TITLE, s, tr("Cancel"));
}


// Force update of the messages font.
void qtractorMainForm::updateMessagesFont (void)
{
	if (m_pOptions == NULL)
		return;

	if (m_pMessages && !m_pOptions->sMessagesFont.isEmpty()) {
		QFont font;
		if (font.fromString(m_pOptions->sMessagesFont))
			m_pMessages->setMessagesFont(font);
	}
}


// Update messages window line limit.
void qtractorMainForm::updateMessagesLimit (void)
{
	if (m_pOptions == NULL)
		return;

	if (m_pMessages) {
		if (m_pOptions->bMessagesLimit)
			m_pMessages->setMessagesLimit(m_pOptions->iMessagesLimitLines);
		else
			m_pMessages->setMessagesLimit(-1);
	}
}


// Enablement of the messages capture feature.
void qtractorMainForm::updateMessagesCapture (void)
{
	if (m_pOptions == NULL)
		return;

	if (m_pMessages)
		m_pMessages->setCaptureEnabled(m_pOptions->bStdoutCapture);
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Timer stuff.

// Timer slot funtion.
void qtractorMainForm::timerSlot (void)
{
	// Currrent state...
	bool bPlaying  = m_pSession->isPlaying();
	long iPlayHead = (long) m_pSession->playHead();
	
	// Playhead status...
	if (iPlayHead != (long) m_iPlayHead) {
		m_iPlayHead = iPlayHead;
		if (m_pTracks) {
			m_pTracks->trackView()->setPlayHead(iPlayHead,
				m_ui.transportFollowAction->isChecked());
		}
		if (!bPlaying && m_iTransportRolling == 0 && m_iTransportStep == 0) {
			// Send MMC LOCATE command...
			m_pSession->midiEngine()->sendMmcLocate(
				m_pSession->locateFromFrame(iPlayHead));
			// Update transport status anyway...
			m_iTransportUpdate++;
		}
	}

	// Transport status...
	if (m_iTransportUpdate > 0) {
		// Do some transport related tricks...
		if (m_iTransportRolling == 0) {
			m_iTransportUpdate = 0;
			if (m_iTransportStep) {
				// Transport stepping over...
				iPlayHead += (long) (m_iTransportStep
					* m_pSession->sampleRate()) / 20;
				if (iPlayHead < 0)
					iPlayHead = 0;
				m_iTransportStep = 0;
				// Make it thru...
				m_pSession->setPlayHead(iPlayHead);
			}
		} else {
			// Transport rolling over...
			iPlayHead += (long) (m_fTransportShuttle
				* float(m_pSession->sampleRate())) / 2;
			if (iPlayHead < 0) {
				iPlayHead = 0;
				m_iTransportUpdate = 0;
				// Stop playback for sure...
				if (setPlaying(false)) {
					m_ui.transportPlayAction->setChecked(false);
					// Send MMC STOP command...
					m_pSession->midiEngine()->sendMmcCommand(
						qtractorMmcEvent::STOP);
				}
			}
			// Make it thru...
			m_pSession->setPlayHead(iPlayHead);
		}
		// Ensure track-view into visibility...
		if (m_pTracks)
			m_pTracks->trackView()->ensureVisibleFrame(iPlayHead);
		// Take the change to give some visual feedback...
		if (m_iTransportUpdate > 0) {
			updateTransportTime(iPlayHead);
			m_pThumbView->updateThumb();
		} else {
			stabilizeForm();
		}
		// Done with transport tricks.
	} else if (m_pSession->isActivated()) {
		// Read JACK transport state and react if out-of-sync..
		jack_client_t *pJackClient = m_pSession->audioEngine()->jackClient();
		if (pJackClient) {
			jack_position_t pos;
			jack_transport_state_t state
				= jack_transport_query(pJackClient, &pos);
			// Check on external transport state request changes...
			if ((state == JackTransportStopped &&  bPlaying) ||
				(state == JackTransportRolling && !bPlaying)) {
				if (!bPlaying)
					m_pSession->seek(pos.frame, true);
				m_ui.transportPlayAction->setChecked(!bPlaying);
				transportPlay();	// Toggle playing!
				if (bPlaying)
					m_pSession->seek(pos.frame, true);
			} else {
				// Check on external transport location changes;
				// note that we'll have a doubled buffer-size guard...
				long iDeltaFrame = (long) pos.frame - iPlayHead;
				int iBufferSize2 = m_pSession->audioEngine()->bufferSize() << 1;
				if (labs(iDeltaFrame) > iBufferSize2) {
					if (++m_iTransportDelta > 1) {
						m_iTransportDelta = 0;
						iPlayHead = pos.frame;
						m_pSession->setPlayHead(iPlayHead);
						m_iTransportUpdate++;
					}
				}	// All quiet...
				else m_iTransportDelta = 0;
			}
		}
		// Check if its time to refresh playhead timer...
		if (bPlaying && m_iPlayTimer < QTRACTOR_TIMER_DELAY) {
			m_iPlayTimer += QTRACTOR_TIMER_MSECS;
			if (m_iPlayTimer >= QTRACTOR_TIMER_DELAY) {
				m_iPlayTimer = 0;
				updateTransportTime(iPlayHead);
				if (m_pTracks && m_pSession->isRecording()) {
					m_pTracks->trackView()->updateContentsRecord();
					m_statusItems[StatusTime]->setText(
						m_pSession->timeFromFrame(m_pSession->sessionLength()));
				}
			}
		}
	}

	// Check if its time to refresh some tracks...
	if (m_iPeakTimer > 0) {
		m_iPeakTimer -= QTRACTOR_TIMER_MSECS;
		if (m_iPeakTimer < QTRACTOR_TIMER_MSECS) {
			m_iPeakTimer = 0;
			if (m_pTracks && m_pTracks->trackView())
				m_pTracks->trackView()->updateContents();
		}
	}

	// Check if we've got some XRUN callbacks...
	if (m_iXrunTimer > 0) {
		m_iXrunTimer -= QTRACTOR_TIMER_MSECS;
		if (m_iXrunTimer < QTRACTOR_TIMER_MSECS) {
			m_iXrunTimer = 0;
			// Did we skip any?
			if (m_iXrunSkip > 0) {
				appendMessagesColor(
					tr("XRUN(%1 skipped)").arg(m_iXrunSkip), "#cc99cc");
				m_iXrunSkip = 0;
			}
			// Just post an informative message...
			appendMessagesColor(
				tr("XRUN(%1): some frames might have been lost.")
				.arg(m_iXrunCount), "#cc0033");
		}
	}

	// Check if its time to refresh Audio connections...
	if (m_iAudioRefreshTimer > 0) {
		m_iAudioRefreshTimer -= QTRACTOR_TIMER_MSECS;
		if (m_iAudioRefreshTimer < QTRACTOR_TIMER_MSECS) {
			m_iAudioRefreshTimer = 0;
			if (m_pSession->audioEngine()->updateConnects() == 0) {
				appendMessagesColor(
					tr("Audio connections change."), "#cc9966");
				if (m_pConnections)
					m_pConnections->connectForm()->audioRefresh();
			}
		}
	}
	// MIDI connections should be checked too...
	if (m_iMidiRefreshTimer > 0) {
		m_iMidiRefreshTimer -= QTRACTOR_TIMER_MSECS;
		if (m_iMidiRefreshTimer < QTRACTOR_TIMER_MSECS) {
			m_iMidiRefreshTimer = 0;
			if (m_pSession->midiEngine()->updateConnects() == 0) {
				appendMessagesColor(
					tr("MIDI connections change."), "#66cc99");
				if (m_pConnections)
					m_pConnections->connectForm()->midiRefresh();
			}
		}
	}

	// Always update mixer monitoring...
	if (m_pMixer)
		m_pMixer->refresh();

	// Register the next timer slot.
	QTimer::singleShot(QTRACTOR_TIMER_MSECS, this, SLOT(timerSlot()));
}


//-------------------------------------------------------------------------
// qtractorMainForm -- MIDI engine notifications.

// ALSA sequencer notification slot.
void qtractorMainForm::alsaNotify (void)
{
	// This specialty needs acknowledgement...
	m_pSession->midiEngine()->alsaNotifyAck();

	// A MIDI graph change has just been occurred;
	// try to postpone the event effect a little more...
	if (m_iMidiRefreshTimer  < QTRACTOR_TIMER_DELAY)
		m_iMidiRefreshTimer += QTRACTOR_TIMER_DELAY;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Tracks stuff.

// Tracks view close slot funtion.
void qtractorMainForm::tracksClosed (void)
{
	// Log this simple event.
	appendMessages(tr("Tracks closed."));

	// Just reset the tracks handler, before something else does.
	m_pTracks = NULL;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- General contents change stuff.

// Audio file addition slot funtion.
void qtractorMainForm::addAudioFile  ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::addAudioFile(\"" + sFilename + "\")");
#endif

	// Add the just dropped audio file...
	if (m_pFiles)
		m_pFiles->addAudioFile(sFilename);

	stabilizeForm();
}


// Audio file activation slot funtion.
void qtractorMainForm::activateAudioFile  ( const QString& /* sFilename */ )
{
	//
	// TODO: Activate the just selected audio file...
	//

	stabilizeForm();
}


// MIDI file addition slot funtion.
void qtractorMainForm::addMidiFile  ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::addMidiFile(\"" + sFilename + "\")");
#endif

	// Add the just dropped MIDI file...
	if (m_pFiles)
		m_pFiles->addMidiFile(sFilename);

	stabilizeForm();
}


// MIDI file activation slot funtion.
void qtractorMainForm::activateMidiFile  ( const QString& /* sFilename */ )
{
	//
	// TODO: Activate the just selected MIDI file...
	//

	stabilizeForm();
}


// Tracks view selection change slot.
void qtractorMainForm::trackSelectionChanged (void)
{
#ifdef CONFIG_DEBUG_0
	appendMessages("qtractorMainForm::trackSelectionChanged()");
#endif

	// Select sync to mixer...
	if (m_pTracks && m_pMixer) {
		qtractorMixerStrip *pStrip = NULL;
		qtractorTrack *pTrack = m_pTracks->currentTrack();
		if (pTrack)
			pStrip = m_pMixer->trackRack()->findStrip(pTrack->monitor());
		if (pStrip) {
			int wm = (pStrip->width() >> 1);
			m_pMixer->trackRack()->ensureVisible(
				pStrip->pos().x() + wm, 0, wm, 0);
			m_pMixer->trackRack()->setSelectedStrip(pStrip);
		}
	}

	stabilizeForm();
}


// Mixer view selection change slot.
void qtractorMainForm::mixerSelectionChanged (void)
{
#ifdef CONFIG_DEBUG_0
	appendMessages("qtractorMainForm::mixerSelectionChanged()");
#endif

	// Select sync to tracks...
	if (m_pTracks && m_pMixer) {
		qtractorMixerStrip *pStrip = m_pMixer->trackRack()->selectedStrip();
		if (pStrip && pStrip->track()) {
			int iTrack = m_pTracks->trackList()->trackRow(pStrip->track());
			if (iTrack >= 0)
				m_pTracks->trackList()->selectTrack(iTrack);
		}
	}

	stabilizeForm();
}


// Clip editors update helper.
void qtractorMainForm::changeNotifySlot (void)
{
	updateNotifySlot(true);
}


// Command update helper.
void qtractorMainForm::updateNotifySlot ( bool bRefresh )
{
	// Maybe, just maybe, we've made things larger...
	m_pSession->updateTimeScale();
	m_pSession->updateSessionLength();

	// Refresh track-view?
	if (m_pTracks)
		m_pTracks->updateContents(bRefresh);

	// Notify who's watching...
	contentsChanged();
}


// Tracks view contents change slot.
void qtractorMainForm::contentsChanged (void)
{
#ifdef CONFIG_DEBUG_0
	appendMessages("qtractorMainForm::contentsChanged()");
#endif

	// Stabilize session toolbar widgets...
	updateTransportTime(m_iPlayHead);

	m_pTempoSpinBox->setValue(m_pSession->tempo());
	m_pSnapPerBeatComboBox->setCurrentIndex(
		qtractorTimeScale::indexFromSnap(m_pSession->snapPerBeat()));

	m_pThumbView->update();

	m_iDirtyCount++;
	stabilizeForm();
}


// Tempo spin-box change slot.
void qtractorMainForm::tempoChanged (void)
{
	// Avoid bogus changes...
	float fTempo = m_pTempoSpinBox->value();
	if (::fabsf(m_pSession->tempo() - fTempo) < 0.01f)
		return;

#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::tempoChanged()");
#endif

	// Now, express the change as a undoable command...
	m_pCommands->exec(new qtractorSessionTempoCommand(m_pSession, fTempo));
	m_iTransportUpdate++;
}


// Snap-per-beat spin-box change slot.
void qtractorMainForm::snapPerBeatChanged ( int iSnap )
{
	// Avoid bogus changes...
	unsigned short iSnapPerBeat = qtractorTimeScale::snapFromIndex(iSnap);
	if (iSnapPerBeat == m_pSession->snapPerBeat())
		return;

#ifdef CONFIG_DEBUG
	appendMessages("qtractorMainForm::snapPerBeatChanged()");
#endif

	// No need to express this change as a undoable command...
	m_pSession->setSnapPerBeat(iSnapPerBeat);
}


// Real thing: the playhead has been changed manually!
void qtractorMainForm::transportTimeChanged ( unsigned long iPlayHead )
{
	if (m_iTransportUpdate > 0)
		return;

	appendMessages("qtractorMainForm::transportTimeChanged(" + QString::number(iPlayHead) + ")");

	m_pSession->setPlayHead(iPlayHead);
	m_iTransportUpdate++;

	stabilizeForm();
}


// end of qtractorMainForm.cpp
