// qtractorAudioEditorForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioEditorForm.h"

#include "qtractorAudioEditor.h"

#include "qtractorAudioEditView.h"

#include "qtractorAudioClip.h"
#include "qtractorAudioEngine.h"
#include "qtractorAudioMonitor.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorTrackView.h"
#include "qtractorConnections.h"

#include "qtractorMainForm.h"
#include "qtractorShortcutForm.h"
#include "qtractorPasteRepeatForm.h"
#include "qtractorClipForm.h"

#include "qtractorTimeScale.h"
#include "qtractorCommand.h"

#include "qtractorExportForm.h"

#include "qtractorClipCommand.h"
#include "qtractorTimeScaleCommand.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
#include <QWindow>
#endif

#include <QFileDialog>
#include <QMessageBox>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QUrl>

#include <QTimer>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define horizontalAdvance  width
#endif


// Local static consts.
static const char *LayoutDockWindowsKey = "/AudioEditor/Layout/DockWindows";


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Main window form implementation.

// Constructor.
qtractorAudioEditorForm::qtractorAudioEditorForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: qtractorEditorForm(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);
#if QT_VERSION < QT_VERSION_CHECK(6, 1, 0)
	QMainWindow::setWindowIcon(QIcon(":/images/qtractor.png"));
#endif
	m_iDirtyCount = 0;

	// Set our central widget.
	m_pAudioEditor = new qtractorAudioEditor(this);
	setCentralWidget(m_pAudioEditor);

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

	// Transport tempo/time-signature tracker.
	m_pTempoCursor = new qtractorTempoCursor();

	const QSize  pad(4, 0);
	const QFont& font0 = qtractorAudioEditorForm::font();
	const QFont  font(font0.family(), font0.pointSize() + 2);
	const QFontMetrics fm(font);
	const int d = fm.height() + fm.leading() + 8;

	// Transport time.
	const QString sTime("+99:99:99.999");
	m_pTimeSpinBox = new qtractorTimeSpinBox(m_ui.timeToolbar);
	m_pTimeSpinBox->setTimeScale(m_pAudioEditor->timeScale());
	m_pTimeSpinBox->setFont(font);
	m_pTimeSpinBox->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	m_pTimeSpinBox->setMinimumSize(QSize(fm.horizontalAdvance(sTime) + d, d) + pad);
	m_pTimeSpinBox->setPalette(pal);
//	m_pTimeSpinBox->setAutoFillBackground(true);
	m_pTimeSpinBox->setToolTip(tr("Current time (play-head)"));
//	m_pTimeSpinBox->setContextMenuPolicy(Qt::CustomContextMenu);
	m_ui.timeToolbar->addWidget(m_pTimeSpinBox);
//	m_ui.timeToolbar->addSeparator();

	// Time-signature spin-box.
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
	// Pre-fill the combo-boxes...
	m_pSnapPerBeatComboBox->setIconSize(QSize(8, 16));
	snapIter.toFront();
	if (snapIter.hasNext())
		m_pSnapPerBeatComboBox->addItem(
			QIcon::fromTheme("itemNone"), snapIter.next());
	while (snapIter.hasNext())
		m_pSnapPerBeatComboBox->addItem(snapIcon, snapIter.next());
//	m_pSnapPerBeatComboBox->insertItems(0, snapItems);
	// Set combo-boxes tooltips...
	m_pSnapPerBeatComboBox->setToolTip(tr("Snap/beat"));
	m_ui.timeToolbar->addWidget(m_pSnapPerBeatComboBox);

	QStatusBar *pStatusBar = statusBar();

	const QString spc(4, ' ');

	// Status clip/sequence name...
	m_pTrackNameLabel = new QLabel(spc);
	m_pTrackNameLabel->setAlignment(Qt::AlignLeft);
	m_pTrackNameLabel->setMinimumWidth(60);
	m_pTrackNameLabel->setToolTip(tr("Audio clip name"));
	m_pTrackNameLabel->setAutoFillBackground(true);
	pStatusBar->addWidget(m_pTrackNameLabel, 1);

	// Status filename...
	m_pFileNameLabel = new QLabel(spc);
	m_pFileNameLabel->setAlignment(Qt::AlignLeft);
	m_pFileNameLabel->setMinimumWidth(240);
	m_pFileNameLabel->setToolTip(tr("Audio file name"));
	m_pFileNameLabel->setAutoFillBackground(true);
	pStatusBar->addWidget(m_pFileNameLabel, 2);

	// Clip modification status.
	m_pStatusModLabel = new QLabel(tr("MOD"));
	m_pStatusModLabel->setAlignment(Qt::AlignHCenter);
	m_pStatusModLabel->setMinimumSize(m_pStatusModLabel->sizeHint() + pad);
	m_pStatusModLabel->setToolTip(tr("Audio modification state"));
	m_pStatusModLabel->setAutoFillBackground(true);
	pStatusBar->addPermanentWidget(m_pStatusModLabel);

	// Clip mute(ness) status.
	m_pStatusMuteLabel = new QLabel(tr("MUTE"));
	m_pStatusMuteLabel->setAlignment(Qt::AlignHCenter);
	m_pStatusMuteLabel->setMinimumSize(m_pStatusMuteLabel->sizeHint() + pad);
	m_pStatusMuteLabel->setToolTip(tr("Audio clip mute state"));
	m_pStatusMuteLabel->setAutoFillBackground(true);
	pStatusBar->addPermanentWidget(m_pStatusMuteLabel);

	// Sequence duration status.
	m_pDurationLabel = new QLabel(tr("00:00:00.000"));
	m_pDurationLabel->setAlignment(Qt::AlignHCenter);
	m_pDurationLabel->setMinimumSize(m_pDurationLabel->sizeHint() + pad);
	m_pDurationLabel->setToolTip(tr("Audio clip duration"));
	m_pDurationLabel->setAutoFillBackground(true);
	pStatusBar->addPermanentWidget(m_pDurationLabel);

	m_pRedPalette = new QPalette(pStatusBar->palette());
	m_pRedPalette->setColor(QPalette::WindowText, Qt::darkRed);
	m_pRedPalette->setColor(QPalette::Window, Qt::red);

	m_pYellowPalette = new QPalette(pStatusBar->palette());
	m_pYellowPalette->setColor(QPalette::WindowText, Qt::darkYellow);
	m_pYellowPalette->setColor(QPalette::Window, Qt::yellow);

	// Some actions surely need those
	// shortcuts firmly attached...
	addAction(m_ui.viewMenubarAction);
#if 0
	// Special integration ones.
	addAction(m_ui.transportBackwardAction);
	addAction(m_ui.transportLoopAction);
	addAction(m_ui.transportLoopSetAction);
	addAction(m_ui.transportStopAction);
	addAction(m_ui.transportPlayAction);
	addAction(m_ui.transportPunchAction);
	addAction(m_ui.transportPunchSetAction);
#endif

	// Ah, make it stand right.
	setFocus();

	// UI signal/slot connections...
	QObject::connect(m_ui.fileSaveAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSave()));
	QObject::connect(m_ui.fileSaveAsAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSaveAs()));
	QObject::connect(m_ui.fileMuteAction,
		SIGNAL(triggered(bool)),
		SLOT(fileMute()));
	QObject::connect(m_ui.filePropertiesAction,
		SIGNAL(triggered(bool)),
		SLOT(fileProperties()));
	QObject::connect(m_ui.fileRangeSetAction,
		SIGNAL(triggered(bool)),
		SLOT(fileRangeSet()));
	QObject::connect(m_ui.fileLoopSetAction,
		SIGNAL(triggered(bool)),
		SLOT(fileLoopSet()));
	QObject::connect(m_ui.fileTrackInputsAction,
		SIGNAL(triggered(bool)),
		SLOT(fileTrackInputs()));
	QObject::connect(m_ui.fileTrackOutputsAction,
		SIGNAL(triggered(bool)),
		SLOT(fileTrackOutputs()));
	QObject::connect(m_ui.fileTrackPropertiesAction,
		SIGNAL(triggered(bool)),
		SLOT(fileTrackProperties()));
	QObject::connect(m_ui.fileCloseAction,
		SIGNAL(triggered(bool)),
		SLOT(fileClose()));

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
	QObject::connect(m_ui.editPasteRepeatAction,
		SIGNAL(triggered(bool)),
		SLOT(editPasteRepeat()));
	QObject::connect(m_ui.editPasteAction,
		SIGNAL(triggered(bool)),
		SLOT(editPaste()));
	QObject::connect(m_ui.editDeleteAction,
		SIGNAL(triggered(bool)),
		SLOT(editDelete()));
	QObject::connect(m_ui.editSelectAllAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectAll()));
	QObject::connect(m_ui.editSelectNoneAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectNone()));
	QObject::connect(m_ui.editSelectInvertAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectInvert()));
	QObject::connect(m_ui.editSelectRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectRange()));

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
	QObject::connect(m_ui.viewToolbarTransportAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTransport(bool)));
	QObject::connect(m_ui.viewToolbarTimeAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTime(bool)));
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

	QObject::connect(m_ui.transportFollowAction,
		SIGNAL(triggered(bool)),
		SLOT(transportFollow(bool)));

	QObject::connect(m_ui.helpShortcutsAction,
		SIGNAL(triggered(bool)),
		SLOT(helpShortcuts()));
	QObject::connect(m_ui.helpAboutAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAbout()));
	QObject::connect(m_ui.helpAboutQtAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAboutQt()));

	QObject::connect(m_ui.viewZoomMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateZoomMenu()));
	QObject::connect(m_ui.viewSnapMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateSnapMenu()));

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
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(transportTempoChanged(float, unsigned short, unsigned short)));
	QObject::connect(m_pTempoSpinBox,
		SIGNAL(editingFinished()),
		SLOT(transportTempoFinished()));
	QObject::connect(m_pTempoSpinBox,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(transportTempoContextMenu(const QPoint&)));

	QObject::connect(m_pSnapPerBeatComboBox,
		SIGNAL(activated(int)),
		SLOT(snapPerBeatChanged(int)));

	QObject::connect(m_pAudioEditor,
		SIGNAL(selectNotifySignal(QObject *)),
		SLOT(selectionChanged(QObject *)));
	QObject::connect(m_pAudioEditor,
		SIGNAL(changeNotifySignal(QObject *)),
		SLOT(contentsChanged(QObject *)));

	// Try to restore old editor state...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		// Initial decorations toggle state.
		m_ui.viewMenubarAction->setChecked(pOptions->bAudioMenubar);
		m_ui.viewStatusbarAction->setChecked(pOptions->bAudioStatusbar);
		m_ui.viewToolbarFileAction->setChecked(pOptions->bAudioFileToolbar);
		m_ui.viewToolbarEditAction->setChecked(pOptions->bAudioEditToolbar);
		m_ui.viewToolbarTransportAction->setChecked(pOptions->bAudioTransportToolbar);
		m_ui.viewToolbarTimeAction->setChecked(pOptions->bAudioTimeToolbar);
		m_ui.viewToolbarLockedAction->setChecked(pOptions->bAudioLockedToolbar);
		m_ui.transportFollowAction->setChecked(pOptions->bAudioFollow);
		m_ui.viewSnapZebraAction->setChecked(pOptions->bAudioSnapZebra);
		m_ui.viewSnapGridAction->setChecked(pOptions->bAudioSnapGrid);
		m_ui.viewToolTipsAction->setChecked(pOptions->bAudioToolTips);
		// Initial decorations visibility state.
		viewMenubar(pOptions->bAudioMenubar);
		viewStatusbar(pOptions->bAudioStatusbar);
		viewToolbarFile(pOptions->bAudioFileToolbar);
		viewToolbarEdit(pOptions->bAudioEditToolbar);
		viewToolbarTransport(pOptions->bAudioTransportToolbar);
		viewToolbarTime(pOptions->bAudioTimeToolbar);
		viewToolbarLocked(pOptions->bAudioLockedToolbar);
		m_pAudioEditor->setZoomMode(pOptions->iAudioZoomMode);
		m_pAudioEditor->setHorizontalZoom(pOptions->iAudioHorizontalZoom);
		m_pAudioEditor->setVerticalZoom(pOptions->iAudioVerticalZoom);
		m_pAudioEditor->setSnapZebra(pOptions->bAudioSnapZebra);
		m_pAudioEditor->setSnapGrid(pOptions->bAudioSnapGrid);
		m_pAudioEditor->setToolTips(pOptions->bAudioToolTips);
		m_pAudioEditor->setSyncView(pOptions->bAudioFollow);
		// Initial transport display options...
		m_pAudioEditor->setSyncViewHold(pOptions->bSyncViewHold);
		// Default snap-per-beat setting...
		m_pSnapPerBeatComboBox->setCurrentIndex(pOptions->iAudioSnapPerBeat);
		// Restore whole dock windows state.
		QByteArray aDockables = pOptions->settings().value(
			LayoutDockWindowsKey).toByteArray();
		if (!aDockables.isEmpty()) {
			// Make it as the last time.
			restoreState(aDockables);
		}
		// Try to restore old window positioning?
		// pOptions->loadWidgetGeometry(this, true);
		// Load (action) keyboard shortcuts...
		pOptions->loadActionShortcuts(this);
	}

	// Make last-but-not-least connections....
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		QObject::connect(m_ui.transportBackwardAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportBackward()));
		QObject::connect(m_ui.transportRewindAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportRewind()));
		QObject::connect(m_ui.transportFastForwardAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportFastForward()));
		QObject::connect(m_ui.transportForwardAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportForward()));
		QObject::connect(m_ui.transportStepBackwardAction,
			SIGNAL(triggered(bool)),
			SLOT(transportStepBackward()));
		QObject::connect(m_ui.transportStepForwardAction,
			SIGNAL(triggered(bool)),
			SLOT(transportStepForward()));
		QObject::connect(m_ui.transportLoopAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportLoop()));
		QObject::connect(m_ui.transportLoopSetAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportLoopSet()));
		QObject::connect(m_ui.transportStopAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportStop()));
		QObject::connect(m_ui.transportPlayAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportPlay()));
		QObject::connect(m_ui.transportRecordAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportRecord()));
		QObject::connect(m_ui.transportPunchAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportPunch()));
		QObject::connect(m_ui.transportPunchSetAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportPunchSet()));
		// Add to main editors list...
		pMainForm->addEditorForm(this);
	}

	// HACK: Some explicit focus immediately...
	m_pAudioEditor->editView()->setFocus();
}


// Destructor.
qtractorAudioEditorForm::~qtractorAudioEditorForm (void)
{
	// Remove this one from main-form list...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->removeEditorForm(this);

	// View/Snap-to-beat actions termination...
	qDeleteAll(m_snapPerBeatActions);
	m_snapPerBeatActions.clear();

	// Ditch color palettes...
	if (m_pYellowPalette)
		delete m_pYellowPalette;
	if (m_pRedPalette)
		delete m_pRedPalette;

	// Custom time-signature cursor.
	if (m_pTempoCursor)
		delete m_pTempoCursor;
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Window close event handlers.

// Pre-close event handlers.
bool qtractorAudioEditorForm::queryClose (void)
{
	bool bQueryClose = true;
#if 0//--TODO: m_iDirtyCount > 0
	// Are we dirty enough to prompt it?
	if (m_iDirtyCount > 0) {
		if (isVisible()) {
			// Currently visible: save conditionally...
			switch (querySave(filename(), this)) {
			case QMessageBox::Save:
				bQueryClose = saveClipFile(false);
				// Fall thru....
			case QMessageBox::Discard:
				break;
			default:    // Cancel.
				bQueryClose = false;
				break;
			}
		} else {
			// Not currently visible: save unconditionally...
			bQueryClose = saveClipFile(false);
		}
	}
#endif
	return bQueryClose;
}


// Save(as) warning message box.
int qtractorAudioEditorForm::querySave (
	const QString& sFilename, QWidget *pParent )
{
	if (pParent == nullptr)
		pParent = qtractorMainForm::getInstance();

	return (QMessageBox::warning(pParent,
		tr("Warning"),
		tr("The current Audio clip has been changed:\n\n"
		"\"%1\"\n\n"
		"Do you want to save the changes?").arg(sFilename),
		QMessageBox::Save |
		QMessageBox::Discard |
		QMessageBox::Cancel));
}


// On-close event handler.
void qtractorAudioEditorForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	// Try to save current editor view state...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && isVisible()) {
		// Save decorations state.
		pOptions->bAudioMenubar = m_ui.menuBar->isVisible();
		pOptions->bAudioStatusbar = statusBar()->isVisible();
		pOptions->bAudioFileToolbar = m_ui.fileToolbar->isVisible();
		pOptions->bAudioEditToolbar = m_ui.editToolbar->isVisible();
		pOptions->bAudioTransportToolbar = m_ui.transportToolbar->isVisible();
		pOptions->bAudioTimeToolbar = m_ui.timeToolbar->isVisible();
		pOptions->bAudioLockedToolbar = m_ui.viewToolbarLockedAction->isChecked();
		pOptions->iAudioZoomMode = m_pAudioEditor->zoomMode();
		pOptions->iAudioHorizontalZoom = m_pAudioEditor->horizontalZoom();
		pOptions->iAudioVerticalZoom = m_pAudioEditor->verticalZoom();
		pOptions->bAudioSnapZebra = m_pAudioEditor->isSnapZebra();
		pOptions->bAudioSnapGrid = m_pAudioEditor->isSnapGrid();
		pOptions->bAudioToolTips = m_pAudioEditor->isToolTips();
		pOptions->iAudioDisplayFormat = (m_pAudioEditor->timeScale())->displayFormat();
		pOptions->bAudioFollow  = m_ui.transportFollowAction->isChecked();
		// Save snap-per-beat setting...
		pOptions->iAudioSnapPerBeat = m_pSnapPerBeatComboBox->currentIndex();
		// Save the dock windows state.
		pOptions->settings().setValue(LayoutDockWindowsKey, saveState());
		// And this main windows state?
		// pOptions->saveWidgetGeometry(this, true);
	}

	// Close it good.
	pCloseEvent->accept();
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Context menu event handlers.

// Context menu request.
void qtractorAudioEditorForm::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	stabilizeForm();

	// Primordial edit menu should be available...
	m_ui.editMenu->exec(pContextMenuEvent->globalPos());
}


// Edit menu accessor.
QMenu *qtractorAudioEditorForm::editMenu (void) const
{
	return m_ui.editMenu;
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Central widget redirect methods.

// Audio editor widget accessor.
qtractorEditor *qtractorAudioEditorForm::editor (void) const
{
	return m_pAudioEditor;
}


// Editing Audio clip accessors.
qtractorAudioClip *qtractorAudioEditorForm::audioClip (void) const
{
	return m_pAudioEditor->audioClip();
}


// Audio clip property accessors.
const QString& qtractorAudioEditorForm::filename (void) const
{
	return m_pAudioEditor->filename();
}


// Special executive setup method.
void qtractorAudioEditorForm::setup ( qtractorAudioClip *pAudioClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	// Get those time-scales in sync,
	// while keeping zoom ratios persistant...
	const unsigned short iHorizontalZoom = m_pAudioEditor->horizontalZoom();
	const unsigned short iVerticalZoom = m_pAudioEditor->verticalZoom();
	qtractorTimeScale *pTimeScale  = m_pAudioEditor->timeScale();
	pTimeScale->copy(*pSession->timeScale());
	m_pAudioEditor->setHorizontalZoom(iHorizontalZoom);
	m_pAudioEditor->setVerticalZoom(iVerticalZoom);

	// Fix some local options though...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		const qtractorTimeScale::DisplayFormat displayFormat
			= qtractorTimeScale::DisplayFormat(pOptions->iAudioDisplayFormat);
		pTimeScale->setDisplayFormat(displayFormat);
		m_pTimeSpinBox->setDisplayFormat(displayFormat);
	}

	// Reset custom time-sig cursor...
	m_pTempoCursor->clear();

	// Default snap-per-beat setting...
	pTimeScale->setSnapPerBeat(
		qtractorTimeScale::snapFromIndex(
			m_pSnapPerBeatComboBox->currentIndex()));

	// Note that there's two modes for this method:
	// whether pAudioClip is given non-null which means
	// form initialization first setup or else...
	if (pAudioClip) {
		// Set initial Audio clip properties has seen fit...
		m_pAudioEditor->setAudioClip(pAudioClip);
		// Setup connections to main widget...
		QObject::connect(m_pAudioEditor,
			SIGNAL(changeNotifySignal(QObject *)),
			pMainForm, SLOT(changeNotifySlot(QObject *)));
		// Setup for last known top-level window position...
		QPoint wpos = pAudioClip->editorPos();
		if (wpos.isNull() || wpos.x() < 0 || wpos.y() < 0) {
			QWidget *pParent = parentWidget();
			if (pParent == nullptr)
				pParent = pMainForm;
			if (pParent) {
				QRect wrect(geometry());
				wrect.moveCenter(pParent->geometry().center());
				wpos = wrect.topLeft();
			}
		}
		move(wpos);
		// Setup for last known top-level window size...
		const QSize& wsize = pAudioClip->editorSize();
		if (!wsize.isNull() && wsize.isValid())
			resize(wsize);
	#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
		// Setup for top-level window geometry changes...
		QWindow *pWindow = windowHandle();
		if (pWindow) {
			QObject::connect(pWindow,
				SIGNAL(xChanged(int)),
				SLOT(posChanged()));
			QObject::connect(pWindow,
				SIGNAL(yChanged(int)),
				SLOT(posChanged()));
			QObject::connect(pWindow,
				SIGNAL(widthChanged(int)),
				SLOT(sizeChanged()));
			QObject::connect(pWindow,
				SIGNAL(heightChanged(int)),
				SLOT(sizeChanged()));
		}
	#endif
	}

	// Whether we're a initial setup or a second coming...
	const bool bAudioClip = (pAudioClip == nullptr);
	if (bAudioClip)
		pAudioClip = audioClip();

	// Reset local dirty flag.
	resetDirtyCount();

	// Refresh the edit-view...
	m_pAudioEditor->updateContents();

	// (Re)try to reposition the editor in the same relative
	// position in track-view, only if clip is not empty/new...
	qtractorTrack *pTrack = nullptr;
	if (bAudioClip)
		pTrack = pAudioClip->track();
	if (pTrack) {
		const int h1 = pTrack->zoomHeight();
		if (h1 > 0) {
			// Try to recenter horizontally...
			qtractorTrackView *pTrackView = pTracks->trackView();
			const QPoint& pos = pTrackView->mapFromGlobal(QCursor::pos());
			unsigned long iFrame = pSession->frameFromPixel(
				pTrackView->contentsX() + pos.x());
			if (iFrame  > m_pAudioEditor->offset()) {
				iFrame -= m_pAudioEditor->offset();
			} else {
				iFrame = 0;
			}
			qtractorAudioEditView *pEditView = m_pAudioEditor->editView();
			const int w2 = (pEditView->width()  >> 1);
			const int h2 = (pEditView->height() >> 1);
			const int x2 = pTimeScale->pixelFromFrame(iFrame);
			const int cx = pEditView->contentsX();
			const int cy = pEditView->contentsY();
			// Then try to recenter vertically...
			int y2 = cy;
			int y1 = 0;
			qtractorTrack *pTrackEx = pSession->tracks().first();
			while (pTrackEx && pTrackEx != pTrack) {
				y1 += pTrackEx->zoomHeight();
				pTrackEx = pTrackEx->next();
			}
			y1 -= pTrackView->contentsY();
			// Need recentering?...
			if (x2 < cx || x2 > cx + w2 ||
				y2 < cy || y2 > cy + h2) {
				pEditView->setContentsPos(
					(x2 > w2 ? x2 - w2 : 0),
					(y2 > h2 ? y2 - h2 : 0));
			}
		}
	}

	// (Re)sync local play/edit-head/tail (avoid follow playhead)...
	m_pAudioEditor->setPlayHead(pSession->playHead(), false);
	m_pAudioEditor->setEditHead(pSession->editHead(), false);
	m_pAudioEditor->setEditTail(pSession->editTail(), false);

	// Finally update current time position display...
	updatePlayHead(pSession->playHead());

	// Done.
	stabilizeForm();
}


// Reset coomposite dirty flag.
void qtractorAudioEditorForm::resetDirtyCount (void)
{
	m_iDirtyCount = 0;

	// Audio clip might be dirty already.
	qtractorAudioClip *pAudioClip = audioClip();
	if (pAudioClip && pAudioClip->isDirty())
		++m_iDirtyCount;
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Clip action methods.

// Save current clip.
bool qtractorAudioEditorForm::saveClipFile ( bool bPrompt )
{
	qtractorAudioClip *pAudioClip = m_pAudioEditor->audioClip();
	if (pAudioClip == nullptr)
		return false;

	qtractorTrack *pTrack = pAudioClip->track();
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	// Suggest a brand new filename, if there's none...
	QString sFilename = pAudioClip->filename();

	// Ask for the file to save...
	if (sFilename.isEmpty())
		bPrompt = true;

	int iFormat = -1; // alias qtractorAudioFileFactory::defaultFormat();
	if (bPrompt) {
		qtractorExportClipForm exportForm(this);
		exportForm.setExportTitle(tr("Save Audio Clip"));
		exportForm.setExportType(qtractorTrack::Audio);
		const QString& sExt = exportForm.exportExt();
		QString sFilename = pSession->createFilePath(pTrack->shortTrackName(), sExt);
		exportForm.setExportPath(sFilename);
		if (!exportForm.exec())
			return false;
		sFilename = exportForm.exportPath();
		iFormat = exportForm.audioExportFormat();
		// Have we cancelled it?
		if (sFilename.isEmpty() || sFilename.at(0) == '.')
			return false;
		// Enforce .mid extension...
		if (QFileInfo(sFilename).suffix().isEmpty())
			sFilename += '.' + sExt;
	}

	// Save it right away...
	// TODO: ?...
	//
	bool bResult = false;

	// Have we done it right?
	if (bResult) {
		// Aha, but we're not dirty no more.
		m_iDirtyCount = 0;
	}

	// Done.
	stabilizeForm();
	return bResult;
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- File Action slots.

// Save current clip.
void qtractorAudioEditorForm::fileSave (void)
{
	saveClipFile(false);
}


// Save current clip with another name.
void qtractorAudioEditorForm::fileSaveAs (void)
{
	saveClipFile(true);
}


// Mute current clip.
void qtractorAudioEditorForm::fileMute (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->muteClip(m_pAudioEditor->audioClip());
	}
}


// File properties dialog.
void qtractorAudioEditorForm::fileProperties (void)
{
	qtractorAudioClip *pAudioClip = m_pAudioEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	qtractorClipForm clipForm(this);
	clipForm.setClip(pAudioClip);
	clipForm.exec();
}


// Show current Audio clip/track input bus connections.
void qtractorAudioEditorForm::fileTrackInputs (void)
{
	qtractorAudioClip *pAudioClip = m_pAudioEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	qtractorTrack *pTrack = pAudioClip->track();
	if (pTrack == nullptr)
		return;
	if (pTrack->inputBus() == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->connections()) {
		(pMainForm->connections())->showBus(
			pTrack->inputBus(), qtractorBus::Input);
	}
}


// Show current Audio clip/track output bus connections.
void qtractorAudioEditorForm::fileTrackOutputs (void)
{
	qtractorAudioClip *pAudioClip = m_pAudioEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	qtractorTrack *pTrack = pAudioClip->track();
	if (pTrack == nullptr)
		return;
	if (pTrack->outputBus() == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->connections()) {
		(pMainForm->connections())->showBus(
			pTrack->outputBus(), qtractorBus::Output);
	}
}


// Edit current Audio clip/track properties.
void qtractorAudioEditorForm::fileTrackProperties (void)
{
	qtractorAudioClip *pAudioClip = m_pAudioEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	qtractorTrack *pTrack = pAudioClip->track();
	if (pTrack == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->editTrack(pTrack);
	}
}


// Edit-range setting to clip extents.
void qtractorAudioEditorForm::fileRangeSet (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->rangeClip(m_pAudioEditor->audioClip());
	}
}


// Loop-range setting to clip extents.
void qtractorAudioEditorForm::fileLoopSet (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->loopClip(m_pAudioEditor->audioClip());
	}
}


// Exit editing.
void qtractorAudioEditorForm::fileClose (void)
{
	// Go for close this thing.
	close();
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Edit action slots.

// Undo last edit command.
void qtractorAudioEditorForm::editUndo (void)
{
	m_pAudioEditor->undoCommand();
}


// Redo last edit command.
void qtractorAudioEditorForm::editRedo (void)
{
	m_pAudioEditor->redoCommand();
}


// Cut current selection to clipboard.
void qtractorAudioEditorForm::editCut (void)
{
	m_pAudioEditor->cutClipboard();
}


// Copy current selection to clipboard.
void qtractorAudioEditorForm::editCopy (void)
{
	m_pAudioEditor->copyClipboard();
}


// Paste from clipboard.
void qtractorAudioEditorForm::editPaste (void)
{
	m_pAudioEditor->pasteClipboard();
}


// Paste/repeat from clipboard.
void qtractorAudioEditorForm::editPasteRepeat (void)
{
	qtractorPasteRepeatForm pasteForm(this);
	pasteForm.setRepeatPeriod(m_pAudioEditor->pastePeriod());
	if (pasteForm.exec()) {
		m_pAudioEditor->pasteClipboard(
			pasteForm.repeatCount(),
			pasteForm.repeatPeriod()
		);
	}
}


// Delete current selection.
void qtractorAudioEditorForm::editDelete (void)
{
	m_pAudioEditor->deleteSelect();
}


// Select none contents.
void qtractorAudioEditorForm::editSelectNone (void)
{
	m_pAudioEditor->selectAll(m_pAudioEditor->editView(), false, false);
}


// Select invert contents.
void qtractorAudioEditorForm::editSelectInvert (void)
{
	m_pAudioEditor->selectAll(m_pAudioEditor->editView(), true, true);
}


// Select all contents.
void qtractorAudioEditorForm::editSelectAll (void)
{
	m_pAudioEditor->selectAll(m_pAudioEditor->editView(), true, false);
}


// Select contents range.
void qtractorAudioEditorForm::editSelectRange (void)
{
	m_pAudioEditor->selectRange(m_pAudioEditor->editView(), true, true);
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- View Action slots.

// Show/hide the main program window menubar.
void qtractorAudioEditorForm::viewMenubar ( bool bOn )
{
	m_ui.menuBar->setVisible(bOn);
}


// Show/hide the main program window statusbar.
void qtractorAudioEditorForm::viewStatusbar ( bool bOn )
{
	statusBar()->setVisible(bOn);
}


// Show/hide the file-toolbar.
void qtractorAudioEditorForm::viewToolbarFile ( bool bOn )
{
	m_ui.fileToolbar->setVisible(bOn);
}


// Show/hide the edit-toolbar.
void qtractorAudioEditorForm::viewToolbarEdit ( bool bOn )
{
	m_ui.editToolbar->setVisible(bOn);
}


// Show/hide the transport-toolbar.
void qtractorAudioEditorForm::viewToolbarTransport ( bool bOn )
{
	m_ui.transportToolbar->setVisible(bOn);
}


// Show/hide the time-signature toolbar.
void qtractorAudioEditorForm::viewToolbarTime( bool bOn )
{
	m_ui.timeToolbar->setVisible(bOn);
}


// Lock/unlock window toolbar positions.
void qtractorAudioEditorForm::viewToolbarLocked ( bool bOn )
{
	m_ui.fileToolbar->setMovable(!bOn);
	m_ui.editToolbar->setMovable(!bOn);
	m_ui.transportToolbar->setMovable(!bOn);
	m_ui.timeToolbar->setMovable(!bOn);
}


// Horizontal and/or vertical zoom-in.
void qtractorAudioEditorForm::viewZoomIn (void)
{
	m_pAudioEditor->zoomIn();
}


// Horizontal and/or vertical zoom-out.
void qtractorAudioEditorForm::viewZoomOut (void)
{
	m_pAudioEditor->zoomOut();
}


// Reset zoom level to default.
void qtractorAudioEditorForm::viewZoomReset (void)
{
	m_pAudioEditor->zoomReset();
}


// Set horizontal zoom mode
void qtractorAudioEditorForm::viewZoomHorizontal (void)
{
	m_pAudioEditor->setZoomMode(qtractorAudioEditor::ZoomHorizontal);
}


// Set vertical zoom mode
void qtractorAudioEditorForm::viewZoomVertical (void)
{
	m_pAudioEditor->setZoomMode(qtractorAudioEditor::ZoomVertical);
}


// Set all zoom mode
void qtractorAudioEditorForm::viewZoomAll (void)
{
	m_pAudioEditor->setZoomMode(qtractorAudioEditor::ZoomAll);
}


// Set zebra mode
void qtractorAudioEditorForm::viewSnapZebra ( bool bOn )
{
	m_pAudioEditor->setSnapZebra(bOn);
	m_pAudioEditor->updateContents();
}


// Set grid mode
void qtractorAudioEditorForm::viewSnapGrid ( bool bOn )
{
	m_pAudioEditor->setSnapGrid(bOn);
	m_pAudioEditor->updateContents();
}


// Set floating tool-tips view mode
void qtractorAudioEditorForm::viewToolTips ( bool bOn )
{
	m_pAudioEditor->setToolTips(bOn);
}


// Change snap-per-beat setting via menu.
void qtractorAudioEditorForm::viewSnap (void)
{
	// Retrieve snap-per-beat index from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		// Commit the change as usual...
		snapPerBeatChanged(pAction->data().toInt());
		// Update the other toolbar control...
		qtractorTimeScale *pTimeScale = m_pAudioEditor->timeScale();
		if (pTimeScale)
			m_pSnapPerBeatComboBox->setCurrentIndex(
				qtractorTimeScale::indexFromSnap(pTimeScale->snapPerBeat()));
	}
}


// Refresh view display.
void qtractorAudioEditorForm::viewRefresh (void)
{
	m_pTempoCursor->clear();

	m_pAudioEditor->updateContents();

	stabilizeForm();
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Transport Action slots.

// Transport step-backward (local)
void qtractorAudioEditorForm::transportStepBackward (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pAudioEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	unsigned long iPlayHead = pSession->playHead();
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
	m_pAudioEditor->setSyncViewHoldOn(false);
	m_pAudioEditor->setPlayHead(iPlayHead);
	pSession->setPlayHead(iPlayHead);
}


// Transport step-forward (local)
void qtractorAudioEditorForm::transportStepForward (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pAudioEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	unsigned long iPlayHead = pSession->playHead();
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
	m_pAudioEditor->setSyncViewHoldOn(false);
	m_pAudioEditor->setPlayHead(iPlayHead);
	pSession->setPlayHead(iPlayHead);
}


// Transport follow playhead (local)
void qtractorAudioEditorForm::transportFollow ( bool bOn )
{
	m_pAudioEditor->setSyncView(bOn);
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Help Action slots.

// Show (and edit) keyboard shortcuts.
void qtractorAudioEditorForm::helpShortcuts (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	const QList<QAction *>& actions
		= findChildren<QAction *> (QString(), Qt::FindDirectChildrenOnly);
	qtractorShortcutForm shortcutForm(actions, this);
	shortcutForm.setActionControl(nullptr); // Disable Audio Controllers here!
	if (shortcutForm.exec() && shortcutForm.isDirtyActionShortcuts())
		pOptions->saveActionShortcuts(this);
}


// Show information about application program.
void qtractorAudioEditorForm::helpAbout (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->helpAbout();
}

// Show information about the Qt toolkit.
void qtractorAudioEditorForm::helpAboutQt (void)
{
	QMessageBox::aboutQt(this);
}


//-------------------------------------------------------------------------
// qtractorAudioEditorForm -- Main form stabilization.

void qtractorAudioEditorForm::stabilizeForm (void)
{
	// Update the main menu state...
	qtractorTrack *pTrack = nullptr;
	qtractorAudioClip *pAudioClip = m_pAudioEditor->audioClip();
	if (pAudioClip)
		pTrack = pAudioClip->track();
	
	m_ui.fileSaveAction->setEnabled(false);   //--TODO: m_iDirtyCount > 0
	m_ui.fileSaveAsAction->setEnabled(false); //
	m_ui.fileMuteAction->setEnabled(pAudioClip != nullptr);
	m_ui.fileMuteAction->setChecked(pAudioClip && pAudioClip->isClipMute());

	m_ui.fileTrackInputsAction->setEnabled(pTrack && pTrack->inputBus() != nullptr);
	m_ui.fileTrackOutputsAction->setEnabled(pTrack && pTrack->outputBus() != nullptr);
	m_ui.fileTrackPropertiesAction->setEnabled(pTrack != nullptr);
	m_ui.fileRangeSetAction->setEnabled(pTrack != nullptr);
	m_ui.fileLoopSetAction->setEnabled(pTrack != nullptr);

	// Update edit menu state...
	qtractorCommandList *pCommands = m_pAudioEditor->commands();
	if (pCommands) {
		pCommands->updateAction(m_ui.editUndoAction, pCommands->lastCommand());
		pCommands->updateAction(m_ui.editRedoAction, pCommands->nextCommand());
	}

	const bool bSelected = m_pAudioEditor->isSelected();
	const bool bSelectable = m_pAudioEditor->isSelectable();
	const bool bClipboard = m_pAudioEditor->isClipboard();
	m_ui.editCutAction->setEnabled(bSelected);
	m_ui.editCopyAction->setEnabled(bSelected);
	m_ui.editPasteAction->setEnabled(bClipboard);
	m_ui.editPasteRepeatAction->setEnabled(bClipboard);
	m_ui.editDeleteAction->setEnabled(bSelected);
	m_ui.editSelectNoneAction->setEnabled(bSelected);

	// Update the main window caption...
	QString sTitle = QFileInfo(filename()).fileName() + ' ';
	if (m_iDirtyCount > 0)
		sTitle += ' ' + tr("[modified]");
	setWindowTitle(sTitle);

	m_pTrackNameLabel->setText(pTrack->trackName().simplified());
	m_pFileNameLabel->setText(filename());

	if (m_iDirtyCount > 0)
		m_pStatusModLabel->setText(tr("MOD"));
	else
		m_pStatusModLabel->clear();

	if ((pAudioClip && pAudioClip->isClipMute())
		|| (pTrack && pTrack->isMute())) {
		m_pStatusMuteLabel->setText(tr("MUTE"));
		m_pStatusMuteLabel->setPalette(*m_pYellowPalette);
	} else {
		m_pStatusMuteLabel->clear();
		m_pStatusMuteLabel->setPalette(statusBar()->palette());
	}

	qtractorTimeScale *pTimeScale = m_pAudioEditor->timeScale();
	m_pDurationLabel->setText(pTimeScale->textFromFrame(
		m_pAudioEditor->offset(), true, m_pAudioEditor->length()));

	qtractorSession  *pSession  = qtractorSession::getInstance();
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pSession && pMainForm) {
		const unsigned long iPlayHead = pSession->playHead();
		const bool bPlaying   = pSession->isPlaying();
		const bool bRecording = pSession->isRecording();
		const bool bPunching  = pSession->isPunching();
		const bool bLooping   = pSession->isLooping();
		const bool bRolling   = (bPlaying && bRecording);
		const bool bBumped    = (!bRolling && (iPlayHead > 0 || bPlaying));
		const int iRolling = pMainForm->rolling();
		m_ui.transportBackwardAction->setEnabled(bBumped);
		m_ui.transportRewindAction->setEnabled(bBumped);
		m_ui.transportFastForwardAction->setEnabled(!bRolling);
		m_ui.transportForwardAction->setEnabled(
			!bRolling && (iPlayHead < pSession->sessionEnd()
				|| iPlayHead < pSession->editHead()
				|| iPlayHead < pSession->editTail()));
		m_ui.transportStepBackwardAction->setEnabled(bBumped);
		m_ui.transportStepForwardAction->setEnabled(!bRolling);
		m_ui.transportLoopAction->setEnabled(
			!bRolling && (bLooping || bSelectable));
		m_ui.transportLoopSetAction->setEnabled(
			!bRolling && bSelectable);
		m_ui.transportStopAction->setEnabled(bPlaying);
		m_ui.transportRecordAction->setEnabled(pSession->recordTracks() > 0);
		m_ui.transportPunchAction->setEnabled(bPunching || bSelectable);
		m_ui.transportPunchSetAction->setEnabled(bSelectable);
		m_ui.transportRewindAction->setChecked(iRolling < 0);
		m_ui.transportFastForwardAction->setChecked(iRolling > 0);
		m_ui.transportLoopAction->setChecked(bLooping);
		m_ui.transportPlayAction->setChecked(bPlaying);
		m_ui.transportRecordAction->setChecked(bRecording);
		m_ui.transportPunchAction->setChecked(bPunching);
		// Special record mode settlement.
		m_pTimeSpinBox->setReadOnly(bRecording);
		m_pTempoSpinBox->setReadOnly(bRecording);
		// Check whether the clip is currently in loop-set...
		if (pAudioClip) {
			const unsigned long iClipStart
				= pAudioClip->clipStart();
			const unsigned long iClipEnd
				= iClipStart + pAudioClip->clipLength();
			m_ui.fileLoopSetAction->setChecked(bLooping
				&& iClipStart == pSession->loopStart()
				&& iClipEnd   == pSession->loopEnd());
		}
	}
}


// Update thumb-view play-head...
void qtractorAudioEditorForm::updatePlayHead ( unsigned long iPlayHead )
{
	m_pTimeSpinBox->setValue(iPlayHead, false);

	// Tricky stuff: node's non-null iif tempo changes...
	qtractorTimeScale *pTimeScale = m_pAudioEditor->timeScale();
	qtractorTimeScale::Node *pNode
		= m_pTempoCursor->seek(pTimeScale, iPlayHead);
	if (pNode) {
		m_pTempoSpinBox->setTempo(pNode->tempo, false);
		unsigned short iBeatsPerBar = pTimeScale->beatsPerBar2();
		if (iBeatsPerBar < 1)
			iBeatsPerBar = pNode->beatsPerBar;
		m_pTempoSpinBox->setBeatsPerBar(iBeatsPerBar, false);
		unsigned short iBeatDivisor = pTimeScale->beatDivisor2();
		if (iBeatDivisor < 1)
			iBeatDivisor = pNode->beatDivisor;
		m_pTempoSpinBox->setBeatDivisor(iBeatDivisor, false);
	}
}


// Update local time-scale...
void qtractorAudioEditorForm::updateTimeScale (void)
{
	m_pTempoCursor->clear();

	m_pAudioEditor->updateTimeScale();
	m_pAudioEditor->updateContents();
}


// Zoom view menu stabilizer.
void qtractorAudioEditorForm::updateZoomMenu (void)
{
	const int iZoomMode = m_pAudioEditor->zoomMode();

	m_ui.viewZoomHorizontalAction->setChecked(
		iZoomMode == qtractorAudioEditor::ZoomHorizontal);
	m_ui.viewZoomVerticalAction->setChecked(
		iZoomMode == qtractorAudioEditor::ZoomVertical);
	m_ui.viewZoomAllAction->setChecked(
		iZoomMode == qtractorAudioEditor::ZoomAll);
}
 
 
// Snap-per-beat view menu builder.
void qtractorAudioEditorForm::updateSnapMenu (void)
{
	m_ui.viewSnapMenu->clear();

	qtractorTimeScale *pTimeScale = m_pAudioEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	const int iSnapCurrent
		= qtractorTimeScale::indexFromSnap(pTimeScale->snapPerBeat());

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


// Time format custom context menu.
void qtractorAudioEditorForm::transportTimeFormatChanged ( int iDisplayFormat )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioEditorForm::transportTimeFormatChanged(%d)", iDisplayFormat);
#endif

	const qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	(m_pAudioEditor->timeScale())->setDisplayFormat(displayFormat);
	m_pTimeSpinBox->setDisplayFormat(displayFormat);

	stabilizeForm();
}


// Real thing: the playhead has been changed manually!
void qtractorAudioEditorForm::transportTimeChanged ( unsigned long iPlayHead )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioEditorForm::transportTimeChanged(%lu)", iPlayHead);
#endif

	m_pAudioEditor->setPlayHead(iPlayHead);
	pSession->setPlayHead(iPlayHead);
}

void qtractorAudioEditorForm::transportTimeFinished (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioEditorForm::transportTimeFinished()");
#endif

	const bool bBlockSignals = m_pTimeSpinBox->blockSignals(true);
	m_pTimeSpinBox->clearFocus();
	m_pTimeSpinBox->blockSignals(bBlockSignals);
}


// Time-signature spin-box change slot.
void qtractorAudioEditorForm::transportTempoChanged (
	float fTempo, unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioEditorForm::transportTempoChanged(%g, %u, %u)",
		fTempo, iBeatsPerBar, iBeatDivisor);
#endif

	// Find appropriate node...
	qtractorTimeScale *pTimeScale = pSession->timeScale();
	qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pSession->playHead());

	// Now, express the change as an undoable command...
	m_pAudioEditor->execute(
		new qtractorTimeScaleUpdateNodeCommand(
			pTimeScale, pNode->frame, fTempo, 2, iBeatsPerBar, iBeatDivisor));
}


void qtractorAudioEditorForm::transportTempoFinished (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioEditorForm::transportTempoFinished()");
#endif

	const bool bBlockSignals = m_pTempoSpinBox->blockSignals(true);
	m_pTempoSpinBox->clearFocus();
	m_pTempoSpinBox->blockSignals(bBlockSignals);
}


// Time-signature custom context menu.
void qtractorAudioEditorForm::transportTempoContextMenu ( const QPoint& /*pos*/ )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->viewTempoMap();
}


// Snap-per-beat spin-box change slot.
void qtractorAudioEditorForm::snapPerBeatChanged ( int iSnap )
{
	qtractorTimeScale *pTimeScale = m_pAudioEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	// Avoid bogus changes...
	const unsigned short iSnapPerBeat = qtractorTimeScale::snapFromIndex(iSnap);
	if (iSnapPerBeat == pTimeScale->snapPerBeat())
		return;

	// No need to express the change as a undoable command?
	pTimeScale->setSnapPerBeat(iSnapPerBeat);

	// If showing grid, it changed a bit for sure...
	if (m_pAudioEditor->isSnapGrid() || m_pAudioEditor->isSnapZebra())
		m_pAudioEditor->updateContents();

	m_pAudioEditor->editView()->setFocus();
}


void qtractorAudioEditorForm::selectionChanged ( QObject *pSender )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->selectNotifySlot(qobject_cast<qtractorAudioEditor *> (pSender));

	stabilizeForm();
}


void qtractorAudioEditorForm::contentsChanged ( QObject *pSender )
{
	++m_iDirtyCount;

	m_pTempoCursor->clear();

	selectionChanged(qobject_cast<qtractorAudioEditor *> (pSender));
}


// Top-level window geometry related slots.
void qtractorAudioEditorForm::posChanged (void)
{
	if (isVisible()) {
		qtractorAudioClip *pAudioClip = audioClip();
		if (pAudioClip)
			pAudioClip->setEditorPos(pos());
	}
}


void qtractorAudioEditorForm::sizeChanged (void)
{
	if (isVisible()) {
		qtractorAudioClip *pAudioClip = audioClip();
		if (pAudioClip)
			pAudioClip->setEditorSize(size());
	}
}


// end of qtractorAudioEditorForm.cpp

