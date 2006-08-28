// qtractorMidiMeter.cpp
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

#include "qtractorMidiMeter.h"
#include "qtractorMidiMonitor.h"
#include "qtractorSlider.h"
#include "qtractorSpinBox.h"

#include <qtooltip.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qlabel.h>


// The decay rates (magic goes here :).
// - value decay rate (faster)
#define QTRACTOR_MIDI_METER_DECAY_RATE1		(1.0f - 1E-4f)
// - peak decay rate (slower)
#define QTRACTOR_MIDI_METER_DECAY_RATE2		(1.0f - 1E-6f)

// Number of cycles the peak stays on hold before fall-off.
#define QTRACTOR_MIDI_METER_PEAK_FALLOFF	16

// Number of cycles the MIDI LED stays on before going off.
#define QTRACTOR_MIDI_METER_HOLD_LEDON  	4


//----------------------------------------------------------------------------
// qtractorMidiMeterScale -- Meter bridge scale widget.

// Constructor.
qtractorMidiMeterScale::qtractorMidiMeterScale (
	qtractorMidiMeter *pMidiMeter, QWidget *pParent, const char *pszName )
	: qtractorMeterScale(pMidiMeter, pParent, pszName)
{
}


// Actual scale drawing method.
void qtractorMidiMeterScale::paintScale ( QPainter *p )
{
	qtractorMidiMeter *pMidiMeter
		= static_cast<qtractorMidiMeter *> (meter());
	if (pMidiMeter == NULL)
		return;

	int h = QWidget::height();
	int d = (h / 5);
	int n = 100;
	while (h > 0) { 
		drawLineLabel(p, h, QString::number(n).latin1());
		h -= d; n -= 20;
	}
}


//----------------------------------------------------------------------------
// qtractorMidiMeterValue -- Meter bridge value widget.

// Constructor.
qtractorMidiMeterValue::qtractorMidiMeterValue(
	qtractorMidiMeter *pMidiMeter, QWidget *pParent, const char *pszName )
	: QFrame(pParent, pszName)
{
	m_pMidiMeter  = pMidiMeter;
	m_iValue      = 0;
	m_fValueDecay = QTRACTOR_MIDI_METER_DECAY_RATE1;
	m_iPeak       = 0;
	m_iPeakHold   = 0;
	m_fPeakDecay  = QTRACTOR_MIDI_METER_DECAY_RATE2;

	QFrame::setFixedWidth(14);
	QFrame::setBackgroundMode(Qt::NoBackground);

	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Sunken);
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


// Paint event handler.
void qtractorMidiMeterValue::paintEvent ( QPaintEvent * )
{
	int iWidth  = QWidget::width();
	int iHeight = QWidget::height();

	QPixmap pm(iWidth, iHeight);
	QPainter p(&pm);

	p.setViewport(0, 0, iWidth, iHeight);
	p.setWindow(0, 0, iWidth, iHeight);

	if (isEnabled()) {
		pm.fill(m_pMidiMeter->color(QTRACTOR_MIDI_METER_BACK));
	} else {
		pm.fill(Qt::gray);
	}

	int y = int(m_pMidiMeter->midiMonitor()->value() * float(iHeight));
	if (y > m_iValue) {
		m_iValue = y;
		m_fValueDecay = QTRACTOR_MIDI_METER_DECAY_RATE1;
	} else {
		m_iValue = (int) ((float) m_iValue * m_fValueDecay);
		if (y > m_iValue) {
			m_iValue = y;
		} else {
			m_fValueDecay *= m_fValueDecay;
			y = m_iValue;
		}
	}

	p.fillRect(0, iHeight - y, iWidth, y,
		m_pMidiMeter->color(QTRACTOR_MIDI_METER_OVER));

	if (y > m_iPeak) {
		m_iPeak = y;
		m_iPeakHold  = 0;
		m_fPeakDecay = QTRACTOR_MIDI_METER_DECAY_RATE2;
	} else if (++m_iPeakHold > m_pMidiMeter->peakFalloff()) {
		m_iPeak = (int) ((float) m_iPeak * m_fPeakDecay);
		if (y > m_iPeak) {
			m_iPeak = y;
		} else {
			m_fPeakDecay *= m_fPeakDecay;
		}
	}

	p.setPen(m_pMidiMeter->color(QTRACTOR_MIDI_METER_PEAK));
	p.drawLine(0, iHeight - m_iPeak, iWidth, iHeight - m_iPeak);

	bitBlt(this, 0, 0, &pm);
}


// Resize event handler.
void qtractorMidiMeterValue::resizeEvent ( QResizeEvent * )
{
	m_iPeak = 0;

	QWidget::repaint(true);
}


//----------------------------------------------------------------------------
// qtractorMidiMeter -- Audio meter bridge slot widget.

// Constructor.
qtractorMidiMeter::qtractorMidiMeter ( qtractorMidiMonitor *pMidiMonitor,
	QWidget *pParent, const char *pszName )
	: qtractorMeter(pParent, pszName)
{
	m_pMidiMonitor = pMidiMonitor;

	m_pMidiPixmap[0] = new QPixmap(QPixmap::fromMimeSource("trackMidiOff.png"));
	m_pMidiPixmap[1] = new QPixmap(QPixmap::fromMimeSource("trackMidiOn.png"));

	m_iMidiCount = 0;

	topLabel()->setAlignment(Qt::AlignRight | Qt::AlignBottom);
	topLabel()->setPixmap(*m_pMidiPixmap[0]);
	gainSpinBox()->setMinValueFloat(0.0f);
	gainSpinBox()->setMaxValueFloat(100.f);
	QToolTip::add(gainSpinBox(), tr("Volume (%)"));

	m_pMidiScale = new qtractorMidiMeterScale(this, hbox());
	m_pMidiValue = new qtractorMidiMeterValue(this, hbox());

	setPeakFalloff(QTRACTOR_MIDI_METER_PEAK_FALLOFF);

	setGain(monitor()->gain());
	setPanning(monitor()->panning());

	m_colors[QTRACTOR_MIDI_METER_PEAK] = QColor(160,220, 20);
	m_colors[QTRACTOR_MIDI_METER_OVER] = QColor( 40,160, 40);
	m_colors[QTRACTOR_MIDI_METER_BACK] = QColor( 20, 40, 20);
	m_colors[QTRACTOR_MIDI_METER_FORE] = QColor( 80, 80, 80);

	updatePanning();
	updateGain();

	reset();
}


// Default destructor.
qtractorMidiMeter::~qtractorMidiMeter (void)
{
	// No need to delete child widgets, Qt does it all for us
	delete m_pMidiValue;
	delete m_pMidiScale;

	delete m_pMidiPixmap[0];
	delete m_pMidiPixmap[1];
}


// Gain-value (percent) converters...
float qtractorMidiMeter::gainFromValue ( float fValue ) const
{
	return (0.01f * fValue);
}

float qtractorMidiMeter::valueFromGain ( float fGain ) const
{
	return (100.0f * fGain);
}


// MIDI monitor reset
void qtractorMidiMeter::reset (void)
{
}


// Reset peak holder.
void qtractorMidiMeter::peakReset (void)
{
	m_pMidiValue->peakReset();
	m_iMidiCount = 0;
}


// Slot refreshment.
void qtractorMidiMeter::refresh (void)
{
	m_pMidiValue->update();
	
	// Take care of the MIDI LED status...
	bool bMidiOn = (m_pMidiMonitor->count() > 0);
	if (bMidiOn) {
		if (m_iMidiCount == 0)
			topLabel()->setPixmap(*m_pMidiPixmap[1]);
		m_iMidiCount = QTRACTOR_MIDI_METER_HOLD_LEDON;
	} else if (m_iMidiCount > 0) {
		m_iMidiCount--;
		if (m_iMidiCount == 0)
			topLabel()->setPixmap(*m_pMidiPixmap[0]);
	}
}


// Resize event handler.
void qtractorMidiMeter::resizeEvent ( QResizeEvent * )
{
	// HACK: make so that the MIDI gain slider (volume)
	// aligns its top at the Audio 0 dB gain level...
	int iFixedHeight = int(0.15f * float(hbox()->height())) - 4;
	if (iFixedHeight < 16)
		iFixedHeight = 16;
	topLabel()->setFixedHeight(iFixedHeight);
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
const QColor& qtractorMidiMeter::color ( int iIndex ) const
{
	return m_colors[iIndex];
}


// Pan-slider value change method.
void qtractorMidiMeter::updatePanning (void)
{
	setPanning(m_pMidiMonitor->panning());

	QToolTip::remove(panSlider());
	QToolTip::add(panSlider(),
		tr("Pan: %1").arg(panning(), 0, 'g', 2));
}


// Gain-slider value change method.
void qtractorMidiMeter::updateGain (void)
{
	setGain(m_pMidiMonitor->gain());

	QToolTip::remove(gainSlider());
	QToolTip::add(gainSlider(),
		tr("Volume: %1%").arg(100.0f * gain(), 0, 'g', 3));
}


// end of qtractorMidiMeter.cpp
