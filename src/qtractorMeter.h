// qtractorMeter.h
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

#ifndef __QTRACTOR_METER_h
#define __QTRACTOR_METER_h

#include <qptrlist.h>
#include <qhbox.h>

// Color/level indexes.
#define QTRACTOR_METER_OVER		0
#define QTRACTOR_METER_0DB		1
#define QTRACTOR_METER_3DB		2
#define QTRACTOR_METER_6DB		3
#define QTRACTOR_METER_10DB		4
#define QTRACTOR_METER_BACK		5
#define QTRACTOR_METER_FORE		6

#define QTRACTOR_METER_LEVELS	5
#define QTRACTOR_METER_COLORS	7

// Forward declarations.
class qtractorMonitor;
class qtractorMeter;
class qtractorSlider;


//----------------------------------------------------------------------------
// qtractorMeterScale -- Meter bridge scale widget.

class qtractorMeterScale : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMeterScale(qtractorMeter *pMeter);
	// Default destructor.
	~qtractorMeterScale();

protected:

	// Specific event handlers.
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);

private:

	// Draw IEC scale line and label.
	void drawLineLabel(QPainter *p, int y, const char* pszLabel);

	// Local instance variables.
	qtractorMeter *m_pMeter;

	// Running variables.
	float m_fScale;
	int   m_iLastY;
};


//----------------------------------------------------------------------------
// qtractorMeterValue -- Meter bridge value widget.

class qtractorMeterValue : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMeterValue(qtractorMeter *pMeter, unsigned short iChannel);
	// Default destructor.
	~qtractorMeterValue();

	// Reset peak holder.
	void peakReset();

protected:

	// Specific event handlers.
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);

private:

	// Local instance variables.
	qtractorMeter *m_pMeter;
	unsigned short m_iChannel;

	// Running variables.
	int   m_iValue;
	float m_fValueDecay;
	int   m_iPeak;
	int   m_iPeakHold;
	float m_fPeakDecay;
	int   m_iPeakColor;
};


//----------------------------------------------------------------------------
// qtractorMeter -- Meter bridge slot widget.

class qtractorMeter : public QHBox
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMeter(qtractorMonitor *pMonitor,
		QWidget *pParent = 0, const char *pszName = 0);
	// Default destructor.
	~qtractorMeter();

	// Monitor accessor.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	// Monitor reset.
	void reset();

	// Gain accessors.
	void setGain_dB(float dB);
	float gain_dB() const;
	void setGain(float fGain);
	float gain() const;

	// IEC scale accessors.
	int iec_scale(float dB) const;
	int iec_level(int iIndex) const;

	// Slot refreshment.
	void refresh();

	// Common resource accessors.
	const QColor& color(int iIndex) const;

	// Peak falloff mode setting.
	void setPeakFalloff(int bPeakFalloff);
	int peakFalloff() const;

	// Reset peak holder.
	void peakReset();

protected:

	// Specific event handlers.
	void resizeEvent(QResizeEvent *);

protected slots:

	// Slider value change slot.
	void valueChangedSlot(int iValue);

private:

	// Local instance variables.
	unsigned short       m_iChannels;
	qtractorMonitor     *m_pMonitor;
	qtractorSlider      *m_pSlider;
	qtractorMeterScale  *m_pScale;
	qtractorMeterValue **m_ppValues;
	int                  m_iLevels[QTRACTOR_METER_LEVELS];
	QColor              *m_pColors[QTRACTOR_METER_COLORS];
	float                m_fScale;

	// Peak falloff mode setting (0=no peak falloff).
	int m_iPeakFalloff;
};

	
#endif  // __QTRACTOR_METER_h

// end of qtractorMeter.h
