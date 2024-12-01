// qtractorMidiEditorForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEditorForm.h"
#include "qtractorMidiEditView.h"
#include "qtractorMidiEditList.h"
#include "qtractorMidiEditEvent.h"

#include "qtractorMidiControlTypeGroup.h"

#include "qtractorMidiClip.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"

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

#include "qtractorMidiThumbView.h"
#include "qtractorMidiEventList.h"
#include "qtractorInstrumentMenu.h"
#include "qtractorFileList.h"

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

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define horizontalAdvance  width
#endif


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Main window form implementation.

// Constructor.
qtractorMidiEditorForm::qtractorMidiEditorForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QMainWindow(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);
#if QT_VERSION < QT_VERSION_CHECK(6, 1, 0)
	QMainWindow::setWindowIcon(QIcon(":/images/qtractor.png"));
#endif
	m_iDirtyCount = 0;

	// Set our central widget.
	m_pMidiEditor = new qtractorMidiEditor(this);
	setCentralWidget(m_pMidiEditor);

	// Toolbar thumbnail view...
	m_ui.thumbViewToolbar->addWidget(m_pMidiEditor->thumbView());
	m_ui.thumbViewToolbar->setAllowedAreas(
		Qt::TopToolBarArea | Qt::BottomToolBarArea);

	// Dockable widgets time.
	m_pMidiEventList = new qtractorMidiEventList(this);
	m_pMidiEventList->setEditor(m_pMidiEditor);
	m_pMidiEventList->hide(); // Initially hidden.

	// Custom track/instrument proxy menu.
	m_pInstrumentMenu = new qtractorInstrumentMenu(this);

	// Set edit-mode action group up...
	m_pEditModeActionGroup = new QActionGroup(this);
	m_pEditModeActionGroup->setExclusive(true);
	m_pEditModeActionGroup->addAction(m_ui.editModeOnAction);
	m_pEditModeActionGroup->addAction(m_ui.editModeOffAction);
	m_pEditModeActionGroup->addAction(m_ui.editModeDrawAction);

	// And the corresponding tool-button drop-down menu...
	m_pEditModeToolButton = new QToolButton(this);
	m_pEditModeToolButton->setPopupMode(QToolButton::InstantPopup);
	m_pEditModeToolButton->setMenu(m_ui.editModeMenu);

	// Add/insert this on its proper place in the edit-toobar...
	m_ui.editToolbar->addSeparator();
	m_ui.editToolbar->addWidget(m_pEditModeToolButton);

	QObject::connect(
		m_pEditModeActionGroup, SIGNAL(triggered(QAction*)),
		m_pEditModeToolButton, SLOT(setDefaultAction(QAction*)));

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
	const QFont& font0 = qtractorMidiEditorForm::font();
	const QFont  font(font0.family(), font0.pointSize() + 2);
	const QFontMetrics fm(font);
	const int d = fm.height() + fm.leading() + 8;

	// Transport time.
	const QString sTime("+99:99:99.999");
	m_pTimeSpinBox = new qtractorTimeSpinBox(m_ui.timeToolbar);
	m_pTimeSpinBox->setTimeScale(m_pMidiEditor->timeScale());
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
//	m_ui.timeToolbar->addSeparator();
	m_pTimeSig2ResetButton = new QToolButton(m_ui.timeToolbar);
	const int h1 = m_pTempoSpinBox->sizeHint().height();
	m_pTimeSig2ResetButton->setMaximumSize(h1, h1);
	m_pTimeSig2ResetButton->setIcon(QIcon::fromTheme("itemReset"));
	m_pTimeSig2ResetButton->setToolTip(tr("Reset time-sig."));
	m_ui.timeToolbar->addWidget(m_pTimeSig2ResetButton);

	// Snap-per-beat combo-box.
	m_pSnapPerBeatComboBox = new QComboBox(m_ui.viewToolbar);
	m_pSnapPerBeatComboBox->setEditable(false);

	// Event type selection widgets...
	m_pViewTypeComboBox = new QComboBox(m_ui.editViewToolbar);
	m_pViewTypeComboBox->setEditable(false);

	m_pEventTypeComboBox = new QComboBox(m_ui.editEventToolbar);
	m_pEventTypeComboBox->setEditable(false);
	m_pEventParamComboBox = new QComboBox(m_ui.editEventToolbar);
//	m_pEventParamComboBox->setEditable(false);
	m_pEventParamComboBox->setMinimumWidth(220);

	m_pEventTypeGroup = new qtractorMidiControlTypeGroup(
		m_pMidiEditor, m_pEventTypeComboBox, m_pEventParamComboBox);

	// Snap-to-scale/quantize selection widgets...
	m_pSnapToScaleKeyComboBox = new QComboBox(m_ui.snapToScaleToolbar);
	m_pSnapToScaleKeyComboBox->setEditable(false);
	m_pSnapToScaleKeyComboBox->setMinimumWidth(86);
	m_pSnapToScaleTypeComboBox = new QComboBox(m_ui.snapToScaleToolbar);
	m_pSnapToScaleKeyComboBox->setEditable(false);

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

	const QIcon& icon = QIcon::fromTheme("itemProperty");

	m_pViewTypeComboBox->addItem(icon,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NOTEON),
		int(qtractorMidiEvent::NOTEON));
	m_pViewTypeComboBox->addItem(icon,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::KEYPRESS),
		int(qtractorMidiEvent::KEYPRESS));

	// Special control event names and stuff.
	m_pEventTypeComboBox->setItemText(0, tr("Note Velocity")); // NOTEON
	m_pEventTypeComboBox->removeItem(1); // NOTEOFF

	// Snap-to-scale/quantize selection widgets...
	QStringListIterator iter(qtractorMidiEditor::scaleKeyNames());
	while (iter.hasNext())
		m_pSnapToScaleKeyComboBox->addItem(icon, iter.next());
	m_pSnapToScaleTypeComboBox->insertItems(0,
		qtractorMidiEditor::scaleTypeNames());

//	updateInstrumentNames();

	// Set combo-boxes tooltips...
	m_pSnapPerBeatComboBox->setToolTip(tr("Snap/beat"));
	m_pViewTypeComboBox->setToolTip(tr("Note type"));
	m_pEventTypeComboBox->setToolTip(tr("Value type"));
	m_pEventParamComboBox->setToolTip(tr("Parameter type"));
	m_pSnapToScaleKeyComboBox->setToolTip(tr("Scale key"));
	m_pSnapToScaleTypeComboBox->setToolTip(tr("Scale type"));

	// Late view/note type menu...
	const QString sViewTypeObjectName("viewNoteType%1");
	const QString sViewTypeStatusTip("Set note type to %1");
	const int iViewTypeCount = m_pViewTypeComboBox->count();
	for (int iIndex = 0; iIndex < iViewTypeCount; ++iIndex) {
		const QString& sViewTypeText = m_pViewTypeComboBox->itemText(iIndex);
		QAction *pAction = new QAction(sViewTypeText, this);
		pAction->setObjectName(sViewTypeObjectName.arg(iIndex));
		pAction->setStatusTip(sViewTypeStatusTip.arg(sViewTypeText));
		pAction->setCheckable(true);
		pAction->setData(iIndex);
		QObject::connect(pAction,
			SIGNAL(triggered(bool)),
			SLOT(viewNoteType()));
		m_ui.viewNoteTypeMenu->addAction(pAction);
	}

	// Late event/type type menu...
	const QString sEventTypeObjectName("viewValueType%1");
	const QString sEventTypeStatusTip("Set value type to %1");
	const int iEventTypeCount = m_pEventTypeComboBox->count();
	for (int iIndex = 0; iIndex < iEventTypeCount; ++iIndex) {
		const QString& sEventTypeText = m_pEventTypeComboBox->itemText(iIndex);
		QAction *pAction = new QAction(sEventTypeText, this);
		pAction->setObjectName(sEventTypeObjectName.arg(iIndex));
		pAction->setStatusTip(sEventTypeStatusTip.arg(sEventTypeText));
		pAction->setCheckable(true);
		pAction->setData(iIndex);
		QObject::connect(pAction,
			SIGNAL(triggered(bool)),
			SLOT(viewValueType()));
		m_ui.viewValueTypeMenu->addAction(pAction);
	}

	// Add combo-boxes to toolbars...
	m_ui.viewToolbar->addSeparator();
	m_ui.viewToolbar->addWidget(m_pSnapPerBeatComboBox);

	m_ui.editViewToolbar->addWidget(m_pViewTypeComboBox);

	m_ui.editEventToolbar->addWidget(m_pEventTypeComboBox);
	m_ui.editEventToolbar->addSeparator();
	m_ui.editEventToolbar->addWidget(m_pEventParamComboBox);

	m_ui.snapToScaleToolbar->addWidget(m_pSnapToScaleKeyComboBox);
	m_ui.snapToScaleToolbar->addSeparator();
	m_ui.snapToScaleToolbar->addWidget(m_pSnapToScaleTypeComboBox);

	QStatusBar *pStatusBar = statusBar();

	const QString spc(4, ' ');

	// Status clip/sequence name...
	m_pTrackNameLabel = new QLabel(spc);
	m_pTrackNameLabel->setAlignment(Qt::AlignLeft);
	m_pTrackNameLabel->setMinimumWidth(60);
	m_pTrackNameLabel->setToolTip(tr("MIDI clip name"));
	m_pTrackNameLabel->setAutoFillBackground(true);
	pStatusBar->addWidget(m_pTrackNameLabel, 1);

	// Status filename...
	m_pFileNameLabel = new QLabel(spc);
	m_pFileNameLabel->setAlignment(Qt::AlignLeft);
	m_pFileNameLabel->setMinimumWidth(240);
	m_pFileNameLabel->setToolTip(tr("MIDI file name"));
	m_pFileNameLabel->setAutoFillBackground(true);
	pStatusBar->addWidget(m_pFileNameLabel, 2);

	// Status track/channel number...
	m_pTrackChannelLabel = new QLabel(spc);
	m_pTrackChannelLabel->setAlignment(Qt::AlignHCenter);
	m_pTrackChannelLabel->setMinimumWidth(60);
	m_pTrackChannelLabel->setToolTip(tr("MIDI track/channel"));
	m_pTrackChannelLabel->setAutoFillBackground(true);
	pStatusBar->addWidget(m_pTrackChannelLabel);

	// Clip modification status.
	m_pStatusModLabel = new QLabel(tr("MOD"));
	m_pStatusModLabel->setAlignment(Qt::AlignHCenter);
	m_pStatusModLabel->setMinimumSize(m_pStatusModLabel->sizeHint() + pad);
	m_pStatusModLabel->setToolTip(tr("MIDI modification state"));
	m_pStatusModLabel->setAutoFillBackground(true);
	pStatusBar->addPermanentWidget(m_pStatusModLabel);

	// Clip recording status.
	m_pStatusRecLabel = new QLabel(tr("REC"));
	m_pStatusRecLabel->setAlignment(Qt::AlignHCenter);
	m_pStatusRecLabel->setMinimumSize(m_pStatusRecLabel->sizeHint() + pad);
	m_pStatusRecLabel->setToolTip(tr("MIDI clip record state"));
	m_pStatusRecLabel->setAutoFillBackground(true);
	pStatusBar->addPermanentWidget(m_pStatusRecLabel);

	// Clip mute(ness) status.
	m_pStatusMuteLabel = new QLabel(tr("MUTE"));
	m_pStatusMuteLabel->setAlignment(Qt::AlignHCenter);
	m_pStatusMuteLabel->setMinimumSize(m_pStatusMuteLabel->sizeHint() + pad);
	m_pStatusMuteLabel->setToolTip(tr("MIDI clip mute state"));
	m_pStatusMuteLabel->setAutoFillBackground(true);
	pStatusBar->addPermanentWidget(m_pStatusMuteLabel);

	// Sequence duration status.
	m_pDurationLabel = new QLabel(tr("00:00:00.000"));
	m_pDurationLabel->setAlignment(Qt::AlignHCenter);
	m_pDurationLabel->setMinimumSize(m_pDurationLabel->sizeHint() + pad);
	m_pDurationLabel->setToolTip(tr("MIDI clip duration"));
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
	// Make those primordially docked...
	addDockWidget(Qt::RightDockWidgetArea, m_pMidiEventList, Qt::Vertical);

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
	QObject::connect(m_ui.fileUnlinkAction,
		SIGNAL(triggered(bool)),
		SLOT(fileUnlink()));
	QObject::connect(m_ui.fileRecordExAction,
		SIGNAL(triggered(bool)),
		SLOT(fileRecordEx(bool)));
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
	QObject::connect(m_ui.editModeOnAction,
		SIGNAL(triggered(bool)),
		SLOT(editModeOn()));
	QObject::connect(m_ui.editModeOffAction,
		SIGNAL(triggered(bool)),
		SLOT(editModeOff()));
	QObject::connect(m_ui.editModeDrawAction,
		SIGNAL(triggered(bool)),
		SLOT(editModeDraw(bool)));
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
	QObject::connect(m_ui.editInsertRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editInsertRange()));
	QObject::connect(m_ui.editInsertStepAction,
		SIGNAL(triggered(bool)),
		SLOT(editInsertStep()));
	QObject::connect(m_ui.editRemoveRangeAction,
		SIGNAL(triggered(bool)),
		SLOT(editRemoveRange()));

	QObject::connect(m_ui.toolsQuantizeAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsQuantize()));
	QObject::connect(m_ui.toolsTransposeAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsTranspose()));
	QObject::connect(m_ui.toolsNormalizeAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsNormalize()));
	QObject::connect(m_ui.toolsRandomizeAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsRandomize()));
	QObject::connect(m_ui.toolsResizeAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsResize()));
	QObject::connect(m_ui.toolsRescaleAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsRescale()));
	QObject::connect(m_ui.toolsTimeshiftAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsTimeshift()));
	QObject::connect(m_ui.toolsTemporampAction,
		SIGNAL(triggered(bool)),
		SLOT(toolsTemporamp()));

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
	QObject::connect(m_ui.viewToolbarViewAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarView(bool)));
	QObject::connect(m_ui.viewToolbarTransportAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTransport(bool)));
	QObject::connect(m_ui.viewToolbarTimeAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarTime(bool)));
	QObject::connect(m_ui.viewToolbarScaleAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarScale(bool)));
	QObject::connect(m_ui.viewToolbarThumbAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbarThumb(bool)));
	QObject::connect(m_ui.viewEventsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewEvents(bool)));
	QObject::connect(m_ui.viewNoteNamesAction,
		SIGNAL(triggered(bool)),
		SLOT(viewNoteNames(bool)));
	QObject::connect(m_ui.viewNoteDurationAction,
		SIGNAL(triggered(bool)),
		SLOT(viewNoteDuration(bool)));
	QObject::connect(m_ui.viewDrumModeAction,
		SIGNAL(triggered(bool)),
		SLOT(viewDrumMode(bool)));
	QObject::connect(m_ui.viewNoteColorAction,
		SIGNAL(triggered(bool)),
		SLOT(viewNoteColor(bool)));
	QObject::connect(m_ui.viewValueColorAction,
		SIGNAL(triggered(bool)),
		SLOT(viewValueColor(bool)));
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
	QObject::connect(m_ui.viewPreviewAction,
		SIGNAL(triggered(bool)),
		SLOT(viewPreview(bool)));
	QObject::connect(m_ui.viewFollowAction,
		SIGNAL(triggered(bool)),
		SLOT(viewFollow(bool)));

	QObject::connect(m_ui.helpShortcutsAction,
		SIGNAL(triggered(bool)),
		SLOT(helpShortcuts()));
	QObject::connect(m_ui.helpAboutAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAbout()));
	QObject::connect(m_ui.helpAboutQtAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAboutQt()));

	QObject::connect(m_ui.fileTrackInstrumentMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateTrackInstrumentMenu()));
	QObject::connect(m_ui.viewNoteTypeMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateNoteTypeMenu()));
	QObject::connect(m_ui.viewValueTypeMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateValueTypeMenu()));
	QObject::connect(m_ui.viewGhostTrackMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateGhostTrackMenu()));
	QObject::connect(m_ui.viewZoomMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateZoomMenu()));
	QObject::connect(m_ui.viewSnapMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateSnapMenu()));
	QObject::connect(m_ui.viewScaleMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateScaleMenu()));

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

	QObject::connect(m_pTimeSig2ResetButton,
		SIGNAL(clicked()),
		SLOT(timeSig2ResetClicked()));

	QObject::connect(m_pSnapPerBeatComboBox,
		SIGNAL(activated(int)),
		SLOT(snapPerBeatChanged(int)));

	// To handle event selection changes.
	QObject::connect(m_pViewTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(viewTypeChanged(int)));
	QObject::connect(m_pEventTypeGroup,
		SIGNAL(controlTypeChanged(int)),
		SLOT(eventTypeChanged(int)));
	QObject::connect(m_pEventTypeGroup,
		SIGNAL(controlParamChanged(int)),
		SLOT(eventParamChanged(int)));

	QObject::connect(m_pSnapToScaleKeyComboBox,
		SIGNAL(activated(int)),
		SLOT(snapToScaleKeyChanged(int)));
	QObject::connect(m_pSnapToScaleTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(snapToScaleTypeChanged(int)));

	QObject::connect(m_pMidiEditor,
		SIGNAL(selectNotifySignal(qtractorMidiEditor *)),
		SLOT(selectionChanged(qtractorMidiEditor *)));
	QObject::connect(m_pMidiEditor,
		SIGNAL(changeNotifySignal(qtractorMidiEditor *)),
		SLOT(contentsChanged(qtractorMidiEditor *)));

	QObject::connect(m_pMidiEventList->toggleViewAction(),
		SIGNAL(triggered(bool)),
		SLOT(stabilizeForm()));

	// Try to restore old editor state...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		// Initial decorations toggle state.
		m_ui.viewMenubarAction->setChecked(pOptions->bMidiMenubar);
		m_ui.viewStatusbarAction->setChecked(pOptions->bMidiStatusbar);
		m_ui.viewToolbarFileAction->setChecked(pOptions->bMidiFileToolbar);
		m_ui.viewToolbarEditAction->setChecked(pOptions->bMidiEditToolbar);
		m_ui.viewToolbarViewAction->setChecked(pOptions->bMidiViewToolbar);
		m_ui.viewToolbarTransportAction->setChecked(pOptions->bMidiTransportToolbar);
		m_ui.viewToolbarTimeAction->setChecked(pOptions->bMidiTimeToolbar);
		m_ui.viewToolbarScaleAction->setChecked(pOptions->bMidiScaleToolbar);
		m_ui.viewToolbarThumbAction->setChecked(pOptions->bMidiThumbToolbar);
		m_ui.viewNoteNamesAction->setChecked(pOptions->bMidiNoteNames);
		m_ui.viewNoteDurationAction->setChecked(pOptions->bMidiNoteDuration);
		m_ui.viewNoteColorAction->setChecked(pOptions->bMidiNoteColor);
		m_ui.viewValueColorAction->setChecked(pOptions->bMidiValueColor);
		m_ui.viewPreviewAction->setChecked(pOptions->bMidiPreview);
		m_ui.viewFollowAction->setChecked(pOptions->bMidiFollow);
		m_ui.viewSnapZebraAction->setChecked(pOptions->bMidiSnapZebra);
		m_ui.viewSnapGridAction->setChecked(pOptions->bMidiSnapGrid);
		m_ui.viewToolTipsAction->setChecked(pOptions->bMidiToolTips);
		if (pOptions->bMidiEditMode)
			m_ui.editModeOnAction->setChecked(true);
		else
			m_ui.editModeOffAction->setChecked(true);
		m_ui.editModeDrawAction->setChecked(
			pOptions->bMidiEditMode && pOptions->bMidiEditModeDraw);
		// Set initial edit mode...
		m_pEditModeToolButton->setDefaultAction(
			m_pEditModeActionGroup->checkedAction());
		// Initial decorations visibility state.
		viewMenubar(pOptions->bMidiMenubar);
		viewStatusbar(pOptions->bMidiStatusbar);
		viewToolbarFile(pOptions->bMidiFileToolbar);
		viewToolbarEdit(pOptions->bMidiEditToolbar);
		viewToolbarView(pOptions->bMidiViewToolbar);
		viewToolbarTransport(pOptions->bMidiTransportToolbar);
		viewToolbarTime(pOptions->bMidiTimeToolbar);
		viewToolbarScale(pOptions->bMidiScaleToolbar);
		viewToolbarThumb(pOptions->bMidiThumbToolbar);
		m_pMidiEditor->setZoomMode(pOptions->iMidiZoomMode);
		m_pMidiEditor->setHorizontalZoom(pOptions->iMidiHorizontalZoom);
		m_pMidiEditor->setVerticalZoom(pOptions->iMidiVerticalZoom);
		m_pMidiEditor->setSnapZebra(pOptions->bMidiSnapZebra);
		m_pMidiEditor->setSnapGrid(pOptions->bMidiSnapGrid);
		m_pMidiEditor->setToolTips(pOptions->bMidiToolTips);
		m_pMidiEditor->setEditMode(pOptions->bMidiEditMode);
		m_pMidiEditor->setEditModeDraw(pOptions->bMidiEditModeDraw);
		m_pMidiEditor->setNoteColor(pOptions->bMidiNoteColor);
		m_pMidiEditor->setValueColor(pOptions->bMidiValueColor);
		m_pMidiEditor->setNoteNames(pOptions->bMidiNoteNames);
		m_pMidiEditor->setNoteDuration(pOptions->bMidiNoteDuration);
		m_pMidiEditor->setSendNotes(pOptions->bMidiPreview);
		m_pMidiEditor->setSyncView(pOptions->bMidiFollow);
		m_pMidiEditor->setSnapToScaleKey(pOptions->iMidiSnapToScaleKey);
		m_pMidiEditor->setSnapToScaleType(pOptions->iMidiSnapToScaleType);
		// Initial transport display options...
		m_pMidiEditor->setSyncViewHold(pOptions->bSyncViewHold);
		// Default snap-per-beat setting...
		m_pSnapPerBeatComboBox->setCurrentIndex(pOptions->iMidiSnapPerBeat);
		// Default snap-to-scale settings...
		m_pSnapToScaleKeyComboBox->setCurrentIndex(pOptions->iMidiSnapToScaleKey);
		m_pSnapToScaleTypeComboBox->setCurrentIndex(pOptions->iMidiSnapToScaleType);
		// Restore whole dock windows state.
		QByteArray aDockables = pOptions->settings().value(
			"/MidiEditor/Layout/DockWindows").toByteArray();
		if (aDockables.isEmpty()) {
			// Some windows are forced initially as is...
			insertToolBarBreak(m_ui.editViewToolbar);
		} else {
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
		QObject::connect(m_ui.transportPanicAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportPanic()));
		// Add to main editors list...
		pMainForm->addEditorForm(this);
	}

	// Finally set initial editor type-params...
	if (pOptions) {
		m_pViewTypeComboBox->setCurrentIndex(pOptions->iMidiViewType);
		const qtractorMidiControl::ControlType ctype
			= m_pEventTypeGroup->controlTypeFromIndex(pOptions->iMidiEventType);
		m_pEventTypeGroup->setControlType(ctype);
	//	eventTypeChanged(pOptions->iMidiEventType);
		m_pEventTypeGroup->setControlParam(pOptions->iMidiEventParam);
		viewTypeChanged(pOptions->iMidiViewType);
	} else {
		m_pEventTypeComboBox->setCurrentIndex(0);
	//	eventTypeChanged(0);
	}

	// HACK: Some explicit focus immediately...
	m_pMidiEditor->editView()->setFocus();
}


// Destructor.
qtractorMidiEditorForm::~qtractorMidiEditorForm (void)
{
	// Remove this one from main-form list...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->removeEditorForm(this);

	// View/Snap-to-beat actions termination...
	qDeleteAll(m_snapPerBeatActions);
	m_snapPerBeatActions.clear();

	// Drop any widgets around (not really necessary)...
	if (m_pMidiEventList)
		delete m_pMidiEventList;
	if (m_pMidiEditor)
		delete m_pMidiEditor;

	// Destroy custom track/instrument proxy menu.
	if (m_pInstrumentMenu)
		delete m_pInstrumentMenu;

	// Get edit-mode action group down.
	if (m_pEditModeActionGroup)
		delete m_pEditModeActionGroup;
	if (m_pEditModeToolButton)
		delete m_pEditModeToolButton;

	if (m_pEventTypeGroup)
		delete m_pEventTypeGroup;

	// Ditch rec-mode/red palette...
	if (m_pRedPalette)
		delete m_pRedPalette;

	// Custom time-signature cursor.
	if (m_pTempoCursor)
		delete m_pTempoCursor;
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Window close event handlers.

// Pre-close event handlers.
bool qtractorMidiEditorForm::queryClose (void)
{
	bool bQueryClose = true;

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

	return bQueryClose;
}


// Save(as) warning message box.
int qtractorMidiEditorForm::querySave (
	const QString& sFilename, QWidget *pParent )
{
	if (pParent == nullptr)
		pParent = qtractorMainForm::getInstance();

	return (QMessageBox::warning(pParent,
		tr("Warning"),
		tr("The current MIDI clip has been changed:\n\n"
		"\"%1\"\n\n"
		"Do you want to save the changes?").arg(sFilename),
		QMessageBox::Save |
		QMessageBox::Discard |
		QMessageBox::Cancel));
}


// On-close event handler.
void qtractorMidiEditorForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	// Try to save current editor view state...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && isVisible()) {
		// Save decorations state.
		pOptions->bMidiMenubar = m_ui.menuBar->isVisible();
		pOptions->bMidiStatusbar = statusBar()->isVisible();
		pOptions->bMidiFileToolbar = m_ui.fileToolbar->isVisible();
		pOptions->bMidiEditToolbar = m_ui.editToolbar->isVisible();
		pOptions->bMidiViewToolbar = m_ui.viewToolbar->isVisible();
		pOptions->bMidiTransportToolbar = m_ui.transportToolbar->isVisible();
		pOptions->bMidiTimeToolbar = m_ui.timeToolbar->isVisible();
		pOptions->bMidiScaleToolbar = m_ui.snapToScaleToolbar->isVisible();
		pOptions->iMidiZoomMode = m_pMidiEditor->zoomMode();
		pOptions->iMidiHorizontalZoom = m_pMidiEditor->horizontalZoom();
		pOptions->iMidiVerticalZoom = m_pMidiEditor->verticalZoom();
		pOptions->bMidiSnapZebra = m_pMidiEditor->isSnapZebra();
		pOptions->bMidiSnapGrid = m_pMidiEditor->isSnapGrid();
		pOptions->bMidiToolTips = m_pMidiEditor->isToolTips();
		pOptions->bMidiEditMode = m_pMidiEditor->isEditMode();
		pOptions->bMidiEditModeDraw = m_pMidiEditor->isEditModeDraw();
		pOptions->iMidiDisplayFormat = (m_pMidiEditor->timeScale())->displayFormat();
		pOptions->bMidiNoteNames = m_ui.viewNoteNamesAction->isChecked();
		pOptions->bMidiNoteDuration = m_ui.viewNoteDurationAction->isChecked();
		pOptions->bMidiNoteColor = m_ui.viewNoteColorAction->isChecked();
		pOptions->bMidiValueColor = m_ui.viewValueColorAction->isChecked();
		pOptions->bMidiPreview = m_ui.viewPreviewAction->isChecked();
		pOptions->bMidiFollow  = m_ui.viewFollowAction->isChecked();
		// Save editor type-params...
		pOptions->iMidiViewType = m_pViewTypeComboBox->currentIndex();
		pOptions->iMidiEventType = m_pEventTypeComboBox->currentIndex();
		pOptions->iMidiEventParam = m_pEventTypeGroup->controlParam();
		// Save snap-per-beat setting...
		pOptions->iMidiSnapPerBeat = m_pSnapPerBeatComboBox->currentIndex();
		// Save snap-to-scale settings...
		pOptions->iMidiSnapToScaleKey = m_pSnapToScaleKeyComboBox->currentIndex();
		pOptions->iMidiSnapToScaleType = m_pSnapToScaleTypeComboBox->currentIndex();
		// Close floating dock windows...
		if (m_pMidiEventList->isFloating())
			m_pMidiEventList->close();
		// Save the dock windows state.
		pOptions->settings().setValue(
			"/MidiEditor/Layout/DockWindows", saveState());
		// And this main windows state?
		// pOptions->saveWidgetGeometry(this, true);
	}

	// Close it good.
	pCloseEvent->accept();
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Context menu event handlers.

// Context menu request.
void qtractorMidiEditorForm::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	stabilizeForm();

	// Primordial edit menu should be available...
	m_ui.editMenu->exec(pContextMenuEvent->globalPos());
}


// Edit menu accessor.
QMenu *qtractorMidiEditorForm::editMenu (void) const
{
	return m_ui.editMenu;
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Central widget redirect methods.

// MIDI editor widget accessor.
qtractorMidiEditor *qtractorMidiEditorForm::editor (void) const
{
	return m_pMidiEditor;
}


// Local time-scale accessor.
qtractorTimeScale *qtractorMidiEditorForm::timeScale (void) const
{
	return m_pMidiEditor->timeScale();
}

unsigned long qtractorMidiEditorForm::timeOffset (void) const
{
	return m_pMidiEditor->timeOffset();
}


// Editing MIDI clip accessors.
qtractorMidiClip *qtractorMidiEditorForm::midiClip (void) const
{
	return m_pMidiEditor->midiClip();
}


// MIDI clip property accessors.
const QString& qtractorMidiEditorForm::filename (void) const
{
	return m_pMidiEditor->filename();
}


unsigned short qtractorMidiEditorForm::trackChannel (void) const
{
	return m_pMidiEditor->trackChannel();
}


unsigned short qtractorMidiEditorForm::format (void) const
{
	return m_pMidiEditor->format();
}


qtractorMidiSequence *qtractorMidiEditorForm::sequence (void) const
{
	return m_pMidiEditor->sequence();
}


// Special executive setup method.
void qtractorMidiEditorForm::setup ( qtractorMidiClip *pMidiClip )
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
	const unsigned short iHorizontalZoom = m_pMidiEditor->horizontalZoom();
	const unsigned short iVerticalZoom = m_pMidiEditor->verticalZoom();
	qtractorTimeScale *pTimeScale  = m_pMidiEditor->timeScale();
	pTimeScale->copy(*pSession->timeScale());
	m_pMidiEditor->setHorizontalZoom(iHorizontalZoom);
	m_pMidiEditor->setVerticalZoom(iVerticalZoom);

	// Fix some local options though...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		const qtractorTimeScale::DisplayFormat displayFormat
			= qtractorTimeScale::DisplayFormat(pOptions->iMidiDisplayFormat);
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
	// whether pMidiClip is given non-null wich means
	// form initialization first setup or else...
	if (pMidiClip) {
		// Set initial MIDI clip properties has seen fit...
		m_pMidiEditor->setMidiClip(pMidiClip);
		// Setup connections to main widget...
		QObject::connect(m_pMidiEditor,
			SIGNAL(changeNotifySignal(qtractorMidiEditor *)),
			pMainForm, SLOT(changeNotifySlot(qtractorMidiEditor *)));
		// This one's local but helps...
		QObject::connect(m_pMidiEditor,
			SIGNAL(sendNoteSignal(int, int, bool)),
			SLOT(sendNote(int, int, bool)));
		// Setup for last known top-level window position...
		QPoint wpos = pMidiClip->editorPos();
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
		const QSize& wsize = pMidiClip->editorSize();
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
	const bool bMidiClip = (pMidiClip == nullptr);
	if (bMidiClip)
		pMidiClip = midiClip();

	// Setup for secondary time-signature, if any...
	if (pMidiClip) {
		pTimeScale->setBeatsPerBar2(pMidiClip->beatsPerBar2());
		pTimeScale->setBeatDivisor2(pMidiClip->beatDivisor2());
	}

	// Reset local dirty flag.
	resetDirtyCount();
	// Get all those names right...
	updateInstrumentNames();
	// Drum mode visuals....
	updateDrumMode();

	// Refresh and try to center (vertically) the edit-view...
	m_pMidiEditor->centerContents();
	m_pMidiEventList->refresh();

	// (Re)try to reposition the editor in the same relative
	// position in track-view, only if clip is not empty/new...
	qtractorTrack *pTrack = nullptr;
	if (pMidiClip) {
		qtractorMidiSequence *pSeq = pMidiClip->sequence();
		if (bMidiClip || (pSeq && pSeq->events().count() > 0))
			pTrack = pMidiClip->track();
	}
	if (pTrack) {
		const int h1 = pTrack->zoomHeight();
		if (h1 > 0) {
			// Try to recenter horizontally...
			qtractorTrackView *pTrackView = pTracks->trackView();
			const QPoint& pos = pTrackView->mapFromGlobal(QCursor::pos());
			unsigned long iFrame = pSession->frameFromPixel(
				pTrackView->contentsX() + pos.x());
			if (iFrame  > m_pMidiEditor->offset()) {
				iFrame -= m_pMidiEditor->offset();
			} else {
				iFrame = 0;
			}
			qtractorMidiEditView *pEditView = m_pMidiEditor->editView();
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
			const int nmax = pTrack->midiNoteMax();
			const int nmin = pTrack->midiNoteMin();
			const int n1 = (pos.y() - y1) * (nmax - nmin) / h1;
			const int n2 = (128 - nmax) + n1;
			if (n2 > 0 && n2 < 128)
				y2 = n2 * (m_pMidiEditor->editList())->itemHeight();
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
	m_pMidiEditor->setPlayHead(pSession->playHead(), false);
	m_pMidiEditor->setEditHead(pSession->editHead(), false);
	m_pMidiEditor->setEditTail(pSession->editTail(), false);

	// Finally update current time position display...
	updatePlayHead(pSession->playHead());

	// Done.
	stabilizeForm();
}


// Reset coomposite dirty flag.
void qtractorMidiEditorForm::resetDirtyCount (void)
{
	m_iDirtyCount = 0;

	// MIDI clip might be dirty already.
	qtractorMidiClip *pMidiClip = midiClip();
	if (pMidiClip && pMidiClip->isDirty())
		++m_iDirtyCount;
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Clip Action methods.

// Save current clip.
bool qtractorMidiEditorForm::saveClipFile ( bool bPrompt )
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return false;

	qtractorTrack *pTrack = pMidiClip->track();
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	// Suggest a brand new filename, if there's none...
	QString sFilename = pMidiClip->createFilePathRevision(bPrompt);

	// Ask for the file to save...
	if (sFilename.isEmpty())
		bPrompt = true;

	if (bPrompt) {
		// If none is given, assume default directory.
		const QString sExt("mid");
		const QString& sTitle
			= tr("Save MIDI Clip");
		QStringList filters;
		filters.append(tr("MIDI files (*.%1 *.smf *.midi)").arg(sExt));
		filters.append(tr("All files (*.*)"));
		const QString& sFilter = filters.join(";;");
		QWidget *pParentWidget = nullptr;
		QFileDialog::Options options;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bDontUseNativeDialogs) {
			options |= QFileDialog::DontUseNativeDialog;
			pParentWidget = QWidget::window();
		}
	#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
		// Ask for the filenames to open...
		sFilename = QFileDialog::getSaveFileName(pParentWidget,
			sTitle, sFilename, sFilter, nullptr, options);
	#else
		// Construct open-files dialog...
		QFileDialog fileDialog(pParentWidget, sTitle, sFilename, sFilter);
		// Set proper open-file modes...
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setDefaultSuffix(sExt);
		// Stuff sidebar...
		if (pOptions) {
			QList<QUrl> urls(fileDialog.sidebarUrls());
			urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
			urls.append(QUrl::fromLocalFile(pOptions->sMidiDir));
			fileDialog.setSidebarUrls(urls);
		}
		fileDialog.setOptions(options);
		// Show dialog...
		if (fileDialog.exec())
			sFilename = fileDialog.selectedFiles().first();
		else
			sFilename.clear();
	#endif
		// Have we cancelled it?
		if (sFilename.isEmpty() || sFilename.at(0) == '.')
			return false;
		// Enforce .mid extension...
		if (QFileInfo(sFilename).suffix().isEmpty())
			sFilename += '.' + sExt;
	}

	// Save it right away...
	const bool bResult
		= pMidiClip->saveCopyFile(sFilename, true);

	// Have we done it right?
	if (bResult) {
		// Aha, but we're not dirty no more.
		m_iDirtyCount = 0;
	}

	// Done.
	m_pMidiEventList->refresh();

	stabilizeForm();
	return bResult;
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- File Action slots.

// Save current clip.
void qtractorMidiEditorForm::fileSave (void)
{
	saveClipFile(false);
}


// Save current clip with another name.
void qtractorMidiEditorForm::fileSaveAs (void)
{
	saveClipFile(true);
}


// Mute current clip.
void qtractorMidiEditorForm::fileMute (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->muteClip(m_pMidiEditor->midiClip());
	}
}


// Unlink current clip.
void qtractorMidiEditorForm::fileUnlink (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->unlinkClip(m_pMidiEditor->midiClip());
	}
}


// Enter in clip record/ overdub mode.
void qtractorMidiEditorForm::fileRecordEx ( bool bOn )
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	// Start record/overdub the current clip, if any...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(new qtractorClipRecordExCommand(pMidiClip, bOn));
}


// File properties dialog.
void qtractorMidiEditorForm::fileProperties (void)
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	qtractorClipForm clipForm(this);
	clipForm.setClip(pMidiClip);
	clipForm.exec();
}


// Show current MIDI clip/track input bus connections.
void qtractorMidiEditorForm::fileTrackInputs (void)
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	qtractorTrack *pTrack = pMidiClip->track();
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


// Show current MIDI clip/track output bus connections.
void qtractorMidiEditorForm::fileTrackOutputs (void)
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	qtractorTrack *pTrack = pMidiClip->track();
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


// Edit current MIDI clip/track properties.
void qtractorMidiEditorForm::fileTrackProperties (void)
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	qtractorTrack *pTrack = pMidiClip->track();
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
void qtractorMidiEditorForm::fileRangeSet (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->rangeClip(m_pMidiEditor->midiClip());
	}
}


// Loop-range setting to clip extents.
void qtractorMidiEditorForm::fileLoopSet (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->loopClip(m_pMidiEditor->midiClip());
	}
}


// Exit editing.
void qtractorMidiEditorForm::fileClose (void)
{
	// Go for close this thing.
	close();
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Edit action slots.

// Undo last edit command.
void qtractorMidiEditorForm::editUndo (void)
{
	m_pMidiEditor->undoCommand();
}


// Redo last edit command.
void qtractorMidiEditorForm::editRedo (void)
{
	m_pMidiEditor->redoCommand();
}


// Cut current selection to clipboard.
void qtractorMidiEditorForm::editCut (void)
{
	m_pMidiEditor->cutClipboard();
}


// Copy current selection to clipboard.
void qtractorMidiEditorForm::editCopy (void)
{
	m_pMidiEditor->copyClipboard();
}


// Paste from clipboard.
void qtractorMidiEditorForm::editPaste (void)
{
	m_pMidiEditor->pasteClipboard();
}


// Paste/repeat from clipboard.
void qtractorMidiEditorForm::editPasteRepeat (void)
{
	qtractorPasteRepeatForm pasteForm(this);
	pasteForm.setRepeatPeriod(m_pMidiEditor->pastePeriod());
	if (pasteForm.exec()) {
		m_pMidiEditor->pasteClipboard(
			pasteForm.repeatCount(),
			pasteForm.repeatPeriod()
		);
	}
}


// Delete current selection.
void qtractorMidiEditorForm::editDelete (void)
{
	m_pMidiEditor->deleteSelect();
}


// Set edit-mode on.
void qtractorMidiEditorForm::editModeOn (void)
{
	m_pMidiEditor->setEditModeDraw(false);
	m_pMidiEditor->setEditMode(true);
	m_pMidiEditor->updateContents();

	stabilizeForm();
}

// Set edit-mode off.
void qtractorMidiEditorForm::editModeOff (void)
{
	m_pMidiEditor->setEditModeDraw(false);
	m_pMidiEditor->setEditMode(false);
	m_pMidiEditor->updateContents();

	stabilizeForm();
}


// Toggle draw-mode (notes)
void qtractorMidiEditorForm::editModeDraw ( bool bOn )
{
	m_pMidiEditor->setEditModeDraw(bOn);
	m_pMidiEditor->setEditMode(bOn);
	m_pMidiEditor->updateContents();

	stabilizeForm();
}


// Select none contents.
void qtractorMidiEditorForm::editSelectNone (void)
{
	qtractorScrollView *pScrollView = m_pMidiEditor->editView();
	if (m_pMidiEditor->editEvent()->hasFocus())
		pScrollView = m_pMidiEditor->editEvent();

	m_pMidiEditor->selectAll(pScrollView, false, false);
}


// Select invert contents.
void qtractorMidiEditorForm::editSelectInvert (void)
{
	qtractorScrollView *pScrollView = m_pMidiEditor->editView();
	if (m_pMidiEditor->editEvent()->hasFocus())
		pScrollView = m_pMidiEditor->editEvent();

	m_pMidiEditor->selectAll(pScrollView, true, true);
}


// Select all contents.
void qtractorMidiEditorForm::editSelectAll (void)
{
	qtractorScrollView *pScrollView = m_pMidiEditor->editView();
	if (m_pMidiEditor->editEvent()->hasFocus())
		pScrollView = m_pMidiEditor->editEvent();

	m_pMidiEditor->selectAll(pScrollView, true, false);
}


// Select contents range.
void qtractorMidiEditorForm::editSelectRange (void)
{
	qtractorScrollView *pScrollView = m_pMidiEditor->editView();
	if (m_pMidiEditor->editEvent()->hasFocus())
		pScrollView = m_pMidiEditor->editEvent();

	m_pMidiEditor->selectRange(pScrollView, true, true);
}


// Insert range.
void qtractorMidiEditorForm::editInsertRange (void)
{
	m_pMidiEditor->insertEditRange();
}


// Insert step.
void qtractorMidiEditorForm::editInsertStep (void)
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip) {
		pMidiClip->advanceStepInput();
		pMidiClip->updateStepInput();
	}
}


// Remove range.
void qtractorMidiEditorForm::editRemoveRange (void)
{
	m_pMidiEditor->removeEditRange();
}


// Quantize tool.
void qtractorMidiEditorForm::toolsQuantize (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Quantize);
}


// Transpose tool.
void qtractorMidiEditorForm::toolsTranspose (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Transpose);
}


// Normalize tool.
void qtractorMidiEditorForm::toolsNormalize (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Normalize);
}


// Randomize tool.
void qtractorMidiEditorForm::toolsRandomize (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Randomize);
}


// Resize tool.
void qtractorMidiEditorForm::toolsResize (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Resize);
}


// Rescale tool.
void qtractorMidiEditorForm::toolsRescale (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Rescale);
}


// Timeshift tool.
void qtractorMidiEditorForm::toolsTimeshift (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Timeshift);
}


// Temporamp tool.
void qtractorMidiEditorForm::toolsTemporamp (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Temporamp);
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- View Action slots.

// Show/hide the main program window menubar.
void qtractorMidiEditorForm::viewMenubar ( bool bOn )
{
	m_ui.menuBar->setVisible(bOn);
}


// Show/hide the main program window statusbar.
void qtractorMidiEditorForm::viewStatusbar ( bool bOn )
{
	statusBar()->setVisible(bOn);
}


// Show/hide the file-toolbar.
void qtractorMidiEditorForm::viewToolbarFile ( bool bOn )
{
	m_ui.fileToolbar->setVisible(bOn);
}


// Show/hide the edit-toolbar.
void qtractorMidiEditorForm::viewToolbarEdit ( bool bOn )
{
	m_ui.editToolbar->setVisible(bOn);
}


// Show/hide the view-toolbar.
void qtractorMidiEditorForm::viewToolbarView ( bool bOn )
{
	m_ui.viewToolbar->setVisible(bOn);
}


// Show/hide the transport-toolbar.
void qtractorMidiEditorForm::viewToolbarTransport ( bool bOn )
{
	m_ui.transportToolbar->setVisible(bOn);
}


// Show/hide the time-signature toolbar.
void qtractorMidiEditorForm::viewToolbarTime( bool bOn )
{
	m_ui.timeToolbar->setVisible(bOn);
}


// Show/hide the snap-to-scale toolbar.
void qtractorMidiEditorForm::viewToolbarScale ( bool bOn )
{
	m_ui.snapToScaleToolbar->setVisible(bOn);
}


// Show/hide the thumb-view toolbar.
void qtractorMidiEditorForm::viewToolbarThumb ( bool bOn )
{
	m_ui.thumbViewToolbar->setVisible(bOn);
}


// Show/hide the events window view.
void qtractorMidiEditorForm::viewEvents ( bool bOn )
{
	m_pMidiEventList->setVisible(bOn);
}


// View note (pitch) coloring.
void qtractorMidiEditorForm::viewNoteColor ( bool bOn )
{
	m_pMidiEditor->setNoteColor(bOn);
	m_pMidiEditor->updateContents();
}


// View note (velocity) coloring.
void qtractorMidiEditorForm::viewValueColor ( bool bOn )
{
	m_pMidiEditor->setValueColor(bOn);
	m_pMidiEditor->updateContents();
}


// View note names (in rectangles)
void qtractorMidiEditorForm::viewNoteNames ( bool bOn )
{
	m_pMidiEditor->setNoteNames(bOn);
	m_pMidiEditor->updateContents();
}


// View duration as widths of notes
void qtractorMidiEditorForm::viewNoteDuration ( bool bOn )
{
	m_pMidiEditor->setNoteDuration(bOn);
	m_pMidiEditor->updateContents();
}


// Change view/note type setting via menu.
void qtractorMidiEditorForm::viewNoteType (void)
{
	// Retrieve view/note type index from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		const int iIndex = pAction->data().toInt();
		// Update the other toolbar control...
		m_pViewTypeComboBox->setCurrentIndex(iIndex);
		// Commit the change as usual...
		viewTypeChanged(iIndex);
	}
}


// View drum note of notes (diamods)
void qtractorMidiEditorForm::viewDrumMode ( bool bOn )
{
	m_pSnapToScaleKeyComboBox->setEnabled(!bOn);
	m_pSnapToScaleTypeComboBox->setEnabled(!bOn);

	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip)
		pMidiClip->setEditorDrumMode(bOn ? 1 : 0);

	m_pMidiEditor->setDrumMode(bOn);
	m_pMidiEditor->updateContents();
}


// Change event/value type setting via menu.
void qtractorMidiEditorForm::viewValueType (void)
{
	// Retrieve event/value type index from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		const int iIndex = pAction->data().toInt();
		// Update the other toolbar control...
		const qtractorMidiControl::ControlType ctype
			= m_pEventTypeGroup->controlTypeFromIndex(iIndex);
		m_pEventTypeGroup->setControlType(ctype);
		// Commit the change as usual...
		// eventTypeChanged(iIndex);
	}
}


// Change ghost-track setting via menu.
void qtractorMidiEditorForm::viewGhostTrack (void)
{
	// Retrieve ghost-track from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		// Commit the change as usual...
		qtractorTrack *pGhostTrack
			= static_cast<qtractorTrack *> (
				pAction->data().value<void *> ());
		m_pMidiEditor->setGhostTrack(pGhostTrack);
		m_pMidiEditor->updateContents();
	}
}


// Horizontal and/or vertical zoom-in.
void qtractorMidiEditorForm::viewZoomIn (void)
{
	m_pMidiEditor->zoomIn();
}


// Horizontal and/or vertical zoom-out.
void qtractorMidiEditorForm::viewZoomOut (void)
{
	m_pMidiEditor->zoomOut();
}


// Reset zoom level to default.
void qtractorMidiEditorForm::viewZoomReset (void)
{
	m_pMidiEditor->zoomReset();
}


// Set horizontal zoom mode
void qtractorMidiEditorForm::viewZoomHorizontal (void)
{
	m_pMidiEditor->setZoomMode(qtractorMidiEditor::ZoomHorizontal);
}


// Set vertical zoom mode
void qtractorMidiEditorForm::viewZoomVertical (void)
{
	m_pMidiEditor->setZoomMode(qtractorMidiEditor::ZoomVertical);
}


// Set all zoom mode
void qtractorMidiEditorForm::viewZoomAll (void)
{
	m_pMidiEditor->setZoomMode(qtractorMidiEditor::ZoomAll);
}


// Set zebra mode
void qtractorMidiEditorForm::viewSnapZebra ( bool bOn )
{
	m_pMidiEditor->setSnapZebra(bOn);
	m_pMidiEditor->updateContents();
}


// Set grid mode
void qtractorMidiEditorForm::viewSnapGrid ( bool bOn )
{
	m_pMidiEditor->setSnapGrid(bOn);
	m_pMidiEditor->updateContents();
}


// Set floating tool-tips view mode
void qtractorMidiEditorForm::viewToolTips ( bool bOn )
{
	m_pMidiEditor->setToolTips(bOn);
}


// Change snap-per-beat setting via menu.
void qtractorMidiEditorForm::viewSnap (void)
{
	// Retrieve snap-per-beat index from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		// Commit the change as usual...
		snapPerBeatChanged(pAction->data().toInt());
		// Update the other toolbar control...
		qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
		if (pTimeScale)
			m_pSnapPerBeatComboBox->setCurrentIndex(
				qtractorTimeScale::indexFromSnap(pTimeScale->snapPerBeat()));
	}
}


// Change snap-to-scale key setting via menu.
void qtractorMidiEditorForm::viewScaleKey (void)
{
	// Retrieve snap-to-scale key index from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		// Commit the change as usual...
		snapToScaleKeyChanged(pAction->data().toInt());
		// Update the other toolbar control...
		m_pSnapToScaleKeyComboBox->setCurrentIndex(
			m_pMidiEditor->snapToScaleKey());
	}
}


// Change snap-to-scale type setting via menu.
void qtractorMidiEditorForm::viewScaleType (void)
{
	// Retrieve snap-to-scale type index from from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		// Commit the change as usual...
		snapToScaleTypeChanged(pAction->data().toInt());
		// Update the other toolbar control...
		m_pSnapToScaleTypeComboBox->setCurrentIndex(
			m_pMidiEditor->snapToScaleType());
	}
}


// Refresh view display.
void qtractorMidiEditorForm::viewRefresh (void)
{
	m_pTempoCursor->clear();

	m_pMidiEditor->updateContents();
	m_pMidiEventList->refresh();

	stabilizeForm();
}


// View preview notes
void qtractorMidiEditorForm::viewPreview ( bool bOn )
{
	m_pMidiEditor->setSendNotes(bOn);
}


// View follow playhead
void qtractorMidiEditorForm::viewFollow ( bool bOn )
{
	m_pMidiEditor->setSyncView(bOn);
}



//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Transport Action slots.

// Transport step-backward (local)
void qtractorMidiEditorForm::transportStepBackward (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
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

	m_pMidiEditor->setSyncViewHoldOn(false);
	m_pMidiEditor->setPlayHead(iPlayHead);
	pSession->setPlayHead(iPlayHead);
}


// Transport step-forward (local)
void qtractorMidiEditorForm::transportStepForward (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
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

	m_pMidiEditor->setSyncViewHoldOn(false);
	m_pMidiEditor->setPlayHead(iPlayHead);
	pSession->setPlayHead(iPlayHead);
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Help Action slots.

// Show (and edit) keyboard shortcuts.
void qtractorMidiEditorForm::helpShortcuts (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	const QList<QAction *>& actions
		= findChildren<QAction *> (QString(), Qt::FindDirectChildrenOnly);
	qtractorShortcutForm shortcutForm(actions, this);
	shortcutForm.setActionControl(nullptr); // Disable MIDI Controllers here!
	if (shortcutForm.exec() && shortcutForm.isDirtyActionShortcuts())
		pOptions->saveActionShortcuts(this);
}


// Show information about application program.
void qtractorMidiEditorForm::helpAbout (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->helpAbout();
}

// Show information about the Qt toolkit.
void qtractorMidiEditorForm::helpAboutQt (void)
{
	QMessageBox::aboutQt(this);
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Utility methods.

// Send note on/off to respective output bus.
void qtractorMidiEditorForm::sendNote ( int iNote, int iVelocity, bool bForce )
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	qtractorTrack *pTrack = pMidiClip->track();
	if (pTrack == nullptr)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus == nullptr)
		return;

	pMidiBus->sendNote(pTrack, iNote, iVelocity, bForce);
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Main form stabilization.

void qtractorMidiEditorForm::stabilizeForm (void)
{
	// Update the main menu state...
	qtractorTrack *pTrack = nullptr;
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip)
		pTrack = pMidiClip->track();
	
	m_ui.fileSaveAction->setEnabled(m_iDirtyCount > 0);
	m_ui.fileMuteAction->setEnabled(pMidiClip != nullptr);
	m_ui.fileMuteAction->setChecked(pMidiClip && pMidiClip->isClipMute());
	m_ui.fileUnlinkAction->setEnabled(pMidiClip && pMidiClip->isHashLinked());

	const bool bClipRecordEx = (pTrack && pTrack->isClipRecordEx()
		&& static_cast<qtractorMidiClip *> (pTrack->clipRecord()) == pMidiClip);
	m_ui.fileRecordExAction->setEnabled(pMidiClip != nullptr);
	m_ui.fileRecordExAction->setChecked(bClipRecordEx);

	m_ui.fileTrackInputsAction->setEnabled(pTrack && pTrack->inputBus() != nullptr);
	m_ui.fileTrackOutputsAction->setEnabled(pTrack && pTrack->outputBus() != nullptr);
	m_ui.fileTrackInstrumentMenu->setEnabled(pTrack != nullptr);
	m_ui.fileTrackPropertiesAction->setEnabled(pTrack != nullptr);
	m_ui.fileRangeSetAction->setEnabled(pTrack != nullptr);
	m_ui.fileLoopSetAction->setEnabled(pTrack != nullptr);

	// Update edit menu state...
	qtractorCommandList *pCommands = m_pMidiEditor->commands();
	if (pCommands) {
		pCommands->updateAction(m_ui.editUndoAction, pCommands->lastCommand());
		pCommands->updateAction(m_ui.editRedoAction, pCommands->nextCommand());
	}

	const bool bSelected = m_pMidiEditor->isSelected();
	const bool bSelectable = m_pMidiEditor->isSelectable();
	const bool bClipboard = m_pMidiEditor->isClipboard();
	m_ui.editCutAction->setEnabled(bSelected);
	m_ui.editCopyAction->setEnabled(bSelected);
	m_ui.editPasteAction->setEnabled(bClipboard);
	m_ui.editPasteRepeatAction->setEnabled(bClipboard);
	m_ui.editDeleteAction->setEnabled(bSelected);
//	m_ui.editModeDrawAction->setEnabled(m_pMidiEditor->isEditMode());
	m_ui.editSelectNoneAction->setEnabled(bSelected);

	const bool bInsertable = m_pMidiEditor->isInsertable();
	m_ui.editInsertRangeAction->setEnabled(bInsertable);
	m_ui.editRemoveRangeAction->setEnabled(bInsertable);

#if 0
	m_ui.toolsMenu->setEnabled(bSelected);
#else
	m_ui.toolsQuantizeAction->setEnabled(bSelected);
	m_ui.toolsTransposeAction->setEnabled(bSelected);
	m_ui.toolsNormalizeAction->setEnabled(bSelected);
	m_ui.toolsRandomizeAction->setEnabled(bSelected);
	m_ui.toolsResizeAction->setEnabled(bSelected);
	m_ui.toolsRescaleAction->setEnabled(bSelected);
	m_ui.toolsTimeshiftAction->setEnabled(bSelected && bSelectable);
	m_ui.toolsTemporampAction->setEnabled(bSelected && bSelectable);
#endif
	// Just having a non-null sequence will indicate
	// that we're editing a legal MIDI clip...
	qtractorMidiSequence *pSeq = sequence();
	if (pSeq == nullptr) {
		setWindowTitle(tr("MIDI Editor"));
		m_pFileNameLabel->clear();
		m_pTrackChannelLabel->clear();
		m_pTrackNameLabel->clear();
		m_pStatusModLabel->clear();
		m_pStatusRecLabel->clear();
		m_pStatusMuteLabel->clear();
		m_pDurationLabel->clear();
		return;
	}

	// Special display formatting...
	QString sTrackChannel;
	int k = 0;
	if (format() == 0) {
		sTrackChannel = tr("Channel %1");
		++k;
	} else {
		sTrackChannel = tr("Track %1");
	}

	// Update the main window caption...
	QString sTitle = QFileInfo(filename()).fileName() + ' ';
	sTitle += '(' + sTrackChannel.arg(trackChannel() + k) + ')';
	if (m_iDirtyCount > 0)
		sTitle += ' ' + tr("[modified]");
	setWindowTitle(sTitle);

	m_ui.viewEventsAction->setChecked(m_pMidiEventList->isVisible());

	m_pTrackNameLabel->setText(pTrack->trackName());
	m_pFileNameLabel->setText(filename());
	m_pTrackChannelLabel->setText(sTrackChannel.arg(trackChannel() + k));

	if (m_iDirtyCount > 0)
		m_pStatusModLabel->setText(tr("MOD"));
	else
		m_pStatusModLabel->clear();

	if (pTrack && pTrack->clipRecord() == pMidiClip) {
		m_pStatusRecLabel->setText(tr("REC"));
		m_pStatusRecLabel->setPalette(*m_pRedPalette);
	} else {
		m_pStatusRecLabel->clear();
		m_pStatusRecLabel->setPalette(statusBar()->palette());
	}

	if (pMidiClip && pMidiClip->isClipMute()) {
		m_pStatusMuteLabel->setText(tr("MUTE"));
		m_pStatusMuteLabel->setPalette(*m_pYellowPalette);
	} else {
		m_pStatusMuteLabel->clear();
		m_pStatusMuteLabel->setPalette(statusBar()->palette());
	}

	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
	m_pDurationLabel->setText(pTimeScale->textFromTick(
		pTimeScale->tickFromFrame(m_pMidiEditor->offset()), true, pSeq->duration()));

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
		m_ui.editInsertStepAction->setEnabled(!bPlaying && bClipRecordEx);
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
		if (pMidiClip) {
			const unsigned long iClipStart
				= pMidiClip->clipStart();
			const unsigned long iClipEnd
				= iClipStart + pMidiClip->clipLength();
			m_ui.fileLoopSetAction->setChecked(bLooping
				&& iClipStart == pSession->loopStart()
				&& iClipEnd   == pSession->loopEnd());
		}
	}

	// Secondary rtime-signature status...
	m_pTimeSig2ResetButton->setEnabled(pMidiClip != nullptr
		&& (pMidiClip->beatsPerBar2() > 0 || pMidiClip->beatDivisor2() > 0));

	// Stabilize thumb-view...
	m_pMidiEditor->thumbView()->update();
	m_pMidiEditor->thumbView()->updateThumb();
}


// Update clip/track instrument names...
void qtractorMidiEditorForm::updateInstrumentNames (void)
{
	// Just in case...
	m_pEventTypeGroup->updateControlType();

	const qtractorMidiEvent::EventType eventType
		= m_pEventTypeGroup->controlType();

	m_pEventParamComboBox->setEnabled(
		eventType == qtractorMidiEvent::CONTROLLER  ||
		eventType == qtractorMidiEvent::REGPARAM    ||
		eventType == qtractorMidiEvent::NONREGPARAM ||
		eventType == qtractorMidiEvent::CONTROL14);
}


// View/drum-mode update.
void qtractorMidiEditorForm::updateDrumMode (void)
{
	const bool bBlockSignals
		= m_ui.viewDrumModeAction->blockSignals(true);
	const bool bDrumMode = m_pMidiEditor->isDrumMode();
	m_ui.viewDrumModeAction->setChecked(bDrumMode);
	m_pSnapToScaleKeyComboBox->setEnabled(!bDrumMode);
	m_pSnapToScaleTypeComboBox->setEnabled(!bDrumMode);
	m_ui.viewDrumModeAction->blockSignals(bBlockSignals);
}


// Update thumb-view play-head...
void qtractorMidiEditorForm::updatePlayHead ( unsigned long iPlayHead )
{
	m_pTimeSpinBox->setValue(iPlayHead, false);
	m_pMidiEditor->thumbView()->updatePlayHead(iPlayHead);

	// Tricky stuff: node's non-null iif tempo changes...
	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
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
void qtractorMidiEditorForm::updateTimeScale (void)
{
	m_pTempoCursor->clear();

	m_pMidiEditor->updateTimeScale();
	m_pMidiEditor->updateContents();
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Selection widget slots.

// Note type view menu stabilizer.
void qtractorMidiEditorForm::updateNoteTypeMenu (void)
{
	const int iCurrentIndex = m_pViewTypeComboBox->currentIndex();
	QListIterator<QAction *> iter(m_ui.viewNoteTypeMenu->actions());
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		pAction->setChecked(pAction->data().toInt() == iCurrentIndex);
	}
}


// Event type view menu stabilizer.
void qtractorMidiEditorForm::updateValueTypeMenu (void)
{
	const int iCurrentIndex = m_pEventTypeComboBox->currentIndex();
	QListIterator<QAction *> iter(m_ui.viewValueTypeMenu->actions());
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		pAction->setChecked(pAction->data().toInt() == iCurrentIndex);
	}
}


// Ghost-track menu builder..
void qtractorMidiEditorForm::updateGhostTrackMenu (void)
{
	m_ui.viewGhostTrackMenu->clear();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	qtractorTrack *pGhostTrack = m_pMidiEditor->ghostTrack();

	QAction *pAction;
	QVariant data;

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Midi) {
			pAction = m_ui.viewGhostTrackMenu->addAction(
				pTrack->trackName(), this, SLOT(viewGhostTrack()));
			pAction->setCheckable(true);
			pAction->setChecked(pGhostTrack == pTrack);
			data.setValue(static_cast<void *> (pTrack));
			pAction->setData(data);
			pAction->setEnabled(pTrack != pMidiClip->track());
		}
	}

	m_ui.viewGhostTrackMenu->addSeparator();
	pAction = m_ui.viewGhostTrackMenu->addAction(
		tr("&None"), this, SLOT(viewGhostTrack()));
	pAction->setCheckable(true);
	pAction->setChecked(pGhostTrack == nullptr);
	data.setValue(0);
	pAction->setData(data);
}


// Zoom view menu stabilizer.
void qtractorMidiEditorForm::updateZoomMenu (void)
{
	const int iZoomMode = m_pMidiEditor->zoomMode();

	m_ui.viewZoomHorizontalAction->setChecked(
		iZoomMode == qtractorMidiEditor::ZoomHorizontal);
	m_ui.viewZoomVerticalAction->setChecked(
		iZoomMode == qtractorMidiEditor::ZoomVertical);
	m_ui.viewZoomAllAction->setChecked(
		iZoomMode == qtractorMidiEditor::ZoomAll);
}
 
 
// Snap-per-beat view menu builder.
void qtractorMidiEditorForm::updateSnapMenu (void)
{
	m_ui.viewSnapMenu->clear();

	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
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


// Snap-to-scale view menu builder.
void qtractorMidiEditorForm::updateScaleMenu (void)
{
	m_ui.viewScaleMenu->clear();

	const int iSnapToScaleKey = m_pMidiEditor->snapToScaleKey();
	int iScaleKey = 0;
	QStringListIterator iter_key(qtractorMidiEditor::scaleKeyNames());
	while (iter_key.hasNext()) {
		QAction *pAction = m_ui.viewScaleMenu->addAction(
			iter_key.next(), this, SLOT(viewScaleKey()));
		pAction->setCheckable(true);
		pAction->setChecked(iScaleKey == iSnapToScaleKey);
		pAction->setData(iScaleKey++);
	}

	m_ui.viewScaleMenu->addSeparator();

	const int iSnapToScaleType = m_pMidiEditor->snapToScaleType();
	int iScaleType = 0;
	QStringListIterator iter_type(qtractorMidiEditor::scaleTypeNames());
	while (iter_type.hasNext()) {
		QAction *pAction = m_ui.viewScaleMenu->addAction(
			iter_type.next(), this, SLOT(viewScaleType()));
		pAction->setCheckable(true);
		pAction->setChecked(iScaleType == iSnapToScaleType);
		pAction->setData(iScaleType++);
	}
}


// Track/Instrument sub-menu stabilizers.
void qtractorMidiEditorForm::updateTrackInstrumentMenu (void)
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip) {
		m_pInstrumentMenu->updateTrackMenu(
			pMidiClip->track(), m_ui.fileTrackInstrumentMenu);
	}
}


// Time format custom context menu.
void qtractorMidiEditorForm::transportTimeFormatChanged ( int iDisplayFormat )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEditorForm::transportTimeFormatChanged(%d)", iDisplayFormat);
#endif

	const qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	(m_pMidiEditor->timeScale())->setDisplayFormat(displayFormat);
	m_pTimeSpinBox->setDisplayFormat(displayFormat);

	m_pMidiEventList->refresh();

	stabilizeForm();
}


// Real thing: the playhead has been changed manually!
void qtractorMidiEditorForm::transportTimeChanged ( unsigned long iPlayHead )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEditorForm::transportTimeChanged(%lu)", iPlayHead);
#endif

	m_pMidiEditor->setPlayHead(iPlayHead);
	pSession->setPlayHead(iPlayHead);
}

void qtractorMidiEditorForm::transportTimeFinished (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEditorForm::transportTimeFinished()");
#endif

	const bool bBlockSignals = m_pTimeSpinBox->blockSignals(true);
	m_pTimeSpinBox->clearFocus();
	m_pTimeSpinBox->blockSignals(bBlockSignals);
}


// Time-signature spin-box change slot.
void qtractorMidiEditorForm::transportTempoChanged (
	float fTempo, unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEditorForm::transportTempoChanged(%g, %u, %u)",
		fTempo, iBeatsPerBar, iBeatDivisor);
#endif

	// Find appropriate node...
	qtractorTimeScale *pTimeScale = pSession->timeScale();
	qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pSession->playHead());

	// Check for local/secondary time-signature/meter changes...
	qtractorMidiClip *pMidiClip = midiClip();
	if (pMidiClip) {
		if (iBeatsPerBar == pNode->beatsPerBar &&
			iBeatDivisor == pNode->beatDivisor) {
			if (pMidiClip->beatsPerBar2() > 0 ||
				pMidiClip->beatDivisor2() > 0) {
				resetTimeSig2();
				return;
			}
		}
		else
		if (iBeatsPerBar != pMidiClip->beatsPerBar2() ||
			iBeatDivisor != pMidiClip->beatDivisor2()) {
			resetTimeSig2(iBeatsPerBar, iBeatDivisor);
			return;
		}
	}

	// Now, express the change as an undoable command...
	m_pMidiEditor->execute(
		new qtractorTimeScaleUpdateNodeCommand(
			pTimeScale, pNode->frame, fTempo, 2, iBeatsPerBar, iBeatDivisor));
}


void qtractorMidiEditorForm::transportTempoFinished (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEditorForm::transportTempoFinished()");
#endif

	const bool bBlockSignals = m_pTempoSpinBox->blockSignals(true);
	m_pTempoSpinBox->clearFocus();
	m_pTempoSpinBox->blockSignals(bBlockSignals);
}


// Time-signature custom context menu.
void qtractorMidiEditorForm::transportTempoContextMenu ( const QPoint& /*pos*/ )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->viewTempoMap();
}


// Secondary time-signature reset slot.
void qtractorMidiEditorForm::timeSig2ResetClicked (void)
{
	resetTimeSig2();
}


// Secondary time-signature reset slot.
void qtractorMidiEditorForm::resetTimeSig2 (
	unsigned short iBeatsPerBar2, unsigned short iBeatDivisor2 )
{
	m_pMidiEditor->execute(
		new qtractorTimeScaleTimeSig2Command(
			timeScale(), midiClip(), iBeatsPerBar2, iBeatDivisor2));
}


// Snap-per-beat spin-box change slot.
void qtractorMidiEditorForm::snapPerBeatChanged ( int iSnap )
{
	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	// Avoid bogus changes...
	const unsigned short iSnapPerBeat = qtractorTimeScale::snapFromIndex(iSnap);
	if (iSnapPerBeat == pTimeScale->snapPerBeat())
		return;

	// No need to express the change as a undoable command?
	pTimeScale->setSnapPerBeat(iSnapPerBeat);

	// If showing grid, it changed a bit for sure...
	if (m_pMidiEditor->isSnapGrid() || m_pMidiEditor->isSnapZebra())
		m_pMidiEditor->updateContents();

	m_pMidiEditor->editView()->setFocus();
}


// Snap-to-scale/quantize combo-box change slots.
void qtractorMidiEditorForm::snapToScaleKeyChanged ( int iSnapToScaleKey )
{
	m_pMidiEditor->setSnapToScaleKey(iSnapToScaleKey);
}


void qtractorMidiEditorForm::snapToScaleTypeChanged ( int iSnapToScaleType )
{
	m_pMidiEditor->setSnapToScaleType(iSnapToScaleType);
}


// Tool selection handlers.
void qtractorMidiEditorForm::viewTypeChanged ( int iIndex )
{
	const qtractorMidiEvent::EventType eventType
		= qtractorMidiEvent::EventType(
			m_pViewTypeComboBox->itemData(iIndex).toInt());

	m_pMidiEditor->editView()->setEventType(eventType);
	m_pMidiEditor->updateContents();
	m_pMidiEventList->refresh();

	stabilizeForm();
}


void qtractorMidiEditorForm::eventTypeChanged ( int iIndex )
{
	const qtractorMidiEvent::EventType eventType
		= qtractorMidiEvent::EventType(
			m_pEventTypeComboBox->itemData(iIndex).toInt());

	m_pEventParamComboBox->setEnabled(
		eventType == qtractorMidiEvent::CONTROLLER  ||
		eventType == qtractorMidiEvent::REGPARAM    ||
		eventType == qtractorMidiEvent::NONREGPARAM ||
		eventType == qtractorMidiEvent::CONTROL14);

//	updateInstrumentNames();

	m_pMidiEditor->editEvent()->setEventType(eventType);
	m_pMidiEditor->updateContents();
	m_pMidiEventList->refresh();

	stabilizeForm();
}


void qtractorMidiEditorForm::eventParamChanged ( int iParam )
{
	m_pMidiEditor->editEvent()->setEventParam(iParam);
	m_pMidiEditor->updateContents();
	m_pMidiEventList->refresh();

	stabilizeForm();
}


void qtractorMidiEditorForm::selectionChanged ( qtractorMidiEditor *pMidiEditor )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->selectionNotifySlot(pMidiEditor);

	stabilizeForm();
}


void qtractorMidiEditorForm::contentsChanged ( qtractorMidiEditor *pMidiEditor )
{
	++m_iDirtyCount;

	m_pTempoCursor->clear();

	selectionChanged(pMidiEditor);
}


// Top-level window geometry related slots.
void qtractorMidiEditorForm::posChanged (void)
{
	if (isVisible()) {
		qtractorMidiClip *pMidiClip = midiClip();
		if (pMidiClip)
			pMidiClip->setEditorPos(pos());
	}
}


void qtractorMidiEditorForm::sizeChanged (void)
{
	if (isVisible()) {
		qtractorMidiClip *pMidiClip = midiClip();
		if (pMidiClip)
			pMidiClip->setEditorSize(size());
	}
}


// end of qtractorMidiEditorForm.cpp

