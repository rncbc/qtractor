// qtractorObserverWidget.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorObserverWidget.h"

#include <QMouseEvent>
#include <QWheelEvent>


//----------------------------------------------------------------------
// class qtractorObserverCheckBox -- Concrete widget observer.
//

// Constructor.
qtractorObserverCheckBox::qtractorObserverCheckBox ( QWidget *pParent ) 
	: qtractorObserverWidget<QCheckBox> (pParent)
{
	QObject::connect(this,
		SIGNAL(toggled(bool)),
		SLOT(checkBoxChanged(bool)));
}


// Visitors overload.
void qtractorObserverCheckBox::updateValue ( float fValue )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorObserverCheckBox[%p]::updateValue(%g)", this, fValue);
#endif
	++m_iUpdateValue;
	QCheckBox::setChecked(bool(scaleFromValue(fValue)));
	--m_iUpdateValue;
}


// Protected slot.
void qtractorObserverCheckBox::checkBoxChanged ( bool bValue )
{
	if (m_iUpdateValue > 0)
		return;

	float fValue = valueFromScale(bValue ? 1.0f : 0.0f);
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorObserverCheckBox[%p]::checkBoxChanged(%g)", this, fValue);
#endif
	observer()->setValue(fValue);
}


//----------------------------------------------------------------------
// class qtractorObserverSpinBox -- Concrete widget observer.
//

// Constructor.
qtractorObserverSpinBox::qtractorObserverSpinBox ( QWidget *pParent ) 
	: qtractorObserverWidget<QDoubleSpinBox> (pParent)
{
	QObject::connect(this,
		SIGNAL(valueChanged(double)),
		SLOT(spinBoxChanged(double)));
}


// Visitors overload.
void qtractorObserverSpinBox::updateValue ( float fValue )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorObserverSpinBox[%p]::updateValue(%g)", this, fValue);
#endif
	++m_iUpdateValue;
	QDoubleSpinBox::setValue(scaleFromValue(fValue));
	--m_iUpdateValue;
}


// Protected slot.
void qtractorObserverSpinBox::spinBoxChanged ( double value )
{
	if (m_iUpdateValue > 0)
		return;

	float fValue = valueFromScale(float(value));
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorObserverSpinBox[%p]::spinBoxChanged(%g)", this, fValue);
#endif
	observer()->setValue(fValue);
}


//----------------------------------------------------------------------
// class qtractorObserverSlider -- Concrete widget observer.
//

// Constructor.
qtractorObserverSlider::qtractorObserverSlider ( QWidget *pParent ) 
	: qtractorObserverWidget<QSlider> (pParent)
{
	QObject::connect(this,
		SIGNAL(valueChanged(int)),
		SLOT(sliderChanged(int)));
}


// Alternate mouse behavior event handlers.
void qtractorObserverSlider::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Reset to default value...
	if (pMouseEvent->button() == Qt::MidButton)
		subject()->resetValue();
	else
		qtractorObserverWidget<QSlider>::mousePressEvent(pMouseEvent);
}

void qtractorObserverSlider::wheelEvent ( QWheelEvent *pWheelEvent )
{
	int iValue = value();

	if (pWheelEvent->delta() < 0)
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


// Visitors overload.
void qtractorObserverSlider::updateValue ( float fValue )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorObserverSlider[%p]::updateValue(%g)", this, fValue);
#endif
	++m_iUpdateValue;
	QSlider::setValue(int(scaleFromValue(fValue)));
	--m_iUpdateValue;
}


// Protected slot.
void qtractorObserverSlider::sliderChanged ( int iValue )
{
	if (m_iUpdateValue > 0)
		return;

	float fValue = valueFromScale(float(iValue));
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorObserverSlider[%p]::sliderChanged(%g)", this, fValue);
#endif
	observer()->setValue(fValue);
};


// end of qtractorObserverWidget.cpp
