// qtractorMonitor.h
//
/****************************************************************************
   Copyright (C) 2006-2016, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMonitor_h
#define __qtractorMonitor_h

#include "qtractorMidiControlObserver.h"


//----------------------------------------------------------------------------
// qtractorMonitor -- Monitor bridge value processor.

class qtractorMonitor
{
public:

	// Constructor.
	qtractorMonitor(float fGain = 1.0f, float fPanning = 0.0f)
		: m_gainSubject(fGain, 1.0f), m_panningSubject(fPanning, 0.0f),
			m_gainObserver(this), m_panningObserver(this)
		{ m_panningSubject.setMinValue(-1.0f); }

	// Virtual destructor.
	virtual ~qtractorMonitor() {}

	// Gain accessors.
	qtractorSubject *gainSubject()
		{ return &m_gainSubject; }
	qtractorMidiControlObserver *gainObserver()
		{ return static_cast<qtractorMidiControlObserver *> (&m_gainObserver); }

	void setGain(float fGain)
		{ m_gainSubject.setValue(fGain); update(); }
	float gain() const
		{ return m_gainSubject.value(); }
	float prevGain() const
		{ return m_gainSubject.prevValue(); }

	// Stereo panning accessors.
	qtractorSubject *panningSubject()
		{ return &m_panningSubject; }
	qtractorMidiControlObserver *panningObserver()
		{ return static_cast<qtractorMidiControlObserver *> (&m_panningObserver); }

	void setPanning(float fPanning)
		{ m_panningSubject.setValue(fPanning); update(); }
	float panning() const
		{ return m_panningSubject.value(); }
	float prevPanning() const
		{ return m_panningSubject.prevValue(); }

	// Rebuild the whole panning-gain array...
	virtual void update() = 0;

private:

	// Observer -- Local dedicated observers.
	class Observer : public qtractorMidiControlObserver
	{
	public:

		// Constructor.
		Observer(qtractorMonitor *pMonitor, qtractorSubject *pSubject)
			: qtractorMidiControlObserver(pSubject), m_pMonitor(pMonitor) {}

	protected:

		// Update feedback.
		void update(bool bUpdate)
		{
			m_pMonitor->update();
			qtractorMidiControlObserver::update(bUpdate);
		}

	private:

		// Members.
		qtractorMonitor *m_pMonitor;
	};

	class GainObserver : public Observer
	{
	public:
		// Constructor.
		GainObserver(qtractorMonitor *pMonitor)
			: Observer(pMonitor, pMonitor->gainSubject()) {}
	};

	class PanningObserver : public Observer
	{
	public:
		// Constructor.
		PanningObserver(qtractorMonitor *pMonitor)
			: Observer(pMonitor, pMonitor->panningSubject()) {}
	};

	// Instance variables.
	qtractorSubject m_gainSubject;
	qtractorSubject m_panningSubject;

	GainObserver    m_gainObserver;
	PanningObserver m_panningObserver;
};


#endif  // __qtractorMonitor_h

// end of qtractorMonitor.h
