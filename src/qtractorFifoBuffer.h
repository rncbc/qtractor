// qtractorFifoBuffer.h
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

   Adapted and refactored from the SoundTouch library (L)GPL,
   Copyright (C) 2001-2006, Olli Parviainen.

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

#ifndef __qtractorFifoBuffer_h
#define __qtractorFifoBuffer_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//----------------------------------------------------------------------
// class qtractorFifoBuffer -- FIFO buffer/cache template declaration.
//

template<typename T>
class qtractorFifoBuffer
{
public:

	// Constructor.
	qtractorFifoBuffer(unsigned short iChannels = 2);
	// Destructor.
	~qtractorFifoBuffer();

	// Implementation initializer.
	void setChannels(unsigned short iChannels);

	// Implementation properties.
	unsigned short channels() const { return m_iChannels;   }
	unsigned int bufferSize() const { return m_iBufferSize; }

	// Write samples/frames to the end of sample frame buffer.
	unsigned int writeFrames(
		T **ppFrames, unsigned int iFrames, unsigned int iOffset = 0);

	// Adjusts the book-keeping to increase number of frames
	// in the buffer without copying any actual frames.
	void putFrames(
		T **ppFrames, unsigned int iFrames, unsigned iOffset = 0);
	void putFrames(unsigned int iFrames);

	// Read frames from beginning of the sample buffer.
	unsigned int readFrames(
		T **ppFrames, unsigned int iFrames, unsigned int iOffset = 0) const;

	// Adjusts book-keeping so that given number of frames are removed
	// from beginning of the sample buffer without copying them anywhere. 
	unsigned int receiveFrames(
		T **ppFrames, unsigned int iFrames, unsigned iOffset = 0);
	unsigned int receiveFrames(unsigned int iFrames);

	// Returns number of frames currently available.
	unsigned int frames() const { return m_iFrameCount; }

	// Returns a pointer to the beginning of the output samples. 
	T *ptrBegin(unsigned short iChannel) const
		{ return m_ppBuffer[iChannel] + m_iFramePos; }

	// Returns a pointer to the end of the used part of the sample buffer.
	T *ptrEnd(unsigned short iChannel) const
		{ return m_ppBuffer[iChannel] + m_iFramePos + m_iFrameCount; }

	// Returns nonzero if there aren't any frames available.
	bool isEmpty() const { return (m_iFrameCount == 0); }

	// Clears all the buffers.
	void clear() { m_iFrameCount = m_iFramePos = 0; }	

	// Ensures that the buffer has capacity for at least this many frames.
	void ensureCapacity(const unsigned int iSlackCapacity);

private:

	unsigned short m_iChannels;
	T **m_ppBuffer;
	T **m_ppBufferUnaligned;
	unsigned int m_iBufferSize;
	unsigned int m_iFrameCount;
	unsigned int m_iFramePos;
};


//----------------------------------------------------------------------
// class qtractorFifoBuffer -- FIFO buffer/cache method implementation.
//

// Constructor.
template<typename T>
qtractorFifoBuffer<T>::qtractorFifoBuffer ( unsigned short iChannels ) :
	m_iChannels(0), m_ppBuffer(NULL), m_ppBufferUnaligned(NULL),
	m_iFrameCount(0), m_iFramePos(0)
{
	setChannels(iChannels);
}


// Destructor.
template<typename T>
qtractorFifoBuffer<T>::~qtractorFifoBuffer (void)
{
	if (m_ppBufferUnaligned) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			delete [] m_ppBufferUnaligned[i];
		delete [] m_ppBufferUnaligned;
	}
	if (m_ppBuffer)
		delete [] m_ppBuffer;
}


// Sets number of channels, 1=mono, 2=stereo, ...
template<typename T>
void qtractorFifoBuffer<T>::setChannels ( unsigned short iChannels )
{
	if (m_iChannels == iChannels)
		return;

	m_iChannels = iChannels;
	m_iBufferSize = 0;

	ensureCapacity(1024);
}


// Read frames from beginning of the sample buffer.
template<typename T>
unsigned int qtractorFifoBuffer<T>::readFrames (
	T **ppFrames, unsigned int iFrames, unsigned int iOffset ) const
{
	unsigned int iMaxFrames
		= (iFrames > m_iFrameCount ? m_iFrameCount : iFrames);
	for (unsigned short i = 0; i < m_iChannels; ++i)
		::memcpy(ppFrames[i], ptrBegin(i) + iOffset, iMaxFrames * sizeof(T));
	return iMaxFrames;
}


// Adjusts book-keeping so that given number of frames are removed
// from beginning of the sample buffer without copying them anywhere. 
//
// Used to reduce the number of samples in the buffer when accessing
// the sample buffer directly with ptrBegin() function.
template<typename T>
unsigned int qtractorFifoBuffer<T>::receiveFrames (
	T **ppFrames, unsigned int iFrames, unsigned int iOffset )
{
	return receiveFrames(readFrames(ppFrames, iFrames, iOffset));
}

template<typename T>
unsigned int qtractorFifoBuffer<T>::receiveFrames ( unsigned int iFrames )
{
	if (iFrames >= m_iFrameCount) {
		unsigned int iFrameCount = m_iFrameCount;
		m_iFrameCount = 0;
		return iFrameCount;
	}

	m_iFrameCount -= iFrames;
	m_iFramePos += iFrames;

	return iFrames;
}


// Write samples/frames to the end of sample frame buffer.
template<typename T>
unsigned int qtractorFifoBuffer<T>::writeFrames (
	T **ppFrames, unsigned int iFrames, unsigned int iOffset )
{
	ensureCapacity(iFrames);
	for (unsigned short i = 0; i < m_iChannels; ++i)
		::memcpy(ptrEnd(i), ppFrames[i] + iOffset, iFrames * sizeof(T));
	return iFrames;
}


// Adjusts the book-keeping to increase number of frames
// in the buffer without copying any actual frames.
//
// This function is used to update the number of frames in the
// sample buffer when accessing the buffer directly with ptrEnd()
// function. Please be careful though.
template<typename T>
void qtractorFifoBuffer<T>::putFrames (
	T **ppFrames, unsigned int iFrames, unsigned int iOffset )
{
	m_iFrameCount += writeFrames(ppFrames, iFrames, iOffset);
}

template<typename T>
void qtractorFifoBuffer<T>::putFrames ( unsigned int iFrames )
{
	ensureCapacity(iFrames);
	m_iFrameCount += iFrames;
}


// Ensures that the buffer has capacity for at least this many frames.
template<typename T>
void qtractorFifoBuffer<T>::ensureCapacity ( const unsigned int iSlackCapacity )
{
	unsigned int iBufferSize
		= m_iFramePos + m_iFrameCount + (iSlackCapacity << 2);

	if (iBufferSize > m_iBufferSize) {
		// Enlarge the buffer in 4KB steps (round up to next 4KB boundary)
		unsigned int iSizeInBytes
			= ((iBufferSize << 1) * sizeof(T) + 4095) & -4096;
		// assert(iSizeInBytes % 2 == 0);
		float **ppTemp = new T * [m_iChannels];
		float **ppTempUnaligned = new T * [m_iChannels];
		m_iBufferSize = (iSizeInBytes / sizeof(T)) + (16 / sizeof(T));
		for (unsigned short i = 0; i < m_iChannels; ++i) {
			ppTempUnaligned[i] = new T [m_iBufferSize];
			ppTemp[i] = (T *) (((unsigned long) ppTempUnaligned[i] + 15) & -16);
			if (m_ppBuffer) {
				if (m_iFrameCount > 0) {
					::memcpy(ppTemp[i], ptrBegin(i),
						m_iFrameCount * sizeof(T));
				}
				delete [] m_ppBufferUnaligned[i];
			}
		}
		if (m_ppBuffer) {
			delete [] m_ppBufferUnaligned;
			delete [] m_ppBuffer;
		}
		m_ppBufferUnaligned = ppTempUnaligned;
		m_ppBuffer = ppTemp;
		// Done realloc.
		m_iFramePos = 0;
	} else if (m_iFramePos > (m_iBufferSize >> 2)) {
		// Rewind the buffer by moving data...
		if (m_iFrameCount > 0) {
			for (unsigned short i = 0; i < m_iChannels; ++i) {
				::memmove(m_ppBuffer[i], ptrBegin(i),
					m_iFrameCount * sizeof(T));
			}
		}
		// Done rewind.
		m_iFramePos = 0;
	}
}


#endif  // __qtractorFifoBuffer_h

// end of qtractorFifoBuffer.h
