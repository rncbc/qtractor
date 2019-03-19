// qtractorAudioMeter.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioMeter.h"
#include "qtractorAudioMonitor.h"

#include "qtractorObserverWidget.h"

#include "qtractorMidiControlObserver.h"

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


// Possible 20 * log10(x) optimization
// (borrowed from musicdsp.org)
static inline float log10f2_opt ( float x )
{
#ifdef CONFIG_FLOAT32_NOP
#	define M_LOG10F20 6.0205999132796239042f // (= 20.0f * M_LN2 / M_LN10)
	// Avoid strict-aliasing optimization (gcc -O2).
	union { float f; int i; } u;
	u.f = x;
	return M_LOG10F20 * ((((u.i & 0x7f800000) >> 23) - 0x7f)
		+ (u.i & 0x007fffff) / (float) 0x800000);
#else
	return 20.0f * ::log10f(x);
#endif
}

static inline float log10f2 ( float x )
{
	return (x > 0.0f ? ::log10f2_opt(x) : QTRACTOR_AUDIO_METER_MINDB);
}

static inline float pow10f2 ( float x )
{
	return ::powf(10.0f, 0.05f * x);
}


// Ref. P.448. Approximate cube root of an IEEE float
// Hacker's Delight (2nd Edition), by Henry S. Warren
// http://www.hackersdelight.org/hdcodetxt/acbrt.c.txt
//
static inline float cbrtf2 ( float x )
{
#ifdef CONFIG_FLOAT32//_NOP
	// Avoid strict-aliasing optimization (gcc -O2).
	union { float f; int i; } u;
	u.f  = x;
	u.i  = (u.i >> 4) + (u.i >> 2);
	u.i += (u.i >> 4) + 0x2a6497f8;
//	return 0.33333333f * (2.0f * u.f + x / (u.f * u.f));
	return u.f;
#else
	return ::cbrtf(x);
#endif
}

static inline float cubef2 ( float x )
{
	return x * x * x;
}


// Audio meter default color array.
QColor qtractorAudioMeter::g_defaultColors[ColorCount] = {
	QColor(240,  0, 20),	// ColorOver
	QColor(240,160, 20),	// Color0dB
	QColor(220,220, 20),	// Color3dB
	QColor(160,220, 20),	// Color6dB
	QColor( 40,160, 40),	// Color10dB
	QColor( 20, 40, 20),	// ColorBack
	QColor( 80, 80, 80) 	// ColorFore
};

// Audio meter color array.
QColor qtractorAudioMeter::g_currentColors[ColorCount] = {
	g_defaultColors[ColorOver],
	g_defaultColors[Color0dB],
	g_defaultColors[Color3dB],
	g_defaultColors[Color6dB],
	g_defaultColors[Color10dB],
	g_defaultColors[ColorBack],
	g_defaultColors[ColorFore]
};


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
	qtractorAudioMeter *pAudioMeter ) : qtractorMeterScale(pAudioMeter)
{
	// Nothing much to do...
}


// Actual scale drawing method.
void qtractorAudioMeterScale::paintScale ( QPainter *p )
{
	qtractorAudioMeter *pAudioMeter
		= static_cast<qtractorAudioMeter *> (meter());
	if (pAudioMeter == NULL)
		return;

//	p->setWindow(0, -4, QWidget::width(), QWidget::height() + 8);

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
qtractorAudioMeterValue::qtractorAudioMeterValue (
	qtractorAudioMeter *pAudioMeter, unsigned short iChannel )
	: qtractorMeterValue(pAudioMeter), m_iChannel(iChannel)
{
	// Avoid intensively annoying repaints...
	QWidget::setAttribute(Qt::WA_StaticContents);
	QWidget::setAttribute(Qt::WA_OpaquePaintEvent);

	QWidget::setBackgroundRole(QPalette::NoRole);

	m_iValue      = 0;
	m_fValueDecay = QTRACTOR_AUDIO_METER_DECAY_RATE1;

	m_iPeak       = 0;
	m_iPeakHold   = 0;
	m_fPeakDecay  = QTRACTOR_AUDIO_METER_DECAY_RATE2;
	m_iPeakColor  = qtractorAudioMeter::Color6dB;

	QWidget::setMinimumWidth(2);
	QWidget::setMaximumWidth(14);
}


// Value refreshment.
void qtractorAudioMeterValue::refresh ( unsigned long iStamp )
{
	qtractorAudioMeter *pAudioMeter
		= static_cast<qtractorAudioMeter *> (meter());
	if (pAudioMeter == NULL)
		return;

	qtractorAudioMonitor *pAudioMonitor = pAudioMeter->audioMonitor();
	if (pAudioMonitor == NULL)
		return;

	const float fValue = pAudioMonitor->value_stamp(m_iChannel, iStamp);
	if (fValue < 0.001f && m_iPeak < 1)
		return;
#if 0
	float dB = QTRACTOR_AUDIO_METER_MINDB;
	if (fValue > 0.0f)
		dB = log10f2_opt(fValue);
	if (dB < QTRACTOR_AUDIO_METER_MINDB)
		dB = QTRACTOR_AUDIO_METER_MINDB;
	else if (dB > QTRACTOR_AUDIO_METER_MAXDB)
		dB = QTRACTOR_AUDIO_METER_MAXDB;
	int iValue = m_pAudioMeter->iec_scale(dB);
#else
	int iValue = 0;
	if (fValue > 0.001f)
		iValue = pAudioMeter->scale(::cbrtf2(fValue));
#endif
	if (iValue < m_iValue) {
		iValue = int(m_fValueDecay * float(m_iValue));
		m_fValueDecay *= m_fValueDecay;
	} else {
		m_fValueDecay = QTRACTOR_AUDIO_METER_DECAY_RATE1;
	}

	int iPeak = m_iPeak;
	if (iPeak < iValue) {
		iPeak = iValue;
		m_iPeakHold = 0;
		m_fPeakDecay = QTRACTOR_AUDIO_METER_DECAY_RATE2;
		m_iPeakColor = qtractorAudioMeter::Color10dB;
		for (; m_iPeakColor > qtractorAudioMeter::ColorOver
			&& iPeak >= pAudioMeter->iec_level(m_iPeakColor); --m_iPeakColor)
			/* empty body loop */;
	} else if (++m_iPeakHold > pAudioMeter->peakFalloff()) {
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
void qtractorAudioMeterValue::paintEvent ( QPaintEvent * )
{
	qtractorAudioMeter *pAudioMeter
		= static_cast<qtractorAudioMeter *> (meter());
	if (pAudioMeter == NULL)
		return;

	QPainter painter(this);

	const int w = QWidget::width();
	const int h = QWidget::height();

	int y;

	if (isEnabled()) {
		painter.fillRect(0, 0, w, h,
			pAudioMeter->color(qtractorAudioMeter::ColorBack));
		y = h - pAudioMeter->iec_level(qtractorAudioMeter::Color0dB);
		painter.setPen(pAudioMeter->color(qtractorAudioMeter::ColorFore));
		painter.drawLine(0, y, w, y);
	} else {
		painter.fillRect(0, 0, w, h, Qt::gray);
	}

#ifdef CONFIG_GRADIENT
	y = h - m_iValue;
	painter.drawPixmap(0, y, pAudioMeter->pixmap(), 0, y, w, m_iValue);
#else
	y = m_iValue;
	
	int y_over = 0;
	int y_curr = 0;

	for (int i = qtractorAudioMeter::Color10dB;
			i > qtractorAudioMeter::ColorOver && y >= y_over; --i) {
		y_curr = pAudioMeter->iec_level(i);
		if (y < y_curr) {
			painter.fillRect(0, h - y, w, y - y_over,
				pAudioMeter->color(i));
		} else {
			painter.fillRect(0, h - y_curr, w, y_curr - y_over,
				pAudioMeter->color(i));
		}
		y_over = y_curr;
	}

	if (y > y_over) {
		painter.fillRect(0, h - y, w, y - y_over,
			pAudioMeter->color(qtractorAudioMeter::ColorOver));
	}
#endif

	y = h - m_iPeak;
	painter.setPen(pAudioMeter->color(m_iPeakColor));
	painter.drawLine(0, y, w, y);
}


// Resize event handler.
void qtractorAudioMeterValue::resizeEvent ( QResizeEvent *pResizeEvent )
{
	m_iPeak = 0;

	qtractorMeterValue::resizeEvent(pResizeEvent);
}


//----------------------------------------------------------------------------
// qtractorAudioMeter -- Audio meter bridge slot widget.

// Constructor.
qtractorAudioMeter::qtractorAudioMeter (
	qtractorAudioMonitor *pAudioMonitor, QWidget *pParent )
	: qtractorMeter(pParent)
{
	m_pAudioMonitor = pAudioMonitor;

	m_iChannels     = 0;
	m_ppAudioValues = NULL;
	m_iRegenerate   = 0;

#ifdef CONFIG_GRADIENT
	m_pPixmap = new QPixmap();
#endif

	setPeakFalloff(QTRACTOR_AUDIO_METER_PEAK_FALLOFF);

	for (int i = 0; i < LevelCount; ++i)
		m_levels[i] = 0;

	reset();
}


// Default destructor.
qtractorAudioMeter::~qtractorAudioMeter (void)
{
#ifdef CONFIG_GRADIENT
	delete m_pPixmap;
#endif

	// No need to delete child widgets, Qt does it all for us
	for (unsigned short i = 0; i < m_iChannels; ++i)
		delete m_ppAudioValues[i];
	delete [] m_ppAudioValues;
}


// IEC standard
int qtractorAudioMeter::iec_scale ( float dB ) const
{
	return scale(IEC_Scale(dB));
}


int qtractorAudioMeter::iec_level ( int iIndex ) const
{
	return m_levels[iIndex];
}


// Audio monitor reset
void qtractorAudioMeter::reset (void)
{
	if (m_pAudioMonitor == NULL)
		return;

	const unsigned short iChannels
		= m_pAudioMonitor->channels();

	if (m_iChannels == iChannels)
		return;

	if (m_ppAudioValues) {
		qtractorMeter::hide();
		for (unsigned short i = 0; i < m_iChannels; ++i) {
		//	m_ppAudioValues[i]->hide();
			boxLayout()->removeWidget(m_ppAudioValues[i]);
			delete m_ppAudioValues[i];
		}
		delete [] m_ppAudioValues;
		m_ppAudioValues = NULL;
		++m_iRegenerate;
	}

	m_iChannels = iChannels;

	if (m_iChannels > 0) {
		m_ppAudioValues = new qtractorAudioMeterValue *[m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; ++i) {
			m_ppAudioValues[i] = new qtractorAudioMeterValue(this, i);
			boxLayout()->addWidget(m_ppAudioValues[i]);
		//	m_ppAudioValues[i]->show();
		}
		if (m_iRegenerate > 0)
			qtractorMeter::show();
	}
}


#ifdef CONFIG_GRADIENT
// Gradient pixmap accessor.
const QPixmap& qtractorAudioMeter::pixmap (void) const
{
	return *m_pPixmap;
}

void qtractorAudioMeter::updatePixmap (void)
{
	const int w = QWidget::width();
	const int h = QWidget::height();

	QLinearGradient grad(0, 0, 0, h);
	grad.setColorAt(0.1f, color(ColorOver));
	grad.setColorAt(0.2f, color(Color0dB));
	grad.setColorAt(0.3f, color(Color3dB));
	grad.setColorAt(0.4f, color(Color6dB));
	grad.setColorAt(0.8f, color(Color10dB));

	*m_pPixmap = QPixmap(w, h);

	QPainter(m_pPixmap).fillRect(0, 0, w, h, grad);
}
#endif


// Resize event handler.
void qtractorAudioMeter::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorMeter::setScale(0.85f * float(QWidget::height()));

	m_levels[Color0dB]  = iec_scale(  0.0f);
	m_levels[Color3dB]  = iec_scale( -3.0f);
	m_levels[Color6dB]  = iec_scale( -6.0f);
	m_levels[Color10dB] = iec_scale(-10.0f);

#ifdef CONFIG_GRADIENT
	updatePixmap();
#endif

	if (m_iChannels > 0) {
		int iMaxWidth = QWidget::width() / m_iChannels - 1;
		if (iMaxWidth < 2)
			iMaxWidth = 2;
		if (iMaxWidth > 14)
			iMaxWidth = 14;
		for (unsigned short i = 0; i < m_iChannels; ++i)
			m_ppAudioValues[i]->setMaximumWidth(iMaxWidth);
	}

	qtractorMeter::resizeEvent(pResizeEvent);
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


// Common resource accessor (static).
void qtractorAudioMeter::setColor ( int iIndex, const QColor& color )
{
	g_currentColors[iIndex] = color;
}

const QColor& qtractorAudioMeter::color ( int iIndex )
{
	return g_currentColors[iIndex];
}

const QColor& qtractorAudioMeter::defaultColor ( int iIndex )
{
	return g_defaultColors[iIndex];
}


//----------------------------------------------------------------------
// class qtractorAudioMixerMeter::GainSpinBoxInterface -- Observer interface.
//

// Local converter interface.
class qtractorAudioMixerMeter::GainSpinBoxInterface
	: public qtractorObserverSpinBox::Interface
{
public:

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return log10f2(fValue); }

	float valueFromScale ( float fScale ) const
		{ return pow10f2(fScale); }
};


//----------------------------------------------------------------------
// class qtractorAudioMixerMeter::GainSliderInterface -- Observer interface.
//

// Local converter interface.
class qtractorAudioMixerMeter::GainSliderInterface
	: public qtractorObserverSlider::Interface
{
public:

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 10000.0f * IEC_Scale(log10f2(fValue)); }

	float valueFromScale ( float fScale ) const
		{ return pow10f2(IEC_dB(fScale / 10000.0f)); }
};


//----------------------------------------------------------------------------
// qtractorAudioMeter -- Audio meter bridge slot widget.

// Constructor.
qtractorAudioMixerMeter::qtractorAudioMixerMeter (
	qtractorAudioMonitor *pAudioMonitor, QWidget *pParent )
	: qtractorMixerMeter(pParent)
{
	m_pAudioMeter = new qtractorAudioMeter(pAudioMonitor);
	m_pAudioScale = new qtractorAudioMeterScale(m_pAudioMeter);

	topWidget()->hide();

	boxLayout()->addWidget(m_pAudioScale);
	boxLayout()->addWidget(m_pAudioMeter);

	gainSlider()->setInterface(new GainSliderInterface());
	gainSpinBox()->setInterface(new GainSpinBoxInterface());

	gainSlider()->setMaximum(11500);

	gainSpinBox()->setMinimum(QTRACTOR_AUDIO_METER_MINDB);
	gainSpinBox()->setMaximum(QTRACTOR_AUDIO_METER_MAXDB);
	gainSpinBox()->setToolTip(tr("Gain (dB)"));
	gainSpinBox()->setSuffix(tr(" dB"));

	reset();

	updatePanning();
	updateGain();
}


// Default destructor.
qtractorAudioMixerMeter::~qtractorAudioMixerMeter (void)
{
	delete m_pAudioScale;
	delete m_pAudioMeter;

	// No need to delete child widgets, Qt does it all for us
}


// Audio monitor reset
void qtractorAudioMixerMeter::reset (void)
{
	qtractorAudioMonitor *pAudioMonitor = m_pAudioMeter->audioMonitor();
	if (pAudioMonitor == NULL)
		return;

	m_pAudioMeter->reset();

	setPanningSubject(pAudioMonitor->panningSubject());
	setGainSubject(pAudioMonitor->gainSubject());

	const unsigned short iChannels = pAudioMonitor->channels();

	panSlider()->setEnabled(iChannels > 1);
	panSpinBox()->setEnabled(iChannels > 1);
}


// Virtual monitor accessor.
void qtractorAudioMixerMeter::setMonitor ( qtractorMonitor *pMonitor )
{
	m_pAudioMeter->setMonitor(pMonitor);
}

qtractorMonitor *qtractorAudioMixerMeter::monitor (void) const
{
	return m_pAudioMeter->monitor();
}


// Audio monitor accessor.
void qtractorAudioMixerMeter::setAudioMonitor ( qtractorAudioMonitor *pAudioMonitor )
{
	m_pAudioMeter->setAudioMonitor(pAudioMonitor);

	reset();
}

qtractorAudioMonitor *qtractorAudioMixerMeter::audioMonitor (void) const
{
	return m_pAudioMeter->audioMonitor();
}


// Pan-slider value change method.
void qtractorAudioMixerMeter::updatePanning (void)
{
//	setPanning(m_pAudioMonitor->panning());

	panSlider()->setToolTip(
		tr("Pan: %1").arg(panning(), 0, 'g', 1));
}

// Gain-slider value change method.
void qtractorAudioMixerMeter::updateGain (void)
{
//	setGain(m_pAudioMonitor->gain());

	gainSlider()->setToolTip(
		tr("Gain: %1 dB").arg(gainSpinBox()->value(), 0, 'g', 3));
}


// end of qtractorAudioMeter.cpp
