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

#ifdef CONFIG_TEST
#define QTRACTOR_TITLE "qtractorMidiEditorTest"
#else
#include "qtractorAbout.h"
#endif

#include "qtractorMidiEditorForm.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditView.h"
#include "qtractorMidiEditEvent.h"

#ifdef CONFIG_TEST
#include "qtractorMidiSequence.h"
#else
#include "qtractorMidiClip.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"
#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorTrackView.h"
#include "qtractorMainForm.h"
#include "qtractorClipForm.h"
#endif

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
	QWidget *pParent, Qt::WFlags wflags ) : QMainWindow(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

#ifndef CONFIG_TEST
	m_pMidiClip = NULL;
#endif

	m_iTrackChannel = 0;
	m_iFormat = 0;

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
	m_pSnapPerBeatComboBox->addItem(tr("None"));
	QString sPrefix = tr("Beat");
	m_pSnapPerBeatComboBox->addItem(sPrefix);
	sPrefix += "/%1";
	for (unsigned short iSnapPerBeat = 2; iSnapPerBeat < 64; iSnapPerBeat <<= 1)
		m_pSnapPerBeatComboBox->addItem(sPrefix.arg(iSnapPerBeat));
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
	// Special integration one.
	addAction(m_ui.transportBackwardAction);
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
	QObject::connect(m_ui.viewPreviewAction,
		SIGNAL(triggered(bool)),
		SLOT(viewPreview(bool)));
	QObject::connect(m_ui.viewFollowAction,
		SIGNAL(triggered(bool)),
		SLOT(viewFollow(bool)));
	QObject::connect(m_ui.viewRefreshAction,
		SIGNAL(triggered(bool)),
		SLOT(viewRefresh()));

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
		SIGNAL(selectNotifySignal()),
		SLOT(stabilizeForm()));
	QObject::connect(m_pMidiEditor,
		SIGNAL(changeNotifySignal()),
		SLOT(contentsChanged()));

#ifdef CONFIG_TEST

	// This is the initial state and selection.
	m_ui.editModeOffAction->setChecked(true);
	m_pMidiEditor->setEditMode(false);

	// Initial decorations toggle state.
	m_ui.viewMenubarAction->setChecked(true);
	m_ui.viewStatusbarAction->setChecked(true);
	m_ui.viewToolbarFileAction->setChecked(true);
	m_ui.viewToolbarEditAction->setChecked(true);
	m_ui.viewToolbarViewAction->setChecked(true);

	// Some windows are forced initially as is...
	insertToolBarBreak(m_ui.editViewToolbar);

	m_ui.filePropertiesAction->setEnabled(false);

#else

	// Try to restore old editor state...
	qtractorOptions *pOptions = NULL;
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
		m_pMidiEditor->setEditMode(pOptions->bMidiEditMode);
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
		QObject::connect(m_ui.transportBackwardAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportBackward()));
		QObject::connect(m_ui.transportPlayAction,
			SIGNAL(triggered(bool)),
			pMainForm, SLOT(transportPlay()));
	}

#endif

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

#ifndef CONFIG_TEST
	// Try to save current editor view state...
	if (bQueryClose && isVisible()) {
		qtractorOptions *pOptions = NULL;
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
			pOptions->bMidiEditMode = m_ui.editModeOnAction->isChecked();
			pOptions->bMidiPreview = m_ui.viewPreviewAction->isChecked();
			pOptions->bMidiFollow  = m_ui.viewFollowAction->isChecked();
			// Save the dock windows state.
			pOptions->settings().setValue(
				"/MidiEditor/Layout/DockWindows", saveState());
			// And the main windows state?
			// pOptions->saveWidgetGeometry(this);
		}
	}
#endif

	return bQueryClose;
}


// On-show event handler.
void qtractorMidiEditorForm::showEvent ( QShowEvent *pShowEvent )
{
#ifndef CONFIG_TEST
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->addEditor(m_pMidiEditor);
#endif

	QMainWindow::showEvent(pShowEvent);
}


// On-close event handler.
void qtractorMidiEditorForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	if (queryClose()) {
#ifndef CONFIG_TEST
		// Remove this one from main-form list...
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->removeEditor(m_pMidiEditor);
#endif
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
	m_ui.editMenu->exec(pContextMenuEvent->globalPos());
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


// MIDI filename accessors.
void qtractorMidiEditorForm::setFilename ( const QString& sFilename )
{
	m_sFilename = sFilename;
}

const QString& qtractorMidiEditorForm::filename (void) const
{
	return m_sFilename;
}


// MIDI track-channel accessors.
void qtractorMidiEditorForm::setTrackChannel ( unsigned short iTrackChannel )
{
	m_iTrackChannel = iTrackChannel;
}

unsigned short qtractorMidiEditorForm::trackChannel (void) const
{
	return m_iTrackChannel;
}


// MIDI file format accessors.
void qtractorMidiEditorForm::setFormat ( unsigned short iFormat )
{
	m_iFormat = iFormat;
}

unsigned short qtractorMidiEditorForm::format (void) const
{
	return m_iFormat;
}


#ifdef CONFIG_TEST

// Editing MIDI event sequence accessors.
void qtractorMidiEditorForm::setSequence ( qtractorMidiSequence *pSeq  )
{
	if (queryClose()) {
		m_iDirtyCount = 0;
		m_pMidiEditor->setSequence(pSeq);
		stabilizeForm();
	}
}

#else


// Editing MIDI clip accessors.
void qtractorMidiEditorForm::setMidiClip ( qtractorMidiClip *pMidiClip  )
{
	if (!queryClose())
		return;

	m_iDirtyCount = 0;

	qtractorTrack *pTrack = NULL;
	qtractorSession *pSession = NULL;
	if (pMidiClip)
		pTrack = pMidiClip->track();
	if (pTrack)
		pSession = pTrack->session();
	if (pSession) {
		// Adjust MIDI editor time-scale fundamentals...
		m_pMidiClip = pMidiClip;
		// Set its most outstanding properties...
		m_pMidiEditor->setForeground(pTrack->foreground());
		m_pMidiEditor->setBackground(pTrack->background());
		// Now set the editing MIDI sequence alright...
		m_pMidiEditor->setOffset(m_pMidiClip->clipStart());
		m_pMidiEditor->setLength(m_pMidiClip->clipLength());
		m_pMidiEditor->setSequence(m_pMidiClip->sequence());
		m_sFilename = m_pMidiClip->filename();
		m_iTrackChannel = m_pMidiClip->trackChannel();
		m_iFormat = m_pMidiClip->format();
	} else {
		// No clip, no chips.
		m_pMidiClip = NULL;
		m_pMidiEditor->setOffset(0);
		m_pMidiEditor->setLength(0);
		m_pMidiEditor->setSequence(NULL);
		m_sFilename.clear();
		m_iTrackChannel = 0;
		m_iFormat = 0;
	}
}

qtractorMidiClip *qtractorMidiEditorForm::midiClip (void) const
{
	return m_pMidiClip;
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

	// Set the MIDI clip properties has seen fit...
	if (pMidiClip) {
		// Do the real thing...
		setMidiClip(pMidiClip);
		// Setup connections to main widget...
		QObject::connect(m_pMidiEditor,
			SIGNAL(changeNotifySignal()),
			pMainForm, SLOT(changeNotifySlot()));
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

	// (Re)sync local play-head (avoid follow playhead)...
	m_pMidiEditor->setPlayHead(pSession->playHead(), false);

	// Done.
	stabilizeForm();
}

#endif


qtractorMidiSequence *qtractorMidiEditorForm::sequence (void) const
{
	return m_pMidiEditor->sequence();
}


// MIDI event foreground (outline) color.
void qtractorMidiEditorForm::setForeground ( const QColor& fore )
{
	m_pMidiEditor->setForeground(fore);
}

const QColor& qtractorMidiEditorForm::foreground (void) const
{
	return m_pMidiEditor->foreground();
}


// MIDI event background (fill) color.
void qtractorMidiEditorForm::setBackground ( const QColor& back )
{
	m_pMidiEditor->setBackground(back);
}

const QColor& qtractorMidiEditorForm::background (void) const
{
	return m_pMidiEditor->background();
}


// Editing contents updater.
void qtractorMidiEditorForm::updateContents (void)
{
	m_pMidiEditor->updateContents();

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
		// First, set our resulting filename.
		setFilename(sFilename);
		// Aha, but we're not dirty no more.
		m_iDirtyCount = 0;
#ifndef CONFIG_TEST
		if (m_pMidiClip) {
			m_pMidiClip->setFilename(sFilename);
			m_pMidiClip->setDirty(false);
		}
#endif
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
	// We need an official stance whether current changes
	// are to be committed, otherwise the clip-properties
	// dialog will override the track/sequence changes...
	if (!queryClose())
		return;

#ifndef CONFIG_TEST
	if (m_pMidiClip) {
		qtractorClipForm clipForm(parentWidget());
		clipForm.setClip(m_pMidiClip);
		clipForm.exec();
	}
#endif
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
}

// Set edit-mode off.
void qtractorMidiEditorForm::editModeOff (void)
{
	m_pMidiEditor->setEditMode(false);
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
	m_pMidiEditor->refresh();

	stabilizeForm();
}


// Send note on/off to respective output bus.
void qtractorMidiEditorForm::sendNote ( int iNote, int iVelocity )
{
#ifdef CONFIG_TEST

	fprintf(stderr, "qtractorMidiEditorForm::sendNote(%d, %d)\n", iNote, iVelocity);

#else

	if (m_pMidiClip == NULL)
		return;

	qtractorTrack *pTrack = m_pMidiClip->track();
	if (pTrack == NULL)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus == NULL)
		return;

	pMidiBus->sendNote(pTrack->midiChannel(), iNote, iVelocity);

	// Track input monitoring...
	if (iVelocity > 0) {
		qtractorMidiMonitor *pMidiMonitor
			= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
		if (pMidiMonitor)
			pMidiMonitor->enqueue(qtractorMidiEvent::NOTEON, iVelocity);
		pMidiMonitor = pMidiBus->midiMonitor_out();
		if (pMidiMonitor)
			pMidiMonitor->enqueue(qtractorMidiEvent::NOTEON, iVelocity);
	}

#endif
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Main form stabilization.

void qtractorMidiEditorForm::stabilizeForm (void)
{
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

	m_pFileNameLabel->setText(filename());
	m_pTrackChannelLabel->setText(sTrackChannel.arg(trackChannel() + k));

	if (sequence())
		m_pTrackNameLabel->setText(sequence()->name());
	else
		m_pTrackNameLabel->clear();

	if (m_iDirtyCount > 0)
		m_pStatusModLabel->setText(tr("MOD"));
	else
		m_pStatusModLabel->clear();

	if (sequence()) {
		m_pDurationLabel->setText(
			(m_pMidiEditor->timeScale())->textFromTick(sequence()->duration()));
	} else {
		m_pDurationLabel->clear();
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
}


// Tool selection handlers.
void qtractorMidiEditorForm::viewTypeChanged ( int iIndex )
{
	qtractorMidiEvent::EventType eventType = qtractorMidiEvent::EventType(
		m_pViewTypeComboBox->itemData(iIndex).toInt());

	m_pMidiEditor->editView()->setEventType(eventType);
	updateContents();
}


void qtractorMidiEditorForm::eventTypeChanged ( int iIndex )
{
	qtractorMidiEvent::EventType eventType = qtractorMidiEvent::EventType(
		m_pEventTypeComboBox->itemData(iIndex).toInt());
	m_pControllerComboBox->setEnabled(
		eventType == qtractorMidiEvent::CONTROLLER);

	m_pMidiEditor->editEvent()->setEventType(eventType);
	updateContents();
}


void qtractorMidiEditorForm::controllerChanged ( int iIndex )
{
	unsigned char controller = (unsigned char) (
		m_pControllerComboBox->itemData(iIndex).toInt() & 0x7f);

	m_pMidiEditor->editEvent()->setController(controller);
	updateContents();
}


void qtractorMidiEditorForm::contentsChanged (void)
{
#ifndef CONFIG_TEST
	if (m_pMidiClip)
		m_pMidiClip->setDirty(true);
#endif

	m_iDirtyCount++;
	stabilizeForm();
}


// end of qtractorMidiEditorForm.cpp
