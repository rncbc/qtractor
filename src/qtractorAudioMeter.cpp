// qtractorAudioMeter.cpp
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAudioMeter.h"
#include "qtractorAudioMonitor.h"
#include "qtractorSlider.h"

#include <QDoubleSpinBox>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QLayout>
#include <QLabel>

#include <math.h>


// Meter level limits (in dB).
#define QTRACTOR_AUDIO_METER_MAXDB			+6.0f
#define QTRACTOR_AUDIO_METER_MINDB			-70.0f

// The decay rates (magic goes here :).
// - value decay rate (faster)
#define QTRACTOR_AUDIO_METER_DECAY_RATE1	(1.0f - 1E-2f)
// - peak decay rate (slower)
#define QTRACTOR_AUDIO_METER_DECAY_RATE2	(1.0f - 1E-6f)

// Number of cycles the peak stays on hold before fall-off.
#define QTRACTOR_AUDIO_METER_PEAK_FALLOFF	32


//----------------------------------------------------------------------------
// IEC standard dB scaling -- as borrowed from meterbridge (c) Steve Harris

static inline float IEC_Scale ( float dB )
{
	float fScale = 1.0f;

	if (dB < -70.0f)
		fScale = 0.0f;
	else if (dB < -60.0f)
		fScale = (dB + 70.0f) * 0.0025f;
	else if (dB < -50.0f)
		fScale = (dB + 60.0f) * 0.005f + 0.025f;
	else if (dB < -40.0)
		fScale = (dB + 50.0f) * 0.0075f + 0.075f;
	else if (dB < -30.0f)
		fScale = (dB + 40.0f) * 0.015f + 0.15f;
	else if (dB < -20.0f)
		fScale = (dB + 30.0f) * 0.02f + 0.3f;
	else if (dB < -0.001f || dB > 0.001f)  /* if (dB < 0.0f) */
		fScale = (dB + 20.0f) * 0.025f + 0.5f;

	return fScale;
}

static inline float IEC_dB ( float fScale )
{
	float dB = 0.0f;

	if (fScale < 0.025f)	    // IEC_Scale(-60.0f)
		dB = (fScale / 0.0025f) - 70.0f;
	else if (fScale < 0.075f)	// IEC_Scale(-50.0f)
		dB = (fScale - 0.025f) / 0.005f - 60.0f;
	else if (fScale < 0.15f)	// IEC_Scale(-40.0f)
		dB = (fScale - 0.075f) / 0.0075f - 50.0f;
	else if (fScale < 0.3f)		// IEC_Scale(-30.0f)
		dB = (fScale - 0.15f) / 0.015f - 40.0f;
	else if (fScale < 0.5f)		// IEC_Scale(-20.0f)
		dB = (fScale - 0.3f) / 0.02f - 30.0f;
	else /* if (fScale < 1.0f)	// IED_Scale(0.0f)) */
		dB = (fScale - 0.5f) / 0.025f - 20.0f;

	return (dB > -0.001f && dB < 0.001f ? 0.0f : dB);
}


//----------------------------------------------------------------------------
// qtractorAudioMeterScale -- Meter bridge scale widget.

// Constructor.
qtractorAudioMeterScale::qtractorAudioMeterScale (
	qtractorAudioMeter *pAudioMeter, QWidget *pParent )
	: qtractorMeterScale(pAudioMeter, pParent)
{
	pParent->layout()->addWidget(this);
}


// Actual scale drawing method.
void qtractorAudioMeterScale::paintScale ( QPainter *p )
{
	qtractorAudioMeter *pAudioMeter
		= static_cast<qtractorAudioMeter *> (meter());
	if (pAudioMeter == NULL)
		return;

	p->setWindow(0, -4, QWidget::width(), QWidget::height() + 8);

	drawLineLabel(p, pAudioMeter->iec_level(qtractorAudioMeter::Color0dB), "0");
	drawLineLabel(p, pAudioMeter->iec_level(qtractorAudioMeter::Color3dB), "3");
	drawLineLabel(p, pAudioMeter->iec_level(qtractorAudioMeter::Color6dB), "6");
	drawLineLabel(p, pAudioMeter->iec_level(qtractorAudioMeter::Color10dB), "10");

	for (float dB = -20.0f; dB > QTRACTOR_AUDIO_METER_MINDB; dB -= 10.0f)
		drawLineLabel(p, pAudioMeter->iec_scale(dB), QString::number(-int(dB)));
}


//----------------------------------------------------------------------------
// qtractorAudioMeterValue -- Meter bridge value widget.

// Constructor.
qtractorAudioMeterValue::qtractorAudioMeterValue(
	qtractorAudioMeter *pAudioMeter, unsigned short iChannel,
	QWidget *pParent ): QFrame(pParent)
{
	m_pAudioMeter = pAudioMeter;
	m_iChannel    = iChannel;
	m_iValue      = 0;
	m_fValueDecay = QTRACTOR_AUDIO_METER_DECAY_RATE1;
	m_iPeak       = 0;
	m_iPeakHold   = 0;
	m_fPeakDecay  = QTRACTOR_AUDIO_METER_DECAY_RATE2;
	m_iPeakColor  = qtractorAudioMeter::Color6dB;

	QFrame::setFixedWidth(10);
	QFrame::setBackgroundRole(QPalette::NoRole);

	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Sunken);

	pParent->layout()->addWidget(this);
}

// Default destructor.
qtractorAudioMeterValue::~qtractorAudioMeterValue (void)
{
}


// Reset peak holder.
void qtractorAudioMeterValue::peakReset (void)
{
	m_iPeak = 0;
}

// Paint event handler.
void qtractorAudioMeterValue::paintEvent ( QPaintEvent * )
{
	QPainter painter(this);

	int w = QWidget::width();
	int h = QWidget::height();
	int y;

	if (isEnabled()) {
		painter.fillRect(0, 0, w, h,
			m_pAudioMeter->color(qtractorAudioMeter::ColorBack));
		y = m_pAudioMeter->iec_level(qtractorAudioMeter::Color0dB);
		painter.setPen(m_pAudioMeter->color(qtractorAudioMeter::ColorFore));
		painter.drawLine(0, h - y, w, h - y);
	} else {
		painter.fillRect(0, 0, w, h, Qt::gray);
	}

	float dB = QTRACTOR_AUDIO_METER_MINDB;
	float fValue = m_pAudioMeter->audioMonitor()->value(m_iChannel);
	if (fValue > 0.0f)
		dB = 20.0f * ::log10f(fValue);

	if (dB < QTRACTOR_AUDIO_METER_MINDB)
		dB = QTRACTOR_AUDIO_METER_MINDB;
	else if (dB > QTRACTOR_AUDIO_METER_MAXDB)
		dB = QTRACTOR_AUDIO_METER_MAXDB;

	int y_over = 0;
	int y_curr = 0;

	y = m_pAudioMeter->iec_scale(dB);
	if (y > m_iValue) {
		m_iValue = y;
		m_fValueDecay = QTRACTOR_AUDIO_METER_DECAY_RATE1;
	} else {
		m_iValue = int(float(m_iValue * m_fValueDecay));
		if (y > m_iValue) {
			m_iValue = y;
		} else {
			m_fValueDecay *= m_fValueDecay;
			y = m_iValue;
		}
	}

	int iLevel;
	for (iLevel = qtractorAudioMeter::Color10dB;
			iLevel > qtractorAudioMeter::ColorOver && y >= y_over; iLevel--) {
		y_curr = m_pAudioMeter->iec_level(iLevel);
		if (y < y_curr) {
			painter.fillRect(0, h - y, w, y - y_over,
				m_pAudioMeter->color(iLevel));
		} else {
			painter.fillRect(0, h - y_curr, w, y_curr - y_over,
				m_pAudioMeter->color(iLevel));
		}
		y_over = y_curr;
	}

	if (y > y_over) {
		painter.fillRect(0, h - y, w, y - y_over,
			m_pAudioMeter->color(qtractorAudioMeter::ColorOver));
	}

	if (y > m_iPeak) {
		m_iPeak = y;
		m_iPeakHold  = 0;
		m_fPeakDecay = QTRACTOR_AUDIO_METER_DECAY_RATE2;
		m_iPeakColor = iLevel;
	} else if (++m_iPeakHold > m_pAudioMeter->peakFalloff()) {
		m_iPeak = int(float(m_iPeak * m_fPeakDecay));
		if (y > m_iPeak) {
			m_iPeak = y;
		} else {
			if (m_iPeak < m_pAudioMeter->iec_level(qtractorAudioMeter::Color10dB))
				m_iPeakColor = qtractorAudioMeter::Color6dB;
			m_fPeakDecay *= m_fPeakDecay;
		}
	}

	painter.setPen(m_pAudioMeter->color(m_iPeakColor));
	painter.drawLine(0, h - m_iPeak, w, h - m_iPeak);
}


// Resize event handler.
void qtractorAudioMeterValue::resizeEvent (QResizeEvent *pResizeEvent)
{
	m_iPeak = 0;

	QWidget::resizeEvent(pResizeEvent);
//	QWidget::repaint();
}


//----------------------------------------------------------------------------
// qtractorAudioMeter -- Audio meter bridge slot widget.

// Constructor.
qtractorAudioMeter::qtractorAudioMeter ( qtractorAudioMonitor *pAudioMonitor,
	QWidget *pParent ) : qtractorMeter(pParent)
{
	m_pAudioMonitor = pAudioMonitor;

	m_iChannels     = 0;
	m_pAudioScale   = new qtractorAudioMeterScale(this, hbox());
	m_fScale        = 0.0f;
	m_ppAudioValues = NULL;

	topLabel()->hide();

	gainSlider()->setMaximum(10000
		+ int(10000.0f * 0.025f * QTRACTOR_AUDIO_METER_MAXDB));

	gainSpinBox()->setMinimum(QTRACTOR_AUDIO_METER_MINDB);
	gainSpinBox()->setMaximum(QTRACTOR_AUDIO_METER_MAXDB);
	gainSpinBox()->setToolTip(tr("Gain (dB)"));

	setPeakFalloff(QTRACTOR_AUDIO_METER_PEAK_FALLOFF);

	for (int i = 0; i < LevelCount; i++)
		m_levels[i] = 0;

	m_colors[ColorOver] = QColor(240,  0, 20);
	m_colors[Color0dB]  = QColor(240,160, 20);
	m_colors[Color3dB]  = QColor(220,220, 20);
	m_colors[Color6dB]  = QColor(160,220, 20);
	m_colors[Color10dB] = QColor( 40,160, 40);
	m_colors[ColorBack] = QColor( 20, 40, 20);
	m_colors[ColorFore] = QColor( 80, 80, 80);

	updatePanning();
	updateGain();

	reset();
}


// Default destructor.
qtractorAudioMeter::~qtractorAudioMeter (void)
{
	// No need to delete child widgets, Qt does it all for us
	for (unsigned short i = 0; i < m_iChannels; i++)
		delete m_ppAudioValues[i];

	delete [] m_ppAudioValues;
	delete m_pAudioScale;
}


// IEC standard
int qtractorAudioMeter::iec_scale ( float dB ) const
{
	return int(m_fScale * IEC_Scale(dB));
}


int qtractorAudioMeter::iec_level ( int iIndex ) const
{
	return m_levels[iIndex];
}


// Gain-scale converters...
float qtractorAudioMeter::gainFromScale ( float fScale ) const
{
	return ::powf(10.0f, IEC_dB(fScale) / 20.0f);
}

float qtractorAudioMeter::scaleFromGain ( float fGain ) const
{
	return IEC_Scale(valueFromGain(fGain));
}


// Gain-value (dB) converters...
float qtractorAudioMeter::gainFromValue ( float fValue ) const
{
	return gainFromScale(IEC_Scale(fValue));
}

float qtractorAudioMeter::valueFromGain ( float fGain ) const
{
	float fValue = QTRACTOR_AUDIO_METER_MINDB;
	if (fGain > 0.0f)
		fValue = 20.0f * ::log10f(fGain);
	return fValue;
}


// Audio monitor reset
void qtractorAudioMeter::reset (void)
{
	if (m_pAudioMonitor == NULL)
		return;

	unsigned short iChannels = m_pAudioMonitor->channels();

	if (m_iChannels == iChannels)
		return;

	if (m_ppAudioValues) {
		for (unsigned short i = 0; i < m_iChannels; i++)
			delete m_ppAudioValues[i];
		delete [] m_ppAudioValues;
		m_ppAudioValues = NULL;
	}

	m_iChannels = iChannels;
	if (m_iChannels > 0) {
		m_ppAudioValues = new qtractorAudioMeterValue *[m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; i++) {
			m_ppAudioValues[i] = new qtractorAudioMeterValue(this, i, hbox());
			m_ppAudioValues[i]->show();
		}
	}

	panSlider()->setEnabled(m_iChannels > 1);
	panSpinBox()->setEnabled(m_iChannels > 1);
}


// Reset peak holder.
void qtractorAudioMeter::peakReset (void)
{
	for (unsigned short i = 0; i < m_iChannels; i++)
		m_ppAudioValues[i]->peakReset();
}


// Slot refreshment.
void qtractorAudioMeter::refresh (void)
{
	for (unsigned short i = 0; i < m_iChannels; i++)
		m_ppAudioValues[i]->update();
}


// Resize event handler.
void qtractorAudioMeter::resizeEvent ( QResizeEvent * )
{
	m_fScale = (1.0f - 0.025f * QTRACTOR_AUDIO_METER_MAXDB)
		* float(hbox()->height());

	m_levels[Color0dB]  = iec_scale(  0.0f);
	m_levels[Color3dB]  = iec_scale( -3.0f);
	m_levels[Color6dB]  = iec_scale( -6.0f);
	m_levels[Color10dB] = iec_scale(-10.0f);
}


// Virtual monitor accessor.
void qtractorAudioMeter::setMonitor ( qtractorMonitor *pMonitor )
{
	setAudioMonitor(static_cast<qtractorAudioMonitor *> (pMonitor));
}

qtractorMonitor *qtractorAudioMeter::monitor (void) const
{
	return audioMonitor();
}


// Audio monitor accessor.
void qtractorAudioMeter::setAudioMonitor ( qtractorAudioMonitor *pAudioMonitor )
{
	m_pAudioMonitor = pAudioMonitor;

	reset();
}

qtractorAudioMonitor *qtractorAudioMeter::audioMonitor (void) const
{
	return m_pAudioMonitor;
}


// Common resource accessor.
const QColor& qtractorAudioMeter::color ( int iIndex ) const
{
	return m_colors[iIndex];
}


// Pan-slider value change method.
void qtractorAudioMeter::updatePanning (void)
{
	setPanning(m_pAudioMonitor->panning());

	panSlider()->setToolTip(
		tr("Pan: %1").arg(panning(), 0, 'g', 2));
}

// Gain-slider value change method.
void qtractorAudioMeter::updateGain (void)
{
	setGain(m_pAudioMonitor->gain());

	gainSlider()->setToolTip(
		tr("Gain: %1 dB").arg(IEC_dB(gainScale()), 0, 'g', 3));
}


// end of qtractorAudioMeter.cpp
