// qtractorAudioEditorForm.h
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

#ifndef __qtractorAudioEditorForm_h
#define __qtractorAudioEditorForm_h

#include "qtractorEditorForm.h"

#include "ui_qtractorAudioEditorForm.h"


// Forward declarations...
class qtractorAudioEditor;
class qtractorAudioClip;
class qtractorTimeScale;

class qtractorTimeSpinBox;
class qtractorTempoSpinBox;
class qtractorTempoCursor;

class QContextMenuEvent;
class QActionGroup;
class QToolButton;
class QComboBox;
class QLabel;
class QPalette;


//----------------------------------------------------------------------------
// qtractorAudioEditorForm -- UI wrapper form.

class qtractorAudioEditorForm : public qtractorEditorForm
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioEditorForm(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags());
	// Destructor.
	~qtractorAudioEditorForm();

	// Audio editor widget accessor.
	qtractorEditor *editor() const;

	// Audio clip sequence accessors.
	qtractorAudioClip *audioClip() const;

	// Audio clip properties accessors.
	const QString& filename() const;

	// Special executive setup method.
	void setup(qtractorAudioClip *pAudioClip = nullptr);

	// Reset coomposite dirty flag.
	void resetDirtyCount();

	// Pre-close event handler.
	bool queryClose();

	// Edit menu accessor.
	QMenu *editMenu() const;
	
	// Save(as) warning message box.
	static int querySave(const QString& sFilename, QWidget *pParent = nullptr);

	// Update thumb-view play-head...
	void updatePlayHead(unsigned long iPlayHead);

	// Update local time-scale...
	void updateTimeScale();

public slots:

	void stabilizeForm();

protected slots:

	void fileSave();
	void fileSaveAs();
	void fileMute();
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
	void editSelectAll();
	void editSelectNone();
	void editSelectInvert();
	void editSelectRange();

	void viewMenubar(bool bOn);
	void viewStatusbar(bool bOn);
	void viewToolbarFile(bool bOn);
	void viewToolbarEdit(bool bOn);
	void viewToolbarView(bool bOn);
	void viewToolbarTransport(bool bOn);
	void viewToolbarTime(bool bOn);
	void viewToolbarLocked(bool bOn);
	void viewZoomIn();
	void viewZoomOut();
	void viewZoomReset();
	void viewZoomHorizontal();
	void viewZoomVertical();
	void viewZoomAll();
	void viewSnap();
	void viewSnapZebra(bool bOn);
	void viewSnapGrid(bool bOn);
	void viewToolTips(bool bOn);
	void viewRefresh();
	void viewFollow(bool bOn);

	void transportStepBackward();
	void transportStepForward();

	void helpShortcuts();
	void helpAbout();
	void helpAboutQt();

	void updateZoomMenu();
	void updateSnapMenu();

	void selectionChanged(QObject *);
	void contentsChanged(QObject *);

	void transportTimeFormatChanged(int iDisplayFormat);
	void transportTimeChanged(unsigned long iPlayHead);
	void transportTimeFinished();

	void transportTempoChanged(float fTempo,
		unsigned short iBeatsPerBar, unsigned short iBeatDivisor);
	void transportTempoFinished();
	void transportTempoContextMenu(const QPoint& pos);

	void snapPerBeatChanged(int iSnapPerBeat);

	// Top-level window geometry related slots.
	void posChanged();
	void sizeChanged();

protected:

	// On-close event handler.
	void closeEvent(QCloseEvent *pCloseEvent);

	// Context menu request.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Save current clip.
	bool saveClipFile(bool bPrompt);

private:

	// The Qt-designer UI struct...
	Ui::qtractorAudioEditorForm m_ui;

	// Instance variables...
	qtractorAudioEditor *m_pAudioEditor;

	int m_iDirtyCount;

	// Transport tempo/time-signature tracker.
	qtractorTempoCursor *m_pTempoCursor;

	// Transport time/tempo widgets.
	qtractorTimeSpinBox *m_pTimeSpinBox;
	qtractorTempoSpinBox *m_pTempoSpinBox;

	// View/Snap-to-beat actions (for shortcuts access)
	QList<QAction *> m_snapPerBeatActions;

	// Edit snap mode.
	QComboBox *m_pSnapPerBeatComboBox;

	// Status items.
	QLabel *m_pTrackNameLabel;
	QLabel *m_pFileNameLabel;
	QLabel *m_pStatusModLabel;
	QLabel *m_pStatusMuteLabel;
	QLabel *m_pDurationLabel;

	QPalette *m_pRedPalette;
	QPalette *m_pYellowPalette;
};


#endif	// __qtractorAudioEditorForm_h


// end of qtractorAudioEditorForm.h

