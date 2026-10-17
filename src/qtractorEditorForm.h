// qtractorEditorForm.h
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

#ifndef __qtractorEditorForm_h
#define __qtractorEditorForm_h

#include <QMainWindow>

// Forward decls.
class qtractorEditor;


//----------------------------------------------------------------------------
// qtractorEditorForm -- UI wrapper form.

class qtractorEditorForm : public QMainWindow
{
	Q_OBJECT

public:

	// Constructor.
	qtractorEditorForm(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags());
	// Destructor.
	virtual ~qtractorEditorForm();

	// Cliup editor widget accessor.
	virtual qtractorEditor *editor() const = 0;

	// Update thumb-view play-head...
	virtual void updatePlayHead(unsigned long iPlayHead) = 0;

	// Update local time-scale...
	virtual void updateTimeScale() = 0;

	// Regular form stabilizer (slot?)...
	virtual void stabilizeForm() = 0;
};


#endif	// __qtractorEditorForm_h


// end of qtractorEditorForm.h

