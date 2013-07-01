// qtractorPasteRepeatForm.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorPasteRepeatForm_h
#define __qtractorPasteRepeatForm_h

#include "ui_qtractorPasteRepeatForm.h"


// Forward declarations.
class qtractorTimeScale;


//----------------------------------------------------------------------------
// qtractorPasteRepeatForm -- UI wrapper form.

class qtractorPasteRepeatForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPasteRepeatForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorPasteRepeatForm();

	// Accepted dialog accessors.
	void setRepeatCount(unsigned short iRepeatCount);
	unsigned short repeatCount()  const;

	void setRepeatPeriod(unsigned long iRepeatPeriod);
	unsigned long repeatPeriod() const;

protected slots:

	void accept();
	void reject();
	void changed();
	void formatChanged(int);
	void stabilizeForm();

private:

	// The Qt-designer UI struct...
	Ui::qtractorPasteRepeatForm m_ui;

	// Instance variables...
	qtractorTimeScale *m_pTimeScale;

	int m_iDirtyCount;
};


#endif	// __qtractorPasteRepeatForm_h


// end of qtractorPasteRepeatForm.h
