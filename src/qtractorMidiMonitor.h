// qtractorMidiMonitor.h
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiMonitor_h
#define __qtractorMidiMonitor_h

#include "qtractorMonitor.h"
#include "qtractorMidiEvent.h"

// Forwrad decalarations.
class qtractorSession;


//----------------------------------------------------------------------------
// qtractorMidiMonitor -- MIDI monitor bridge value processor.

class qtractorMidiMonitor : public qtractorMonitor
{
public:

	// Constructor.
	qtractorMidiMonitor(float fGain = 1.0f, float fPanning = 0.0f);
	// Destructor.
	~qtractorMidiMonitor();

	// Monitor enqueue methods.
	void enqueue(qtractorMidiEvent::EventType type,
		unsigned char val, unsigned long tick = 0);

	// Monitor dequeue methods.
	float value();
	int   count();

	// Reset monitor.
	void reset();

	// Singleton sync reset.
	static void syncReset(qtractorSession *pSession);

protected:

	// Update monitor (nothing really done here).
	void update();

private:

	// Queue iten struct.
	struct QueueItem
	{
		unsigned char value;
		unsigned char count;
	};

	// Instance variables.
	QueueItem    *m_pQueue;
	unsigned int  m_iQueueIndex;
	unsigned long m_iFrameStart;
	unsigned long m_iTimeStart;
	QueueItem     m_item;

	// Singleton variables.
	static unsigned long s_iFrameSlot;
	static unsigned long s_iTimeSlot;
};


#endif  // __qtractorMidiMonitor_h

// end of qtractorMidiMonitor.h
