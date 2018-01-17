// qtractorMidiControlTypeGroup.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiControlTypeGroup_h
#define __qtractorMidiControlTypeGroup_h

#include "qtractorMidiControl.h"
#include "qtractorMidiEditor.h"


// Forwrad decls.
class QComboBox;
class QLabel;


//----------------------------------------------------------------------------
// qtractorMidiControlTypeGroup - MIDI control type/param widget group.

class qtractorMidiControlTypeGroup : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiControlTypeGroup(
		qtractorMidiEditor *pMidiEditor,
		QComboBox *pControlTypeComboBox,
		QComboBox *pControlParamComboBox,
		QLabel *pControlParamTextLabel = NULL);

	// Accessors.
	void setControlType(qtractorMidiControl::ControlType ctype);
	qtractorMidiControl::ControlType controlType() const;
	qtractorMidiControl::ControlType controlTypeFromIndex(int iIndex) const;

	void setControlParam(unsigned short iParam);
	unsigned short controlParam() const;
	unsigned short controlParamFromIndex(int iIndex) const;

	// Stabilizers.
	void updateControlType(int iControlType = -1);

signals:

	void controlTypeChanged(int);
	void controlParamChanged(int);

protected slots:

	void activateControlType(int);
	void activateControlParam(int);

	void editControlParamFinished();

protected:

	// Find combo-box indexes.
	int indexFromControlType(qtractorMidiControl::ControlType ctype) const;
	int indexFromControlParam(unsigned short iParam) const;

private:

	// Instance member variables.
	qtractorMidiEditor *m_pMidiEditor;

	QComboBox *m_pControlTypeComboBox;
	QComboBox *m_pControlParamComboBox;
	QLabel    *m_pControlParamTextLabel;

	unsigned int m_iControlParamUpdate;
};


#endif  // __qtractorMidiControlTypeGroup_h

// end of qtractorMidiControlTypeGroup.h
