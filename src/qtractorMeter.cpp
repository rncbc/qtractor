// qtractorMeter.cpp
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

#include "qtractorAbout.h"
#include "qtractorMeter.h"

#include "qtractorMonitor.h"

#include "qtractorMidiControlObserver.h"
#include "qtractorMidiControlObserverForm.h"

#include "qtractorObserverWidget.h"

#include <QAction>
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
qtractorMeterScale::qtractorMeterScale (
	qtractorMeter *pMeter ) : QFrame(pMeter)
{
	m_pMeter = pMeter;
	m_iLastY = 0;

	QFrame::setFrameShape(QFrame::Panel);
	QFrame::setFrameShadow(QFrame::Sunken);

//	QFrame::setMinimumWidth(16);
	QFrame::setMaximumWidth(24);
//	QFrame::setBackgroundRole(QPalette::Mid);

	const QFont& font = QFrame::font();
	QFrame::setFont(QFont(font.family(), font.pointSize() - 3));
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
	const int iCurrY = QWidget::height() - y;
	const int iWidth = QWidget::width();

	const QFontMetrics& fm = p->fontMetrics();
	const int iMidHeight = (fm.height() >> 1);

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


//----------------------------------------------------------------------------
// qtractorMeterValue -- Meter bridge value widget.

// List of meter-values (global obviously)
QList<qtractorMeterValue *> qtractorMeterValue::g_values;
unsigned long qtractorMeterValue::g_iStamp = 0;

// Constructor.
qtractorMeterValue::qtractorMeterValue (
	qtractorMeter *pMeter )	: QWidget(pMeter), m_pMeter(pMeter)
{
	g_values.append(this);
}


// Destructor (virtual).
qtractorMeterValue::~qtractorMeterValue (void)
{
	g_values.removeAll(this);
}


// Global refreshment (static).
void qtractorMeterValue::refreshAll (void)
{
	++g_iStamp;

	QListIterator<qtractorMeterValue *> iter(g_values);
	while (iter.hasNext())
		iter.next()->refresh(g_iStamp);
}


//----------------------------------------------------------------------------
// qtractorMeter -- Meter bridge slot widget.

// Constructor.
qtractorMeter::qtractorMeter ( QWidget *pParent )
	: QWidget(pParent)
{
	m_pBoxLayout = new QHBoxLayout();
	m_pBoxLayout->setMargin(0);
	m_pBoxLayout->setSpacing(2);

	QWidget::setLayout(m_pBoxLayout);

	m_fScale = 0.0f;

	m_iPeakFalloff = 0;

//	QWidget::setMinimumHeight(160);
//	QWidget::setMaximumHeight(480);
	QWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));
}


// Default destructor.
qtractorMeter::~qtractorMeter (void)
{
	// No need to delete child widgets, Qt does it all for us
}


//----------------------------------------------------------------------
// class qtractorMixerMeter::PanSliderInterface -- Observer interface.
//

// Local converter interface.
class qtractorMixerMeter::PanSliderInterface
	: public qtractorObserverSlider::Interface
{
public:

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 100.0f * fValue; }

	float valueFromScale ( float fScale ) const
		{ return 0.01f * fScale; }
};


//----------------------------------------------------------------------------
// qtractorMixerMeter::PanObserver -- Local dedicated observer.

class qtractorMixerMeter::PanObserver : public qtractorObserver
{
public:

	// Constructor.
	PanObserver(qtractorMixerMeter *pMixerMeter)
		: qtractorObserver(NULL), m_pMixerMeter(pMixerMeter) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
		{ if (bUpdate) m_pMixerMeter->updatePanning(); }

private:

	// Members.
	qtractorMixerMeter *m_pMixerMeter;
};


//----------------------------------------------------------------------------
// qtractorMixerMeter::GainObserver -- Local dedicated observer.

class qtractorMixerMeter::GainObserver : public qtractorObserver
{
public:

	// Constructor.
	GainObserver(qtractorMixerMeter *pMixerMeter)
		: qtractorObserver(NULL), m_pMixerMeter(pMixerMeter) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
		{ if (bUpdate) m_pMixerMeter->updateGain(); }

private:

	// Members.
	qtractorMixerMeter *m_pMixerMeter;
};


//----------------------------------------------------------------------------
// qtractorMixerMeter -- Mixer-strip meter bridge widget.

// Constructor.
qtractorMixerMeter::qtractorMixerMeter ( QWidget *pParent )
	: QWidget(pParent)
{
	const QFont& font = QWidget::font();
//	const QFont font2(font.family(), font.pointSize() - 2);
	const int iFixedHeight = QFontMetrics(font).lineSpacing() + 4;

//	QWidget::setFont(font2);

	QVBoxLayout *pVBoxLayout = new QVBoxLayout();
	pVBoxLayout->setMargin(0);
	pVBoxLayout->setSpacing(2);
	QWidget::setLayout(pVBoxLayout);

	m_pPanSlider = new qtractorObserverSlider(/*this*/);
	m_pPanSlider->setOrientation(Qt::Horizontal);
	m_pPanSlider->setFixedHeight(20);
	pVBoxLayout->addWidget(m_pPanSlider);

	m_pPanSpinBox = new qtractorObserverSpinBox(/*this*/);
//	m_pPanSpinBox->setFont(font2);
	m_pPanSpinBox->setFixedHeight(iFixedHeight);
	m_pPanSpinBox->setKeyboardTracking(false);
	pVBoxLayout->addWidget(m_pPanSpinBox);

	m_pTopWidget = new QWidget(/*this*/);
	m_pTopLayout = new QHBoxLayout();
	m_pTopLayout->setMargin(2);
	m_pTopLayout->setSpacing(0);
	m_pTopWidget->setLayout(m_pTopLayout);
	pVBoxLayout->addWidget(m_pTopWidget);

	m_pBoxLayout = new QHBoxLayout();
	m_pBoxLayout->setMargin(2);
	m_pBoxLayout->setSpacing(2);
	pVBoxLayout->addLayout(m_pBoxLayout);

	m_pGainSlider = new qtractorObserverSlider(/*, m_pBoxWidget*/);
	m_pGainSlider->setOrientation(Qt::Vertical);
	m_pGainSlider->setFixedWidth(20);
	m_pBoxLayout->addWidget(m_pGainSlider);

	m_pGainSpinBox = new qtractorObserverSpinBox(/*this*/);
//	m_pGainSpinBox->setFont(font2);
	m_pGainSpinBox->setFixedHeight(iFixedHeight);
	m_pGainSpinBox->setKeyboardTracking(false);
	pVBoxLayout->addWidget(m_pGainSpinBox);

	m_pPanObserver  = new PanObserver(this);
	m_pGainObserver = new GainObserver(this);

	m_pPanSlider->setInterface(new PanSliderInterface());

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

	QWidget::setMinimumHeight(160);
//	QWidget::setMaximumHeight(480);
	QWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));
}


// Default destructor.
qtractorMixerMeter::~qtractorMixerMeter (void)
{
	delete m_pGainObserver;
	delete m_pPanObserver;

	// No need to delete child widgets, Qt does it all for us
}


// Panning subject accessors.
void qtractorMixerMeter::setPanningSubject ( qtractorSubject *pSubject )
{
	m_pPanSlider->setSubject(pSubject);
	m_pPanSpinBox->setSubject(pSubject);

	m_pPanObserver->setSubject(pSubject);

	m_pPanSpinBox->observer()->update(true);
	m_pPanSlider->observer()->update(true);
}

qtractorSubject *qtractorMixerMeter::panningSubject (void) const
{
	return m_pPanObserver->subject();
}


// Stereo panning accessors.
void qtractorMixerMeter::setPanning ( float fPanning )
{
	m_pPanObserver->setValue(fPanning);
	monitor()->update();
	updatePanning();
}

float qtractorMixerMeter::panning (void) const
{
	return m_pPanObserver->value();
}

float qtractorMixerMeter::prevPanning (void) const
{
	return m_pPanObserver->prevValue();
}


// Gain subject accessors.
void qtractorMixerMeter::setGainSubject ( qtractorSubject *pSubject )
{
	m_pGainSlider->setSubject(pSubject);
	m_pGainSpinBox->setSubject(pSubject);

	m_pGainObserver->setSubject(pSubject);

	m_pGainSpinBox->observer()->update(true);
	m_pGainSlider->observer()->update(true);
}

qtractorSubject *qtractorMixerMeter::gainSubject (void) const
{
	return m_pGainObserver->subject();
}


// Gain accessors.
void qtractorMixerMeter::setGain ( float fGain )
{
	m_pGainObserver->setValue(fGain);
	monitor()->update();
	updateGain();
}

float qtractorMixerMeter::gain (void) const
{
	return m_pGainObserver->value();
}

float qtractorMixerMeter::prevGain (void) const
{
	return m_pGainObserver->prevValue();
}


// MIDI controller/observer attachment (context menu) activator.
//

void qtractorMixerMeter::addMidiControlAction (
	QWidget *pWidget, qtractorMidiControlObserver *pMidiObserver )
{
	qtractorMidiControlObserverForm::addMidiControlAction(
		this, pWidget, pMidiObserver);
}


void qtractorMixerMeter::midiControlActionSlot (void)
{
	qtractorMidiControlObserverForm::midiControlAction(
		this, qobject_cast<QAction *> (sender()));
}


void qtractorMixerMeter::midiControlMenuSlot ( const QPoint& pos )
{
	qtractorMidiControlObserverForm::midiControlMenu(
		qobject_cast<QWidget *> (sender()), pos);
}


// end of qtractorMeter.cpp
