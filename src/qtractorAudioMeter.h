// qtractorAudioMeter.h
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

#ifndef __qtractorAudioMeter_h
#define __qtractorAudioMeter_h

#include "qtractorMeter.h"

// Color/level indexes.
#define QTRACTOR_AUDIO_METER_OVER	0
#define QTRACTOR_AUDIO_METER_0DB	1
#define QTRACTOR_AUDIO_METER_3DB	2
#define QTRACTOR_AUDIO_METER_6DB	3
#define QTRACTOR_AUDIO_METER_10DB	4
#define QTRACTOR_AUDIO_METER_BACK	5
#define QTRACTOR_AUDIO_METER_FORE	6

#define QTRACTOR_AUDIO_METER_LEVELS	5
#define QTRACTOR_AUDIO_METER_COLORS	7

// Forward declarations.
class qtractorAudioMeter;
class qtractorAudioMeterValue;
class qtractorAudioMonitor;


//----------------------------------------------------------------------------
// qtractorAudioMeterScale -- Audio meter bridge scale widget.

class qtractorAudioMeterScale : public qtractorMeterScale
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioMeterScale(qtractorAudioMeter *pAudioMeter,
		QWidget *pParent = 0, const char *pszName = 0);

protected:

	// Actual scale drawing method.
	void paintScale(QPainter *p);
};


//----------------------------------------------------------------------------
// qtractorAudioMeterValue -- Audio meter bridge value widget.

class qtractorAudioMeterValue : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioMeterValue(qtractorAudioMeter *pAudioMeter,
		unsigned short iChannel, QWidget *pParent = 0, const char *pszName = 0);
	// Default destructor.
	~qtractorAudioMeterValue();

	// Reset peak holder.
	void peakReset();

protected:

	// Specific event handlers.
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);

private:

	// Local instance variables.
	qtractorAudioMeter *m_pAudioMeter;
	unsigned short      m_iChannel;

	// Running variables.
	int   m_iValue;
	float m_fValueDecay;
	int   m_iPeak;
	int   m_iPeakHold;
	float m_fPeakDecay;
	int   m_iPeakColor;
};


//----------------------------------------------------------------------------
// qtractorAudioMeter -- Audio meter bridge slot widget.

class qtractorAudioMeter : public qtractorMeter
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioMeter(qtractorAudioMonitor *pAudioMonitor,
		QWidget *pParent = 0, const char *pszName = 0);
	// Default destructor.
	~qtractorAudioMeter();

	// Virtual monitor accessor.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	// Audio monitor accessor.
	void setAudioMonitor(qtractorAudioMonitor *pAudioMonitor);
	qtractorAudioMonitor *audioMonitor() const;

	// Local slider update methods.
	void updatePanning();
	void updateGain();

	// Monitor reset.
	void reset();

	// Slot refreshment.
	void refresh();

	// Reset peak holder.
	void peakReset();

	// IEC scale accessors.
	int iec_scale(float dB) const;
	int iec_level(int iIndex) const;

	// Common resource accessors.
	const QColor& color(int iIndex) const;

protected:

	// Gain-scale converters...
	float gainFromScale(float fScale) const;
	float scaleFromGain(float fGain)  const;

	// Specific event handlers.
	void resizeEvent(QResizeEvent *);

private:

	// Local instance variables.
	qtractorAudioMonitor     *m_pAudioMonitor;
	unsigned short            m_iChannels;
	qtractorAudioMeterScale  *m_pAudioScale;
	qtractorAudioMeterValue **m_ppAudioValues;

	float  m_fScale;

	int    m_levels[QTRACTOR_AUDIO_METER_LEVELS];
	QColor m_colors[QTRACTOR_AUDIO_METER_COLORS];
};

	
#endif  // __qtractorAudioMeter_h

// end of qtractorAudioMeter.h
