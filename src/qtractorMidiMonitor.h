// qtractorMidiMonitor.h
//
/****************************************************************************
   Copyright (C) 2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorMidiMonitor_h
#define __qtractorMidiMonitor_h

#include "qtractorMonitor.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiEvent.h"


//----------------------------------------------------------------------------
// qtractorMidiMonitor -- MIDI monitor bridge value processor.

class qtractorMidiMonitor : public qtractorMonitor
{
public:

	// Constructor.
	qtractorMidiMonitor(qtractorSession *pSession,
		unsigned int iBufferSize = 32)
		: qtractorMonitor(), m_pSession(pSession), m_pBuffer(NULL)
		{ setBufferSize(iBufferSize); }

	// Destructor.
	~qtractorMidiMonitor()
		{ setBufferSize(0); }

	// Buffer property accessors.
	unsigned int bufferSize() const
		{ return m_iBufferSize; }

	void setBufferSize(unsigned int iBufferSize)
	{
		// Delete old buffer...
		if (m_pBuffer) {
			delete [] m_pBuffer;
			m_pBuffer = NULL;
		}
		// Set new buffer holders...		
		m_iBufferSize = iBufferSize;
		if (m_iBufferSize > 0) {
			// Buffer size range.
			const unsigned int iMinBufferSize = 32;
			const unsigned int iMaxBufferSize = 32 * 32; // 1024		
			// Adjust size of nearest power-of-two, if necessary.
			if (iBufferSize < iMaxBufferSize) {
				m_iBufferSize = iMinBufferSize;
				while (m_iBufferSize < iBufferSize)
					m_iBufferSize <<= 1;
			} else {
				m_iBufferSize = iMaxBufferSize;
			}
			// The size overflow convenience mask.
			m_iBufferMask = (m_iBufferSize - 1);
			// Allocate actual buffer stuff...
			m_pBuffer = new unsigned char [m_iBufferSize];
			// Time to reset buffer...
			resetBuffer();
		}
	}		

	// Monitor enqueue method.
	void enqueue ( qtractorMidiEvent::EventType type,
		unsigned char val, unsigned long tick )
	{
		if (m_iTimeSlot < 1)
			return;
		unsigned long iOffset = (tick - m_iTimeStart)  / m_iTimeSlot;
		if (iOffset < m_iBufferSize) {
			unsigned int iIndex = m_iReadIndex + iOffset;
			if (type == qtractorMidiEvent::NOTEON) {
				if (m_pBuffer[iIndex] < val)
					m_pBuffer[iIndex] = val;
			} else {
				m_pBuffer[iIndex]++;
			}
		}
	}
	
	// Monitor dequeue method.
	unsigned char dequeue()
	{
		unsigned char val = 0;
		unsigned long iCurrentTime = m_pSession->tickFromFrame(
			m_pSession->audioEngine()->sessionCursor()->frameTime());
		if (iCurrentTime < m_iTimeStart + m_iBufferSize * m_iTimeSlot) {
			while (m_iTimeStart < iCurrentTime) {
				if (val < m_pBuffer[m_iReadIndex])
					val = m_pBuffer[m_iReadIndex];
				m_pBuffer[m_iReadIndex] = 0;
				++m_iReadIndex &= m_iBufferMask;
				m_iTimeStart += m_iTimeSlot;
			}
		}
		return val;
	}
	
	// Reset monitor buffer (should be private).
	void resetBuffer()
	{
		// Reset monitor variables.
		m_iReadIndex = 0;
		m_iTimeSlot  = m_pSession->tickFromFrame(
			2 * m_pSession->midiEngine()->readAhead()) / m_iBufferSize;
		m_iTimeStart = m_pSession->tickFromFrame(
			m_pSession->audioEngine()->sessionCursor()->frameTime());	
		// Reset monitor buffer.
		for (unsigned int i = 0; i < m_iBufferSize; i++)
			m_pBuffer[i] = 0;
	}
	
protected:

	// Reset monitor (nothing really done here).
	void reset() {}

private:

	// Instance variables.
	qtractorSession *m_pSession;
	unsigned int     m_iBufferSize;
	unsigned int     m_iBufferMask;
	unsigned char   *m_pBuffer;
	unsigned int     m_iReadIndex;
	unsigned long    m_iTimeSlot;
	unsigned long    m_iTimeStart;
};


#endif  // __qtractorMidiMonitor_h

// end of qtractorMidiMonitor.h
