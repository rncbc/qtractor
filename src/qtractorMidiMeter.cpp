// qtractorMidiMeter.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiMeter.h"
#include "qtractorMidiMonitor.h"
#include "qtractorSlider.h"

#include <QDoubleSpinBox>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QLayout>
#include <QLabel>


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
	qtractorMidiMeter *pMidiMeter, QWidget *pParent )
	: qtractorMeterScale(pMidiMeter, pParent)
{
	pParent->layout()->addWidget(this);
}


// Actual scale drawing method.
void qtractorMidiMeterScale::paintScale ( QPainter *pPainter )
{
	qtractorMidiMeter *pMidiMeter
		= static_cast<qtractorMidiMeter *> (meter());
	if (pMidiMeter == NULL)
		return;

	int h = QWidget::height() - 4;
	int d = (h - 4) / 5;
	int n = 100;
	while (h > 0) {
		drawLineLabel(pPainter, h, QString::number(n));
		h -= d; n -= 20;
	}
}


//----------------------------------------------------------------------------
// qtractorMidiMeterValue -- Meter bridge value widget.

// Constructor.
qtractorMidiMeterValue::qtractorMidiMeterValue(
	qtractorMidiMeter *pMidiMeter, QWidget *pParent )
	: QFrame(pParent)
{
	m_pMidiMeter  = pMidiMeter;
	m_fValue      = 0.0f;
	m_iValueHold  = 0;
	m_fValueDecay = QTRACTOR_MIDI_METER_DECAY_RATE1;
	m_iPeak       = 0;
	m_iPeakHold   = 0;
	m_fPeakDecay  = QTRACTOR_MIDI_METER_DECAY_RATE2;

	QFrame::setFixedWidth(14);
	QFrame::setBackgroundRole(QPalette::NoRole);

	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Sunken);

	pParent->layout()->addWidget(this);
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
	// Grab the value...
	if (m_pMidiMeter->midiMonitor()) {
		float fValue = m_pMidiMeter->midiMonitor()->value();
		// If value pending of change, proceed for update...
		if (m_fValue < fValue) {
			m_fValue = fValue;
			if (m_iPeak > 0)
				update();
		}
	}
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

	int y = int(m_fValue * float(h)); m_fValue = 0.0f;
	if (m_iValueHold > y) {
		if (y > 0) {
			m_fValueDecay = QTRACTOR_MIDI_METER_DECAY_RATE1;
		} else {
			m_iValueHold = int(float(m_iValueHold * m_fValueDecay));
			m_fValueDecay *= m_fValueDecay;
			y = m_iValueHold;
		}
	} else {
		m_iValueHold = y;
		m_fValueDecay = QTRACTOR_MIDI_METER_DECAY_RATE1;
	}

	painter.fillRect(0, h - y, w, y,
		m_pMidiMeter->color(qtractorMidiMeter::ColorOver));

	if (m_iPeak < y) {
		m_iPeak = y;
		m_iPeakHold = 0;
		m_fPeakDecay = QTRACTOR_MIDI_METER_DECAY_RATE2;
	} else if (++m_iPeakHold > m_pMidiMeter->peakFalloff()) {
		m_iPeak = int(float(m_iPeak * m_fPeakDecay));
		if (m_iPeak < y) {
			m_iPeak = y;
		} else {
			m_fPeakDecay *= m_fPeakDecay;
		}
	}

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


//----------------------------------------------------------------------------
// qtractorMidiMeter -- Audio meter bridge slot widget.

// Constructor.
qtractorMidiMeter::qtractorMidiMeter ( qtractorMidiMonitor *pMidiMonitor,
	QWidget *pParent ) : qtractorMeter(pParent)
{
	m_pMidiMonitor = pMidiMonitor;

	m_pMidiPixmap[LedOff] = new QPixmap(":/icons/trackMidiOff.png");
	m_pMidiPixmap[LedOn]  = new QPixmap(":/icons/trackMidiOn.png");

	m_iMidiCount = 0;

	topLabel()->setPixmap(*m_pMidiPixmap[LedOff]);
	topLabel()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	gainSpinBox()->setMinimum(0.0f);
	gainSpinBox()->setMaximum(100.0f);
	gainSpinBox()->setToolTip(tr("Volume (%)"));

	m_pMidiScale = new qtractorMidiMeterScale(this, hbox());
	m_pMidiValue = new qtractorMidiMeterValue(this, hbox());

	setPeakFalloff(QTRACTOR_MIDI_METER_PEAK_FALLOFF);

	setGain(monitor()->gain());
	setPanning(monitor()->panning());

	m_colors[ColorPeak] = QColor(160,220, 20);
	m_colors[ColorOver] = QColor( 40,160, 40);
	m_colors[ColorBack] = QColor( 20, 40, 20);
	m_colors[ColorFore] = QColor( 80, 80, 80);

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

	delete m_pMidiPixmap[LedOff];
	delete m_pMidiPixmap[LedOn];
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
	m_pMidiValue->refresh();
	
	// Take care of the MIDI LED status...
	bool bMidiOn = (m_pMidiMonitor->count() > 0);
	if (bMidiOn) {
		if (m_iMidiCount == 0)
			topLabel()->setPixmap(*m_pMidiPixmap[LedOn]);
		m_iMidiCount = QTRACTOR_MIDI_METER_HOLD_LEDON;
	} else if (m_iMidiCount > 0) {
		m_iMidiCount--;
		if (m_iMidiCount == 0)
			topLabel()->setPixmap(*m_pMidiPixmap[LedOff]);
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

	panSlider()->setToolTip(
		tr("Pan: %1").arg(panning(), 0, 'g', 2));
}


// Gain-slider value change method.
void qtractorMidiMeter::updateGain (void)
{
	setGain(m_pMidiMonitor->gain());

	gainSlider()->setToolTip(
		tr("Volume: %1%").arg(100.0f * gain(), 0, 'g', 3));
}


// end of qtractorMidiMeter.cpp
