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

#ifndef CONFIG_TEST
#include "qtractorSession.h"
#include "qtractorClipForm.h"
#include "qtractorMidiClip.h"
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

  	insertToolBarBreak(m_ui.editViewToolbar);

	const QSize pad(4, 0);
	const QString spc(4, ' ');

	// Status filename...
	m_pFileNameLabel = new QLabel(spc);
	m_pFileNameLabel->setAlignment(Qt::AlignLeft);
	m_pFileNameLabel->setToolTip(tr("MIDI file name"));
	m_pFileNameLabel->setAutoFillBackground(true);
	statusBar()->addWidget(m_pFileNameLabel, 2);

	// Status clip/sequence name...
	m_pTrackNameLabel = new QLabel(spc);
	m_pTrackNameLabel->setAlignment(Qt::AlignLeft);
	m_pTrackNameLabel->setToolTip(tr("MIDI sequence name"));
	m_pTrackNameLabel->setAutoFillBackground(true);
	statusBar()->addWidget(m_pTrackNameLabel, 1);

	// Status track/channel number...
	m_pTrackChannelLabel = new QLabel(spc);
	m_pTrackChannelLabel->setAlignment(Qt::AlignHCenter);
	m_pTrackChannelLabel->setMinimumSize(m_pTrackChannelLabel->sizeHint() + pad);
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

	// Some actions surely need those
	// shortcuts firmly attached...
	addAction(m_ui.viewMenubarAction);

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

	// This is the initial state and selection.
	m_ui.editModeOffAction->setChecked(true);

	// Initial decorations toggle state.
	m_ui.viewMenubarAction->setChecked(true);
	m_ui.viewStatusbarAction->setChecked(true);
	m_ui.viewToolbarFileAction->setChecked(true);
	m_ui.viewToolbarEditAction->setChecked(true);
	m_ui.viewToolbarViewAction->setChecked(true);

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
			bQueryClose = saveFile(false);
			// Fall thru....
		case 1:     // Discard
			break;
		default:    // Cancel.
			bQueryClose = false;
			break;
		}
	}

	return bQueryClose;
}


// On-close event handlers.
void qtractorMidiEditorForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	if (queryClose()) {
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
	m_ui.editMenu->exec(pContextMenuEvent->globalPos());
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Central widget redirect methods.

// MIDI editor widget accessor.
qtractorMidiEditor *qtractorMidiEditorForm::editor (void) const
{
	return m_pMidiEditor;
}


// MIDI filename accessors.
void qtractorMidiEditorForm::setFilename ( const QString& sFilename )
{
	m_sFilename = sFilename;
#ifndef CONFIG_TEST
	if (m_pMidiClip)
		m_pMidiClip->setFilename(m_sFilename);
#endif
}


// MIDI track-channel accessors.
void qtractorMidiEditorForm::setTrackChannel ( unsigned short iTrackChannel )
{
	m_iTrackChannel = iTrackChannel;
#ifndef CONFIG_TEST
	if (m_pMidiClip)
		m_pMidiClip->setTrackChannel(m_iTrackChannel);
#endif
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
	if (queryClose()) {
		m_iDirtyCount = 0;
		m_pMidiClip = pMidiClip;
		qtractorTrack *pTrack = NULL;
		qtractorSession *pSession = NULL;
		if (m_pMidiClip)
			pTrack = m_pMidiClip->track();
		if (pTrack)
			pSession = pTrack->session();
		if (pSession) {
			// Adjust MIDI editor time-scale fundamentals...
			qtractorTimeScale *pTimeScale = m_pMidiEditor->timeScale();
			pTimeScale->setSampleRate(pSession->sampleRate());
			pTimeScale->setTempo(pSession->tempo());
			pTimeScale->setTicksPerBeat(pSession->ticksPerBeat());
			pTimeScale->setBeatsPerBar(pSession->beatsPerBar());
			pTimeScale->setSnapPerBeat(pSession->snapPerBeat());
			pTimeScale->updateScale();
			// Now set the editing MIDI sequence alright...
			m_pMidiEditor->setSequence(m_pMidiClip->sequence());
			m_sFilename = m_pMidiClip->filename();
			m_iTrackChannel = m_pMidiClip->trackChannel();
		} else {
			m_pMidiEditor->setSequence(NULL);
			m_sFilename.clear();
			m_iTrackChannel = 0;
		}
		stabilizeForm();
	}
}

qtractorMidiClip *qtractorMidiEditorForm::midiClip (void) const
{
	return m_pMidiClip;
}

#endif


qtractorTimeScale *qtractorMidiEditorForm::timeScale (void) const
{
	return m_pMidiEditor->timeScale();
}

qtractorMidiSequence *qtractorMidiEditorForm::sequence (void) const
{
	return m_pMidiEditor->sequence();
}

const QString& qtractorMidiEditorForm::filename (void) const
{
	return m_sFilename;
}

unsigned short qtractorMidiEditorForm::trackChannel (void) const
{
	return m_iTrackChannel;
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
bool qtractorMidiEditorForm::saveFile ( bool bPrompt )
{
	// Suggest a filename, if there's none...
	QString sFilename = qtractorMidiEditor::createFilenameRevision(filename());

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
		setFilename(sFilename);
		m_iDirtyCount = 0;
#ifndef CONFIG_TEST
		m_pMidiEditor->contentsChangeNotify();
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
	saveFile(false);
}


// Save current clip with another name.
void qtractorMidiEditorForm::fileSaveAs (void)
{
	saveFile(true);
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
		qtractorClipForm clipForm(this);
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

	stabilizeForm();
}

// Set edit-mode off.
void qtractorMidiEditorForm::editModeOff (void)
{
	m_pMidiEditor->setEditMode(false);

	stabilizeForm();
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


// Refresh view display.
void qtractorMidiEditorForm::viewRefresh (void)
{
	m_pMidiEditor->refresh();

	stabilizeForm();
}


//-------------------------------------------------------------------------
// qtractorMidiEditorForm -- Main form stabilization.

void qtractorMidiEditorForm::stabilizeForm (void)
{
	// Update the main window caption...
	QString sTitle = QFileInfo(filename()).fileName();
	sTitle += ':' + QString::number(trackChannel());
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
	m_pTrackChannelLabel->setText(QString::number(trackChannel()));

	if (sequence())
		m_pTrackNameLabel->setText(sequence()->name());
	else
		m_pTrackNameLabel->clear();

	if (m_iDirtyCount > 0)
		m_pStatusModLabel->setText(tr("MOD"));
	else
		m_pStatusModLabel->clear();
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
