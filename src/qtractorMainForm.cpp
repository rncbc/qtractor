// qtractorMainForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorInstrumentMenu.h"
#include "qtractorMessages.h"
#include "qtractorFileSystem.h"
#include "qtractorFiles.h"
#include "qtractorConnections.h"
#include "qtractorMixer.h"

#include "qtractorTracks.h"
#include "qtractorTrackList.h"
#include "qtractorTrackView.h"
#include "qtractorThumbView.h"
#include "qtractorSpinBox.h"

#include "qtractorMonitor.h"

#include "qtractorAudioPeak.h"
#include "qtractorAudioBuffer.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorSessionCommand.h"
#include "qtractorTimeScaleCommand.h"
#include "qtractorClipCommand.h"

#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"

#include "qtractorMidiManager.h"

#include "qtractorActionControl.h"

#include "qtractorExportForm.h"
#include "qtractorSessionForm.h"
#include "qtractorOptionsForm.h"
#include "qtractorPaletteForm.h"
#include "qtractorConnectForm.h"
#include "qtractorShortcutForm.h"
#include "qtractorMidiControlForm.h"
#include "qtractorInstrumentForm.h"
#include "qtractorTimeScaleForm.h"
#include "qtractorBusForm.h"

#include "qtractorTakeRangeForm.h"

#include "qtractorMidiClip.h"
#include "qtractorMidiEditor.h"
#include "qtractorMidiEditorForm.h"

#include "qtractorTrackCommand.h"
#include "qtractorCurveCommand.h"

#include "qtractorMessageList.h"

#include "qtractorPluginFactory.h"

#ifdef CONFIG_DSSI
#include "qtractorDssiPlugin.h"
#endif

#ifdef CONFIG_VST2
#include "qtractorVst2Plugin.h"
#endif

#ifdef CONFIG_VST3
#include "qtractorVst3Plugin.h"
#endif

#ifdef CONFIG_LV2
#include "qtractorLv2Plugin.h"
#endif

#ifdef CONFIG_JACK_SESSION
#include <jack/session.h>
#endif

#ifdef CONFIG_NSM
#include "qtractorNsmClient.h"
#endif

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QUrl>
#include <QRegularExpression>

#include <QDomDocument>

#include <QSocketNotifier>
#include <QActionGroup>
#include <QStatusBar>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QClipboard>
#include <QProgressBar>

#include <QColorDialog>

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QCloseEvent>
#include <QDropEvent>

#include <QStyleFactory>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QMimeData>
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#if !defined(QT_NO_STYLE_GTK)
#include <QGtkStyle>
#endif
#endif

#include <algorithm>

#include <cmath>

// Timer constants (magic) stuff.
#define QTRACTOR_TIMER_MSECS    66
#define QTRACTOR_TIMER_DELAY    233

#if QT_VERSION < QT_VERSION_CHECK(4, 5, 0)
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define horizontalAdvance  width
#endif

#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#undef HAVE_SIGNAL_H
#endif


//-------------------------------------------------------------------------
// LADISH Level 1 support stuff.

#ifdef HAVE_SIGNAL_H

#include <QSocketNotifier>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

// File descriptor for SIGUSR1 notifier.
static int g_fdSigusr1[2] = { -1, -1 };

// Unix SIGUSR1 signal handler.
static void qtractor_sigusr1_handler ( int /* signo */ )
{
	char c = 1;

	(void) (::write(g_fdSigusr1[0], &c, sizeof(c)) > 0);
}

// File descriptor for SIGTERM notifier.
static int g_fdSigterm[2] = { -1, -1 };

// Unix SIGTERM signal handler.
static void qtractor_sigterm_handler ( int /* signo */ )
{
	char c = 1;

	(void) (::write(g_fdSigterm[0], &c, sizeof(c)) > 0);
}

#endif	// HAVE_SIGNAL_H


//-------------------------------------------------------------------------
// qtractorMainForm -- Main window form implementation.

// Kind of singleton reference.
qtractorMainForm *qtractorMainForm::g_pMainForm = nullptr;

// Constructor.
qtractorMainForm::qtractorMainForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QMainWindow(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);
#if QT_VERSION < QT_VERSION_CHECK(6, 1, 0)
	QMainWindow::setWindowIcon(QIcon(":/images/qtractor.png"));
#endif
	// Pseudo-singleton reference setup.
	g_pMainForm = this;

	// Initialize some pointer references.
	m_pOptions = nullptr;

	// FIXME: This gotta go, somewhere in time...
	m_pSession = new qtractorSession();
	m_pTempoCursor = new qtractorTempoCursor();
	m_pMessageList = new qtractorMessageList();
	m_pAudioFileFactory = new qtractorAudioFileFactory();
	m_pPluginFactory = new qtractorPluginFactory();

	// Custom track/instrument proxy menu.
	m_pInstrumentMenu = new qtractorInstrumentMenu(this);

	// All child forms are to be created later, not earlier than setup.
	m_pMessages    = nullptr;
	m_pFileSystem  = nullptr;
	m_pFiles       = nullptr;
	m_pMixer       = nullptr;
	m_pConnections = nullptr;
	m_pTracks      = nullptr;

	// To remember last time we've shown the playhead.
	m_iPlayHead = 0;

	// We'll start clean.
	m_iUntitled   = 0;
	m_iDirtyCount = 0;

	m_iBackupCount = 0;

	m_iTransportUpdate  = 0;
	m_iTransportRolling = 0;
	m_bTransportPlaying = false;
	m_fTransportShuttle = 0.0f;
	m_iTransportStep    = 0;

	m_iXrunCount = 0;
	m_iXrunSkip  = 0;
	m_iXrunTimer = 0;

	m_iAudioPeakTimer = 0;

	m_iAudioRefreshTimer = 0;
	m_iMidiRefreshTimer  = 0;

	m_iPlayerTimer = 0;

	m_pNsmClient = nullptr;
	m_bNsmDirty  = false;

	m_iAudioPropertyChange = 0;

	m_iStabilizeTimer = 0;

	// Configure the audio file peak factory...
	qtractorAudioPeakFactory *pAudioPeakFactory
		= m_pSession->audioPeakFactory();
	if (pAudioPeakFactory) {
		QObject::connect(pAudioPeakFactory,
			SIGNAL(peakEvent()),
			SLOT(audioPeakNotify()));
	}

	// Configure the audio engine event handling...
	const qtractorAudioEngineProxy *pAudioEngineProxy = nullptr;
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine)
		pAudioEngineProxy = pAudioEngine->proxy();
	if (pAudioEngineProxy) {
		QObject::connect(pAudioEngineProxy,
			SIGNAL(shutEvent()),
			SLOT(audioShutNotify()));
		QObject::connect(pAudioEngineProxy,
			SIGNAL(xrunEvent()),
			SLOT(audioXrunNotify()));
		QObject::connect(pAudioEngineProxy,
			SIGNAL(portEvent()),
			SLOT(audioPortNotify()));
		QObject::connect(pAudioEngineProxy,
			SIGNAL(buffEvent(unsigned int)),
			SLOT(audioBuffNotify(unsigned int)));
		QObject::connect(pAudioEngineProxy,
			SIGNAL(sessEvent(void *)),
			SLOT(audioSessNotify(void *)));
		QObject::connect(pAudioEngineProxy,
			SIGNAL(syncEvent(unsigned long, bool)),
			SLOT(audioSyncNotify(unsigned long, bool)));
		QObject::connect(pAudioEngineProxy,
			SIGNAL(propEvent()),
			SLOT(audioPropNotify()));
	}

	// Configure the MIDI engine event handling...
	const qtractorMidiEngineProxy *pMidiEngineProxy = nullptr;
	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine)
		pMidiEngineProxy = pMidiEngine->proxy();
	if (pMidiEngineProxy) {
		qRegisterMetaType<qtractorMmcEvent> ("qtractorMmcEvent");
		qRegisterMetaType<qtractorCtlEvent> ("qtractorCtlEvent");
		QObject::connect(pMidiEngineProxy,
			SIGNAL(mmcEvent(const qtractorMmcEvent&)),
			SLOT(midiMmcNotify(const qtractorMmcEvent&)));
		QObject::connect(pMidiEngineProxy,
			SIGNAL(ctlEvent(const qtractorCtlEvent&)),
			SLOT(midiCtlNotify(const qtractorCtlEvent&)));
		QObject::connect(pMidiEngineProxy,
			SIGNAL(sppEvent(int, unsigned short)),
			SLOT(midiSppNotify(int, unsigned short)));
		QObject::connect(pMidiEngineProxy,
			SIGNAL(clkEvent(float)),
			SLOT(midiClkNotify(float)));
		QObject::connect(pMidiEngineProxy,
			SIGNAL(inpEvent(unsigned short)),
			SLOT(midiInpNotify(unsigned short)));
	}

	// Add the midi controller map...
	m_pMidiControl = new qtractorMidiControl();

#ifdef HAVE_SIGNAL_H

	// Set to ignore any fatal "Broken pipe" signals.
	::signal(SIGPIPE, SIG_IGN);

	// LADISH Level 1 suport.
	// Initialize file descriptors for SIGUSR1 socket notifier.
	::socketpair(AF_UNIX, SOCK_STREAM, 0, g_fdSigusr1);
	m_pSigusr1Notifier
		= new QSocketNotifier(g_fdSigusr1[1], QSocketNotifier::Read, this);

	QObject::connect(m_pSigusr1Notifier,
		SIGNAL(activated(int)),
		SLOT(handle_sigusr1()));

	// Install SIGUSR1 signal handler.
	struct sigaction sigusr1;
	sigusr1.sa_handler = qtractor_sigusr1_handler;
	sigemptyset(&sigusr1.sa_mask);
	sigusr1.sa_flags = 0;
	sigusr1.sa_flags |= SA_RESTART;
	::sigaction(SIGUSR1, &sigusr1, nullptr);

	// LADISH termination suport.
	// Initialize file descriptors for SIGTERM socket notifier.
	::socketpair(AF_UNIX, SOCK_STREAM, 0, g_fdSigterm);
	m_pSigtermNotifier
		= new QSocketNotifier(g_fdSigterm[1], QSocketNotifier::Read, this);

	QObject::connect(m_pSigtermNotifier,
		SIGNAL(activated(int)),
		SLOT(handle_sigterm()));

	// Install SIGTERM/SIGQUIT signal handlers.
	struct sigaction sigterm;
	sigterm.sa_handler = qtractor_sigterm_handler;
	sigemptyset(&sigterm.sa_mask);
	sigterm.sa_flags = 0;
	sigterm.sa_flags |= SA_RESTART;
	::sigaction(SIGTERM, &sigterm, nullptr);
	::sigaction(SIGQUIT, &sigterm, nullptr);

	// Ignore SIGHUP/SIGINT signals.
	::signal(SIGHUP, SIG_IGN);
	::signal(SIGINT, SIG_IGN);

#else	// HAVE_SIGNAL_H

	m_pSigusr1Notifier = nullptr;
	m_pSigtermNotifier = nullptr;
	
#endif	// !HAVE_SIGNAL_H

	// Also the (QAction) MIDI observer map...
	m_pActionControl = new qtractorActionControl(this);

	// Get edit selection mode action group up...
	m_pSelectModeActionGroup = new QActionGroup(this);
	m_pSelectModeActionGroup->setExclusive(true);
	m_pSelectModeActionGroup->addAction(m_ui.editSelectModeClipAction);
	m_pSelectModeActionGroup->addAction(m_ui.editSelectModeRangeAction);
	m_pSelectModeActionGroup->addAction(m_ui.editSelectModeRectAction);
	m_pSelectModeActionGroup->addAction(m_ui.editSelectModeCurveAction);

	// And the corresponding tool-button drop-down menu...
	m_pSelectModeToolButton = new QToolButton(this);
	m_pSelectModeToolButton->setPopupMode(QToolButton::InstantPopup);
	m_pSelectModeToolButton->setMenu(m_ui.editSelectModeMenu);

	// Add/insert this on its proper place in the edit-toobar...
	m_ui.editToolbar->insertWidget(m_ui.clipNewAction,
		m_pSelectModeToolButton);
	m_ui.editToolbar->insertSeparator(m_ui.clipNewAction);

	QObject::connect(
		m_pSelectModeActionGroup, SIGNAL(triggered(QAction*)),
		m_pSelectModeToolButton, SLOT(setDefaultAction(QAction*)));

	// Get transport mode action group up...
	m_pTransportModeActionGroup = new QActionGroup(this);
	m_pTransportModeActionGroup->setExclusive(true);
	m_pTransportModeActionGroup->addAction(m_ui.transportModeNoneAction);
	m_pTransportModeActionGroup->addAction(m_ui.transportModeSlaveAction);
	m_pTransportModeActionGroup->addAction(m_ui.transportModeMasterAction);
	m_pTransportModeActionGroup->addAction(m_ui.transportModeFullAction);

	// And the corresponding tool-button drop-down menu...
	m_pTransportModeToolButton = new QToolButton(this);
	m_pTransportModeToolButton->setPopupMode(QToolButton::InstantPopup);
	m_pTransportModeToolButton->setMenu(m_ui.transportModeMenu);

	// Add/insert this on its proper place in the options-toobar...
	m_ui.optionsToolbar->insertWidget(m_ui.transportPanicAction,
		m_pTransportModeToolButton);
	m_ui.optionsToolbar->insertSeparator(m_ui.transportPanicAction);

	QObject::connect(
		m_pTransportModeActionGroup, SIGNAL(triggered(QAction*)),
		m_pTransportModeToolButton, SLOT(setDefaultAction(QAction*)));

	// Additional time-toolbar controls...
//	m_ui.timeToolbar->addSeparator();

	// View/Snap-to-beat actions initialization...
	int iSnap = 0;
	const QIcon& snapIcon = QIcon::fromTheme("itemBeat");
	const QString sSnapObjectName("viewSnapPerBeat%1");
	const QString sSnapStatusTip(tr("Set current snap to %1"));
	const QStringList& snapItems = qtractorTimeScale::snapItems();
	QStringListIterator snapIter(snapItems);
	while (snapIter.hasNext()) {
		const QString& sSnapText = snapIter.next();
		QAction *pAction = new QAction(sSnapText, this);
		pAction->setObjectName(sSnapObjectName.arg(iSnap));
		pAction->setStatusTip(sSnapStatusTip.arg(sSnapText));
		pAction->setCheckable(true);
	//	pAction->setIcon(snapIcon);
		pAction->setData(iSnap++);
		QObject::connect(pAction,
			SIGNAL(triggered(bool)),
			SLOT(viewSnap()));
		m_snapPerBeatActions.append(pAction);
		m_ui.viewSnapMenu->addAction(pAction);
	}
//	m_ui.viewSnapMenu->addSeparator();
	m_ui.viewSnapMenu->addAction(m_ui.viewSnapZebraAction);
	m_ui.viewSnapMenu->addAction(m_ui.viewSnapGridAction);

	// Automation/curve mode menu setup...
	m_ui.trackCurveModeMenu->addAction(m_ui.trackCurveLogarithmicAction);
	m_ui.trackCurveModeMenu->addAction(m_ui.trackCurveColorAction);

	QIcon iconProcess;
	iconProcess.addPixmap(
		QIcon::fromTheme("trackCurveProcess").pixmap(16, 16), QIcon::Normal, QIcon::On);
	iconProcess.addPixmap(
		QIcon::fromTheme("trackCurveEnabled").pixmap(16, 16), QIcon::Normal, QIcon::Off);
	iconProcess.addPixmap(
		QIcon::fromTheme("trackCurveNone").pixmap(16, 16), QIcon::Disabled, QIcon::Off);
	m_ui.trackCurveProcessAction->setIcon(iconProcess);

	QIcon iconCapture;
	iconCapture.addPixmap(
		QIcon::fromTheme("trackCurveCapture").pixmap(16, 16), QIcon::Normal, QIcon::On);
	iconCapture.addPixmap(
		QIcon::fromTheme("trackCurveEnabled").pixmap(16, 16), QIcon::Normal, QIcon::Off);
	iconCapture.addPixmap(
		QIcon::fromTheme("trackCurveNone").pixmap(16, 16), QIcon::Disabled, QIcon::Off);
	m_ui.trackCurveCaptureAction->setIcon(iconCapture);

	// Editable toolbar widgets special palette.
	QPalette pal;
	// Outrageous HACK: GTK+ ppl won't see green on black thing...
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#if !defined(QT_NO_STYLE_GTK)
	if (qobject_cast<QGtkStyle *> (style()) == nullptr) {
#endif
#endif
	//	pal.setColor(QPalette::Window, Qt::black);
		pal.setColor(QPalette::Base, Qt::black);
		pal.setColor(QPalette::Text, Qt::green);
	//	pal.setColor(QPalette::Button, Qt::darkGray);
	//	pal.setColor(QPalette::ButtonText, Qt::green);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#if !defined(QT_NO_STYLE_GTK)
	}
#endif
#endif

	const QSize  pad(4, 0);
	const QFont& font0 = qtractorMainForm::font();
	const QFont  font(font0.family(), font0.pointSize() + 2);
	const QFontMetrics fm(font);
	const int d = fm.height() + fm.leading() + 8;
	
	// Transport time.
	const QString sTime("+99:99:99.999");
	m_pTimeSpinBox = new qtractorTimeSpinBox(m_ui.timeToolbar);
	m_pTimeSpinBox->setTimeScale(m_pSession->timeScale());
	m_pTimeSpinBox->setFont(font);
	m_pTimeSpinBox->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	m_pTimeSpinBox->setMinimumSize(QSize(fm.horizontalAdvance(sTime) + d, d) + pad);
	m_pTimeSpinBox->setPalette(pal);
//	m_pTimeSpinBox->setAutoFillBackground(true);
	m_pTimeSpinBox->setToolTip(tr("Current time (play-head)"));
//	m_pTimeSpinBox->setContextMenuPolicy(Qt::CustomContextMenu);
	m_ui.timeToolbar->addWidget(m_pTimeSpinBox);
//	m_ui.timeToolbar->addSeparator();

	// Tempo spin-box.
	const QString sTempo("+999 9/9");
	m_pTempoSpinBox = new qtractorTempoSpinBox(m_ui.timeToolbar);
//	m_pTempoSpinBox->setFont(font);
	m_pTempoSpinBox->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	m_pTempoSpinBox->setMinimumSize(QSize(fm.horizontalAdvance(sTempo) + d, d) + pad);
	m_pTempoSpinBox->setPalette(pal);
//	m_pTempoSpinBox->setAutoFillBackground(true);
	m_pTempoSpinBox->setToolTip(tr("Current tempo (BPM)"));
	m_pTempoSpinBox->setContextMenuPolicy(Qt::CustomContextMenu);
	m_ui.timeToolbar->addWidget(m_pTempoSpinBox);
	m_ui.timeToolbar->addSeparator();

	// Snap-per-beat combo-box.
	m_pSnapPerBeatComboBox = new QComboBox(m_ui.timeToolbar);
	m_pSnapPerBeatComboBox->setEditable(false);
//	m_pSnapPerBeatComboBox->insertItems(0, snapItems);
	m_pSnapPerBeatComboBox->setIconSize(QSize(8, 16));
	snapIter.toFront();
	if (snapIter.hasNext())
		m_pSnapPerBeatComboBox->addItem(
			QIcon::fromTheme("itemNone"), snapIter.next());
	while (snapIter.hasNext())
		m_pSnapPerBeatComboBox->addItem(snapIcon, snapIter.next());
	m_pSnapPerBeatComboBox->setToolTip(tr("Snap/beat"));
	m_ui.timeToolbar->addWidget(m_pSnapPerBeatComboBox);

	// Track-line thumbnail view...
	m_pThumbView = new qtractorThumbView();
	m_ui.thumbViewToolbar->addWidget(m_pThumbView);
	m_ui.thumbViewToolbar->setAllowedAreas(
		Qt::TopToolBarArea | Qt::BottomToolBarArea);

	QObject::connect(m_pTimeSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(transportTimeFormatChanged(int)));
	QObject::connect(m_pTimeSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(transportTimeChanged(unsigned long)));
	QObject::connect(m_pTimeSpinBox,
		SIGNAL(editingFinished()),
		SLOT(transportTimeFinished()));
	QObject::connect(m_pTempoSpinBox,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(transportTempoContextMenu(const QPoint&)));
	QObject::connect(m_pTempoSpinBox,
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(transportTempoChanged(float, unsigned short, unsigned short)));
	QObject::connect(m_pTempoSpinBox,
		SIGNAL(editingFinished()),
		SLOT(transportTempoFinished()));
	QObject::connect(m_pSnapPerBeatComboBox,
		SIGNAL(activated(int)),
		SLOT(snapPerBeatChanged(int)));

	// Create some statusbar labels...
	QStatusBar *pStatusBar = statusBar();
	const QPalette& spal = pStatusBar->palette();

	QLabel *pLabel;
	QPalette *pPalette = new QPalette(spal);
	m_paletteItems[PaletteNone] = pPalette;

	pPalette = new QPalette(spal);
	pPalette->setColor(QPalette::WindowText, Qt::darkRed);
	pPalette->setColor(QPalette::Window, Qt::red);
	m_paletteItems[PaletteRed] = pPalette;

	pPalette = new QPalette(spal);
	pPalette->setColor(QPalette::WindowText, Qt::darkYellow);
	pPalette->setColor(QPalette::Window, Qt::yellow);
	m_paletteItems[PaletteYellow] = pPalette;

	pPalette = new QPalette(spal);
	pPalette->setColor(QPalette::WindowText, Qt::darkCyan);
	pPalette->setColor(QPalette::Window, Qt::cyan);
	m_paletteItems[PaletteCyan] = pPalette;

	pPalette = new QPalette(spal);
	pPalette->setColor(QPalette::WindowText, Qt::darkGreen);
	pPalette->setColor(QPalette::Window, Qt::green);
	m_paletteItems[PaletteGreen] = pPalette;

	// Track status.
	pLabel = new QLabel(tr("Track"));
	pLabel->setAlignment(Qt::AlignLeft);
	pLabel->setMinimumWidth(120);
	pLabel->setToolTip(tr("Current track name"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusName] = pLabel;
	pStatusBar->addWidget(pLabel, 2);

	// Hideous progress bar...
	m_pProgressBar = new QProgressBar();
	m_pProgressBar->setFixedHeight(pLabel->sizeHint().height());
	m_pProgressBar->setMinimumWidth(120);
	pStatusBar->addPermanentWidget(m_pProgressBar);
	m_pProgressBar->hide();

	// Session modification status.
	pLabel = new QLabel(tr("MOD"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session modification state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusMod] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// Session recording status.
	pLabel = new QLabel(tr("REC"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session record state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusRec] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// Session muting status.
	pLabel = new QLabel(tr("MUTE"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session muting state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusMute] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// Session soloing status.
	pLabel = new QLabel(tr("SOLO"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session soloing state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusSolo] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// Session looping status.
	pLabel = new QLabel(tr("LOOP"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session looping state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusLoop] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// XRUN count.
	pLabel = new QLabel("XRUN");
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setAutoFillBackground(true);
	pLabel->setToolTip(tr("Session XRUN state"));
	m_statusItems[StatusXrun] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// Session length time.
	pLabel = new QLabel(sTime);
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session total time"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusTime] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// Session buffer size.
	pLabel = new QLabel("1999");
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setAutoFillBackground(true);
	pLabel->setToolTip(tr("Session buffer size"));
	m_statusItems[StatusSize] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	// Session sample rate.
	pLabel = new QLabel("199999");
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setAutoFillBackground(true);
	pLabel->setToolTip(tr("Session sample rate"));
	m_statusItems[StatusRate] = pLabel;
	pStatusBar->addPermanentWidget(pLabel);

	m_ui.transportLoopAction->setAutoRepeat(false);
	m_ui.transportLoopSetAction->setAutoRepeat(false);
	m_ui.transportPlayAction->setAutoRepeat(false);
	m_ui.transportRecordAction->setAutoRepeat(false);
	m_ui.transportPunchAction->setAutoRepeat(false);
	m_ui.transportPunchSetAction->setAutoRepeat(false);

	// Some actions surely need those
	// shortcuts firmly attached...
	addAction(m_ui.viewMenubarAction);
#if 0
	const QList<QAction *>& actions = findChildren<QAction *> ();
	QListIterator<QAction *> iter(actions);
	while (iter.hasNext())
		iter.next()->setShortcutContext(Qt::ApplicationShortcut);
#else
	m_ui.viewFileSystemAction->setShortcutContext(Qt::ApplicationShortcut);
	m_ui.viewFilesAction->setShortcutContext(Qt::ApplicationShortcut);
	m_ui.viewConnectionsAction->setShortcutContext(Qt::ApplicationShortcut);
	m_ui.viewMixerAction->setShortcutContext(Qt::ApplicationShortcut);
	m_ui.viewMessagesAction->setShortcutContext(Qt::ApplicationShortcut);
#endif
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
	QObject::connect(m_ui.editPasteRepeatAction,
		SIGNAL(triggered(bool)),
		SLOT(editPasteRepeat()));
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
	QObject::connect(m_ui.editSelectModeCurveAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectModeCurve()));
	QObject::connect(m_ui.editSelectAllAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectAll()));
	QObject::connect(m_ui.editSelectNoneAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectNone()));
	QObject::connect(m_ui.editSelectInvertAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectInvert()));
	QObject::connect(m_ui.editSelectTrackAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectTrack()));
	QObject::connect(m_ui.editSelectTrackRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectTrackRange()));
	QObject::connect(m_ui.editSelectRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectRange()));
	QObject::connect(m_ui.editInsertRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editInsertRange()));
	QObject::connect(m_ui.editInsertTrackRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editInsertTrackRange()));
	QObject::connect(m_ui.editRemoveRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editRemoveRange()));
	QObject::connect(m_ui.editRemoveTrackRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editRemoveTrackRange()));
	QObject::connect(m_ui.editSplitAction,
		SIGNAL(triggered(bool)),
		SLOT(editSplit()));

	QObject::connect(m_ui.trackAddAction,
		SIGNAL(triggered(bool)),
		SLOT(trackAdd()));
	QObject::connect(m_ui.trackRemoveAction,
		SIGNAL(triggered(bool)),
		SLOT(trackRemove()));
	QObject::connect(m_ui.trackDuplicateAction,
		SIGNAL(triggered(bool)),
		SLOT(trackDuplicate()));
	QObject::connect(m_ui.trackPropertiesAction,
		SIGNAL(triggered(bool)),
		SLOT(trackProperties()));
	QObject::connect(m_ui.trackInputsAction,
		SIGNAL(triggered(bool)),
		SLOT(trackInputs()));
	QObject::connect(m_ui.trackOutputsAction,
		SIGNAL(triggered(bool)),
		SLOT(trackOutputs()));
	QObject::connect(m_ui.trackStateRecordAction,
		SIGNAL(triggered(bool)),
		SLOT(trackStateRecord(bool)));
	QObject::connect(m_ui.trackStateMuteAction,
		SIGNAL(triggered(bool)),
		SLOT(trackStateMute(bool)));
	QObject::connect(m_ui.trackStateSoloAction,
		SIGNAL(triggered(bool)),
		SLOT(trackStateSolo(bool)));
	QObject::connect(m_ui.trackStateMonitorAction,
		SIGNAL(triggered(bool)),
		SLOT(trackStateMonitor(bool)));
	QObject::connect(m_ui.trackNavigateFirstAction,
		SIGNAL(triggered(bool)),
		SLOT(trackNavigateFirst()));
	QObject::connect(m_ui.trackNavigatePrevAction,
		SIGNAL(triggered(bool)),
		SLOT(trackNavigatePrev()));
	QObject::connect(m_ui.trackNavigateNextAction,
		SIGNAL(triggered(bool)),
		SLOT(trackNavigateNext()));
	QObject::connect(m_ui.trackNavigateLastAction,
		SIGNAL(triggered(bool)),
		SLOT(trackNavigateLast()));
	QObject::connect(m_ui.trackNavigateNoneAction,
		SIGNAL(triggered(bool)),
		SLOT(trackNavigateNone()));
	QObject::connect(m_ui.trackMoveTopAction,
		SIGNAL(triggered(bool)),
		SLOT(trackMoveTop()));
	QObject::connect(m_ui.trackMoveUpAction,
		SIGNAL(triggered(bool)),
		SLOT(trackMoveUp()));
	QObject::connect(m_ui.trackMoveDownAction,
		SIGNAL(triggered(bool)),
		SLOT(trackMoveDown()));
	QObject::connect(m_ui.trackMoveBottomAction,
		SIGNAL(triggered(bool)),
		SLOT(trackMoveBottom()));
	QObject::connect(m_ui.trackHeightUpAction,
		SIGNAL(triggered(bool)),
		SLOT(trackHeightUp()));
	QObject::connect(m_ui.trackHeightDownAction,
		SIGNAL(triggered(bool)),
		SLOT(trackHeightDown()));
	QObject::connect(m_ui.trackHeightResetAction,
		SIGNAL(triggered(bool)),
		SLOT(trackHeightReset()));
	QObject::connect(m_ui.trackAutoMonitorAction,
		SIGNAL(triggered(bool)),
		SLOT(trackAutoMonitor(bool)));
	QObject::connect(m_ui.trackAutoDeactivateAction,
		SIGNAL(triggered(bool)),
		SLOT(trackAutoDeactivate(bool)));
	QObject::connect(m_ui.trackImportAudioAction,
		SIGNAL(triggered(bool)),
		SLOT(trackImportAudio()));
	QObject::connect(m_ui.trackImportMidiAction,
		SIGNAL(triggered(bool)),
		SLOT(trackImportMidi()));
	QObject::connect(m_ui.trackExportAudioAction,
		SIGNAL(triggered(bool)),
		SLOT(trackExportAudio()));
	QObject::connect(m_ui.trackExportMidiAction,
		SIGNAL(triggered(bool)),
		SLOT(trackExportMidi()));
	QObject::connect(m_ui.trackCurveSelectMenu,
		SIGNAL(triggered(QAction *)),
		SLOT(trackCurveSelect(QAction *)));
	QObject::connect(m_ui.trackCurveModeMenu,
		SIGNAL(triggered(QAction *)),
		SLOT(trackCurveMode(QAction *)));
	QObject::connect(m_ui.trackCurveLogarithmicAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveLogarithmic(bool)));
	QObject::connect(m_ui.trackCurveColorAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveColor()));
	QObject::connect(m_ui.trackCurveLockedAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveLocked(bool)));
	QObject::connect(m_ui.trackCurveProcessAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveProcess(bool)));
	QObject::connect(m_ui.trackCurveCaptureAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveCapture(bool)));
	QObject::connect(m_ui.trackCurveClearAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveClear()));
	QObject::connect(m_ui.trackCurveLockedAllAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveLockedAll(bool)));
	QObject::connect(m_ui.trackCurveProcessAllAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveProcessAll(bool)));
	QObject::connect(m_ui.trackCurveCaptureAllAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveCaptureAll(bool)));
	QObject::connect(m_ui.trackCurveClearAllAction,
		SIGNAL(triggered(bool)),
		SLOT(trackCurveClearAll()));

	QObject::connect(m_ui.clipNewAction,
		SIGNAL(triggered(bool)),
		SLOT(clipNew()));
	QObject::connect(m_ui.clipEditAction,
		SIGNAL(triggered(bool)),
		SLOT(clipEdit()));
	QObject::connect(m_ui.clipMuteAction,
		SIGNAL(triggered(bool)),
		SLOT(clipMute()));
	QObject::connect(m_ui.clipUnlinkAction,
		SIGNAL(triggered(bool)),
		SLOT(clipUnlink()));
	QObject::connect(m_ui.clipRecordExAction,
		SIGNAL(triggered(bool)),
		SLOT(clipRecordEx(bool)));
	QObject::connect(m_ui.clipSplitAction,
		SIGNAL(triggered(bool)),
		SLOT(clipSplit()));
	QObject::connect(m_ui.clipMergeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipMerge()));
	QObject::connect(m_ui.clipNormalizeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipNormalize()));
	QObject::connect(m_ui.clipTempoAdjustAction,
		SIGNAL(triggered(bool)),
		SLOT(clipTempoAdjust()));
	QObject::connect(m_ui.clipCrossFadeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipCrossFade()));
	QObject::connect(m_ui.clipRangeSetAction,
		SIGNAL(triggered(bool)),
		SLOT(clipRangeSet()));
	QObject::connect(m_ui.clipLoopSetAction,
		SIGNAL(triggered(bool)),
		SLOT(clipLoopSet()));
	QObject::connect(m_ui.clipImportAction,
		SIGNAL(triggered(bool)),
		SLOT(clipImport()));
	QObject::connect(m_ui.clipExportAction,
		SIGNAL(triggered(bool)),
		SLOT(clipExport()));
	QObject::connect(m_ui.clipToolsQuantizeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsQuantize()));
	QObject::connect(m_ui.clipToolsTransposeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsTranspose()));
	QObject::connect(m_ui.clipToolsNormalizeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsNormalize()));
	QObject::connect(m_ui.clipToolsRandomizeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsRandomize()));
	QObject::connect(m_ui.clipToolsResizeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsResize()));
	QObject::connect(m_ui.clipToolsRescaleAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsRescale()));
	QObject::connect(m_ui.clipToolsTimeshiftAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsTimeshift()));
	QObject::connect(m_ui.clipToolsTemporampAction,
		SIGNAL(triggered(bool)),
		SLOT(clipToolsTemporamp()));
	QObject::connect(m_ui.clipTakeSelectMenu,
		SIGNAL(triggered(QAction *)),
		SLOT(clipTakeSelect(QAction *)));
	QObject::connect(m_ui.clipTakeFirstAction,
		SIGNAL(triggered(bool)),
		SLOT(clipTakeFirst()));
	QObject::connect(m_ui.clipTakePrevAction,
		SIGNAL(triggered(bool)),
		SLOT(clipTakePrev()));
	QObject::connect(m_ui.clipTakeNextAction,
		SIGNAL(triggered(bool)),
		SLOT(clipTakeNext()));
	QObject::connect(m_ui.clipTakeLastAction,
		SIGNAL(triggered(bool)),
		SLOT(clipTakeLast()));
	QObject::connect(m_ui.clipTakeResetAction,
		SIGNAL(triggered(bool)),
		SLOT(clipTakeReset()));
	QObject::connect(m_ui.clipTakeRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(clipTakeRange()));

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
	QObject::connect(m_ui.viewToolbarOptionsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarOptions(bool)));
	QObject::connect(m_ui.viewToolbarTransportAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTransport(bool)));
	QObject::connect(m_ui.viewToolbarTimeAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTime(bool)));
	QObject::connect(m_ui.viewToolbarThumbAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarThumb(bool)));
	QObject::connect(m_ui.viewFileSystemAction,
		SIGNAL(triggered(bool)),
		SLOT(viewFileSystem(bool)));
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
	QObject::connect(m_ui.viewZoomInAction,
		SIGNAL(triggered(bool)),
		SLOT(viewZoomIn()));
	QObject::connect(m_ui.viewZoomOutAction,
		SIGNAL(triggered(bool)),
		SLOT(viewZoomOut()));
	QObject::connect(m_ui.viewZoomResetAction,
		SIGNAL(triggered(bool)),
		SLOT(viewZoomReset()));
	QObject::connect(m_ui.viewZoomHorizontalAction,
		SIGNAL(triggered(bool)),
		SLOT(viewZoomHorizontal()));
	QObject::connect(m_ui.viewZoomVerticalAction,
		SIGNAL(triggered(bool)),
		SLOT(viewZoomVertical()));
	QObject::connect(m_ui.viewZoomAllAction,
		SIGNAL(triggered(bool)),
		SLOT(viewZoomAll()));
	QObject::connect(m_ui.viewSnapZebraAction,
		SIGNAL(triggered(bool)),
		SLOT(viewSnapZebra(bool)));
	QObject::connect(m_ui.viewSnapGridAction,
		SIGNAL(triggered(bool)),
		SLOT(viewSnapGrid(bool)));
	QObject::connect(m_ui.viewToolTipsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolTips(bool)));
	QObject::connect(m_ui.viewRefreshAction,
		SIGNAL(triggered(bool)),
		SLOT(viewRefresh()));
	QObject::connect(m_ui.viewInstrumentsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewInstruments()));
	QObject::connect(m_ui.viewControllersAction,
		SIGNAL(triggered(bool)),
		SLOT(viewControllers()));
	QObject::connect(m_ui.viewBusesAction,
		SIGNAL(triggered(bool)),
		SLOT(viewBuses()));
	QObject::connect(m_ui.viewTempoMapAction,
		SIGNAL(triggered(bool)),
		SLOT(viewTempoMap()));
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
	QObject::connect(m_ui.transportStepBackwardAction,
		SIGNAL(triggered(bool)),
		SLOT(transportStepBackward()));
	QObject::connect(m_ui.transportStepForwardAction,
		SIGNAL(triggered(bool)),
		SLOT(transportStepForward()));
	QObject::connect(m_ui.transportLoopAction,
		SIGNAL(triggered(bool)),
		SLOT(transportLoop()));
	QObject::connect(m_ui.transportLoopSetAction,
		SIGNAL(triggered(bool)),
		SLOT(transportLoopSet()));
	QObject::connect(m_ui.transportStopAction,
		SIGNAL(triggered(bool)),
		SLOT(transportStop()));
	QObject::connect(m_ui.transportPlayAction,
		SIGNAL(triggered(bool)),
		SLOT(transportPlay()));
	QObject::connect(m_ui.transportRecordAction,
		SIGNAL(triggered(bool)),
		SLOT(transportRecord()));
	QObject::connect(m_ui.transportPunchAction,
		SIGNAL(triggered(bool)),
		SLOT(transportPunch()));
	QObject::connect(m_ui.transportPunchSetAction,
		SIGNAL(triggered(bool)),
		SLOT(transportPunchSet()));
	QObject::connect(m_ui.transportCountInAction,
		SIGNAL(triggered(bool)),
		SLOT(transportCountIn()));
	QObject::connect(m_ui.transportMetroAction,
		SIGNAL(triggered(bool)),
		SLOT(transportMetro()));
	QObject::connect(m_ui.transportFollowAction,
		SIGNAL(triggered(bool)),
		SLOT(transportFollow()));
	QObject::connect(m_ui.transportAutoBackwardAction,
		SIGNAL(triggered(bool)),
		SLOT(transportAutoBackward()));
	QObject::connect(m_ui.transportContinueAction,
		SIGNAL(triggered(bool)),
		SLOT(transportContinue()));
	QObject::connect(m_ui.transportModeNoneAction,
		SIGNAL(triggered(bool)),
		SLOT(transportModeNone()));
	QObject::connect(m_ui.transportModeSlaveAction,
		SIGNAL(triggered(bool)),
		SLOT(transportModeSlave()));
	QObject::connect(m_ui.transportModeMasterAction,
		SIGNAL(triggered(bool)),
		SLOT(transportModeMaster()));
	QObject::connect(m_ui.transportModeFullAction,
		SIGNAL(triggered(bool)),
		SLOT(transportModeFull()));
	QObject::connect(m_ui.transportPanicAction,
		SIGNAL(triggered(bool)),
		SLOT(transportPanic()));

	QObject::connect(m_ui.helpShortcutsAction,
		SIGNAL(triggered(bool)),
		SLOT(helpShortcuts()));
	QObject::connect(m_ui.helpAboutAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAbout()));
	QObject::connect(m_ui.helpAboutQtAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAboutQt()));

	QObject::connect(m_ui.fileMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateRecentFilesMenu()));
	QObject::connect(m_ui.trackMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateTrackMenu()));
	QObject::connect(m_ui.clipMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateClipMenu()));
	QObject::connect(m_ui.clipTakeMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateTakeMenu()));
	QObject::connect(m_ui.clipTakeSelectMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateTakeSelectMenu()));
	QObject::connect(m_ui.trackExportMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateExportMenu()));
	QObject::connect(m_ui.trackCurveMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateCurveMenu()));
	QObject::connect(m_ui.trackCurveModeMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateCurveModeMenu()));
	QObject::connect(m_ui.trackInstrumentMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateTrackInstrumentMenu()));
	QObject::connect(m_ui.viewZoomMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateZoomMenu()));
	QObject::connect(m_ui.viewSnapMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateSnapMenu()));

	QObject::connect(m_pSession->commands(),
		SIGNAL(updateNotifySignal(unsigned int)),
		SLOT(updateNotifySlot(unsigned int)));

//	Already handled in files widget...
//	QObject::connect(QApplication::clipboard(),
//		SIGNAL(dataChanged()),
//		SLOT(stabilizeForm()));
}


// Destructor.
qtractorMainForm::~qtractorMainForm (void)
{
#ifdef HAVE_SIGNAL_H
	if (m_pSigtermNotifier)
		delete m_pSigtermNotifier;
	if (m_pSigusr1Notifier)
		delete m_pSigusr1Notifier;
#endif

	// View/Snap-to-beat actions termination...
	qDeleteAll(m_snapPerBeatActions);
	m_snapPerBeatActions.clear();

	// Drop any widgets around (not really necessary)...
	if (m_pMixer)
		delete m_pMixer;
	if (m_pConnections)
		delete m_pConnections;
	if (m_pFiles)
		delete m_pFiles;
	if (m_pFileSystem)
		delete m_pFileSystem;
	if (m_pMessages)
		delete m_pMessages;
	if (m_pTracks)
		delete m_pTracks;

	// Get select mode action group down.
	if (m_pSelectModeActionGroup)
		delete m_pSelectModeActionGroup;
	if (m_pSelectModeToolButton)
		delete m_pSelectModeToolButton;

	// Get transport mode action group down.
	if (m_pTransportModeActionGroup)
		delete m_pTransportModeActionGroup;
	if (m_pTransportModeToolButton)
		delete m_pTransportModeToolButton;

	// Reclaim status items palettes...
	for (int i = 0; i < PaletteItems; ++i)
		delete m_paletteItems[i];

	// Destroy instrument menu proxy.
	if (m_pInstrumentMenu)
		delete m_pInstrumentMenu;

	// Custom tempo cursor.
	if (m_pTempoCursor)
		delete m_pTempoCursor;

	// Remove midi controllers.
	if (m_pMidiControl)
		delete m_pMidiControl;

	// Remove (QAction) MIDI observer map.
	if (m_pActionControl)
		delete m_pActionControl;

	// Remove plugin path/files registry.
	if (m_pPluginFactory)
		delete m_pPluginFactory;

	// Remove audio file formats registry.
	if (m_pAudioFileFactory)
		delete m_pAudioFileFactory;

	// Remove message list buffer.
	if (m_pMessageList)
		delete m_pMessageList;

	// And finally the session object.
	if (m_pSession)
		delete m_pSession;

	// Pseudo-singleton reference shut-down.
	g_pMainForm = nullptr;
}


// kind of singleton reference.
qtractorMainForm *qtractorMainForm::getInstance (void)
{
	return g_pMainForm;
}


// Make and set a proper setup options step.
void qtractorMainForm::setup ( qtractorOptions *pOptions )
{
	// We got options?
	m_pOptions = pOptions;

	// Some child/dockable forms are to be created right now.
	m_pFileSystem = new qtractorFileSystem(this);
	m_pFiles = new qtractorFiles(this);
	m_pFiles->audioListView()->setRecentDir(m_pOptions->sAudioDir);
	m_pFiles->midiListView()->setRecentDir(m_pOptions->sMidiDir);

	// Setup messages logging appropriately...
	m_pMessages = new qtractorMessages(this);
	m_pMessages->setLogging(
		m_pOptions->bMessagesLog,
		m_pOptions->sMessagesLogPath);

	// What style do we create tool childs?
	QWidget *pParent = nullptr;
	Qt::WindowFlags wflags = Qt::Window;
	if (m_pOptions->bKeepToolsOnTop) {
		wflags |= Qt::Tool;
	//	wflags |= Qt::WindowStaysOnTopHint;
		pParent = this;
	}
	// Other child/tools forms are also created right away...
	m_pConnections = new qtractorConnections(pParent, wflags);
	m_pMixer = new qtractorMixer(pParent, wflags);

	// Make those primordially docked...
	addDockWidget(Qt::LeftDockWidgetArea, m_pFileSystem, Qt::Vertical);
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

	qtractorTrackView *pTrackView = m_pTracks->trackView();
	pTrackView->setSelectMode(selectMode);
	pTrackView->setDropSpan(m_pOptions->bTrackViewDropSpan);
	pTrackView->setSnapGrid(m_pOptions->bTrackViewSnapGrid);
	pTrackView->setToolTips(m_pOptions->bTrackViewToolTips);
	pTrackView->setCurveEdit(m_pOptions->bTrackViewCurveEdit);

	if (pTrackView->isCurveEdit())
		m_ui.editSelectModeCurveAction->setChecked(true);

	// Set initial select mode...
	m_pSelectModeToolButton->setDefaultAction(
		m_pSelectModeActionGroup->checkedAction());

	// Initial zoom mode...
	m_pTracks->setZoomMode(m_pOptions->iZoomMode);

	// Initial auto time-stretching, loop-recording modes...
	m_pSession->setAutoTimeStretch(m_pOptions->bAudioAutoTimeStretch);
	m_pSession->setLoopRecordingMode(m_pOptions->iLoopRecordingMode);

	// Initial auto plugin deactivation
	m_pSession->setAutoDeactivate(m_pOptions->bAutoDeactivate);

	// Initial decorations toggle state.
	m_ui.viewMenubarAction->setChecked(m_pOptions->bMenubar);
	m_ui.viewStatusbarAction->setChecked(m_pOptions->bStatusbar);
	m_ui.viewToolbarFileAction->setChecked(m_pOptions->bFileToolbar);
	m_ui.viewToolbarEditAction->setChecked(m_pOptions->bEditToolbar);
	m_ui.viewToolbarTrackAction->setChecked(m_pOptions->bTrackToolbar);
	m_ui.viewToolbarViewAction->setChecked(m_pOptions->bViewToolbar);
	m_ui.viewToolbarOptionsAction->setChecked(m_pOptions->bOptionsToolbar);
	m_ui.viewToolbarTransportAction->setChecked(m_pOptions->bTransportToolbar);
	m_ui.viewToolbarTimeAction->setChecked(m_pOptions->bTimeToolbar);
	m_ui.viewToolbarThumbAction->setChecked(m_pOptions->bThumbToolbar);
	m_ui.viewSnapZebraAction->setChecked(pOptions->bTrackViewSnapZebra);
	m_ui.viewSnapGridAction->setChecked(pOptions->bTrackViewSnapGrid);
	m_ui.viewToolTipsAction->setChecked(pOptions->bTrackViewToolTips);

	m_ui.transportCountInAction->setChecked(m_pOptions->bCountIn);
	m_ui.transportMetroAction->setChecked(m_pOptions->bMetronome);
	m_ui.transportFollowAction->setChecked(m_pOptions->bFollowPlayhead);
	m_ui.transportAutoBackwardAction->setChecked(m_pOptions->bAutoBackward);
	m_ui.transportContinueAction->setChecked(m_pOptions->bContinuePastEnd);

	m_ui.trackAutoMonitorAction->setChecked(m_pOptions->bAutoMonitor);
	m_ui.trackAutoDeactivateAction->setChecked(m_pOptions->bAutoDeactivate);

	// Initial decorations visibility state.
	viewMenubar(m_pOptions->bMenubar);
	viewStatusbar(m_pOptions->bStatusbar);
	viewToolbarFile(m_pOptions->bFileToolbar);
	viewToolbarEdit(m_pOptions->bEditToolbar);
	viewToolbarTrack(m_pOptions->bTrackToolbar);
	viewToolbarView(m_pOptions->bViewToolbar);
	viewToolbarOptions(m_pOptions->bOptionsToolbar);
	viewToolbarTransport(m_pOptions->bTransportToolbar);
	viewToolbarTime(m_pOptions->bTimeToolbar);
	viewToolbarThumb(m_pOptions->bThumbToolbar);

	// Restore whole dock windows state.
	const QByteArray aDockables
		= m_pOptions->settings().value("/Layout/DockWindows").toByteArray();
	if (aDockables.isEmpty()) {
		// Some windows are forced initially as is...
		insertToolBarBreak(m_ui.transportToolbar);
	} else {
		// Make it as the last time.
		restoreState(aDockables);
	}

	// Restore file-system dock-window state.
	if (m_pFileSystem) {
		const QByteArray aFileSystem
			= m_pOptions->settings().value("/FileSystem/State").toByteArray();
		if (aFileSystem.isEmpty()) {
			// Should be hidden first time...
			viewFileSystem(false);
		} else {
			// Make it as the last time visible.
			m_pFileSystem->restoreState(aFileSystem);
		}
	}

	// Try to restore old window positioning.
	m_pOptions->loadWidgetGeometry(this);
	m_pOptions->loadWidgetGeometry(m_pMixer);
	m_pOptions->loadWidgetGeometry(m_pConnections);

	// Set MIDI control non catch-up/hook global option...
	qtractorMidiControl::setSync(m_pOptions->bMidiControlSync);

	// Load MIDI controller configuration files...
	QStringListIterator it(m_pOptions->midiControlFiles);
	while (it.hasNext())
		m_pMidiControl->loadDocument(it.next());

	// Load instrument definition files...
	QStringListIterator iter(m_pOptions->instrumentFiles);
	while (iter.hasNext())
		(m_pSession->instruments())->load(iter.next());

	// Load custom meter colors, if any...
	int iColor;
	for (iColor = 0; iColor < m_pOptions->audioMeterColors.count(); ++iColor)
		qtractorAudioMeter::setColor(iColor,
			QColor(m_pOptions->audioMeterColors[iColor]));
	for (iColor = 0; iColor < m_pOptions->midiMeterColors.count(); ++iColor)
		qtractorMidiMeter::setColor(iColor,
			QColor(m_pOptions->midiMeterColors[iColor]));

	// Primary startup stabilization...
	updateRecentFilesMenu();
	updatePeakAutoRemove();
	updateDisplayFormat();
	updateTransportModePre();
	updateTransportModePost();
	updateTimebase();
	updateAudioPlayer();
	updateAudioMetronome();
	updateMidiControlModes();
	updateMidiQueueTimer();
	updateMidiDriftCorrect();
	updateMidiPlayer();
	updateMidiControl();
	updateMidiMetronome();
	updateSyncViewHold();

	// FIXME: This is what it should ever be,
	// make it right from this very moment...
	qtractorAudioFileFactory::setDefaultType(
		m_pOptions->sAudioCaptureExt,
		m_pOptions->iAudioCaptureType,
		m_pOptions->iAudioCaptureFormat,
		m_pOptions->iAudioCaptureQuality);
	qtractorMidiClip::setDefaultFormat(
		m_pOptions->iMidiCaptureFormat);
	// Set default MIDI (plugin) instrument audio output mode.
	qtractorMidiManager::setDefaultAudioOutputBus(
		m_pOptions->bAudioOutputBus);
	qtractorMidiManager::setDefaultAudioOutputAutoConnect(
		m_pOptions->bAudioOutputAutoConnect);
	// Set default audio-buffer quality...
	qtractorAudioBuffer::setDefaultResampleType(
		m_pOptions->iAudioResampleType);
	qtractorAudioBuffer::setDefaultWsolaTimeStretch(
		m_pOptions->bAudioWsolaTimeStretch);
	qtractorAudioBuffer::setDefaultWsolaQuickSeek(
		m_pOptions->bAudioWsolaQuickSeek);
	qtractorTrack::setTrackColorSaturation(
		m_pOptions->iTrackColorSaturation);

	// Set default custom spin-box edit mode (deferred)...
	qtractorSpinBox::setEditMode(qtractorSpinBox::DeferredMode);

	// Load (action) keyboard shortcuts...
	m_pOptions->loadActionShortcuts(this);
	m_pOptions->loadActionControls(this);

	// Initial thumb-view background (empty)
	m_pThumbView->updateContents();

	// Some special defaults...
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine)
		pAudioEngine->setMasterAutoConnect(m_pOptions->bAudioMasterAutoConnect);
	
	// Final widget slot connections....
	QObject::connect(m_pFileSystem->toggleViewAction(),
		SIGNAL(triggered(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_pFiles->toggleViewAction(),
		SIGNAL(triggered(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_pMessages->toggleViewAction(),
		SIGNAL(triggered(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_pConnections->connectForm(),
		SIGNAL(connectChanged()),
		SLOT(contentsChanged()));
	QObject::connect(m_pMixer->trackRack(),
		SIGNAL(selectionChanged()),
		SLOT(mixerSelectionChanged()));
	QObject::connect(m_pFiles->audioListView(),
		SIGNAL(selected(const QString&, int, bool)),
		SLOT(selectAudioFile(const QString&, int, bool)));
	QObject::connect(m_pFiles->audioListView(),
		SIGNAL(activated(const QString&, int)),
		SLOT(activateAudioFile(const QString&, int)));
	QObject::connect(m_pFiles->midiListView(),
		SIGNAL(selected(const QString&, int, bool)),
		SLOT(selectMidiFile(const QString&, int, bool)));
	QObject::connect(m_pFiles->midiListView(),
		SIGNAL(activated(const QString&, int)),
		SLOT(activateMidiFile(const QString&, int)));
	QObject::connect(m_pFiles->audioListView(),
		SIGNAL(contentsChanged()),
		SLOT(contentsChanged()));
	QObject::connect(m_pFiles->midiListView(),
		SIGNAL(contentsChanged()),
		SLOT(contentsChanged()));
	QObject::connect(m_pFileSystem,
		SIGNAL(activated(const QString&)),
		SLOT(activateFile(const QString&)));
	QObject::connect(m_pTracks->trackList(),
		SIGNAL(selectionChanged()),
		SLOT(trackSelectionChanged()));

	// Other dedicated signal/slot connections...
	QObject::connect(m_pTracks->trackView(),
		SIGNAL(contentsMoving(int,int)),
		m_pThumbView, SLOT(updateThumb()));

#ifdef CONFIG_NSM
	// Check whether to participate into a NSM session...
	const QString& sNsmUrl
		= QString::fromLatin1(::getenv("NSM_URL"));
	if (!sNsmUrl.isEmpty()) {
		m_pNsmClient = new qtractorNsmClient(sNsmUrl);
		QObject::connect(m_pNsmClient,
			SIGNAL(open()),
			SLOT(openNsmSession()));
		QObject::connect(m_pNsmClient,
			SIGNAL(save()),
			SLOT(saveNsmSession()));
		QObject::connect(m_pNsmClient,
			SIGNAL(show()),
			SLOT(showNsmSession()));
		QObject::connect(m_pNsmClient,
			SIGNAL(hide()),
			SLOT(hideNsmSession()));
		m_pNsmClient->announce(QTRACTOR_TITLE, ":switch:dirty:optional-gui:");
		m_sNsmExt = m_pOptions->sSessionExt;
		if (m_sNsmExt.isEmpty())
			m_sNsmExt = qtractorDocument::defaultExt();
		// Run-time special non-persistent options.
		//m_pOptions->bDontUseNativeDialogs = true;
		QMainWindow::hide();
		m_pConnections->hide();
		m_pMixer->hide();
	}
	else
#endif
	QMainWindow::show();

	// Make it ready :-)
	statusBar()->showMessage(tr("Ready"), 3000);

	// Is any session identification to get loaded?
	const bool bSessionId = !m_pOptions->sSessionId.isEmpty();
	if (bSessionId && pAudioEngine) {
		pAudioEngine->setSessionId(m_pOptions->sSessionId);
		m_pOptions->sSessionId.clear();
	}

	// Is any session pending to be loaded?
	if (!m_pOptions->sessionFiles.isEmpty()) {
		// We're supposedly clean...
		m_iDirtyCount = 0;
		// Just load the prabable startup session...
		const int iFlags = qtractorDocument::Default;
		if (loadSessionFileEx(m_pOptions->sessionFiles, iFlags, !bSessionId)) {
			m_pOptions->sessionFiles.clear();
			// Take appropriate action when session is loaded from
			// some foreign session manager (eg. JACK session)...
			const QString& sLadishAppName
				= QString::fromLatin1(::getenv("LADISH_APP_NAME"));
			const bool bLadishApp = !sLadishAppName.isEmpty();
			if (bSessionId || bLadishApp) {
				// JACK session manager will take care of audio connections...
				if (pAudioEngine)
					pAudioEngine->clearConnects();
				// LADISH session manager will take care of MIDI connections...
				if (bLadishApp)
					m_pSession->midiEngine()->clearConnects();
			}
		}
	} else {
		// Change to last known session dir...
		if (!m_pOptions->sSessionDir.isEmpty()) {
			QFileInfo info(m_pOptions->sSessionDir);
			while (!info.exists() && !info.isRoot())
				info.setFile(info.absolutePath());
			if (info.exists() && !info.isRoot())
				m_pOptions->sSessionDir = info.absoluteFilePath();
			if (!QDir::setCurrent(m_pOptions->sSessionDir)) {
				appendMessagesError(
					tr("Could not set default session directory:\n\n"
					"%1\n\nSorry.").arg(m_pOptions->sSessionDir));
				m_pOptions->sSessionDir.clear();
			}
		}
		// Open up with a new empty session...
		if (!autoSaveOpen())
			newSession();
	}

	autoSaveReset();

	// Register the first timer slots.
	QTimer::singleShot(QTRACTOR_TIMER_DELAY, this, SLOT(slowTimerSlot()));
	QTimer::singleShot(QTRACTOR_TIMER_DELAY, this, SLOT(fastTimerSlot()));
}


// LADISH Level 1 -- SIGUSR1 signal handler.
void qtractorMainForm::handle_sigusr1 (void)
{
#ifdef HAVE_SIGNAL_H

	char c;

	if (::read(g_fdSigusr1[1], &c, sizeof(c)) > 0)
		saveSession(false);

#endif
}


// LADISH termination -- SIGTERM signal handler.
void qtractorMainForm::handle_sigterm (void)
{
#ifdef HAVE_SIGNAL_H

	char c;

	if (::read(g_fdSigterm[1], &c, sizeof(c)) > 0) {
	#ifdef CONFIG_NSM
		if (m_pNsmClient && m_pNsmClient->is_active())
			m_iDirtyCount = 0;
	#endif
		if (queryClose()) {
		#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			QApplication::exit(0);
		#else
			QApplication::quit();
		#endif
		}
	}

#endif
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
			if (QMainWindow::isVisible()) {
				m_pOptions->bMenubar = m_ui.menuBar->isVisible();
				m_pOptions->bStatusbar = statusBar()->isVisible();
				m_pOptions->bFileToolbar = m_ui.fileToolbar->isVisible();
				m_pOptions->bEditToolbar = m_ui.editToolbar->isVisible();
				m_pOptions->bTrackToolbar = m_ui.trackToolbar->isVisible();
				m_pOptions->bViewToolbar = m_ui.viewToolbar->isVisible();
				m_pOptions->bOptionsToolbar = m_ui.optionsToolbar->isVisible();
				m_pOptions->bTransportToolbar = m_ui.transportToolbar->isVisible();
				m_pOptions->bTimeToolbar = m_ui.timeToolbar->isVisible();
				m_pOptions->bThumbToolbar = m_ui.thumbViewToolbar->isVisible();
			}
			m_pOptions->bTrackViewSnapZebra = m_ui.viewSnapZebraAction->isChecked();
			m_pOptions->bTrackViewSnapGrid = m_ui.viewSnapGridAction->isChecked();
			m_pOptions->bTrackViewToolTips = m_ui.viewToolTipsAction->isChecked();
			m_pOptions->bTrackViewCurveEdit = m_ui.editSelectModeCurveAction->isChecked();
			m_pOptions->bCountIn = m_ui.transportCountInAction->isChecked();
			m_pOptions->bMetronome = m_ui.transportMetroAction->isChecked();
			m_pOptions->bFollowPlayhead = m_ui.transportFollowAction->isChecked();
			m_pOptions->bAutoBackward = m_ui.transportAutoBackwardAction->isChecked();
			m_pOptions->bContinuePastEnd = m_ui.transportContinueAction->isChecked();
			m_pOptions->bAutoMonitor = m_ui.trackAutoMonitorAction->isChecked();
			m_pOptions->bAutoDeactivate = m_ui.trackAutoDeactivateAction->isChecked();
			// Final zoom mode...
			if (m_pTracks)
				m_pOptions->iZoomMode = m_pTracks->zoomMode();
			// Save instrument definition file list...
			m_pOptions->instrumentFiles = (m_pSession->instruments())->files();
			// Save custom meter colors, if any...
			int iColor;
			m_pOptions->audioMeterColors.clear();
			for (iColor = 0; iColor < qtractorAudioMeter::ColorCount - 1; ++iColor)
				m_pOptions->audioMeterColors.append(
					qtractorAudioMeter::color(iColor).name());
			m_pOptions->midiMeterColors.clear();
			for (iColor = 0; iColor < qtractorMidiMeter::ColorCount - 1; ++iColor)
				m_pOptions->midiMeterColors.append(
					qtractorMidiMeter::color(iColor).name());
			// Make sure there will be defaults...
			m_pOptions->iSnapPerBeat = m_pSnapPerBeatComboBox->currentIndex();
			m_pOptions->fTempo = m_pTempoSpinBox->tempo();
			m_pOptions->iBeatsPerBar = m_pTempoSpinBox->beatsPerBar();
			m_pOptions->iBeatDivisor = m_pTempoSpinBox->beatDivisor();
			// Save MIDI control non catch-up/hook global option...
			m_pOptions->bMidiControlSync = qtractorMidiControl::isSync();
			// Save the file-system dock-window state...
			if (m_pFileSystem && m_pFileSystem->isVisible()) {
				m_pOptions->settings().setValue(
					"/FileSystem/State", m_pFileSystem->saveState());
			}
			// Save the dock windows state...
			m_pOptions->settings().setValue("/Layout/DockWindows", saveState());
			// Audio master bus auto-connection option...
			qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
			if (pAudioEngine)
				m_pOptions->bAudioMasterAutoConnect = pAudioEngine->isMasterAutoConnect();
			// And the main windows state.
			bool bSaveVisibility = true;
		#ifdef CONFIG_NSM
			if (m_pNsmClient && m_pNsmClient->is_active())
				bSaveVisibility = false;
		#endif
			m_pOptions->saveWidgetGeometry(m_pConnections, !bSaveVisibility);
			m_pOptions->saveWidgetGeometry(m_pMixer, !bSaveVisibility);
			m_pOptions->saveWidgetGeometry(this, bSaveVisibility);
		}
	}

	return bQueryClose;
}


void qtractorMainForm::closeEvent ( QCloseEvent *pCloseEvent )
{
#ifdef CONFIG_NSM
	// Just hide if under NSM auspice...
	if (m_pNsmClient && m_pNsmClient->is_active()) {
		pCloseEvent->ignore();
		QMainWindow::hide();
		m_pConnections->hide();
		m_pMixer->hide();
	}
	else
#endif
	// Let's be sure about that...
	if (queryClose()) {
		pCloseEvent->accept();
	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QApplication::exit(0);
	#else
		QApplication::quit();
	#endif
	} else {
		pCloseEvent->ignore();
	}
}


// Window drag-n-drop event handlers.
void qtractorMainForm::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
	// Accept external drags only...
	if (pDragEnterEvent->source() == nullptr
		&& pDragEnterEvent->mimeData()->hasUrls()) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


void qtractorMainForm::dropEvent ( QDropEvent *pDropEvent )
{
#if 0
	// Accept externally originated drops only...
	if (pDropEvent->source())
		return;
#endif
	const QMimeData *pMimeData = pDropEvent->mimeData();
	if (pMimeData->hasUrls()) {
		const QString sFilename = pMimeData->urls().first().toLocalFile();
		// Close current session and try to load the new one...
		if (!sFilename.isEmpty() && closeSession())
			loadSessionFile(sFilename);
	}
}


// Context menu event handler.
void qtractorMainForm::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	stabilizeForm();

	// Primordial edit menu should be available...
	m_ui.editMenu->exec(pContextMenuEvent->globalPos());
}


// Optional main widget visibility handler.
void qtractorMainForm::showEvent ( QShowEvent *pShowEvent )
{
	QMainWindow::showEvent(pShowEvent);

#ifdef CONFIG_NSM
	if (m_pNsmClient)
		m_pNsmClient->visible(true);
#endif
}


void qtractorMainForm::hideEvent ( QHideEvent *pHideEvent )
{
#ifdef CONFIG_NSM
	if (m_pNsmClient)
		m_pNsmClient->visible(false);
#endif

	QMainWindow::hideEvent(pHideEvent);
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Brainless public property accessors.

// The global session tracks reference.
qtractorTracks *qtractorMainForm::tracks (void) const
{
	return m_pTracks;
}

// The global file-system reference.
qtractorFileSystem *qtractorMainForm::fileSystem (void) const
{
	return m_pFileSystem;
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

// The session thumb-view widget accessor.
qtractorThumbView *qtractorMainForm::thumbView (void) const
{
	return m_pThumbView;
}

// The session transport fast-rolling direction accessor.
int qtractorMainForm::rolling (void) const
{
	return m_iTransportRolling;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Session file stuff.

// Format the displayable session filename.
QString qtractorMainForm::sessionName ( const QString& sFilename ) const
{
	const bool bCompletePath = (m_pOptions && m_pOptions->bCompletePath);
	QString sSessionName = sFilename;
	if (sSessionName.isEmpty() && m_pSession)
		sSessionName = m_pSession->sessionName();
	if (sSessionName.isEmpty())
		sSessionName = untitledName();
	else if (!bCompletePath)
		sSessionName = QFileInfo(sSessionName).baseName();
	return sSessionName;
}

// Retrieve current untitled sessoin name.
QString qtractorMainForm::untitledName (void) const
{
	return tr("Untitled%1").arg(m_iUntitled);
}


// Clear current filename; prompt for a new one when saving.
void qtractorMainForm::clearFilename (void)
{
	m_sFilename.clear();
}


// Create a new session file from scratch.
bool qtractorMainForm::newSession (void)
{
	// Check if we can do it.
	if (!closeSession())
		return false;

#ifdef CONFIG_LV2
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	qtractorLv2PluginType::lv2_open();
	QApplication::restoreOverrideCursor();
#endif

	// We're supposedly clean...
	m_iDirtyCount = 0;

#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active())
		return true;
#endif

	// Check whether we start the new session
	// based on existing template...
	if (m_pOptions && m_pOptions->bSessionTemplate) {
		const QStringList files(m_pOptions->sSessionTemplatePath);
		const int iFlags = qtractorDocument::Template;
		return loadSessionFileEx(files, iFlags, false);
	}

	// Prepare the session engines...
	updateSessionPre();

	// Ok, increment untitled count.
	++m_iUntitled;

	// Stabilize form.
	m_sFilename.clear();
//	m_iDirtyCount = 0;
	appendMessages(tr("New session: \"%1\".").arg(sessionName(m_sFilename)));

	// Give us what we got, right now...
	updateSessionPost();

	return true;
}


// Open an existing sampler session.
bool qtractorMainForm::openSession (void)
{
	if (m_pOptions == nullptr)
		return false;

	// Ask for the filename to open...
	QString sFilename;

	QString sExt("qtr");
	QStringList filters;
#ifdef CONFIG_LIBZ
	filters.append(tr("Session files (*.%1 *.%2 *.%3)")
		.arg(sExt).arg(qtractorDocument::defaultExt())
		.arg(qtractorDocument::archiveExt()));
#else
	filters.append(tr("Session files (*.%1 *.%2)")
		.arg(sExt).arg(qtractorDocument::defaultExt()));
#endif
	filters.append(tr("Template files (*.%1)")
		.arg(qtractorDocument::templateExt()));
#ifdef CONFIG_LIBZ
	filters.append(tr("Archive files (*.%1)")
		.arg(qtractorDocument::archiveExt()));
#endif
	filters.append(tr("All files (*.*)"));

	sExt = m_pOptions->sSessionExt; // Default session file format...

	const QString& sTitle
		= tr("Open Session");
	const QString& sFilter
		= filters.join(";;");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, m_pOptions->sSessionDir, sFilter, nullptr, options);
#else
	// Construct open-file session/template dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, m_pOptions->sSessionDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setHistory(m_pOptions->recentFiles);
	fileDialog.setDefaultSuffix(sExt);
#ifdef CONFIG_LIBZ
	// Special case for archive by default...
	if (sExt == qtractorDocument::archiveExt())
		fileDialog.setNameFilter(filters.last());
#endif
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (!fileDialog.exec())
		return false;
	// Have the open-file name...
	sFilename = fileDialog.selectedFiles().first();
#endif

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
	if (m_pOptions == nullptr)
		return false;

	// It must be a session name...
	if (m_pSession->sessionName().isEmpty() && !editSession())
		return false;

	// Suggest a filename, if there's none...
	QString sFilename = m_sFilename;

	if (sFilename.isEmpty()) {
		sFilename = QFileInfo(m_pSession->sessionDir(),
			qtractorSession::sanitize(m_pSession->sessionName())).absoluteFilePath();
		bPrompt = true;
	}

	// Ask for the file to save...
	if (bPrompt) {
		// Prompt the guy...
		QString sExt("qtr");
		QStringList filters;
	#ifdef CONFIG_LIBZ
		filters.append(tr("Session files (*.%1 *.%2 *.%3)")
			.arg(sExt).arg(qtractorDocument::defaultExt())
			.arg(qtractorDocument::archiveExt()));
	#else
		filters.append(tr("Session files (*.%1 *.%2)")
			.arg(sExt).arg(qtractorDocument::defaultExt()));
	#endif
		filters.append(tr("Template files (*.%1)")
			.arg(qtractorDocument::templateExt()));
	#ifdef CONFIG_LIBZ
		filters.append(tr("Archive files (*.%1)")
			.arg(qtractorDocument::archiveExt()));
	#endif
		filters.append(tr("All files (*.*)"));
		sExt = m_pOptions->sSessionExt; // Default session  file format...
		const QString& sTitle
			= tr("Save Session");
		const QString& sFilter
			= filters.join(";;");
		QWidget *pParentWidget = nullptr;
		QFileDialog::Options options;
		if (m_pOptions->bDontUseNativeDialogs) {
			options |= QFileDialog::DontUseNativeDialog;
			pParentWidget = QWidget::window();
		}
		// Always avoid to store session on extracted direactories...
		sFilename = sessionArchivePath(sFilename);
		// Try to rename as if a backup is about...
		sFilename = sessionBackupPath(sFilename);
	#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
		sFilename = QFileDialog::getSaveFileName(pParentWidget,
			sTitle, sFilename, sFilter, nullptr, options);
	#else
		// Construct save-file session/template dialog...
		QFileDialog fileDialog(pParentWidget,
			sTitle, sFilename, sFilter);
		// Set proper save-file modes...
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setHistory(m_pOptions->recentFiles);
		fileDialog.setDefaultSuffix(sExt);
	#ifdef CONFIG_LIBZ
		// Special case for archive by default...
		if (sExt == qtractorDocument::archiveExt())
			fileDialog.setNameFilter(filters.last());
	#endif
		// Stuff sidebar...
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
		fileDialog.setSidebarUrls(urls);
		fileDialog.setOptions(options);
		// Show save-file dialog...
		if (!fileDialog.exec())
			return false;
		// Have the save-file name...
		sFilename = fileDialog.selectedFiles().first();
		// Check whether we're on the template or archive filter...
		switch (filters.indexOf(fileDialog.selectedNameFilter())) {
		case 1:
			sExt = qtractorDocument::templateExt();
			break;
		case 2:
			sExt = qtractorDocument::archiveExt();
			break;
		}
	#endif
		// Have we cancelled it?
		if (sFilename.isEmpty() || sFilename.at(0) == '.')
			return false;
		// Enforce extension...
		if (QFileInfo(sFilename).suffix().isEmpty()) {
			sFilename += '.' + sExt;
			// Check if already exists...
			if (sFilename != m_sFilename && QFileInfo(sFilename).exists()) {
				if (QMessageBox::warning(this,
					tr("Warning"),
					tr("The file already exists:\n\n"
					"\"%1\"\n\n"
					"Do you want to replace it?")
					.arg(sFilename),
					QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
					return false;
			}
		}
		// Reset backup count, anyway...
		m_iBackupCount = 0;
	}
	else
	// Backup versioning?
	if (m_pOptions->bSessionBackup) {
		const QString& sBackupPath = sessionBackupPath(sFilename);
		if (m_pOptions->iSessionBackupMode > 0) {
			// Remove from recent files list...
			const int iIndex
				= m_pOptions->recentFiles.indexOf(sFilename);
			if (iIndex >= 0)
				m_pOptions->recentFiles.removeAt(iIndex);
			// Also remove from the file system...?
			if (m_iBackupCount > 0)
				QFile::remove(sFilename);
			// Make it a brand new one...
			sFilename = sBackupPath;
			++m_iBackupCount;
		} else {
			const QFileInfo fi(sBackupPath);
			if (QFile(sFilename).rename(sBackupPath)) {
				appendMessages(
					tr("Backup session: \"%1\" as \"%2\".")
					.arg(sFilename).arg(fi.fileName()));
			} else {
				appendMessagesError(
					tr("Could not backup existing session:\n\n"
					"%1 as %2\n\nSorry.").arg(sFilename).arg(fi.fileName()));
			}
		}
	}

	// Save it right away.
	return saveSessionFileEx(sFilename, qtractorDocument::Default, true);
}


// Edit session properties.
bool qtractorMainForm::editSession (void)
{
	// Session Properties...
	qtractorSessionForm sessionForm(this);
	const bool bSessionDir
	#ifdef CONFIG_NSM
		= (m_pNsmClient == nullptr || !m_pNsmClient->is_active());
	#else
		= true;
	#endif
	sessionForm.setSession(m_pSession, bSessionDir);
	if (!sessionForm.exec())
		return false;

	// If currently playing, we need to do a stop and go...
	const bool bPlaying = m_pSession->isPlaying();
	if (bPlaying)
		m_pSession->lock();

	// Now, express the change as a undoable command...
	m_pSession->execute(
		new qtractorSessionEditCommand(m_pSession, sessionForm.properties()));

	// Restore playback state, if needed...
	if (bPlaying)
		m_pSession->unlock();

	// Transport status needs an update too...
	++m_iTransportUpdate;

	// Done.
	return true;
}



// Close current session.
bool qtractorMainForm::closeSession (void)
{
	bool bClose = true;

	// Are we dirty enough to prompt it?
	if (bClose && m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning"),
			tr("The current session has been changed:\n\n"
			"\"%1\"\n\n"
			"Do you want to save the changes?")
			.arg(sessionName(m_sFilename)),
			QMessageBox::Save |
			QMessageBox::Discard |
			QMessageBox::Cancel)) {
		case QMessageBox::Save:
			bClose = saveSession(false);
			// Fall thru....
		case QMessageBox::Discard:
			break;
		default:    // Cancel.
			bClose = false;
			break;
		}
	}

	// If we may close it, do it!
	if (bClose) {
		// Just in case we were in the middle of something...
		setPlaying(false);
		// Reset (soft) subject/observer queue.
		qtractorSubject::resetQueue();
		// HACK: Track-list plugins-view must get wiped first...
		m_pTracks->trackList()->clear();
		// Reset all dependables to default.
		m_pMixer->clear();
		m_pFiles->clear();
		// Close session engines.
		m_pSession->close();
		m_pSession->clear();
		m_pTempoCursor->clear();
		// And last but not least.
		m_pConnections->clear();
		m_pTracks->clear();
		// Clear (hard) subject/observer queue.
		qtractorSubject::clearQueue();
		// Reset playhead.
		m_iPlayHead = 0;
	#ifdef CONFIG_VST3
		qtractorVst3Plugin::clearAll();
	#endif
	#ifdef CONFIG_CLAP
		qtractorClapPlugin::clearAll();
	#endif
	#ifdef CONFIG_LV2
		qtractorLv2PluginType::lv2_close();
	#endif
	#ifdef CONFIG_LIBZ
		// Is it time to cleanup extracted archives?
		const QStringList& paths = qtractorDocument::extractedArchives();
		if (!paths.isEmpty()) {
			bool bRemoveArchive = true;
			bool bConfirmArchive = (m_pOptions && m_pOptions->bConfirmArchive);
		#ifdef CONFIG_NSM
			if (m_pNsmClient && m_pNsmClient->is_active())
				bConfirmArchive = false;
		#endif
			if (bConfirmArchive) {
				const QString& sTitle = tr("Warning");
				const QString& sText = tr(
					"About to remove archive directory:\n\n"
					"\"%1\"\n\n"
					"Are you sure?")
					.arg(paths.join("\",\n\""));
			#if 0
				bArchiveRemove = (QMessageBox::warning(this, sTitle, sText,
					QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok);
			#else
				QMessageBox mbox(this);
				mbox.setIcon(QMessageBox::Warning);
				mbox.setWindowTitle(sTitle);
				mbox.setText(sText);
				mbox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
				QCheckBox cbox(tr("Don't ask this again"));
				cbox.setChecked(false);
				cbox.blockSignals(true);
				mbox.addButton(&cbox, QMessageBox::ActionRole);
				bRemoveArchive = (mbox.exec() == QMessageBox::Ok);
				if (cbox.isChecked())
					m_pOptions->bConfirmArchive = false;
			#endif
			}
			qtractorDocument::clearExtractedArchives(bRemoveArchive);
		}
	#endif
		// Some defaults are due...
		if (m_pOptions) {
			m_pSession->setSessionDir(m_pOptions->sSessionDir);
			m_pSession->setSnapPerBeat(
				qtractorTimeScale::snapFromIndex(m_pOptions->iSnapPerBeat));
			m_pSession->setTempo(m_pOptions->fTempo);
			m_pSession->setBeatsPerBar(m_pOptions->iBeatsPerBar);
			m_pSession->setBeatDivisor(m_pOptions->iBeatDivisor);
		}
		// We're now clean, for sure.
		m_iDirtyCount = 0;
		m_iBackupCount = 0;
		autoSaveClose();
		appendMessages(tr("Session closed."));
	}

	return bClose;
}


// Load a session from specific file path.
bool qtractorMainForm::loadSessionFile ( const QString& sFilename )
{
	bool bUpdate = true;

	// We're supposedly clean...
	m_iDirtyCount = 0;

#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active()) {
		m_pSession->setClientName(m_pNsmClient->client_id());
		bUpdate = false;
	}
#endif

	const bool bLoadSessionFile
		= loadSessionFileEx(
			QStringList(sFilename), qtractorDocument::Default, bUpdate);

#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active()) {
		m_pSession->setSessionName(m_pNsmClient->display_name());
		m_pSession->setSessionDir(m_pNsmClient->path_name());
		m_sNsmExt = QFileInfo(sFilename).suffix();
		updateDirtyCount(true);
		++m_iStabilizeTimer;
	}
#endif

	return bLoadSessionFile;
}


bool qtractorMainForm::loadSessionFileEx (
	const QStringList& files, int iFlags, bool bUpdate )
{
	if (files.isEmpty())
		return false;

	const QString& sFilename = files.first();
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::loadSessionFileEx(\"%s\", %d, %d)",
		sFilename.toUtf8().constData(), iFlags, int(bUpdate));
#endif

	// Flag whether we're about to load a template or archive...
	QFileInfo info(sFilename);
	const QString& sSuffix = info.suffix();
	if (sSuffix == qtractorDocument::templateExt())
		iFlags |= qtractorDocument::Template;

#ifdef CONFIG_LIBZ
	if (sSuffix == qtractorDocument::archiveExt()) {
		iFlags |= qtractorDocument::Archive;
		// Take special precaution for already
		// existing non-temporary archive directory...
		if (!bUpdate) {
			iFlags |= qtractorDocument::Temporary;
		} else {
			info.setFile(info.path() + QDir::separator() + info.completeBaseName());
			if (info.exists() && info.isDir()) {
				bool bRemoveArchive = true;
				if  (m_pOptions && m_pOptions->bConfirmArchive) {
					const QString& sTitle
						= tr("Warning");
					const QString& sText = tr(
						"The directory already exists:\n\n"
						"\"%1\"\n\n"
						"Do you want to replace it?")
						.arg(info.filePath());
				#if 0
					bRemoveArchive (QMessageBox::warning(this, sTitle, sText,
						QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok);
				#else
					QMessageBox mbox(this);
					mbox.setIcon(QMessageBox::Warning);
					mbox.setWindowTitle(sTitle);
					mbox.setText(sText);
					mbox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
					QCheckBox cbox(tr("Don't ask this again"));
					cbox.setChecked(false);
					cbox.blockSignals(true);
					mbox.addButton(&cbox, QMessageBox::ActionRole);
					bRemoveArchive = (mbox.exec() == QMessageBox::Ok);
					if (cbox.isChecked())
						m_pOptions->bConfirmArchive = false;
				#endif
					// Restarting?...
					if (!bRemoveArchive) {
					#ifdef CONFIG_LV2
						QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
						qtractorLv2PluginType::lv2_open();
						QApplication::restoreOverrideCursor();
					#endif
						updateSessionPre();
						++m_iUntitled;
						m_sFilename.clear();
						updateSessionPost();
						return false;
					}
				}
			}
		}
	}
#endif

	// Tell the world we'll take some time...
	appendMessages(tr("Opening \"%1\"...").arg(sFilename));

#ifdef CONFIG_LV2
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	qtractorLv2PluginType::lv2_open();
	QApplication::restoreOverrideCursor();
#endif

	// Warm-up the session engines...
	updateSessionPre();

	// We'll take some time anyhow...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Read the file...
	//
	// Check first whether it's a media file...
	bool bLoadSessionFileEx = false;
	if (iFlags == qtractorDocument::Default)
		bLoadSessionFileEx = m_pTracks->importTracks(files, 0);
	// Have we succeeded (or not at all)?
	if (bLoadSessionFileEx) {
		iFlags |= qtractorDocument::Template; // HACK!
	} else {
		// Load regular session file...
		QDomDocument doc("qtractorSession");
		bLoadSessionFileEx = qtractorSession::Document(&doc, m_pSession, m_pFiles)
			.load(sFilename, qtractorDocument::Flags(iFlags));
	}

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	if (bLoadSessionFileEx) {
		// Got something loaded...
		// we're not dirty anymore.
		if ((iFlags & qtractorDocument::Template) == 0 && bUpdate) {
			updateRecentFiles(sFilename);
		//	m_iDirtyCount = 0;
		}
		// Save as default session directory...
		if (m_pOptions && bUpdate) {
			// Do not set (next) default session directory on zip/archives...
			if ((iFlags & qtractorDocument::Archive) == 0)
				m_pOptions->sSessionDir = sessionDir(sFilename);
			// Save also some Audio engine hybrid-properties...
			qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
			if (pAudioEngine) {
				m_pOptions->iTransportMode = int(pAudioEngine->transportMode());
				m_pOptions->bTimebase = pAudioEngine->isTimebase();
				updateTransportModePre();
			}
			// Save also some MIDI engine hybrid-properties...
			qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
			if (pMidiEngine) {
				m_pOptions->iMidiMmcMode   = int(pMidiEngine->mmcMode());
				m_pOptions->iMidiMmcDevice = pMidiEngine->mmcDevice();
				m_pOptions->iMidiSppMode   = int(pMidiEngine->sppMode());
				m_pOptions->iMidiClockMode = int(pMidiEngine->clockMode());
				m_pOptions->bMidiResetAllControllers = pMidiEngine->isResetAllControllers();
			}
			// Save it good...
			m_pOptions->saveOptions();
		}	
	} else {
		// Something went wrong...
		appendMessagesError(
			tr("Session could not be loaded\n"
			"from \"%1\".\n\n"
			"Sorry.").arg(sFilename));
	}

	// Stabilize form title...
	if (iFlags & qtractorDocument::Template) {
		++m_iUntitled;
		m_sFilename.clear();
	} else if (bUpdate) {
		m_sFilename = sFilename;
	}

	appendMessages(tr("Open session: \"%1\".").arg(sessionName(sFilename)));

	// Now we'll try to create (update) the whole GUI session.
	updateSessionPost();

	// Do initial auto-deactivate as late as possible to give tracks/plugins
	// the chance to perform initial program-change events
	if (m_pSession->isAutoDeactivate()) {
		m_pSession->stabilize();
		m_pSession->autoDeactivatePlugins();
	}

	return bLoadSessionFileEx;
}


// Save current session to specific file path.
bool qtractorMainForm::saveSessionFileEx (
	const QString& sFilename, int iFlags, bool bUpdate )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::saveSessionFileEx(\"%s\", %d, %d)",
		sFilename.toUtf8().constData(), iFlags, int(bUpdate));
#endif

	// Flag whether we're about to save as template or archive...
	QFileInfo info(sFilename);
	const QString& sSuffix = info.suffix();
	if (sSuffix == qtractorDocument::templateExt())
		iFlags |= qtractorDocument::Template;
#ifdef CONFIG_LIBZ
	if (sSuffix == qtractorDocument::archiveExt()) {
		iFlags |= qtractorDocument::Archive;
		info.setFile(info.path() + QDir::separator() + info.completeBaseName());
		if (info.exists() && info.isDir() &&
			!qtractorDocument::extractedArchives().contains(info.filePath())) {
			bool bConfirmArchive = true;
			if  (m_pOptions && m_pOptions->bConfirmArchive) {
				const QString& sTitle
					= tr("Warning");
				const QString& sText = tr(
					"A directory with same name already exists:\n\n"
					"\"%1\"\n\n"
					"This directory will be replaced, "
					"erasing all its current data,\n"
					"when opening and extracting "
					"this archive in the future.\n\n"
					"Do you want to continue?")
					.arg(info.filePath());
			#if 0
				bConfirmArchive (QMessageBox::warning(this, sTitle, sText,
					QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok);
			#else
				QMessageBox mbox(this);
				mbox.setIcon(QMessageBox::Warning);
				mbox.setWindowTitle(sTitle);
				mbox.setText(sText);
				mbox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
				QCheckBox cbox(tr("Don't ask this again"));
				cbox.setChecked(false);
				cbox.blockSignals(true);
				mbox.addButton(&cbox, QMessageBox::ActionRole);
				bConfirmArchive = (mbox.exec() == QMessageBox::Ok);
				if (cbox.isChecked())
					m_pOptions->bConfirmArchive = false;
			#endif
			}
			// Aborting?...
			if (!bConfirmArchive)
				return false;
		}
	}
#endif

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	appendMessages(tr("Saving \"%1\"...").arg(sFilename));

	// Trap dirty clips (only MIDI at this time...)
	const bool bTemporary = (iFlags & qtractorDocument::Temporary);
	typedef QHash<qtractorMidiClip *, QString> MidiClipFilenames;
	MidiClipFilenames midiClips;
	for (qtractorTrack *pTrack = m_pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		// Only MIDI track/clips...
		if (pTrack->trackType() != qtractorTrack::Midi)
			continue;
		for (qtractorClip *pClip = pTrack->clips().first();
			pClip; pClip = pClip->next()) {
			// Are any dirty changes pending commit?
			if (pClip->isDirty()) {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (pClip);
				if (pMidiClip) {
					if (bTemporary)
						midiClips.insert(pMidiClip, pMidiClip->filename());
					// Have a new filename revision...
					const QString& sFilename
						= pMidiClip->createFilePathRevision(bTemporary);
					// Save/replace the clip track...
					pMidiClip->saveCopyFile(sFilename, !bTemporary);
				}
			}
		}
	}

	// Soft-house-keeping...
	m_pSession->files()->cleanup(false);

	// Write the file...
	QDomDocument doc("qtractorSession");
	bool bResult = qtractorSession::Document(&doc, m_pSession, m_pFiles)
		.save(sFilename, qtractorDocument::Flags(iFlags));

#ifdef CONFIG_LIBZ
	if ((iFlags & qtractorDocument::Archive) == 0 && bUpdate)
		qtractorDocument::clearExtractedArchives();
#endif

	// Restore old clip filenames, saved previously...
	MidiClipFilenames::ConstIterator iter = midiClips.constBegin();
	const MidiClipFilenames::ConstIterator& iter_end = midiClips.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiClip *pMidiClip = iter.key();
		const QString& sFilename = iter.value();
		pMidiClip->setFilenameEx(sFilename, false);
		m_pFiles->removeMidiFile(sFilename, false);
	}

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	if (bResult) {
		// Got something saved...
		// we're not dirty anymore.
		if ((iFlags & qtractorDocument::Template) == 0 && bUpdate) {
			updateRecentFiles(sFilename);
			autoSaveReset();
			m_iDirtyCount = 0;
		}
		// Save some default session properties...
		if (m_pOptions && bUpdate) {
			qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
			if (pAudioEngine) {
				m_pOptions->iTransportMode = int(pAudioEngine->transportMode());
				m_pOptions->bTimebase = pAudioEngine->isTimebase();
			}
			qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
			if (pMidiEngine) {
				m_pOptions->iMidiMmcMode   = int(pMidiEngine->mmcMode());
				m_pOptions->iMidiMmcDevice = int(pMidiEngine->mmcDevice());
				m_pOptions->iMidiSppMode   = int(pMidiEngine->sppMode());
				m_pOptions->iMidiClockMode = int(pMidiEngine->clockMode());
				m_pOptions->bMidiResetAllControllers = pMidiEngine->isResetAllControllers();
			}
			// Do not set (next) default session directory on zip/archives...
			if ((iFlags & qtractorDocument::Archive) == 0)
				m_pOptions->sSessionDir = sessionDir(sFilename);
			// Sync
			m_pOptions->saveOptions();
		}
	} else {
		// Something went wrong...
		appendMessagesError(
			tr("Session could not be saved\n"
			"to \"%1\".\n\n"
			"Sorry.").arg(sFilename));
	}

	// Stabilize form title...
	if ((iFlags & qtractorDocument::Template) == 0 && bUpdate)
		m_sFilename = sFilename;

	appendMessages(tr("Save session: \"%1\".").arg(sessionName(sFilename)));

	// Show static results...
	++m_iStabilizeTimer;

	return bResult;
}


QString qtractorMainForm::sessionBackupPath ( const QString& sFilename ) const
{
	QFileInfo fi(sFilename);

	QString sBackupName = fi.completeBaseName();
	if (fi.isDir())
		fi.setFile(QDir(fi.filePath()), sBackupName);
	if (fi.exists()) {
		int iBackupNo = 0;
		QRegularExpression rxBackupNo("\\.([0-9]+)$");
		QRegularExpressionMatch match = rxBackupNo.match(sBackupName);
		if (match.hasMatch()) {
			iBackupNo = match.captured(1).toInt();
			sBackupName.remove(rxBackupNo);
		}
		sBackupName += ".%1";
		const QString& sExt = fi.suffix();
		if (!sExt.isEmpty())
			sBackupName += '.' + sExt;
		const QDir dir = fi.absoluteDir();
		fi.setFile(dir, sBackupName.arg(++iBackupNo));
		while (fi.exists())
			fi.setFile(dir, sBackupName.arg(++iBackupNo));
	}

	return fi.absoluteFilePath();
}


// Whenever on some Save As... situation:
// better check whether the target directory
// is one of the extracted archives/zip ones...
//
QString qtractorMainForm::sessionArchivePath ( const QString& sFilename ) const
{
	QFileInfo fi(sFilename);
#ifdef CONFIG_LIBZ
	const QStringList& paths
		= qtractorDocument::extractedArchives();
	if (!paths.isEmpty()) {
		QStringListIterator iter(paths);
		while (iter.hasNext()) {
			const QString& sPath = iter.next();
			if (sPath == fi.absolutePath()) {
				const QString& sDir
					= QFileInfo(sPath).absolutePath();
				fi.setFile(sDir, fi.fileName());
			}
		}
	}
#endif
	return fi.absoluteFilePath();
}


//-------------------------------------------------------------------------
// qtractorMainForm -- NSM client slots.

void qtractorMainForm::openNsmSession (void)
{
#ifdef CONFIG_NSM

	if (m_pNsmClient == nullptr)
		return;

	if (!m_pNsmClient->is_active())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::openNsmSession()");
#endif

	openNsmSessionEx(true);

#endif	// CONFIG_NSM
}


void qtractorMainForm::openNsmSessionEx ( bool bOpenReply )
{
#ifdef CONFIG_NSM

	// We're supposedly clean...
	m_iDirtyCount = 0;
	m_bNsmDirty = false;
	m_sNsmFile.clear();

	bool bLoaded = false;

	if (closeSession()) {
		const QString& path_name = m_pNsmClient->path_name();
		const QString& display_name = m_pNsmClient->display_name();
		const QString& client_id = m_pNsmClient->client_id();
		const QDir dir(path_name);
		if (!dir.exists()) {
			dir.mkpath(path_name);
		} else {
			QStringList filters;
			const QString& prefix_dot = display_name + "*.";
			filters << prefix_dot + qtractorDocument::defaultExt();
			filters << prefix_dot + qtractorDocument::templateExt();
			filters << prefix_dot + qtractorDocument::archiveExt();
			filters << prefix_dot + "qtr";
			const QStringList& files
				= dir.entryList(filters,
					QDir::Files | QDir::NoSymLinks | QDir::Readable,
					QDir::Time);
			if (!files.isEmpty())
				m_sNsmExt = QFileInfo(files.first()).suffix();
		}
		m_pSession->setClientName(client_id);
		m_pSession->setSessionName(display_name);
		m_pSession->setSessionDir(path_name);
		if (bOpenReply)
			m_pNsmClient->open_reply(qtractorNsmClient::ERR_OK);
		QFileInfo fi(path_name, "session." + m_sNsmExt);
		if (!fi.exists())
			fi.setFile(path_name, display_name + '.' + m_sNsmExt);
		const QString& sFilename = fi.absoluteFilePath();
		if (fi.exists()) {
			const int iFlags = qtractorDocument::Default;
			bLoaded = loadSessionFileEx(QStringList(sFilename), iFlags, false);
			if (bLoaded) m_sNsmFile = sFilename;
		} else {
			updateSessionPre();
		#ifdef CONFIG_LV2
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			qtractorLv2PluginType::lv2_open();
			QApplication::restoreOverrideCursor();
		#endif
			appendMessages(tr("New session: \"%1\".")
				.arg(sessionName(sFilename)));
			updateSessionPost();
			bLoaded = true;
		}
	}

	if (bLoaded)
		m_pNsmClient->dirty(false);

	m_pNsmClient->visible(QMainWindow::isVisible());

#endif	// CONFIG_NSM
}


void qtractorMainForm::saveNsmSession (void)
{
#ifdef CONFIG_NSM

	if (m_pNsmClient == nullptr)
		return;

	if (!m_pNsmClient->is_active())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::saveNsmSession()");
#endif

	saveNsmSessionEx(true);

#endif	// CONFIG_NSM
}


void qtractorMainForm::saveNsmSessionEx ( bool bSaveReply )
{
#ifdef CONFIG_NSM

	bool bSaved = true;

	if (!m_sNsmFile.isEmpty()) {
		const QFileInfo fi(m_sNsmFile);
		if (fi.exists() && fi.suffix() != m_sNsmExt) {
			QFile(m_sNsmFile).remove();
			m_sNsmFile.clear();
		}
	}

	if (!bSaveReply || m_bNsmDirty) {
		m_iDirtyCount = 0;
		m_bNsmDirty = false;
		const QString& path_name = m_pNsmClient->path_name();
		const QString& display_name = m_pNsmClient->display_name();
	//	const QString& client_id = m_pNsmClient->client_id();
	//	m_pSession->setClientName(client_id);
		m_pSession->setSessionName(display_name);
		m_pSession->setSessionDir(path_name);
	//	const QFileInfo fi(path_name, display_name + '.' + m_sNsmExt);
		const QFileInfo fi(path_name, "session." + m_sNsmExt);
		const QString& sFilename = fi.absoluteFilePath();
		const int iFlags = qtractorDocument::SymLink;
		bSaved = saveSessionFileEx(sFilename, iFlags, false);
		if (bSaved) m_sNsmFile = sFilename;
	}

	if (bSaveReply) {
		m_pNsmClient->save_reply(bSaved
			? qtractorNsmClient::ERR_OK
			: qtractorNsmClient::ERR_GENERAL);
	}

	if (bSaved)
		m_pNsmClient->dirty(false);

#endif	// CONFIG_NSM
}


void qtractorMainForm::showNsmSession (void)
{
#ifdef CONFIG_NSM

	if (m_pNsmClient == nullptr)
		return;

	if (!m_pNsmClient->is_active())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::showNsmSession()");
#endif

	QMainWindow::show();
	QMainWindow::raise();
	QMainWindow::activateWindow();

#endif	// CONFIG_NSM
}


void qtractorMainForm::hideNsmSession (void)
{
#ifdef CONFIG_NSM

	if (m_pNsmClient == nullptr)
		return;

	if (!m_pNsmClient->is_active())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::hideNsmSession()");
#endif

	QMainWindow::hide();
	m_pConnections->hide();
	m_pMixer->hide();

#endif	// CONFIG_NSM
}


//-------------------------------------------------------------------------
// qtractorMainForm -- auot-save executive methods.

// Reset auto-save stats.
void qtractorMainForm::autoSaveReset (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::autoSaveReset()");
#endif

	m_iAutoSaveTimer = 0;

	if (m_pOptions->bAutoSaveEnabled)
		m_iAutoSavePeriod = 60000 * m_pOptions->iAutoSavePeriod;
	else
		m_iAutoSavePeriod = 0;
}


// Execute auto-save routine...
void qtractorMainForm::autoSaveSession (void)
{
#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active())
		return;
#endif

	QString sAutoSaveDir = m_pSession->sessionDir();
	if (sAutoSaveDir.isEmpty())
		sAutoSaveDir = m_pOptions->sSessionDir;
	if (sAutoSaveDir.isEmpty() || !QFileInfo(sAutoSaveDir).isWritable())
		sAutoSaveDir = QDir::tempPath();

	QString sAutoSaveName = m_pSession->sessionName();
	if (sAutoSaveName.isEmpty())
		sAutoSaveName = untitledName();

	const QString& sAutoSavePathname = QFileInfo(sAutoSaveDir,
		qtractorSession::sanitize(sAutoSaveName)).filePath()
		+ ".auto-save." + qtractorDocument::defaultExt();

	const QString& sOldAutoSavePathname
		= m_pOptions->sAutoSavePathname;
	if (!sOldAutoSavePathname.isEmpty()
		&& sOldAutoSavePathname != sAutoSavePathname
		&& QFileInfo(sOldAutoSavePathname).exists())
		QFile(sOldAutoSavePathname).remove();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::autoSaveSession(\"%s\")",
		sAutoSavePathname.toUtf8().constData());
#endif

	const int iFlags = qtractorDocument::Default | qtractorDocument::Temporary;
	if (saveSessionFileEx(sAutoSavePathname, iFlags, false)) {
		m_pOptions->sAutoSavePathname = sAutoSavePathname;
		m_pOptions->sAutoSaveFilename = m_sFilename;
		m_pOptions->saveOptions();
	}
}


// Auto-save/crash-recovery setup...
bool qtractorMainForm::autoSaveOpen (void)
{
#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active())
		return false;
#endif

	const QString& sAutoSavePathname = m_pOptions->sAutoSavePathname;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::autoSaveOpen(\"%s\")",
		sAutoSavePathname.toUtf8().constData());
#endif

	if (!sAutoSavePathname.isEmpty()
		&& QFileInfo(sAutoSavePathname).exists()) {
		if (QMessageBox::warning(this,
			tr("Warning"),
			tr("Oops!\n\n"
			"Looks like it crashed or did not close "
			"properly last time it was run... however, "
			"an auto-saved session file exists:\n\n"
			"\"%1\"\n\n"
			"Do you want to crash-recover from it?")
			.arg(sAutoSavePathname),
			QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
			const QStringList files(sAutoSavePathname);
			const int iFlags = qtractorDocument::Default;
			if (loadSessionFileEx(files, iFlags, false)) {
				m_sFilename = m_pOptions->sAutoSaveFilename;
				++m_iDirtyCount;
				return true;
			}
		}
	}

	return false;
}


// Auto-save/crash-recovery cleanup.
void qtractorMainForm::autoSaveClose (void)
{
#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active())
		return;
#endif

	const QString& sAutoSavePathname = m_pOptions->sAutoSavePathname;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::autoSaveClose(\"%s\")",
		sAutoSavePathname.toUtf8().constData());
#endif

	if (!sAutoSavePathname.isEmpty()
		&& QFileInfo(sAutoSavePathname).exists())
		QFile(sAutoSavePathname).remove();

	m_pOptions->sAutoSavePathname.clear();
	m_pOptions->sAutoSaveFilename.clear();

	autoSaveReset();
}


// Execute auto-save as soon as possible (quasi-immediately please).
void qtractorMainForm::autoSaveAsap (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::autoSaveAsap()");
#endif

	if (m_pOptions->bAutoSaveEnabled)
		m_iAutoSaveTimer = m_iAutoSavePeriod;
}


// Make it sane a session directory...
QString qtractorMainForm::sessionDir ( const QString& sFilename ) const
{
	QFileInfo fi(sFilename);
	const QDir& dir = fi.dir();
	if (fi.completeBaseName() == dir.dirName())
		fi.setFile(dir.absolutePath());
	return fi.absolutePath();
}


//-------------------------------------------------------------------------
// qtractorMainForm -- File Action slots.

// Create a new sampler session.
void qtractorMainForm::fileNew (void)
{
#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active()) {
	//	openNsmSessionEx(false);
		return;
	}
#endif
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
#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active()) {
		saveNsmSessionEx(false);
		return;
	}
#endif

	// Save it right away.
	saveSession(false);
}


// Save current sampler session with another name.
void qtractorMainForm::fileSaveAs (void)
{
#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active()) {
	//	saveNsmSessionEx(false);
		return;
	}
#endif

	// Save it right away, maybe with another name.
	saveSession(true);
}


// Edit session properties.
void qtractorMainForm::fileProperties (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::fileProperties()");
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
	qDebug("qtractorMainForm::editUndo()");
#endif

	(m_pSession->commands())->undo();
}


// Redo last action.
void qtractorMainForm::editRedo (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editRedo()");
#endif

	(m_pSession->commands())->redo();
}


// Cut selection to clipboard.
void qtractorMainForm::editCut (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editCut()");
#endif

	// Cut from files...
	if (m_pFiles && m_pFiles->hasFocus())
		m_pFiles->cutItemSlot();
	else
	// Cut selection...
	if (m_pTracks)
		m_pTracks->cutClipboard();
}




// Copy selection to clipboard.
void qtractorMainForm::editCopy (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editCopy()");
#endif

	// Copy from files...
	if (m_pFiles && m_pFiles->hasFocus())
		m_pFiles->copyItemSlot();
	else
	// Copy selection...
	if (m_pTracks)
		m_pTracks->copyClipboard();

	++m_iStabilizeTimer;
}


// Paste clipboard contents.
void qtractorMainForm::editPaste (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editPaste()");
#endif

	// Paste to files...
	if (m_pFiles && m_pFiles->hasFocus())
		m_pFiles->pasteItemSlot();
	else
	// Paste selection...
	if (m_pTracks)
		m_pTracks->pasteClipboard();
}


// Paste/repeat clipboard contents.
void qtractorMainForm::editPasteRepeat (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editPasteRepeat()");
#endif

	// Paste/repeat selection...
	if (m_pTracks)
		m_pTracks->pasteRepeatClipboard();
}


// Delete selection.
void qtractorMainForm::editDelete (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editDelete()");
#endif

	// Delete from files...
	if (m_pFiles && m_pFiles->hasFocus())
		m_pFiles->removeItemSlot();
	else
	// Delete selection...
	if (m_pTracks)
		m_pTracks->deleteSelect();
}


// Set selection to whole clip mode.
void qtractorMainForm::editSelectModeClip (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectModeClip()");
#endif

	// Select clip mode...
	if (m_pTracks) {
		qtractorTrackView *pTrackView = m_pTracks->trackView();
		pTrackView->setSelectMode(qtractorTrackView::SelectClip);
		pTrackView->setCurveEdit(false);
	}

	if (m_pOptions)
		m_pOptions->iTrackViewSelectMode = 0;

	++m_iStabilizeTimer;
}


// Set selection to range mode.
void qtractorMainForm::editSelectModeRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectModeRange()");
#endif

	// Select clip mode...
	if (m_pTracks) {
		qtractorTrackView *pTrackView = m_pTracks->trackView();
		pTrackView->setSelectMode(qtractorTrackView::SelectRange);
		pTrackView->setCurveEdit(false);
	}

	if (m_pOptions)
		m_pOptions->iTrackViewSelectMode = 1;

	++m_iStabilizeTimer;
}


// Set selection to rectangularmode.
void qtractorMainForm::editSelectModeRect (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectModeRect()");
#endif

	// Select clip mode...
	if (m_pTracks) {
		qtractorTrackView *pTrackView = m_pTracks->trackView();
		pTrackView->setSelectMode(qtractorTrackView::SelectRect);
		pTrackView->setCurveEdit(false);
	}

	if (m_pOptions)
		m_pOptions->iTrackViewSelectMode = 2;

	++m_iStabilizeTimer;
}


// Special automation curve node edit mode.
void qtractorMainForm::editSelectModeCurve (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectModeCurve()");
#endif

	if (m_pTracks)
		m_pTracks->trackView()->setCurveEdit(true);
}


// Mark all as selected.
void qtractorMainForm::editSelectAll (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectAll()");
#endif

	// Select all...
	if (m_pTracks)
		m_pTracks->selectAll();

	++m_iStabilizeTimer;
}


// Mark all as unselected.
void qtractorMainForm::editSelectNone (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectNone()");
#endif

	// Select nothing...
	if (m_pTracks)
		m_pTracks->selectNone();

	++m_iStabilizeTimer;
}


// Invert current selection.
void qtractorMainForm::editSelectInvert (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectInvert()");
#endif

	// Invert selection...
	if (m_pTracks)
		m_pTracks->selectInvert();

	++m_iStabilizeTimer;
}


// Mark track as selected.
void qtractorMainForm::editSelectTrack (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectTrack()");
#endif

	// Select current track...
	if (m_pTracks)
		m_pTracks->selectCurrentTrack();

	++m_iStabilizeTimer;
}


// Mark track-range as selected.
void qtractorMainForm::editSelectTrackRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectTrackRange()");
#endif

	// Select track-range...
	if (m_pTracks)
		m_pTracks->selectCurrentTrackRange();

	++m_iStabilizeTimer;
}


// Mark range as selected.
void qtractorMainForm::editSelectRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSelectRange()");
#endif

	// Select edit-range...
	if (m_pTracks)
		m_pTracks->selectEditRange();

	++m_iStabilizeTimer;
}


// Insert range as selected.
void qtractorMainForm::editInsertRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editInsertRange()");
#endif

	// Insert edit-range...
	if (m_pTracks)
		m_pTracks->insertEditRange();

	++m_iStabilizeTimer;
}


// Insert track-range as selected.
void qtractorMainForm::editInsertTrackRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editInsertTrackRange()");
#endif

	// Select track-range...
	if (m_pTracks)
		m_pTracks->insertEditRange(m_pTracks->currentTrack());

	++m_iStabilizeTimer;
}


// Remove range as selected.
void qtractorMainForm::editRemoveRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editRemoveRange()");
#endif

	// Insert edit-range...
	if (m_pTracks)
		m_pTracks->removeEditRange();

	++m_iStabilizeTimer;
}


// Remove track-range as selected.
void qtractorMainForm::editRemoveTrackRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editRemoveTrackRange()");
#endif

	// Select track-range...
	if (m_pTracks)
		m_pTracks->removeEditRange(m_pTracks->currentTrack());

	++m_iStabilizeTimer;
}


// Split (clip) selection...
void qtractorMainForm::editSplit (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::editSplit()");
#endif

	// Split selection...
	if (m_pTracks)
		m_pTracks->splitSelect();

	++m_iStabilizeTimer;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Track Action slots.

// Add a new track to session.
void qtractorMainForm::trackAdd (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackAdd()");
#endif

	// Add Track...
	if (m_pTracks)
		m_pTracks->addTrack();
}


// Remove current track from session.
void qtractorMainForm::trackRemove (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackRemove()");
#endif

	// Remove Track...
	if (m_pTracks)
		m_pTracks->removeTrack();
}


// Duplicate/copy track on session.
void qtractorMainForm::trackDuplicate (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackDuplicate()");
#endif

	// Track Properties...
	if (m_pTracks)
		m_pTracks->copyTrack();
}


// Edit track properties on session.
void qtractorMainForm::trackProperties (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackProperties()");
#endif

	// Track Properties...
	if (m_pTracks)
		m_pTracks->editTrack();
}


// Show current track input bus connections.
void qtractorMainForm::trackInputs (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;
	if (pTrack->inputBus() == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackInputs()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	if (m_pConnections)
		m_pConnections->showBus(pTrack->inputBus(), qtractorBus::Input);
}


// Show current track output bus connections.
void qtractorMainForm::trackOutputs (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;
	if (pTrack->outputBus() == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackOutputs()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	if (m_pConnections)
		m_pConnections->showBus(pTrack->outputBus(), qtractorBus::Output);
}


// Arm current track for recording.
void qtractorMainForm::trackStateRecord ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackStateRecord(%d)", int(bOn));
#endif

	m_pSession->execute(
		new qtractorTrackStateCommand(pTrack, qtractorTrack::Record, bOn));
}


// Mute current track.
void qtractorMainForm::trackStateMute ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackStateMute(%d)", int(bOn));
#endif

	m_pSession->execute(
		new qtractorTrackStateCommand(pTrack, qtractorTrack::Mute, bOn));
}


// Solo current track.
void qtractorMainForm::trackStateSolo (  bool bOn  )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackStateSolo(%d)", int(bOn));
#endif

	m_pSession->execute(
		new qtractorTrackStateCommand(pTrack, qtractorTrack::Solo, bOn));
}


// Monitor current track.
void qtractorMainForm::trackStateMonitor ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackStateMonitor(%d)", int(bOn));
#endif

	m_pSession->execute(
		new qtractorTrackMonitorCommand(pTrack, bOn));
}


// Make current the first track on list.
void qtractorMainForm::trackNavigateFirst (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackNavigateFirst()");
#endif

	if (m_pTracks)
		m_pTracks->trackList()->setCurrentTrackRow(0);
}


// Make current the previous track on list.
void qtractorMainForm::trackNavigatePrev (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackNavigatePrev()");
#endif

	if (m_pTracks) {
		qtractorTrackList *pTrackList = m_pTracks->trackList();
		const int iTrackCount = pTrackList->trackRowCount();
		int iTrack = pTrackList->currentTrackRow() - 1;
		if (iTrack < 0)
			iTrack = iTrackCount - 1;
		if (iTrack >= 0 && iTrackCount >= iTrack)
			pTrackList->setCurrentTrackRow(iTrack);
	}
}


// Make current the next track on list.
void qtractorMainForm::trackNavigateNext (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackNavigateNext()");
#endif

	if (m_pTracks) {
		qtractorTrackList *pTrackList = m_pTracks->trackList();
		const int iTrackCount = pTrackList->trackRowCount();
		int iTrack = pTrackList->currentTrackRow() + 1;
		if (iTrack >= iTrackCount)
			iTrack  = 0;
		if (iTrack >= 0 && iTrackCount >= iTrack)
			pTrackList->setCurrentTrackRow(iTrack);
	}
}


// Make current the last track on list.
void qtractorMainForm::trackNavigateLast (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackNavigateLast()");
#endif

	if (m_pTracks) {
		qtractorTrackList *pTrackList = m_pTracks->trackList();
		const int iTrack = pTrackList->trackRowCount() - 1;
		if (iTrack >= 0)
			pTrackList->setCurrentTrackRow(iTrack);
	}
}


// Make none current track on list.
void qtractorMainForm::trackNavigateNone (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackNavigateNone()");
#endif

	if (m_pTracks)
		m_pTracks->trackList()->setCurrentTrackRow(-1);
}


// Move current track to top of list.
void qtractorMainForm::trackMoveTop (void)
{
	if (m_pSession == nullptr)
		return;

	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackMoveTop()");
#endif

	m_pSession->execute(
		new qtractorMoveTrackCommand(pTrack, m_pSession->tracks().first()));
}


// Move current track up towards the top of list.
void qtractorMainForm::trackMoveUp (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorTrack *pNextTrack = pTrack->prev();
	if (pNextTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackMoveUp()");
#endif

	m_pSession->execute(
		new qtractorMoveTrackCommand(pTrack, pNextTrack));
}


// Move current track down towards the bottom of list
void qtractorMainForm::trackMoveDown (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorTrack *pNextTrack = pTrack->next();
	if (pNextTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackMoveDown()");
#endif

	m_pSession->execute(
		new qtractorMoveTrackCommand(pTrack, pNextTrack->next()));
}


// Move current track to bottom of list.
void qtractorMainForm::trackMoveBottom (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackMoveBottom()");
#endif

	m_pSession->execute(
		new qtractorMoveTrackCommand(pTrack, nullptr));
}


// Increase current track height.
void qtractorMainForm::trackHeightUp (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackHeightUp()");
#endif

	const int iZoomHeight = (150 * pTrack->zoomHeight()) / 100;
	m_pSession->execute(
		new qtractorResizeTrackCommand(pTrack, iZoomHeight));
}


// Decreate current track height.
void qtractorMainForm::trackHeightDown (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackHeightDown()");
#endif

	const int iZoomHeight = (75 * pTrack->zoomHeight()) / 100;
	m_pSession->execute(
		new qtractorResizeTrackCommand(pTrack, iZoomHeight));
}


// Reset current track height.
void qtractorMainForm::trackHeightReset (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackHeightReset()");
#endif

	const int iVerticalZoom = m_pSession->verticalZoom();
	const int iZoomHeight = (iVerticalZoom * qtractorTrack::HeightBase) / 100;
	m_pSession->execute(
		new qtractorResizeTrackCommand(pTrack, iZoomHeight));
}


// Auto-monitor current track.
void qtractorMainForm::trackAutoMonitor ( bool bOn )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackAutoMonitor(%d)", int(bOn));
#endif

	qtractorTrack *pTrack = nullptr;
	if (bOn && m_pTracks)
		pTrack = m_pTracks->currentTrack();
	m_pSession->setCurrentTrack(pTrack);
}


// Auto-deactivate plugins not producing sound.
void qtractorMainForm::trackAutoDeactivate ( bool bOn )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackAutoDeactivate(%d)", int(bOn));
#endif

	m_pSession->setAutoDeactivate(bOn);
}


// Import some tracks from Audio file.
void qtractorMainForm::trackImportAudio (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackImportAudio()");
#endif

	// Import Audio files into tracks...
	if (m_pTracks) {
		const unsigned long iClipStart = m_pSession->editHead();
		qtractorTrack *pTrack = m_pTracks->currentTrack();
		m_pTracks->addAudioTracks(
			m_pFiles->audioListView()->openFileNames(), iClipStart, 0, 0, pTrack);
		m_pTracks->trackView()->ensureVisibleFrame(iClipStart);
	}
}


// Import some tracks from MIDI file.
void qtractorMainForm::trackImportMidi (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackImportMidi()");
#endif

	// Import MIDI files into tracks...
	if (m_pTracks) {
		const unsigned long iClipStart = m_pSession->editHead();
		qtractorTrack *pTrack = m_pTracks->currentTrack();
		m_pTracks->addMidiTracks(
			m_pFiles->midiListView()->openFileNames(), iClipStart, 0, 0, pTrack);
		m_pTracks->trackView()->ensureVisibleFrame(iClipStart);
	}
}


// Export tracks to audio file.
void qtractorMainForm::trackExportAudio (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackExportAudio()");
#endif

	// Disable auto-deactivation during export. Disabling here has a nice side
	// effect: While the user makes the selections in the export dialog, the
	// reactivated plugins have time to finish sounds which were active when
	// plugins were deactivated and those remnants don't make it into exported
	// file.
	const bool bAutoDeactivate = m_pSession->isAutoDeactivate();
	m_pSession->setAutoDeactivate(false);

	qtractorExportTrackForm exportForm(this);
	exportForm.setExportType(qtractorTrack::Audio);
	exportForm.exec();

	m_pSession->setAutoDeactivate(bAutoDeactivate);
}


// Export tracks to MIDI file.
void qtractorMainForm::trackExportMidi (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackExportMidi()");
#endif

	qtractorExportTrackForm exportForm(this);
	exportForm.setExportType(qtractorTrack::Midi);
	exportForm.exec();
}


// Track automation curve selection menu.
Q_DECLARE_METATYPE(qtractorMidiControlObserver *);

void qtractorMainForm::trackCurveSelect ( QAction *pAction, bool bOn )
{
	qtractorSubject *pSubject = nullptr;
	qtractorCurveList *pCurveList = nullptr;
	qtractorMidiControlObserver *pMidiObserver
		= pAction->data().value<qtractorMidiControlObserver *> ();
	if (pMidiObserver) {
		pCurveList = pMidiObserver->curveList();
		pSubject = pMidiObserver->subject();
	} else {
		qtractorTrack *pTrack = nullptr;
		if (m_pTracks)
			pTrack = m_pTracks->currentTrack();
		if (pTrack)
			pCurveList = pTrack->curveList();
	}

	if (pCurveList == nullptr)
		return;

	qtractorCurve *pCurve = nullptr;
	if (bOn && pSubject) {
		pCurve = pSubject->curve();
		if (pCurve == nullptr) {
			qtractorCurve::Mode mode = qtractorCurve::Hold;
			if (m_pOptions && pSubject->isDecimal())
				mode = qtractorCurve::Mode(m_pOptions->iCurveMode);
			pCurve = new qtractorCurve(pCurveList, pSubject, mode);
			pCurve->setLogarithmic(pMidiObserver->isLogarithmic());
			if (m_pOptions && !m_pOptions->sCurveColor.isEmpty()) {
			#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
				pCurve->setColor(QColor::fromString(m_pOptions->sCurveColor));
			#else
				pCurve->setColor(QColor(m_pOptions->sCurveColor));
			#endif
			}
		}
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveSelect(%p)", pCurve);
#endif

	m_pSession->execute(new qtractorCurveSelectCommand(pCurveList, pCurve));
}

void qtractorMainForm::trackCurveSelect ( bool bOn )
{
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction)
		trackCurveSelect(pAction, bOn);
}


void qtractorMainForm::trackCurveMode ( QAction *pAction )
{
	const int iMode = pAction->data().toInt();
	if (iMode < 0)
		return;

	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveMode(%d)", iMode);
#endif

	// Save as future default...
	if (m_pOptions)
		m_pOptions->iCurveMode = iMode;

	const qtractorCurve::Mode mode = qtractorCurve::Mode(iMode);

	m_pSession->execute(new qtractorCurveModeCommand(pCurrentCurve, mode));
}


// Track automation curve lock toggle.
void qtractorMainForm::trackCurveLocked ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveLocked(%d)", int(bOn));
#endif

	pCurrentCurve->setLocked(bOn);

	m_pTracks->updateTrackView();

	updateDirtyCount(true);
	++m_iStabilizeTimer;
}


// Track automation curve playback toggle.
void qtractorMainForm::trackCurveProcess ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveProcess(%d)", int(bOn));
#endif

	m_pSession->execute(new qtractorCurveProcessCommand(pCurrentCurve, bOn));
}


// Track automation curve record toggle.
void qtractorMainForm::trackCurveCapture ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveCapture(%d)", int(bOn));
#endif

	m_pSession->execute(new qtractorCurveCaptureCommand(pCurrentCurve, bOn));
}


// Track automation curve logarithmic toggle.
void qtractorMainForm::trackCurveLogarithmic ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveLogarithmic(%d)", int(bOn));
#endif

	m_pSession->execute(new qtractorCurveLogarithmicCommand(pCurrentCurve, bOn));
}


// Track automation curve color picker.
void qtractorMainForm::trackCurveColor (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveColor()");
#endif

	QWidget *pParentWidget = nullptr;
	QColorDialog::ColorDialogOptions options;
	if (m_pOptions && m_pOptions->bDontUseNativeDialogs) {
		options |= QColorDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
	const QString& sTitle = pCurrentCurve->subject()->name();
	const QColor& color = QColorDialog::getColor(
		pCurrentCurve->color(), pParentWidget,
		sTitle, options);
	if (!color.isValid())
		return;

	// Save as future default...
	if (m_pOptions)
		m_pOptions->sCurveColor = color.name();

	m_pSession->execute(new qtractorCurveColorCommand(pCurrentCurve, color));
}


// Track automation curve clear.
void qtractorMainForm::trackCurveClear (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return;
	if (pCurrentCurve->isEmpty())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveClear()");
#endif

	if (m_pOptions && m_pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning"),
			tr("About to clear automation:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(pCurrentCurve->subject()->name()),
			QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
			return;
	}

	m_pSession->execute(new qtractorCurveClearCommand(pCurrentCurve));
}


// Track automation all curves lock toggle.
void qtractorMainForm::trackCurveLockedAll ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveLockedAll(%d)", int(bOn));
#endif

	pCurveList->setLockedAll(bOn);

	m_pTracks->updateTrackView();

	updateDirtyCount(true);
	++m_iStabilizeTimer;
}


// Track automation all curves playback toggle.
void qtractorMainForm::trackCurveProcessAll ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveProcessAll(%d)", int(bOn));
#endif

	m_pSession->execute(new qtractorCurveProcessAllCommand(pCurveList, bOn));
}


// Track automation all curves record toggle.
void qtractorMainForm::trackCurveCaptureAll ( bool bOn )
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveCaptureAll(%d)", int(bOn));
#endif

	m_pSession->execute(new qtractorCurveCaptureAllCommand(pCurveList, bOn));
}


// Track automation all curves clear.
void qtractorMainForm::trackCurveClearAll (void)
{
	qtractorTrack *pTrack = nullptr;
	if (m_pTracks)
		pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return;

	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList == nullptr)
		return;
	if (pCurveList->isEmpty())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::trackCurveClearAll()");
#endif

	if (m_pOptions && m_pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning"),
			tr("About to clear all automation:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(pTrack->shortTrackName()),
			QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
			return;
	}

	m_pSession->execute(new qtractorCurveClearAllCommand(pCurveList));
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Clip Action slots.

// Enter in clip create mode.
void qtractorMainForm::clipNew (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipNew()");
#endif

	// New clip: we must have a session name...
	if (m_pSession->sessionName().isEmpty() && !editSession())
		return;

	// Start editing a new clip...
	if (m_pTracks)
		m_pTracks->newClip();
}


// Enter in clip edit mode.
void qtractorMainForm::clipEdit (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipEdit()");
#endif

	// Start editing the current clip, if any...
	if (m_pTracks)
		m_pTracks->editClip();
}


// Mute a clip.
void qtractorMainForm::clipMute (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipMute()");
#endif

	// Mute the current clip, if any...
	if (m_pTracks)
		m_pTracks->muteClip();
}


// Unlink a (MIDI) linked clip.
void qtractorMainForm::clipUnlink (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipUnlink()");
#endif

	// Unlink the current clip, if any...
	if (m_pTracks)
		m_pTracks->unlinkClip();
}


// Enter in clip record/ overdub mode.
void qtractorMainForm::clipRecordEx ( bool bOn )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipRecordEx(%d)", int(bOn));
#endif

	// Start record/overdub the current clip, if any...
	if (m_pTracks) {
		qtractorClip *pClip = m_pTracks->currentClip();
		if (pClip) {
			qtractorTrack *pTrack = pClip->track();
			if (pTrack && pTrack->trackType() == qtractorTrack::Midi)
				m_pSession->execute(new qtractorClipRecordExCommand(pClip, bOn));
		}
	}
}


// Split current clip at playhead.
void qtractorMainForm::clipSplit (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipSplit()");
#endif

	// Split current clip, if any...
	if (m_pTracks)
		m_pTracks->splitClip();
}


// Merge selected (MIDI) clips.
void qtractorMainForm::clipMerge (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipMerge()");
#endif

	// Merge clip selection, if any...
	if (m_pTracks)
		m_pTracks->mergeClips();
}


// Normalize current clip.
void qtractorMainForm::clipNormalize (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipNormalize()");
#endif

	// Normalize current clip, if any...
	if (m_pTracks)
		m_pTracks->normalizeClip();
}


// Adjust current tempo from clip selection or interactive tapping...
void qtractorMainForm::clipTempoAdjust (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTempoAdjust()");
#endif

	if (m_pTracks)
		m_pTracks->tempoClip();
}


// Cross-fade current overllaping clips...
void qtractorMainForm::clipCrossFade (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipCrossFade()");
#endif

	if (m_pTracks)
		m_pTracks->crossFadeClip();
}


// Set edit-range from current clip.
void qtractorMainForm::clipRangeSet (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipRangeSet()");
#endif

	if (m_pTracks)
		m_pTracks->rangeClip();
}


// Set loop from current clip range.
void qtractorMainForm::clipLoopSet (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipLoopSet()");
#endif

	if (m_pTracks)
		m_ui.clipLoopSetAction->setChecked(m_pTracks->loopClip());
}


// Import (audio) clip.
void qtractorMainForm::clipImport (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipImport()");
#endif

	// Import (audio) clip(s)...
	if (m_pTracks) {
		// Depending on current track type (default to audio)...
		const unsigned long iClipStart = m_pSession->editHead();
		QStringList files;
		qtractorTrack *pTrack = m_pTracks->currentTrack();
		if (pTrack == nullptr)
			pTrack = m_pSession->tracks().first();
		if (pTrack && pTrack->trackType() == qtractorTrack::Midi)
			files = m_pFiles->midiListView()->openFileNames();
		else
			files = m_pFiles->audioListView()->openFileNames();
		m_pTracks->importClips(files, iClipStart);
		m_pTracks->trackView()->ensureVisibleFrame(iClipStart);
	}
}


// Export current clip.
void qtractorMainForm::clipExport (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipExport()");
#endif

	// Export current clip, if any...
	if (m_pTracks)
		m_pTracks->exportClips();
}


// Quantize current clip.
void qtractorMainForm::clipToolsQuantize (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsQuantize()");
#endif

	// Quantize current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Quantize);
}


// Transpose current clip.
void qtractorMainForm::clipToolsTranspose (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsTranspose()");
#endif

	// Tranpose current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Transpose);
}


// Normalize current clip.
void qtractorMainForm::clipToolsNormalize (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsNormalize()");
#endif

	// Normalize current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Normalize);
}


// Randomize current clip.
void qtractorMainForm::clipToolsRandomize (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsRandomize()");
#endif

	// Randomize current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Randomize);
}


// Resize current clip.
void qtractorMainForm::clipToolsResize (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsResize()");
#endif

	// Resize current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Resize);
}


// Rescale current clip.
void qtractorMainForm::clipToolsRescale (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsRescale()");
#endif

	// Rescale  current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Rescale);
}


// Timeshift current clip.
void qtractorMainForm::clipToolsTimeshift (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsTimeshift()");
#endif

	// Timeshift current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Timeshift);
}


// Temporamp current clip.
void qtractorMainForm::clipToolsTemporamp (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipToolsTemporamp()");
#endif

	// Temporamp current clip events, if any...
	if (m_pTracks)
		m_pTracks->executeClipTool(qtractorMidiEditor::Temporamp);
}


// Select current clip take.
void qtractorMainForm::clipTakeSelect ( QAction *pAction )
{
	const int iTake = pAction->data().toInt();

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTakeSelect(%d)", iTake);
#endif

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pTakeInfo && pTakeInfo->currentTake() != iTake) {
		m_pSession->execute(
			new qtractorClipTakeCommand(pTakeInfo, pClip->track(), iTake));
	}
}


// Select first clip take.
void qtractorMainForm::clipTakeFirst (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTakeFirst()");
#endif

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pTakeInfo) {
		m_pSession->execute(
			new qtractorClipTakeCommand(pTakeInfo, pClip->track(), 0));
	}
}


// Select previous clip take.
void qtractorMainForm::clipTakePrev (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTakePrev()");
#endif

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pTakeInfo) {
		int iTake = pTakeInfo->currentTake() - 1;
		if (iTake < 0)
			iTake = pTakeInfo->takeCount() - 1; // Wrap to last take.
		m_pSession->execute(
			new qtractorClipTakeCommand(pTakeInfo, pClip->track(), iTake));
	}
}


// Select next clip take.
void qtractorMainForm::clipTakeNext (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTakeNext()");
#endif

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pTakeInfo) {
		int iTake = pTakeInfo->currentTake() + 1;
		if (iTake >= pTakeInfo->takeCount())
			iTake = 0; // Wrap to first take.
		m_pSession->execute(
			new qtractorClipTakeCommand(pTakeInfo, pClip->track(), iTake));
	}
}


// Select last clip take.
void qtractorMainForm::clipTakeLast (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTakeLast()");
#endif

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pTakeInfo) {
		const int iTake = pTakeInfo->takeCount() - 1;
		m_pSession->execute(
			new qtractorClipTakeCommand(pTakeInfo, pClip->track(), iTake));
	}
}


// Unfold current clip takes.
void qtractorMainForm::clipTakeReset (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTakeReset()");
#endif

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pTakeInfo) {
		m_pSession->execute(
			new qtractorClipTakeCommand(pTakeInfo));
	}
}


// Fold current clip into takes.
void qtractorMainForm::clipTakeRange (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::clipTakeRange()");
#endif

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pClip && pTakeInfo == nullptr) {
		qtractorTakeRangeForm form(this);
		form.setClip(pClip);
		if (form.exec()) {
			const unsigned long iTakeStart = form.takeStart();
			const unsigned long iTakeEnd = form.takeEnd();
			pTakeInfo = new qtractorClip::TakeInfo(
				pClip->clipStart(), pClip->clipOffset(), pClip->clipLength(),
				iTakeStart, iTakeEnd, 0);
			qtractorClipCommand *pClipCommand
				= new qtractorClipCommand(tr("take range"));
			pClipCommand->takeInfoClip(pClip, pTakeInfo);
			pTakeInfo->setClipPart(qtractorClip::TakeInfo::ClipTake, pClip);
			int iTake = form.currentTake();
			iTake = pTakeInfo->select(pClipCommand, pClip->track(), iTake);
			pTakeInfo->setCurrentTake(iTake);
			if (m_pSession->execute(pClipCommand)) {
				m_pSession->setEditHead(iTakeStart);
				m_pSession->setEditTail(iTakeEnd);
				selectionNotifySlot(nullptr);
			}
		}
	}
}


//-------------------------------------------------------------------------
// qtractorMainForm -- View Action slots.

// Show/hide the main program window menubar.
void qtractorMainForm::viewMenubar ( bool bOn )
{
	m_ui.menuBar->setVisible(bOn);
}


// Show/hide the main program window statusbar.
void qtractorMainForm::viewStatusbar ( bool bOn )
{
	statusBar()->setVisible(bOn);
}


// Show/hide the file-toolbar.
void qtractorMainForm::viewToolbarFile ( bool bOn )
{
	m_ui.fileToolbar->setVisible(bOn);
}


// Show/hide the edit-toolbar.
void qtractorMainForm::viewToolbarEdit ( bool bOn )
{
	m_ui.editToolbar->setVisible(bOn);
}


// Show/hide the track-toolbar.
void qtractorMainForm::viewToolbarTrack ( bool bOn )
{
	m_ui.trackToolbar->setVisible(bOn);
}


// Show/hide the view-toolbar.
void qtractorMainForm::viewToolbarView ( bool bOn )
{
	m_ui.viewToolbar->setVisible(bOn);
}


// Show/hide the options toolbar.
void qtractorMainForm::viewToolbarOptions ( bool bOn )
{
	m_ui.optionsToolbar->setVisible(bOn);
}


// Show/hide the transport toolbar.
void qtractorMainForm::viewToolbarTransport ( bool bOn )
{
	m_ui.transportToolbar->setVisible(bOn);
}


// Show/hide the time toolbar.
void qtractorMainForm::viewToolbarTime ( bool bOn )
{
	m_ui.timeToolbar->setVisible(bOn);
}


// Show/hide the thumb (track-line)ime toolbar.
void qtractorMainForm::viewToolbarThumb ( bool bOn )
{
	m_ui.thumbViewToolbar->setVisible(bOn);
}


// Show/hide the file-system window view.
void qtractorMainForm::viewFileSystem ( bool bOn )
{
	m_pFileSystem->setVisible(bOn);
}


// Show/hide the files window view.
void qtractorMainForm::viewFiles ( bool bOn )
{
	m_pFiles->setVisible(bOn);
}


// Show/hide the messages window logger.
void qtractorMainForm::viewMessages ( bool bOn )
{
	m_pMessages->setVisible(bOn);
}


// Show/hide the mixer window.
void qtractorMainForm::viewMixer ( bool bOn )
{
	if (m_pOptions)
		m_pOptions->saveWidgetGeometry(m_pMixer);

	m_pMixer->setVisible(bOn);
}


// Show/hide the connections window.
void qtractorMainForm::viewConnections ( bool bOn )
{
	if (m_pOptions)
		m_pOptions->saveWidgetGeometry(m_pConnections);

	if (bOn) m_pConnections->reset();
	m_pConnections->setVisible(bOn);
}


// Horizontal and/or vertical zoom-in.
void qtractorMainForm::viewZoomIn (void)
{
	if (m_pTracks)
		m_pTracks->zoomIn();
}


// Horizontal and/or vertical zoom-out.
void qtractorMainForm::viewZoomOut (void)
{
	if (m_pTracks)
		m_pTracks->zoomOut();
}


// Reset zoom level to default.
void qtractorMainForm::viewZoomReset (void)
{
	if (m_pTracks)
		m_pTracks->zoomReset();
}


// Set horizontal zoom mode
void qtractorMainForm::viewZoomHorizontal (void)
{
	if (m_pTracks)
		m_pTracks->setZoomMode(qtractorTracks::ZoomHorizontal);
}


// Set vertical zoom mode
void qtractorMainForm::viewZoomVertical (void)
{
	if (m_pTracks)
		m_pTracks->setZoomMode(qtractorTracks::ZoomVertical);
}


// Set all zoom mode
void qtractorMainForm::viewZoomAll (void)
{
	if (m_pTracks)
		m_pTracks->setZoomMode(qtractorTracks::ZoomAll);
}


// Set zebra mode
void qtractorMainForm::viewSnapZebra ( bool bOn )
{
	if (m_pTracks)
		m_pTracks->trackView()->setSnapZebra(bOn);
}


// Set grid mode
void qtractorMainForm::viewSnapGrid ( bool bOn )
{
	if (m_pTracks)
		m_pTracks->trackView()->setSnapGrid(bOn);
}


// Set floating tool-tips view mode
void qtractorMainForm::viewToolTips ( bool bOn )
{
	if (m_pTracks)
		m_pTracks->trackView()->setToolTips(bOn);
}


// Change snap-per-beat setting via menu.
void qtractorMainForm::viewSnap (void)
{
	// Retrieve snap-per-beat index from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		// Commit the change as usual...
		snapPerBeatChanged(pAction->data().toInt());
		// Update the other toolbar control...
		m_pSnapPerBeatComboBox->setCurrentIndex(
			qtractorTimeScale::indexFromSnap(m_pSession->snapPerBeat()));
	}
}


// Refresh view display.
void qtractorMainForm::viewRefresh (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::viewRefresh()");
#endif

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Update the whole session view dependables...
	m_pTempoCursor->clear();
	m_pSession->updateTimeScale();
	m_pSession->updateSession();

	// Initialize toolbar widgets...
//	m_pTempoSpinBox->setTempo(m_pSession->tempo(), false);
//	m_pTempoSpinBox->setBeatsPerBar(m_pSession->beatsPerBar(), false);
//	m_pTempoSpinBox->setBeatDivisor(m_pSession->beatDivisor(), false);
	m_pSnapPerBeatComboBox->setCurrentIndex(
		qtractorTimeScale::indexFromSnap(m_pSession->snapPerBeat()));

	// Read session edit-head/tails...
	const unsigned long iEditHead = m_pSession->editHead();
	const unsigned long iEditTail = m_pSession->editTail();

	if (m_pTracks) {
		qtractorTrackView *pTrackView = m_pTracks->trackView();
		pTrackView->setEditHead(iEditHead);
		pTrackView->setEditTail(iEditTail);
		pTrackView->setPlayHeadAutoBackward(
			m_pSession->playHeadAutoBackward());
		m_pTracks->updateContents(true);
	}

	if (m_pConnections)
		m_pConnections->refresh();
	if (m_pMixer) {
		m_pMixer->updateBuses();
		m_pMixer->updateTracks();
	}

	if (m_pThumbView)
		m_pThumbView->updateContents();

	// Update other editors contents...
	QListIterator<qtractorMidiEditorForm *> iter(m_editors);
	while (iter.hasNext()) {
		qtractorMidiEditorForm *pForm = iter.next();
		pForm->updateTimeScale();
		qtractorMidiEditor *pEditor = pForm->editor();
		pEditor->setEditHead(iEditHead, false);
		pEditor->setEditTail(iEditTail, false);
	}

	// Reset XRUN counters...
	m_iXrunCount = 0;
	m_iXrunSkip  = 0;
	m_iXrunTimer = 0;

	++m_iStabilizeTimer;

	// We're formerly done.
	QApplication::restoreOverrideCursor();
}


// Show instruments dialog.
void qtractorMainForm::viewInstruments (void)
{
	// Just set and show the instruments dialog...
	qtractorInstrumentForm(this).exec();
}


// Show MIDI controllers dialog.
void qtractorMainForm::viewControllers (void)
{
	// Just set and show the MIDI controllers dialog...
	qtractorMidiControlForm(this).exec();
}


// Show buses dialog.
void qtractorMainForm::viewBuses (void)
{
	// Just set and show the buses dialog...
	qtractorBusForm(this).exec();
}


// Show tempo-map dialog.
void qtractorMainForm::viewTempoMap (void)
{
	// Just set and show the tempo-map dialog...
	qtractorTimeScaleForm form(this);
	form.setFrame(m_iPlayHead);
	form.exec();
}


// Show options dialog.
void qtractorMainForm::viewOptions (void)
{
	if (m_pOptions == nullptr)
		return;

	// Check out some initial nullities(tm)...
	if (m_pOptions->sMessagesFont.isEmpty() && m_pMessages)
		m_pOptions->sMessagesFont = m_pMessages->messagesFont().toString();
	// To track down deferred or immediate changes.
	const bool    bOldMessagesLog        = m_pOptions->bMessagesLog;
	const QString sOldMessagesLogPath    = m_pOptions->sMessagesLogPath;
	const QString sOldMessagesFont       = m_pOptions->sMessagesFont;
	const bool    bOldStdoutCapture      = m_pOptions->bStdoutCapture;
	const int     bOldMessagesLimit      = m_pOptions->bMessagesLimit;
	const int     iOldMessagesLimitLines = m_pOptions->iMessagesLimitLines;
	const bool    bOldCompletePath       = m_pOptions->bCompletePath;
	const bool    bOldPeakAutoRemove     = m_pOptions->bPeakAutoRemove;
	const bool    bOldKeepToolsOnTop     = m_pOptions->bKeepToolsOnTop;
	const bool    bOldKeepEditorsOnTop   = m_pOptions->bKeepEditorsOnTop;
	const int     iOldMaxRecentFiles     = m_pOptions->iMaxRecentFiles;
	const int     iOldDisplayFormat      = m_pOptions->iDisplayFormat;
	const int     iOldBaseFontSize       = m_pOptions->iBaseFontSize;
	const int     iOldResampleType       = m_pOptions->iAudioResampleType;
	const bool    bOldWsolaTimeStretch   = m_pOptions->bAudioWsolaTimeStretch;
	const bool    bOldWsolaQuickSeek     = m_pOptions->bAudioWsolaQuickSeek;
	const bool    bOldAudioPlayerAutoConnect = m_pOptions->bAudioPlayerAutoConnect;
	const bool    bOldAudioPlayerBus     = m_pOptions->bAudioPlayerBus;
	const bool    bOldAudioMetronome     = m_pOptions->bAudioMetronome;
	const int     iOldTransportMode      = m_pOptions->iTransportMode;
	const bool    bOldTimebase           = m_pOptions->bTimebase;
	const int     iOldMidiMmcDevice      = m_pOptions->iMidiMmcDevice;
	const int     iOldMidiMmcMode        = m_pOptions->iMidiMmcMode;
	const int     iOldMidiSppMode        = m_pOptions->iMidiSppMode;
	const int     iOldMidiClockMode      = m_pOptions->iMidiClockMode;
	const bool    bOldMidiResetAllControllers = m_pOptions->bMidiResetAllControllers;
	const int     iOldMidiCaptureQuantize = m_pOptions->iMidiCaptureQuantize;
	const int     iOldMidiQueueTimer     = m_pOptions->iMidiQueueTimer;
	const bool    bOldMidiDriftCorrect   = m_pOptions->bMidiDriftCorrect;
	const bool    bOldMidiPlayerBus      = m_pOptions->bMidiPlayerBus;
	const QString sOldMetroBarFilename   = m_pOptions->sMetroBarFilename;
	const float   fOldMetroBarGain       = m_pOptions->fMetroBarGain;
	const QString sOldMetroBeatFilename  = m_pOptions->sMetroBeatFilename;
	const float   fOldMetroBeatGain      = m_pOptions->fMetroBeatGain;
	const bool    bOldAudioMetroBus      = m_pOptions->bAudioMetroBus;
	const bool    bOldAudioMetroAutoConnect = m_pOptions->bAudioMetroAutoConnect;
	const unsigned long iOldAudioMetroOffset = m_pOptions->iAudioMetroOffset;
	const int     iOldAudioCountInMode   = m_pOptions->iAudioCountInMode;
	const int     iOldAudioCountInBeats  = m_pOptions->iAudioCountInBeats;
	const bool    bOldMidiControlBus     = m_pOptions->bMidiControlBus;
	const bool    bOldMidiMetronome      = m_pOptions->bMidiMetronome;
	const int     iOldMidiCountInMode    = m_pOptions->iMidiCountInMode;
	const int     iOldMidiCountInBeats   = m_pOptions->iMidiCountInBeats;
	const int     iOldMetroChannel       = m_pOptions->iMetroChannel;
	const int     iOldMetroBarNote       = m_pOptions->iMetroBarNote;
	const int     iOldMetroBarVelocity   = m_pOptions->iMetroBarVelocity;
	const int     iOldMetroBarDuration   = m_pOptions->iMetroBarDuration;
	const int     iOldMetroBeatNote      = m_pOptions->iMetroBeatNote;
	const int     iOldMetroBeatVelocity  = m_pOptions->iMetroBeatVelocity;
	const int     iOldMetroBeatDuration  = m_pOptions->iMetroBeatDuration;
	const bool    bOldMidiMetroBus       = m_pOptions->bMidiMetroBus;
	const int     iOldMidiMetroOffset    = m_pOptions->iMidiMetroOffset;
	const bool    bOldSyncViewHold       = m_pOptions->bSyncViewHold;
	const int     iOldTrackColorSaturation = m_pOptions->iTrackColorSaturation;
	const QString sOldCustomColorTheme   = m_pOptions->sCustomColorTheme;
	const QString sOldCustomStyleTheme   = m_pOptions->sCustomStyleTheme;
	const QString sOldCustomStyleSheet   = m_pOptions->sCustomStyleSheet;
	const QString sOldCustomIconsTheme   = m_pOptions->sCustomIconsTheme;
#ifdef CONFIG_LV2
	const QString sep(':'); 
	const QString sOldLv2Paths           = m_pOptions->lv2Paths.join(sep);
#endif
	// Load the current setup settings.
	qtractorOptionsForm optionsForm(this);
	optionsForm.setOptions(m_pOptions);
	// Show the setup dialog...
	if (optionsForm.exec()) {
		enum { RestartSession = 1, RestartProgram = 2, RestartAny = 3 };
		int iNeedRestart = 0;
		// Check wheather something immediate has changed.
		if (iOldResampleType != m_pOptions->iAudioResampleType) {
			qtractorAudioBuffer::setDefaultResampleType(
				m_pOptions->iAudioResampleType);
			iNeedRestart |= RestartSession;
		}
		if (( bOldWsolaTimeStretch && !m_pOptions->bAudioWsolaTimeStretch) ||
			(!bOldWsolaTimeStretch &&  m_pOptions->bAudioWsolaTimeStretch)) {
			qtractorAudioBuffer::setDefaultWsolaTimeStretch(
				m_pOptions->bAudioWsolaTimeStretch);
			iNeedRestart |= RestartSession;
		}
		if (( bOldWsolaQuickSeek && !m_pOptions->bAudioWsolaQuickSeek) ||
			(!bOldWsolaQuickSeek &&  m_pOptions->bAudioWsolaQuickSeek)) {
			qtractorAudioBuffer::setDefaultWsolaQuickSeek(
				m_pOptions->bAudioWsolaQuickSeek);
			iNeedRestart |= RestartSession;
		}
		// Audio engine control modes...
		if (iOldTransportMode != m_pOptions->iTransportMode) {
			++m_iDirtyCount; // Fake session properties change.
			updateTransportModePre();
			updateTransportModePost();
		//	iNeedRestart |= RestartSession;
		}
		if (( bOldTimebase && !m_pOptions->bTimebase) ||
			(!bOldTimebase &&  m_pOptions->bTimebase)) {
			++m_iDirtyCount; // Fake session properties change.
			updateTimebase();
		//	iNeedRestart |= RestartSession;
		}
		// MIDI engine queue timer...
		if (iOldMidiQueueTimer != m_pOptions->iMidiQueueTimer) {
			updateMidiQueueTimer();
			iNeedRestart |= RestartSession;
		}
	#ifdef CONFIG_LV2
		if (sOldLv2Paths != m_pOptions->lv2Paths.join(sep))
			iNeedRestart |= RestartProgram;
	#endif
		if (( bOldStdoutCapture && !m_pOptions->bStdoutCapture) ||
			(!bOldStdoutCapture &&  m_pOptions->bStdoutCapture)) {
			updateMessagesCapture();
			iNeedRestart |= RestartProgram;
		}
		if (iOldBaseFontSize != m_pOptions->iBaseFontSize)
			iNeedRestart |= RestartProgram;
		if (sOldCustomIconsTheme != m_pOptions->sCustomIconsTheme)
			iNeedRestart |= RestartProgram;
		if (sOldCustomStyleSheet != m_pOptions->sCustomStyleSheet)
			updateCustomStyleSheet();
		if (sOldCustomStyleTheme != m_pOptions->sCustomStyleTheme) {
			if (m_pOptions->sCustomStyleTheme.isEmpty())
				iNeedRestart |= RestartProgram;
			else
				updateCustomStyleTheme();
		}
		if ((sOldCustomColorTheme != m_pOptions->sCustomColorTheme) ||
			(optionsForm.isDirtyCustomColorThemes())) {
			if (m_pOptions->sCustomColorTheme.isEmpty())
				iNeedRestart |= RestartProgram;
			else
				updateCustomColorTheme();
		}
		if (optionsForm.isDirtyMeterColors())
			qtractorMeterValue::updateAll();
		if (( bOldCompletePath && !m_pOptions->bCompletePath) ||
			(!bOldCompletePath &&  m_pOptions->bCompletePath) ||
			(iOldMaxRecentFiles != m_pOptions->iMaxRecentFiles))
			updateRecentFilesMenu();
		if (( bOldPeakAutoRemove && !m_pOptions->bPeakAutoRemove) ||
			(!bOldPeakAutoRemove &&  m_pOptions->bPeakAutoRemove))
			updatePeakAutoRemove();
		if (( bOldKeepToolsOnTop && !m_pOptions->bKeepToolsOnTop) ||
			(!bOldKeepToolsOnTop &&  m_pOptions->bKeepToolsOnTop))
			iNeedRestart |= RestartProgram;
		if (( bOldKeepEditorsOnTop && !m_pOptions->bKeepEditorsOnTop) ||
			(!bOldKeepEditorsOnTop &&  m_pOptions->bKeepEditorsOnTop))
			updateEditorForms();
		if (sOldMessagesFont != m_pOptions->sMessagesFont)
			updateMessagesFont();
		if (( bOldMessagesLimit && !m_pOptions->bMessagesLimit) ||
			(!bOldMessagesLimit &&  m_pOptions->bMessagesLimit) ||
			(iOldMessagesLimitLines !=  m_pOptions->iMessagesLimitLines))
			updateMessagesLimit();
		if (iOldDisplayFormat != m_pOptions->iDisplayFormat)
			updateDisplayFormat();
		if (( bOldMessagesLog && !m_pOptions->bMessagesLog) ||
			(!bOldMessagesLog &&  m_pOptions->bMessagesLog) ||
			(sOldMessagesLogPath != m_pOptions->sMessagesLogPath))
			m_pMessages->setLogging(
				m_pOptions->bMessagesLog, m_pOptions->sMessagesLogPath);
		// FIXME: This is what it should ever be,
		// make it right from this very moment...
		qtractorAudioFileFactory::setDefaultType(
			m_pOptions->sAudioCaptureExt,
			m_pOptions->iAudioCaptureType,
			m_pOptions->iAudioCaptureFormat,
			m_pOptions->iAudioCaptureQuality);
		qtractorMidiClip::setDefaultFormat(
			m_pOptions->iMidiCaptureFormat);
		// Set default MIDI (plugin) instrument audio output mode.
		qtractorMidiManager::setDefaultAudioOutputBus(
			m_pOptions->bAudioOutputBus);
		qtractorMidiManager::setDefaultAudioOutputAutoConnect(
			m_pOptions->bAudioOutputAutoConnect);
		// Auto time-stretching, loop-recording global modes...
		if (m_pSession) {
			m_pSession->setAutoTimeStretch(m_pOptions->bAudioAutoTimeStretch);
			m_pSession->setLoopRecordingMode(m_pOptions->iLoopRecordingMode);
		}
		// Special track-view drop-span mode...
		if (m_pTracks)
			m_pTracks->trackView()->setDropSpan(m_pOptions->bTrackViewDropSpan);
		// MIDI engine control modes...
		if ((iOldMidiMmcDevice != m_pOptions->iMidiMmcDevice) ||
			(iOldMidiMmcMode   != m_pOptions->iMidiMmcMode)   ||
			(iOldMidiSppMode   != m_pOptions->iMidiSppMode)   ||
			(iOldMidiClockMode != m_pOptions->iMidiClockMode) ||
			( bOldMidiResetAllControllers && !m_pOptions->bMidiResetAllControllers) ||
			(!bOldMidiResetAllControllers &&  m_pOptions->bMidiResetAllControllers) ||
			(iOldMidiCaptureQuantize != m_pOptions->iMidiCaptureQuantize)) {
			++m_iDirtyCount; // Fake session properties change.
			updateMidiControlModes();
		}
		// Audio engine audition/pre-listening player options...
		if (( bOldAudioPlayerBus && !m_pOptions->bAudioPlayerBus) ||
			(!bOldAudioPlayerBus &&  m_pOptions->bAudioPlayerBus) ||
			( bOldAudioPlayerAutoConnect && !m_pOptions->bAudioPlayerAutoConnect) ||
			(!bOldAudioPlayerAutoConnect &&  m_pOptions->bAudioPlayerAutoConnect))
			updateAudioPlayer();
		// MIDI engine drift correction option...
		if (( bOldMidiDriftCorrect && !m_pOptions->bMidiDriftCorrect) ||
			(!bOldMidiDriftCorrect &&  m_pOptions->bMidiDriftCorrect))
			updateMidiDriftCorrect();
		// MIDI engine player options...
		if (( bOldMidiPlayerBus && !m_pOptions->bMidiPlayerBus) ||
			(!bOldMidiPlayerBus &&  m_pOptions->bMidiPlayerBus))
			updateMidiPlayer();
		// MIDI engine control options...
		if (( bOldMidiControlBus && !m_pOptions->bMidiControlBus) ||
			(!bOldMidiControlBus &&  m_pOptions->bMidiControlBus))
			updateMidiControl();
		// Audio engine metronome options...
		if (( bOldAudioMetronome   && !m_pOptions->bAudioMetronome)   ||
			(!bOldAudioMetronome   &&  m_pOptions->bAudioMetronome)   ||
			(sOldMetroBarFilename  != m_pOptions->sMetroBarFilename)  ||
			(fOldMetroBarGain      != m_pOptions->fMetroBarGain)      ||
			(sOldMetroBeatFilename != m_pOptions->sMetroBeatFilename) ||
			(fOldMetroBeatGain     != m_pOptions->fMetroBeatGain)     ||
			(iOldAudioMetroOffset  != m_pOptions->iAudioMetroOffset)  ||
			( bOldAudioMetroBus    && !m_pOptions->bAudioMetroBus)    ||
			(!bOldAudioMetroBus    &&  m_pOptions->bAudioMetroBus)    ||
			(iOldAudioCountInMode  != m_pOptions->iAudioCountInMode)  ||
			(iOldAudioCountInBeats != m_pOptions->iAudioCountInBeats) ||
			( bOldAudioMetroAutoConnect && !m_pOptions->bAudioMetroAutoConnect) ||
			(!bOldAudioMetroAutoConnect &&  m_pOptions->bAudioMetroAutoConnect))
			updateAudioMetronome();
		// MIDI engine metronome options...
		if (( bOldMidiMetronome    && !m_pOptions->bMidiMetronome)    ||
			(!bOldMidiMetronome    &&  m_pOptions->bMidiMetronome)    ||
			(iOldMidiCountInMode   != m_pOptions->iMidiCountInMode)   ||
			(iOldMidiCountInBeats  != m_pOptions->iMidiCountInBeats)  ||
			(iOldMetroChannel      != m_pOptions->iMetroChannel)      ||
			(iOldMetroBarNote      != m_pOptions->iMetroBarNote)      ||
			(iOldMetroBarVelocity  != m_pOptions->iMetroBarVelocity)  ||
			(iOldMetroBarDuration  != m_pOptions->iMetroBarDuration)  ||
			(iOldMetroBeatNote     != m_pOptions->iMetroBeatNote)     ||
			(iOldMetroBeatVelocity != m_pOptions->iMetroBeatVelocity) ||
			(iOldMetroBeatDuration != m_pOptions->iMetroBeatDuration) ||
			(iOldMidiMetroOffset   != m_pOptions->iMidiMetroOffset)   ||
			( bOldMidiMetroBus     && !m_pOptions->bMidiMetroBus)     ||
			(!bOldMidiMetroBus     &&  m_pOptions->bMidiMetroBus))
			updateMidiMetronome();
		// Transport display options...
		if (( bOldSyncViewHold && !m_pOptions->bSyncViewHold) ||
			(!bOldSyncViewHold &&  m_pOptions->bSyncViewHold))
			updateSyncViewHold();
		// Default track color saturation factor [0..400].
		if (iOldTrackColorSaturation != m_pOptions->iTrackColorSaturation)
			qtractorTrack::setTrackColorSaturation(
				m_pOptions->iTrackColorSaturation);
		// Warn if something will be only effective on next time.
		if (iNeedRestart & RestartAny) {
			QString sNeedRestart;
			if (iNeedRestart & RestartSession)
				sNeedRestart += tr("session");
			if (iNeedRestart & RestartProgram) {
				if (!sNeedRestart.isEmpty())
					sNeedRestart += tr(" or ");
				sNeedRestart += tr("program");
			}
			// Show restart needed message...
			QMessageBox::information(this,
				tr("Information"),
				tr("Some settings may be only effective\n"
				"next time you start this %1.")
				.arg(sNeedRestart));
		}
		// Things that must be reset anyway...
		autoSaveReset();
	}

	// This makes it.
	++m_iStabilizeTimer;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Transport Action slots.

// Transport backward.
void qtractorMainForm::transportBackward (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportBackward()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Move playhead to edit-tail, head or full session-start.
	bool bShiftKeyModifier = QApplication::keyboardModifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier);
	if (m_pOptions && m_pOptions->bShiftKeyModifier)
		bShiftKeyModifier = !bShiftKeyModifier;
	if (bShiftKeyModifier) {
		m_pSession->setPlayHead(0);
	} else {
		const unsigned long iPlayHead = m_pSession->playHead();
		const bool bPlaying = m_pSession->isPlaying();
		QList<unsigned long> list;
		list.append(0);
		if (iPlayHead > m_pSession->playHeadAutoBackward())
			list.append(m_pSession->playHeadAutoBackward());
		if (iPlayHead > m_pSession->editHead())
			list.append(m_pSession->editHead());
		if (iPlayHead > m_pSession->editTail() && !bPlaying)
			list.append(m_pSession->editTail());
		if (m_pSession->isLooping()) {
			if (iPlayHead > m_pSession->loopStart())
				list.append(m_pSession->loopStart());
			if (iPlayHead > m_pSession->loopEnd() && !bPlaying)
				list.append(m_pSession->loopEnd());
		}
		if (m_pSession->isPunching()) {
			if (iPlayHead > m_pSession->punchIn())
				list.append(m_pSession->punchIn());
			if (iPlayHead > m_pSession->punchOut() && !bPlaying)
				list.append(m_pSession->punchOut());
		}
		if (iPlayHead > m_pSession->sessionStart())
			list.append(m_pSession->sessionStart());
	//	if (iPlayHead > m_pSession->sessionEnd() && !bPlaying)
	//		list.append(m_pSession->sessionEnd());
		qtractorTimeScale::Marker *pMarker
			= m_pSession->timeScale()->markers().seekFrame(iPlayHead);
		while (pMarker && pMarker->frame >= iPlayHead)
			pMarker = pMarker->prev();
		if (pMarker && iPlayHead > pMarker->frame)
			list.append(pMarker->frame);
		std::sort(list.begin(), list.end());
		m_pSession->setPlayHead(list.last());
	}

	++m_iTransportUpdate;
	++m_iStabilizeTimer;
}


// Transport rewind.
void qtractorMainForm::transportRewind (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportRewind()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Rolling direction and speed (negative)...
	bool bShiftKeyModifier = QApplication::keyboardModifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier);
	if (m_pOptions && m_pOptions->bShiftKeyModifier)
		bShiftKeyModifier = !bShiftKeyModifier;
	const int iRolling = (bShiftKeyModifier ? -3 : -1);

	// Toggle rolling backward...
	if (setRolling(iRolling) < 0) {
		setPlaying(false);
	} else {
		// Send MMC REWIND command...
		m_pSession->midiEngine()->sendMmcCommand(
			qtractorMmcEvent::REWIND);
	}

	++m_iStabilizeTimer;
}


// Transport fast-forward
void qtractorMainForm::transportFastForward (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportFastForward()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Rolling direction and speed (positive)...
	bool bShiftKeyModifier = QApplication::keyboardModifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier);
	if (m_pOptions && m_pOptions->bShiftKeyModifier)
		bShiftKeyModifier = !bShiftKeyModifier;
	const int iRolling = (bShiftKeyModifier ? +3 : +1);

	// Toggle rolling backward...
	if (setRolling(iRolling) > 0) {
		setPlayingEx(false);
	} else {
		// Send MMC FAST_FORWARD command...
		m_pSession->midiEngine()->sendMmcCommand(
			qtractorMmcEvent::FAST_FORWARD);
	}

	++m_iStabilizeTimer;
}


// Transport forward
void qtractorMainForm::transportForward (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportForward()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Move playhead to edit-head, tail or full session-end.
	bool bShiftKeyModifier = QApplication::keyboardModifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier);
	if (m_pOptions && m_pOptions->bShiftKeyModifier)
		bShiftKeyModifier = !bShiftKeyModifier;
	if (bShiftKeyModifier) {
		m_pSession->setPlayHead(m_pSession->sessionEnd());
	} else {
		const unsigned long iPlayHead = m_pSession->playHead();
		QList<unsigned long> list;
		if (iPlayHead < m_pSession->playHeadAutoBackward())
			list.append(m_pSession->playHeadAutoBackward());
		if (iPlayHead < m_pSession->editHead())
			list.append(m_pSession->editHead());
		if (iPlayHead < m_pSession->editTail())
			list.append(m_pSession->editTail());
		if (m_pSession->isLooping()) {
			if (iPlayHead < m_pSession->loopStart())
				list.append(m_pSession->loopStart());
			if (iPlayHead < m_pSession->loopEnd())
				list.append(m_pSession->loopEnd());
		}
		if (m_pSession->isPunching()) {
			if (iPlayHead < m_pSession->punchIn())
				list.append(m_pSession->punchIn());
			if (iPlayHead < m_pSession->punchOut())
				list.append(m_pSession->punchOut());
		}
		if (iPlayHead < m_pSession->sessionStart())
			list.append(m_pSession->sessionStart());
		if (iPlayHead < m_pSession->sessionEnd())
			list.append(m_pSession->sessionEnd());
		qtractorTimeScale::Marker *pMarker
			= m_pSession->timeScale()->markers().seekFrame(iPlayHead);
		while (pMarker && iPlayHead >= pMarker->frame)
			pMarker = pMarker->next();
		if (pMarker && iPlayHead < pMarker->frame)
			list.append(pMarker->frame);
		if (!list.isEmpty()) {
			std::sort(list.begin(), list.end());
			m_pSession->setPlayHead(list.first());
		}
	}

	++m_iTransportUpdate;
	++m_iStabilizeTimer;
}


// Transport step-backward
void qtractorMainForm::transportStepBackward (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportStepBackward()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	qtractorTimeScale *pTimeScale = m_pSession->timeScale();
	if (pTimeScale == nullptr)
		return;

	// Just make one step backward...
	unsigned long iPlayHead = m_pSession->playHead();
	const unsigned short iSnapPerBeat = pTimeScale->snapPerBeat();
	if (iSnapPerBeat > 0) {
		// Step-backward a beat/fraction...
		const unsigned long t0
			= pTimeScale->tickFromFrame(iPlayHead);
		const unsigned int iBeat
			= pTimeScale->beatFromTick(t0);
		const unsigned long t1
			= pTimeScale->tickFromBeat(iBeat > 0 ? iBeat : iBeat + 1);
		const unsigned long t2
			= pTimeScale->tickFromBeat(iBeat > 0 ? iBeat - 1 : iBeat);
		const unsigned long dt
			= (t1 - t2) / iSnapPerBeat;
		iPlayHead = pTimeScale->frameFromTick(
			pTimeScale->tickSnap(t0 > dt ? t0 - dt : 0));
	} else {
		// Step-backward a bar...
		const unsigned short iBar
			= pTimeScale->barFromFrame(iPlayHead);
		iPlayHead = pTimeScale->frameFromBar(iBar > 0 ? iBar - 1 : iBar);
	}

	m_pSession->setPlayHead(iPlayHead);

	++m_iTransportUpdate;
	++m_iStabilizeTimer;
}


// Transport step-forward
void qtractorMainForm::transportStepForward (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportStepForward()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	qtractorTimeScale *pTimeScale = m_pSession->timeScale();
	if (pTimeScale == nullptr)
		return;

	// Just make one step forward...
	unsigned long iPlayHead = m_pSession->playHead();
	const unsigned short iSnapPerBeat = pTimeScale->snapPerBeat();
	if (iSnapPerBeat > 0) {
		// Step-forward a beat/fraction...
		const unsigned long t0
			= pTimeScale->tickFromFrame(iPlayHead);
		const unsigned int iBeat
			= pTimeScale->beatFromTick(t0);
		const unsigned long t1
			= pTimeScale->tickFromBeat(iBeat);
		const unsigned long t2
			= pTimeScale->tickFromBeat(iBeat + 1);
		const unsigned long dt
			= (t2 - t1) / iSnapPerBeat;
		iPlayHead = pTimeScale->frameFromTick(
			pTimeScale->tickSnap(t0 + dt));
	} else {
		// Step-forward a bar...
		const unsigned short iBar
			= pTimeScale->barFromFrame(iPlayHead);
		iPlayHead = pTimeScale->frameFromBar(iBar + 1);
	}

	m_pSession->setPlayHead(iPlayHead);

	++m_iTransportUpdate;
	++m_iStabilizeTimer;
}


// Transport loop.
void qtractorMainForm::transportLoop (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportLoop()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Do the loop toggle switch...
	unsigned long iLoopStart = 0;
	unsigned long iLoopEnd   = 0;

	if (!m_pSession->isLooping()) {
		iLoopStart = m_pSession->editHead();
		iLoopEnd   = m_pSession->editTail();
	}

	// Now, express the change as an undoable command...
	m_pSession->execute(
		new qtractorSessionLoopCommand(m_pSession, iLoopStart, iLoopEnd));
}


// Transport loop setting.
void qtractorMainForm::transportLoopSet (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportLoopSet()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Now, express the change as an undoable command...
	m_pSession->execute(
		new qtractorSessionLoopCommand(m_pSession,
			m_pSession->editHead(), m_pSession->editTail()));
}


// Transport stop.
void qtractorMainForm::transportStop (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportStop()");
#endif

	// Make sure session is activated...
	if (!checkRestartSession())
		return;

	// Stop playing...
	if (setPlayingEx(false)) {
		// Auto-backward reset feature...
		if (m_ui.transportAutoBackwardAction->isChecked())
			m_pSession->setPlayHead(m_pSession->playHeadAutoBackward());
	}

	++m_iStabilizeTimer;
}


// Transport play.
void qtractorMainForm::transportPlay (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportPlay()");
#endif

	// Make sure session is activated...
	if (!checkRestartSession())
		return;

	// Toggle playing...
	const bool bPlaying = !m_pSession->isPlaying();
	if (setPlayingEx(bPlaying)) {
		// Save auto-backward return position...
		if (bPlaying) {
			const unsigned long iPlayHead = m_pSession->playHead();
			qtractorTrackView *pTrackView = m_pTracks->trackView();
			pTrackView->setPlayHeadAutoBackward(iPlayHead);
			pTrackView->setSyncViewHoldOn(false);
		}
		else
		// Auto-backward reset feature...
		if (m_ui.transportAutoBackwardAction->isChecked())
			m_pSession->setPlayHead(m_pSession->playHeadAutoBackward());
	}

	++m_iStabilizeTimer;
}


// Transport record.
void qtractorMainForm::transportRecord (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportRecord()");
#endif

	// Make sure session is activated...
	if (!checkRestartSession())
		return;

	// Don't hold on anymore...
	if (m_pTracks)
		m_pTracks->trackView()->setSyncViewHoldOn(false);

	// Toggle recording...
	const bool bRecording = !m_pSession->isRecording();
	if (setRecording(bRecording)) {
		// Send MMC RECORD_STROBE/EXIT command...
		m_pSession->midiEngine()->sendMmcCommand(bRecording ?
			qtractorMmcEvent::RECORD_STROBE : qtractorMmcEvent::RECORD_EXIT);
	}

	++m_iStabilizeTimer;
}


// Transport punch in/out.
void qtractorMainForm::transportPunch (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportPunch()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Do the punch in/out toggle switch...
	unsigned long iPunchIn  = 0;
	unsigned long iPunchOut = 0;

	if (!m_pSession->isPunching()) {
		iPunchIn  = m_pSession->editHead();
		iPunchOut = m_pSession->editTail();
	}

	// Now, express the change as an undoable command...
	m_pSession->execute(
		new qtractorSessionPunchCommand(m_pSession, iPunchIn, iPunchOut));
}


// Transport punch set.
void qtractorMainForm::transportPunchSet (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportPunchSet()");
#endif

	// Make sure session is activated?...
	//checkRestartSession();

	// Now, express the change as an undoable command...
	m_pSession->execute(
		new qtractorSessionPunchCommand(m_pSession,
			m_pSession->editHead(), m_pSession->editTail()));
}


// Count-in metronome transport option.
void qtractorMainForm::transportCountIn (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportCountIn()");
#endif

	// Toggle Audio count-in metronome...
	if (m_pOptions->bAudioMetronome) {
		qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
		if (pAudioEngine)
			pAudioEngine->setCountIn(!pAudioEngine->isCountIn());
	}

	// Toggle MIDI count-in metronome...
	if (m_pOptions->bMidiMetronome) {
		qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
		if (pMidiEngine)
			pMidiEngine->setCountIn(!pMidiEngine->isCountIn());
	}

	++m_iStabilizeTimer;
}


// Metronome transport option.
void qtractorMainForm::transportMetro (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportMetro()");
#endif

	// Toggle Audio metronome...
	if (m_pOptions->bAudioMetronome) {
		qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
		if (pAudioEngine)
			pAudioEngine->setMetronome(!pAudioEngine->isMetronome());
	}

	// Toggle MIDI metronome...
	if (m_pOptions->bMidiMetronome) {
		qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
		if (pMidiEngine)
			pMidiEngine->setMetronome(!pMidiEngine->isMetronome());
	}

	++m_iStabilizeTimer;
}


// Follow playhead transport option.
void qtractorMainForm::transportFollow (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportFollow()");
#endif

	// Can't hold on anymore...
	if (m_pTracks)
		m_pTracks->trackView()->setSyncViewHoldOn(false);

	// Toggle follow-playhead...
	++m_iStabilizeTimer;
}


// Auto-backward transport option.
void qtractorMainForm::transportAutoBackward (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportAutoBackward()");
#endif

	// Toggle auto-backward...
	++m_iStabilizeTimer;
}


// Set Transport mode option to None.
void qtractorMainForm::transportModeNone (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportModeNone()");
#endif

	// Set Transport mode to None...
	if (m_pOptions) {
		m_pOptions->iTransportMode = int(qtractorBus::None);
		updateTransportModePost();
	}
}


// Set Transport mode option to Slave.
void qtractorMainForm::transportModeSlave (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportModeSlave()");
#endif

	// Set Transport mode to Slave...
	if (m_pOptions) {
		m_pOptions->iTransportMode = int(qtractorBus::Input);
		updateTransportModePost();
	}
}


// Set Transport mode option to Master.
void qtractorMainForm::transportModeMaster (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportModeMaster()");
#endif

	// Set Transport mode to Master...
	if (m_pOptions) {
		m_pOptions->iTransportMode = int(qtractorBus::Output);
		updateTransportModePost();
	}

	++m_iStabilizeTimer;
}


// Set Transport mode option to Full.
void qtractorMainForm::transportModeFull (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportModeFull()");
#endif

	// Set Transport mode to Full...
	if (m_pOptions) {
		m_pOptions->iTransportMode = int(qtractorBus::Duplex);
		updateTransportModePost();
	}
}


// Continue past end transport option.
void qtractorMainForm::transportContinue (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportContinue()");
#endif

	// Toggle continue-past-end...
	++m_iStabilizeTimer;
}


// All tracks shut-off (panic).
void qtractorMainForm::transportPanic (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportPanic()");
#endif

	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	// All players must end now...
	if (pAudioEngine->isPlayerOpen() || pMidiEngine->isPlayerOpen()) {
		m_iPlayerTimer = 0;
		if (m_pFiles && m_pFiles->isPlayState())
			m_pFiles->setPlayState(false);
		if (m_pFileSystem && m_pFileSystem->isPlayState())
			m_pFileSystem->setPlayState(false);
		appendMessages(tr("Player panic!"));
		pAudioEngine->closePlayer();
		pMidiEngine->closePlayer();
	}

	// All (MIDI) tracks shut-off (panic)...
	pMidiEngine->shutOffAllTracks();

	// Reset XRUN counters...
	m_iXrunCount = 0;
	m_iXrunSkip  = 0;
	m_iXrunTimer = 0;

	++m_iStabilizeTimer;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Help Action slots.

// Show (and edit) keyboard shortcuts.
void qtractorMainForm::helpShortcuts (void)
{
	if (m_pOptions == nullptr)
		return;

	const QList<QAction *>& actions
		= findChildren<QAction *> (QString(), Qt::FindDirectChildrenOnly);
	qtractorShortcutForm shortcutForm(actions, this);
	shortcutForm.setActionControl(m_pActionControl);
	if (shortcutForm.exec()) {
		if (shortcutForm.isDirtyActionShortcuts())
			m_pOptions->saveActionShortcuts(this);
		if (shortcutForm.isDirtyActionControls())
			m_pOptions->saveActionControls(this);
	}
}


// Show information about application program.
void qtractorMainForm::helpAbout (void)
{
	QStringList list;
#ifdef CONFIG_DEBUG
	list << tr("Debugging option enabled.");
#endif
#ifndef CONFIG_LIBVORBIS
	list << tr("Ogg Vorbis (libvorbis) file support disabled.");
#endif
#ifndef CONFIG_LIBMAD
	list << tr("MPEG-1 Audio Layer 3 (libmad) file support disabled.");
#endif
#ifndef CONFIG_LIBSAMPLERATE
	list << tr("Sample-rate conversion (libsamplerate) disabled.");
#endif
#ifndef CONFIG_LIBRUBBERBAND
	list << tr("Pitch-shifting support (librubberband) disabled.");
#endif
#ifndef CONFIG_LIBAUBIO
	list << tr("Beat-detection support (libaubio) disabled.");
#endif
#ifndef CONFIG_LIBLO
	list << tr("OSC service support (liblo) disabled.");
#endif
#ifndef CONFIG_LADSPA
	list << tr("LADSPA Plug-in support disabled.");
#endif
#ifndef CONFIG_DSSI
	list << tr("DSSI Plug-in support disabled.");
#endif
#ifndef CONFIG_VST2
	list << tr("VST2 Plug-in support disabled.");
#endif
#ifndef CONFIG_VST3
	list << tr("VST3 Plug-in support disabled.");
#endif
#ifndef CONFIG_CLAP
	list << tr("CLAP Plug-in support disabled.");
#endif
#ifndef CONFIG_LV2
	list << tr("LV2 Plug-in support disabled.");
#else
#ifndef CONFIG_LIBLILV
	list << tr("LV2 Plug-in support (liblilv) disabled.");
#endif
#ifndef CONFIG_LV2_UI
	list << tr("LV2 Plug-in UI support disabled.");
#else
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#ifndef CONFIG_LIBSUIL
	list << tr("LV2 Plug-in UI support (libsuil) disabled.");
#endif
#endif
#ifndef CONFIG_LV2_EXTERNAL_UI
	list << tr("LV2 Plug-in External UI support disabled.");
#endif
#endif // CONFIG_LV2_UI
#ifdef  CONFIG_LV2_EVENT
	list << tr("LV2 Plug-in MIDI/Event support (DEPRECATED) enabled.");
#endif
#ifndef CONFIG_LV2_ATOM
	list << tr("LV2 Plug-in MIDI/Atom support disabled.");
#endif
#ifndef CONFIG_LV2_WORKER
	list << tr("LV2 Plug-in Worker/Schedule support disabled.");
#endif
#ifndef CONFIG_LV2_STATE
	list << tr("LV2 Plug-in State support disabled.");
#endif
#ifdef  CONFIG_LV2_STATE_FILES
#ifdef  CONFIG_LV2_STATE_MAKE_PATH
	list << tr("LV2 plug-in State Make Path support (DANGEROUS)	enabled.");
#endif
#else
	list << tr("LV2 Plug-in State Files support disabled.");
#endif // CONFIG_LV2_STATE_FILES
#ifndef CONFIG_LV2_PROGRAMS
	list << tr("LV2 Plug-in Programs support disabled.");
#endif
#ifndef CONFIG_LV2_MIDNAM
	list << tr("LV2 Plug-in MIDNAM support disabled.");
#endif
#ifndef CONFIG_LV2_PRESETS
	list << tr("LV2 Plug-in Presets support disabled.");
#endif
#ifndef CONFIG_LV2_PATCH
	list << tr("LV2 Plug-in Patch support disabled.");
#endif
#ifndef CONFIG_LV2_TIME
	list << tr("LV2 Plug-in Time/position support disabled.");
#endif
#ifndef CONFIG_LV2_OPTIONS
	list << tr("LV2 Plug-in Options support disabled.");
#endif
#ifndef CONFIG_LV2_BUF_SIZE
	list << tr("LV2 Plug-in Buf-size support disabled.");
#endif
#ifdef  CONFIG_LV2_UI
#ifndef CONFIG_LV2_UI_TOUCH
	list << tr("LV2 Plug-in UI Touch interface support disabled.");
#endif
#ifndef CONFIG_LV2_UI_REQ_VALUE
	list << tr("LV2 Plug-in UI Request-value support disabled.");
#endif
#ifndef CONFIG_LV2_UI_IDLE
	list << tr("LV2 Plug-in UI Idle interface support disabled.");
#endif
#ifndef CONFIG_LV2_UI_SHOW
	list << tr("LV2 Plug-in UI Show interface support disabled.");
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
#ifndef CONFIG_LV2_UI_GTK2
	list << tr("LV2 Plug-in UI GTK2 native support disabled.");
#endif
#ifndef CONFIG_LV2_UI_GTKMM2
	list << tr("LV2 Plug-in UI GTKMM2 native support disabled.");
#endif
#ifndef CONFIG_LV2_UI_X11
	list << tr("LV2 Plug-in UI X11 native support disabled.");
#endif
#endif
#endif // CONFIG_LV2_UI
#endif // CONFIG_LV2
#ifndef CONFIG_JACK_SESSION
	list << tr("JACK Session support disabled.");
#endif
#ifndef CONFIG_JACK_LATENCY
	list << tr("JACK Latency support disabled.");
#endif
#ifndef CONFIG_JACK_METADATA
	list << tr("JACK Metadata support disabled.");
#endif
#ifndef CONFIG_NSM
	list << tr("NSM support disabled.");
#endif

	// Stuff the about box text...
	QString sText = "<h1>" QTRACTOR_TITLE "</h1>\n";
	sText += "<p>" + tr(QTRACTOR_SUBTITLE) + "<br />\n";
	sText += "<br />\n";
	sText += tr("Version") + ": <b>" PROJECT_VERSION "</b><br />\n";
//	sText += "<small>" + tr("Build") + ": " CONFIG_BUILD_DATE "</small><br />\n";
	if (!list.isEmpty()) {
		sText += "<small><font color=\"red\">";
		sText += list.join("<br />\n");
		sText += "</font></small>\n";
	}
	sText += "<br />\n";
	sText += tr("Using: Qt %1").arg(qVersion());
#if defined(QT_STATIC)
	sText += "-static";
#endif
	sText += "<br />\n";
	sText += "<br />\n";
	sText += tr("Website") + ": <a href=\"" QTRACTOR_WEBSITE "\">" QTRACTOR_WEBSITE "</a><br />\n";
	sText += "<br />\n";
	sText += "<small>";
	sText += QTRACTOR_COPYRIGHT "<br />\n";
	sText += "<br />\n";
	sText += tr("This program is free software; you can redistribute it and/or modify it") + "<br />\n";
	sText += tr("under the terms of the GNU General Public License version 2 or later.");
	sText += "</small>";
	sText += "<br />\n";
	sText += "</p>\n";

	QMessageBox::about(this, tr("About"), sText);
}


// Show information about the Qt toolkit.
void qtractorMainForm::helpAboutQt (void)
{
	QMessageBox::aboutQt(this);
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Internal transport stabilization.

bool qtractorMainForm::setPlaying ( bool bPlaying )
{
	// Toggle engine play status...
	m_pSession->setPlaying(bPlaying);

	// We must start/stop certain things...
	if (bPlaying) {
		// In case of (re)starting playback, send now
		// all tracks MIDI bank select/program changes...
		m_pSession->resetAllMidiControllers(true);
		// Start something?...
		++m_iTransportUpdate;
	} else {
		// Shutdown recording anyway...
		if (m_pSession->isRecording() && setRecording(false)) {
			// Send MMC RECORD_EXIT command...
			m_pSession->midiEngine()->sendMmcCommand(
				qtractorMmcEvent::RECORD_EXIT);
			++m_iTransportUpdate;
		}
		// Stop transport rolling, immediately...
		setRolling(0);
		// Session tracks automation recording.
		qtractorCurveCaptureListCommand *pCurveCommand = nullptr;
		for (qtractorTrack *pTrack = m_pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			qtractorCurveList *pCurveList = pTrack->curveList();
			if (pCurveList && pCurveList->isCapture()
				&& !pCurveList->isEditListEmpty()) {
				if (pCurveCommand == nullptr)
					pCurveCommand = new qtractorCurveCaptureListCommand();
				pCurveCommand->addCurveList(pCurveList);
			}
		}
		if (pCurveCommand)
			m_pSession->commands()->push(pCurveCommand);
	}

	// Done with playback switch...
	return true;
}


bool qtractorMainForm::setPlayingEx ( bool bPlaying )
{
	if (!setPlaying(bPlaying))
		return false;

	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine) {
		// Avoid double update (owhile n fastTimerSlot...)
		m_iPlayHead = m_pSession->playHead();
		// Send MMC PLAY/STOP command...
		pMidiEngine->sendMmcCommand(bPlaying
			? qtractorMmcEvent::PLAY
			: qtractorMmcEvent::STOP);
		pMidiEngine->sendSppCommand(bPlaying
			? (m_iPlayHead > 0
				? SND_SEQ_EVENT_CONTINUE
				: SND_SEQ_EVENT_START)
			: SND_SEQ_EVENT_STOP);
		// Send MMC LOCATE and MIDI SPP commands on stop/pause...
		if (!bPlaying) {
			pMidiEngine->sendMmcLocate(
				m_pSession->locateFromFrame(m_iPlayHead));
			pMidiEngine->sendSppCommand(SND_SEQ_EVENT_SONGPOS,
				m_pSession->songPosFromFrame(m_iPlayHead));
		}
	}

	return true;
}


bool qtractorMainForm::setRecording ( bool bRecording )
{
	// Avoid if no tracks are armed...
	if (m_pSession->recordTracks() < 1)
		return false;

	if (bRecording) {
		// Starting recording: we must have a session name...
		if (m_pSession->sessionName().isEmpty() && !editSession())
			return false;
		// Will start recording...
	} else {
		// Stopping recording: fetch and commit
		// all new clips as a composite command...
		int iUpdate = 0;
		qtractorClipCommand *pClipCommand
			= new qtractorClipCommand(tr("record clip"));
		// For all non-empty clip on record...
		const unsigned long iFrameTime = m_pSession->frameTimeEx();
		for (qtractorTrack *pTrack = m_pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			if (pClipCommand->addClipRecord(pTrack, iFrameTime))
				++iUpdate;
		}
		// Put it in the form of an undoable command...
		if (iUpdate > 0) {
			m_pSession->execute(pClipCommand);
		} else {
			// The allocated command is unhelpful...
			delete pClipCommand;
			// Try to postpone an overall refresh...
			if (m_iAudioPeakTimer < 2) ++m_iAudioPeakTimer;
		}
	}

	// Finally, toggle session record status...
	m_pSession->setRecording(bRecording);

	// Also force some kind of a checkpoint,
	// next time, whenever applicable...
	if (m_iAutoSavePeriod > 0 && !bRecording)
		m_iAutoSaveTimer += m_iAutoSavePeriod;

	// Done with record switch...
	return true;
}


int qtractorMainForm::setRolling ( int iRolling )
{
	const int iOldRolling = m_iTransportRolling;

	// Avoid if recording is armed...
	if (m_pSession->isRecording() || iOldRolling == iRolling)
		iRolling = 0;

	// Set the rolling flag.
	m_iTransportRolling = iRolling;
	m_fTransportShuttle = float(iRolling);
	m_iTransportStep    = 0;

	// We've started/stopped something...
	if (m_iTransportRolling) {
		if (!m_bTransportPlaying)
			m_bTransportPlaying = m_pSession->isPlaying();
		if (m_bTransportPlaying)
			m_pSession->setPlaying(false);
		++m_iTransportUpdate;
	} else {
		if (m_bTransportPlaying)
			m_pSession->setPlaying(true);
		m_bTransportPlaying = false;
	}

	// Done with rolling switch...
	return iOldRolling;
}


void qtractorMainForm::setLocate ( unsigned int iLocate )
{
	m_pSession->setPlayHead(m_pSession->frameFromLocate(iLocate));
	++m_iTransportUpdate;
}


void qtractorMainForm::setShuttle ( float fShuttle )
{
	const float fOldShuttle = m_fTransportShuttle;

	if (fShuttle < 0.0f && fOldShuttle >= 0.0f)
		setRolling(-1);
	else
	if (fShuttle > 0.0f && 0.0f >= fOldShuttle)
		setRolling(+1);

	m_fTransportShuttle = fShuttle;
	++m_iTransportUpdate;
}


void qtractorMainForm::setStep ( int iStep )
{
	m_iTransportStep += iStep;
	++m_iTransportUpdate;
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
			case qtractorMmcEvent::TRACK_MONITOR:
				pTrack->setMonitor(bOn);
				break;
			default:
				break;
			}
			// Done.
			++m_iStabilizeTimer;
		}
	}
}


void qtractorMainForm::setSongPos ( unsigned int iSongPos )
{
	m_pSession->setPlayHead(m_pSession->frameFromSongPos(iSongPos));
	++m_iTransportUpdate;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Main window stabilization.

void qtractorMainForm::updateTransportTime ( unsigned long iPlayHead )
{
	m_pTimeSpinBox->setValue(iPlayHead, false);
	m_pThumbView->updatePlayHead(iPlayHead);

	// Update other editors thumb-views...
	QListIterator<qtractorMidiEditorForm *> iter(m_editors);
	while (iter.hasNext())
		iter.next()->updatePlayHead(iPlayHead);

	// Tricky stuff: node's non-null iif tempo changes...
	qtractorTimeScale::Node *pNode
		= m_pTempoCursor->seek(m_pSession->timeScale(), iPlayHead);
	if (pNode) {
		m_pTempoSpinBox->setTempo(pNode->tempo, false);
		m_pTempoSpinBox->setBeatsPerBar(pNode->beatsPerBar, false);
		m_pTempoSpinBox->setBeatDivisor(pNode->beatDivisor, false);
	}
}


void qtractorMainForm::stabilizeForm (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::stabilizeForm()");
#endif

	// Update the main application caption...
	QString sSessionName = sessionName(m_sFilename);
	if (m_iDirtyCount > 0)
		sSessionName += ' ' + tr("[modified]");
	setWindowTitle(sSessionName);

	// Update the main menu state...
	m_ui.fileSaveAction->setEnabled(m_iDirtyCount > 0);
#ifdef CONFIG_NSM
	const bool bNsmActive = (m_pNsmClient && m_pNsmClient->is_active());
	m_ui.fileNewAction->setEnabled(!bNsmActive);
	m_ui.fileSaveAsAction->setEnabled(!bNsmActive);
#endif

	const unsigned long iPlayHead = m_pSession->playHead();
	const unsigned long iSessionEnd = m_pSession->sessionEnd();

	const bool bTracks = (m_pTracks && m_pSession->tracks().count() > 0);
	qtractorTrack *pTrack = (bTracks ? m_pTracks->currentTrack() : nullptr);

	const bool bEnabled    = (pTrack != nullptr);
	const bool bSelected   = (m_pTracks && m_pTracks->isSelected())
		|| (m_pFiles && m_pFiles->hasFocus() && m_pFiles->isFileSelected());
	const bool bSelectable = (m_pSession->editHead() < m_pSession->editTail());
	const bool bClipboard  = qtractorTrackView::isClipboard();
	const bool bPlaying    = m_pSession->isPlaying();
	const bool bRecording  = m_pSession->isRecording();
	const bool bPunching   = m_pSession->isPunching();
	const bool bLooping    = m_pSession->isLooping();
	const bool bRolling    = (bPlaying && bRecording);
	const bool bBumped     = (!bRolling && (iPlayHead > 0 || bPlaying));

	// Update edit menu state...
	if (bRolling) {
		m_ui.editUndoAction->setEnabled(false);
		m_ui.editRedoAction->setEnabled(false);
	} else {
		qtractorCommandList *pCommands = m_pSession->commands();
		pCommands->updateAction(m_ui.editUndoAction, pCommands->lastCommand());
		pCommands->updateAction(m_ui.editRedoAction, pCommands->nextCommand());
	}

//	m_ui.editCutAction->setEnabled(bSelected);
//	m_ui.editCopyAction->setEnabled(bSelected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	const QMimeData *pMimeData
		= QApplication::clipboard()->mimeData();
	m_ui.editPasteAction->setEnabled(bClipboard
		|| (pMimeData && pMimeData->hasUrls()));
#else
	m_ui.editPasteAction->setEnabled(bClipboard);
#endif
	m_ui.editPasteRepeatAction->setEnabled(bClipboard);
//	m_ui.editDeleteAction->setEnabled(bSelected);

	m_ui.editSelectAllAction->setEnabled(iSessionEnd > 0);
	m_ui.editSelectInvertAction->setEnabled(iSessionEnd > 0);
	m_ui.editSelectTrackRangeAction->setEnabled(bEnabled && bSelectable);
	m_ui.editSelectTrackAction->setEnabled(bEnabled);
	m_ui.editSelectRangeAction->setEnabled(iSessionEnd > 0 && bSelectable);
	m_ui.editSelectNoneAction->setEnabled(bSelected);

	const bool bInsertable = m_pSession->editHead() < iSessionEnd;
	m_ui.editInsertTrackRangeAction->setEnabled(bEnabled && bInsertable);
	m_ui.editInsertRangeAction->setEnabled(bInsertable);
	m_ui.editRemoveTrackRangeAction->setEnabled(bEnabled && bInsertable);
	m_ui.editRemoveRangeAction->setEnabled(bInsertable);

	m_ui.editSplitAction->setEnabled(bSelected);

	// Top-level menu/toolbar items stabilization...
	updateTrackMenu();
	updateClipMenu();

	// Update view menu state...
	m_ui.viewFileSystemAction->setChecked(
		m_pFileSystem && m_pFileSystem->isVisible());
	m_ui.viewFilesAction->setChecked(
		m_pFiles && m_pFiles->isVisible());
	m_ui.viewMessagesAction->setChecked(
		m_pMessages && m_pMessages->isVisible());
	m_ui.viewConnectionsAction->setChecked(
		m_pConnections && m_pConnections->isVisible());
	m_ui.viewMixerAction->setChecked(
		m_pMixer && m_pMixer->isVisible());

	// Recent files menu.
	m_ui.fileOpenRecentMenu->setEnabled(m_pOptions->recentFiles.count() > 0);

	// Always make the latest message visible.
	if (m_pMessages)
		m_pMessages->flushStdoutBuffer();

	// Session status...
	updateTransportTime(iPlayHead);

	if (pTrack)
		m_statusItems[StatusName]->setText(pTrack->trackName().simplified());
	else
		m_statusItems[StatusName]->clear();

	if (m_iDirtyCount > 0)
		m_statusItems[StatusMod]->setText(tr("MOD"));
	else
		m_statusItems[StatusMod]->clear();

	if (bRecording && m_pSession->recordTracks() > 0)
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

	if (m_iXrunCount > 0)
		m_statusItems[StatusXrun]->setText(tr("XRUN"));
	else
		m_statusItems[StatusXrun]->clear();

	m_statusItems[StatusTime]->setText(
		m_pSession->timeScale()->textFromFrame(0, true, iSessionEnd));

	m_statusItems[StatusSize]->setText(
		QString::number(m_pSession->audioEngine()->bufferSize()));

	m_statusItems[StatusRate]->setText(
		QString::number(m_pSession->sampleRate()));

	m_statusItems[StatusRec]->setPalette(*m_paletteItems[
		bRecording && bRolling ? PaletteRed : PaletteNone]);
	m_statusItems[StatusMute]->setPalette(*m_paletteItems[
		m_pSession->muteTracks() > 0 ? PaletteYellow : PaletteNone]);
	m_statusItems[StatusSolo]->setPalette(*m_paletteItems[
		m_pSession->soloTracks() > 0 ? PaletteCyan : PaletteNone]);
	m_statusItems[StatusLoop]->setPalette(*m_paletteItems[
		bLooping ? PaletteGreen : PaletteNone]);

	m_statusItems[StatusXrun]->setPalette(*m_paletteItems[
		m_iXrunCount > 0 ? PaletteRed : PaletteNone]);

	// Transport stuff...
	m_ui.transportBackwardAction->setEnabled(bBumped);
	m_ui.transportRewindAction->setEnabled(bBumped);
	m_ui.transportFastForwardAction->setEnabled(!bRolling);
	m_ui.transportForwardAction->setEnabled(
		!bRolling && (iPlayHead < iSessionEnd
			|| iPlayHead < m_pSession->editHead()
			|| iPlayHead < m_pSession->editTail()));
	m_ui.transportStepBackwardAction->setEnabled(bBumped);
	m_ui.transportStepForwardAction->setEnabled(!bRolling);
	m_ui.transportLoopAction->setEnabled(
		!bRolling && (bLooping || bSelectable));
	m_ui.transportLoopSetAction->setEnabled(
		!bRolling && bSelectable);
	m_ui.transportStopAction->setEnabled(bPlaying);
	m_ui.transportRecordAction->setEnabled(m_pSession->recordTracks() > 0);
	m_ui.transportPunchAction->setEnabled(bPunching || bSelectable);
	m_ui.transportPunchSetAction->setEnabled(bSelectable);
	m_ui.transportCountInAction->setEnabled(
		(m_pOptions->bAudioMetronome
			&& int(m_pSession->audioEngine()->countInMode()) > 0) ||
		(m_pOptions->bMidiMetronome
			&& int(m_pSession->midiEngine()->countInMode()) > 0));
	m_ui.transportMetroAction->setEnabled(
		m_pOptions->bAudioMetronome || m_pOptions->bMidiMetronome);
	m_ui.transportPanicAction->setEnabled(bTracks
		|| (m_pFiles && m_pFiles->isPlayState())
		|| (m_pFileSystem && m_pFileSystem->isPlayState()));

	m_ui.transportRewindAction->setChecked(m_iTransportRolling < 0);
	m_ui.transportFastForwardAction->setChecked(m_iTransportRolling > 0);
	m_ui.transportLoopAction->setChecked(bLooping);
	m_ui.transportPlayAction->setChecked(bPlaying);
	m_ui.transportRecordAction->setChecked(bRecording);
	m_ui.transportPunchAction->setChecked(bPunching);

	// Special record mode settlement.
	m_pTimeSpinBox->setReadOnly(bRecording);
	m_pTempoSpinBox->setReadOnly(bRecording);

	// Stabilize thumb-view...
	m_pThumbView->update();
	m_pThumbView->updateThumb();

	// Update editors too...
	QListIterator<qtractorMidiEditorForm *> iter(m_editors);
	while (iter.hasNext())
		iter.next()->stabilizeForm();
}


// Actually start all session engines.
bool qtractorMainForm::startSession (void)
{
	m_iTransportUpdate  = 0; 
	m_iTransportRolling = 0;
	m_bTransportPlaying = false;
	m_fTransportShuttle = 0.0f;
	m_iTransportStep    = 0;

	m_iXrunCount = 0;
	m_iXrunSkip  = 0;
	m_iXrunTimer = 0;

	m_iAudioRefreshTimer = 0;
	m_iMidiRefreshTimer  = 0;

	m_iPlayerTimer = 0;

	m_iAudioPropertyChange = 0;

	m_iStabilizeTimer = 0;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	const bool bResult = m_pSession->init();
	QApplication::restoreOverrideCursor();

	autoSaveReset();

	if (bResult) {
		// Get on with the special ALSA sequencer notifier...
		qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
		if (pMidiEngine && pMidiEngine->alsaNotifier()) {
			QObject::connect(pMidiEngine->alsaNotifier(),
				SIGNAL(activated(int)),
				SLOT(alsaNotify()));
		}
		// Game on...
		appendMessages(tr("Session started."));
	} else {
		// Uh-oh, we can't go on like this...
		appendMessagesError(
			tr("The audio/MIDI engine could not be started.\n\n"
			"Make sure the JACK/Pipewire audio service and\n"
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
		const unsigned long iPlayHead = m_pSession->playHead();
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		m_pSession->close();
		QApplication::restoreOverrideCursor();
		// Bail out if can't start it...
		if (!startSession()) {
			// Can go on with no-business...
			++m_iStabilizeTimer;
			return false;
		}
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		m_pSession->open();
		QApplication::restoreOverrideCursor();
		// Restore previous playhead position...
		m_pSession->setPlayHead(iPlayHead);
		// Done restart.
	} else {
		// Reset XRUN counters...
		m_iXrunCount = 0;
		m_iXrunSkip  = 0;
		m_iXrunTimer = 0;
		// HACK: When applicable, make sure we're
		// always the (JACK) Timebase master...
		if (m_pSession->audioEngine())
			m_pSession->audioEngine()->resetTimebase();
		// Done checking.
	}

	return true;
}


// Prepare session start.
void qtractorMainForm::updateSessionPre (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::updateSessionPre()");
#endif

	//  Actually (re)start session engines, no matter what...
	startSession();
#if 0
	// (Re)set playhead...
	if (m_ui.transportAutoBackwardAction->isChecked())
		m_pSession->setPlayHead(playHeadBackward());
#endif
	// Start collection of nested messages...
	qtractorMessageList::clear();
}


// Finalize session start.
void qtractorMainForm::updateSessionPost (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::updateSessionPost()");
#endif

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	m_pSession->open();
	QApplication::restoreOverrideCursor();

	// HACK: Special treatment for disparate sample rates,
	// and only for (just loaded) non empty sessions...
	const unsigned int iSampleRate
		= m_pSession->audioEngine()->sampleRate();

	if (m_pSession->sampleRate() != iSampleRate) {
		appendMessagesError(
			tr("The original session sample rate (%1 Hz)\n"
			"is not the same as the current audio engine (%2 Hz).\n\n"
			"Saving and reloading from a new session file\n"
			"is highly recommended.")
			.arg(m_pSession->sampleRate())
			.arg(iSampleRate));
		// We'll doing the conversion right here and right now...
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		m_pSession->updateSampleRate(iSampleRate);
		QApplication::restoreOverrideCursor();
		// Prompt for a brand new filename (ie. Save As...)
		// whenever session Save is invoked next time.
		m_sFilename.clear();
		updateDirtyCount(true);
	}

	// We're definitely clean...
	qtractorSubject::resetQueue();

	// Update the session views...
	viewRefresh();

	// Sync all process-enabled automation curves...
	m_pSession->process_curve(m_iPlayHead);

	// Check for any pending nested messages...
	if (!qtractorMessageList::isEmpty()) {
		if (QMessageBox::warning(this,
			tr("Warning"),
			tr("The following issues were detected:\n\n%1\n\n"
			"Saving into another session file is highly recommended.")
			.arg(qtractorMessageList::items().join("\n")),
			QMessageBox::Save | QMessageBox::Ignore) == QMessageBox::Save) {
			saveSession(true);
		} else {
			// Prompt for a brand new filename (ie. Save As...)
			// whenever session Save is invoked next time.
			m_sFilename.clear();
			updateDirtyCount(true);
		}
		qtractorMessageList::clear();
	}

	// Reset/reset all MIDI controllers (conditional)...
	m_pSession->resetAllMidiControllers(false);

	// Ah, make it stand right.
	if (m_pTracks)
		m_pTracks->trackView()->setFocus();

	// Of course!...
	++m_iTransportUpdate;
}


// Update the track export menu.
void qtractorMainForm::updateExportMenu (void)
{
	// Special export enablement...
	int iAudioClips = 0;
	int iMidiClips = 0;
	
	if (!m_pSession->isPlaying()) {
		for (qtractorTrack *pTrack = m_pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			const int iClips = pTrack->clips().count();
			switch (pTrack->trackType()) {
			case qtractorTrack::Audio:
				iAudioClips += iClips;
				break;
			case qtractorTrack::Midi:
				iMidiClips += iClips;
				// Fall thru...
			default:
				break;
			}
		}
	}

	// nb. audio export also applies to MIDI instrument tracks...
	m_ui.trackExportAudioAction->setEnabled(iAudioClips > 0 || iMidiClips > 0);
	m_ui.trackExportMidiAction->setEnabled(iMidiClips > 0);
}


// Update the recent files list and menu.
void qtractorMainForm::updateRecentFiles ( const QString& sFilename )
{
	if (m_pOptions == nullptr)
		return;

	// Remove from list if already there (avoid duplicates)
	const int iIndex = m_pOptions->recentFiles.indexOf(sFilename);
	if (iIndex >= 0)
		m_pOptions->recentFiles.removeAt(iIndex);
	// Put it to front...
	m_pOptions->recentFiles.push_front(sFilename);
}


// Update the recent files list and menu.
void qtractorMainForm::updateRecentFilesMenu (void)
{
	if (m_pOptions == nullptr)
		return;

	// Time to keep the list under limits.
	int iRecentFiles = m_pOptions->recentFiles.count();
	while (iRecentFiles > m_pOptions->iMaxRecentFiles) {
		m_pOptions->recentFiles.pop_back();
		--iRecentFiles;
	}

	// Rebuild the recent files menu...
	m_ui.fileOpenRecentMenu->clear();
	for (int i = 0; i < iRecentFiles; ++i) {
		const QString& sFilename = m_pOptions->recentFiles.at(i);
		if (QFileInfo(sFilename).exists()) {
			QAction *pAction = m_ui.fileOpenRecentMenu->addAction(
				QString("&%1 %2").arg(i + 1).arg(sessionName(sFilename)),
				this, SLOT(fileOpenRecent()));
			pAction->setData(i);
		}
	}

	// Settle as enabled?
	m_ui.fileOpenRecentMenu->setEnabled(!m_ui.fileOpenRecentMenu->isEmpty());
}


// Force update of the peak-files auto-remove mode.
void qtractorMainForm::updatePeakAutoRemove (void)
{
	if (m_pOptions == nullptr)
		return;

	qtractorAudioPeakFactory *pPeakFactory
		= m_pSession->audioPeakFactory();
	if (pPeakFactory)
		pPeakFactory->setAutoRemove(m_pOptions->bPeakAutoRemove);
}


// Update main transport-time display format.
void qtractorMainForm::updateDisplayFormat (void)
{
	if (m_pOptions == nullptr)
		return;

	// Main transport display format is due...
	const qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(m_pOptions->iDisplayFormat);

	(m_pSession->timeScale())->setDisplayFormat(displayFormat);
	m_pTimeSpinBox->setDisplayFormat(displayFormat);
}


// Update audio player parameters.
void qtractorMainForm::updateAudioPlayer (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the Audio engine player handling...
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	pAudioEngine->setPlayerAutoConnect(m_pOptions->bAudioPlayerAutoConnect);
	pAudioEngine->setPlayerBus(m_pOptions->bAudioPlayerBus);
}


// Update Audio engine control mode settings.
void qtractorMainForm::updateTransportModePre (void)
{
	if (m_pOptions == nullptr)
		return;

	switch (qtractorBus::BusMode(m_pOptions->iTransportMode)) {
	case qtractorBus::None:   // None
		m_ui.transportModeNoneAction->setChecked(true);
		break;
	case qtractorBus::Input:  // Slave
		m_ui.transportModeSlaveAction->setChecked(true);
		break;
	case qtractorBus::Output: // Master
		m_ui.transportModeMasterAction->setChecked(true);
		break;
	case qtractorBus::Duplex: // Full
	default:
		m_ui.transportModeFullAction->setChecked(true);
		break;
	}

	// Set initial transport mode...
	m_pTransportModeToolButton->setDefaultAction(
		m_pTransportModeActionGroup->checkedAction());
}


void qtractorMainForm::updateTransportModePost (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the Audio engine handling...
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	pAudioEngine->setTransportMode(
		qtractorBus::BusMode(m_pOptions->iTransportMode));
}


// Update JACK Timebase master mode.
void qtractorMainForm::updateTimebase (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the Audio engine handling...
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	pAudioEngine->setTimebase(m_pOptions->bTimebase);
	pAudioEngine->resetTimebase();
}


// Update MIDI engine control mode settings.
void qtractorMainForm::updateMidiControlModes (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the MIDI engine handling...
	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	pMidiEngine->setCaptureQuantize(
		qtractorTimeScale::snapFromIndex(m_pOptions->iMidiCaptureQuantize));
	pMidiEngine->setMmcDevice(m_pOptions->iMidiMmcDevice);
	pMidiEngine->setMmcMode(qtractorBus::BusMode(m_pOptions->iMidiMmcMode));
	pMidiEngine->setSppMode(qtractorBus::BusMode(m_pOptions->iMidiSppMode));
	pMidiEngine->setClockMode(qtractorBus::BusMode(m_pOptions->iMidiClockMode));
	pMidiEngine->setResetAllControllers(m_pOptions->bMidiResetAllControllers);
}


// Update MIDI playback queue timersetting.
void qtractorMainForm::updateMidiQueueTimer (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the MIDI engine queue timer...
	m_pSession->midiEngine()->setAlsaTimer(m_pOptions->iMidiQueueTimer);
}


// Update MIDI playback drift correction.
void qtractorMainForm::updateMidiDriftCorrect (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the MIDI engine drift correction...
	m_pSession->midiEngine()->setDriftCorrect(m_pOptions->bMidiDriftCorrect);
}


// Update MIDI player parameters.
void qtractorMainForm::updateMidiPlayer (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the MIDI engine player handling...
	m_pSession->midiEngine()->setPlayerBus(m_pOptions->bMidiPlayerBus);
}


// Update MIDI control parameters.
void qtractorMainForm::updateMidiControl (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the MIDI engine control handling...
	m_pSession->midiEngine()->setControlBus(m_pOptions->bMidiControlBus);
}


// Update audio metronome parameters.
void qtractorMainForm::updateAudioMetronome (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the Audio engine metronome handling...
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	pAudioEngine->setMetroBarFilename(m_pOptions->sMetroBarFilename);
	pAudioEngine->setMetroBarGain(m_pOptions->fMetroBarGain);

	pAudioEngine->setMetroBeatFilename(m_pOptions->sMetroBeatFilename);
	pAudioEngine->setMetroBeatGain(m_pOptions->fMetroBeatGain);

	pAudioEngine->setMetroOffset(m_pOptions->iAudioMetroOffset);

	const bool bAudioMetronome = m_pOptions->bAudioMetronome;
	pAudioEngine->setMetroAutoConnect(m_pOptions->bAudioMetroAutoConnect);
	pAudioEngine->setMetroEnabled(bAudioMetronome);
	pAudioEngine->setMetroBus(
		bAudioMetronome && m_pOptions->bAudioMetroBus);
	pAudioEngine->setMetronome(
		bAudioMetronome && m_ui.transportMetroAction->isChecked());

	pAudioEngine->setCountInMode(
		qtractorAudioEngine::CountInMode(m_pOptions->iAudioCountInMode));
	pAudioEngine->setCountInBeats(m_pOptions->iAudioCountInBeats);
	pAudioEngine->setCountIn(m_pOptions->iAudioCountInMode > 0 &&
		bAudioMetronome && m_ui.transportCountInAction->isChecked());
}


// Update MIDI metronome parameters.
void qtractorMainForm::updateMidiMetronome (void)
{
	if (m_pOptions == nullptr)
		return;

	// Configure the MIDI engine metronome handling...
	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	pMidiEngine->setMetroChannel(m_pOptions->iMetroChannel);
	pMidiEngine->setMetroBar(
		m_pOptions->iMetroBarNote,
		m_pOptions->iMetroBarVelocity,
		m_pOptions->iMetroBarDuration);
	pMidiEngine->setMetroBeat(
		m_pOptions->iMetroBeatNote,
		m_pOptions->iMetroBeatVelocity,
		m_pOptions->iMetroBeatDuration);

	pMidiEngine->setMetroOffset(m_pOptions->iMidiMetroOffset);

	const bool bMidiMetronome = m_pOptions->bMidiMetronome;
	pMidiEngine->setMetroEnabled(bMidiMetronome);
	pMidiEngine->setMetroBus(
		bMidiMetronome && m_pOptions->bMidiMetroBus);
	pMidiEngine->setMetronome(
		bMidiMetronome && m_ui.transportMetroAction->isChecked());

	pMidiEngine->setCountInMode(
		qtractorMidiEngine::CountInMode(m_pOptions->iMidiCountInMode));
	pMidiEngine->setCountInBeats(m_pOptions->iMidiCountInBeats);
	pMidiEngine->setCountIn(m_pOptions->iMidiCountInMode > 0 &&
		bMidiMetronome && m_ui.transportCountInAction->isChecked());
}


// Update transport display options.
void qtractorMainForm::updateSyncViewHold (void)
{
	if (m_pOptions == nullptr)
		return;

	if (m_pTracks)
		m_pTracks->trackView()->setSyncViewHold(m_pOptions->bSyncViewHold);

	// Update editors ...
	QListIterator<qtractorMidiEditorForm *> iter(m_editors);
	while (iter.hasNext())
		(iter.next()->editor())->setSyncViewHold(m_pOptions->bSyncViewHold);
}


// Track menu stabilizer.
void qtractorMainForm::updateTrackMenu (void)
{
	const bool bTracks = (m_pTracks && m_pSession->tracks().count() > 0);
	qtractorTrack *pTrack = (bTracks ? m_pTracks->currentTrack() : nullptr);

	const bool bEnabled   = (pTrack != nullptr);
	const bool bPlaying   = m_pSession->isPlaying();
	const bool bRecording = m_pSession->isRecording();
	const bool bRolling   = (bPlaying && bRecording);

	// Update track menu state...
	m_ui.trackRemoveAction->setEnabled(
		bEnabled && (!bRolling || !pTrack->isRecord()));
	m_ui.trackDuplicateAction->setEnabled(bEnabled);
	m_ui.trackPropertiesAction->setEnabled(
		bEnabled && (!bRolling || !pTrack->isRecord()));
	m_ui.trackInputsAction->setEnabled(
		bEnabled && pTrack->inputBus() != nullptr);
	m_ui.trackOutputsAction->setEnabled(
		bEnabled && pTrack->outputBus() != nullptr);
	m_ui.trackStateMenu->setEnabled(bEnabled);
	m_ui.trackNavigateMenu->setEnabled(bTracks);
	m_ui.trackNavigateFirstAction->setEnabled(bTracks);
	m_ui.trackNavigatePrevAction->setEnabled(bTracks);
	m_ui.trackNavigateNextAction->setEnabled(bTracks);
	m_ui.trackNavigateLastAction->setEnabled(bTracks);
	m_ui.trackNavigateNoneAction->setEnabled(bEnabled);
	m_ui.trackMoveMenu->setEnabled(bEnabled);
	m_ui.trackMoveTopAction->setEnabled(bEnabled && pTrack->prev() != nullptr);
	m_ui.trackMoveUpAction->setEnabled(bEnabled && pTrack->prev() != nullptr);
	m_ui.trackMoveDownAction->setEnabled(bEnabled && pTrack->next() != nullptr);
	m_ui.trackMoveBottomAction->setEnabled(bEnabled && pTrack->next() != nullptr);
	m_ui.trackHeightMenu->setEnabled(bEnabled);
	m_ui.trackCurveMenu->setEnabled(bEnabled);
//	m_ui.trackImportAudioAction->setEnabled(m_pTracks != nullptr);
//	m_ui.trackImportMidiAction->setEnabled(m_pTracks != nullptr);
//	m_ui.trackAutoMonitorAction->setEnabled(m_pTracks != nullptr);
	m_ui.trackInstrumentMenu->setEnabled(
		bEnabled && pTrack->trackType() == qtractorTrack::Midi);

	// Update track menu state...
	if (bEnabled) {
		m_ui.trackStateRecordAction->setChecked(pTrack->isRecord());
		m_ui.trackStateMuteAction->setChecked(pTrack->isMute());
		m_ui.trackStateSoloAction->setChecked(pTrack->isSolo());
		m_ui.trackStateMonitorAction->setChecked(pTrack->isMonitor());
	}
}


// Curve/automation menu stabilizer.
void qtractorMainForm::updateCurveMenu (void)
{
	qtractorTrack *pTrack
		= (m_pTracks ? m_pTracks->currentTrack(): nullptr);
	qtractorCurveList *pCurveList
		= (pTrack ? pTrack->curveList() : nullptr);
	qtractorCurve *pCurrentCurve
		= (pCurveList ? pCurveList->currentCurve() : nullptr);

	bool bEnabled = trackCurveSelectMenuReset(m_ui.trackCurveSelectMenu);
	m_ui.trackCurveMenu->setEnabled(bEnabled);
	m_ui.trackCurveSelectMenu->setEnabled(bEnabled);

	if (bEnabled)
		bEnabled = (pCurveList && !pCurveList->isEmpty());

	const bool bCurveEnabled = bEnabled && pCurrentCurve != nullptr;

	m_ui.trackCurveModeMenu->setEnabled(bCurveEnabled);

	m_ui.trackCurveLockedAction->setEnabled(bCurveEnabled);
	m_ui.trackCurveLockedAction->setChecked(
		pCurrentCurve && pCurrentCurve->isLocked());

	m_ui.trackCurveProcessAction->setEnabled(bCurveEnabled);
	m_ui.trackCurveProcessAction->setChecked(
		pCurrentCurve && pCurrentCurve->isProcess());

	m_ui.trackCurveCaptureAction->setEnabled(bCurveEnabled);
	m_ui.trackCurveCaptureAction->setChecked(
		pCurrentCurve && pCurrentCurve->isCapture());

	m_ui.trackCurveClearAction->setEnabled(bCurveEnabled);

	m_ui.trackCurveLockedAllAction->setEnabled(bEnabled);
	m_ui.trackCurveLockedAllAction->setChecked(
		pCurveList && pCurveList->isLockedAll());
	m_ui.trackCurveLockedAllAction->setEnabled(bEnabled);

	m_ui.trackCurveProcessAllAction->setEnabled(bEnabled);
	m_ui.trackCurveProcessAllAction->setChecked(
		pCurveList && pCurveList->isProcessAll());
	m_ui.trackCurveProcessAllAction->setEnabled(bEnabled);

	m_ui.trackCurveCaptureAllAction->setEnabled(bEnabled);
	m_ui.trackCurveCaptureAllAction->setChecked(
		pCurveList && pCurveList->isCaptureAll());
	m_ui.trackCurveCaptureAllAction->setEnabled(bEnabled);

	m_ui.trackCurveClearAllAction->setEnabled(
		pCurveList && !pCurveList->isEmpty());
}


// Curve/automation mode menu stabilizer.
void qtractorMainForm::updateCurveModeMenu (void)
{
	trackCurveModeMenuReset(m_ui.trackCurveModeMenu);
}


// Track curve/automation select item builder.
void qtractorMainForm::trackCurveSelectMenuAction ( QMenu *pMenu,
	qtractorMidiControlObserver *pObserver, qtractorSubject *pCurrentSubject ) const
{
	QIcon   icon(QIcon::fromTheme("trackCurveNone"));
	QString text(tr("&None"));

	qtractorSubject *pSubject = (pObserver ? pObserver->subject() : nullptr);
	if (pSubject) {
		text = pSubject->name();
		qtractorCurve *pCurve = pSubject->curve();
		if (pCurve) {
			if (pCurve->isCapture())
				icon = QIcon::fromTheme("trackCurveCapture");
			else
			if (pCurve->isProcess())
				icon = QIcon::fromTheme("trackCurveProcess");
			else
		//	if (pCurve->isEnabled())
				icon = QIcon::fromTheme("trackCurveEnabled");
			text += '*';
		}
	}

	QAction *pAction = pMenu->addAction(icon, text);
	pAction->setCheckable(true);
	pAction->setChecked(pCurrentSubject == pSubject);
	pAction->setData(QVariant::fromValue(pObserver));
}


// Track curve/automation select menu builder.
bool qtractorMainForm::trackCurveSelectMenuReset ( QMenu *pMenu ) const
{
	pMenu->clear();

	if (m_pTracks == nullptr)
		return false;

	qtractorTrack *pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return false;

	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList == nullptr)
		return false;

	qtractorMonitor *pMonitor = pTrack->monitor();
	if (pMonitor == nullptr)
		return false;

	qtractorCurve *pCurrentCurve
	   = pCurveList->currentCurve();
	qtractorSubject *pCurrentSubject
	   = (pCurrentCurve ? pCurrentCurve->subject() : nullptr);

	trackCurveSelectMenuAction(pMenu,
		pTrack->monitorObserver(), pCurrentSubject);
	trackCurveSelectMenuAction(pMenu,
		pMonitor->panningObserver(), pCurrentSubject);
	trackCurveSelectMenuAction(pMenu,
		pMonitor->gainObserver(), pCurrentSubject);

	pMenu->addSeparator();

	trackCurveSelectMenuAction(pMenu,
		pTrack->recordObserver(), pCurrentSubject);
	trackCurveSelectMenuAction(pMenu,
		pTrack->muteObserver(), pCurrentSubject);
	trackCurveSelectMenuAction(pMenu,
		pTrack->soloObserver(), pCurrentSubject);

	qtractorPluginList *pPluginList = pTrack->pluginList();
	if (pPluginList->count() > 0) {
		pMenu->addSeparator();
		qtractorPlugin *pPlugin = pPluginList->first();
		while (pPlugin) {
			QMenu *pPluginMenu = pMenu->addMenu(pPlugin->title());
			trackCurveSelectMenuAction(pPluginMenu,
				pPlugin->activateObserver(), pCurrentSubject);
			const qtractorPlugin::Params& params = pPlugin->params();
			if (params.count() > 0) {
				pPluginMenu->addSeparator();
				qtractorPlugin::Params::ConstIterator param
					= params.constBegin();
				const qtractorPlugin::Params::ConstIterator& param_end
					= params.constEnd();
				for ( ; param != param_end; ++param) {
					trackCurveSelectMenuAction(pPluginMenu,
						param.value()->observer(), pCurrentSubject);
				}
			}
			const qtractorPlugin::PropertyKeys& props
				= pPlugin->propertyKeys();
			if (props.count() > 0) {
				pPluginMenu->addSeparator();
				qtractorPlugin::PropertyKeys::ConstIterator prop
					= props.constBegin();
				const qtractorPlugin::PropertyKeys::ConstIterator& prop_end
					= props.constEnd();
				for ( ; prop != prop_end; ++prop) {
					qtractorPlugin::Property *pProp = prop.value();
					if (pProp->isAutomatable()) {
						trackCurveSelectMenuAction(pPluginMenu,
							pProp->observer(), pCurrentSubject);
					}
				}
			}
			pPlugin = pPlugin->next();
		}
	}

	pMenu->addSeparator();

	trackCurveSelectMenuAction(pMenu, nullptr, pCurrentSubject);
	
	return true;
}


bool qtractorMainForm::trackCurveModeMenuReset ( QMenu *pMenu ) const
{
	pMenu->clear();

	qtractorTrack *pTrack = m_pTracks->currentTrack();
	if (pTrack == nullptr)
		return false;

	qtractorCurve *pCurrentCurve = pTrack->currentCurve();
	if (pCurrentCurve == nullptr)
		return false;

	const qtractorCurve::Mode mode = pCurrentCurve->mode();
	const bool bDecimal = (pCurrentCurve->subject())->isDecimal();

	QAction *pAction;

	pAction = pMenu->addAction(tr("&Hold"));
	pAction->setCheckable(true);
	pAction->setChecked(mode == qtractorCurve::Hold);
	pAction->setData(int(qtractorCurve::Hold));

	pAction = pMenu->addAction(tr("&Linear"));
	pAction->setCheckable(true);
	pAction->setChecked(mode == qtractorCurve::Linear);
	pAction->setData(int(qtractorCurve::Linear));
	pAction->setEnabled(bDecimal);

	pAction = pMenu->addAction(tr("&Spline"));
	pAction->setCheckable(true);
	pAction->setChecked(mode == qtractorCurve::Spline);
	pAction->setData(int(qtractorCurve::Spline));
	pAction->setEnabled(bDecimal);

	pMenu->addSeparator();

	pMenu->addAction(m_ui.trackCurveLogarithmicAction);
	m_ui.trackCurveLogarithmicAction->setChecked(
		pCurrentCurve && pCurrentCurve->isLogarithmic());
	m_ui.trackCurveLogarithmicAction->setData(-1);
	m_ui.trackCurveLogarithmicAction->setEnabled(bDecimal);

	pMenu->addAction(m_ui.trackCurveColorAction);
	m_ui.trackCurveColorAction->setData(-1);

	return true;
}


// Clip menu stabilizer.
void qtractorMainForm::updateClipMenu (void)
{
	const unsigned long iPlayHead = m_pSession->playHead();

	qtractorClip  *pClip  = nullptr;
	qtractorTrack *pTrack = nullptr;
	const bool bTracks = (m_pTracks && m_pSession->tracks().count() > 0);
	if (bTracks) {
		pClip  = m_pTracks->currentClip();
		pTrack = (pClip ? pClip->track() : m_pTracks->currentTrack());
	}

	const bool bEnabled = (pTrack != nullptr);
	const bool bSelected = (m_pTracks && m_pTracks->isSelected());
	const bool bClipSelected = (pClip != nullptr)
		|| (m_pTracks && m_pTracks->isClipSelected());
	const bool bClipSelectable = bClipSelected
		&& (m_pSession->editHead() < m_pSession->editTail());
	const bool bSingleTrackSelected = bClipSelected
		&& (pTrack && m_pTracks->singleTrackSelected() == pTrack);

	m_ui.editCutAction->setEnabled(bSelected);
	m_ui.editCopyAction->setEnabled(bSelected);
	m_ui.editDeleteAction->setEnabled(bSelected);

	m_ui.clipNewAction->setEnabled(bEnabled);
	m_ui.clipEditAction->setEnabled(pClip != nullptr);

	m_ui.clipMuteAction->setEnabled(bClipSelected);
	m_ui.clipMuteAction->setChecked(pClip && pClip->isClipMute());

	// Special unlink (MIDI) clip...
	qtractorMidiClip *pMidiClip = nullptr;
	if (pTrack && pTrack->trackType() == qtractorTrack::Midi)
		pMidiClip = static_cast<qtractorMidiClip *> (pClip);
	m_ui.clipUnlinkAction->setEnabled(pMidiClip && pMidiClip->isHashLinked());

	m_ui.clipRecordExAction->setEnabled(pMidiClip != nullptr);
	m_ui.clipRecordExAction->setChecked(pTrack && pTrack->isClipRecordEx()
		&& static_cast<qtractorMidiClip *> (pTrack->clipRecord()) == pMidiClip);

	m_ui.clipSplitAction->setEnabled(bClipSelected
		|| pTrack != nullptr || (pClip != nullptr
			&& iPlayHead > pClip->clipStart()
			&& iPlayHead < pClip->clipStart() + pClip->clipLength()));
	m_ui.clipMergeAction->setEnabled(bSingleTrackSelected);
	m_ui.clipNormalizeAction->setEnabled(bClipSelected);
	m_ui.clipTempoAdjustAction->setEnabled(bClipSelected);
	m_ui.clipCrossFadeAction->setEnabled(bClipSelected);
	m_ui.clipRangeSetAction->setEnabled(bClipSelected);
	m_ui.clipLoopSetAction->setEnabled(bClipSelected);
//	m_ui.clipImportAction->setEnabled(bTracks);
	m_ui.clipExportAction->setEnabled(bSingleTrackSelected);

	const bool bClipToolsEnabled = bClipSelected
		&& pTrack && pTrack->trackType() == qtractorTrack::Midi;
	m_ui.clipToolsMenu->setEnabled(bClipToolsEnabled);
	if (bClipToolsEnabled) {
		m_ui.clipToolsTimeshiftAction->setEnabled(bClipSelectable);
		m_ui.clipToolsTemporampAction->setEnabled(bClipSelectable);
	}

	m_ui.clipTakeMenu->setEnabled(pClip != nullptr);
	if (pClip)
		updateTakeMenu();
}


// Take menu stabilizers.
void qtractorMainForm::updateTakeMenu (void)
{
	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();

	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	const int iCurrentTake = (pTakeInfo ? pTakeInfo->currentTake() : -1);
	const int iTakeCount = (pTakeInfo ? pTakeInfo->takeCount() : 0);

//	m_ui.clipTakeMenu->setEnabled(pTakeInfo != nullptr);
	m_ui.clipTakeSelectMenu->setEnabled(iTakeCount > 0);
	m_ui.clipTakeFirstAction->setEnabled(iCurrentTake != 0 && iTakeCount > 0);
	m_ui.clipTakePrevAction->setEnabled(iTakeCount > 0);
	m_ui.clipTakeNextAction->setEnabled(iTakeCount > 0);
	m_ui.clipTakeLastAction->setEnabled(iCurrentTake < iTakeCount - 1);
	m_ui.clipTakeResetAction->setEnabled(iCurrentTake >= 0);
	m_ui.clipTakeRangeAction->setEnabled(pTakeInfo == nullptr);
}


// Take selection menu stabilizer.
void qtractorMainForm::updateTakeSelectMenu (void)
{
	m_ui.clipTakeSelectMenu->clear();

	qtractorClip *pClip = nullptr;
	if (m_pTracks)
		pClip = m_pTracks->currentClip();
	
	qtractorClip::TakeInfo *pTakeInfo = (pClip ? pClip->takeInfo() : nullptr);
	if (pTakeInfo) {
		QAction *pAction;
		const int iCurrentTake = pTakeInfo->currentTake();
		const int iTakeCount = pTakeInfo->takeCount();
		for (int iTake = 0; iTake < iTakeCount; ++iTake) {
			pAction = m_ui.clipTakeSelectMenu->addAction(
				tr("Take %1").arg(iTake + 1));
			pAction->setCheckable(true);
			pAction->setChecked(iTake == iCurrentTake);
			pAction->setData(iTake);
		}
		if (iTakeCount > 0)
			m_ui.clipTakeSelectMenu->addSeparator();
		pAction = m_ui.clipTakeSelectMenu->addAction(tr("None"));
		pAction->setCheckable(true);
		pAction->setChecked(iCurrentTake < 0);
		pAction->setData(-1);
	}
}


// Zoom view menu stabilizer.
void qtractorMainForm::updateZoomMenu (void)
{
	int iZoomMode = qtractorTracks::ZoomNone;
	if (m_pTracks)
		iZoomMode = m_pTracks->zoomMode();

	m_ui.viewZoomHorizontalAction->setChecked(
		iZoomMode == qtractorTracks::ZoomHorizontal);
	m_ui.viewZoomVerticalAction->setChecked(
		iZoomMode == qtractorTracks::ZoomVertical);
	m_ui.viewZoomAllAction->setChecked(
		iZoomMode == qtractorTracks::ZoomAll);
}


// Snap-per-beat view menu builder.
void qtractorMainForm::updateSnapMenu (void)
{
	m_ui.viewSnapMenu->clear();

	const int iSnapCurrent
		= qtractorTimeScale::indexFromSnap(m_pSession->snapPerBeat());

	int iSnap = 0;
	QListIterator<QAction *> iter(m_snapPerBeatActions);
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		pAction->setChecked(iSnap == iSnapCurrent);
		m_ui.viewSnapMenu->addAction(pAction);
		++iSnap;
	}

	m_ui.viewSnapMenu->addSeparator();
	m_ui.viewSnapMenu->addAction(m_ui.viewSnapZebraAction);
	m_ui.viewSnapMenu->addAction(m_ui.viewSnapGridAction);
}


// Track/Instrument sub-menu stabilizers.
void qtractorMainForm::updateTrackInstrumentMenu (void)
{
	if (m_pTracks) {
		m_pInstrumentMenu->updateTrackMenu(
			m_pTracks->currentTrack(), m_ui.trackInstrumentMenu);
	}
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

void qtractorMainForm::appendMessagesColor( const QString& s, const QColor& rgb )
{
	if (m_pMessages)
		m_pMessages->appendMessagesColor(s, rgb);

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

	appendMessagesColor(s.simplified(), Qt::red);

	QMessageBox::critical(this, tr("Error"), s);
}


// Force update of the messages font.
void qtractorMainForm::updateMessagesFont (void)
{
	if (m_pOptions == nullptr)
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
	if (m_pOptions == nullptr)
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
	if (m_pOptions == nullptr)
		return;

	if (m_pMessages)
		m_pMessages->setCaptureEnabled(m_pOptions->bStdoutCapture);
}


// Update/reset custome color (palette) theme..
void qtractorMainForm::updateCustomColorTheme (void)
{
	if (m_pOptions == nullptr)
		return;

	if (m_pOptions->sCustomColorTheme.isEmpty())
		return;

	QPalette pal(QApplication::palette());

	if (qtractorPaletteForm::namedPalette(
			&m_pOptions->settings(), m_pOptions->sCustomColorTheme, pal)) {

		QApplication::setPalette(pal);

		if (m_pTracks)
			m_pTracks->trackList()->updateTrackButtons();
		if (m_pMixer) {
			m_pMixer->updateTracks(true);
			m_pMixer->updateBuses(true);
		}

		QTimer::singleShot(QTRACTOR_TIMER_MSECS, this, SLOT(viewRefresh()));
	}
}


// Update/reset custom (widget) style theme..
void qtractorMainForm::updateCustomStyleTheme (void)
{
	if (m_pOptions == nullptr)
		return;

	if (m_pOptions->sCustomStyleTheme.isEmpty())
		return;

	QApplication::setStyle(
		QStyleFactory::create(m_pOptions->sCustomStyleTheme));
}


// Update/reset custom style sheet (QSS)..
void qtractorMainForm::updateCustomStyleSheet (void)
{
	if (m_pOptions == nullptr)
		return;

	QString sStyleSheet;

	if (!m_pOptions->sCustomStyleSheet.isEmpty()) {
		QFile file(m_pOptions->sCustomStyleSheet);
		if (file.open(QFile::ReadOnly)) {
			sStyleSheet = QString::fromUtf8(file.readAll());
			file.close();
		}
	}

	qApp->setStyleSheet(sStyleSheet);
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Editors stuff.

void qtractorMainForm::addEditorForm ( qtractorMidiEditorForm *pEditorForm )
{
	if (m_editors.indexOf(pEditorForm) < 0)
		m_editors.append(pEditorForm);
}

void qtractorMainForm::removeEditorForm ( qtractorMidiEditorForm *pEditorForm )
{
	const int iEditorForm = m_editors.indexOf(pEditorForm);
	if (iEditorForm >= 0)
		m_editors.removeAt(iEditorForm);
}


void qtractorMainForm::updateEditorForms (void)
{
	if (m_pOptions == nullptr)
		return;

	Qt::WindowFlags wflags = Qt::Window;
	if (m_pOptions->bKeepEditorsOnTop) {
		wflags |= Qt::Tool;
		wflags |= Qt::WindowStaysOnTopHint;
	}

	QListIterator<qtractorMidiEditorForm *> iter(m_editors);
	while (iter.hasNext()) {
		qtractorMidiEditorForm *pForm = iter.next();
		const bool bVisible = pForm->isVisible();
	#if 0//QTRACTOR_MIDI_EDITOR_TOOL_PARENT
		if (m_pOptions->bKeepEditorsOnTop)
			pForm->setParent(this);
		else
			pForm->setParent(nullptr);
	#endif
		pForm->setWindowFlags(wflags);
		if (bVisible)
			pForm->show();
	}
}


//-------------------------------------------------------------------------
// qtractorMainForm -- Timer stuff.

// Fast-timer slot funtion.
void qtractorMainForm::fastTimerSlot (void)
{
	// Currrent state...
	const bool bPlaying = m_pSession->isPlaying();
	long iPlayHead = long(m_pSession->playHead());

	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	qtractorMidiEngine  *pMidiEngine  = m_pSession->midiEngine();

	// Playhead status...
	if (iPlayHead != long(m_iPlayHead)) {
		// Update tracks-view play-head...
		m_pTracks->trackView()->setPlayHead(iPlayHead,
			m_ui.transportFollowAction->isChecked());
		// Update editors play-head...
		QListIterator<qtractorMidiEditorForm *> iter(m_editors);
		while (iter.hasNext())
			(iter.next()->editor())->setPlayHead(iPlayHead);
		// Update transport status anyway...
		if (!bPlaying && m_iTransportRolling == 0 && m_iTransportStep == 0) {
			++m_iTransportUpdate;
			// Send MMC LOCATE and MIDI SPP command...
			if (!pAudioEngine->isFreewheel()) {
				pMidiEngine->sendMmcLocate(
						m_pSession->locateFromFrame(iPlayHead));
				pMidiEngine->sendSppCommand(SND_SEQ_EVENT_SONGPOS,
						m_pSession->songPosFromFrame(iPlayHead));
			}
		}
		else
		if (iPlayHead < long(m_iPlayHead) && m_pSession->isLooping()) {
			// On probable loop turn-around:
			// Send MMC LOCATE command...
			pMidiEngine->sendMmcLocate(
				m_pSession->locateFromFrame(iPlayHead));
			pMidiEngine->sendSppCommand(SND_SEQ_EVENT_SONGPOS,
				m_pSession->songPosFromFrame(iPlayHead));
		}
		// Current position update...
		m_iPlayHead = iPlayHead;
	}

	// Transport status...
	if (m_iTransportUpdate > 0) {
		// Do some transport related tricks...
		if (m_iTransportRolling == 0) {
			m_iTransportUpdate = 0;
			if (m_iTransportStep) {
				// Transport stepping over...
				iPlayHead += (m_iTransportStep
					* long(m_pSession->frameFromTick(m_pSession->ticksPerBeat())));
				if (iPlayHead < 0)
					iPlayHead = 0;
				m_iTransportStep = 0;
				// Make it thru...
				m_pSession->setPlayHead(m_pSession->frameSnap(iPlayHead));
			}
		} else {
			// Transport rolling over...
			iPlayHead += long(m_fTransportShuttle
				* float(m_pSession->sampleRate())) >> 1;
			if (iPlayHead < 0) {
				iPlayHead = 0;
				m_iTransportUpdate = 0;
				// Stop playback for sure...
				setPlaying(false);
			}
			// Make it thru...
			m_pSession->setPlayHead(iPlayHead);
		}
		// Ensure track-view into visibility...
		if (m_ui.transportFollowAction->isChecked())
			m_pTracks->trackView()->ensureVisibleFrame(iPlayHead);
		// Take the chance to give some visual feedback...
		if (m_iTransportUpdate > 0) {
			updateTransportTime(iPlayHead);
			m_pThumbView->updateThumb();
		}
		// Ensure the main form is stable later on...
		++m_iStabilizeTimer;
		// Done with transport tricks.
	}

	// Always update meter values...
	qtractorMeterValue::refreshAll();

	// Asynchronous observer update...
	if (qtractorSubject::flushQueue(true))
		++m_iStabilizeTimer;

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_TIME
	// Update plugin LV2 Time designated ports, if any...
	qtractorLv2Plugin::updateTimePost();
#endif
#ifdef CONFIG_LV2_UI
	// Crispy plugin LV2 UI idle-updates...
	qtractorLv2Plugin::idleEditorAll();
#endif
#endif
#ifdef CONFIG_CLAP
	// Crispy plugin CLAP UI idle-updates...
	qtractorClapPlugin::idleEditorAll();
#endif
#ifdef CONFIG_VST2
	// Crispy plugin VST2 UI idle-updates...
	qtractorVst2Plugin::idleEditorAll();
#endif

	// Register the next fast-timer slot.
	QTimer::singleShot(QTRACTOR_TIMER_MSECS, this, SLOT(fastTimerSlot()));
}


// Slow-timer slot funtion.
void qtractorMainForm::slowTimerSlot (void)
{
	// Avoid stabilize re-entrancy...
	if (m_pSession->isBusy() || m_iTransportUpdate > 0) {
		// Register the next timer slot.
		QTimer::singleShot(QTRACTOR_TIMER_DELAY, this, SLOT(slowTimerSlot()));
		return;
	}

	// Currrent state...
	const bool bPlaying = m_pSession->isPlaying();
	unsigned long iPlayHead = m_pSession->playHead();

	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	qtractorMidiEngine  *pMidiEngine  = m_pSession->midiEngine();

	// Read JACK transport state...
	jack_client_t *pJackClient = pAudioEngine->jackClient();
	if (pJackClient && !pAudioEngine->isFreewheel()) {
		jack_position_t pos;
		jack_transport_state_t state
			= jack_transport_query(pJackClient, &pos);
		// React if out-of-sync...
		if (pAudioEngine->transportMode() & qtractorBus::Input) {
			// 1. Check on external transport state request changes...
			if ((state == JackTransportStopped &&  bPlaying) ||
				(state == JackTransportRolling && !bPlaying)) {
			#ifdef CONFIG_DEBUG
				qDebug("qtractorMainForm::slowTimerSlot() playing=%d state=%d",
					int(bPlaying), int(state == JackTransportRolling));
			#endif
				iPlayHead = pos.frame;
				transportPlay(); // Toggle playing!
				if (!bPlaying)
					m_pSession->seek(iPlayHead, true);
			}
			// 2. Watch for tempo/time-sig changes on JACK transport...
			if ((pos.valid & JackPositionBBT) && pAudioEngine->isTimebaseEx()) {
				qtractorTimeScale *pTimeScale = m_pSession->timeScale();
				qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
				qtractorTimeScale::Node *pNode = cursor.seekFrame(pos.frame);
				if (pNode && pos.frame >= pNode->frame && (
					qAbs(pNode->tempo - pos.beats_per_minute) > 0.001f ||
					pNode->beatsPerBar != (unsigned short) pos.beats_per_bar ||
					(1 << pNode->beatDivisor) != (unsigned short) pos.beat_type)) {
				#ifdef CONFIG_DEBUG
					qDebug("qtractorMainForm::slowTimerSlot() tempo=%g %u/%u",
						pos.beats_per_minute,
						(unsigned short) pos.beats_per_bar,
						(unsigned short) pos.beat_type);
				#endif
					m_pTracks->clearSelect(true);
					m_pSession->lock();
					pNode->tempo = pos.beats_per_minute;
					pNode->beatsPerBar = pos.beats_per_bar;
					pNode->beatDivisor = 0;
					unsigned short i = pos.beat_type;
					while (i > 1) {	++(pNode->beatDivisor); i >>= 1; }
					pTimeScale->updateNode(pNode);
					m_pTempoSpinBox->setTempo(pNode->tempo, false);
					m_pTempoSpinBox->setBeatsPerBar(pNode->beatsPerBar, false);
					m_pTempoSpinBox->setBeatDivisor(pNode->beatDivisor, false);
					pAudioEngine->resetMetro();
					pMidiEngine->resetTempo();
					m_pSession->unlock();
					updateContents(nullptr, true);
				}
			}
		}
	}

	// Check if its time to refresh playhead timer...
	if (bPlaying) {
		updateTransportTime(iPlayHead);
		// If recording update track view and session length, anyway...
		if (m_pSession->isRecording()) {
			// HACK: Care of punch-out...
			if (m_pSession->isPunching()) {
				const unsigned long iFrameTime = m_pSession->frameTimeEx();
				const unsigned long iLoopStart = m_pSession->loopStart();
				const unsigned long iLoopEnd = m_pSession->loopEnd();
				unsigned long iPunchOut = m_pSession->punchOut();
				if (iLoopStart < iLoopEnd &&
					iLoopStart < iFrameTime &&
					m_pSession->loopRecordingMode() > 0) {
					const unsigned long iLoopLength
						= iLoopEnd - iLoopStart;
					const unsigned long iLoopCount
						= (iFrameTime - iLoopStart) / iLoopLength;
					iPunchOut += (iLoopCount + 1) * iLoopLength;
				}
				// Make sure it's really about to punch-out...
				if (iFrameTime > iPunchOut && setRecording(false)) {
					// Send MMC RECORD_EXIT command...
					pMidiEngine->sendMmcCommand(
						qtractorMmcEvent::RECORD_EXIT);
					++m_iTransportUpdate;
				}
			}
			// Recording visual feedback...
			m_pTracks->updateContentsRecord();
			m_pSession->updateSession(0, iPlayHead);
			m_statusItems[StatusTime]->setText(
				m_pSession->timeScale()->textFromFrame(
					0, true, m_pSession->sessionEnd()));
		}
		else
		// Whether to continue past end...
		if (!m_ui.transportContinueAction->isChecked()) {
			unsigned long iSessionEnd = m_pSession->sessionEnd();
			if (m_pSession->isLooping()) {
				const unsigned long iLoopEnd = m_pSession->loopEnd();
				if (iSessionEnd < iLoopEnd)
					iSessionEnd = iLoopEnd;
			}
			if (iPlayHead > iSessionEnd)
				transportPlay(); // Stop at once!
		}
	}

	// Check if we've got some XRUN callbacks...
	if (m_iXrunTimer > 0 && --m_iXrunTimer < 1) {
		m_iXrunTimer = 0;
		// Reset audio/MIDI drift correction...
		if (bPlaying)
			pMidiEngine->resetDrift();
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
		// Let the XRUN status item get an update...
		++m_iStabilizeTimer;
	}

	// Check if its time to refresh some tracks...
	if (m_iAudioPeakTimer > 0 && --m_iAudioPeakTimer < 1) {
		m_iAudioPeakTimer = 0;
		m_pTracks->trackView()->updateContents();
	}

	// Check if its time to refresh Audio connections...
	if (m_iAudioRefreshTimer > 0 && --m_iAudioRefreshTimer < 1) {
		m_iAudioRefreshTimer = 0;
		if (pAudioEngine->updateConnects() == 0) {
			appendMessagesColor(
				tr("Audio connections change."), "#cc9966");
			if (m_iAudioPropertyChange > 0) {
				m_iAudioPropertyChange = 0;
				m_pConnections->connectForm()->audioClear();
			} else {
				m_pConnections->connectForm()->audioRefresh();
			}
		}
	}

	// MIDI connections should be checked too...
	if (m_iMidiRefreshTimer > 0 && --m_iMidiRefreshTimer < 1) {
		m_iMidiRefreshTimer = 0;
		if (pMidiEngine->updateConnects() == 0) {
			appendMessagesColor(
				tr("MIDI connections change."), "#66cc99");
			m_pConnections->connectForm()->midiRefresh();
		}
	}

	// Check if its time to refresh audition/pre-listening status...
	if (m_iPlayerTimer > 0 && --m_iPlayerTimer < 1) {
		m_iPlayerTimer = 0;
		const bool bPlayerOpen
			= (pAudioEngine->isPlayerOpen() || pMidiEngine->isPlayerOpen());
		if (bPlayerOpen) {
			if ((m_pFiles && m_pFiles->isPlayState()) &&
				(m_pFileSystem && m_pFileSystem->isPlayState()))
				++m_iPlayerTimer;
		}
		if (m_iPlayerTimer < 1) {
			if (m_pFiles && m_pFiles->isPlayState())
				m_pFiles->setPlayState(false);
			if (m_pFileSystem && m_pFileSystem->isPlayState())
				m_pFileSystem->setPlayState(false);
			if (bPlayerOpen) {
				appendMessages(tr("Playing ended."));
				pAudioEngine->closePlayer();
				pMidiEngine->closePlayer();
			}
		}
	}

	// Slower plugin UI idle cycle...
#ifdef CONFIG_DSSI
#ifdef CONFIG_LIBLO
	qtractorDssiPlugin::idleEditorAll();
#endif
#endif

	// Auto-save option routine...
	if (m_iAutoSavePeriod > 0 && m_iDirtyCount > 0) {
		m_iAutoSaveTimer += QTRACTOR_TIMER_DELAY;
		if (m_iAutoSaveTimer > m_iAutoSavePeriod && !bPlaying) {
			m_iAutoSaveTimer = 0;
			autoSaveSession();
		}
	}

	// Check if its time to stabilize main form...
	if (m_iStabilizeTimer > 0/* && --m_iStabilizeTimer < 1 */) {
		m_iStabilizeTimer = 0;
		stabilizeForm();
	}

	// Register the next slow-timer slot.
	QTimer::singleShot(QTRACTOR_TIMER_DELAY, this, SLOT(slowTimerSlot()));
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
	if (m_iMidiRefreshTimer < 2) ++m_iMidiRefreshTimer;
}


// Audio file peak notification slot.
void qtractorMainForm::audioPeakNotify (void)
{
	// An audio peak file has just been (re)created;
	// try to postpone the event effect a little more...
	if (m_iAudioPeakTimer < 2) ++m_iAudioPeakTimer;
}


// Custom audio shutdown event handler.
void qtractorMainForm::audioShutNotify (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::audioShutNotify()");
#endif

	// HACK: The audio engine (jackd) most probably
	// is down, not up and running anymore, anyway...
	m_pSession->lock();

	// Always do auto-save here, hence...
	autoSaveSession();

	// Engines shutdown is on demand...
	m_pSession->shutdown();
	m_pConnections->clear();

	// HACK: Done.
	m_pSession->unlock();

	// Send an informative message box...
	appendMessagesError(
		tr("The audio engine has been shutdown.\n\n"
		"Make sure the JACK audio server (jackd)\n"
		"is up and running and then restart session."));

	// Try an immediate restart, though...
	checkRestartSession();

	// Make things just bearable...
	++m_iStabilizeTimer;
}


// Custom audio XRUN event handler.
void qtractorMainForm::audioXrunNotify (void)
{
	// An XRUN has just been notified...
	++m_iXrunCount;

	// Skip this one, maybe we're under some kind of storm;
	if (m_iXrunTimer > 0)
		++m_iXrunSkip;

	// Defer the informative effect...
	++m_iXrunTimer;
}


// Custom audio port/graph change event handler.
void qtractorMainForm::audioPortNotify (void)
{
	// An Audio graph change has just been issued;
	// try to postpone the event effect a little more...
	if (m_iAudioRefreshTimer < 2) ++m_iAudioRefreshTimer;
}


// Custom audio buffer size change event handler.
void qtractorMainForm::audioBuffNotify ( unsigned int iBufferSize )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::audioBuffNotify(%u)", iBufferSize);
#endif

	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	// HACK: The audio engine (jackd) is still up
	// and running, just with bigger buffer size...
	m_pSession->lock();

	// Always do auto-save here, hence...
	autoSaveSession();

	// Connections snapshot stuff...
	qtractorBus::Connections audio_connections;
	qtractorBus::Connections midi_connections;

	// Get all connections snapshot...
	audio_connections.save(pAudioEngine);
	midi_connections.save(pMidiEngine);

	// Engines shutdown is on demand...
	m_pSession->shutdown();
	m_pConnections->clear();

	// HACK: Done.
	m_pSession->unlock();

	// Send an informative message box...
	appendMessagesError(
		tr("The audio engine buffer size has changed,\n"
		"increased from %1 to %2 frames/period.\n\n"
		"Reloading the current session file\n"
		"is highly recommended.")
		.arg(pAudioEngine->bufferSize())
		.arg(iBufferSize));
#if 0
	// Reload the previously auto-saved session...
	const QString& sAutoSavePathname = m_pOptions->sAutoSavePathname;
	if (!sAutoSavePathname.isEmpty()
		&& QFileInfo(sAutoSavePathname).exists()) {
		// Reset (soft) subject/observer queue.
		qtractorSubject::resetQueue();
		// Reset all dependables to default.
		m_pMixer->clear();
		m_pFiles->clear();
		// Close session engines.
		m_pSession->close();
		m_pSession->clear();
		m_pTempoCursor->clear();
		// And last but not least.
		m_pConnections->clear();
		m_pTracks->clear();
		// Clear (hard) subject/observer queue.
		qtractorSubject::clearQueue();
		// Reset playhead.
		m_iPlayHead = 0;
	#ifdef CONFIG_LV2
		qtractorLv2PluginType::lv2_close();
	#endif
		// Reload the auto-saved session alright...
		const int iFlags = qtractorDocument::Default;
		if (loadSessionFileEx(sAutoSavePathname, iFlags, false)) {
			m_sFilename = m_pOptions->sAutoSaveFilename;
			++m_iDirtyCount;
		}
		// Final view update, just in case...
		selectionNotifySlot(nullptr);
	}
	else
#else
	// Try an immediate restart, and restore connections snapshot...
	if (checkRestartSession()) {
		audio_connections.load(pAudioEngine);
		midi_connections.load(pMidiEngine);
	}
	// Shall make it dirty anyway...
	updateDirtyCount(true);
#endif
	// Make things just bearable...
	++m_iStabilizeTimer;
}


#ifdef CONFIG_JACK_SESSION
#if defined(Q_CC_GNU) || defined(Q_CC_MINGW)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

// Custom (JACK) session event handler.
void qtractorMainForm::audioSessNotify ( void *pvSessionArg )
{
#ifdef CONFIG_JACK_SESSION

	if (m_pOptions == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	jack_client_t *pJackClient = pAudioEngine->jackClient();
	if (pJackClient == nullptr)
		return;

	jack_session_event_t *pJackSessionEvent
		= (jack_session_event_t *) pvSessionArg;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::audioSessNotify()"
		" type=%d client_uuid=\"%s\" session_dir=\"%s\"",
		int(pJackSessionEvent->type),
		pJackSessionEvent->client_uuid,
		pJackSessionEvent->session_dir);
#endif

	const bool bTemplate = (pJackSessionEvent->type == JackSessionSaveTemplate);
	const bool bQuit = (pJackSessionEvent->type == JackSessionSaveAndQuit);

	const QString sOldSessionDir
		= m_pSession->sessionDir();
	const QString sSessionDir
		= QString::fromUtf8(pJackSessionEvent->session_dir);
	m_pSession->setSessionDir(sSessionDir);

	if (m_pSession->sessionName().isEmpty())
		m_pSession->setSessionName(::getenv("LADISH_PROJECT_NAME"));
	if (m_pSession->sessionName().isEmpty())
		editSession();

	QString sSessionName = m_pSession->sessionName();
	if (sSessionName.isEmpty())
		sSessionName = tr("Untitled%1").arg(m_iUntitled);

	const QString sSessionExt = (bTemplate
		? qtractorDocument::templateExt()
		: m_pOptions->sSessionExt);
	const QString sSessionFile = sSessionName + '.' + sSessionExt;

	QStringList args;
	args << QApplication::applicationFilePath();
	args << QString("--session-id=%1").arg(pJackSessionEvent->client_uuid);

	const QString sFilename
		= QFileInfo(sSessionDir, sSessionFile).absoluteFilePath();
	const int iFlags
		= (bTemplate ? qtractorDocument::Template : qtractorDocument::Default);

	if (saveSessionFileEx(sFilename, iFlags | qtractorDocument::SymLink, false))
		args << QString("${SESSION_DIR}%1").arg(sSessionFile);

	const QByteArray aCmdLine = args.join(" ").toUtf8();
	pJackSessionEvent->command_line = ::strdup(aCmdLine.constData());

	jack_session_reply(pJackClient, pJackSessionEvent);
	jack_session_event_free(pJackSessionEvent);

	m_pSession->setSessionDir(sOldSessionDir);

	if (bQuit) {
		m_iDirtyCount = 0;
		close();
	}

#endif	// CONFIG_JACK_SESSION
}

#ifdef CONFIG_JACK_SESSION
#if defined(Q_CC_GNU) || defined(Q_CC_MINGW)
#pragma GCC diagnostic pop
#endif
#endif


// Custom (JACK) transport sync event handler.
void qtractorMainForm::audioSyncNotify ( unsigned long iPlayHead, bool bPlaying )
{
	if (m_pSession->isBusy())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::audioSyncNotify(%lu, %d)", iPlayHead, int(bPlaying));
#endif

	if (( bPlaying && !m_pSession->isPlaying()) ||
		(!bPlaying &&  m_pSession->isPlaying())) {
		// Toggle playing!
		transportPlay();
	}

	m_pSession->setPlayHeadEx(iPlayHead);
	++m_iTransportUpdate;
}


// Custom (JACK) audio property change event handler.
void qtractorMainForm::audioPropNotify (void)
{
	// An Audio property change has just been issued;
	// try to postpone the event effect a little more...
	if (m_iAudioRefreshTimer < 2) ++m_iAudioRefreshTimer;

	// Mark that a complete refresh is needed...
	++m_iAudioPropertyChange;
}


// Custom MMC event handler.
void qtractorMainForm::midiMmcNotify ( const qtractorMmcEvent& mmce )
{
	QString sMmcText("MIDI MMC: ");
	switch (mmce.cmd()) {
	case qtractorMmcEvent::STOP:
	case qtractorMmcEvent::PAUSE:
		sMmcText += tr("STOP");
		if (setPlaying(false) // Auto-backward reset feature...
			&& m_ui.transportAutoBackwardAction->isChecked())
			m_pSession->setPlayHead(m_pSession->playHeadAutoBackward());
		break;
	case qtractorMmcEvent::PLAY:
	case qtractorMmcEvent::DEFERRED_PLAY:
		sMmcText += tr("PLAY");
		setPlaying(true);
		break;
	case qtractorMmcEvent::FAST_FORWARD:
		sMmcText += tr("FFWD");
		setRolling(+1);
		break;
	case qtractorMmcEvent::REWIND:
		sMmcText += tr("REW");
		setRolling(-1);
		break;
	case qtractorMmcEvent::RECORD_STROBE:
	case qtractorMmcEvent::RECORD_PAUSE:
		sMmcText += tr("REC ON");
		if (!setRecording(true)) {
			// Send MMC RECORD_EXIT command immediate reply...
			m_pSession->midiEngine()->sendMmcCommand(
				qtractorMmcEvent::RECORD_EXIT);
		}
		break;
	case qtractorMmcEvent::RECORD_EXIT:
		sMmcText += tr("REC OFF");
		setRecording(false);
		break;
	case qtractorMmcEvent::MMC_RESET:
		sMmcText += tr("RESET");
		setRolling(0);
		break;
	case qtractorMmcEvent::LOCATE:
		sMmcText += tr("LOCATE %1").arg(mmce.locate());
		setLocate(mmce.locate());
		break;
	case qtractorMmcEvent::SHUTTLE:
		sMmcText += tr("SHUTTLE %1").arg(mmce.shuttle());
		setShuttle(mmce.shuttle());
		break;
	case qtractorMmcEvent::STEP:
		sMmcText += tr("STEP %1").arg(mmce.step());
		setStep(mmce.step());
		break;
	case qtractorMmcEvent::MASKED_WRITE:
		switch (mmce.scmd()) {
		case qtractorMmcEvent::TRACK_RECORD:
			sMmcText += tr("TRACK RECORD %1 %2")
				.arg(mmce.track())
				.arg(mmce.isOn());
			break;
		case qtractorMmcEvent::TRACK_MUTE:
			sMmcText += tr("TRACK MUTE %1 %2")
				.arg(mmce.track())
				.arg(mmce.isOn());
			break;
		case qtractorMmcEvent::TRACK_SOLO:
			sMmcText += tr("TRACK SOLO %1 %2")
				.arg(mmce.track())
				.arg(mmce.isOn());
			break;
		case qtractorMmcEvent::TRACK_MONITOR:
			sMmcText += tr("TRACK MONITOR %1 %2")
				.arg(mmce.track())
				.arg(mmce.isOn());
			break;
		default:
			sMmcText += tr("Unknown sub-command");
			break;
		}
		setTrack(mmce.scmd(), mmce.track(), mmce.isOn());
		break;
	default:
		sMmcText += tr("Not implemented");
		break;
	}

	appendMessages(sMmcText);
	++m_iStabilizeTimer;
}


// Custom controller event handler.
void qtractorMainForm::midiCtlNotify ( const qtractorCtlEvent& ctle )
{
#ifdef CONFIG_DEBUG
	QString sCtlText(tr("MIDI CTL: %1, Channel %2, Param %3, Value %4")
		.arg(qtractorMidiControl::nameFromType(ctle.type()))
		.arg(ctle.channel() + 1)
		.arg(ctle.param())
		.arg(ctle.value()));
	qDebug("qtractorMainForm::midiCtlNotify() %s.",
		sCtlText.toUtf8().constData());
#endif

	// Check if controller is used as MIDI controller...
	if (m_pMidiControl->processEvent(ctle)) {
	#ifdef CONFIG_DEBUG
		appendMessages(sCtlText);
	#endif
		return;
	}

	if (ctle.type() == qtractorMidiEvent::CONTROLLER) {
		/* FIXME: JLCooper faders (as from US-224)...
		if (ctle.channel() == 15) {
			// Event translation...
			int   iTrack = int(ctle.controller()) & 0x3f;
			float fGain  = float(ctle.value()) / 127.0f;
			// Find the track by number...
			qtractorTrack *pTrack = m_pSession->tracks().at(iTrack);
			if (pTrack && qAbs(fGain - pTrack->gain()) > 0.001f) {
				m_pSession->execute(
					new qtractorTrackGainCommand(pTrack, fGain, true));
			#ifdef CONFIG_DEBUG
				sCtlText += ' ';
				sCtlText += tr("(track %1, gain %2)")
					.arg(iTrack).arg(fGain);
				appendMessages(sCtlText);
			#endif
			}
		}
		else */
		// Handle volume controls...
		if (ctle.param() == 7) {
			int iTrack = 0;
			const float fGain = float(ctle.value()) / 127.0f;
			for (qtractorTrack *pTrack = m_pSession->tracks().first();
					pTrack; pTrack = pTrack->next()) {
				if (pTrack->trackType() == qtractorTrack::Midi &&
					pTrack->midiChannel() == ctle.channel() &&
					qAbs(fGain - pTrack->gain()) > 0.001f) {
					m_pSession->execute(
						new qtractorTrackGainCommand(pTrack, fGain, true));
				#ifdef CONFIG_DEBUG
					sCtlText += ' ';
					sCtlText += tr("(track %1, gain %2)")
						.arg(iTrack).arg(fGain);
					appendMessages(sCtlText);
				#endif
				}
				++iTrack;
			}
		}
		else
		// Handle pan controls...
		if (ctle.param() == 10) {
			int iTrack = 0;
			const float fPanning = (float(ctle.value()) - 64.0f) / 63.0f;
			for (qtractorTrack *pTrack = m_pSession->tracks().first();
					pTrack; pTrack = pTrack->next()) {
				if (pTrack->trackType() == qtractorTrack::Midi &&
					pTrack->midiChannel() == ctle.channel() &&
					qAbs(fPanning - pTrack->panning()) > 0.001f) {
					m_pSession->execute(
						new qtractorTrackPanningCommand(pTrack, fPanning, true));
				#ifdef CONFIG_DEBUG
					sCtlText += ' ';
					sCtlText += tr("(track %1, panning %2)")
						.arg(iTrack).arg(fPanning);
					appendMessages(sCtlText);
				#endif
				}
				++iTrack;
			}
		}
	}
}


// Custom MIDI SPP  event handler.
void qtractorMainForm::midiSppNotify ( int iSppCmd, unsigned short iSongPos )
{
#ifdef CONFIG_DEBUG
	QString sSppText("MIDI SPP: ");
#endif
	switch (iSppCmd) {
	case SND_SEQ_EVENT_START:
	#ifdef CONFIG_DEBUG
		sSppText += tr("START");
	#endif
		setSongPos(0);
		setPlaying(true);
		break;
	case SND_SEQ_EVENT_STOP:
	#ifdef CONFIG_DEBUG
		sSppText += tr("STOP");
	#endif
		if (setPlaying(false) // Auto-backward reset feature...
			&& m_ui.transportAutoBackwardAction->isChecked())
			m_pSession->setPlayHead(m_pSession->playHeadAutoBackward());
		break;
	case SND_SEQ_EVENT_CONTINUE:
	#ifdef CONFIG_DEBUG
		sSppText += tr("CONTINUE");
	#endif
		setPlaying(true);
		break;
	case SND_SEQ_EVENT_SONGPOS:
	#ifdef CONFIG_DEBUG
		sSppText += tr("SONGPOS %1").arg(iSongPos);
	#endif
		setSongPos(iSongPos);
		break;
	default:
	#ifdef CONFIG_DEBUG
		sSppText += tr("Not implemented");
	#endif
		break;
	}
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::midiSppNotify() %s.",
		sSppText.toUtf8().constData());
	appendMessages(sSppText);
#endif
	++m_iStabilizeTimer;
}


// Custom MIDI Clock event handler.
void qtractorMainForm::midiClkNotify ( float fTempo )
{
#ifdef CONFIG_DEBUG
	QString sClkText("MIDI CLK: ");
	sClkText += tr("%1 BPM").arg(fTempo);
	qDebug("qtractorMainForm::midiClkNotify() %s.",
		sClkText.toUtf8().constData());
	appendMessages(sClkText);
#endif

	if (m_pTracks)
		m_pTracks->clearSelect(true);

	// Find appropriate node...
	qtractorTimeScale *pTimeScale = m_pSession->timeScale();
	qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_pSession->playHead());

	// Now, express the change immediately...
	if (pNode->prev()) {
		pNode->tempo = fTempo;
		pTimeScale->updateNode(pNode);
	} else {
		m_pSession->setTempo(fTempo);
	}
	++m_iTransportUpdate;

	updateContents(nullptr, true);
	++m_iStabilizeTimer;
}


// Custom MIDI Step input event handler.
void qtractorMainForm::midiInpNotify ( unsigned short flags )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::midiInpNotify(%u)", flags);
#endif

	// Update current step-input location
	// for all the in-recording clips out there...
	if (flags & (qtractorMidiEngine::InpReset | qtractorMidiEngine::InpEvent)) {
		const unsigned long iStepInputHead = m_pSession->playHead();
		for (qtractorTrack *pTrack = m_pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			if (pTrack->trackType() == qtractorTrack::Midi
				&& pTrack->isClipRecordEx()) {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (pTrack->clipRecord());
				if (pMidiClip) {
					if (flags & qtractorMidiEngine::InpReset)
						pMidiClip->setStepInputHead(iStepInputHead);
					pMidiClip->updateStepInput();
				}
			}
		}
	}

	// Handle pending step-input events...
	if (flags & qtractorMidiEngine::InpEvent)
		m_pSession->midiEngine()->processInpEvents();

	++m_iStabilizeTimer;
}


//-------------------------------------------------------------------------
// qtractorMainForm -- General contents change stuff.

// Audio file addition slot funtion.
void qtractorMainForm::addAudioFile ( const QString& sFilename )
{
	// Add the just dropped audio file...
	if (m_pFiles)
		m_pFiles->addAudioFile(sFilename, true);

	++m_iStabilizeTimer;
}


// Audio file selection slot funtion.
void qtractorMainForm::selectAudioFile (
	const QString& sFilename, int iTrackChannel, bool bSelect )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::selectAudioFile(\"%s\", %d, %d)",
		sFilename.toUtf8().constData(), iTrackChannel, int(bSelect));
#endif

	// Select audio file...
	if (m_pTracks) {
		m_pTracks->trackView()->selectClipFile(
			qtractorTrack::Audio, sFilename, iTrackChannel, bSelect);
	}

	++m_iStabilizeTimer;
}


// Audio file activation slot funtion.
void qtractorMainForm::activateAudioFile (
	const QString& sFilename, int /*iTrackChannel*/ )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::activateAudioFile(\"%s\")",
		sFilename.toUtf8().constData());
#endif

	// Make sure session is activated...
	checkRestartSession();

	// We'll start playing if the file is valid, otherwise
	// the player is stopped (eg. empty filename)...
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine && pAudioEngine->openPlayer(sFilename)) {
		// Try updating player status anyway...
		if (m_pFiles)
			m_pFiles->setPlayState(true);
		if (m_pFileSystem)
			m_pFileSystem->setPlayState(true);
		++m_iPlayerTimer;
		appendMessages(tr("Playing \"%1\"...")
			.arg(QFileInfo(sFilename).fileName()));
	}

	++m_iStabilizeTimer;
}


// MIDI file addition slot funtion.
void qtractorMainForm::addMidiFile ( const QString& sFilename )
{
	// Add the just dropped MIDI file...
	if (m_pFiles)
		m_pFiles->addMidiFile(sFilename, true);

	++m_iStabilizeTimer;
}


// MIDI file selection slot funtion.
void qtractorMainForm::selectMidiFile (
	const QString& sFilename, int iTrackChannel, bool bSelect )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::selectMidiFile(\"%s\", %d, %d)",
		sFilename.toUtf8().constData(), iTrackChannel, int(bSelect));
#endif

	// Select MIDI file track/channel...
	if (m_pTracks) {
		m_pTracks->trackView()->selectClipFile(
			qtractorTrack::Midi, sFilename, iTrackChannel, bSelect);
	}

	++m_iStabilizeTimer;
}


// MIDI file activation slot funtion.
void qtractorMainForm::activateMidiFile (
	const QString& sFilename, int iTrackChannel )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::activateMidiFile(\"%s\", %d)",
		sFilename.toUtf8().constData(), iTrackChannel);
#endif

	// Make sure session is activated...
	checkRestartSession();

	// We'll start playing if the file is valid, otherwise
	// the player is stopped (eg. empty filename)...
	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine && pMidiEngine->openPlayer(sFilename, iTrackChannel)) {
		// Try updating player status anyway...
		if (m_pFiles)
			m_pFiles->setPlayState(true);
		if (m_pFileSystem)
			m_pFileSystem->setPlayState(true);
		++m_iPlayerTimer;
		appendMessages(tr("Playing \"%1\"...")
			.arg(QFileInfo(sFilename).fileName()));
	}

	++m_iStabilizeTimer;
}


// Generic file activation slot funtion.
void qtractorMainForm::activateFile ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::activateFile(\"%s\")",
		sFilename.toUtf8().constData());
#endif

	// First test if it's an audio file ?
	qtractorAudioFile *pFile
		= qtractorAudioFileFactory::createAudioFile(sFilename);
	if (pFile) {
		if (pFile->open(sFilename)) {
			pFile->close();
			delete pFile;
			activateAudioFile(sFilename);
		} else {
			delete pFile;
		}
	} else {
		// Then whether it's a MIDI file...
		qtractorMidiFile file;
		if (file.open(sFilename)) {
			file.close();
			activateMidiFile(sFilename);
		}
		else
		// Maybe a session file?...
		if (closeSession())
			loadSessionFile(sFilename);
	}
}


// Tracks view selection change slot.
void qtractorMainForm::trackSelectionChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::trackSelectionChanged()");
#endif

	// Select sync to mixer...
	if (m_pTracks && m_pMixer) {
		// Doesn't matter whether current track is none...
		qtractorTrack *pTrack = m_pTracks->trackList()->currentTrack();
		m_pMixer->setCurrentTrack(pTrack);
		// HACK: Set current session track for monitoring purposes...
		if (m_ui.trackAutoMonitorAction->isChecked())
			m_pSession->setCurrentTrack(pTrack);
	}

	++m_iStabilizeTimer;
}


// Mixer view selection change slot.
void qtractorMainForm::mixerSelectionChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::mixerSelectionChanged()");
#endif

	// Select sync to tracks...
	if (m_pTracks && m_pMixer && m_pMixer->trackRack()) {
		qtractorTrack *pTrack = nullptr;
		qtractorMixerStrip *pStrip = (m_pMixer->trackRack())->selectedStrip();
		if (pStrip)
			pTrack = pStrip->track();
		m_pTracks->trackList()->setCurrentTrack(pTrack);
	}

	++m_iStabilizeTimer;
}


// Tracks view selection change slot.
void qtractorMainForm::selectionNotifySlot ( qtractorMidiEditor *pMidiEditor )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::selectionNotifySlot()");
#endif

	// Read session edit-head/tails...
	const unsigned long iEditHead = m_pSession->editHead();
	const unsigned long iEditTail = m_pSession->editTail();

	// Track-view is due...
	if (m_pTracks) {
		qtractorTrackView *pTrackView = m_pTracks->trackView();
		pTrackView->setEditHead(iEditHead);
		pTrackView->setEditTail(iEditTail);
		pTrackView->setPlayHeadAutoBackward(
			m_pSession->playHeadAutoBackward());
	//	if (pMidiEditor) m_pTracks->clearSelect();
	}

	// Update editors edit-head/tails...
	QListIterator<qtractorMidiEditorForm *> iter(m_editors);
	while (iter.hasNext()) {
		qtractorMidiEditor *pEditor = (iter.next())->editor();
		if (pEditor != pMidiEditor) {
			pEditor->setEditHead(iEditHead, false);
			pEditor->setEditTail(iEditTail, false);
		}
	}

	// Normal status ahead...
	++m_iStabilizeTimer;
}


// Clip editors update helper.
void qtractorMainForm::changeNotifySlot ( qtractorMidiEditor *pMidiEditor )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::changeNotifySlot(%p)", pMidiEditor);
#endif

	updateContents(pMidiEditor, true);
}


// Command update helper.
void qtractorMainForm::updateNotifySlot ( unsigned int flags )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::updateNotifySlot(0x%02x)", int(flags));
#endif

	// Always reset any track view selection...
	// (avoid change/update notifications, again)
	if (m_pTracks && (flags & qtractorCommand::ClearSelect))
		m_pTracks->clearSelect(flags & qtractorCommand::Reset);

	// Proceed as usual...
	updateContents(nullptr, (flags & qtractorCommand::Refresh));
}


// Common update helper.
void qtractorMainForm::updateContents (
	qtractorMidiEditor *pMidiEditor, bool bRefresh )
{
	// Maybe, just maybe, we've made things larger...
	m_pTempoCursor->clear();

	m_pSession->updateTimeScale();
	m_pSession->updateSession();

	// Refresh track-view?
	if (m_pTracks)
		m_pTracks->updateContents(bRefresh);

	// Update other editors contents...
	QListIterator<qtractorMidiEditorForm *> iter(m_editors);
	while (iter.hasNext()) {
		qtractorMidiEditorForm *pForm = iter.next();
		if (pForm->editor() != pMidiEditor)
			pForm->updateTimeScale();
	}

	// Notify who's watching...
	contentsChanged();
}


void qtractorMainForm::updateDirtyCount ( bool bDirtyCount )
{
	if (bDirtyCount) {
		++m_iDirtyCount;
	} else {
		m_iDirtyCount = 0;
	}

#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active()) {
		if (!m_bNsmDirty && bDirtyCount) {
			m_pNsmClient->dirty(true);
			m_bNsmDirty = true;
		}
		else
		if (m_bNsmDirty && !bDirtyCount) {
			m_pNsmClient->dirty(false);
			m_bNsmDirty = false;
		}
	}
#endif
}


// Tracks view contents change slot.
void qtractorMainForm::contentsChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::contentsChanged()");
#endif

	// HACK: Force immediate stabilization later...
	//m_iStabilizeTimer = 0;

	// Stabilize session toolbar widgets...
//	m_pTempoSpinBox->setTempo(m_pSession->tempo(), false);
//	m_pTempoSpinBox->setBeatsPerBar(m_pSession->beatsPerBar(), false);
//	m_pTempoSpinBox->setBeatDivisor(m_pSession->beatDivisor(), false);
	m_pSnapPerBeatComboBox->setCurrentIndex(
		qtractorTimeScale::indexFromSnap(m_pSession->snapPerBeat()));

	m_pThumbView->updateContents();

	dirtyNotifySlot();
}


// Tracks view contents dirty up slot.
void qtractorMainForm::dirtyNotifySlot (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMainForm::dirtyNotifySlot()");
#endif

	updateDirtyCount(true);
	selectionNotifySlot(nullptr);
}


// Tempo spin-box change slot.
void qtractorMainForm::transportTempoChanged (
	float fTempo, unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportTempoChanged(%g, %u, %u)",
		fTempo, iBeatsPerBar, iBeatDivisor);
#endif

	// Find appropriate node...
	qtractorTimeScale *pTimeScale = m_pSession->timeScale();
	qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_pSession->playHead());

	// Now, express the change as an undoable command...
	m_pSession->execute(new qtractorTimeScaleUpdateNodeCommand(
		pTimeScale, pNode->frame, fTempo, 2, iBeatsPerBar, iBeatDivisor));

	++m_iTransportUpdate;
}


void qtractorMainForm::transportTempoFinished (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportTempoFinished()");
#endif

	const bool bBlockSignals = m_pTempoSpinBox->blockSignals(true);
	m_pTempoSpinBox->clearFocus();
	m_pTempoSpinBox->blockSignals(bBlockSignals);
}


// Snap-per-beat spin-box change slot.
void qtractorMainForm::snapPerBeatChanged ( int iSnap )
{
	// Avoid bogus changes...
	const unsigned short iSnapPerBeat
		= qtractorTimeScale::snapFromIndex(iSnap);
	if (iSnapPerBeat == m_pSession->snapPerBeat())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::snapPerBeatChanged(%u)", iSnapPerBeat);
#endif

	// No need to express this change as a undoable command...
	m_pSession->setSnapPerBeat(iSnapPerBeat);
}


// Time format custom context menu.
void qtractorMainForm::transportTimeFormatChanged ( int iDisplayFormat )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportTimeFormatChanged(%d)", iDisplayFormat);
#endif

	if (m_pOptions) {
		m_pOptions->iDisplayFormat = iDisplayFormat;
		updateDisplayFormat();
	}

	++m_iStabilizeTimer;
}


// Real thing: the playhead has been changed manually!
void qtractorMainForm::transportTimeChanged ( unsigned long iPlayHead )
{
	if (m_iTransportUpdate > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportTimeChanged(%lu)", iPlayHead);
#endif

	m_pSession->setPlayHead(iPlayHead);
	++m_iTransportUpdate;

	++m_iStabilizeTimer;
}

void qtractorMainForm::transportTimeFinished (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMainForm::transportTimeFinished()");
#endif

	const bool bBlockSignals = m_pTimeSpinBox->blockSignals(true);
	m_pTimeSpinBox->clearFocus();
	m_pTimeSpinBox->blockSignals(bBlockSignals);
}


// Tempo-map custom context menu.
void qtractorMainForm::transportTempoContextMenu ( const QPoint& /*pos*/ )
{
	viewTempoMap();
}


// end of qtractorMainForm.cpp
