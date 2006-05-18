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

#include <qtooltip.h>
#include <qpainter.h>
#include <qpixmap.h>


// The decay rates (magic goes here :).
// - value decay rate (faster)
#define QTRACTOR_MIDI_METER_DECAY_RATE1		(1.0f - 1E-4f)
// - peak decay rate (slower)
#define QTRACTOR_MIDI_METER_DECAY_RATE2		(1.0f - 1E-6f)

// Number of cycles the peak stays on hold before fall-off.
#define QTRACTOR_MIDI_METER_PEAK_FALLOFF	16


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
	drawLineLabel(p, (3 * h) / 4, "75");
	drawLineLabel(p, (2 * h) / 4, "50");
	drawLineLabel(p,      h  / 4, "25");
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

	QFrame::setMinimumWidth(12);
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

	int y = (iHeight * int(m_pMidiMeter->midiMonitor()->dequeue())) / 127;
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

	m_pMidiScale = new qtractorMidiMeterScale(this, hbox());
	m_pMidiValue = new qtractorMidiMeterValue(this, hbox());

	setPeakFalloff(QTRACTOR_MIDI_METER_PEAK_FALLOFF);

	setGain(monitor()->gain());
	setPanning(monitor()->panning());

	m_colors[QTRACTOR_MIDI_METER_PEAK] = QColor(160,220, 20);
	m_colors[QTRACTOR_MIDI_METER_OVER] = QColor( 40,160, 40);
	m_colors[QTRACTOR_MIDI_METER_BACK] = QColor( 20, 40, 20);
	m_colors[QTRACTOR_MIDI_METER_FORE] = QColor( 80, 80, 80);

	reset();

	QObject::connect(panSlider(), SIGNAL(valueChanged(int)),
		this, SLOT(panChangedSlot(int)));
	QObject::connect(gainSlider(), SIGNAL(valueChanged(int)),
		this, SLOT(gainChangedSlot(int)));

	panChangedSlot(panSlider()->value());
	gainChangedSlot(gainSlider()->value());
}


// Default destructor.
qtractorMidiMeter::~qtractorMidiMeter (void)
{
	// No need to delete child widgets, Qt does it all for us
	delete m_pMidiValue;
	delete m_pMidiScale;
}


// MIDI monitor reset
void qtractorMidiMeter::reset (void)
{
}


// Gain accessors.
void qtractorMidiMeter::setGain ( float fGain )
{
	gainSlider()->setValue(10000 - int(10000.0f * fGain));
}

float qtractorMidiMeter::gain (void) const
{
	return float(10000 - gainSlider()->value()) / 10000.0f;
}


// Stereo panning accessors.
void qtractorMidiMeter::setPanning ( float fPanning )
{
	panSlider()->setValue(int(100.0f * fPanning));
}

float qtractorMidiMeter::panning (void) const
{
	return float(panSlider()->value()) / 100.0f;
}


// Reset peak holder.
void qtractorMidiMeter::peakReset (void)
{
	m_pMidiValue->peakReset();
}


// Slot refreshment.
void qtractorMidiMeter::refresh (void)
{
	m_pMidiValue->update();
}


// Resize event handler.
void qtractorMidiMeter::resizeEvent ( QResizeEvent * )
{
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


// Pan-slider value change slot.
void qtractorMidiMeter::panChangedSlot ( int /*iValue*/ )
{
	monitor()->setPanning(panning());

	QToolTip::remove(panSlider());
	QToolTip::add(panSlider(), tr("Pan: %1").arg(panning(), 0, 'g', 3));
}


// Gain-slider value change slot.
void qtractorMidiMeter::gainChangedSlot ( int /*iValue*/ )
{
	monitor()->setGain(gain());

	QToolTip::remove(gainSlider());
	QToolTip::add(gainSlider(), tr("Gain: %1").arg(gain(), 0, 'g', 3));
}


// end of qtractorMidiMeter.cpp
