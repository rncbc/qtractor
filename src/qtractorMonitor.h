// qtractorMonitor.h
//
/****************************************************************************
   Copyright (C) 2006-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorObserver.h"


//----------------------------------------------------------------------------
// qtractorMonitor -- Monitor bridge value processor.

class qtractorMonitor
{
public:

	// Constructor.
	qtractorMonitor(float fGain = 1.0f, float fPanning = 0.0f)
		: m_gain(fGain), m_panning(fPanning) {}

	// Virtual destructor.
	virtual ~qtractorMonitor() {}

	// Gain accessors.
	float gain() const
		{ return m_gain.value(); }
	void setGain(float fGain)
		{ m_gain.setValue(fGain); update(); }
	qtractorSubject *gainSubject()
		{ return &m_gain; }

	// Stereo panning accessors.
	float panning() const
		{ return m_panning.value(); }
	void setPanning(float fPanning)
		{ m_panning.setValue(fPanning); update(); }
	qtractorSubject *panningSubject()
		{ return &m_panning; }

protected:

	// Rebuild the whole panning-gain array...
	virtual void update() = 0;

private:

	// Instance variables.
	qtractorSubject m_gain;
	qtractorSubject m_panning;
};


#endif  // __qtractorMonitor_h

// end of qtractorMonitor.h
