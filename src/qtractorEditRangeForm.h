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

    // Range option flags.
    enum Option {
		None       = 0,
		Clips      = 1,
		Automation = 2,
		Loop       = 4,
		Punch      = 8,
		Markers    = 16,
		TempoMap   = 32
    };

    // Retrieve range option flags.
    unsigned int rangeOptions() const;

protected:

    // Option flags accessors.
    void setOption(Option option, bool bOn);
    bool isOption(Option option) const;

    // Update options settings.
    void updateOptions();

protected slots:

    void optionsChanged();
    void rangeChanged();
	void formatChanged(int);
	void valueChanged();
	void stabilizeForm();

    void accept();

private:

	// The Qt-designer UI struct...
	Ui::qtractorEditRangeForm m_ui;

	// Instance variables...
	qtractorTimeScale *m_pTimeScale;

	// Initial static selection range.
	unsigned long m_iSelectStart;
	unsigned long m_iSelectEnd;

    // Applicable options;
    unsigned int m_options;

    // Pseudo-mutex.
    unsigned int m_iUpdate;
};


#endif	// __qtractorEditRangeForm_h


// end of qtractorEditRangeForm.h
