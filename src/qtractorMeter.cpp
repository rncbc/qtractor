// qtractorMeter.cpp
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

#include "qtractorAbout.h"
#include "qtractorMeter.h"

#include "qtractorMonitor.h"

#include "qtractorMidiControlObserver.h"
#include "qtractorMidiControlObserverForm.h"

#include "qtractorObserverWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QLabel>

#include <QPaintEvent>
#include <QResizeEvent>

#include <math.h>


//----------------------------------------------------------------------------
// qtractorMeterScale -- Meter bridge scale widget.

// Constructor.
qtractorMeterScale::qtractorMeterScale ( qtractorMeter *pMeter,
	QWidget *pParent ) : QFrame(pParent)
{
	m_pMeter = pMeter;
	m_iLastY = 0;

	QFrame::setFrameShape(QFrame::Panel);
	QFrame::setFrameShadow(QFrame::Sunken);

	QFrame::setMinimumWidth(16);
//	QFrame::setBackgroundRole(QPalette::Mid);

	const QFont& font = QFrame::font();
	QFrame::setFont(QFont(font.family(), font.pointSize() - 2));
}


// Meter accessor.
qtractorMeter *qtractorMeterScale::meter (void) const
{
	return m_pMeter;
}


// Draw scale line and label; assumes labels drawed from top to bottom.
void qtractorMeterScale::drawLineLabel ( QPainter *p,
	int y, const QString& sLabel )
{
	int iCurrY = QWidget::height() - y;
	int iWidth = QWidget::width();

	const QFontMetrics& fm = p->fontMetrics();
	int iMidHeight = (fm.height() >> 1);

	if (iCurrY < iMidHeight || iCurrY > (m_iLastY + iMidHeight)) {
		if (fm.width(sLabel) < iWidth - 5)
			p->drawLine(iWidth - 3, iCurrY, iWidth - 1, iCurrY);
		p->drawText(0, iCurrY - iMidHeight, iWidth - 3, fm.height(),
			Qt::AlignHCenter | Qt::AlignVCenter, sLabel);
		m_iLastY = iCurrY + 1;
	}
}


// Paint event handler.
void qtractorMeterScale::paintEvent ( QPaintEvent * )
{
	QPainter painter(this);

	m_iLastY = 0;

#ifndef CONFIG_GRADIENT
	painter.setPen(Qt::darkGray);
#endif

	paintScale(&painter);
}


//----------------------------------------------------------------------
// class qtractorMeter::PanSliderInterface -- Observer interface.
//

// Local converter interface.
class qtractorMeter::PanSliderInterface
	: public qtractorObserverSlider::Interface
{
public:

	// Constructor.
	PanSliderInterface (
		qtractorObserverSlider *pSlider )
		: qtractorObserverSlider::Interface(pSlider) {}

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 100.0f * fValue; }

	float valueFromScale ( float fScale ) const
		{ return 0.01f * fScale; }
};


//----------------------------------------------------------------------------
// qtractorMeter::PanObserver -- Local dedicated observer.

class qtractorMeter::PanObserver : public qtractorMidiControlObserver
{
public:

	// Constructor.
	PanObserver(qtractorMeter *pMeter)
		: qtractorMidiControlObserver(NULL), m_pMeter(pMeter) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
	{
		m_pMeter->monitor()->update();
		if (bUpdate)
			m_pMeter->updatePanning();
		qtractorMidiControlObserver::update(bUpdate);
	}

private:

	// Members.
	qtractorMeter *m_pMeter;
};


//----------------------------------------------------------------------------
// qtractorMeter::GainObserver -- Local dedicated observer.

class qtractorMeter::GainObserver : public qtractorMidiControlObserver
{
public:

	// Constructor.
	GainObserver(qtractorMeter *pMeter)
		: qtractorMidiControlObserver(NULL), m_pMeter(pMeter) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
	{
		m_pMeter->monitor()->update();
		if (bUpdate)
			m_pMeter->updateGain();
		qtractorMidiControlObserver::update(bUpdate);
	}

private:

	// Members.
	qtractorMeter *m_pMeter;
};


//----------------------------------------------------------------------------
// qtractorMeter -- Meter bridge slot widget.

// Constructor.
qtractorMeter::qtractorMeter ( QWidget *pParent )
	: QWidget(pParent)
{
	const QFont& font = QWidget::font();
	QFont font7(font.family(), font.pointSize() - 1);
	QFontMetrics fm(font7);

	m_pVBoxLayout = new QVBoxLayout();
	m_pVBoxLayout->setMargin(0);
	m_pVBoxLayout->setSpacing(2);
	QWidget::setLayout(m_pVBoxLayout);

	m_pPanSlider = new qtractorObserverSlider(/*this*/);
	m_pPanSlider->setOrientation(Qt::Horizontal);
	m_pPanSlider->setFixedHeight(20);
	m_pVBoxLayout->addWidget(m_pPanSlider);

	m_pPanSpinBox = new qtractorObserverSpinBox(/*this*/);
	m_pPanSpinBox->setFont(font7);
	m_pPanSpinBox->setFixedHeight(fm.lineSpacing() + 2);
	m_pPanSpinBox->setKeyboardTracking(false);
	m_pVBoxLayout->addWidget(m_pPanSpinBox);

	m_pTopWidget = new QWidget(/*this*/);
	m_pTopLayout = new QHBoxLayout();
	m_pTopLayout->setMargin(0);
	m_pTopLayout->setSpacing(2);
	m_pTopWidget->setLayout(m_pTopLayout);
	m_pVBoxLayout->addWidget(m_pTopWidget);

	m_pBoxWidget = new QWidget(/*this*/);
	m_pBoxLayout = new QHBoxLayout();
	m_pBoxLayout->setMargin(2);
	m_pBoxLayout->setSpacing(2);
	m_pBoxWidget->setLayout(m_pBoxLayout);
	m_pVBoxLayout->addWidget(m_pBoxWidget);

	m_pGainSlider = new qtractorObserverSlider(/*, m_pBoxWidget*/);
	m_pGainSlider->setOrientation(Qt::Vertical);
	m_pGainSlider->setFixedWidth(20);
	m_pBoxLayout->addWidget(m_pGainSlider);

	m_pGainSpinBox = new qtractorObserverSpinBox(/*this*/);
	m_pGainSpinBox->setFont(font7);
	m_pGainSpinBox->setFixedHeight(fm.lineSpacing() + 2);
	m_pGainSpinBox->setKeyboardTracking(false);
	m_pVBoxLayout->addWidget(m_pGainSpinBox);

	m_pPanObserver  = new PanObserver(this);
	m_pGainObserver = new GainObserver(this);
	
	m_iPeakFalloff = 0;

	m_pPanSlider->setInterface(new PanSliderInterface(m_pPanSlider));

	m_pPanSlider->setTickPosition(QSlider::NoTicks);
	m_pPanSlider->setMinimum(-100);
	m_pPanSlider->setMaximum(+100);
	m_pPanSlider->setPageStep(10);
	m_pPanSlider->setSingleStep(1);

	m_pPanSpinBox->setDecimals(1);
	m_pPanSpinBox->setMinimum(-1.0f);
	m_pPanSpinBox->setMaximum(+1.0f);
	m_pPanSpinBox->setSingleStep(0.1f);
	m_pPanSpinBox->setAlignment(Qt::AlignHCenter);
	m_pPanSpinBox->setAccelerated(true);
	m_pPanSpinBox->setToolTip(tr("Pan"));

	m_pGainSlider->setTickPosition(QSlider::NoTicks);
	m_pGainSlider->setMinimum(0);
	m_pGainSlider->setMaximum(10000);
	m_pGainSlider->setPageStep(1000);
	m_pGainSlider->setSingleStep(100);

	m_pGainSpinBox->setDecimals(1);
//	m_pGainSpinBox->setSingleStep(0.1f);
	m_pGainSpinBox->setAlignment(Qt::AlignHCenter);
	m_pGainSpinBox->setAccelerated(true);
	m_pGainSpinBox->setToolTip(tr("Gain"));

	QWidget::setMinimumHeight(140);
	QWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));
}


// Default destructor.
qtractorMeter::~qtractorMeter (void)
{
	delete m_pGainObserver;
	delete m_pPanObserver;

	// No need to delete child widgets, Qt does it all for us
}


// Dynamic layout accessors.
QWidget *qtractorMeter::topWidget (void) const
{
	return m_pTopWidget;
}

QHBoxLayout *qtractorMeter::topLayout (void) const
{
	return m_pTopLayout;
}


QWidget *qtractorMeter::boxWidget (void) const
{
	return m_pBoxWidget;
}

QHBoxLayout *qtractorMeter::boxLayout (void) const
{
	return m_pBoxLayout;
}


// Panning subject accessors.
void qtractorMeter::setPanningSubject ( qtractorSubject *pSubject )
{
	m_pPanSlider->setSubject(pSubject);
	m_pPanSpinBox->setSubject(pSubject);

	m_pPanObserver->setSubject(pSubject);

	m_pPanSpinBox->observer()->update(true);
	m_pPanSlider->observer()->update(true);
}

qtractorSubject *qtractorMeter::panningSubject (void) const
{
	return m_pPanObserver->subject();
}

qtractorMidiControlObserver *qtractorMeter::panningObserver (void) const
{
	return static_cast<qtractorMidiControlObserver *> (m_pPanObserver);
}


// Stereo panning accessors.
void qtractorMeter::setPanning ( float fPanning )
{
	m_pPanObserver->setValue(fPanning);
	monitor()->update();
	updatePanning();
}

float qtractorMeter::panning (void) const
{
	return m_pPanObserver->value();
}

float qtractorMeter::prevPanning (void) const
{
	return m_pPanObserver->prevValue();
}


// Gain subject accessors.
void qtractorMeter::setGainSubject ( qtractorSubject *pSubject )
{
	m_pGainSlider->setSubject(pSubject);
	m_pGainSpinBox->setSubject(pSubject);

	m_pGainObserver->setSubject(pSubject);

	m_pGainSpinBox->observer()->update(true);
	m_pGainSlider->observer()->update(true);
}

qtractorSubject *qtractorMeter::gainSubject (void) const
{
	return m_pGainObserver->subject();
}

qtractorMidiControlObserver *qtractorMeter::gainObserver (void) const
{
	return static_cast<qtractorMidiControlObserver *> (m_pGainObserver);
}


// Gain accessors.
void qtractorMeter::setGain ( float fGain )
{
	m_pGainObserver->setValue(fGain);
	monitor()->update();
	updateGain();
}

float qtractorMeter::gain (void) const
{
	return m_pGainObserver->value();
}

float qtractorMeter::prevGain (void) const
{
	return m_pGainObserver->prevValue();
}


// Common slider/spin-box accessors.
qtractorObserverSlider *qtractorMeter::panSlider (void) const
{
	return m_pPanSlider;
}

qtractorObserverSpinBox *qtractorMeter::panSpinBox (void) const
{
	return m_pPanSpinBox;
}

qtractorObserverSlider *qtractorMeter::gainSlider (void) const
{
	return m_pGainSlider;
}

qtractorObserverSpinBox *qtractorMeter::gainSpinBox (void) const
{
	return m_pGainSpinBox;
}


// Peak falloff mode setting.
void qtractorMeter::setPeakFalloff ( int iPeakFalloff )
{
	m_iPeakFalloff = iPeakFalloff;
}

int qtractorMeter::peakFalloff (void) const
{
	return m_iPeakFalloff;
}


// MIDI controller/observer attachment (context menu) activator.
//

void qtractorMeter::addMidiControlAction (
	QWidget *pWidget, qtractorMidiControlObserver *pMidiObserver )
{
	qtractorMidiControlObserverForm::addMidiControlAction(
		this, pWidget, pMidiObserver);
}


void qtractorMeter::midiControlActionSlot (void)
{
	qtractorMidiControlObserverForm::midiControlAction(
		this, qobject_cast<QAction *> (sender()));
}


void qtractorMeter::midiControlMenuSlot ( const QPoint& pos )
{
	qtractorMidiControlObserverForm::midiControlMenu(
		qobject_cast<QWidget *> (sender()), pos);
}


// end of qtractorMeter.cpp
