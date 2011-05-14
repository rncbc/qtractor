// qtractorRingBuffer.h
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

#ifndef __qtractorRingBuffer_h
#define __qtractorRingBuffer_h

#include "qtractorAtomic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//----------------------------------------------------------------------
// class qtractorRingBuffer -- Ring buffer/cache template declaration.
//

template<typename T>
class qtractorRingBuffer
{
public:

	// Constructors.
	qtractorRingBuffer(unsigned short iChannels, unsigned int iBufferSize = 0);
	// Default destructor.
	~qtractorRingBuffer();

	// Implementation properties.
	unsigned short channels() const { return m_iChannels;   }
	unsigned int bufferSize() const { return m_iBufferSize; }
	unsigned int bufferMask() const { return m_iBufferMask; }

	// Direct ring-buffer accessor (DANGEROUS).
	T **buffer() const { return m_ppBuffer; }

	// Ring-buffer cache properties.
	unsigned int readable() const;
	unsigned int writable() const;

	// Buffer data read/write.
	int read(T **ppBuffer, unsigned int iFrames, unsigned int iOffset = 0);
	int write(T **ppBuffer, unsigned int iFrames, unsigned int iOffset = 0);

	// Reset this buffer's state.
	void reset();

	// Next read position index accessors.
	void setReadIndex(unsigned int iReadIndex);
	unsigned int readIndex() const;

	// Next write position index accessors.
	void setWriteIndex(unsigned int iWriteIndex);
	unsigned int writeIndex() const;

private:

	unsigned short m_iChannels;
	unsigned int   m_iBufferSize;
	unsigned int   m_iBufferMask;
	qtractorAtomic m_iReadIndex;
	qtractorAtomic m_iWriteIndex;

	T** m_ppBuffer;
};


//----------------------------------------------------------------------
// class qtractorRingBuffer -- Ring buffer/cache method implementation.
//

// Constructors.
template<typename T>
qtractorRingBuffer<T>::qtractorRingBuffer ( unsigned short iChannels,
	unsigned int iBufferSize )
{
	m_iChannels = iChannels;

	// Adjust buffer size of nearest power-of-two, if necessary.
	const unsigned int iMinBufferSize = 4096;
	m_iBufferSize = iMinBufferSize;
	while (m_iBufferSize < iBufferSize)
		m_iBufferSize <<= 1;

	// The size overflow convenience mask and tthreshold.
	m_iBufferMask = (m_iBufferSize - 1);

	// Allocate actual buffer stuff...
	m_ppBuffer = new T* [m_iChannels];
	for (unsigned short i = 0; i < m_iChannels; ++i)
		m_ppBuffer[i] = new T [m_iBufferSize];

	ATOMIC_SET(&m_iReadIndex,  0);
	ATOMIC_SET(&m_iWriteIndex, 0);
}

// Default destructor.
template<typename T>
qtractorRingBuffer<T>::~qtractorRingBuffer (void)
{
	// Deallocate any buffer stuff...
	if (m_ppBuffer) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			delete [] m_ppBuffer[i];
		delete [] m_ppBuffer;
	}
}


template<typename T>
unsigned int qtractorRingBuffer<T>::readable (void) const
{
	unsigned int w = ATOMIC_GET(&m_iWriteIndex);
	unsigned int r = ATOMIC_GET(&m_iReadIndex);
	if (w > r) {
		return (w - r);
	} else {
		return (w - r + m_iBufferSize) & m_iBufferMask;
	}
}

template<typename T>
unsigned int qtractorRingBuffer<T>::writable (void) const
{
	unsigned int w = ATOMIC_GET(&m_iWriteIndex);
	unsigned int r = ATOMIC_GET(&m_iReadIndex);
	if (w > r){
		return ((r - w + m_iBufferSize) & m_iBufferMask) - 1;
	} else if (r > w) {
		return (r - w) - 1;
	} else {
		return m_iBufferSize - 1;
	}
}


// Buffer raw data read.
template<typename T>
int qtractorRingBuffer<T>::read ( T **ppFrames, unsigned int iFrames,
	unsigned int iOffset )
{
	unsigned int rs = readable();
	if (rs == 0)
		return 0;

	if (iFrames > rs)
		iFrames = rs;

	unsigned int r = ATOMIC_GET(&m_iReadIndex);

	unsigned int n1, n2;
	if (r + iFrames > m_iBufferSize) {
		n1 = (m_iBufferSize - r);
		n2 = (r + iFrames) & m_iBufferMask;
	} else {
		n1 = iFrames;
		n2 = 0;
	}

	for (unsigned short i = 0; i < m_iChannels; ++i) {
		::memcpy((T *)(ppFrames[i] + iOffset),
			(T *)(m_ppBuffer[i] + r), n1 * sizeof(T));
		if (n2) {
			n1 += iOffset;
			::memcpy((T *)(ppFrames[i] + n1),
				m_ppBuffer[i], n2 * sizeof(T));
		}
	}

	ATOMIC_SET(&m_iReadIndex, (r + iFrames) & m_iBufferMask);

	return iFrames;
}


// Buffer raw data write.
template<typename T>
int qtractorRingBuffer<T>::write ( T **ppFrames, unsigned int iFrames,
	unsigned int iOffset )
{
	unsigned int ws = writable();
	if (ws == 0)
		return 0;

	if (iFrames > ws)
		iFrames = ws;

	unsigned int w = ATOMIC_GET(&m_iWriteIndex);

	unsigned int n1, n2;
	if (w + iFrames > m_iBufferSize) {
		n1 = (m_iBufferSize - w);
		n2 = (w + iFrames) & m_iBufferMask;
	} else {
		n1 = iFrames;
		n2 = 0;
	}

	for (unsigned short i = 0; i < m_iChannels; ++i) {
		::memcpy((T *)(m_ppBuffer[i] + w),
			(T *)(ppFrames[i] + iOffset), n1 * sizeof(T));
		if (n2 > 0) {
			n1 += iOffset;
			::memcpy(m_ppBuffer[i],
				(T *)(ppFrames[i] + n1), n2 * sizeof(T));
		}
	}

	ATOMIC_SET(&m_iWriteIndex, (w + iFrames) & m_iBufferMask);

	return iFrames;
}


// Reset this buffers state.
template<typename T>
void qtractorRingBuffer<T>::reset (void)
{
	ATOMIC_SET(&m_iReadIndex,  0);
	ATOMIC_SET(&m_iWriteIndex, 0);
}


// Next read position index accessors.
template<typename T>
void qtractorRingBuffer<T>::setReadIndex ( unsigned int iReadIndex )
{
	ATOMIC_SET(&m_iReadIndex, (iReadIndex & m_iBufferMask));
}

template<typename T>
unsigned int qtractorRingBuffer<T>::readIndex (void) const
{
	return ATOMIC_GET(&m_iReadIndex);
}


// Next write position index accessors.
template<typename T>
void qtractorRingBuffer<T>::setWriteIndex ( unsigned int iWriteIndex )
{
	ATOMIC_SET(&m_iWriteIndex, (iWriteIndex & m_iBufferMask));
}

template<typename T>
unsigned int qtractorRingBuffer<T>::writeIndex (void) const
{
	return ATOMIC_GET(&m_iWriteIndex);
}


#endif  // __qtractorRingBuffer_h

// end of qtractorRingBuffer.h
