// qtractorMidiMonitor.cpp
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

#include "qtractorMidiMonitor.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"


// Module constants.
const unsigned int c_iQueueSize = 16; // Must be power of 2;
const unsigned int c_iQueueMask = c_iQueueSize - 1;

// Singleton variables.
unsigned int  qtractorMidiMonitor::g_iFrameSlot = 0;
unsigned int  qtractorMidiMonitor::g_iTimeSlot[2] = { 0, 0 };
unsigned long qtractorMidiMonitor::g_iTimeSplit = 0;


//----------------------------------------------------------------------------
// qtractorMidiMonitor -- MIDI monitor bridge value processor.

// Constructor.
qtractorMidiMonitor::qtractorMidiMonitor ( float fGain, float fPanning )
	: qtractorMonitor(fGain, fPanning)
{
	// Allocate actual buffer stuff...
	m_pQueue = new QueueItem [c_iQueueSize];

	// May reset now...
	reset();
}

// Destructor.
qtractorMidiMonitor::~qtractorMidiMonitor (void)
{
	delete [] m_pQueue;
}


// Monitor enqueue method.
void qtractorMidiMonitor::enqueue ( qtractorMidiEvent::EventType type,
	unsigned char val, unsigned long tick )
{
	// Check whether this is a scheduled value...
	if (m_iTimeStart < tick && g_iTimeSlot[1] > 0) {
		// Find queue offset index...
		unsigned int iOffset = (tick - m_iTimeStart) / timeSlot(tick);
		// FIXME: Ignore outsiders (which would manifest as
		// out-of-time phantom monitor peak values...)
		if (iOffset > c_iQueueMask)
			iOffset = c_iQueueMask;
		unsigned int iIndex = (m_iQueueIndex + iOffset) & c_iQueueMask;
		// Set the value in buffer...
		QueueItem& item = m_pQueue[iIndex];
		if (item.value < val && type == qtractorMidiEvent::NOTEON)
			item.value = val;
		// Increment enqueued count.
		++(item.count);
		// Done enqueueing.
	} else {
		// Alternative is sending it directly
		// as a non-enqueued direct value...
		if (m_item.value < val && type == qtractorMidiEvent::NOTEON)
			m_item.value = val;
		// Increment direct count.
		++(m_item.count);
		// Done direct.
	}
}


// Monitor value dequeue method.
float qtractorMidiMonitor::value (void)
{
	// Grab-and-reset current direct value...
	unsigned char val = m_item.value;
	m_item.value = 0;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession && g_iFrameSlot > 0) {
		// Sweep the queue until current time...
		unsigned long iFrameTime = pSession->frameTimeEx();
		while (m_iFrameStart < iFrameTime) {
			QueueItem& item = m_pQueue[m_iQueueIndex];
			if (val < item.value)
				val = item.value;
			m_item.count += item.count;
			item.value = 0;
			item.count = 0;
			++m_iQueueIndex &= c_iQueueMask;
			m_iFrameStart += g_iFrameSlot;
			m_iTimeStart += timeSlot(m_iTimeStart);
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


// Clear monitor.
void qtractorMidiMonitor::clear (void)
{
	// (Re)initialize all...
	m_item.value  = 0;
	m_item.count  = 0;

	m_iQueueIndex = 0;

	// Time to reset buffer...
	for (unsigned int i = 0; i < c_iQueueSize; ++i) {
		m_pQueue[i].value = 0;
		m_pQueue[i].count = 0;
	}
}


// Reset monitor.
void qtractorMidiMonitor::reset (void)
{
	// (Re)initialize all...
	clear();

	// Reset actual frame/time start...
	m_iFrameStart = 0;
	m_iTimeStart  = 0;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		m_iFrameStart = pSession->frameTimeEx();
}


// Update monitor (nothing really done here).
void qtractorMidiMonitor::update (void)
{
	// Do nothing yet...
}


// Singleton time base reset.
void qtractorMidiMonitor::resetTime (
	qtractorTimeScale *pTimeScale, unsigned long iFrame )
{
	g_iFrameSlot = pTimeScale->sampleRate() / c_iQueueSize;

	splitTime(pTimeScale, iFrame, 0);
}


// Singleton time base split (scheduled tempo change)
void qtractorMidiMonitor::splitTime ( qtractorTimeScale *pTimeScale,
	unsigned long iFrame, unsigned long iTime )
{
	// Reset time references...
	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);
	unsigned long t0 = pNode->tickFromFrame(iFrame);

	// Time slot: the amount of time (in ticks)
	// each queue slot will hold scheduled events;
	g_iTimeSlot[0] = g_iTimeSlot[1];
	g_iTimeSlot[1] = pNode->tickFromFrame(iFrame + g_iFrameSlot) - t0;

	// Relative time where time splits.
	g_iTimeSplit = iTime;
}


// end of qtractorMidiMonitor.cpp
