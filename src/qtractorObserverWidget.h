// qtractorObserverWidget.h
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

#ifndef __qtractorObserverWidget_h
#define __qtractorObserverWidget_h

#include "qtractorObserver.h"


//----------------------------------------------------------------------
// class qtractorObserverWidget -- Template widget observer/visitor.
//

#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>


template <class W>
class qtractorObserverWidget : public W
{
public:

	// Local observer.
	class Observer : public qtractorObserver
	{
	public:
		// Constructor.
		Observer(qtractorSubject *pSubject, W *pWidget)
			: qtractorObserver(pSubject), m_pWidget(pWidget) {}

	protected:

		// Visitors overload.
		void visit(QCheckBox *pCheckBox, float fValue)
			{ pCheckBox->setChecked(fValue > 0.0f); }
		void visit(QSpinBox *pSpinBox, float fValue)
			{ pSpinBox->setValue(int(100.0f * fValue)); }
		void visit(QDoubleSpinBox *pDoubleSpinBox, float fValue)
			{ pDoubleSpinBox->setValue(fValue); }
		void visit(QSlider *pSlider, float fValue)
			{ pSlider->setValue(int(10000.0f * fValue)); }

		// Observer updater.
		void update() { visit(m_pWidget, value()); }

	private:
		// Members.
		W *m_pWidget;
	};

	// Constructor.
	qtractorObserverWidget(QWidget *pParent = 0)
		: W(pParent), m_pObserver(NULL) {};

	// Destructor.
	~qtractorObserverWidget()
		{ if (m_pObserver) delete m_pObserver; }

	// Setup.
	void setSubject(qtractorSubject *pSubject)
	{
		if (m_pObserver) delete m_pObserver;
		m_pObserver = new Observer(pSubject, this); 
	}

	// Observer accessor.
	Observer *observer() const { return m_pObserver; }

private:

	// Members.
	Observer *m_pObserver;
};


//----------------------------------------------------------------------
// class qtractorObserverCheckBox -- Concrete widget observer.
//

class qtractorObserverCheckBox : public qtractorObserverWidget<QCheckBox>
{
	Q_OBJECT

public:

	// Constructor.
	qtractorObserverCheckBox(QWidget *pParent = 0);

protected slots:

	void checkBoxChanged(bool bValue);
};


//----------------------------------------------------------------------
// class qtractorObserverSpinBox -- Concrete widget observer.
//

class qtractorObserverSpinBox : public qtractorObserverWidget<QSpinBox>
{
	Q_OBJECT

public:

	// Constructor.
	qtractorObserverSpinBox(QWidget *pParent = 0);

protected slots:

	void spinBoxChanged(int iValue);
};


//----------------------------------------------------------------------
// class qtractorObserverDoubleSpinBox -- Concrete widget observer.
//

class qtractorObserverDoubleSpinBox : public qtractorObserverWidget<QDoubleSpinBox>
{
	Q_OBJECT

public:

	// Constructor.
	qtractorObserverDoubleSpinBox(QWidget *pParent = 0);

protected slots:

	void spinBoxChanged(double value);
};


//----------------------------------------------------------------------
// class qtractorObserverSlider -- Concrete widget observer.
//

class qtractorObserverSlider : public qtractorObserverWidget<QSlider>
{
	Q_OBJECT

public:

	// Constructor.
	qtractorObserverSlider(QWidget *pParent = 0);

	// Get default (mid) value.
	int getDefault() const;

public slots:

	// Set default (mid) value.
	void setDefault(int iDefault);

protected:

	// Alternate mouse behavior event handlers.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void wheelEvent(QWheelEvent *pWheelEvent);

protected slots:

	void sliderChanged(int iValue);

private:

	// Default (mid) value.
	int m_iDefault;	
};


#endif  // __qtractorObserverWidget_h


// end of qtractorObserverWidget.h
