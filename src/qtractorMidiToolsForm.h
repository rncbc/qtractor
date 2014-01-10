// qtractorMidiToolsForm.h
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

#ifndef __qtractorMidiToolsForm_h
#define __qtractorMidiToolsForm_h

#include "ui_qtractorMidiToolsForm.h"


// Forward declarations.
class qtractorMidiClip;
class qtractorMidiEditSelect;
class qtractorMidiEditCommand;
class qtractorTimeScale;


//----------------------------------------------------------------------------
// qtractorMidiToolsForm -- UI wrapper form.

class qtractorMidiToolsForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiToolsForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorMidiToolsForm();

	// Tool page accessors.
	void setToolIndex(int iToolIndex);
	int toolIndex() const;

	// Create edit command based on given selection.
	qtractorMidiEditCommand *editCommand(
		qtractorMidiClip *pMidiClip,
		qtractorMidiEditSelect *pSelect,
		unsigned long iTimeOffset);

protected slots:

	// Preset management slots...
	void presetChanged(const QString& sPreset);
	void presetActivated(const QString& sPreset);
	void presetSave();
	void presetDelete();

	void timeshiftSpinBoxChanged(double p);
	void timeshiftSliderChanged(int i);

	void formatChanged(int);
	void changed();
	void accept();
	void reject();

	void stabilizeForm();

protected:

	// Preset management methods...
    void loadPreset(const QString& sPreset);
    void savePreset(const QString& sPreset);

	void refreshPresets();

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiToolsForm m_ui;

	// Instance variables...
	qtractorTimeScale *m_pTimeScale;

	int m_iDirtyCount;
	int m_iUpdate;

	class TimeshiftCurve *m_pTimeshiftCurve;
};


#endif	// __qtractorMidiToolsForm_h


// end of qtractorMidiToolsForm.h
