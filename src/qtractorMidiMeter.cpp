// qtractorMidiMeter.cpp
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
#include "qtractorMidiMeter.h"
#include "qtractorMidiMonitor.h"

#include "qtractorObserverWidget.h"

#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QLayout>
#include <QLabel>


// The decay rates (magic goes here :).
// - value decay rate (faster)
#define QTRACTOR_MIDI_METER_DECAY_RATE1		(1.0f - 1E-5f)
// - peak decay rate (slower)
#define QTRACTOR_MIDI_METER_DECAY_RATE2		(1.0f - 1E-6f)

// Number of cycles the peak stays on hold before fall-off.
#define QTRACTOR_MIDI_METER_PEAK_FALLOFF	16

// Number of cycles the MIDI LED stays on before going off.
#define QTRACTOR_MIDI_METER_HOLD_LEDON  	4


// MIDI On/Off LED pixmap resource.
int      qtractorMidiMeter::g_iLedRefCount = 0;
QPixmap *qtractorMidiMeter::g_pLedPixmap[qtractorMidiMeter::LedCount];

// MIDI meter color arrays.
QColor qtractorMidiMeter::g_defaultColors[qtractorMidiMeter::ColorCount] = {
	QColor(160,220, 20),	// ColorPeak
	QColor(160,160, 40),	// ColorOver
	QColor( 20, 40, 20),	// ColorBack
	QColor( 80, 80, 80) 	// ColorFore
};

QColor qtractorMidiMeter::g_currentColors[qtractorMidiMeter::ColorCount] = {
	g_defaultColors[ColorPeak],
	g_defaultColors[ColorOver],
	g_defaultColors[ColorBack],
	g_defaultColors[ColorFore]
};

//----------------------------------------------------------------------------
// qtractorMidiMeterScale -- Meter bridge scale widget.

// Constructor.
qtractorMidiMeterScale::qtractorMidiMeterScale (
	qtractorMidiMeter *pMidiMeter, QWidget *pParent )
	: qtractorMeterScale(pMidiMeter, pParent)
{
	pMidiMeter->boxLayout()->addWidget(this);
}


// Actual scale drawing method.
void qtractorMidiMeterScale::paintScale ( QPainter *pPainter )
{
	qtractorMidiMeter *pMidiMeter
		= static_cast<qtractorMidiMeter *> (meter());
	if (pMidiMeter == NULL)
		return;

	int h = QWidget::height() - 4;
	int d = (h / 5);
	int n = 100;
	while (h > 0) {
		drawLineLabel(pPainter, h, QString::number(n));
		h -= d; n -= 20;
	}
}


//----------------------------------------------------------------------------
// qtractorMidiMeterValue -- Meter bridge value widget.

// Constructor.
qtractorMidiMeterValue::qtractorMidiMeterValue (
	qtractorMidiMeter *pMidiMeter, QWidget *pParent )
	: QWidget(pParent), m_pMidiMeter(pMidiMeter)
{
	// Avoid intensively annoying repaints...
	QWidget::setAttribute(Qt::WA_StaticContents);
	QWidget::setAttribute(Qt::WA_OpaquePaintEvent);

	m_iValue      = 0;
	m_fValueDecay = QTRACTOR_MIDI_METER_DECAY_RATE1;

	m_iPeak       = 0;
	m_iPeakHold   = 0;
	m_fPeakDecay  = QTRACTOR_MIDI_METER_DECAY_RATE2;

	QWidget::setFixedWidth(14);
	QWidget::setBackgroundRole(QPalette::NoRole);

	pMidiMeter->boxLayout()->addWidget(this);
}

// Default destructor.
qtractorMidiMeterValue::~qtractorMidiMeterValue (void)
{
}


// Reset peak holder.
void qtractorMidiMeterValue::peakReset (void)
{
	m_iPeak = 0;
}


// Value refreshment.
void qtractorMidiMeterValue::refresh (void)
{
	qtractorMidiMonitor *pMidiMonitor = m_pMidiMeter->midiMonitor();
	if (pMidiMonitor == NULL)
		return;

	float fValue = pMidiMonitor->value();
	if (fValue < 0.001f && m_iPeak < 1)
		return;

	int iValue = int(fValue * float(QWidget::height()));
	if (iValue < m_iValue) {
		iValue = int(m_fValueDecay * float(m_iValue));
		m_fValueDecay *= m_fValueDecay;
	} else {
		m_fValueDecay = QTRACTOR_MIDI_METER_DECAY_RATE1;
	}

	int iPeak = m_iPeak;
	if (iPeak < iValue) {
		iPeak = iValue;
		m_iPeakHold = 0;
		m_fPeakDecay = QTRACTOR_MIDI_METER_DECAY_RATE2;
	} else if (++m_iPeakHold > m_pMidiMeter->peakFalloff()) {
		iPeak = int(m_fPeakDecay * float(iPeak));
		if (iPeak < iValue) {
			iPeak = iValue;
		} else {
			m_fPeakDecay *= m_fPeakDecay;
		}
	}

	if (iValue == m_iValue && iPeak == m_iPeak)
		return;

	m_iValue = iValue;
	m_iPeak  = iPeak;

	update();
}


// Paint event handler.
void qtractorMidiMeterValue::paintEvent ( QPaintEvent * )
{
	QPainter painter(this);

	int w = QWidget::width();
	int h = QWidget::height();

	if (isEnabled()) {
		painter.fillRect(0, 0, w, h,
			m_pMidiMeter->color(qtractorMidiMeter::ColorBack));
	} else {
		painter.fillRect(0, 0, w, h, Qt::gray);
	}

#ifdef CONFIG_GRADIENT
	painter.drawPixmap(0, h - m_iValue,
		m_pMidiMeter->pixmap(), 0, h - m_iValue, w, m_iValue);
#else
	painter.fillRect(0, h - m_iValue, w, m_iValue,
		m_pMidiMeter->color(qtractorMidiMeter::ColorOver));
#endif

	painter.setPen(m_pMidiMeter->color(qtractorMidiMeter::ColorPeak));
	painter.drawLine(0, h - m_iPeak, w, h - m_iPeak);
}


// Resize event handler.
void qtractorMidiMeterValue::resizeEvent ( QResizeEvent *pResizeEvent )
{
	m_iPeak = 0;

	QWidget::resizeEvent(pResizeEvent);
//	QWidget::repaint();
}


//----------------------------------------------------------------------
// class qtractorMidiMeter::GainSpinBoxInterface -- Observer interface.
//

// Local converter interface.
class qtractorMidiMeter::GainSpinBoxInterface
	: public qtractorObserverSpinBox::Interface
{
public:

	// Constructor.
	GainSpinBoxInterface ( qtractorObserverSpinBox *pSpinBox )
		: qtractorObserverSpinBox::Interface(pSpinBox) {}

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 100.0f * fValue; }

	float valueFromScale ( float fScale ) const
		{ return 0.01f * fScale; }
};


//----------------------------------------------------------------------
// class qtractorMidiMeter::GainSliderInterface -- Observer interface.
//

// Local converter interface.
class qtractorMidiMeter::GainSliderInterface
	: public qtractorObserverSlider::Interface
{
public:

	// Constructor.
	GainSliderInterface ( qtractorObserverSlider *pSlider )
		: qtractorObserverSlider::Interface(pSlider) {}

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 10000.0f * fValue; }

	float valueFromScale ( float fScale ) const
		{ return 0.0001f * fScale; }
};


//----------------------------------------------------------------------------
// qtractorMidiMeter -- Audio meter bridge slot widget.

// Constructor.
qtractorMidiMeter::qtractorMidiMeter ( qtractorMidiMonitor *pMidiMonitor,
	QWidget *pParent ) : qtractorMeter(pParent)
{
	if (++g_iLedRefCount == 1) {
		g_pLedPixmap[LedOff] = new QPixmap(":/images/trackMidiOff.png");
		g_pLedPixmap[LedOn]  = new QPixmap(":/images/trackMidiOn.png");
	}

	m_pMidiMonitor = pMidiMonitor;

	m_iMidiCount = 0;

#ifdef CONFIG_GRADIENT
	m_pPixmap = new QPixmap();
#endif

	topLayout()->addStretch();
	m_pMidiLabel = new QLabel(/*topWidget()*/);
	m_pMidiLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_pMidiLabel->setPixmap(*g_pLedPixmap[LedOff]);
	topLayout()->addWidget(m_pMidiLabel);

	gainSlider()->setInterface(new GainSliderInterface(gainSlider()));
	gainSpinBox()->setInterface(new GainSpinBoxInterface(gainSpinBox()));

	gainSpinBox()->setMinimum(0.0f);
	gainSpinBox()->setMaximum(100.0f);
	gainSpinBox()->setToolTip(tr("Volume (%)"));
	gainSpinBox()->setSuffix(tr(" %"));

	m_pMidiScale = new qtractorMidiMeterScale(this/*, boxWidget()*/);
	m_pMidiValue = new qtractorMidiMeterValue(this/*, boxWidget()*/);

	setPeakFalloff(QTRACTOR_MIDI_METER_PEAK_FALLOFF);

	reset();

	updatePanning();
	updateGain();
}


// Default destructor.
qtractorMidiMeter::~qtractorMidiMeter (void)
{
#ifdef CONFIG_GRADIENT
	delete m_pPixmap;
#endif

	// No need to delete child widgets, Qt does it all for us
	delete m_pMidiValue;
	delete m_pMidiScale;

	delete m_pMidiLabel;

	if (--g_iLedRefCount == 0) {
		delete g_pLedPixmap[LedOff];
		delete g_pLedPixmap[LedOn];
	}
}


// MIDI monitor reset
void qtractorMidiMeter::reset (void)
{
	if (m_pMidiMonitor == NULL)
		return;

	setPanningSubject(m_pMidiMonitor->panningSubject());
	setGainSubject(m_pMidiMonitor->gainSubject());
}


// Reset peak holder.
void qtractorMidiMeter::peakReset (void)
{
	m_pMidiValue->peakReset();
	m_iMidiCount = 0;
}


#ifdef CONFIG_GRADIENT
// Gradient pixmap accessor.
const QPixmap& qtractorMidiMeter::pixmap (void) const
{
	return *m_pPixmap;
}

void qtractorMidiMeter::updatePixmap (void)
{
	int w = boxWidget()->width();
	int h = boxWidget()->height();

	QLinearGradient grad(0, 0, 0, h);
	grad.setColorAt(0.0f, color(ColorPeak));
	grad.setColorAt(0.4f, color(ColorOver));

	*m_pPixmap = QPixmap(w, h);

	QPainter(m_pPixmap).fillRect(0, 0, w, h, grad);
}
#endif


// Slot refreshment.
void qtractorMidiMeter::refresh (void)
{
	m_pMidiValue->refresh();
	
	// Take care of the MIDI LED status...
	bool bMidiOn = (m_pMidiMonitor->count() > 0);
	if (bMidiOn) {
		if (m_iMidiCount == 0)
			m_pMidiLabel->setPixmap(*g_pLedPixmap[LedOn]);
		m_iMidiCount = QTRACTOR_MIDI_METER_HOLD_LEDON;
	} else if (m_iMidiCount > 0) {
		if (--m_iMidiCount == 0)
			m_pMidiLabel->setPixmap(*g_pLedPixmap[LedOff]);
	}
}


// Resize event handler.
void qtractorMidiMeter::resizeEvent ( QResizeEvent * )
{
	// HACK: make so that the MIDI gain slider (volume)
	// aligns its top at the Audio 0 dB gain level...
	int iFixedHeight = int(0.15f * float(boxWidget()->height())) - 4;
	if (iFixedHeight < 16)
		iFixedHeight = 16;
	topWidget()->setFixedHeight(iFixedHeight);

#ifdef CONFIG_GRADIENT
	updatePixmap();
#endif
}


// Virtual monitor accessor.
void qtractorMidiMeter::setMonitor ( qtractorMonitor *pMonitor )
{
	setMidiMonitor(static_cast<qtractorMidiMonitor *> (pMonitor));
}

qtractorMonitor *qtractorMidiMeter::monitor (void) const
{
	return midiMonitor();
}

// MIDI monitor accessor.
void qtractorMidiMeter::setMidiMonitor ( qtractorMidiMonitor *pMidiMonitor )
{
	m_pMidiMonitor = pMidiMonitor;

	reset();
}

qtractorMidiMonitor *qtractorMidiMeter::midiMonitor (void) const
{
	return m_pMidiMonitor;
}


// Common resource accessor.
void qtractorMidiMeter::setColor ( int iIndex, const QColor& color )
{
	g_currentColors[iIndex] = color;
}

const QColor& qtractorMidiMeter::color ( int iIndex )
{
	return g_currentColors[iIndex];
}

const QColor& qtractorMidiMeter::defaultColor ( int iIndex )
{
	return g_defaultColors[iIndex];
}


// Pan-slider value change method.
void qtractorMidiMeter::updatePanning (void)
{
//	setPanning(m_pMidiMonitor->panning());

	panSlider()->setToolTip(
		tr("Pan: %1").arg(panning(), 0, 'g', 2));
}


// Gain-slider value change method.
void qtractorMidiMeter::updateGain (void)
{
//	setGain(m_pMidiMonitor->gain());

	gainSlider()->setToolTip(
		tr("Volume: %1%").arg(gainSpinBox()->value(), 0, 'g', 3));
}


// end of qtractorMidiMeter.cpp
