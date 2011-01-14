// qtractorAudioMeter.h
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

#ifndef __qtractorAudioMeter_h
#define __qtractorAudioMeter_h

#include "qtractorMeter.h"


// Forward declarations.
class qtractorAudioMeter;
class qtractorAudioMeterValue;
class qtractorAudioMonitor;

class QResizeEvent;
class QPaintEvent;


//----------------------------------------------------------------------------
// qtractorAudioMeterScale -- Audio meter bridge scale widget.

class qtractorAudioMeterScale : public qtractorMeterScale
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioMeterScale(qtractorAudioMeter *pAudioMeter,
		QWidget *pParent = 0);

protected:

	// Actual scale drawing method.
	void paintScale(QPainter *p);
};


//----------------------------------------------------------------------------
// qtractorAudioMeterValue -- Audio meter bridge value widget.

class qtractorAudioMeterValue : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioMeterValue(qtractorAudioMeter *pAudioMeter,
		unsigned short iChannel, QWidget *pParent = 0);
	// Default destructor.
	~qtractorAudioMeterValue();

	// Reset peak holder.
	void peakReset();

	// Value refreshment.
	void refresh();

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
		QWidget *pParent = 0);
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

#ifdef CONFIG_GRADIENT
	const QPixmap& pixmap() const;
	void updatePixmap();
#endif

	// Color/level indexes.
	enum {
		ColorOver	= 0,
		Color0dB	= 1,
		Color3dB	= 2,
		Color6dB	= 3,
		Color10dB	= 4,
		LevelCount	= 5,
		ColorBack	= 5,
		ColorFore	= 6,
		ColorCount	= 7
	};

	// Common resource accessors.
	static void setColor(int iIndex, const QColor& color);
	static const QColor& color(int iIndex);
	static const QColor& defaultColor(int iIndex);

protected:

	// Specific event handlers.
	void resizeEvent(QResizeEvent *);

private:

	// Local forward declarations.
	class GainSliderInterface;
	class GainSpinBoxInterface;

	// Local instance variables.
	qtractorAudioMonitor     *m_pAudioMonitor;
	unsigned short            m_iChannels;
	qtractorAudioMeterScale  *m_pAudioScale;
	qtractorAudioMeterValue **m_ppAudioValues;

	float  m_fScale;
	int    m_levels[LevelCount];

#ifdef CONFIG_GRADIENT
	QPixmap *m_pPixmap;
#endif

	static QColor g_defaultColors[ColorCount];
	static QColor g_currentColors[ColorCount];
};

	
#endif  // __qtractorAudioMeter_h

// end of qtractorAudioMeter.h
