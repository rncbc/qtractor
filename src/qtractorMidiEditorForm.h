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

#ifndef CONFIG_TEST
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
	qtractorMidiEditorForm(QWidget *pParent = 0, Qt::WFlags wflags = 0);
	// Destructor.
	~qtractorMidiEditorForm();

	// MIDI editor widget accessor.
	qtractorMidiEditor *editor() const;

	// MIDI clip properties accessors.
	void setFilename(const QString& sFilename);
	void setTrackChannel(unsigned short iTrackChannel);
#ifdef CONFIG_TEST
	void setSequence(qtractorMidiSequence *pSeq);
#else
	// MIDI clip accessors.
	void setMidiClip(qtractorMidiClip *pMidiClip);
	qtractorMidiClip *midiClip() const;
#endif

	// MIDI clip properties accessors.
	const QString& filename() const;
	unsigned short trackChannel() const;
	qtractorMidiSequence *sequence() const;

	// MIDI event foreground (outline) color.
	void setForeground(const QColor& fore);
	const QColor& foreground() const;

	// MIDI event background (fill) color.
	void setBackground(const QColor& back);
	const QColor& background() const;

	// General update method.
	void updateContents();

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

	void viewMenubar(bool bOn);
	void viewStatusbar(bool bOn);
	void viewToolbarFile(bool bOn);
	void viewToolbarEdit(bool bOn);
	void viewToolbarView(bool bOn);
	void viewRefresh();

	void contentsChanged();

	void stabilizeForm();

protected slots:

	// Event type selection slots.
	void viewTypeChanged(int);
	void eventTypeChanged(int);
	void controllerChanged(int);

	void snapPerBeatChanged(int iSnap);

protected:

	// Pre-close event handler.
	bool queryClose();

	// On-close event handlers.
	void closeEvent(QCloseEvent *pCloseEvent);

	// Context menu request.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Save current clip track-channel sequence.
	bool saveClip(bool bPrompt);
	
	// Save current clip track-channel sequence stand-alone method.
	bool saveClipFile(const QString& sOldFilename, const QString& sNewFilename,
		unsigned short iTrackChannel, qtractorMidiSequence *pSeq);
	
	// Create filename revision.
	static QString createFilenameRevision(
		const QString& sFilename, int iRevision = 0);

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiEditorForm m_ui;

	// Instance variables...
	qtractorMidiEditor *m_pMidiEditor;

#ifndef CONFIG_TEST
	qtractorMidiClip *m_pMidiClip;
#endif

	QString        m_sFilename;
	unsigned short m_iTrackChannel;

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
	QLabel *m_pFileNameLabel;
	QLabel *m_pTrackNameLabel;
	QLabel *m_pTrackChannelLabel;
	QLabel *m_pStatusModLabel;
};


#endif	// __qtractorMidiEditorForm_h


// end of qtractorMidiEditorForm.h
