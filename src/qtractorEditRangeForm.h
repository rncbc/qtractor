// qtractorEditRangeForm.h
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

#ifndef __qtractorEditRangeForm_h
#define __qtractorEditRangeForm_h

#include "ui_qtractorEditRangeForm.h"


// Forward declarations.
class qtractorTimeScale;


//----------------------------------------------------------------------------
// qtractorEditRangeForm -- UI wrapper form.

class qtractorEditRangeForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorEditRangeForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorEditRangeForm();

	// Set the current initial selection range.
	void setSelectionRange(unsigned long iSelectStart, unsigned long iSelectEnd);

	// Retrieve the current range, if the case arises.
	unsigned long rangeStart() const;
	unsigned long rangeEnd() const;

protected slots:

	void rangeChanged();
	void formatChanged();
	void valueChanged();
	void stabilizeForm();

private:

	// The Qt-designer UI struct...
	Ui::qtractorEditRangeForm m_ui;

	// Instance variables...
	qtractorTimeScale *m_pTimeScale;

	// Initial static selection range.
	unsigned long m_iSelectStart;
	unsigned long m_iSelectEnd;
};


#endif	// __qtractorEditRangeForm_h


// end of qtractorEditRangeForm.h
