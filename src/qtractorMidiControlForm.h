// qtractorMidiControlForm.h
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiControlForm_h
#define __qtractorMidiControlForm_h

#include "ui_qtractorMidiControlForm.h"

#include "qtractorMidiControl.h"


//----------------------------------------------------------------------------
// qtractorMidiControlForm -- UI wrapper form.

class qtractorMidiControlForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiControlForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorMidiControlForm();

protected slots:

    void reject();

    void importSlot();
    void removeSlot();
    void moveUpSlot();
    void moveDownSlot();
	void mapSlot();
	void unmapSlot();
    void reloadSlot();
    void exportSlot();

	void keyChangedSlot();
	void valueChangedSlot();

    void stabilizeForm();

protected:

    void stabilizeKeyChange();
    void stabilizeValueChange();

    void refreshFiles();
    void refreshControlMap();

	qtractorMidiControl::ControlType typeFromText(const QString& sText) const;
	QString textFromType(qtractorMidiControl::ControlType ctype) const;

	unsigned short channelFromText(const QString& sText) const;
	QString textFromChannel(unsigned short iChannel) const;

	unsigned short controllerFromText(const QString& sText) const;
	QString textFromController(unsigned short iController) const;

	qtractorMidiControl::Command commandFromText(const QString& sText) const;
	QString textFromCommand(qtractorMidiControl::Command command) const;

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiControlForm m_ui;

	// Instance variables...
	int m_iDirtyCount;
	int m_iDirtyMap;
	int m_iUpdating;
};


#endif	// __qtractorMidiControlForm_h


// end of qtractorMidiControlForm.h
