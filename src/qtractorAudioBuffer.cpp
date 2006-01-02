// qtractorAudioBuffer.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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
qtractorAudioBuffer::qtractorAudioBuffer ( unsigned int iSampleRate )
{
	m_pFile          = NULL;

	m_pRingBuffer    = NULL;

	m_iThreshold     = 0;
	m_iOffset        = 0;
	m_iLength        = 0;
	m_bIntegral      = false;
	m_bEndOfFile     = false;

	m_iLoopStart     = 0;
	m_iLoopEnd       = 0;

	m_iSeekPending   = 0;

	m_iSampleRate    = iSampleRate;
	m_bResample      = false;
	m_fResampleRatio = 1.0;
	m_iInputPending  = 0;
	m_ppFrames       = NULL;
	m_ppInBuffer     = NULL;
#ifdef CONFIG_LIBSAMPLERATE
	m_ppOutBuffer    = NULL;
	m_ppSrcState     = NULL;
#endif
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
	return (m_pFile ? framesIn(m_pFile->frames()) : 0);
}


// Working sample rate.
unsigned int qtractorAudioBuffer::sampleRate (void) const
{
	return m_iSampleRate;
}


// Working resample ratio.
float qtractorAudioBuffer::resampleRatio (void) const
{
	return m_fResampleRatio;
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

	// Check samplerate and how many channels there are.
	unsigned short iChannels = m_pFile->channels();
	// Just one more sanity check...
	if (iChannels < 1 || m_pFile->sampleRate() < 1) {
		m_pFile->close();
		return false;
	}

	// Compute sample rate converter stuff.
	m_iInputPending  = 0;
	m_fResampleRatio = 1.0;
#ifdef CONFIG_LIBSAMPLERATE
	m_bResample = (m_iSampleRate != m_pFile->sampleRate());
	if (m_bResample) {
		m_fResampleRatio = (float) m_iSampleRate / m_pFile->sampleRate();
		m_ppOutBuffer    = new float *     [iChannels];
		m_ppSrcState     = new SRC_STATE * [iChannels];
	}
#else   // CONFIG_LIBSAMPLERATE
	m_bResample = false;
#endif

	// Allocate ring-buffer now.
	m_pRingBuffer = new qtractorRingBuffer<float> (iChannels, frames());
	m_iThreshold  = (m_pRingBuffer->size() >> 2);

	// Allocate actual buffer stuff...
	m_ppFrames   = new float * [iChannels];
	m_ppInBuffer = new float * [iChannels];
	for (unsigned short i = 0; i < iChannels; i++) {
		m_ppInBuffer[i] = new float [m_iThreshold];
		m_ppFrames[i]   = m_ppInBuffer[i];
	}

#ifdef CONFIG_LIBSAMPLERATE
	// Sample rate converter stuff, whether needed...
	if (m_ppOutBuffer) {
		int err = 0;
		for (unsigned short i = 0; i < iChannels; i++) {
			m_ppOutBuffer[i] = new float [m_iThreshold];
			m_ppSrcState[i]  = src_new(SRC_SINC_BEST_QUALITY, 1, &err);
		}
	}
#endif  // CONFIG_LIBSAMPLERATE

	// Read-ahead a whole bunch, if applicable...
	if (m_pFile->mode() & qtractorAudioFile::Read) {
		readSync();
		if (m_pRingBuffer->writeIndex() < m_pRingBuffer->size() - 1) {
			m_bIntegral = true;
			deleteIOBuffers();
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

	// Release internal I/O buffers.
	deleteIOBuffers();

	// Deallocate any buffer stuff...
	if (m_pRingBuffer) {
		delete m_pRingBuffer;
		m_pRingBuffer = NULL;
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
	//	m_iOffset = iOffset;
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
			m_pRingBuffer->setReadIndex(ri + iOffset - (wo - rs));
		//	m_iOffset += iOffset - (wo - rs);
			return true;
		}
	}

	// Bad luck, gotta go straight down to disk...
	//	if (!m_pFile->seek(iOffset))
	//		return false;

	m_iOffset = iOffset;
	m_iSeekPending++;
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

	unsigned long offset = m_iOffset;

	// Check whether we have some hard-seek pending...
	if (m_iSeekPending > 0) {
		m_iSeekPending = 0;
		// Refill the whole buffer....
		m_pRingBuffer->reset();
		m_bEndOfFile = false;
		m_iInputPending = 0;
#ifdef CONFIG_LIBSAMPLERATE
		for (unsigned short i = 0; i < m_pRingBuffer->channels(); i++) {
			if (m_ppSrcState && m_ppSrcState[i])
				src_reset(m_ppSrcState[i]);
			m_ppFrames[i] = m_ppInBuffer[i];
		}
#endif  // CONFIG_LIBSAMPLERATE
		if (!m_pFile->seek(framesIn(offset)))
			return;
	}

	unsigned int ws = m_pRingBuffer->writable();
	if (ws == 0 || m_bEndOfFile)
		return;

	unsigned int nahead  = ws;
	unsigned int ntotal  = 0;
	unsigned int nbuffer = m_iThreshold;

	unsigned long ls = m_iLoopStart;
	unsigned long le = m_iLoopEnd;

	while (nahead > 0) {
		if (nahead > nbuffer)
			nahead = nbuffer;
		if (ls < le && offset < le
			&& offset + framesOut(nahead) >= le)
			nahead = framesOut(le - offset);
		unsigned int nread = readBuffer(nahead);
		if (nread > 0) {
			ntotal += nread;
			offset += nread;
			if (ls < le && offset >= le) {
				m_pFile->seek(framesIn(ls));
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

	unsigned long offset = m_iOffset;

	unsigned int rs = m_pRingBuffer->readable();
	if (rs == 0)
		return;

	unsigned int nbehind = rs;
	unsigned int ntotal  = 0;
	unsigned int nbuffer = m_iThreshold;

	while (nbehind > 0) {
		if (nbehind > nbuffer)
			nbehind = nbuffer;
		unsigned int nwrite = writeBuffer(nbehind);
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

	if (m_iLength < m_iOffset)
		m_iLength = m_iOffset;

#ifdef DEBUG
	dump_state("-writeSync()");
#endif
}


// I/O buffer read process.
int qtractorAudioBuffer::readBuffer ( unsigned int nframes )
{
#ifdef DEBUG_0
	fprintf(stderr, "+readBuffer(%u) pending=%u\n", nframes, m_iInputPending);
#endif

	int nread = 0;

#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample) {

		unsigned int nahead = nframes;
		if (nahead > m_iInputPending) {
			nahead -= m_iInputPending;
			nread   = m_pFile->read(m_ppFrames, nahead);
		}
		nread += m_iInputPending;

		if (m_ppOutBuffer == NULL) {
			if (nread > 0)
				nread = m_pRingBuffer->write(m_ppFrames, nread);
			return nread;
		}

		int ngen = 0;
		SRC_DATA src_data;

		for (unsigned short i = 0; i < m_pRingBuffer->channels(); i++) {
			// Fill all resampler parameter data...
			src_data.data_in       = m_ppInBuffer[i];
			src_data.data_out      = m_ppOutBuffer[i];
			src_data.input_frames  = nread;
			src_data.output_frames = nframes;
			src_data.end_of_input  = (nread < 1);
			src_data.src_ratio     =  m_fResampleRatio;
			src_data.input_frames_used = 0;
			src_data.output_frames_gen = 0;
			// Do the resample work...
			if (src_process(m_ppSrcState[i], &src_data) == 0) {
				if (i == 0) {
					m_iInputPending = nread - src_data.input_frames_used;
					ngen = src_data.output_frames_gen;
				}
				if (m_iInputPending > 0 && src_data.input_frames_used > 0) {
					::memmove(m_ppInBuffer[i],
						m_ppInBuffer[i] + src_data.input_frames_used,
						m_iInputPending * sizeof(float));
				}
				m_ppFrames[i] = m_ppInBuffer[i] + m_iInputPending;
			}
		}

		if (ngen > 0)
			nread = m_pRingBuffer->write(m_ppOutBuffer, ngen);

	} else {
#endif   // CONFIG_LIBSAMPLERATE

		nread = m_pFile->read(m_ppFrames, nframes);
		if (nread > 0)
			nread = m_pRingBuffer->write(m_ppFrames, nread);

#ifdef CONFIG_LIBSAMPLERATE
	}
#endif   // CONFIG_LIBSAMPLERATE

#ifdef DEBUG_0
	fprintf(stderr, "-readBuffer(%u) pending=%u --> nread=%d\n",
		nframes, m_iInputPending, nread);
#endif

	return nread;
}


// I/O buffer write process.
int qtractorAudioBuffer::writeBuffer ( unsigned int nframes )
{
	int nwrite = m_pRingBuffer->read(m_ppFrames, nframes, 0);
	if (nwrite > 0)
		nwrite = m_pFile->write(m_ppFrames, nwrite);

	return nwrite;
}


// I/O buffer release.
void qtractorAudioBuffer::deleteIOBuffers (void)
{
	// Release internal and resampler buffers.
	for (unsigned short i = 0; i < m_pRingBuffer->channels(); i++) {
#ifdef CONFIG_LIBSAMPLERATE
		if (m_ppSrcState && m_ppSrcState[i])
			m_ppSrcState[i] = src_delete(m_ppSrcState[i]);
		if (m_ppOutBuffer && m_ppOutBuffer[i]) {
			delete [] m_ppOutBuffer[i];
			m_ppOutBuffer[i] = NULL;
		}
#endif	// CONFIG_LIBSAMPLERATE
		if (m_ppInBuffer && m_ppInBuffer[i]) {
			delete [] m_ppInBuffer[i];
			m_ppInBuffer[i] = NULL;
		}
	}

#ifdef CONFIG_LIBSAMPLERATE
	if (m_ppSrcState) {
		delete [] m_ppSrcState;
		m_ppSrcState = NULL;
	}
	if (m_ppOutBuffer) {
		delete [] m_ppOutBuffer;
		m_ppOutBuffer = NULL;
	}
#endif	// CONFIG_LIBSAMPLERATE
	if (m_ppInBuffer) {
		delete [] m_ppInBuffer;
		m_ppInBuffer = NULL;
	}
	if (m_ppFrames) {
		delete [] m_ppFrames;
		m_ppFrames = NULL;
	}

	// Remaining instance variables.
	m_iInputPending  = 0;
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
	if (m_bIntegral) {
		m_pRingBuffer->reset();
		m_pRingBuffer->setWriteIndex(m_iLength);
	} else {
		m_iSeekPending++;
		m_iLength = 0;
		m_bEndOfFile = false;
		qtractorAudioBufferThread::Instance().sync();
	}

#ifdef DEBUG
	dump_state("-reset()");
#endif
}

// Frame position converters.
unsigned long qtractorAudioBuffer::framesIn ( unsigned long iFrames ) const
{
	if (m_bResample)
		iFrames = (unsigned long) ((float) iFrames * m_fResampleRatio);

	return iFrames;
}

unsigned long qtractorAudioBuffer::framesOut ( unsigned long iFrames ) const
{
	if (m_bResample)
		iFrames = (unsigned long) ((float) iFrames / m_fResampleRatio);

	return iFrames;
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
			pAudioBuffer = pAudioBuffer->next();
		}
		m_cond.wait(&m_mutex);
	}
	m_mutex.unlock();

#ifdef DEBUG
	fprintf(stderr, "qtractorAudioBufferThread::run(%p): stopped.\n", this);
#endif
}


// end of qtractorAudioBuffer.cpp
