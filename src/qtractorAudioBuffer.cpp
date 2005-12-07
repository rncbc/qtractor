// qtractorAudioBuffer.cpp
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

#include "qtractorAudioBuffer.h"


//----------------------------------------------------------------------
// class qtractorAudioBuffer -- Ring buffer/cache method implementation.
//

// Constructors.
qtractorAudioBuffer::qtractorAudioBuffer (void)
{
	m_pFile       = NULL;

	m_pRingBuffer = NULL;

	m_iThreshold  = 0;
	m_iOffset     = 0;
	m_iLength     = 0;
	m_bIntegral   = false;
	m_bEndOfFile  = false;

	m_iLoopStart  = 0;
	m_iLoopEnd    = 0;

	m_ppFrames    = NULL;
}

// Default destructor.
qtractorAudioBuffer::~qtractorAudioBuffer (void)
{
	close();
}


// Return the internal file descriptor.
qtractorAudioFile *qtractorAudioBuffer::file(void) const
{
	return m_pFile;
}


// Buffer channels.
unsigned short qtractorAudioBuffer::channels (void) const
{
	return (m_pFile ? m_pFile->channels() : 0);
}


// Estimated file size in frames.
unsigned long qtractorAudioBuffer::frames (void) const
{
	return (m_pFile ? m_pFile->frames() : 0);
}


// File recorded sample rate.
unsigned int qtractorAudioBuffer::samplerate (void) const
{
	return (m_pFile ? m_pFile->samplerate() : 0);
}


// Operational buffer initializer/terminator.
bool qtractorAudioBuffer::open ( const char *pszName, int iMode )
{
	// Make sure everything starts closed.
	close();

	m_pFile = qtractorAudioFileFactory::createAudioFile(pszName);
	if (m_pFile == NULL)
		return false;

	// Go open it...
	if (!m_pFile->open(pszName, iMode))
		return false;	

	// Check how many channels there are.
	unsigned short iChannels = m_pFile->channels();
	// Just one more sanity check...
	if (iChannels < 1) {
		m_pFile->close();
		return false;
	}

	// Allocate ring-buffer now.
	const unsigned long iFrames = m_pFile->frames();
	m_pRingBuffer = new qtractorRingBuffer<float> (iChannels, iFrames);
	m_iThreshold  = (m_pRingBuffer->size() >> 2);

	// Allocate actual buffer stuff...
	m_ppFrames = new float* [iChannels];
	for (unsigned short i = 0; i < iChannels; i++)
		m_ppFrames[i] = new float [m_iThreshold];

	// Read-ahead a whole bunch, if applicable...
	if (m_pFile->mode() & qtractorAudioFile::Read) {
		readSync();
		if (m_pRingBuffer->writeIndex() < m_pRingBuffer->size() - 1) {
			m_bIntegral = true;
			for (unsigned short i = 0; i < iChannels; i++)
				delete [] m_ppFrames[i];
			delete [] m_ppFrames;
			m_ppFrames = NULL;
		}
	}

	// Make it sync-managed...
	qtractorAudioBufferThread::Instance().attach(this);
	
	return true;
}


// Operational buffer terminator.
void qtractorAudioBuffer::close (void)
{
	if (m_pFile == NULL)
		return;

	// Not sync-managed anymore...
	qtractorAudioBufferThread::Instance().detach(this);

	// Write-behind any remains, if applicable...
	if (m_pFile->mode() & qtractorAudioFile::Write)
		writeSync();

	// Time to close it good.
	m_pFile->close();

	// Deallocate any buffer stuff...
	unsigned short iChannels = 0;
	if (m_pRingBuffer) {
		iChannels = m_pRingBuffer->channels();
		delete m_pRingBuffer;
		m_pRingBuffer = NULL;
	}

	if (m_ppFrames) {
		for (unsigned short i = 0; i < iChannels; i++)
			delete [] m_ppFrames[i];
		delete [] m_ppFrames;
		m_ppFrames = NULL;
	}

	// Finally delete what we still own.
	if (m_pFile) {
	    delete m_pFile;
	    m_pFile = NULL;
	}

	// Reset all relevant state variables.
	m_iThreshold  = 0;
	m_iOffset     = 0;
	m_iLength     = 0;
	m_bIntegral   = false;
	m_bEndOfFile  = false;
}


// Buffer data read.
int qtractorAudioBuffer::read ( float **ppFrames, unsigned int iFrames,
	unsigned int iOffset )
{
	if (m_pRingBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

#ifdef DEBUG_0
	dump_state(QString("+read(%1)").arg(iFrames));
#endif

	// Are we self-contained (ie. got integral file in buffer) and looping?
	if (m_bIntegral) {
		unsigned long ls = m_iLoopStart;
		unsigned long le = m_iLoopEnd;
		// Are we in the middle of the loop range ?
		if (ls < le) {
			unsigned int ri = m_pRingBuffer->readIndex();
			while (ri < le && ri + iFrames >= le) {
				unsigned int nread = le - ri;
				m_pRingBuffer->read(ppFrames, nread, iOffset);
				iOffset += nread;
				iFrames -= nread;
				m_pRingBuffer->setReadIndex(ls);
			}
		}
	}
	// Move the (remaining) data around...	
	m_pRingBuffer->read(ppFrames, iFrames, iOffset);

#ifdef DEBUG_0
	dump_state(QString("-read(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (m_pRingBuffer->writable() > m_iThreshold)
		qtractorAudioBufferThread::Instance().sync();

	return iFrames;
}


// Buffer data write.
int qtractorAudioBuffer::write ( float **ppFrames, unsigned int iFrames )
{
	if (m_pRingBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

#ifdef DEBUG_0
	dump_state(QString("+write(%1)").arg(iFrames));
#endif

	// Move the data around...
	m_pRingBuffer->write(ppFrames, iFrames);
	m_iOffset += iFrames;

#ifdef DEBUG_0
	dump_state(QString("-write(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (m_pRingBuffer->readable() > m_iThreshold)
		qtractorAudioBufferThread::Instance().sync();

	return iFrames;
}


// Special kind of super-read/channel-mix.
int qtractorAudioBuffer::readMix (
	float **ppFrames, unsigned short iChannels, unsigned int iFrames,
	unsigned int iOffset, float fGain )
{
	if (m_pRingBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

#ifdef DEBUG_0
	dump_state(QString("+readMix(%1)").arg(iFrames));
#endif

	// Are we self-contained (ie. got integral file in buffer) and looping?
	if (m_bIntegral) {
		unsigned long ls = m_iLoopStart;
		unsigned long le = m_iLoopEnd;
		// Are we in the middle of the loop range ?
		if (ls < le) {
			unsigned int ri = m_pRingBuffer->readIndex();
			while (ri < le && ri + iFrames >= le) {
				unsigned int nread = le - ri;
				m_pRingBuffer->readMix(ppFrames, iChannels, nread, iOffset, fGain);
				iOffset += nread;
				iFrames -= nread;
				m_pRingBuffer->setReadIndex(ls);
			}
		}
	}
	// Mix the (remaining) data around...	
	m_pRingBuffer->readMix(ppFrames, iChannels, iFrames, iOffset, fGain);

#ifdef DEBUG_0
	dump_state(QString("-readMix(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (m_pRingBuffer->writable() > m_iThreshold)
		qtractorAudioBufferThread::Instance().sync();

	return iFrames;
}


// Buffer data seek.
bool qtractorAudioBuffer::seek ( unsigned long iOffset )
{
	if (m_pRingBuffer == NULL)
		return false;
	if (m_pFile == NULL)
		return false;

	// Seek is only valid on read-only mode.
	if (m_pFile->mode() & qtractorAudioFile::Write)
		return false;

	// Special case on integral cached files...
	if (m_bIntegral) {
		if (iOffset >= m_iLength)
			return false;
		m_pRingBuffer->setReadIndex(iOffset);
		m_iOffset = iOffset;
		return true;
	}

#ifdef DEBUG
	dump_state(QString(">seek(%1)").arg(iOffset));
#endif

	unsigned int  rs = m_pRingBuffer->readable();
	unsigned int  ri = m_pRingBuffer->readIndex();
	unsigned long wo = m_iOffset;

	// Check if target is already cached...
	if (iOffset >= wo - rs && iOffset < wo) {
		// If not under loop, it won't break...
		unsigned long ls = m_iLoopStart;
		unsigned long le = m_iLoopEnd;
		if (ls >= le || iOffset > le) {
			m_pRingBuffer->setReadIndex(ri + (iOffset - (wo - rs)));
			m_iOffset += (iOffset - (wo - rs));
			return true;
		}
	}

	// Bad luck, gotta go straight down to disk...
	//	if (!m_pFile->seek(iOffset))
	//		return false;

	// Reset to intended position.
	m_pRingBuffer->reset();
	m_iOffset = iOffset;
	m_iLength = iOffset;

	// Refill the buffer.
	m_bEndOfFile = false;
	// readSync();
	qtractorAudioBufferThread::Instance().sync();

	return true;
}


// Base-mode sync executive.
void qtractorAudioBuffer::sync (void)
{
	if (m_ppFrames == NULL)
		return;
	if (m_pRingBuffer == NULL)
		return;
	if (m_pFile == NULL)
		return;

	int mode = m_pFile->mode();
	if (mode & qtractorAudioFile::Read)
		readSync();
	if (mode & qtractorAudioFile::Write)
		writeSync();
}


// Read-mode sync executive.
void qtractorAudioBuffer::readSync (void)
{
#ifdef DEBUG
	dump_state("+readSync()");
#endif

	unsigned int  ws = m_pRingBuffer->writable();
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
			m_pRingBuffer->write(m_ppFrames, nread);
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

	m_iOffset = offset;

	if (m_iLength < offset)
		m_iLength = offset;

#ifdef DEBUG
	dump_state("-readSync()");
#endif
}


// Write-mode sync executive.
void qtractorAudioBuffer::writeSync (void)
{
#ifdef DEBUG
	dump_state("+writeSync()");
#endif

	unsigned int  rs = m_pRingBuffer->readable();
	unsigned long offset = m_iOffset;

	if (rs == 0)
		return;

	unsigned int nbehind = rs;
	unsigned int ntotal  = 0;
	unsigned int nbuffer = m_iThreshold;

	while (nbehind > 0) {
		if (nbehind > nbuffer)
			nbehind = nbuffer;
		m_pRingBuffer->read(m_ppFrames, nbehind, 0);
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

	m_iOffset = offset;

	if (m_iLength < offset)
		m_iLength = offset;

#ifdef DEBUG
	dump_state("-writeSync()");
#endif
}


// Reset this buffers state.
void qtractorAudioBuffer::reset (void)
{
	if (m_pRingBuffer == NULL)
		return;
	if (m_pFile == NULL)
		return;

#ifdef DEBUG
	dump_state("+reset()");
#endif

	m_iOffset = 0;
	m_pRingBuffer->reset();
	if (m_bIntegral) {
		m_pRingBuffer->setWriteIndex(m_iLength);
	} else {
		m_iLength = 0;
		m_bEndOfFile = false;
		qtractorAudioBufferThread::Instance().sync();
	}

#ifdef DEBUG
	dump_state("-reset()");
#endif
}


// Physical (next read-ahead/write-behind) offset accessors.
void qtractorAudioBuffer::setOffset ( unsigned long iOffset )
{
	m_iOffset = iOffset;
}

unsigned long qtractorAudioBuffer::offset (void) const
{
	return m_iOffset;
}


// Current known length (in frames).
unsigned long qtractorAudioBuffer::length (void) const
{
	return m_iLength;
}


// Whether concrete file fits completely in buffer.
bool qtractorAudioBuffer::integral (void) const
{
	return m_bIntegral;
}


// Whether file read has exausted.
bool qtractorAudioBuffer::eof (void) const
{
	return m_bEndOfFile;
}


// Loop settings.
void qtractorAudioBuffer::setLoop ( unsigned long iLoopStart,
	unsigned long iLoopEnd )
{
	if (iLoopEnd >= iLoopStart) {
		m_iLoopStart = iLoopStart;
		m_iLoopEnd   = iLoopEnd;
	}
}

unsigned long qtractorAudioBuffer::loopStart (void) const
{
	return m_iLoopStart;
}

unsigned long qtractorAudioBuffer::loopEnd (void) const
{
	return m_iLoopEnd;
}


#ifdef DEBUG
void qtractorAudioBuffer::dump_state ( const char *pszPrefix ) const
{
	unsigned int  rs = m_pRingBuffer->readable();
	unsigned int  ws = m_pRingBuffer->writable();
	unsigned int  ri = m_pRingBuffer->readIndex();
	unsigned int  wi = m_pRingBuffer->writeIndex();
	unsigned long offset = m_iOffset;
	unsigned long frames = m_iLength;

	fprintf(stderr, "%-16s rs=%6u ws=%6u ri=%6u wi=%6u o=%8lu f=%8lu\n",
		pszPrefix, rs, ws, ri, wi, offset, frames);
}
#endif


//----------------------------------------------------------------------
// class qtractorAudioBufferThread -- Ring-cache manager thread (singleton).
//

// Initialize singleton instance pointer.
qtractorAudioBufferThread *qtractorAudioBufferThread::g_pInstance = NULL;


// Singleton instance accessor.
qtractorAudioBufferThread& qtractorAudioBufferThread::Instance (void)
{
	if (g_pInstance == NULL) {
		// Create the singleton instance...
		g_pInstance = new qtractorAudioBufferThread();
		std::atexit(Destroy);
		g_pInstance->start(QThread::HighPriority);
		// Give it a bit of time to startup...
		QThread::msleep(100);
 #ifdef DEBUG
		fprintf(stderr, "qtractorAudioBufferThread::Instance(%p)\n", g_pInstance);
 #endif
	}
	return *g_pInstance;
}


// Singleton instance destroyer.
void qtractorAudioBufferThread::Destroy (void)
{
	if (g_pInstance) {
#ifdef DEBUG
		fprintf(stderr, "qtractorAudioBufferThread::Destroy(%p)\n", g_pInstance);
#endif
		// Try to wake and terminate executive thread.
		g_pInstance->setRunState(false);
		if (g_pInstance->running())
			g_pInstance->sync();
		// Give it a bit of time to cleanup...
		QThread::msleep(100);
		// OK. We're done with ourselves.
		delete g_pInstance;
		g_pInstance = NULL;
	}
}


// Constructor.
qtractorAudioBufferThread::qtractorAudioBufferThread (void)
	: QThread()
{
	m_bRunState = false;
	m_list.setAutoDelete(false);
}


// Ring-cache list manager methods.
void qtractorAudioBufferThread::attach ( qtractorAudioBuffer *pAudioBuffer )
{
	QMutexLocker locker(&m_mutex);

	if (m_list.find(pAudioBuffer) < 0)
		m_list.append(pAudioBuffer);
}

void qtractorAudioBufferThread::detach ( qtractorAudioBuffer *pAudioBuffer )
{
	QMutexLocker locker(&m_mutex);

	if (m_list.find(pAudioBuffer) >= 0)
		m_list.remove(pAudioBuffer);
}



// Run state accessor.
void qtractorAudioBufferThread::setRunState ( bool bRunState )
{
	m_bRunState = bRunState;
}

bool qtractorAudioBufferThread::runState (void) const
{
	return m_bRunState;
}


// Wake from executive wait condition.
void qtractorAudioBufferThread::sync (void)
{
	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef DEBUG_0
	else fprintf(stderr, "qtractorAudioBufferThread::sync(%p): tryLock() failed.\n", this);
	msleep(5);
#endif
}


// Thread run executive.
void qtractorAudioBufferThread::run (void)
{
#ifdef DEBUG
	fprintf(stderr, "qtractorAudioBufferThread::run(%p): started.\n", this);
#endif

	m_bRunState = true;

	m_mutex.lock();
	while (m_bRunState) {
		qtractorAudioBuffer *pAudioBuffer = m_list.first();
		while (pAudioBuffer) {
			pAudioBuffer->sync();
			pAudioBuffer = m_list.next();
		}
		m_cond.wait(&m_mutex);
	}
	m_mutex.unlock();

#ifdef DEBUG
	fprintf(stderr, "qtractorAudioBufferThread::run(%p): stopped.\n", this);
#endif
}


// end of qtractorAudioBuffer.cpp
