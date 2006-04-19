// qtractorSlider.h
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorSlider_h
#define __qtractorSlider_h

#include <qslider.h>


//----------------------------------------------------------------------
// class qtractorSlider -- A derived (vertical) QSlider.
//

class qtractorSlider : public QSlider
{
	Q_OBJECT
	Q_PROPERTY( int defaultValue READ getDefaultValue WRITE setDefaultValue )

public:

	// Constructor.
	qtractorSlider(QWidget *pParent = 0, const char *pszName = 0)
		: QSlider(Qt::Vertical, pParent, pszName), m_iDefaultValue(0) {}

	// Get default (mid) value.
	int getDefaultValue() const
		{ return m_iDefaultValue; }

public slots:

	// Set default (mid) value.
	void setDefaultValue(int iDefaultValue)
		{ m_iDefaultValue = iDefaultValue; }

protected:

	// Alternate mouse behavior event handlers.
	void mousePressEvent(QMouseEvent *pMouseEvent)
	{
		// Reset to default value...
		if (pMouseEvent->button() == Qt::MidButton)
			setValue(m_iDefaultValue);
		else
			QSlider::mousePressEvent(pMouseEvent);
	}

	void wheelEvent(QWheelEvent *pWheelEvent)
	{
		int iValue = value();
		if (pWheelEvent->delta() > 0)
			iValue -= pageStep();
		else
			iValue += pageStep();
		if (iValue > maxValue())
			iValue = maxValue();
		else
		if (iValue < minValue())
			iValue = minValue();
		setValue(iValue);
	}

private:

	// Default (mid) value.
	int m_iDefaultValue;
};


#endif  // __qtractorSlider_h

// end of qtractorSlider.h

