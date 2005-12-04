// qtractorSpinBox.h
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorSpinBox_h
#define __qtractorSpinBox_h

#include <qspinbox.h>
#include <qvalidator.h>

#include <math.h>


//----------------------------------------------------------------------
// class qtractorSpinBox -- A rough floating-point enabled QSpinBox.
//

class qtractorSpinBox : public QSpinBox
{
	Q_OBJECT

public:

	// Constructor.
	qtractorSpinBox(QWidget *pParent = 0, const char *pszName = 0,
		int iDecs = 1) : QSpinBox(pParent, pszName), m_iDecs(iDecs)
	{	// Multiplier is intentionally stepped up for precision...
		m_iMult = int(::pow(10.0, m_iDecs + 1));
		QDoubleValidator *pValidator = new QDoubleValidator(this);
		pValidator->setDecimals(iDecs);
		setValidator(pValidator);
		setLineStep(m_iMult / 10);
	}

	// Float value accessors.
	float valueFloat() const { return (float(value()) / float(m_iMult)); }
	void setValueFloat(float fValue) { setValue(int(fValue * m_iMult)); }

protected:

	// Virtual overrides.
	QString mapValueToText (int iValue)
		{ return QString::number(float(iValue) / float(m_iMult), 'f', m_iDecs); }
	int mapTextToValue (bool *pbOk)
		{ return int(float(m_iMult) * text().toFloat(pbOk)); }

private:

	// Instance variables.
	int m_iDecs;
	int m_iMult;
};


#endif  // __qtractorSpinBox_h

// end of qtractorSpinBox.h

