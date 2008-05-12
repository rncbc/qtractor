// qtractorMidiBuffer.h
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiBuffer_h
#define __qtractorMidiBuffer_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alsa/asoundlib.h>


//----------------------------------------------------------------------
// class qtractorMidiBuffer -- MIDI event FIFO buffer/cache declaration.
//

class qtractorMidiBuffer
{
public:

	// Minimem buffer size
	enum { MinBufferSize = 0x100 };

	// Constructor.
	qtractorMidiBuffer(unsigned int iBufferSize = MinBufferSize) :
		m_pBuffer(NULL), m_iBufferSize(0), m_iBufferMask(0),
		m_iReadIndex(0), m_iWriteIndex(0)
	{
		// Adjust size of nearest power-of-two, if necessary.
		m_iBufferSize = MinBufferSize;
		while (m_iBufferSize < iBufferSize)
			m_iBufferSize <<= 1;
		m_iBufferMask = (m_iBufferSize - 1);
		m_pBuffer = new snd_seq_event_t [m_iBufferSize];
	}
		
	// Destructor.
	~qtractorMidiBuffer() { if (m_pBuffer) delete [] m_pBuffer; }

	// Implementation properties.
	unsigned int bufferSize() const { return m_iBufferSize; }

	// Clears the buffer.
	void clear() { m_iReadIndex = m_iWriteIndex = 0; }	

	// Returns nonzero if there aren't any events available.
	bool isEmpty() const { return (m_iReadIndex == m_iWriteIndex); }

	// Returns a pointer to the first of the output events. 
	snd_seq_event_t *peek() const
		{ return (isEmpty() ? NULL : &m_pBuffer[m_iReadIndex]); }

	// Read next event from buffer.
	snd_seq_event_t *next()
	{
		if (!isEmpty())	++m_iReadIndex &= m_iBufferMask;
		return peek();
	}

	// Read event from buffer.
	snd_seq_event_t *pop()
	{
		unsigned int iReadIndex = m_iReadIndex;
		if (iReadIndex == m_iWriteIndex)
			return NULL;
		m_iReadIndex = (iReadIndex + 1) & m_iBufferMask;
		return &m_pBuffer[iReadIndex];
	}

	// Write event to buffer.
	bool push(snd_seq_event_t *pEvent, unsigned long iTick = 0)
	{
		unsigned int iWriteIndex = (m_iWriteIndex + 1) & m_iBufferMask;
		if (iWriteIndex == m_iReadIndex)
			return false;
		m_pBuffer[m_iWriteIndex] = *pEvent;
		if (iTick > 0)
			m_pBuffer[m_iWriteIndex].time.tick = iTick;
		m_iWriteIndex = iWriteIndex;
		return true;
	}

	// Returns number of events currently available.
	unsigned int count() const
	{
		unsigned int iWriteIndex = m_iWriteIndex;
		unsigned int iReadIndex  = m_iReadIndex;
		if (iWriteIndex > iReadIndex) {
			return (iWriteIndex - iReadIndex);
		} else {
			return (iWriteIndex - iReadIndex + m_iBufferSize) & m_iBufferMask;
		}
	}

private:

	// Instance variables.
	snd_seq_event_t *m_pBuffer;
	unsigned int m_iBufferSize;
	unsigned int m_iBufferMask;
	unsigned int m_iWriteIndex;
	unsigned int m_iReadIndex;
};


//----------------------------------------------------------------------
// class qtractorMidiMerger -- MIDI event merger buffer/cache.
//

class qtractorMidiMerger
{
public:

	// Constructor.
	qtractorMidiMerger(unsigned int iBufferSize = 0) :
		m_directBuffer(iBufferSize),
		m_queuedBuffer(iBufferSize),
		m_pBuffer(NULL), m_iBuffer(0)
		{ m_pBuffer = new snd_seq_event_t [bufferSize() << 1]; }
		
	// Destructor.
	~qtractorMidiMerger() { if (m_pBuffer) delete [] m_pBuffer; }

	// Implementation properties.
	unsigned int bufferSize() const { return m_queuedBuffer.bufferSize(); }

	// Clears the buffer.
	void clear() { m_iBuffer = 0; }	

	// Returns nonzero if there aren't any events available.
	bool isEmpty() const { return (m_iBuffer == 0); }

	// Event buffer accessor. 
	snd_seq_event_t *buffer() const	{ return m_pBuffer; }

	// Returns number of events result of process.
	unsigned int count() const { return m_iBuffer; }

	// Append one event to direct buffer.
	bool direct(snd_seq_event_t *pEvent, unsigned long iTick = 0)
		{ return m_directBuffer.push(pEvent, iTick); }

	// Append one event to queued buffer.
	bool queued(snd_seq_event_t *pEvent, unsigned long iTick = 0)
		{ return m_queuedBuffer.push(pEvent, iTick); }

	// Process this batch.
	unsigned int process(unsigned long iTickStart, unsigned long iTickEnd)
	{
		clear();

		snd_seq_event_t *pEvent1 = m_directBuffer.peek();
		snd_seq_event_t *pEvent2 = m_queuedBuffer.peek();

		while ((pEvent1 && pEvent1->time.tick < iTickEnd)
			|| (pEvent2 && pEvent2->time.tick < iTickEnd)) {
			while ((pEvent1 &&  pEvent2 && pEvent2->time.tick >= pEvent1->time.tick)
				|| (pEvent1 && !pEvent2 && pEvent1->time.tick < iTickEnd)) {
				m_pBuffer[m_iBuffer] = *pEvent1;
				m_pBuffer[m_iBuffer++].time.tick
					= (pEvent1->time.tick > iTickStart ?
						pEvent1->time.tick - iTickStart : 0);
				pEvent1 = m_directBuffer.next();
			}
			while ((pEvent2 &&  pEvent1 && pEvent1->time.tick >= pEvent2->time.tick)
				|| (pEvent2 && !pEvent1 && pEvent2->time.tick < iTickEnd)) {
				m_pBuffer[m_iBuffer] = *pEvent2;
				m_pBuffer[m_iBuffer++].time.tick
					= (pEvent2->time.tick > iTickStart ?
						pEvent2->time.tick - iTickStart : 0);
				pEvent2 = m_queuedBuffer.next();
			}
		}

		return m_iBuffer;
	}

private:

	// Instance variables.
	qtractorMidiBuffer m_directBuffer;
	qtractorMidiBuffer m_queuedBuffer;

	snd_seq_event_t *m_pBuffer;
	unsigned int     m_iBuffer;
};


#endif  // __qtractorMidiBuffer_h

// end of qtractorMidiBuffer.h
