// qtractorAudioToolsForm.h
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

#ifndef __qtractorAudioToolsForm_h
#define __qtractorAudioToolsForm_h

#include "ui_qtractorAudioToolsForm.h"

#include <QDialog>


// Forward declarations.
class qtractorAudioClip;
class qtractorAudioEditCommand;


//----------------------------------------------------------------------
// class qtractorAudioToolsForm -- Audio tools dialog.
//
// Provides five DSP tools applicable to the current (or selected range
// of the) audio clip:
//   0 – Gain / Panning
//   1 – Normalize
//   2 – TimeStretch
//   3 – PitchShift
//   4 – Reverse
//
// On OK, audioEditCommand() returns a ready-to-execute
// qtractorAudioEditCommand(ApplyTool) that encapsulates all enabled tools.
//

class qtractorAudioToolsForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioToolsForm(QWidget *pParent = nullptr);
	// Destructor.
	~qtractorAudioToolsForm();

	// Select the initial visible tab (uses qtractorAudioEditor::Tool enum values).
	void setToolIndex(int iToolIndex);
	int  toolIndex() const;

	// Produce a ready-to-execute ApplyTool command for the given clip.
	// iStart  – intra-file frame of selection start (0 = file beginning).
	// iLength – length of selection in frames (= clipLength for whole-clip).
	// Returns nullptr when no tool is enabled or parameters are invalid.
	qtractorAudioEditCommand *audioEditCommand(
		qtractorAudioClip *pAudioClip,
		unsigned long iStart,
		unsigned long iLength);

protected slots:

	// Preset management slots.
	void presetChanged(const QString& sPreset);
	void presetActivated(int iPreset);
	void presetSave();
	void presetDelete();

	// Change notification.
	void changed();

	// Dialog button slots.
	void accept();
	void reject();

	// Form stabilization.
	void stabilizeForm();

protected:

	// Preset management methods.
	void loadPreset(const QString& sPreset);
	void savePreset(const QString& sPreset);
	void refreshPresets();

private:

	// The Qt-designer UI struct.
	Ui::qtractorAudioToolsForm m_ui;

	// Dirty / update guard counters.
	int m_iDirtyCount;
	int m_iUpdate;
};


#endif	// __qtractorAudioToolsForm_h

// end of qtractorAudioToolsForm.h
