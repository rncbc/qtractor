// qtractorTakeRangeForm.h
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

#ifndef __qtractorTakeRangeForm_h
#define __qtractorTakeRangeForm_h

#include "ui_qtractorTakeRangeForm.h"


// Forward declarations.
class qtractorTimeScale;
class qtractorClip;


//----------------------------------------------------------------------------
// qtractorTakeRangeForm -- UI wrapper form.

class qtractorTakeRangeForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTakeRangeForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorTakeRangeForm();

	// Setup accessors.
	void setClip(qtractorClip *pClip);
	qtractorClip *clip() const;

	// Result accessors.
	unsigned long takeStart() const;
	unsigned long takeEnd() const;

	int currentTake() const;

protected slots:

	void rangeChanged();
	void formatChanged(int);
	void valueChanged();
	void updateCurrentTake();
	void stabilizeForm();

private:

	// The Qt-designer UI struct...
	Ui::qtractorTakeRangeForm m_ui;

	// Instance variables...
	qtractorTimeScale *m_pTimeScale;
	qtractorClip *m_pClip;
};


#endif	// __qtractorTakeRangeForm_h


// end of qtractorTakeRangeForm.h
