// qtractorMidiEditorForm.cpp
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
#include "qtractorMidiEditorForm.h"
#include "qtractorMidiEditor.h"
#include "qtractorMidiEditView.h"
#include "qtractorMidiEditEvent.h"

#include "qtractorMidiClip.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorTrackView.h"

#include "qtractorMainForm.h"
#include "qtractorClipForm.h"

#include "qtractorTimeScale.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Main window form implementation.

// Constructor.
qtractorMidiEditorForm::qtractorMidiEditorForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QMainWindow(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_iClipLength = 0;
	m_iDirtyCount = 0;

	// Set our central widget.
	m_pMidiEditor = new qtractorMidiEditor(this);
	setCentralWidget(m_pMidiEditor);

	// Set edit-mode action group up...
	m_pEditModeActionGroup = new QActionGroup(this);
	m_pEditModeActionGroup->setExclusive(true);
	m_pEditModeActionGroup->addAction(m_ui.editModeOnAction);
	m_pEditModeActionGroup->addAction(m_ui.editModeOffAction);

	// Snap-per-beat combo-box.
	m_pSnapPerBeatComboBox = new QComboBox(m_ui.viewToolbar);
	m_pSnapPerBeatComboBox->setEditable(false);
	m_pSnapPerBeatComboBox->insertItems(0, qtractorTimeScale::snapItems());
	m_pSnapPerBeatComboBox->setToolTip(tr("Snap/beat"));
	m_ui.viewToolbar->addSeparator();
	m_ui.viewToolbar->addWidget(m_pSnapPerBeatComboBox);

	// Event type selection widgets...
	m_pViewTypeComboBox = new QComboBox(m_ui.editViewToolbar);
	m_pViewTypeComboBox->setEditable(false);
	m_pEventTypeComboBox = new QComboBox(m_ui.editEventToolbar);
	m_pEventTypeComboBox->setEditable(false);
	m_pControllerComboBox = new QComboBox(m_ui.editEventToolbar);
	m_pControllerComboBox->setEditable(false);

	// Pre-fill the combo-boxes...
	const QIcon iconViewType(":/icons/itemProperty.png");
	m_pViewTypeComboBox->addItem(iconViewType,
		tr("Note On"), int(qtractorMidiEvent::NOTEON));
	m_pViewTypeComboBox->addItem(iconViewType,
		tr("Key Press"), int(qtractorMidiEvent::KEYPRESS));

	const QIcon iconEventType(":/icons/itemProperty.png");
	m_pEventTypeComboBox->addItem(iconEventType,
		tr("Note Velocity"), int(qtractorMidiEvent::NOTEON));
	m_pEventTypeComboBox->addItem(iconEventType,
		tr("Key Pressure"), int(qtractorMidiEvent::KEYPRESS));
	m_pEventTypeComboBox->addItem(iconEventType,
		tr("Controller"), int(qtractorMidiEvent::CONTROLLER));
	m_pEventTypeComboBox->addItem(iconEventType,
		tr("Program Change"), int(qtractorMidiEvent::PGMCHANGE));
	m_pEventTypeComboBox->addItem(iconEventType,
		tr("Channel Pressure"), int(qtractorMidiEvent::CHANPRESS));
	m_pEventTypeComboBox->addItem(iconEventType,
		tr("Pitch Bend"), int(qtractorMidiEvent::PITCHBEND));
	m_pEventTypeComboBox->addItem(iconEventType,
		tr("System Exclusive"), int(qtractorMidiEvent::SYSEX));

	const QIcon iconController(":/icons/itemControllers.png");
	for (int i = 0; i < 128; ++i) {
		m_pControllerComboBox->addItem(iconController,
			QString::number(i) + " - " + qtractorMidiEditor::controllerName(i), i);
	}

	// Add combo-boxes to toolbars...
	m_ui.editViewToolbar->addWidget(m_pViewTypeComboBox);
	m_ui.editEventToolbar->addWidget(m_pEventTypeComboBox);
	m_ui.editEventToolbar->addSeparator();
	m_ui.editEventToolbar->addWidget(m_pControllerComboBox);

	const QSize pad(4, 0);
	const QString spc(4, ' ');

	// Status clip/sequence name...
	m_pTrackNameLabel = new QLabel(spc);
	m_pTrackNameLabel->setAlignment(Qt::AlignLeft);
	m_pTrackNameLabel->setToolTip(tr("MIDI clip name"));
	m_pTrackNameLabel->setAutoFillBackground(true);
	statusBar()->addWidget(m_pTrackNameLabel, 1);

	// Status filename...
	m_pFileNameLabel = new QLabel(spc);
	m_pFileNameLabel->setAlignment(Qt::AlignLeft);
	m_pFileNameLabel->setToolTip(tr("MIDI file name"));
	m_pFileNameLabel->setAutoFillBackground(true);
	statusBar()->addWidget(m_pFileNameLabel, 2);

	// Status track/channel number...
	m_pTrackChannelLabel = new QLabel(spc);
	m_pTrackChannelLabel->setAlignment(Qt::AlignHCenter);
	m_pTrackChannelLabel->setToolTip(tr("MIDI track/channel"));
	m_pTrackChannelLabel->setAutoFillBackground(true);
	statusBar()->addWidget(m_pTrackChannelLabel);

	// Status modification status.
	m_pStatusModLabel = new QLabel(tr("MOD"));
	m_pStatusModLabel->setAlignment(Qt::AlignHCenter);
	m_pStatusModLabel->setMinimumSize(m_pStatusModLabel->sizeHint() + pad);
	m_pStatusModLabel->setToolTip(tr("MIDI modification state"));
	m_pStatusModLabel->setAutoFillBackground(true);
	statusBar()->addWidget(m_pStatusModLabel);

	// Sequence duration status.
	m_pDurationLabel = new QLabel(tr("00:00:00.000"));
	m_pDurationLabel->setAlignment(Qt::AlignHCenter);
	m_pDurationLabel->setMinimumSize(m_pDurationLabel->sizeHint() + pad);
	m_pDurationLabel->setToolTip(tr("MIDI clip duration"));
	m_pDurationLabel->setAutoFillBackground(true);
	statusBar()->addWidget(m_pDurationLabel);

	// Some actions surely need those
	// shortcuts firmly attached...
	addAction(m_ui.viewMenubarAction);
	// Special integration ones.
	addAction(m_ui.editSelectRangeAction);
	addAction(m_ui.transportBackwardAction);
	addAction(m_ui.transportLoopAction);
	addAction(m_ui.transportLoopSetAction);
	addAction(m_ui.transportPlayAction);

	// Ah, make it stand right.
	setFocus();

	// UI signal/slot connections...
	QObject::connect(m_ui.fileSaveAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSave()));
	QObject::connect(m_ui.fileSaveAsAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSaveAs()));
	QObject::connect(m_ui.filePropertiesAction,
		SIGNAL(triggered(bool)),
		SLOT(fileProperties()));
	QObject::connect(m_ui.fileCloseAction,
		SIGNAL(triggered(bool)),
		SLOT(fileClose()));

	QObject::connect(m_ui.editModeOnAction,
		SIGNAL(triggered(bool)),
		SLOT(editModeOn()));
	QObject::connect(m_ui.editModeOffAction,
		SIGNAL(triggered(bool)),
		SLOT(editModeOff()));

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
	QObject::connect(m_ui.editSelectNoneAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectNone()));
	QObject::connect(m_ui.editSelectInvertAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectInvert()));
	QObject::connect(m_ui.editSelectAllAction,
		SIGNAL(triggered(bool)),
		SLOT(editSelectAll()));

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
	QObject::connect(m_ui.viewNoteDurationAction,
		SIGNAL(triggered(bool)),
		SLOT(viewNoteDuration(bool)));
	QObject::connect(m_ui.viewNoteColorAction,
		SIGNAL(triggered(bool)),
		SLOT(viewNoteColor(bool)));
	QObject::connect(m_ui.viewValueColorAction,
		SIGNAL(triggered(bool)),
		SLOT(viewValueColor(bool)));
	QObject::connect(m_ui.viewPreviewAction,
		SIGNAL(triggered(bool)),
		SLOT(viewPreview(bool)));
	QObject::connect(m_ui.viewFollowAction,
		SIGNAL(triggered(bool)),
		SLOT(viewFollow(bool)));
	QObject::connect(m_ui.viewRefreshAction,
		SIGNAL(triggered(bool)),
		SLOT(viewRefresh()));

	QObject::connect(m_ui.helpAboutAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAbout()));
	QObject::connect(m_ui.helpAboutQtAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAboutQt()));

	QObject::connect(m_pSnapPerBeatComboBox,
		SIGNAL(activated(int)),
		SLOT(snapPerBeatChanged(int)));

	// To handle event selection changes.
	QObject::connect(m_pViewTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(viewTypeChanged(int)));
	QObject::connect(m_pEventTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(eventTypeChanged(int)));
	QObject::connect(m_pControllerComboBox,
		SIGNAL(activated(int)),
		SLOT(controllerChanged(int)));

	QObject::connect(m_pMidiEditor,
		SIGNAL(selectNotifySignal(qtractorMidiEditor *)),
		SLOT(selectionChanged(qtractorMidiEditor *)));
	QObject::connect(m_pMidiEditor,
		SIGNAL(changeNotifySignal(qtractorMidiEditor *)),
		SLOT(contentsChanged(qtractorMidiEditor *)));

	// Try to restore old editor state...
	qtractorOptions  *pOptions  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pOptions = pMainForm->options();
	if (pOptions) {
		// Initial decorations toggle state.
		m_ui.viewMenubarAction->setChecked(pOptions->bMidiMenubar);
		m_ui.viewStatusbarAction->setChecked(pOptions->bMidiStatusbar);
		m_ui.viewToolbarFileAction->setChecked(pOptions->bMidiFileToolbar);
		m_ui.viewToolbarEditAction->setChecked(pOptions->bMidiEditToolbar);
		m_ui.viewToolbarViewAction->setChecked(pOptions->bMidiViewToolbar);
		m_ui.viewToolbarTransportAction->setChecked(pOptions->bMidiTransportToolbar);
		m_ui.viewNoteDurationAction->setChecked(pOptions->bMidiNoteDuration);
		m_ui.viewNoteColorAction->setChecked(pOptions->bMidiNoteColor);
		m_ui.viewValueColorAction->setChecked(pOptions->bMidiValueColor);
		m_ui.viewPreviewAction->setChecked(pOptions->bMidiPreview);
		m_ui.viewFollowAction->setChecked(pOptions->bMidiFollow);
		if (pOptions->bMidiEditMode)
			m_ui.editModeOnAction->setChecked(true);
		else
			m_ui.editModeOffAction->setChecked(true);
		// Initial decorations visibility state.
		viewMenubar(pOptions->bMidiMenubar);
		viewStatusbar(pOptions->bMidiStatusbar);
		viewToolbarFile(pOptions->bMidiFileToolbar);
		viewToolbarEdit(pOptions->bMidiEditToolbar);
		viewToolbarView(pOptions->bMidiViewToolbar);
		viewToolbarTransport(pOptions->bMidiTransportToolbar);
		m_pMidiEditor->setEditMode(pOptions->bMidiEditMode);
		m_pMidiEditor->setNoteColor(pOptions->bMidiNoteColor);
		m_pMidiEditor->setValueColor(pOptions->bMidiValueColor);
		m_pMidiEditor->setNoteDuration(pOptions->bMidiNoteDuration);
		m_pMidiEditor->setSendNotes(pOptions->bMidiPreview);
		m_pMidiEditor->setSyncView(pOptions->bMidiFollow);
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
		// pOptions->loadWidgetGeometry(this);
	}

	// Make last-but-not-least conections....
	if (pMainForm) {
		QObject::connect(m_ui.editSelectRangeAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(editSelectRange()));
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
		QObject::connect(m_ui.transportLoopAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportLoop()));
		QObject::connect(m_ui.transportLoopSetAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportLoopSet()));
		QObject::connect(m_ui.transportPlayAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportPlay()));
	}

	// Default snap-per-beat setting...
	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
	if (pTimeScale) {
		m_pSnapPerBeatComboBox->setCurrentIndex(
			pTimeScale->indexFromSnap(pTimeScale->snapPerBeat()));
	}

	eventTypeChanged(0);
}


// Destructor.
qtractorMidiEditorForm::~qtractorMidiEditorForm (void)
{
	// Drop any widgets around (not really necessary)...
	if (m_pMidiEditor)
		delete m_pMidiEditor;

	// Get edit-mode action group down.
	if (m_pEditModeActionGroup)
		delete m_pEditModeActionGroup;
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Window close event handlers.

// Pre-close event handlers.
bool qtractorMidiEditorForm::queryClose (void)
{
	bool bQueryClose = true;

	// Are we dirty enough to prompt it?
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("The current MIDI clip has been changed:\n\n"
			"\"%1\"\n\n"
			"Do you want to save the changes?")
			.arg(filename()),
			tr("Save"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Save...
			bQueryClose = saveClipFile(false);
			// Fall thru....
		case 1:     // Discard
			break;
		default:    // Cancel.
			bQueryClose = false;
			break;
		}
	}

	// Try to save current editor view state...
	if (bQueryClose && isVisible()) {
		qtractorOptions  *pOptions  = NULL;
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pOptions = pMainForm->options();
		if (pOptions) {
			// Save decorations state.
			pOptions->bMidiMenubar = m_ui.MenuBar->isVisible();
			pOptions->bMidiStatusbar = statusBar()->isVisible();
			pOptions->bMidiFileToolbar = m_ui.fileToolbar->isVisible();
			pOptions->bMidiEditToolbar = m_ui.editToolbar->isVisible();
			pOptions->bMidiViewToolbar = m_ui.viewToolbar->isVisible();
			pOptions->bMidiTransportToolbar = m_ui.transportToolbar->isVisible();
			pOptions->bMidiEditMode = m_ui.editModeOnAction->isChecked();
			pOptions->bMidiNoteDuration = m_ui.viewNoteDurationAction->isChecked();
			pOptions->bMidiNoteColor = m_ui.viewNoteColorAction->isChecked();
			pOptions->bMidiValueColor = m_ui.viewValueColorAction->isChecked();
			pOptions->bMidiPreview = m_ui.viewPreviewAction->isChecked();
			pOptions->bMidiFollow  = m_ui.viewFollowAction->isChecked();
			// Save the dock windows state.
			pOptions->settings().setValue(
				"/MidiEditor/Layout/DockWindows", saveState());
			// And the main windows state?
			// pOptions->saveWidgetGeometry(this);
		}
	}

	return bQueryClose;
}


// On-show event handler.
void qtractorMidiEditorForm::showEvent ( QShowEvent *pShowEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->addEditorForm(this);

	QMainWindow::showEvent(pShowEvent);
}


// On-close event handler.
void qtractorMidiEditorForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	if (queryClose()) {
		// Remove this one from main-form list...
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->removeEditorForm(this);
		// Should always (re)open the clip...
		qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
		if (pMidiClip && pMidiClip->isDirty()) {
			 // Revert to original clip length...
			pMidiClip->setClipLength(m_iClipLength);
			pMidiClip->openMidiFile(
				pMidiClip->filename(),
				pMidiClip->trackChannel());
			if (pMainForm)
				pMainForm->updateNotifySlot(true);
		}
		// Not dirty anymore...
		m_iDirtyCount = 0;
		pCloseEvent->accept();
	} else {
		pCloseEvent->ignore();
	}
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Context menu event handlers.

// Context menu request.
void qtractorMidiEditorForm::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	m_ui.contextMenu->exec(pContextMenuEvent->globalPos());
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


// Editing MIDI clip accessors.
void qtractorMidiEditorForm::setMidiClip ( qtractorMidiClip *pMidiClip  )
{
	if (queryClose()) {
		m_pMidiEditor->setMidiClip(pMidiClip);
		m_iClipLength = pMidiClip->clipLength();
		m_iDirtyCount = 0;
	}
}

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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	// Get those time-scales in sync...
	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
	pTimeScale->copy(*pSession->timeScale());

	// Note that there's two modes for this method:
	// wether pMidiClip is given non-null wich means
	// form initialization and first setup or else... 
	if (pMidiClip) {
		// Set initial MIDI clip properties has seen fit...
		setMidiClip(pMidiClip);
		// Setup connections to main widget...
		QObject::connect(m_pMidiEditor,
			SIGNAL(changeNotifySignal(qtractorMidiEditor *)),
			pMainForm, SLOT(changeNotifySlot(qtractorMidiEditor *)));
		// This one's local but helps...
		QObject::connect(m_pMidiEditor,
			SIGNAL(sendNoteSignal(int,int)),
			SLOT(sendNote(int,int)));
	}

	// Refresh and try to center (vertically) the edit-view...
	m_pMidiEditor->centerContents();

	// (Re)try to position the editor in same of track view...
	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks) {
		unsigned long iFrame = pTimeScale->frameFromPixel(
			(pTracks->trackView())->contentsX());
		if (iFrame  > m_pMidiEditor->offset()) {
			iFrame -= m_pMidiEditor->offset();
		} else {
			iFrame = 0;
		}
		int cx = pTimeScale->pixelFromFrame(iFrame);
		int cy = (m_pMidiEditor->editView())->contentsY();
		(m_pMidiEditor->editView())->setContentsPos(cx, cy);
	}

	// (Re)sync local play/edit-head/tail (avoid follow playhead)...
	m_pMidiEditor->setPlayHead(pSession->playHead(), false);
	m_pMidiEditor->setEditHead(pSession->editHead(), false);
	m_pMidiEditor->setEditTail(pSession->editTail(), false);

	// Done.
	stabilizeForm();
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Clip Action methods.

// Save current clip.
bool qtractorMidiEditorForm::saveClipFile ( bool bPrompt )
{
	// Suggest a filename, if there's none...
	QString sFilename = qtractorMidiEditor::createFilePathRevision(filename());

	if (sFilename.isEmpty())
		bPrompt = true;

	// Ask for the file to save...
	if (bPrompt) {
		// If none is given, assume default directory.
		// Prompt the guy...
		sFilename = QFileDialog::getSaveFileName(
			this,                                        // Parent.
			tr("Save MIDI Clip") + " - " QTRACTOR_TITLE, // Caption.
			sFilename,                                   // Start here.
			tr("MIDI files") + " (*.mid)"                // Filter files.
		);
		// Have we cancelled it?
		if (sFilename.isEmpty())
			return false;
		// Enforce .mid extension...
		const QString sExt("mid");
		if (QFileInfo(sFilename).suffix() != sExt)
			sFilename += '.' + sExt;
	}

	// Save it right away...
	bool bResult = qtractorMidiEditor::saveCopyFile(sFilename,
		filename(), trackChannel(), sequence(), timeScale());

	// Have we done it right?
	if (bResult) {
		// Aha, but we're not dirty no more.
		m_iDirtyCount = 0;
		qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
		if (pMidiClip) {
			pMidiClip->setFilename(sFilename);
			pMidiClip->setDirty(false);
		}
	}

	// Done.
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


// File properties dialog.
void qtractorMidiEditorForm::fileProperties (void)
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip) {
		qtractorClipForm clipForm(parentWidget());
		clipForm.setClip(pMidiClip);
		clipForm.exec();
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

// Set edit-mode on.
void qtractorMidiEditorForm::editModeOn (void)
{
	m_pMidiEditor->setEditMode(true);
	m_pMidiEditor->updateContents();
}

// Set edit-mode off.
void qtractorMidiEditorForm::editModeOff (void)
{
	m_pMidiEditor->setEditMode(false);
	m_pMidiEditor->updateContents();
}


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


// Delete current selection.
void qtractorMidiEditorForm::editDelete (void)
{
	m_pMidiEditor->deleteSelect();
}


// Select none contents.
void qtractorMidiEditorForm::editSelectNone (void)
{
	m_pMidiEditor->selectAll(false, false);
}


// Select invert contents.
void qtractorMidiEditorForm::editSelectInvert (void)
{
	m_pMidiEditor->selectAll(true, true);
}


// Select all contents.
void qtractorMidiEditorForm::editSelectAll (void)
{
	m_pMidiEditor->selectAll(true, false);
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


// Rezize tool.
void qtractorMidiEditorForm::toolsResize (void)
{
	m_pMidiEditor->executeTool(qtractorMidiEditor::Resize);
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- View Action slots.

// Show/hide the main program window menubar.
void qtractorMidiEditorForm::viewMenubar ( bool bOn )
{
	if (bOn)
		m_ui.MenuBar->show();
	else
		m_ui.MenuBar->hide();
}


// Show/hide the main program window statusbar.
void qtractorMidiEditorForm::viewStatusbar ( bool bOn )
{
	if (bOn)
		statusBar()->show();
	else
		statusBar()->hide();
}


// Show/hide the file-toolbar.
void qtractorMidiEditorForm::viewToolbarFile ( bool bOn )
{
	if (bOn) {
		m_ui.fileToolbar->show();
	} else {
		m_ui.fileToolbar->hide();
	}
}


// Show/hide the edit-toolbar.
void qtractorMidiEditorForm::viewToolbarEdit ( bool bOn )
{
	if (bOn) {
		m_ui.editToolbar->show();
	} else {
		m_ui.editToolbar->hide();
	}
}


// Show/hide the view-toolbar.
void qtractorMidiEditorForm::viewToolbarView ( bool bOn )
{
	if (bOn) {
		m_ui.viewToolbar->show();
	} else {
		m_ui.viewToolbar->hide();
	}
}


// Show/hide the transport-toolbar.
void qtractorMidiEditorForm::viewToolbarTransport ( bool bOn )
{
	if (bOn) {
		m_ui.transportToolbar->show();
	} else {
		m_ui.transportToolbar->hide();
	}
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


// View duration as widths of notes
void qtractorMidiEditorForm::viewNoteDuration ( bool bOn )
{
	m_pMidiEditor->setNoteDuration(bOn);
	m_pMidiEditor->updateContents();
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


// Refresh view display.
void qtractorMidiEditorForm::viewRefresh (void)
{
	m_pMidiEditor->updateContents();

	stabilizeForm();
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Help Action slots.

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
void qtractorMidiEditorForm::sendNote ( int iNote, int iVelocity )
{
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip == NULL)
		return;

	qtractorTrack *pTrack = pMidiClip->track();
	if (pTrack == NULL)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus == NULL)
		return;

	pMidiBus->sendNote(pTrack->midiChannel(), iNote, iVelocity);

	// Track output monitoring...
	if (iVelocity > 0) {
		qtractorMidiMonitor *pMidiMonitor
			= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
		if (pMidiMonitor)
			pMidiMonitor->enqueue(qtractorMidiEvent::NOTEON, iVelocity);
	}
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Main form stabilization.

void qtractorMidiEditorForm::stabilizeForm (void)
{
	// Update the main menu state...
	bool bSelected = m_pMidiEditor->isSelected();
	m_ui.fileSaveAction->setEnabled(m_iDirtyCount > 0);
	m_pMidiEditor->updateUndoAction(m_ui.editUndoAction);
	m_pMidiEditor->updateRedoAction(m_ui.editRedoAction);
	m_ui.editCutAction->setEnabled(bSelected);
	m_ui.editCopyAction->setEnabled(bSelected);
	m_ui.editPasteAction->setEnabled(m_pMidiEditor->isClipboard());
	m_ui.editDeleteAction->setEnabled(bSelected);
	m_ui.editSelectNoneAction->setEnabled(bSelected);
	m_ui.toolsMenu->setEnabled(bSelected);

	// Just having a non-null sequence will indicate
	// that we're editing a legal MIDI clip...
	qtractorMidiSequence *pSeq = sequence();
	if (pSeq == NULL) {
		setWindowTitle(tr("MIDI Editor") + " - " QTRACTOR_TITLE);
		m_pFileNameLabel->clear();
		m_pTrackChannelLabel->clear();
		m_pTrackNameLabel->clear();
		m_pStatusModLabel->clear();
		m_pDurationLabel->clear();
		return;
	}

	// Special display formatting...
	QString sTrackChannel;
	int k = 0;
	if (format() == 0) {
		sTrackChannel = tr("Channel %1");
		k++;
	} else {
		sTrackChannel = tr("Track %1");
	}

	// Update the main window caption...
	QString sTitle = QFileInfo(filename()).fileName() + ' ';
	sTitle += '(' + sTrackChannel.arg(trackChannel() + k) + ')';
	if (m_iDirtyCount > 0)
		sTitle += ' ' + tr("[modified]");
	setWindowTitle(sTitle + " - " QTRACTOR_TITLE);

	m_pFileNameLabel->setText(filename());
	m_pTrackChannelLabel->setText(sTrackChannel.arg(trackChannel() + k));
	m_pTrackNameLabel->setText(pSeq->name());
	if (m_iDirtyCount > 0)
		m_pStatusModLabel->setText(tr("MOD"));
	else
		m_pStatusModLabel->clear();

	m_pDurationLabel->setText(
		(m_pMidiEditor->timeScale())->textFromTick(pSeq->duration()));

	qtractorSession  *pSession  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pSession = pMainForm->session();
	if (pSession) {
		unsigned long iPlayHead = pSession->playHead();
		bool bPlaying    = pSession->isPlaying();
		bool bRecording  = pSession->isRecording();
		bool bLooping    = pSession->isLooping();
		bool bEnabled    = (!bPlaying || !bRecording);
		bool bSelectable = (pSession->editHead() < pSession->editTail());
		bool bBumped = (bEnabled && (pSession->playHead() > 0 || bPlaying));
		int iRolling = pMainForm->rolling();
		m_ui.transportBackwardAction->setEnabled(bBumped);
		m_ui.transportRewindAction->setEnabled(bBumped);
		m_ui.transportFastForwardAction->setEnabled(bEnabled);
		m_ui.transportForwardAction->setEnabled(bEnabled
			&& (iPlayHead < pSession->sessionLength()
				|| iPlayHead < pSession->editHead()
				|| iPlayHead < pSession->editTail()));
		m_ui.transportLoopAction->setEnabled(bEnabled
			&& (bLooping || bSelectable));
		m_ui.transportLoopSetAction->setEnabled(bEnabled && bSelectable);
		m_ui.transportRewindAction->setChecked(iRolling < 0);
		m_ui.transportFastForwardAction->setChecked(iRolling > 0);
		m_ui.transportPlayAction->setChecked(bPlaying);
		m_ui.transportLoopAction->setChecked(bLooping);
	}
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Selection widget slots.

// Snap-per-beat spin-box change slot.
void qtractorMidiEditorForm::snapPerBeatChanged ( int iSnap )
{
	qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	// Avoid bogus changes...
	unsigned short iSnapPerBeat = pTimeScale->snapFromIndex(iSnap);
	if (iSnapPerBeat == pTimeScale->snapPerBeat())
		return;

	// No need to express the change as a undoable command?
	pTimeScale->setSnapPerBeat(iSnapPerBeat);
	m_pMidiEditor->editView()->setFocus();
}


// Tool selection handlers.
void qtractorMidiEditorForm::viewTypeChanged ( int iIndex )
{
	qtractorMidiEvent::EventType eventType = qtractorMidiEvent::EventType(
		m_pViewTypeComboBox->itemData(iIndex).toInt());

	m_pMidiEditor->editView()->setEventType(eventType);
	m_pMidiEditor->updateContents();

	stabilizeForm();
}


void qtractorMidiEditorForm::eventTypeChanged ( int iIndex )
{
	qtractorMidiEvent::EventType eventType = qtractorMidiEvent::EventType(
		m_pEventTypeComboBox->itemData(iIndex).toInt());
	m_pControllerComboBox->setEnabled(
		eventType == qtractorMidiEvent::CONTROLLER);

	m_pMidiEditor->editEvent()->setEventType(eventType);
	m_pMidiEditor->updateContents();

	stabilizeForm();
}


void qtractorMidiEditorForm::controllerChanged ( int iIndex )
{
	unsigned char controller = (unsigned char) (
		m_pControllerComboBox->itemData(iIndex).toInt() & 0x7f);

	m_pMidiEditor->editEvent()->setController(controller);
	m_pMidiEditor->updateContents();

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
	qtractorMidiClip *pMidiClip = m_pMidiEditor->midiClip();
	if (pMidiClip)
		pMidiClip->setDirty(true);

	m_iDirtyCount++;
	selectionChanged(pMidiEditor);
}


// end of qtractorMidiEditorForm.cpp
