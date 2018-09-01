// qtractorMeter.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMeter_h
#define __qtractorMeter_h

#include <QFrame>


// Forward declarations.
class qtractorMeter;
class qtractorMonitor;

class qtractorSubject;

class qtractorObserverSlider;
class qtractorObserverSpinBox;

class qtractorMidiControlObserver;

class qtractorDocument;

class QDomElement;

class QHBoxLayout;
class QVBoxLayout;

class QPaintEvent;
class QResizeEvent;


//----------------------------------------------------------------------------
// qtractorMeterScale -- Meter bridge scale widget.

class qtractorMeterScale : public QFrame
{
public:

	// Constructor.
	qtractorMeterScale(qtractorMeter *pMeter);

	// Meter accessor.
	qtractorMeter *meter() const;

protected:
	
	// Specific event handlers.
	void paintEvent(QPaintEvent *);

	// Draw IEC scale line and label.
	void drawLineLabel(QPainter *p, int y, const QString& sLabel);

	// Actual scale drawing method.
	virtual void paintScale(QPainter *p) = 0;

private:

	// Local instance variables.
	qtractorMeter *m_pMeter;

	// Running variables.
	int m_iLastY;
};


//----------------------------------------------------------------------------
// qtractorMeterValue -- Meter bridge value widget.

class qtractorMeterValue : public QWidget
{
public:

	// Constructor.
	qtractorMeterValue(qtractorMeter *pMeter);

	// Default destructor.
	virtual ~qtractorMeterValue();

	// Meter bridge accessor.
	qtractorMeter *meter() const
		{ return m_pMeter; }

	// Value refreshment.
	virtual void refresh(unsigned long iStamp) = 0;

	// Global refreshment.
	static void refreshAll();

private:

	// Local instance variables.
	qtractorMeter *m_pMeter;

	// List of meter-values (global obviously)
	static QList<qtractorMeterValue *> g_values;
	static unsigned long g_iStamp;
};


//----------------------------------------------------------------------------
// qtractorMeter -- Meter bridge slot widget.

class qtractorMeter : public QWidget
{
public:

	// Constructor.
	qtractorMeter(QWidget *pParent = 0);

	// Default destructor.
	virtual ~qtractorMeter();

	// Dynamic layout accessors.
	QHBoxLayout *boxLayout() const
		{ return m_pBoxLayout; }

	// Monitor accessors.
	virtual void setMonitor(qtractorMonitor *pMonitor) = 0;
	virtual qtractorMonitor *monitor() const = 0;

	// Meter reset.
	virtual void reset() = 0;

	// For faster scaling when drawing...
	int scale(float fValue) const
		{ return int(m_fScale * fValue); }

	// Peak falloff mode setting.
	void setPeakFalloff(int iPeakFalloff)
		{ m_iPeakFalloff = iPeakFalloff; }
	int peakFalloff() const
		{ return m_iPeakFalloff; }

protected:

	// Scale accessor.
	void setScale(float fScale)
		{ m_fScale = fScale; }

private:

	// Local instance variables.
	QHBoxLayout *m_pBoxLayout;

	// Meter width/height scale.
	float m_fScale;

	// Peak falloff mode setting (0=no peak falloff).
	int m_iPeakFalloff;
};


//----------------------------------------------------------------------------
// qtractorMixerMeter -- Mixer-strip meter bridge widget.

class qtractorMixerMeter : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMixerMeter(QWidget *pParent = 0);

	// Default destructor.
	virtual ~qtractorMixerMeter();

	// Dynamic layout accessors.
	QWidget *topWidget() const
		{ return m_pTopWidget; }
	QHBoxLayout *topLayout() const
		{ return m_pTopLayout; }
	QHBoxLayout *boxLayout() const
		{ return m_pBoxLayout; }

	// Common slider/spin-box accessors.
	qtractorObserverSlider *panSlider() const
		{ return m_pPanSlider; }
	qtractorObserverSpinBox *panSpinBox() const
		{ return m_pPanSpinBox; }

	qtractorObserverSlider *gainSlider() const
		{ return m_pGainSlider; }
	qtractorObserverSpinBox *gainSpinBox() const
		{ return m_pGainSpinBox; }

	// Panning subject accessors.
	void setPanningSubject(qtractorSubject *pSubject);
	qtractorSubject *panningSubject() const;

	// Panning accessors.
	void setPanning(float fPanning);
	float panning() const;
	float prevPanning() const;

	// Gain subject accessors.
	void setGainSubject(qtractorSubject *pSubject);
	qtractorSubject *gainSubject() const;

	// Gain accessors.
	void setGain(float fGain);
	float gain() const;
	float prevGain() const;

	// Monitor accessors.
	virtual void setMonitor(qtractorMonitor *pMonitor) = 0;
	virtual qtractorMonitor *monitor() const = 0;

	// Local slider update methods.
	virtual void updatePanning() = 0;
	virtual void updateGain() = 0;

	// Meter reset.
	virtual void reset() = 0;

	// MIDI controller/observer attachment (context menu) activator.
	void addMidiControlAction(
		QWidget *pWidget, qtractorMidiControlObserver *pObserver);

protected slots:

	// MIDI controller/observer attachment (context menu) slot.
	void midiControlActionSlot();
	void midiControlMenuSlot(const QPoint& pos);

private:

	// Local forward declarations.
	class PanSliderInterface;

	// Local instance variables.
	QWidget     *m_pTopWidget;
	QHBoxLayout *m_pTopLayout;
	QHBoxLayout *m_pBoxLayout;

	qtractorObserverSlider  *m_pPanSlider;
	qtractorObserverSpinBox *m_pPanSpinBox;
	qtractorObserverSlider  *m_pGainSlider;
	qtractorObserverSpinBox *m_pGainSpinBox;

	class PanObserver;
	class GainObserver;

	PanObserver  *m_pPanObserver;
	GainObserver *m_pGainObserver;
};


#endif  // __qtractorMeter_h

// end of qtractorMeter.h
