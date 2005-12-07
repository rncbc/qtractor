// qtractorAudioBuffer.h
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

#ifndef __qtractorAudioBuffer_h
#define __qtractorAudioBuffer_h

#include "qtractorAudioFile.h"
#include "qtractorRingBuffer.h"

#include <qthread.h>


//----------------------------------------------------------------------
// class qtractorAudioBuffer -- Ring buffer/cache template declaration.
//

class qtractorAudioBuffer
{
public:

	// Constructors.
	qtractorAudioBuffer();
	// Default destructor.
	virtual ~qtractorAudioBuffer();

	// Internal file descriptor accessors.
	qtractorAudioFile* file() const;

	// File implementation properties.
	unsigned short channels() const;
	unsigned long frames() const;
	unsigned int samplerate() const;

	// Operational initializer/terminator.
	bool open(const char *pszName,int iMode = qtractorAudioFile::Read);
	void close();

	// Buffer data read/write.
	int read(float **ppBuffer, unsigned int iFrames, unsigned int iOffset = 0);
	int write(float **ppBuffer, unsigned int iFrames);

	// Special kind of super-read/channel-mix.
	int readMix(float **ppBuffer, unsigned short iChannels, unsigned int iFrames,
		unsigned int iOffset = 0, float fGain = 1.0);

	// Buffer data seek.
	bool seek (unsigned long iOffset);

	// Reset this buffer's state.
	void reset();

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

	// Base sync method.
	void sync();
	
#ifdef DEBUG
	void dump_state(const char *pszPrefix) const;
#endif

protected:

	// Sync mode methods.
	void readSync();
	void writeSync();

private:

	// Audio file instance variables.
	qtractorAudioFile *m_pFile;

	qtractorRingBuffer<float> *m_pRingBuffer;

	unsigned int   m_iThreshold;
	unsigned long  m_iOffset;
	unsigned long  m_iLength;
	bool           m_bIntegral;
	bool           m_bEndOfFile;

	unsigned long  m_iLoopStart;
	unsigned long  m_iLoopEnd;

	float        **m_ppFrames;
};


//----------------------------------------------------------------------
// class qtractorAudioBufferThread -- Ring-cache manager thread (singleton).
//

class qtractorAudioBufferThread : public QThread
{
public:

	// Singleton instance accessor.
	static qtractorAudioBufferThread& Instance();
	// Singleton destroyer.
	static void Destroy();

	// Audio file list manager methods.
	void attach(qtractorAudioBuffer *pAudioBuffer);
	void detach(qtractorAudioBuffer *pAudioBuffer);

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition.
	void sync();

protected:

	// Constructor.
	qtractorAudioBufferThread();

	// The main thread executive.
	void run();

private:

	// The singleton instance.
	static qtractorAudioBufferThread* g_pInstance;

	// Whether the thread is logically running.
	bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;

	// The list of managed audio buffers.
	QPtrList<qtractorAudioBuffer> m_list;
};


#endif  // __qtractorAudioBuffer_h


// end of qtractorAudioBuffer.h
