// qtractorMidiEditorForm.h
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

#ifndef __qtractorMidiEditorForm_h
#define __qtractorMidiEditorForm_h

#include "ui_qtractorMidiEditorForm.h"


// Forward declarations...
class qtractorMidiEditor;
class qtractorMidiSequence;
class qtractorTimeScale;

#ifndef QTRACTOR_TEST
class qtractorMidiClip;
#endif

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

	// MIDI clip properties accessors.
	void setFilename(const QString& sFilename);
	const QString& filename() const;

	void setTrackChannel(unsigned short iTrackChannel);
	unsigned short trackChannel() const;

	void setFormat(unsigned short iFormat);
	unsigned short format() const;

#ifdef QTRACTOR_TEST
	// MIDI clip sequence accessor.
	void setSequence(qtractorMidiSequence *pSeq);
#else
	// MIDI clip accessors.
	void setMidiClip(qtractorMidiClip *pMidiClip);
	qtractorMidiClip *midiClip() const;
	// Special executive setup method.
	void setup(qtractorMidiClip *pMidiClip = NULL);
#endif

	// MIDI clip sequence accessors.
	qtractorMidiSequence *sequence() const;

	// MIDI event foreground (outline) color.
	void setForeground(const QColor& fore);
	const QColor& foreground() const;

	// MIDI event background (fill) color.
	void setBackground(const QColor& back);
	const QColor& background() const;

	// General update method.
	void updateContents();

	// Pre-close event handler.
	bool queryClose();

public slots:

	void fileSave();
	void fileSaveAs();
	void fileProperties();
	void fileClose();

	void editModeOn();
	void editModeOff();

	void editUndo();
	void editRedo();

	void editCut();
	void editCopy();
	void editPaste();
	void editDelete();

	void editSelectNone();
	void editSelectAll();
	void editSelectInvert();

	void toolsQuantize();
	void toolsTranspose();
	void toolsNormalize();
	void toolsRandomize();
	void toolsResize();

	void viewMenubar(bool bOn);
	void viewStatusbar(bool bOn);
	void viewToolbarFile(bool bOn);
	void viewToolbarEdit(bool bOn);
	void viewToolbarView(bool bOn);
	void viewDuration(bool bOn);
	void viewPreview(bool bOn);
	void viewFollow(bool bOn);
	void viewRefresh();

	void helpAbout();
	void helpAboutQt();

	void sendNote(int iNote, int iVelocity);

	void selectionChanged(qtractorMidiEditor *);
	void contentsChanged(qtractorMidiEditor *);

	void stabilizeForm();

protected slots:

	// Event type selection slots.
	void viewTypeChanged(int);
	void eventTypeChanged(int);
	void controllerChanged(int);

	void snapPerBeatChanged(int iSnap);

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

#ifndef QTRACTOR_TEST
	qtractorMidiClip *m_pMidiClip;
#endif

	QString        m_sFilename;
	unsigned short m_iTrackChannel;
	unsigned short m_iFormat;

	int m_iDirtyCount;

	// Edit-mode action group up.
	QActionGroup *m_pEditModeActionGroup;

	// Edit snap mode.
	QComboBox *m_pSnapPerBeatComboBox;

	// Event type selection widgets...
	QComboBox *m_pViewTypeComboBox;
	QComboBox *m_pEventTypeComboBox;
	QComboBox *m_pControllerComboBox;

	// Status items.
	QLabel *m_pTrackNameLabel;
	QLabel *m_pFileNameLabel;
	QLabel *m_pTrackChannelLabel;
	QLabel *m_pStatusModLabel;
	QLabel *m_pDurationLabel;
};


#endif	// __qtractorMidiEditorForm_h


// end of qtractorMidiEditorForm.h
