// qtractorMidiMonitor.cpp
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

#include "qtractorMidiMonitor.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"


//----------------------------------------------------------------------------
// qtractorMidiMonitor -- MIDI monitor bridge value processor.

// Constructor.
qtractorMidiMonitor::qtractorMidiMonitor ( qtractorSession *pSession,
	float fGain, float fPanning, unsigned int iQueueSize )
	: qtractorMonitor(fGain, fPanning),	m_pSession(pSession), m_pQueue(0)
{
	setQueueSize(iQueueSize);
}

// Destructor.
qtractorMidiMonitor::~qtractorMidiMonitor (void)
{
	setQueueSize(0);
}


// Queue property accessors.
void qtractorMidiMonitor::setQueueSize ( unsigned int iQueueSize)
{
	// Delete old buffer...
	if (m_pQueue) {
		delete [] m_pQueue;
		m_pQueue = 0;
	}

	// Set new buffer holders...
	m_iQueueMask = 0;
	m_iQueueSize = iQueueSize;
	if (m_iQueueSize > 0) {
		// Queue size range.
		const unsigned int iMinQueueSize = 8;
		const unsigned int iMaxQueueSize = 256;
		// Adjust size to nearest power-of-two, if necessary.
		if (iQueueSize < iMaxQueueSize) {
			m_iQueueSize = iMinQueueSize;
			while (m_iQueueSize < iQueueSize)
				m_iQueueSize <<= 1;
		} else {
			m_iQueueSize = iMaxQueueSize;
		}
		// The size overflow convenience mask.
		m_iQueueMask = (m_iQueueSize - 1);
		// Allocate actual buffer stuff...
		m_pQueue = new QueueItem [m_iQueueSize];
	}

	// May reset now...
	reset();
}

unsigned int qtractorMidiMonitor::queueSize() const
{
	return m_iQueueSize;
}


// Monitor enqueue method.
void qtractorMidiMonitor::enqueue ( qtractorMidiEvent::EventType type,
	unsigned char val, unsigned long tick )
{
	// Check whether this is a scheduled value...
	if (m_iTimeStart < tick && m_iTimeSlot > 0) {
		// Find queue offset index...
		unsigned int iOffset = (tick - m_iTimeStart) / m_iTimeSlot;
		// FIXME: Ignore outsiders (which would manifest as
		// out-of-time phantom monitor peak values...)
		if (iOffset > m_iQueueMask)
			iOffset = m_iQueueMask;
		unsigned int iIndex  = (m_iQueueIndex + iOffset) & m_iQueueMask;
		// Set the value in buffer...
		if (type == qtractorMidiEvent::NOTEON) {
			if (m_pQueue[iIndex].value < val)
				m_pQueue[iIndex].value = val;
		}
		// Increment enqueued count.
		m_pQueue[iIndex].count++;
		// Done with enqueueing.
	} else {
		// Alternative is sending it directly
		// as a non-enqueued direct value...
		if (type == qtractorMidiEvent::NOTEON) {
			if (m_item.value < val)
				m_item.value = val;
		}
		// Increment direct count.
		m_item.count++;
		// Done direct.
	}
}


// Monitor value dequeue method.
float qtractorMidiMonitor::value (void)
{
	// Grab-and-reset current direct value...
	unsigned char val = m_item.value;
	m_item.value = 0;
	// Sweep the queue until current time...
	if (m_iTimeSlot > 0) {
		unsigned long iFrameTime = m_pSession->tickFromFrame(
			m_pSession->audioEngine()->sessionCursor()->frameTime());
		while (iFrameTime > m_iTimeStart) {
			QueueItem& item = m_pQueue[m_iQueueIndex];
			if (val < item.value)
				val = item.value;
			m_item.count += item.count;
			item.value = 0;
			item.count = 0;
			++m_iQueueIndex &= m_iQueueMask;
			m_iTimeStart += m_iTimeSlot;
		}
	}
	// Dequeue done.
	return (gain() * val) / 127.0f;
}


// Monitor count dequeue method.
int qtractorMidiMonitor::count (void)
{
	// Grab latest direct/dequeued count...
	int iCount = int(m_item.count);
	m_item.count = 0;
	return iCount;
}


// Reset monitor.
void qtractorMidiMonitor::reset (void)
{
	// (Re)initialize all...
	m_item.value  = 0;
	m_item.count  = 0;
	m_iQueueIndex = 0;
	// have we an actual queue?...
	if (m_pQueue && m_iQueueSize > 0) {
		// Reset time references...
		// time slot: the amount of time (in ticks)
		// each queue slot will hold scheduled events;
		m_iTimeSlot = 1 + m_pSession->tickFromFrame(
			m_pSession->midiEngine()->readAhead() << 1) / m_iQueueSize;
		// time start: the time (in ticks) of the
		// current queue head slot; usually zero ;)
		m_iTimeStart = m_pSession->tickFromFrame(
			m_pSession->audioEngine()->sessionCursor()->frameTime());
		// Time to reset buffer...
		for (unsigned int i = 0; i < m_iQueueSize; ++i) {
			m_pQueue[i].value = 0;
			m_pQueue[i].count = 0;
		}
		// Done reset.
	} else {
		m_iTimeStart = 0;
		m_iTimeSlot  = 0;
	}
}


// Update monitor (nothing really done here).
void qtractorMidiMonitor::update (void)
{
	// Do nothing yet...
}


// end of qtractorMidiMonitor.cpp
