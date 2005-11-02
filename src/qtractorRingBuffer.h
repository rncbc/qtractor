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

#include "qtractorRingBufferThread.h"


//----------------------------------------------------------------------
// class qtractorRingBufferFile -- Ring buffer/cache abstract file mockup.
//

template<typename T>
class qtractorRingBufferFile
{
public:

	// Virtual destructor.
	virtual ~qtractorRingBufferFile() {}

	// Basic file open mode.
	enum { None = 0, Read = 1, Write = 2 };

	// Pure virtual method mockups.
	virtual bool open  (const char *pszName, int iMode = Read) = 0;
	virtual int  read  (T **ppFrames, unsigned int iFrames) = 0;
	virtual int  write (T **ppFrames, unsigned int iFrames) = 0;
	virtual bool seek  (unsigned long iOffset) = 0;
	virtual void close () = 0;

	// Pure virtual accessor mockups.
	virtual int mode() const = 0;

	// These shall give us a clue on the size
	// of the ring buffer size (in frames).
	virtual unsigned short channels() const = 0;
	virtual unsigned long frames() const = 0;

	// Other special informational methods.
	virtual unsigned int samplerate() const = 0;
};


//----------------------------------------------------------------------
// class qtractorRingBufferBase -- Ring buffer/cache base declaration.
//

class qtractorRingBufferBase
{
public:

	// Virtual destructor.
	virtual ~qtractorRingBufferBase() {}

	// Virtual base sync method.
	virtual void sync() = 0;
};


//----------------------------------------------------------------------
// class qtractorRingBuffer -- Ring buffer/cache template declaration.
//

template<typename T>
class qtractorRingBuffer : public qtractorRingBufferBase
{
public:

	// Constructors.
	qtractorRingBuffer(qtractorRingBufferFile<T> *pFile = NULL);
	// Default destructor.
	virtual ~qtractorRingBuffer();

	// Internal file descriptor accessors.
	void setFile(qtractorRingBufferFile<T>* pFile);
	qtractorRingBufferFile<T>* file() const;

	// File implementation properties.
	unsigned short channels() const;
	unsigned long frames() const;
	unsigned int samplerate() const;

	// Ring-buffer cache properties.
	unsigned int size() const;
	unsigned int readable() const;
	unsigned int writable() const;

	// Operational initializer/terminator.
	bool open(const char *pszName,int iMode = qtractorRingBufferFile<T>::Read);
	void close();

	// Buffer data read/write.
	int read(T **ppBuffer, unsigned int iFrames, unsigned int iOffset = 0);
	int write(T **ppBuffer, unsigned int iFrames);

	// Special kind of super-read/channel-mix.
	int readMix(T **ppBuffer, unsigned short iChannels, unsigned int iFrames,
		unsigned int iOffset = 0, float fGain = 1.0);

	// Buffer data seek.
	bool seek (unsigned long iOffset);

	// Reset this buffer's state.
	void reset();

	// Next read position index accessors.
	void setReadIndex(unsigned int iReadIndex);
	unsigned int readIndex() const;

	// Next write position index accessors.
	void setWriteIndex(unsigned int iWriteIndex);
	unsigned int writeIndex() const;

	// Physical (next read-ahead/write-behind) offset accessors.
	void setOffset(unsigned long iOffset);
	unsigned long offset() const;

	// Current known length (in frames).
	unsigned long length() const;

	// Whether concrete file fits completely in buffer.
	bool integral() const;

	// Whether file read has exausted.
	bool eof() const;

	// Loop points accessors.
	void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd);
	unsigned long loopStart() const;
	unsigned long loopEnd() const;

	// Virtual base sync method.
	void sync();
	
#ifdef DEBUG
	void dump_state(const QString& s) const;
#endif

protected:

	// Buffer raw data read/write.
	void read_i(unsigned int iReadIndex,
		T **ppBuffer, unsigned int iFrames, unsigned int iOffset) const;
	void write_i(unsigned int iWriteIndex,
		T **ppBuffer, unsigned int iFrames);

	void readMix_i(unsigned int iReadIndex,
		T **ppBuffer, unsigned short iChannels, unsigned int iFrames,
		unsigned int iOffset, float fGain) const;

	// Sync mode methods.
	void readSync();
	void writeSync();

private:

	// Audio (ring) buffer instance variables.
	qtractorRingBufferFile<T> *m_pFile;

	unsigned short m_iChannels;
	unsigned int   m_iSize;
	unsigned int   m_iSizeMask;
	unsigned int   m_iReadIndex;
	unsigned int   m_iWriteIndex;
	unsigned int   m_iThreshold;
	unsigned long  m_iOffset;
	unsigned long  m_iLength;
	bool           m_bIntegral;
	bool           m_bEndOfFile;

	unsigned long  m_iLoopStart;
	unsigned long  m_iLoopEnd;

	T** m_ppBuffer;
	T** m_ppFrames;
};


//----------------------------------------------------------------------
// class qtractorRingBuffer -- Ring buffer/cache method implementation.
//

// Constructors.
template<typename T>
qtractorRingBuffer<T>::qtractorRingBuffer ( qtractorRingBufferFile<T> *pFile )
{
	m_pFile       = pFile;  // We'll own this one.

	m_iChannels   = 0;
	m_iSize       = 0;
	m_iSizeMask   = 0;
	m_iReadIndex  = 0;
	m_iWriteIndex = 0;
	m_iThreshold  = 0;
	m_iOffset     = 0;
	m_iLength     = 0;
	m_bIntegral   = false;
	m_bEndOfFile  = false;

	m_iLoopStart  = 0;
	m_iLoopEnd    = 0;

	m_ppBuffer    = NULL;
	m_ppFrames    = NULL;
}

// Default destructor.
template<typename T>
qtractorRingBuffer<T>::~qtractorRingBuffer (void)
{
	setFile(NULL);
}


// Setup the internal file descriptor.
template<typename T>
void qtractorRingBuffer<T>::setFile( qtractorRingBufferFile<T>* pFile )
{
	// Close the file, definitively.
	close();

	// As we must own it, delete file descriptor too...
	if (m_pFile)
		delete m_pFile;

	m_pFile = pFile;
}


// Return the internal file descriptor.
template<typename T>
qtractorRingBufferFile<T>* qtractorRingBuffer<T>::file(void) const
{
	return m_pFile;
}


// Buffer channels.
template<typename T>
unsigned short qtractorRingBuffer<T>::channels (void) const
{
	return m_iChannels; // (m_pFile ? m_pFile->channels() : 0);
}


// Estimated file size in frames.
template<typename T>
unsigned long qtractorRingBuffer<T>::frames (void) const
{
	return (m_pFile ? m_pFile->frames() : 0);
}


// File recorded sample rate.
template<typename T>
unsigned int qtractorRingBuffer<T>::samplerate (void) const
{
	return (m_pFile ? m_pFile->samplerate() : 0);
}


// Buffer size in frames.
template<typename T>
unsigned int qtractorRingBuffer<T>::size (void) const
{
	return m_iSize;
}


template<typename T>
unsigned int qtractorRingBuffer<T>::readable (void) const
{
	unsigned int w = m_iWriteIndex;
	unsigned int r = m_iReadIndex;
	if (w > r) {
		return (w - r);
	} else {
		return (w - r + m_iSize) & m_iSizeMask;
	}
}

template<typename T>
unsigned int qtractorRingBuffer<T>::writable (void) const
{
	unsigned int w = m_iWriteIndex;
	unsigned int r = m_iReadIndex;
	if (w > r){
		return ((r - w + m_iSize) & m_iSizeMask) - 1;
	} else if (r > w) {
		return (r - w) - 1;
	} else {
		return m_iSize - 1;
	}
}


// Operational buffer initializer/terminator.
template<typename T>
bool qtractorRingBuffer<T>::open ( const char *pszName, int iMode )
{
	if (m_pFile == NULL)
		return false;

	// Make sure everything starts closed.
	close();

	// Go open it...
	if (!m_pFile->open(pszName, iMode))
		return false;	

	// Check how many channels there are.
	m_iChannels = m_pFile->channels();
	// Just one more sanity check...
	if (m_iChannels < 1) {
		m_pFile->close();
		return false;
	}

	// Buffer size range.
	const unsigned int iMinBufferSize = 4096;
	const unsigned int iMaxBufferSize = 4096 * 32;

	// Adjust size of nearest power-of-two, if necessary.
	const unsigned long iFrames = m_pFile->frames();
	if (iFrames < iMaxBufferSize) {
		m_iSize = iMinBufferSize;
		while (m_iSize < iFrames)
			m_iSize <<= 1;
	} else {
		m_iSize = iMaxBufferSize;
	}
	// The size overflow convenience mask and tthreshold.
	m_iSizeMask  = (m_iSize - 1);
	m_iThreshold = (m_iSize >> 2);

	// Allocate actual buffer stuff...
	if (m_iSize > 0) {
		m_ppBuffer = new T* [m_iChannels];
		m_ppFrames = new T* [m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; i++) {
			m_ppBuffer[i] = new T [m_iSize];
			m_ppFrames[i] = new T [m_iThreshold];
		}
	}

	// Read-ahead a whole bunch, if applicable...
	if (m_pFile->mode() & qtractorRingBufferFile<T>::Read) {
		readSync();
		if (/* m_bEndOfFile ||*/ m_iWriteIndex < m_iSizeMask) {
			m_bIntegral = true;
			for (unsigned short i = 0; i < m_iChannels; i++)
				delete [] m_ppFrames[i];
			delete [] m_ppFrames;
			m_ppFrames = NULL;
		}
	}

	// Make it sync-managed...
	qtractorRingBufferThread::Instance().attach(this);
	
	return true;
}


// Operational buffer terminator.
template<typename T>
void qtractorRingBuffer<T>::close (void)
{
	if (m_pFile == NULL)
		return;

	// Not sync-managed anymore...
	qtractorRingBufferThread::Instance().detach(this);

	// Write-behind any remains, if applicable...
	if (m_pFile->mode() & qtractorRingBufferFile<T>::Write)
		writeSync();

	// Time to close it good.
	m_pFile->close();

	// Deallocate any buffer stuff...
	if (m_ppBuffer) {
		for (unsigned short i = 0; i < m_iChannels; i++)
			delete [] m_ppBuffer[i];
		delete [] m_ppBuffer;
		m_ppBuffer = NULL;
	}

	if (m_ppFrames) {
		for (unsigned short i = 0; i < m_iChannels; i++)
			delete [] m_ppFrames[i];
		delete [] m_ppFrames;
		m_ppFrames = NULL;
	}

	// Reset all relevant state variables.
	m_iChannels   = 0;
	m_iSize       = 0;
	m_iSizeMask   = 0;
	m_iReadIndex  = 0;
	m_iWriteIndex = 0;
	m_iThreshold  = 0;
	m_iOffset     = 0;
	m_iLength     = 0;
	m_bIntegral   = false;
	m_bEndOfFile  = false;
}


// Buffer raw data read.
template<typename T>
void qtractorRingBuffer<T>::read_i ( unsigned int iReadIndex,
	T **ppFrames, unsigned int iFrames, unsigned int iOffset ) const
{
	unsigned int r = (iReadIndex & m_iSizeMask);

	unsigned int n1, n2;
	if (r + iFrames > m_iSize) {
		n1 = (m_iSize - r);
		n2 = (r + iFrames) & m_iSizeMask;
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
}


// Buffer raw data write.
template<typename T>
void qtractorRingBuffer<T>::write_i ( unsigned int iWriteIndex,
	T **ppFrames, unsigned int iFrames )
{
	unsigned int w = (iWriteIndex & m_iSizeMask);

	unsigned int n1, n2;
	if (w + iFrames > m_iSize) {
		n1 = (m_iSize - w);
		n2 = (w + iFrames) & m_iSizeMask;
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
}


// Buffer data read.
template<typename T>
int qtractorRingBuffer<T>::read ( T **ppFrames, unsigned int iFrames,
	unsigned int iOffset )
{
	if (m_ppBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

#ifdef DEBUG_0
	dump_state(QString("+read(%1)").arg(iFrames));
#endif

	unsigned int rs = readable();
	if (rs == 0)
		return 0;

	if (iFrames > rs)
		iFrames = rs;

	unsigned int ri = m_iReadIndex;

	// Are we self-contained (ie. got integral file in buffer) and looping?
	if (m_bIntegral) {
		unsigned long ls = m_iLoopStart;
		unsigned long le = m_iLoopEnd;
		// Are we in the middle of the loop range ?
		if (ls < le) {
			while (ri < le && ri + iFrames >= le) {
				unsigned int nread = le - ri;
				read_i(ri, ppFrames, nread, iOffset);
				iOffset += nread;
				iFrames -= nread;
				ri = ls;
			}
		}
	}
	// Move the (remaining) data around...	
	read_i(ri, ppFrames, iFrames, iOffset);

	m_iReadIndex = (ri + iFrames) & m_iSizeMask;

#ifdef DEBUG_0
	dump_state(QString("-read(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (writable() > m_iThreshold)
		qtractorRingBufferThread::Instance().sync();

	return iFrames;
}


// Buffer data write.
template<typename T>
int qtractorRingBuffer<T>::write ( T **ppFrames, unsigned int iFrames )
{
	if (m_ppBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

#ifdef DEBUG_0
	dump_state(QString("+write(%1)").arg(iFrames));
#endif

	unsigned int ws = writable();
	if (ws == 0)
		return 0;

	if (iFrames > ws)
		iFrames = ws;

	unsigned int wi = m_iWriteIndex;
	unsigned long offset = m_iOffset;

	// Move the data around...
	write_i(wi, ppFrames, iFrames);

	m_iWriteIndex = (wi + iFrames) & m_iSizeMask;
	m_iOffset = (offset + iFrames);

#ifdef DEBUG_0
	dump_state(QString("-write(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (readable() > m_iThreshold)
		qtractorRingBufferThread::Instance().sync();

	return iFrames;
}


// Special kind of super-read/channel-mix.
template<typename T>
void qtractorRingBuffer<T>::readMix_i ( unsigned int iReadIndex,
	T **ppFrames, unsigned short iChannels, unsigned int iFrames,
	unsigned int iOffset, float fGain ) const
{
	unsigned int r = (iReadIndex & m_iSizeMask);

	unsigned short i, j, iAux;
	unsigned int n, n1, n2;
	if (r + iFrames > m_iSize) {
		n1 = (m_iSize - r);
		n2 = (r + iFrames) & m_iSizeMask;
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
}


// Special data read/mix.
template<typename T>
int qtractorRingBuffer<T>::readMix (
	T **ppFrames, unsigned short iChannels, unsigned int iFrames,
	unsigned int iOffset, float fGain )
{
	if (m_ppBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

#ifdef DEBUG_0
	dump_state(QString("+readMix(%1)").arg(iFrames));
#endif

	unsigned int rs = readable();
	if (rs == 0)
		return 0;

	if (iFrames > rs)
		iFrames = rs;

	unsigned int ri = m_iReadIndex;

	// Are we self-contained (ie. got integral file in buffer) and looping?
	if (m_bIntegral) {
		unsigned long ls = m_iLoopStart;
		unsigned long le = m_iLoopEnd;
		// Are we in the middle of the loop range ?
		if (ls < le) {
			while (ri < le && ri + iFrames >= le) {
				unsigned int nread = le - ri;
				readMix_i(ri, ppFrames, iChannels, nread, iOffset, fGain);
				iOffset += nread;
				iFrames -= nread;
				ri = ls;
			}
		}
	}
	// Mix the (remaining) data around...	
	readMix_i(ri, ppFrames, iChannels, iFrames, iOffset, fGain);

	m_iReadIndex = (ri + iFrames) & m_iSizeMask;

#ifdef DEBUG_0
	dump_state(QString("-readMix(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (writable() > m_iThreshold)
		qtractorRingBufferThread::Instance().sync();

	return iFrames;
}


// Buffer data seek.
template<typename T>
bool qtractorRingBuffer<T>::seek ( unsigned long iOffset )
{
	if (m_pFile == NULL)
		return false;

	// Seek is only valid on read-only mode.
	if (m_pFile->mode() & qtractorRingBufferFile<T>::Write)
		return false;

	// Special case on integral cached files...
	if (m_bIntegral) {
		if (iOffset >= m_iLength)
			return false;
		m_iReadIndex = iOffset;
		m_iOffset = iOffset;
		return true;
	}

#ifdef DEBUG
	dump_state(QString(">seek(%1)").arg(iOffset));
#endif

	unsigned int  rs = readable();
	unsigned int  ri = m_iReadIndex;
	unsigned long wo = m_iOffset;

	// Check if target is already cached...
	if (iOffset >= wo - rs && iOffset < wo) {
		// If not under loop, it won't break...
		unsigned long ls = m_iLoopStart;
		unsigned long le = m_iLoopEnd;
		if (ls >= le || iOffset > le) {
			m_iReadIndex = (ri + (iOffset - (wo - rs))) & m_iSizeMask;
			m_iOffset += (iOffset - (wo - rs));
			return true;
		}
	}

	// Bad luck, gotta go straight down to disk...
	//	if (!m_pFile->seek(iOffset))
	//		return false;

	// Reset to intended position.
	m_iReadIndex  = 0;
	m_iWriteIndex = 0;
	m_iOffset = iOffset;
	m_iLength = iOffset;

	// Refill the buffer.
	m_bEndOfFile = false;
	// readSync();
	qtractorRingBufferThread::Instance().sync();

	return true;
}


// Base-mode sync executive.
template<typename T>
void qtractorRingBuffer<T>::sync (void)
{
	if (m_ppFrames == NULL)
		return;
	if (m_ppBuffer == NULL)
		return;
	if (m_pFile == NULL)
		return;

	int mode = m_pFile->mode();
	if (mode & qtractorRingBufferFile<T>::Read)
		readSync();
	if (mode & qtractorRingBufferFile<T>::Write)
		writeSync();
}

// Read-mode sync executive.
template<typename T>
void qtractorRingBuffer<T>::readSync (void)
{
#ifdef DEBUG
	dump_state("+readSync()");
#endif
	unsigned int  ws = writable();
	unsigned int  wi = m_iWriteIndex;
	unsigned long offset = m_iOffset;

	if (ws == 0 || m_bEndOfFile)
		return;

	if (!m_pFile->seek(offset))
		return;

	unsigned int nahead  = ws;
	unsigned int ntotal  = 0;
	unsigned int nbuffer = m_iThreshold;

	unsigned long ls = m_iLoopStart;
	unsigned long le = m_iLoopEnd;

	while (nahead > 0) {
		if (nahead > nbuffer)
			nahead = nbuffer;
		if (ls < le && offset < le && offset + nahead >= le)
			nahead = le - offset;
		unsigned int nread = m_pFile->read(m_ppFrames, nahead);
		if (nread > 0) {
			write_i(wi + ntotal, m_ppFrames, nread);
			ntotal += nread;
			offset += nread;
			if (ls < le && offset >= le) {
				m_pFile->seek(ls);
				offset = ls;
			}
			nahead = ws - ntotal;
		} else {
			nahead = 0;
			m_bEndOfFile = true;
		}
	}

	m_iWriteIndex = (wi + ntotal) & m_iSizeMask;
	m_iOffset = offset;

	if (m_iLength < offset)
		m_iLength = offset;

#ifdef DEBUG
	dump_state("-readSync()");
#endif
}


// Write-mode sync executive.
template<typename T>
void qtractorRingBuffer<T>::writeSync (void)
{
#ifdef DEBUG
	dump_state("+writeSync()");
#endif
	unsigned int  rs = readable();
	unsigned int  ri = m_iReadIndex;
	unsigned long offset = m_iOffset;

	if (rs == 0)
		return;

	unsigned int nbehind = rs;
	unsigned int ntotal  = 0;
	unsigned int nbuffer = m_iThreshold;

	while (nbehind > 0) {
		if (nbehind > nbuffer)
			nbehind = nbuffer;
		read_i(ri + ntotal, m_ppFrames, nbehind, 0);
		unsigned int nwrite = m_pFile->write(m_ppFrames, nbehind);
		if (nwrite > 0) {
			ntotal += nwrite;
			offset += nwrite;
		}
		if (nwrite < nbehind) {
			nbehind = 0;
		} else {
			nbehind = rs - ntotal;
		}
	}

	m_iReadIndex  = (ri + ntotal) & m_iSizeMask;
	m_iOffset = offset;

	if (m_iLength < offset)
		m_iLength = offset;

#ifdef DEBUG
	dump_state("-writeSync()");
#endif
}


// Reset this buffers state.
template<typename T>
void qtractorRingBuffer<T>::reset (void)
{
#ifdef DEBUG
	dump_state("+reset()");
#endif

	m_iReadIndex  = 0;
	m_iWriteIndex = (m_bIntegral ? m_iLength : 0);
	m_iOffset     = 0;

	if (!m_bIntegral) {
		m_iLength = 0;
		m_bEndOfFile = false;
		qtractorRingBufferThread::Instance().sync();
	}

#ifdef DEBUG
	dump_state("-reset()");
#endif
}


// Next read position index accessors.
template<typename T>
void qtractorRingBuffer<T>::setReadIndex ( unsigned int iReadIndex )
{
	m_iReadIndex = (iReadIndex & m_iSizeMask);
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
	m_iWriteIndex = (iWriteIndex & m_iSizeMask);
}

template<typename T>
unsigned int qtractorRingBuffer<T>::writeIndex (void) const
{
	return m_iWriteIndex;
}


// Physical (next read-ahead/write-behind) offset accessors.
template<typename T>
void qtractorRingBuffer<T>::setOffset ( unsigned long iOffset )
{
	m_iOffset = iOffset;
}

template<typename T>
unsigned long qtractorRingBuffer<T>::offset (void) const
{
	return m_iOffset;
}


// Current known length (in frames).
template<typename T>
unsigned long qtractorRingBuffer<T>::length (void) const
{
	return m_iLength;
}


// Whether concrete file fits completely in buffer.
template<typename T>
bool qtractorRingBuffer<T>::integral (void) const
{
	return m_bIntegral;
}


// Whether file read has exausted.
template<typename T>
bool qtractorRingBuffer<T>::eof (void) const
{
	return m_bEndOfFile;
}


// Loop settings.
template<typename T>
void qtractorRingBuffer<T>::setLoop ( unsigned long iLoopStart,
	unsigned long iLoopEnd )
{
	if (iLoopEnd >= iLoopStart) {
		m_iLoopStart = iLoopStart;
		m_iLoopEnd   = iLoopEnd;
	}
}

template<typename T>
unsigned long qtractorRingBuffer<T>::loopStart (void) const
{
	return m_iLoopStart;
}

template<typename T>
unsigned long qtractorRingBuffer<T>::loopEnd (void) const
{
	return m_iLoopEnd;
}


#ifdef DEBUG
template<typename T>
void qtractorRingBuffer<T>::dump_state ( const QString& s ) const
{
	unsigned int  rs = readable();
	unsigned int  ws = writable();
	unsigned int  ri = m_iReadIndex;
	unsigned int  wi = m_iWriteIndex;
	unsigned long offset = m_iOffset;
	unsigned long frames = m_iLength;

	fprintf(stderr, "%-16s rs=%6u ws=%6u ri=%6u wi=%6u o=%8lu f=%8lu\n",
		s.latin1(), rs, ws, ri, wi, offset, frames);
}
#endif

#endif  // __qtractorRingBuffer_h


// end of qtractorRingBuffer.h
