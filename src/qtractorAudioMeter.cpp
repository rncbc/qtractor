// qtractorAudioMeter.cpp
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
#ifdef CONFIG_FLOAT32
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
	return (x > 0.0f ? 20.0f * ::log10f(x) : QTRACTOR_AUDIO_METER_MINDB);
}

static inline float pow10f2 ( float x )
{
	return ::powf(10.0f, 0.05f * x);
}


// Audio meter default color array.
QColor qtractorAudioMeter::g_defaultColors[qtractorAudioMeter::ColorCount] = {
	QColor(240,  0, 20),	// ColorOver
	QColor(240,160, 20),	// Color0dB
	QColor(220,220, 20),	// Color3dB
	QColor(160,220, 20),	// Color6dB
	QColor( 40,160, 40),	// Color10dB
	QColor( 20, 40, 20),	// ColorBack
	QColor( 80, 80, 80) 	// ColorFore
};

// Audio meter color array.
QColor qtractorAudioMeter::g_currentColors[qtractorAudioMeter::ColorCount] = {
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
	qtractorAudioMeter *pAudioMeter, QWidget *pParent )
	: qtractorMeterScale(pAudioMeter, pParent)
{
	pAudioMeter->boxLayout()->addWidget(this);
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
	qtractorAudioMeter *pAudioMeter, unsigned short iChannel, QWidget *pParent )
	: QWidget(pParent), m_pAudioMeter(pAudioMeter), m_iChannel(iChannel)
{
	// Avoid intensively annoying repaints...
	QWidget::setAttribute(Qt::WA_StaticContents);
	QWidget::setAttribute(Qt::WA_OpaquePaintEvent);

	m_iValue      = 0;
	m_fValueDecay = QTRACTOR_AUDIO_METER_DECAY_RATE1;

	m_iPeak       = 0;
	m_iPeakHold   = 0;
	m_fPeakDecay  = QTRACTOR_AUDIO_METER_DECAY_RATE2;
	m_iPeakColor  = qtractorAudioMeter::Color6dB;

	QWidget::setFixedWidth(10);
	QWidget::setBackgroundRole(QPalette::NoRole);

	pAudioMeter->boxLayout()->addWidget(this);
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


// Value refreshment.
void qtractorAudioMeterValue::refresh (void)
{
	qtractorAudioMonitor *pAudioMonitor = m_pAudioMeter->audioMonitor();
	if (pAudioMonitor == NULL)
		return;

	float fValue = pAudioMonitor->value(m_iChannel);
	if (fValue < 0.001f && m_iPeak < 1)
		return;

	float dB = QTRACTOR_AUDIO_METER_MINDB;
	if (fValue > 0.0f)
		dB = log10f2_opt(fValue);
	if (dB < QTRACTOR_AUDIO_METER_MINDB)
		dB = QTRACTOR_AUDIO_METER_MINDB;
	else if (dB > QTRACTOR_AUDIO_METER_MAXDB)
		dB = QTRACTOR_AUDIO_METER_MAXDB;

	int iValue = m_pAudioMeter->iec_scale(dB);
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
			&& iPeak >= m_pAudioMeter->iec_level(m_iPeakColor); --m_iPeakColor)
			/* empty body loop */;
	} else if (++m_iPeakHold > m_pAudioMeter->peakFalloff()) {
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

#ifdef CONFIG_GRADIENT
	painter.drawPixmap(0, h - m_iValue,
		m_pAudioMeter->pixmap(), 0, h - m_iValue, w, m_iValue);
#else
	y = m_iValue;
	
	int y_over = 0;
	int y_curr = 0;

	for (int i = qtractorAudioMeter::Color10dB;
			i > qtractorAudioMeter::ColorOver && y >= y_over; --i) {
		y_curr = m_pAudioMeter->iec_level(i);
		if (y < y_curr) {
			painter.fillRect(0, h - y, w, y - y_over,
				m_pAudioMeter->color(i));
		} else {
			painter.fillRect(0, h - y_curr, w, y_curr - y_over,
				m_pAudioMeter->color(i));
		}
		y_over = y_curr;
	}

	if (y > y_over) {
		painter.fillRect(0, h - y, w, y - y_over,
			m_pAudioMeter->color(qtractorAudioMeter::ColorOver));
	}
#endif

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


//----------------------------------------------------------------------
// class qtractorAudioMeter::GainSpinBoxInterface -- Observer interface.
//

// Local converter interface.
class qtractorAudioMeter::GainSpinBoxInterface
	: public qtractorObserverSpinBox::Interface
{
public:

	// Constructor.
	GainSpinBoxInterface ( qtractorObserverSpinBox *pSpinBox )
		: qtractorObserverSpinBox::Interface(pSpinBox) {}

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return log10f2(fValue); }

	float valueFromScale ( float fScale ) const
		{ return pow10f2(fScale); }
};


//----------------------------------------------------------------------
// class qtractorAudioMeter::GainSliderInterface -- Observer interface.
//

// Local converter interface.
class qtractorAudioMeter::GainSliderInterface
	: public qtractorObserverSlider::Interface
{
public:

	// Constructor.
	GainSliderInterface ( qtractorObserverSlider *pSlider )
		: qtractorObserverSlider::Interface(pSlider) {}

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 10000.0f * IEC_Scale(log10f2(fValue)); }

	float valueFromScale ( float fScale ) const
		{ return pow10f2(IEC_dB(fScale / 10000.0f)); }
};


//----------------------------------------------------------------------------
// qtractorAudioMeter -- Audio meter bridge slot widget.

// Constructor.
qtractorAudioMeter::qtractorAudioMeter ( qtractorAudioMonitor *pAudioMonitor,
	QWidget *pParent ) : qtractorMeter(pParent)
{
	m_pAudioMonitor = pAudioMonitor;

	m_iChannels     = 0;
	m_pAudioScale   = new qtractorAudioMeterScale(this/*, boxWidget()*/);
	m_fScale        = 0.0f;
	m_ppAudioValues = NULL;

#ifdef CONFIG_GRADIENT
	m_pPixmap = new QPixmap();
#endif

	topWidget()->hide();

	gainObserver()->setLogarithmic(true);

	gainSlider()->setInterface(new GainSliderInterface(gainSlider()));
	gainSpinBox()->setInterface(new GainSpinBoxInterface(gainSpinBox()));

	gainSlider()->setMaximum(11500);

	gainSpinBox()->setMinimum(QTRACTOR_AUDIO_METER_MINDB);
	gainSpinBox()->setMaximum(QTRACTOR_AUDIO_METER_MAXDB);
	gainSpinBox()->setToolTip(tr("Gain (dB)"));
	gainSpinBox()->setSuffix(tr(" dB"));

	setPeakFalloff(QTRACTOR_AUDIO_METER_PEAK_FALLOFF);

	for (int i = 0; i < LevelCount; ++i)
		m_levels[i] = 0;

	reset();

	updatePanning();
	updateGain();
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


// Audio monitor reset
void qtractorAudioMeter::reset (void)
{
	if (m_pAudioMonitor == NULL)
		return;

	setPanningSubject(m_pAudioMonitor->panningSubject());
	setGainSubject(m_pAudioMonitor->gainSubject());

	unsigned short iChannels = m_pAudioMonitor->channels();

	if (m_iChannels == iChannels)
		return;

	if (m_ppAudioValues) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			delete m_ppAudioValues[i];
		delete [] m_ppAudioValues;
		m_ppAudioValues = NULL;
	}

	m_iChannels = iChannels;
	if (m_iChannels > 0) {
		m_ppAudioValues = new qtractorAudioMeterValue *[m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; ++i) {
			m_ppAudioValues[i] = new qtractorAudioMeterValue(this, i);
			m_ppAudioValues[i]->show();
		}
	}

	panSlider()->setEnabled(m_iChannels > 1);
	panSpinBox()->setEnabled(m_iChannels > 1);
}


// Reset peak holder.
void qtractorAudioMeter::peakReset (void)
{
	for (unsigned short i = 0; i < m_iChannels; ++i)
		m_ppAudioValues[i]->peakReset();
}


#ifdef CONFIG_GRADIENT
// Gradient pixmap accessor.
const QPixmap& qtractorAudioMeter::pixmap (void) const
{
	return *m_pPixmap;
}

void qtractorAudioMeter::updatePixmap (void)
{
	int w = boxWidget()->width();
	int h = boxWidget()->height();

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


// Slot refreshment.
void qtractorAudioMeter::refresh (void)
{
	for (unsigned short i = 0; i < m_iChannels; ++i)
		m_ppAudioValues[i]->refresh();
}


// Resize event handler.
void qtractorAudioMeter::resizeEvent ( QResizeEvent * )
{
	m_fScale = 0.85f * float(boxWidget()->height());

	m_levels[Color0dB]  = iec_scale(  0.0f);
	m_levels[Color3dB]  = iec_scale( -3.0f);
	m_levels[Color6dB]  = iec_scale( -6.0f);
	m_levels[Color10dB] = iec_scale(-10.0f);

#ifdef CONFIG_GRADIENT
	updatePixmap();
#endif
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


// Pan-slider value change method.
void qtractorAudioMeter::updatePanning (void)
{
//	setPanning(m_pAudioMonitor->panning());

	panSlider()->setToolTip(
		tr("Pan: %1").arg(panning(), 0, 'g', 2));
}

// Gain-slider value change method.
void qtractorAudioMeter::updateGain (void)
{
//	setGain(m_pAudioMonitor->gain());

	gainSlider()->setToolTip(
		tr("Gain: %1 dB").arg(gainSpinBox()->value(), 0, 'g', 3));
}


// end of qtractorAudioMeter.cpp
