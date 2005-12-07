// qtractorRingBuffer.h
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorRingBuffer_h
#define __qtractorRingBuffer_h

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
	unsigned short channels() const;

	// Ring-buffer cache properties.
	unsigned int size() const;
	unsigned int readable() const;
	unsigned int writable() const;

	// Buffer data read/write.
	int read(T **ppBuffer, unsigned int iFrames, unsigned int iOffset = 0);
	int write(T **ppBuffer, unsigned int iFrames);

	// Special kind of super-read/channel-mix.
	int readMix(T **ppBuffer, unsigned short iChannels, unsigned int iFrames,
		unsigned int iOffset = 0, float fGain = 1.0);

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
	unsigned int   m_iReadIndex;
	unsigned int   m_iWriteIndex;

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

	// Buffer size range.
	const unsigned int iMinBufferSize = 4096;
	const unsigned int iMaxBufferSize = 4096 * 32;

	// Adjust size of nearest power-of-two, if necessary.
	if (iBufferSize < iMaxBufferSize) {
		m_iBufferSize = iMinBufferSize;
		while (m_iBufferSize < iBufferSize)
			m_iBufferSize <<= 1;
	} else {
		m_iBufferSize = iMaxBufferSize;
	}
	// The size overflow convenience mask and tthreshold.
	m_iBufferMask = (m_iBufferSize - 1);

	// Allocate actual buffer stuff...
	m_ppBuffer = new T* [m_iChannels];
	for (unsigned short i = 0; i < m_iChannels; i++)
		m_ppBuffer[i] = new T [m_iBufferSize];

	m_iReadIndex  = 0;
	m_iWriteIndex = 0;
}

// Default destructor.
template<typename T>
qtractorRingBuffer<T>::~qtractorRingBuffer (void)
{
	// Deallocate any buffer stuff...
	if (m_ppBuffer) {
		for (unsigned short i = 0; i < m_iChannels; i++)
			delete [] m_ppBuffer[i];
		delete [] m_ppBuffer;
	}
}


// Buffer channels.
template<typename T>
unsigned short qtractorRingBuffer<T>::channels (void) const
{
	return m_iChannels;
}


// Buffer size in frames.
template<typename T>
unsigned int qtractorRingBuffer<T>::size (void) const
{
	return m_iBufferSize;
}


template<typename T>
unsigned int qtractorRingBuffer<T>::readable (void) const
{
	unsigned int w = m_iWriteIndex;
	unsigned int r = m_iReadIndex;
	if (w > r) {
		return (w - r);
	} else {
		return (w - r + m_iBufferSize) & m_iBufferMask;
	}
}

template<typename T>
unsigned int qtractorRingBuffer<T>::writable (void) const
{
	unsigned int w = m_iWriteIndex;
	unsigned int r = m_iReadIndex;
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

	unsigned int r = m_iReadIndex;

	unsigned int n1, n2;
	if (r + iFrames > m_iBufferSize) {
		n1 = (m_iBufferSize - r);
		n2 = (r + iFrames) & m_iBufferMask;
	} else {
		n1 = iFrames;
		n2 = 0;
	}

	for (unsigned short i = 0; i < m_iChannels; i++) {
		::memcpy((T *) (ppFrames[i] + iOffset),
			(T *)(m_ppBuffer[i] + r), n1 * sizeof(T));
		if (n2) {
			n1 += iOffset;
			::memcpy((T *)(ppFrames[i] + n1),
				m_ppBuffer[i], n2 * sizeof(T));
		}
	}

	m_iReadIndex = (r + iFrames) & m_iBufferMask;

	return iFrames;
}


// Buffer raw data write.
template<typename T>
int qtractorRingBuffer<T>::write ( T **ppFrames, unsigned int iFrames )
{
	unsigned int ws = writable();
	if (ws == 0)
		return 0;

	if (iFrames > ws)
		iFrames = ws;

	unsigned int w = m_iWriteIndex;

	unsigned int n1, n2;
	if (w + iFrames > m_iBufferSize) {
		n1 = (m_iBufferSize - w);
		n2 = (w + iFrames) & m_iBufferMask;
	} else {
		n1 = iFrames;
		n2 = 0;
	}

	for (unsigned short i = 0; i < m_iChannels; i++) {
		::memcpy((T *)(m_ppBuffer[i] + w), ppFrames[i],
			n1 * sizeof(T));
		if (n2 > 0) {
			::memcpy(m_ppBuffer[i], (T *)(ppFrames[i] + n1),
				n2 * sizeof(T));
		}
	}

	m_iWriteIndex = (w + iFrames) & m_iBufferMask;

	return iFrames;
}


// Special kind of super-read/channel-mix.
template<typename T>
int qtractorRingBuffer<T>::readMix (
	T **ppFrames, unsigned short iChannels, unsigned int iFrames,
	unsigned int iOffset, float fGain )
{
	unsigned int rs = readable();
	if (rs == 0)
		return 0;

	if (iFrames > rs)
		iFrames = rs;

	unsigned int r = m_iReadIndex;

	unsigned short i, j, iAux;
	unsigned int n, n1, n2;
	if (r + iFrames > m_iBufferSize) {
		n1 = (m_iBufferSize - r);
		n2 = (r + iFrames) & m_iBufferMask;
	} else {
		n1 = iFrames;
		n2 = 0;
	}

	if (iChannels == m_iChannels) {
		for (i = 0; i < m_iChannels; i++) {
			for (n = 0; n < n1; n++)
				ppFrames[i][n + iOffset] += fGain * m_ppBuffer[i][n + r];
			for (n = 0; n < n2; n++)
				ppFrames[i][n + n1 + iOffset] += fGain * m_ppBuffer[i][n];
		}
	}
	else if (iChannels > m_iChannels) {
		j = 0;
		iAux = (iChannels - (iChannels % m_iChannels));
		for (i = 0; i < iAux; i++) {
			for (n = 0; n < n1; n++)
				ppFrames[i][n + iOffset] += fGain * m_ppBuffer[j][n + r];
			for (n = 0; n < n2; n++)
				ppFrames[i][n + n1 + iOffset] += fGain * m_ppBuffer[j][n];
			if (++j >= m_iChannels)
				j = 0;
		}
	}
	else {
		i = 0;
		iAux = (m_iChannels - (m_iChannels % iChannels));
		for (j = 0; j < iAux; j++) {
			for (n = 0; n < n1; n++)
				ppFrames[i][n + iOffset] += fGain * m_ppBuffer[j][n + r];
			for (n = 0; n < n2; n++)
				ppFrames[i][n + n1 + iOffset] += fGain * m_ppBuffer[j][n];
			if (++i >= iChannels)
				i = 0;
		}
	}

	m_iReadIndex = (r + iFrames) & m_iBufferMask;

	return iFrames;
}


// Reset this buffers state.
template<typename T>
void qtractorRingBuffer<T>::reset (void)
{
	m_iReadIndex  = 0;
	m_iWriteIndex = 0;
}


// Next read position index accessors.
template<typename T>
void qtractorRingBuffer<T>::setReadIndex ( unsigned int iReadIndex )
{
	m_iReadIndex = (iReadIndex & m_iBufferMask);
}

template<typename T>
unsigned int qtractorRingBuffer<T>::readIndex (void) const
{
	return m_iReadIndex;
}


// Next write position index accessors.
template<typename T>
void qtractorRingBuffer<T>::setWriteIndex ( unsigned int iWriteIndex )
{
	m_iWriteIndex = (iWriteIndex & m_iBufferMask);
}

template<typename T>
unsigned int qtractorRingBuffer<T>::writeIndex (void) const
{
	return m_iWriteIndex;
}


#endif  // __qtractorRingBuffer_h

// end of qtractorRingBuffer.h
