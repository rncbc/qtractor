// qtractorMidiMeter.h
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

#ifndef __qtractorMidiMeter_h
#define __qtractorMidiMeter_h

#include "qtractorMeter.h"


// Forward declarations.
class qtractorMidiMeter;
class qtractorMidiMonitor;

class qtractorAudioMeter;
class qtractorAudioOutputMonitor;

class QLabel;

class QResizeEvent;
class QPaintEvent;


//----------------------------------------------------------------------------
// qtractorMidiMeterScale -- MIDI meter bridge scale widget.

class qtractorMidiMeterScale : public qtractorMeterScale
{
public:

	// Constructor.
	qtractorMidiMeterScale(qtractorMidiMeter *pMidiMeter);

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
	qtractorMidiMeterValue(qtractorMidiMeter *pMidiMeter);

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
	qtractorMidiMeterLed(qtractorMidiMeter *pMidiMeter);

	// Default destructor.
	~qtractorMidiMeterLed();

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
	qtractorMidiMeter(
		qtractorMidiMonitor *pMidiMonitor, QWidget *pParent = 0);

	// Default destructor.
	~qtractorMidiMeter();

	// Virtual monitor accessor.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	// MIDI monitor accessor.
	void setMidiMonitor(qtractorMidiMonitor *pMidiMonitor);
	qtractorMidiMonitor *midiMonitor() const;

	// Monitor reset.
	void reset();

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

	// Local instance variables.
	qtractorMidiMonitor    *m_pMidiMonitor;
	qtractorMidiMeterValue *m_pMidiValue;

#ifdef CONFIG_GRADIENT
	QPixmap *m_pPixmap;
#endif

	static QColor g_defaultColors[ColorCount];
	static QColor g_currentColors[ColorCount];
};


//----------------------------------------------------------------------------
// qtractorMidiComboMeter -- MIDI meter combo widget.

class qtractorMidiComboMeter : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiComboMeter(
		qtractorMidiMonitor *pMidiMonitor, QWidget *pParent = 0);

	// Default destructor.
	~qtractorMidiComboMeter();

	// Regular MIDI meter accessor.
	qtractorMidiMeter *midiMeter() const;

	// Combined audio-output meter accessor.
	qtractorAudioMeter *audioMeter() const;

	// Virtual monitor accessor.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	// MIDI monitor accessor.
	void setMidiMonitor(qtractorMidiMonitor *pMidiMonitor);
	qtractorMidiMonitor *midiMonitor() const;

	// Audio-output monitor accessor.
	void setAudioOutputMonitor(qtractorAudioOutputMonitor *pAudioOutputMonitor);
	qtractorAudioOutputMonitor *audioOutputMonitor() const;

	// Monitor reset.
	void reset();

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *);

private:

	// Local instance variables.
	qtractorMidiMeter  *m_pMidiMeter;
	qtractorAudioMeter *m_pAudioMeter;
};


//----------------------------------------------------------------------------
// qtractorMidiMixerMeter -- MIDI mixer-strip meter bridge widget.

class qtractorMidiMixerMeter : public qtractorMixerMeter
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiMixerMeter(
		qtractorMidiMonitor *pMidiMonitor, QWidget *pParent = 0);

	// Default destructor.
	~qtractorMidiMixerMeter();

	// Virtual monitor accessor.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	// MIDI monitor accessor.
	void setMidiMonitor(qtractorMidiMonitor *pMidiMonitor);
	qtractorMidiMonitor *midiMonitor() const;

	// Audio-output monitor accessor.
	void setAudioOutputMonitor(qtractorAudioOutputMonitor *pAudioOutputMonitor);
	qtractorAudioOutputMonitor *audioOutputMonitor() const;

	// Local slider update methods.
	void updatePanning();
	void updateGain();

	// Monitor reset.
	void reset();

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *);

private:

	// Local forward declarations.
	class GainSliderInterface;
	class GainSpinBoxInterface;

	// Local instance variables.
	qtractorMidiComboMeter *m_pMidiMeter;
	qtractorMidiMeterScale *m_pMidiScale;
	qtractorMidiMeterLed   *m_pMidiLed;
};


#endif  // __qtractorMidiMeter_h

// end of qtractorMidiMeter.h
