// qtractorMidiMeter.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiMeter_h
#define __qtractorMidiMeter_h

#include "qtractorMeter.h"


// Forward declarations.
class qtractorMidiMeter;
class qtractorMidiMonitor;

class QLabel;

class QResizeEvent;
class QPaintEvent;


//----------------------------------------------------------------------------
// qtractorMidiMeterScale -- MIDI meter bridge scale widget.

class qtractorMidiMeterScale : public qtractorMeterScale
{
public:

	// Constructor.
	qtractorMidiMeterScale(qtractorMidiMeter *pMidiMeter,
		QWidget *pParent = 0);

protected:

	// Actual scale drawing method.
	void paintScale(QPainter *p);
};


//----------------------------------------------------------------------------
// qtractorMidiMeterValue -- MIDI meter bridge value widget.

class qtractorMidiMeterValue : public qtractorMeterValue
{
public:

	// Constructor.
	qtractorMidiMeterValue(qtractorMidiMeter *pMidiMeter,
		QWidget *pParent = 0);

	// Reset peak holder.
	void peakReset() { m_iPeak = 0; }

	// Value refreshment.
	void refresh(unsigned long iStamp);

protected:

	// Specific event handlers.
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);

private:

	// Running variables.
	int   m_iValue;
	float m_fValueDecay;

	int   m_iPeak;
	int   m_iPeakHold;
	float m_fPeakDecay;
};


//----------------------------------------------------------------------------
// qtractorMidiMeterLed -- MIDI meter bridge LED widget.

class qtractorMidiMeterLed : public qtractorMeterValue
{
public:

	// Constructor.
	qtractorMidiMeterLed(qtractorMidiMeter *pMidiMeter,
		QWidget *pParent = 0);

	// Default destructor.
	~qtractorMidiMeterLed();

	// Reset peak holder.
	void peakReset() { m_iMidiCount = 0; }

	// Value refreshment.
	void refresh(unsigned long iStamp);

private:

	// Local instance variables.
	QLabel *m_pMidiLabel;

	// Running variables.
	unsigned int m_iMidiCount;

	// MIDI I/O LED pixmap stuff.
	enum { LedOff = 0, LedOn = 1, LedCount = 2 };

	static int      g_iLedRefCount;
	static QPixmap *g_pLedPixmap[LedCount];
};


//----------------------------------------------------------------------------
// qtractorMidiMeter -- MIDI meter bridge slot widget.

class qtractorMidiMeter : public qtractorMeter
{
public:

	// Constructor.
	qtractorMidiMeter(qtractorMidiMonitor *pMidiMonitor,
		QWidget *pParent = 0);
	// Default destructor.
	~qtractorMidiMeter();

	// Virtual monitor accessor.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	// MIDI monitor accessor.
	void setMidiMonitor(qtractorMidiMonitor *pMidiMonitor);
	qtractorMidiMonitor *midiMonitor() const;

	// Local slider update methods.
	void updatePanning();
	void updateGain();

	// Monitor reset.
	void reset();

	// Reset peak holder.
	void peakReset();

#ifdef CONFIG_GRADIENT
	const QPixmap& pixmap() const;
	void updatePixmap();
#endif

	// Color/level indexes.
	enum {
		ColorPeak	= 0,
		ColorOver	= 1,
		ColorBack	= 2,
		ColorFore	= 3,
		ColorCount	= 4
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
	qtractorMidiMonitor    *m_pMidiMonitor;

	qtractorMidiMeterLed   *m_pMidiLed;
	qtractorMidiMeterScale *m_pMidiScale;
	qtractorMidiMeterValue *m_pMidiValue;

#ifdef CONFIG_GRADIENT
	QPixmap *m_pPixmap;
#endif

	static QColor g_defaultColors[ColorCount];
	static QColor g_currentColors[ColorCount];
};

	
#endif  // __qtractorMidiMeter_h

// end of qtractorMidiMeter.h
