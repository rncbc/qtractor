// qtractorObserverWidget.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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


template <class Widget>
class qtractorObserverWidget : public Widget
{
public:

	// Local interface converter.
	class Interface
	{
	public:
		
		// Constructor.
		Interface(qtractorObserverWidget<Widget> *pWidget)
			: m_pWidget(pWidget) {}

		qtractorObserverWidget<Widget> *widget() const
			{ return m_pWidget; }

		// Virtual destructor.
		virtual ~Interface() {}

		// Pure virtuals.
		virtual float scaleFromValue(float fValue) const = 0;
		virtual float valueFromScale(float fScale) const = 0;
	
	private:

		// Members.
		qtractorObserverWidget<Widget> *m_pWidget;
	};
	
	// Local observer.
	class Observer : public qtractorObserver
	{
	public:

		// Constructor.
		Observer(qtractorSubject *pSubject,
			qtractorObserverWidget<Widget> *pWidget)
			: qtractorObserver(pSubject), m_pWidget(pWidget) {}

		// Observer updater.
		void update(bool bUpdate)
			{ if (bUpdate) m_pWidget->updateValue(value()); }

	private:

		// Members.
		qtractorObserverWidget<Widget> *m_pWidget;
	};

	// Constructor.
	qtractorObserverWidget(QWidget *pParent = 0)
		: Widget(pParent), m_pInterface(NULL), m_observer(NULL, this) {}

	// Destructor.
	~qtractorObserverWidget()
	{
		if (m_pInterface)
			delete m_pInterface;
	}

	// Setup.
	void setSubject(qtractorSubject *pSubject)
		{ m_observer.setSubject(pSubject); }
	qtractorSubject *subject() const
		{ return m_observer.subject(); }

	// Observer accessor.
	Observer *observer() { return &m_observer; }

	// Interface setup.
	void setInterface(Interface *pInterface)
	{
		if (m_pInterface)
			delete m_pInterface;
		m_pInterface = pInterface;
	}

	// Interface methods.
	float scaleFromValue(float fValue) const
		{ return (m_pInterface ? m_pInterface->scaleFromValue(fValue) : fValue); }
	float valueFromScale(float fScale) const
		{ return (m_pInterface ? m_pInterface->valueFromScale(fScale) : fScale); }

protected:

	// Pure virtual visitor.
	virtual void updateValue(float fValue) = 0;

private:

	// Members.
	Interface *m_pInterface;
	Observer   m_observer;
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
	
protected:

	// Visitors overload.
	void updateValue(float fValue);

protected slots:

	void checkBoxChanged(bool bValue);

signals:

	void valueChanged(float);
};


//----------------------------------------------------------------------
// class qtractorObserverSpinBox -- Concrete widget observer.
//

class qtractorObserverSpinBox : public qtractorObserverWidget<QDoubleSpinBox>
{
	Q_OBJECT

public:

	// Constructor.
	qtractorObserverSpinBox(QWidget *pParent = 0);
	
protected:

	// Visitors overload.
	void updateValue(float fValue);

protected slots:

	void spinBoxChanged(double value);

signals:

	void valueChanged(float);
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

protected:

	// Alternate mouse behavior event handlers.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void wheelEvent(QWheelEvent *pWheelEvent);

	// Visitors overload.
	void updateValue(float fValue);

protected slots:

	void sliderChanged(int iValue);

signals:

	void valueChanged(float);
};


#endif  // __qtractorObserverWidget_h


// end of qtractorObserverWidget.h
