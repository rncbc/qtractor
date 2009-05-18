// qtractorMidiControlForm.h
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

    void accept();
    void reject();

    void importSlot();
    void removeSlot();
    void moveUpSlot();
    void moveDownSlot();
    void exportSlot();
    void applySlot();

    void refreshForm();
    void stabilizeForm();

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiControlForm m_ui;

	// Instance variables...
	int *m_iDirtyCount;
};


#endif	// __qtractorMidiControlForm_h


// end of qtractorMidiControlForm.h
