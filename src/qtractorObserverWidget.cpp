// qtractorObserverWidget.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

// Protected slot.
void qtractorObserverCheckBox::checkBoxChanged ( bool bValue )
{
	if (observer()->isBusy())
		return;

	float fValue = (bValue ? 1.0f : 0.0f);
#ifdef CONFIG_DEBUG
	qDebug("qtractorObserverCheckBox[%p]::checkBoxChanged(%g)", this, fValue);
#endif
	observer()->setValue(fValue);
}


//----------------------------------------------------------------------
// class qtractorObserverSpinBox -- Concrete widget observer.
//

// Constructor.
qtractorObserverSpinBox::qtractorObserverSpinBox ( QWidget *pParent ) 
	: qtractorObserverWidget<QSpinBox> (pParent)
{
	QObject::connect(this,
		SIGNAL(valueChanged(int)),
		SLOT(spinBoxChanged(int)));
}

// Protected slot.
void qtractorObserverSpinBox::spinBoxChanged ( int iValue )
{
	if (observer()->isBusy())
		return;

	float fValue = float(iValue); // float(maximum() - minimum());
#ifdef CONFIG_DEBUG
	qDebug("qtractorObserverSpinBox[%p]::spinBoxChanged(%g)", this, fValue);
#endif
	observer()->setValue(fValue);
}


//----------------------------------------------------------------------
// class qtractorObserverDoubleSpinBox -- Concrete widget observer.
//

// Constructor.
qtractorObserverDoubleSpinBox::qtractorObserverDoubleSpinBox ( QWidget *pParent ) 
	: qtractorObserverWidget<QDoubleSpinBox> (pParent)
{
	QObject::connect(this,
		SIGNAL(valueChanged(double)),
		SLOT(spinBoxChanged(double)));
}

// Protected slot.
void qtractorObserverDoubleSpinBox::spinBoxChanged ( double value )
{
	if (observer()->isBusy())
		return;

	float fValue = float(value); // float(maximum() - minimum());
#ifdef CONFIG_DEBUG
	qDebug("qtractorObserverDoubleSpinBox[%p]::spinBoxChanged(%g)", this, fValue);
#endif
	observer()->setValue(fValue);
}


//----------------------------------------------------------------------
// class qtractorObserverSlider -- Concrete widget observer.
//

// Constructor.
qtractorObserverSlider::qtractorObserverSlider ( QWidget *pParent ) 
	: qtractorObserverWidget<QSlider> (pParent), m_iDefault(0)
{
	QObject::connect(this,
		SIGNAL(valueChanged(int)),
		SLOT(sliderChanged(int)));
}


// Get default (mid) value.
int qtractorObserverSlider::getDefault (void) const
{
	return m_iDefault;
}


// Set default (mid) value.
void qtractorObserverSlider::setDefault ( int iDefault )
{
	m_iDefault = iDefault;
}


// Alternate mouse behavior event handlers.
void qtractorObserverSlider::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Reset to default value...
	if (pMouseEvent->button() == Qt::MidButton)
		setValue(m_iDefault);
	else
		qtractorObserverWidget<QSlider>::mousePressEvent(pMouseEvent);
}

void qtractorObserverSlider::wheelEvent ( QWheelEvent *pWheelEvent )
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


// Protected slot.
void qtractorObserverSlider::sliderChanged ( int iValue )
{
	if (observer()->isBusy())
		return;

	int iMaximum = maximum();
	if (iMaximum > m_iDefault)
		iMaximum = m_iDefault;
	float fValue = float(iValue) / float(iMaximum - minimum());
#ifdef CONFIG_DEBUG
	qDebug("qtractorObserverSlider[%p]::sliderChanged(%g)", this, fValue);
#endif
	observer()->setValue(fValue);
};


// end of qtractorObserverWidget.cpp
