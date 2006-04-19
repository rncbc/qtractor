// qtractorMeter.cpp
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

#include "qtractorMeter.h"
#include "qtractorMonitor.h"
#include "qtractorSlider.h"

#include <qtooltip.h>
#include <qpainter.h>
#include <qpixmap.h>

#include <math.h>

// Meter level limits (in dB).
#define QTRACTORMETER_MAXDB			(+3.0f)
#define QTRACTORMETER_MINDB			(-70.0f)

// The peak decay rate (magic goes here :).
#define QTRACTORMETER_DECAY_RATE	(1.0f - 3E-2f)
// Number of cycles the peak stays on hold (falloff).
#define QTRACTORMETER_DECAY_HOLD	16

#if defined(__BORLANDC__)
static inline float log10f ( float x )  { return float(::log(x)) / M_LN10; }
static inline float powf ( float x, float y ) { return float(::pow(x, y)); }
#endif


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
// qtractorMeterScale -- Meter bridge scale widget.

// Constructor.
qtractorMeterScale::qtractorMeterScale ( qtractorMeter *pMeter )
	: QWidget(pMeter)
{
	m_pMeter = pMeter;
	m_iLastY = 0;

	QWidget::setMinimumWidth(14);
//	QWidget::setBackgroundMode(Qt::PaletteMid);
}

// Default destructor.
qtractorMeterScale::~qtractorMeterScale (void)
{
}


// Draw IEC scale line and label; assumes labels drawed from top to bottom.
void qtractorMeterScale::drawLineLabel ( QPainter *p, int y, const char* pszLabel )
{
	const QFontMetrics& fm = p->fontMetrics();

	int iMidHeight = (fm.height() >> 1);
	if (y < iMidHeight)
		return;

	int iCurrY = QWidget::height() - y;
	int iWidth = QWidget::width();

	if (fm.width(pszLabel) < iWidth - 5) {
		p->drawLine(0, iCurrY, 2, iCurrY);
		p->drawLine(iWidth - 3, iCurrY, iWidth - 1, iCurrY);
	}

	if (iCurrY > m_iLastY + iMidHeight) {
		p->drawText(2, iCurrY - iMidHeight, iWidth - 3, fm.height(),
			Qt::AlignHCenter | Qt::AlignVCenter, pszLabel);
		m_iLastY = iCurrY + 1;
	}
}


// Paint event handler.
void qtractorMeterScale::paintEvent ( QPaintEvent * )
{
	QPainter p(this);

	p.setFont(QFont("Helvetica", 5));
	p.setPen(m_pMeter->color(QTRACTORMETER_FORE));

	m_iLastY = 0;

	drawLineLabel(&p, m_pMeter->iec_level(QTRACTORMETER_0DB),  "0");
	drawLineLabel(&p, m_pMeter->iec_level(QTRACTORMETER_3DB),  "3");
	drawLineLabel(&p, m_pMeter->iec_level(QTRACTORMETER_6DB),  "6");
	drawLineLabel(&p, m_pMeter->iec_level(QTRACTORMETER_10DB), "10");

	for (float dB = -20.0f; dB > QTRACTORMETER_MINDB; dB -= 10.0f)
		drawLineLabel(&p, m_pMeter->iec_scale(dB), QString::number(-int(dB)));
}


// Resize event handler.
void qtractorMeterScale::resizeEvent ( QResizeEvent * )
{
	QWidget::repaint(true);
}


//----------------------------------------------------------------------------
// qtractorMeterValue -- Meter bridge value widget.

// Constructor.
qtractorMeterValue::qtractorMeterValue( qtractorMeter *pMeter,
	unsigned short iChannel ) : QFrame(pMeter)
{
	m_pMeter      = pMeter;
	m_iChannel    = iChannel;

	m_iValue      = 0;
	m_fValueDecay = QTRACTORMETER_DECAY_RATE;
	m_iPeak       = 0;
	m_iPeakHold   = 0;
	m_fPeakDecay  = QTRACTORMETER_DECAY_RATE;
	m_iPeakColor  = QTRACTORMETER_6DB;

	QFrame::setMinimumWidth(8);
	QFrame::setBackgroundMode(Qt::NoBackground);

	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Sunken);
}

// Default destructor.
qtractorMeterValue::~qtractorMeterValue (void)
{
}


// Reset peak holder.
void qtractorMeterValue::peakReset (void)
{
	m_iPeak = 0;
}


// Paint event handler.
void qtractorMeterValue::paintEvent ( QPaintEvent * )
{
	int iWidth  = QWidget::width();
	int iHeight = QWidget::height();
	int y;

	QPixmap pm(iWidth, iHeight);
	QPainter p(&pm);

	p.setViewport(0, 0, iWidth, iHeight);
	p.setWindow(0, 0, iWidth, iHeight);

	if (isEnabled()) {
		pm.fill(m_pMeter->color(QTRACTORMETER_BACK));
		y = m_pMeter->iec_level(QTRACTORMETER_0DB);
		p.setPen(m_pMeter->color(QTRACTORMETER_FORE));
		p.drawLine(0, iHeight - y, iWidth, iHeight - y);
	} else {
		pm.fill(Qt::gray);
	}

	float dB = QTRACTORMETER_MINDB;
	float fValue = m_pMeter->monitor()->value(m_iChannel);
	if (fValue > 0.0f)
		dB = 20.0f * ::log10f(fValue);

	if (dB < QTRACTORMETER_MINDB)
		dB = QTRACTORMETER_MINDB;
	else if (dB > QTRACTORMETER_MAXDB)
		dB = QTRACTORMETER_MAXDB;

	int y_over = 0;
	int y_curr = 0;

	y = m_pMeter->iec_scale(dB);
	if (y > m_iValue) {
		m_iValue = y;
		m_fValueDecay = QTRACTORMETER_DECAY_RATE;
	} else {
		m_iValue = (int) ((float) m_iValue * m_fValueDecay);
		if (y > m_iValue) {
			m_iValue = y;
		} else {
			m_fValueDecay *= m_fValueDecay;
			y = m_iValue;
		}
	}

	int iLevel;
	for (iLevel = QTRACTORMETER_10DB;
			iLevel > QTRACTORMETER_OVER && y >= y_over; iLevel--) {
		y_curr = m_pMeter->iec_level(iLevel);
		if (y < y_curr) {
			p.fillRect(0, iHeight - y, iWidth, y - y_over,
				m_pMeter->color(iLevel));
		} else {
			p.fillRect(0, iHeight - y_curr, iWidth, y_curr - y_over,
				m_pMeter->color(iLevel));
		}
		y_over = y_curr;
	}

	if (y > y_over) {
		p.fillRect(0, iHeight - y, iWidth, y - y_over,
			m_pMeter->color(QTRACTORMETER_OVER));
	}

	if (y > m_iPeak) {
		m_iPeak = y;
		m_iPeakHold  = 0;
		m_fPeakDecay = QTRACTORMETER_DECAY_RATE;
		m_iPeakColor = iLevel;
	} else if (++m_iPeakHold > QTRACTORMETER_DECAY_HOLD) {
		m_iPeak = (int) ((float) m_iPeak * m_fPeakDecay);
		if (y > m_iPeak) {
			m_iPeak = y;
		} else {
			if (m_iPeak < m_pMeter->iec_level(QTRACTORMETER_10DB))
				m_iPeakColor = QTRACTORMETER_6DB;
			m_fPeakDecay *= m_fPeakDecay;
		}
	}

	p.setPen(m_pMeter->color(m_iPeakColor));
	p.drawLine(0, iHeight - m_iPeak, iWidth, iHeight - m_iPeak);

	bitBlt(this, 0, 0, &pm);
}


// Resize event handler.
void qtractorMeterValue::resizeEvent ( QResizeEvent * )
{
	m_iPeak = 0;

	QWidget::repaint(true);
}


//----------------------------------------------------------------------------
// qtractorMeter -- Meter bridge slot widget.

// Constructor.
qtractorMeter::qtractorMeter ( qtractorMonitor *pMonitor,
	QWidget *pParent, const char *pszName )
	: QHBox(pParent, pszName)
{
	m_pMonitor     = pMonitor;
	m_pSlider      = new qtractorSlider(this);
	m_pScale       = new qtractorMeterScale(this);
	m_ppValues     = NULL;
	m_fScale       = 0.0f;
	m_iPeakFalloff = QTRACTORMETER_DECAY_HOLD;

	m_pSlider->setTickmarks(QSlider::NoMarks);
	m_pSlider->setMinValue(-int(10000.0f * 0.025f * QTRACTORMETER_MAXDB));
	m_pSlider->setMaxValue(10000);
	m_pSlider->setPageStep(1000);
	m_pSlider->setLineStep(100);
	m_pSlider->setDefaultValue(0);
	
	setGain(m_pMonitor->gain());

	for (int i = 0; i < QTRACTORMETER_LEVELS; i++)
		m_iLevels[i] = 0;

	m_pColors[QTRACTORMETER_OVER] = new QColor(240,  0, 20);
	m_pColors[QTRACTORMETER_0DB]  = new QColor(240,160, 20);
	m_pColors[QTRACTORMETER_3DB]  = new QColor(220,220, 20);
	m_pColors[QTRACTORMETER_6DB]  = new QColor(160,220, 20);
	m_pColors[QTRACTORMETER_10DB] = new QColor( 40,160, 40);
	m_pColors[QTRACTORMETER_BACK] = new QColor( 20, 40, 20);
	m_pColors[QTRACTORMETER_FORE] = new QColor( 80, 80, 80);

	unsigned short iChannels = m_pMonitor->channels();
	if (iChannels > 0) {
		m_ppValues = new qtractorMeterValue *[iChannels];
		for (unsigned short i = 0; i < iChannels; i++)
			m_ppValues[i] = new qtractorMeterValue(this, i);
	}

	QHBox::setMinimumHeight(120);
	QHBox::setSpacing(1);
	QHBox::setSizePolicy(
		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));

//	QHBox::setFrameShape(QFrame::StyledPanel);
//	QHBox::setFrameShadow(QFrame::Sunken);

	QObject::connect(m_pSlider, SIGNAL(valueChanged(int)),
		this, SLOT(valueChangedSlot(int)));

	valueChangedSlot(m_pSlider->value());
}


// Default destructor.
qtractorMeter::~qtractorMeter (void)
{
	for (unsigned short i = 0; i < m_pMonitor->channels(); i++)
		delete m_ppValues[i];

	delete [] m_ppValues;
	delete m_pScale;
	delete m_pSlider;

	for (int i = 0; i < QTRACTORMETER_COLORS; i++)
		delete m_pColors[i];
}


// IEC standard 
int qtractorMeter::iec_scale ( float dB ) const
{
	return int(m_fScale * IEC_Scale(dB));
}


int qtractorMeter::iec_level ( int iIndex ) const
{
	return m_iLevels[iIndex];
}


qtractorMonitor *qtractorMeter::monitor (void) const
{
	return m_pMonitor;
}


// Gain accessors.
void qtractorMeter::setGain_dB ( float dB )
{
	m_pSlider->setValue(10000 - int(10000.0f * IEC_Scale(dB)));
}

float qtractorMeter::gain_dB (void) const
{
	return IEC_dB(float(10000 - m_pSlider->value()) / 10000.0f);
}

void qtractorMeter::setGain ( float fGain )
{
	float dB = 0.0f;
	if (fGain > 0.0f)
		dB = 20.0f * ::log10f(fGain);
	setGain_dB(dB);
}

float qtractorMeter::gain (void) const
{
	return ::powf(10.0f, gain_dB() / 20.0f);
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


// Reset peak holder.
void qtractorMeter::peakReset (void)
{
	for (unsigned short i = 0; i < m_pMonitor->channels(); i++)
		m_ppValues[i]->peakReset();
}


// Slot refreshment.
void qtractorMeter::refresh (void)
{
	for (unsigned short i = 0; i < m_pMonitor->channels(); i++)
		m_ppValues[i]->update();
}


// Resize event handler.
void qtractorMeter::resizeEvent ( QResizeEvent * )
{
	m_fScale = (1.0f - 0.025f * QTRACTORMETER_MAXDB) * float(QWidget::height());

	m_iLevels[QTRACTORMETER_0DB]  = iec_scale(  0.0f);
	m_iLevels[QTRACTORMETER_3DB]  = iec_scale( -3.0f);
	m_iLevels[QTRACTORMETER_6DB]  = iec_scale( -6.0f);
	m_iLevels[QTRACTORMETER_10DB] = iec_scale(-10.0f);
}


// Common resource accessor.
const QColor& qtractorMeter::color ( int iIndex ) const
{
	return *m_pColors[iIndex];
}


// Slider value change slot.
void qtractorMeter::valueChangedSlot ( int /*iValue*/ )
{
	m_pMonitor->setGain(gain());

	QToolTip::remove(this);
	QToolTip::add(this, tr("%1 dB").arg(gain_dB(), 0, 'g', 3));
}


// end of qtractorMeter.cpp
