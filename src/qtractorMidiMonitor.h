// qtractorMidiMonitor.h
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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
	qtractorMidiMonitor(qtractorSession *pSession,
		float fGain = 1.0f, float fPanning = 0.0f,
		unsigned int iQueueSize = 16);
	// Destructor.
	~qtractorMidiMonitor();

	// Queue property accessors.
	void setQueueSize(unsigned int iQueueSize);
	unsigned int queueSize() const;

	// Monitor enqueue methods.
	void enqueue(qtractorMidiEvent::EventType type,
		unsigned char val, unsigned long tick = 0);

	// Monitor dequeue methods.
	float value();
	int   count();

	// Reset monitor.
	void reset();

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
	qtractorSession *m_pSession;
	unsigned int     m_iQueueSize;
	unsigned int     m_iQueueMask;
	QueueItem       *m_pQueue;
	unsigned int     m_iReadIndex;
	unsigned long    m_iTimeStart;
	unsigned long    m_iTimeSlot;
	QueueItem        m_item;
};


#endif  // __qtractorMidiMonitor_h

// end of qtractorMidiMonitor.h
