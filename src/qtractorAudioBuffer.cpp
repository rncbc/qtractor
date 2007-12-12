// qtractorAudioBuffer.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorAudioBuffer.h"

#include "qtractorTimeStretch.h"


//----------------------------------------------------------------------
// class qtractorAudioBufferThread -- Ring-cache manager thread.
//

class qtractorAudioBufferThread : public QThread
{
public:

	// Constructor.
	qtractorAudioBufferThread(qtractorAudioBuffer *pAudioBuffer);

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition (RT-safe).
	void sync();

	// Bypass executive wait condition (non RT-safe).
	void syncExport();

protected:

	// The main thread executive.
	void run();

private:

	// Instance variable.
	qtractorAudioBuffer* m_pAudioBuffer;

	// Whether the thread is logically running.
	bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;
};



//----------------------------------------------------------------------
// class qtractorAudioBuffer -- Ring buffer/cache method implementation.
//

// Constructors.
qtractorAudioBuffer::qtractorAudioBuffer ( unsigned short iChannels,
	unsigned int iSampleRate )
{
	m_iChannels      = iChannels;
	m_iSampleRate    = iSampleRate;

	m_pFile          = NULL;

	m_pRingBuffer    = NULL;
	m_pSyncThread    = NULL;

	m_iThreshold     = 0;
	m_iBufferSize    = 0;
	m_bReadSync      = false;
	m_iReadOffset    = 0;
	m_iWriteOffset   = 0;
	m_iFileLength    = 0;
	m_bIntegral      = false;

	m_iOffset        = 0;
	m_iLength        = 0;

	m_iLoopStart     = 0;
	m_iLoopEnd       = 0;

	m_iSeekOffset    = 0;
	m_iSeekPending   = 0;

	m_ppFrames       = NULL;

	m_bTimeStretch   = false;
	m_fTimeStretch   = 1.0f;
	m_pTimeStretch   = NULL;

#ifdef CONFIG_LIBSAMPLERATE
	m_bResample      = false;
	m_fResampleRatio = 1.0f;
	m_iInputPending  = 0;
	m_ppInBuffer     = NULL;
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
qtractorAudioFile *qtractorAudioBuffer::file (void) const
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
#ifdef CONFIG_LIBSAMPLERATE
	return m_fResampleRatio;
#else
	return 1.0f;
#endif
}


// Operational buffer initializer/terminator.
bool qtractorAudioBuffer::open ( const QString& sFilename, int iMode )
{
	// Make sure everything starts closed.
	close();

	// Get proper file type class...
	m_pFile = qtractorAudioFileFactory::createAudioFile(
		sFilename, m_iChannels, m_iSampleRate);
	if (m_pFile == NULL)
		return false;

	// Go open it...
	if (!m_pFile->open(sFilename, iMode)) {
		delete m_pFile;
		m_pFile = NULL;
		return false;
	}

	// Check samplerate and how many channels there really are.
	unsigned short iChannels = m_pFile->channels();
	// Just one more sanity check...
	if (iChannels < 1 || m_pFile->sampleRate() < 1) {
		m_pFile->close();
		return false;
	}

#ifdef CONFIG_LIBSAMPLERATE
	// Compute sample rate converter stuff.
	m_iInputPending  = 0;
	m_fResampleRatio = 1.0f;
	m_bResample = (m_iSampleRate != m_pFile->sampleRate());
	if (m_bResample) {
		m_fResampleRatio = (float) m_iSampleRate / m_pFile->sampleRate();
		m_ppInBuffer  = new float *     [iChannels];
		m_ppOutBuffer = new float *     [iChannels];
		m_ppSrcState  = new SRC_STATE * [iChannels];
	}
#endif

	// ALlocate time-stretch engine whether needed...
	if (m_bTimeStretch) {
		m_pTimeStretch = new qtractorTimeStretch(iChannels, m_iSampleRate);
		m_pTimeStretch->setTempo(m_fTimeStretch);
		m_pTimeStretch->setQuickSeek(g_bQuickSeek);
	}

	// FIXME: default logical length gets it total...
	if (m_iLength == 0) {
		m_iLength = frames();
		if (m_iOffset < m_iLength)
			m_iLength -= m_iOffset;
		else
			m_iOffset = 0;
	}

	// Allocate ring-buffer now.
	m_pRingBuffer = new qtractorRingBuffer<float> (iChannels, m_iLength);
	m_iThreshold  = (m_pRingBuffer->bufferSize() >> 2);
	m_iBufferSize = m_iThreshold;
#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample && m_fResampleRatio < 1.0f) {
		unsigned int iMinBufferSize = (unsigned int) framesOut(m_iBufferSize);
		while (m_iBufferSize < iMinBufferSize)
			m_iBufferSize <<= 1;
	}
#endif

	// Allocate actual buffer stuff...
	m_ppFrames = new float * [iChannels];
	for (unsigned short i = 0; i < iChannels; i++)
		m_ppFrames[i] = new float [m_iBufferSize];

#ifdef CONFIG_LIBSAMPLERATE
	// Sample rate converter stuff, whether needed...
	if (m_bResample) {
		int err = 0;
		for (unsigned short i = 0; i < iChannels; i++) {
			m_ppInBuffer[i]  = m_ppFrames[i];
			m_ppOutBuffer[i] = new float [m_iBufferSize];
			m_ppSrcState[i]  = src_new(g_iResampleType, 1, &err);
		}
	}
#endif

	// Make it sync-managed...
	m_pSyncThread = new qtractorAudioBufferThread(this);
	m_pSyncThread->start(QThread::HighPriority);

	return true;
}


// Operational buffer terminator.
void qtractorAudioBuffer::close (void)
{
	if (m_pFile == NULL)
		return;

	// Not sync-managed anymore...
	if (m_pSyncThread) {
		if (m_pSyncThread->isRunning()) do {
			m_pSyncThread->setRunState(false);
		//	m_pSyncThread->terminate();
			m_pSyncThread->sync();
		} while (!m_pSyncThread->wait(100));
		delete m_pSyncThread;
		m_pSyncThread = NULL;
	}

	// Write-behind any remains, if applicable...
	if (m_pFile->mode() & qtractorAudioFile::Write)
		writeSync();

	// Time to close it good.
	m_pFile->close();

	// Release internal I/O buffers.
	deleteIOBuffers();

	// Deallocate any buffer stuff...
	if (m_pTimeStretch) {
		delete m_pTimeStretch;
		m_pTimeStretch = NULL;
	}

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
	m_iThreshold   = 0;
	m_iBufferSize  = 0;
	m_bReadSync    = false;
	m_iReadOffset  = 0;
	m_iWriteOffset = 0;
	m_iFileLength  = 0;
	m_bIntegral    = false;

	m_iSeekOffset  = 0;
	m_iSeekPending = 0;
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

	// Check for logical EOF...
	if (m_iReadOffset + iFrames > m_iFileLength)
		iFrames = m_iFileLength - m_iReadOffset;

	// Are we self-contained (ie. got integral file in buffer) and looping?
	unsigned int nframes = iFrames;
	unsigned long ls = m_iLoopStart;
	unsigned long le = m_iLoopEnd;
	// Are we in the middle of the loop range ?
	if (ls < le) {
		if (m_bIntegral) {
			unsigned int ri = m_pRingBuffer->readIndex();
			while (ri < le && ri + nframes >= le) {
				unsigned int nread = le - ri;
				m_pRingBuffer->read(ppFrames, nread, iOffset);
				nframes -= nread;
				iOffset += nread;
				m_pRingBuffer->setReadIndex(ls);
				m_iReadOffset = ls;
			}
			iFrames = nframes;
		} else {
			ls += m_iOffset;
			le += m_iOffset;
			while (le >= m_iReadOffset && m_iReadOffset + nframes >= le) {
				nframes -= (le - m_iReadOffset);
				m_iReadOffset = ls;
			}
		}
	}

	// Move the (remaining) data around...	
	m_pRingBuffer->read(ppFrames, iFrames, iOffset);
	if (m_iReadOffset + nframes > m_iOffset + m_iLength) {
		m_iReadOffset = (ls < le ? ls : m_iOffset);
	} else {
		m_iReadOffset += nframes;
	}

#ifdef DEBUG_0
	dump_state(QString("-read(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (m_pSyncThread && m_pRingBuffer->writable() > m_iThreshold)
		m_pSyncThread->sync();

	return iFrames;
}


// Buffer data write.
int qtractorAudioBuffer::write ( float **ppFrames, unsigned int iFrames,
	unsigned short iChannels )
{
	if (m_pRingBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

	unsigned short iBuffers = m_pRingBuffer->channels();
	if (iChannels < 1)
		iChannels = iBuffers;

#ifdef DEBUG_0
	dump_state(QString("+write(%1)").arg(iFrames));
#endif

	unsigned int nwrite = iFrames;

	if (iChannels == iBuffers) {
		// Direct write...
		nwrite = m_pRingBuffer->write(ppFrames, nwrite);
	} else {
		// Multiplexed write...
		unsigned int ws = m_pRingBuffer->writable();
		if (nwrite > ws)
			nwrite = ws;
		if (nwrite > 0) {
			unsigned int w = m_pRingBuffer->writeIndex();
			unsigned int n, n1, n2;
			unsigned int bs = m_pRingBuffer->bufferSize();
			if (w + nwrite > bs) {
				n1 = (bs - w);
				n2 = (w + nwrite) & m_pRingBuffer->bufferMask();
			} else {
				n1 = nwrite;
				n2 = 0;
			}
			unsigned short i, j;
			float **ppBuffer = m_pRingBuffer->buffer();
			if (iChannels > iBuffers) {
				for (i = 0, j = 0; i < iBuffers; i++, j++) {
					for (n = 0; n < n1; n++)
						ppBuffer[j][n + w] = ppFrames[i][n];
					for (n = 0; n < n2; n++)
						ppBuffer[j][n] = ppFrames[i][n + n1];
				}
				for (j = 0; i < iChannels; i++) {
					for (n = 0; n < n1; n++)
						ppBuffer[j][n + w] += ppFrames[i][n];
					for (n = 0; n < n2; n++)
						ppBuffer[j][n] += ppFrames[i][n + n1];
					if (++j >= iBuffers)
						j = 0;
				}
			} else { // (iChannels < iBuffers)
				i = 0;
				for (j = 0; j < iBuffers; j++) {
					for (n = 0; n < n1; n++)
						ppBuffer[j][n + w] = ppFrames[i][n];
					for (n = 0; n < n2; n++)
						ppBuffer[j][n] = ppFrames[i][n + n1];
					if (++i >= iChannels)
						i = 0;
				}
			}
			m_pRingBuffer->setWriteIndex(w + nwrite);
		}
	}

	// Make it statiscally correct...
	m_iWriteOffset += nwrite;

#ifdef DEBUG_0
	dump_state(QString("-write(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (m_pSyncThread && m_pRingBuffer->readable() > m_iThreshold)
		m_pSyncThread->sync();

	return nwrite;
}


// Special kind of super-read/channel-mix.
int qtractorAudioBuffer::readMix ( float **ppFrames, unsigned int iFrames,
	unsigned short iChannels, unsigned int iOffset, float fGain )
{
	if (m_pRingBuffer == NULL)
		return -1;
	if (m_pFile == NULL)
		return -1;

#ifdef DEBUG_0
	dump_state(QString("+readMix(%1)").arg(iFrames));
#endif

	// Check for logical EOF...
	if (m_iReadOffset + iFrames > m_iFileLength)
		iFrames = m_iFileLength - m_iReadOffset;

	// Are we self-contained (ie. got integral file in buffer) and looping?
	unsigned int nframes = iFrames;
	unsigned long ls = m_iLoopStart;
	unsigned long le = m_iLoopEnd;
	// Are we in the middle of the loop range ?
	if (ls < le) {
		if (m_bIntegral) {
			unsigned int ri = m_pRingBuffer->readIndex();
			while (ri < le && ri + nframes >= le) {
				unsigned int nread = le - ri;
				readMixFrames(ppFrames, nread, iChannels, iOffset, fGain);
				nframes -= nread;
				iOffset += nread;
				m_pRingBuffer->setReadIndex(ls);
				m_iReadOffset = ls;
			}
			iFrames = nframes;
		} else {
			ls += m_iOffset;
			le += m_iOffset;
			while (le >= m_iReadOffset && m_iReadOffset + nframes >= le) {
				nframes -= (le - m_iReadOffset);
				m_iReadOffset = ls;
			}
		}
	}

	// Mix the (remaining) data around...
	readMixFrames(ppFrames, iFrames, iChannels, iOffset, fGain);
	if (m_iReadOffset + nframes > m_iOffset + m_iLength) {
		m_iReadOffset = (ls < le ? ls : m_iOffset);
	} else {
		m_iReadOffset += nframes;
	}

#ifdef DEBUG_0
	dump_state(QString("-readMix(%1)").arg(iFrames));
#endif

	// Time to sync()?
	if (m_pSyncThread && m_pRingBuffer->writable() > m_iThreshold)
		m_pSyncThread->sync();

	return iFrames;
}


// Buffer data seek.
bool qtractorAudioBuffer::seek ( unsigned long iFrame )
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
		if (iFrame >= m_iLength)
			return false;
		m_pRingBuffer->setReadIndex(iFrame);
	//	m_iWriteOffset = iFrame;
		m_iReadOffset  = iFrame;
		return true;
	}

	// Adjust to logical offset...
	iFrame += m_iOffset;
	if (iFrame >= m_iOffset + m_iLength)
		return false;

#ifdef DEBUG
	dump_state(QString(">seek(%1)").arg(iFrame));
#endif

	unsigned int  rs = m_pRingBuffer->readable();
	unsigned int  ri = m_pRingBuffer->readIndex();
	unsigned long ro = m_iReadOffset;

	// Check if target is already cached...
	if (/* m_bReadSync && */ iFrame >= ro && ro + rs >= iFrame) {
		m_pRingBuffer->setReadIndex(ri + iFrame - ro);
	//	m_iWriteOffset += iFrame - ro;
		m_iReadOffset  += iFrame - ro;
		return true;
	}

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorAudioBuffer::seek(%p, %lu) pending(%d, %lu) wo=%lu ro=%lu\n",
		this, iFrame, m_iSeekPending, m_iSeekOffset, m_iWriteOffset, m_iReadOffset);
#endif

	// Bad luck, gotta go straight down to disk...
	//	if (!seekSync(iFrame))
	//		return false;
	m_bReadSync   = false;
	m_iReadOffset = 0;	// Force out-of-sync...
	m_iSeekOffset = iFrame;
	m_iSeekPending++;
	// readSync();
	if (m_pSyncThread)
		m_pSyncThread->sync();

	return true;
}


// Initial thread-sync executive (if file is on read mode,
// check whether it can be cache-loaded integrally).
bool qtractorAudioBuffer::initSync (void)
{
	if (m_pFile == NULL)
		return false;

	// Reset all relevant state variables.
	m_bReadSync    = false;
	m_iReadOffset  = 0;
	m_iWriteOffset = 0;
	m_iFileLength  = 0;
	m_bIntegral    = false;

	m_iSeekOffset  = 0;
	m_iSeekPending = 0;

	// Read-ahead a whole bunch, if applicable...
	if (m_pFile->mode() & qtractorAudioFile::Read) {
		// Set to initial offset...
		m_iSeekOffset = m_iOffset;
		m_iSeekPending++;
		// Initial buffer read in...
		readSync();
		// Check if fitted integrally...
		if (m_iFileLength < m_pRingBuffer->bufferSize() - 1) {
			m_bIntegral = true;
			deleteIOBuffers();
		}
	}

	// Just carry on, if not integrally cached...
	return !m_bIntegral;
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
		do { readSync(); } while (m_iSeekPending > 0);
	if (mode & qtractorAudioFile::Write)
		writeSync();
}


// Audio frame process synchronization predicate method.
bool qtractorAudioBuffer::inSync (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (!m_bReadSync) {
#ifdef CONFIG_DEBUG
		fprintf(stderr, "qtractorAudioBuffer::inSync(%p, %lu, %lu) (%ld)\n",
			this, iFrameStart, iFrameEnd,
			(long) m_iReadOffset - (iFrameStart + m_iOffset));
#endif
		if (m_iReadOffset == iFrameStart + m_iOffset) {
			m_bReadSync = true;
		} else {
			seek(iFrameEnd);
		}
	}

	return m_bReadSync;
}


// Export-mode sync executive.
void qtractorAudioBuffer::syncExport (void)
{
	if (m_pSyncThread) m_pSyncThread->syncExport();
}


// Read-mode sync executive.
void qtractorAudioBuffer::readSync (void)
{
#ifdef DEBUG
	dump_state("+readSync()");
#endif

	// Check whether we have some hard-seek pending...
	if (m_iSeekPending > 0) {
		m_iSeekPending = 0;
		// Do it...
		if (!seekSync(m_iSeekOffset))
			return;
		// Refill the whole buffer....
		m_pRingBuffer->reset();
		// Override with new intended offset...
		m_iWriteOffset = m_iSeekOffset;
		m_iReadOffset  = m_iSeekOffset;
	}

	unsigned int ws = m_pRingBuffer->writable();
	if (ws == 0)
		return;

	unsigned int nahead = ws;
	unsigned int ntotal = 0;

	unsigned long ls = m_iLoopStart + m_iOffset;
	unsigned long le = m_iLoopEnd   + m_iOffset;

	bool bLooping = (ls < le && m_iWriteOffset < le);

	while (nahead > 0) {
		// Adjust request for sane size...
		if (nahead > m_iBufferSize)
			nahead = m_iBufferSize;
		// Check whether behind the loop-end point...
		if (bLooping && m_iWriteOffset + nahead > le)
			nahead = le - m_iWriteOffset;
		// Check whether behind the logical end-of-file...
		if (m_iWriteOffset + nahead > m_iOffset + m_iLength) {
			if (m_iWriteOffset < m_iOffset + m_iLength) {
				nahead = (m_iOffset + m_iLength) - m_iWriteOffset;
			} else {
				nahead = 0;
			}
		}
		// Read the block in...
		int nread = 0;
		if (nahead > 0)
			nread = readBuffer(nahead);
		if (nread > 0) {
			// Another block was read in...
			m_iWriteOffset += nread;
			if (m_iFileLength < m_iWriteOffset)
				m_iFileLength = m_iWriteOffset;
			if (bLooping && m_iWriteOffset >= le && seekSync(ls))
				m_iWriteOffset = ls;
			ntotal += nread;
			nahead = (ws > ntotal ? ws - ntotal : 0);
		}
		else
		if (nread < 0) {
			// Think of end-of-file...
			nahead = 0;
			// But we can re-cache, if not an integral fit...
			if (m_iFileLength >= m_pRingBuffer->bufferSize() - 1) {
				unsigned long offset = (bLooping ? ls : m_iOffset);
				if (seekSync(offset)) {
					m_iWriteOffset = offset;
				}
			}
		}
	}

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

	unsigned int rs = m_pRingBuffer->readable();
	if (rs == 0)
		return;

	unsigned int nwrite;
	unsigned int nbehind = rs;
	unsigned int ntotal  = 0;

	while (nbehind > 0) {
		// Adjust request for sane size...
		if (nbehind > m_iBufferSize)
			nbehind = m_iBufferSize;
		// Read the block out...
		nwrite = writeBuffer(nbehind);
		if (nwrite > 0) {
			// Another block was written out...
			m_iReadOffset += nwrite;
			if (m_iFileLength < m_iReadOffset)
				m_iFileLength = m_iReadOffset;
			ntotal += nwrite;
		}
		// Think of end-of-turn...
		if (nwrite < nbehind) {
			nbehind = 0;
		} else {
			nbehind = rs - ntotal;
		}
	}

#ifdef DEBUG
	dump_state("-writeSync()");
#endif
}


// Internal-seek sync executive.
bool qtractorAudioBuffer::seekSync ( unsigned long iFrame )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorAudioBuffer::seekSync(%p, %lu) pending(%d, %lu) wo=%lu ro=%lu\n",
		this, iFrame, m_iSeekPending, m_iSeekOffset, m_iWriteOffset, m_iReadOffset);
#endif

#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample) {
		m_iInputPending = 0;
		for (unsigned short i = 0; i < m_pRingBuffer->channels(); i++) {
			if (m_ppSrcState && m_ppSrcState[i])
				src_reset(m_ppSrcState[i]);
			m_ppFrames[i] = m_ppInBuffer[i];
		}
	}
#endif

	if (m_pTimeStretch)
		m_pTimeStretch->clear();

	return m_pFile->seek(framesOut(iFrame));
}


// Last-mile frame buffer-helper processor.
int qtractorAudioBuffer::writeFrames (
	float **ppFrames, unsigned int iFrames )
{
	// Time-stretch processing...
	if (m_pTimeStretch) {
		int nread = 0;
		unsigned int nahead = iFrames;
		m_pTimeStretch->putFrames(ppFrames, nahead);
		while (nahead > 0 && nread < (int) iFrames) {
			nahead = m_pTimeStretch->receiveFrames(ppFrames, iFrames - nread);
			if (nahead > 0)
				nread += m_pRingBuffer->write(ppFrames, nahead);
		}
		// Done with time-stretching...
		return nread;
	}

	// Normal straight processing...
	return m_pRingBuffer->write(ppFrames, iFrames);
}


// Flush buffer-helper processor.
int qtractorAudioBuffer::flushFrames ( unsigned int iFrames )
{
	int nread = 0;

	// Flush time-stretch processing...
	if (m_pTimeStretch) {
		unsigned int nahead = iFrames;
		m_pTimeStretch->flushInput();
		while (nahead > 0 && nread < (int) iFrames) {
			nahead = m_pTimeStretch->receiveFrames(m_ppFrames, iFrames - nread);
			if (nahead > 0)
				nread += m_pRingBuffer->write(m_ppFrames, nahead);
		}
	}

	return (nread > 0 ? nread : -1);
}


// I/O buffer read process; return -1 on end-of-file.
int qtractorAudioBuffer::readBuffer ( unsigned int iFrames )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "+readBuffer(%u)\n", iFrames);
#endif

	int nread = 0;

#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample) {

		if (iFrames > m_iInputPending)
			nread = m_pFile->read(m_ppFrames, iFrames - m_iInputPending);

		nread += m_iInputPending;

		if (m_ppOutBuffer == NULL) {
			if (nread > 0)
				return writeFrames(m_ppFrames, nread);
			else
				return flushFrames(iFrames); // Maybe EoF!
		}

		int ngen = 0;
		SRC_DATA src_data;

		for (unsigned short i = 0; i < m_pRingBuffer->channels(); i++) {
			// Fill all resampler parameter data...
			src_data.data_in       = m_ppInBuffer[i];
			src_data.data_out      = m_ppOutBuffer[i];
			src_data.input_frames  = nread;
			src_data.output_frames = iFrames;
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
			nread = writeFrames(m_ppOutBuffer, ngen);
		else
		if (nread < 1)
			nread = flushFrames(iFrames); // Maybe EoF!

	} else {
#endif   // CONFIG_LIBSAMPLERATE

		nread = m_pFile->read(m_ppFrames, iFrames);
		if (nread > 0)
			nread = writeFrames(m_ppFrames, nread);
		else
			nread = flushFrames(iFrames); // Maybe EoF!

#ifdef CONFIG_LIBSAMPLERATE
	}
#endif   // CONFIG_LIBSAMPLERATE

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "-readBuffer(%u) --> nread=%d\n", iFrames, nread);
#endif

	return nread;
}


// I/O buffer write process.
int qtractorAudioBuffer::writeBuffer ( unsigned int iFrames )
{
	int nwrite = m_pRingBuffer->read(m_ppFrames, iFrames, 0);
	if (nwrite > 0)
		nwrite = m_pFile->write(m_ppFrames, nwrite);

	return nwrite;
}


// Special kind of super-read/channel-mix buffer helper.
int qtractorAudioBuffer::readMixFrames (
	float **ppFrames, unsigned int iFrames, unsigned short iChannels,
	unsigned int iOffset, float fGain )
{
	unsigned int rs = m_pRingBuffer->readable();
	if (rs == 0)
		return 0;

	if (iFrames > rs)
		iFrames = rs;

	unsigned int r = m_pRingBuffer->readIndex();

	unsigned int n, n1, n2;
	unsigned int bs = m_pRingBuffer->bufferSize();
	if (r + iFrames > bs) {
		n1 = (bs - r);
		n2 = (r + iFrames) & m_pRingBuffer->bufferMask();
	} else {
		n1 = iFrames;
		n2 = 0;
	}

	unsigned short i, j;
	unsigned short iBuffers = m_pRingBuffer->channels();
	float **ppBuffer = m_pRingBuffer->buffer();
	if (iChannels == iBuffers) {
		for (i = 0; i < iBuffers; i++) {
			for (n = 0; n < n1; n++)
				ppFrames[i][n + iOffset] += fGain * ppBuffer[i][n + r];
			for (n = 0; n < n2; n++)
				ppFrames[i][n + n1 + iOffset] += fGain * ppBuffer[i][n];
		}
	}
	else if (iChannels > iBuffers) {
		j = 0;
		for (i = 0; i < iChannels; i++) {
			for (n = 0; n < n1; n++)
				ppFrames[i][n + iOffset] += fGain * ppBuffer[j][n + r];
			for (n = 0; n < n2; n++)
				ppFrames[i][n + n1 + iOffset] += fGain * ppBuffer[j][n];
			if (++j >= iBuffers)
				j = 0;
		}
	}
	else { // (iChannels < iBuffers)
		i = 0;
		for (j = 0; j < iBuffers; j++) {
			for (n = 0; n < n1; n++)
				ppFrames[i][n + iOffset] += fGain * ppBuffer[j][n + r];
			for (n = 0; n < n2; n++)
				ppFrames[i][n + n1 + iOffset] += fGain * ppBuffer[j][n];
			if (++i >= iChannels)
				i = 0;
		}
	}

	m_pRingBuffer->setReadIndex(r + iFrames);

	return iFrames;
}


// I/O buffer release.
void qtractorAudioBuffer::deleteIOBuffers (void)
{
#ifdef CONFIG_LIBSAMPLERATE
	// Release internal and resampler buffers.
	for (unsigned short i = 0; i < m_pRingBuffer->channels(); i++) {
		if (m_ppSrcState && m_ppSrcState[i])
			m_ppSrcState[i] = src_delete(m_ppSrcState[i]);
		if (m_ppOutBuffer && m_ppOutBuffer[i]) {
			delete [] m_ppOutBuffer[i];
			m_ppOutBuffer[i] = NULL;
		}
		if (m_ppInBuffer && m_ppInBuffer[i]) {
			delete [] m_ppInBuffer[i];
			m_ppInBuffer[i] = NULL;
		}
	}
	if (m_ppSrcState) {
		delete [] m_ppSrcState;
		m_ppSrcState = NULL;
	}
	if (m_ppOutBuffer) {
		delete [] m_ppOutBuffer;
		m_ppOutBuffer = NULL;
	}
	if (m_ppInBuffer) {
		delete [] m_ppInBuffer;
		m_ppInBuffer = NULL;
	}
	m_iInputPending = 0;
#endif

	if (m_ppFrames) {
		delete [] m_ppFrames;
		m_ppFrames = NULL;
	}
}


// Reset this buffers state.
void qtractorAudioBuffer::reset ( bool bLooping )
{
	if (m_pRingBuffer == NULL)
		return;
	if (m_pFile == NULL)
		return;

#ifdef DEBUG
	dump_state("+reset()");
#endif

	unsigned long iFrame = 0;

	// If looping, we'll reset to loop-start point,
	// otherwise it's a buffer full-reset...
	if (bLooping && m_iLoopStart < m_iLoopEnd)
		iFrame = m_iLoopStart;

#if 0
	if (m_bIntegral) {
		m_iReadOffset = m_iOffset + iFrame;
		m_pRingBuffer->setReadIndex(iFrame);
	//	m_pRingBuffer->setWriteIndex(m_iLength);
	} else {
		m_bReadSync   = false;
		m_iReadOffset = 0;	// Force out-of-sync...
		m_iSeekOffset = m_iOffset + iFrame;
		m_iSeekPending++;
		if (m_pSyncThread)
			m_pSyncThread->sync();
	}
#else
	seek(iFrame);
#endif

#ifdef DEBUG
	dump_state("-reset()");
#endif
}


// Frame position converters.
unsigned long qtractorAudioBuffer::framesIn ( unsigned long iFrames ) const
{
#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample)
		iFrames = (unsigned long) ((float) iFrames * m_fResampleRatio);
#endif

	if (m_bTimeStretch)
		iFrames = (unsigned long) ((float) iFrames / m_fTimeStretch);

	return iFrames;
}

unsigned long qtractorAudioBuffer::framesOut ( unsigned long iFrames ) const
{
#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample)
		iFrames = (unsigned long) ((float) iFrames / m_fResampleRatio);
#endif

	if (m_bTimeStretch)
		iFrames = (unsigned long) ((float) iFrames * m_fTimeStretch);

	return iFrames;
}


// Logical clip-offset (in frames from beginning-of-file).
void qtractorAudioBuffer::setOffset ( unsigned long iOffset )
{
	m_iOffset = iOffset;
}

unsigned long qtractorAudioBuffer::offset() const
{
	return m_iOffset;
}


// Logical clip-length (in frames from clip-start/offset).
void qtractorAudioBuffer::setLength ( unsigned long iLength )
{
	m_iLength = iLength;
}

unsigned long qtractorAudioBuffer::length (void) const
{
	return m_iLength;
}


// Current (last known) file length accessor.
unsigned long qtractorAudioBuffer::fileLength (void) const
{
	return m_iFileLength;
}


// Loop points accessors.
void qtractorAudioBuffer::setLoop ( unsigned long iLoopStart,
	unsigned long iLoopEnd )
{
	if (iLoopStart == m_iLoopStart && iLoopEnd == m_iLoopEnd)
		return;

	if (iLoopStart < iLoopEnd) {
		m_iLoopStart = iLoopStart;
		m_iLoopEnd   = iLoopEnd;
	} else {
		m_iLoopStart = 0;
		m_iLoopEnd   = 0;
	}

	m_bReadSync   = false;
	m_iReadOffset = 0;	// Force out-of-sync...
}

unsigned long qtractorAudioBuffer::loopStart (void) const
{
	return m_iLoopStart;
}

unsigned long qtractorAudioBuffer::loopEnd (void) const
{
	return m_iLoopEnd;
}


// Time-stretch factor.
void qtractorAudioBuffer::setTimeStretch ( float fTimeStretch )
{
	m_bTimeStretch = (fTimeStretch > 0.05f && fTimeStretch < 1.0f - 1e-3f)
		|| (fTimeStretch > 1.0f + 1e-3f && fTimeStretch < 5.0f);

	m_fTimeStretch = (m_bTimeStretch ? fTimeStretch : 1.0f);
}

float qtractorAudioBuffer::timeStretch (void) const
{
	return m_fTimeStretch;
}

bool qtractorAudioBuffer::isTimeStretch (void) const
{
	return m_bTimeStretch;
}


// Sample-rate converter type (global option).
int qtractorAudioBuffer::g_iResampleType = 2;	// SRC_SINC_FASTEST;

void qtractorAudioBuffer::setResampleType ( int iResampleType )
{
	g_iResampleType = iResampleType;
}

int qtractorAudioBuffer::resampleType (void)
{
	return g_iResampleType;
}


// Time stretch quick-seek mode (global option).
bool qtractorAudioBuffer::g_bQuickSeek = false;

void qtractorAudioBuffer::setQuickSeek ( bool bQuickSeek )
{
	g_bQuickSeek = bQuickSeek;
}

bool qtractorAudioBuffer::isQuickSeek (void)
{
	return g_bQuickSeek;
}


#ifdef DEBUG
void qtractorAudioBuffer::dump_state ( const QString& sPrefix ) const
{
	unsigned int  rs  = m_pRingBuffer->readable();
	unsigned int  ws  = m_pRingBuffer->writable();
	unsigned int  ri  = m_pRingBuffer->readIndex();
	unsigned int  wi  = m_pRingBuffer->writeIndex();
	unsigned long wo  = m_iWriteOffset;
	unsigned long ro  = m_iReadOffset;
	unsigned long ofs = m_iOffset;
	unsigned long len = m_iLength;

	fprintf(stderr, "%s rs=%u ws=%u ri=%u wi=%u wo=%lu ro=%lu ofs=%lu len=%lu\n",
		sPrefix.toUtf8().constData(), rs, ws, ri, wi, wo, ro, ofs, len);
}
#endif


//----------------------------------------------------------------------
// class qtractorAudioBufferThread -- Ring-cache manager thread.
//

// Constructor.
qtractorAudioBufferThread::qtractorAudioBufferThread (
	qtractorAudioBuffer *pAudioBuffer ) : QThread()
{
	m_pAudioBuffer = pAudioBuffer;
	m_bRunState    = false;
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


// Wake from executive wait condition (RT-safe).
void qtractorAudioBufferThread::sync (void)
{
	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef CONFIG_DEBUG_0
	else fprintf(stderr, "qtractorAudioBufferThread::sync(%p): tryLock() failed.\n", this);
#endif
}


// Bypass executive wait condition (non RT-safe).
void qtractorAudioBufferThread::syncExport (void)
{
	m_mutex.lock();
	m_pAudioBuffer->sync();
	m_mutex.unlock();
}


// Thread run executive.
void qtractorAudioBufferThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorAudioBufferThread::run(%p): started.\n", this);
#endif

	m_mutex.lock();
	m_bRunState = m_pAudioBuffer->initSync();
	while (m_bRunState) {
		// Do whatever we must, then wait for more...
		m_pAudioBuffer->sync();
		m_cond.wait(&m_mutex);
	}
	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorAudioBufferThread::run(%p): stopped.\n", this);
#endif
}


// end of qtractorAudioBuffer.cpp
