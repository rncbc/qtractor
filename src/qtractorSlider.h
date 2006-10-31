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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __qtractorSlider_h
#define __qtractorSlider_h

#include <QSlider>

#include <QMouseEvent>
#include <QWheelEvent>


//----------------------------------------------------------------------
// class qtractorSlider -- A derived (vertical) QSlider.
//

class qtractorSlider : public QSlider
{
	Q_OBJECT

public:

	// Constructor.
	qtractorSlider(Qt::Orientation orientation, QWidget *pParent = 0)
		: QSlider(orientation, pParent), m_iDefault(0) {}

	// Get default (mid) value.
	int getDefault() const
		{ return m_iDefault; }

public slots:

	// Set default (mid) value.
	void setDefault(int iDefault)
		{ m_iDefault = iDefault; }

protected:

	// Alternate mouse behavior event handlers.
	void mousePressEvent(QMouseEvent *pMouseEvent)
	{
		// Reset to default value...
		if (pMouseEvent->button() == Qt::MidButton)
			setValue(m_iDefault);
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
		if (iValue > maximum())
			iValue = maximum();
		else
		if (iValue < minimum())
			iValue = minimum();
		setValue(iValue);
	}

private:

	// Default (mid) value.
	int m_iDefault;
};


#endif  // __qtractorSlider_h

// end of qtractorSlider.h

