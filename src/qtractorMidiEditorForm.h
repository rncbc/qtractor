// qtractorMidiEditorForm.h
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

#ifndef __qtractorMidiEditorForm_h
#define __qtractorMidiEditorForm_h

#include "ui_qtractorMidiEditorForm.h"


// Forward declarations...
class qtractorMidiEditor;
class qtractorMidiClip;
class qtractorMidiSequence;
class qtractorTimeScale;

class qtractorMidiEventList;

class qtractorMidiControlTypeGroup;

class QContextMenuEvent;
class QActionGroup;
class QComboBox;
class QLabel;


//----------------------------------------------------------------------------
// qtractorMidiEditorForm -- UI wrapper form.

class qtractorMidiEditorForm : public QMainWindow
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEditorForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorMidiEditorForm();

	// MIDI editor widget accessor.
	qtractorMidiEditor *editor() const;

	// Local time-scale accessor.
	qtractorTimeScale *timeScale() const;
	unsigned long timeOffset() const;

	// MIDI clip sequence accessors.
	qtractorMidiClip *midiClip() const;

	// MIDI clip properties accessors.
	const QString& filename() const;
	unsigned short trackChannel() const;
	unsigned short format() const;

	qtractorMidiSequence *sequence() const;

	// Special executive setup method.
	void setup(qtractorMidiClip *pMidiClip = NULL);

	// Reset coomposite dirty flag.
	void resetDirtyCount();

	// Instrument/controller names update.
	void updateInstrumentNames();

	// Pre-close event handler.
	bool queryClose();

	// Edit menu accessor.
	QMenu *editMenu() const;
	
	// Save(as) warning message box.
	static int querySave(const QString& sFilename);

	// Update thumb-view play-head...
	void updatePlayHead(unsigned long iPlayHead);

public slots:

	void stabilizeForm();

protected slots:

	void fileSave();
	void fileSaveAs();
	void fileUnlink();
	void fileTrackInputs();
	void fileTrackOutputs();
	void fileTrackProperties();
	void fileProperties();
	void fileRangeSet();
	void fileLoopSet();
	void fileClose();

	void editUndo();
	void editRedo();
	void editCut();
	void editCopy();
	void editPaste();
	void editPasteRepeat();
	void editDelete();
	void editModeOn();
	void editModeOff();
	void editModeDraw(bool bOn);
	void editSelectAll();
	void editSelectNone();
	void editSelectInvert();
	void editSelectRange();
	void editInsertRange();
	void editRemoveRange();

	void toolsQuantize();
	void toolsTranspose();
	void toolsNormalize();
	void toolsRandomize();
	void toolsResize();
	void toolsRescale();
	void toolsTimeshift();

	void viewMenubar(bool bOn);
	void viewStatusbar(bool bOn);
	void viewToolbarFile(bool bOn);
	void viewToolbarEdit(bool bOn);
	void viewToolbarView(bool bOn);
	void viewToolbarTransport(bool bOn);
	void viewToolbarScale(bool bOn);
	void viewToolbarThumb(bool bOn);
	void viewNoteDuration(bool bOn);
	void viewNoteColor(bool bOn);
	void viewValueColor(bool bOn);
	void viewEvents(bool bOn);
	void viewPreview(bool bOn);
	void viewFollow(bool bOn);
	void viewZoomIn();
	void viewZoomOut();
	void viewZoomReset();
	void viewZoomHorizontal();
	void viewZoomVertical();
	void viewZoomAll();
	void viewSnap();
	void viewScaleKey();
	void viewScaleType();
	void viewSnapZebra(bool bOn);
	void viewSnapGrid(bool bOn);
	void viewToolTips(bool bOn);
	void viewRefresh();

	void helpShortcuts();
	void helpAbout();
	void helpAboutQt();

	void updateZoomMenu();
	void updateSnapMenu();
	void updateScaleMenu();

	void sendNote(int iNote, int iVelocity);

	void selectionChanged(qtractorMidiEditor *);
	void contentsChanged(qtractorMidiEditor *);

	// Event type selection slots.
	void viewTypeChanged(int);
	void eventTypeChanged(int);
	void eventParamChanged(int);

	void snapToScaleKeyChanged(int iSnapToScaleKey);
	void snapToScaleTypeChanged(int iSnapToScaleType);

	void snapPerBeatChanged(int iSnapPerBeat);

protected:

	// On-open/close event handlers.
	void showEvent(QShowEvent *pShowEvent);
	void closeEvent(QCloseEvent *pCloseEvent);

	// Context menu request.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Save current clip track-channel sequence.
	bool saveClipFile(bool bPrompt);

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiEditorForm m_ui;

	// Instance variables...
	qtractorMidiEditor *m_pMidiEditor;

	// Event list dockable widget.
	qtractorMidiEventList *m_pMidiEventList;

	int m_iDirtyCount;

	// Edit-mode action group up.
	QActionGroup *m_pEditModeActionGroup;

	// View/Snap-to-beat actions (for shortcuts access)
	QList<QAction *> m_snapPerBeatActions;

	// Edit snap mode.
	QComboBox *m_pSnapPerBeatComboBox;

	// Event type selection widgets...
	QComboBox *m_pViewTypeComboBox;
	QComboBox *m_pEventTypeComboBox;
	QComboBox *m_pEventParamComboBox;

	qtractorMidiControlTypeGroup *m_pEventTypeGroup;

	// Snap-to-scale/quantize selection widgets...
	QComboBox *m_pSnapToScaleKeyComboBox;
	QComboBox *m_pSnapToScaleTypeComboBox;

	// Status items.
	QLabel *m_pTrackNameLabel;
	QLabel *m_pFileNameLabel;
	QLabel *m_pTrackChannelLabel;
	QLabel *m_pStatusModLabel;
	QLabel *m_pDurationLabel;
};


#endif	// __qtractorMidiEditorForm_h


// end of qtractorMidiEditorForm.h
