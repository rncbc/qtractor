// qtractorAudioBuffer.cpp
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioPeak.h"

#include "qtractorTimeStretcher.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"

#include <math.h>


// Glitch, click, pop-free ramp length (in frames).
#define QTRACTOR_RAMP_LENGTH	32


//----------------------------------------------------------------------
// class qtractorAudioBufferThread -- Ring-cache manager thread.
//

// Constructor.
qtractorAudioBufferThread::qtractorAudioBufferThread (
	unsigned int iSyncSize ) : QThread()
{
	m_iSyncSize = (4 << 1);
	while (m_iSyncSize < iSyncSize)
		m_iSyncSize <<= 1;
	m_iSyncMask = (m_iSyncSize - 1);
	m_ppSyncItems = new qtractorAudioBuffer * [m_iSyncSize];
	m_iSyncRead   = 0;
	m_iSyncWrite  = 0;

	m_bRunState = false;
}

// Destructor.
qtractorAudioBufferThread::~qtractorAudioBufferThread (void)
{
	if (isRunning()) do {
		setRunState(false);
	//	terminate();
		sync();
	} while (!wait(100));

	delete [] m_ppSyncItems;
}

// Run state accessor.
void qtractorAudioBufferThread::setRunState ( bool bRunState )
{
	QMutexLocker locker(&m_mutex);

	m_bRunState = bRunState;
}

bool qtractorAudioBufferThread::runState (void) const
{
	return m_bRunState;
}


// Wake from executive wait condition (RT-safe).
void qtractorAudioBufferThread::sync ( qtractorAudioBuffer *pAudioBuffer )
{
	if (pAudioBuffer == NULL) {
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			m_ppSyncItems[r]->setSyncFlag(qtractorAudioBuffer::WaitSync, false);
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
	} else {
		// !pAudioBuffer->isSyncFlag(qtractorAudioBuffer::WaitSync)
		unsigned int n;
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		if (w > r) {
			n = ((r - w + m_iSyncSize) & m_iSyncMask) - 1;
		} else if (r > w) {
			n = (r - w) - 1;
		} else {
			n = m_iSyncSize - 1;
		}
		if (n > 0) {
			pAudioBuffer->setSyncFlag(qtractorAudioBuffer::WaitSync);
			m_ppSyncItems[w] = pAudioBuffer;
			m_iSyncWrite = (w + 1) & m_iSyncMask;
		}
	}

	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef CONFIG_DEBUG_0
	else qDebug("qtractorAudioBufferThread[%p]::sync(): tryLock() failed.", this);
#endif
}


// Bypass executive wait condition (non RT-safe).
void qtractorAudioBufferThread::syncExport (void)
{
	QMutexLocker locker(&m_mutex);

	process();
}


// Thread run executive.
void qtractorAudioBufferThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioBufferThread[%p]::run(): started.", this);
#endif

	m_mutex.lock();

	m_bRunState = true;

	while (m_bRunState) {
		// Do whatever we must, then wait for more...
		process();
		// Wait for sync...
		m_cond.wait(&m_mutex);
	}

	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioBufferThread[%p]::run(): stopped.", this);
#endif
}


// Thread run executive.
void qtractorAudioBufferThread::process (void)
{
	unsigned int r = m_iSyncRead;
	unsigned int w = m_iSyncWrite;

	while (r != w) {
		m_ppSyncItems[r]->sync();
		++r &= m_iSyncMask;
		w = m_iSyncWrite;
	}

	m_iSyncRead = r;
}


// Conditional resize check.
void qtractorAudioBufferThread::checkSyncSize ( unsigned int iSyncSize )
{
	if (iSyncSize > (m_iSyncSize - 4)) {
		QMutexLocker locker(&m_mutex);
		unsigned int iNewSyncSize = (m_iSyncSize << 1);
		while (iNewSyncSize < iSyncSize)
			iNewSyncSize <<= 1;
		qtractorAudioBuffer **ppNewSyncItems
			= new qtractorAudioBuffer * [iNewSyncSize];
		qtractorAudioBuffer **ppOldSyncItems = m_ppSyncItems;
		::memcpy(ppNewSyncItems, ppOldSyncItems,
			m_iSyncSize * sizeof(qtractorAudioBuffer *));
		m_iSyncSize = iNewSyncSize;
		m_iSyncMask = (iNewSyncSize - 1);
		m_ppSyncItems = ppNewSyncItems;
		delete [] ppOldSyncItems;
	}
}


//----------------------------------------------------------------------
// class qtractorAudioBuffer -- Ring buffer/cache method implementation.
//

// Constructors.
qtractorAudioBuffer::qtractorAudioBuffer (
	qtractorAudioBufferThread *pSyncThread, unsigned short iChannels )
{
	m_pSyncThread    = pSyncThread;

	m_iChannels      = iChannels;

	m_pFile          = NULL;

	m_pRingBuffer    = NULL;

	m_iThreshold     = 0;
	m_iBufferSize    = 0;

	m_syncFlags      = 0;

	m_iReadOffset    = 0;
	m_iWriteOffset   = 0;
	m_iFileLength    = 0;
	m_bIntegral      = false;

	m_iOffset        = 0;
	m_iLength        = 0;

	m_iLoopStart     = 0;
	m_iLoopEnd       = 0;

	m_iSeekOffset    = 0;

	ATOMIC_SET(&m_seekPending, 0);

	m_ppFrames       = NULL;
	m_ppBuffer       = NULL;

	m_bTimeStretch   = false;
	m_fTimeStretch   = 1.0f;

	m_bPitchShift    = false;
	m_fPitchShift    = 1.0f;

	m_pTimeStretcher = NULL;

	m_fGain          = 1.0f;
	m_fPanning       = 0.0f;

	m_pfGains        = NULL;

	m_fNextGain      = 0.0f;
	m_iRampGain      = 0;

#ifdef CONFIG_LIBSAMPLERATE
	m_bResample      = false;
	m_fResampleRatio = 1.0f;
	m_iInputPending  = 0;
	m_ppInBuffer     = NULL;
	m_ppOutBuffer    = NULL;
	m_ppSrcState     = NULL;
#endif

	m_pPeakFile      = NULL;

	// Time-stretch mode local options.
	m_bWsolaTimeStretch = g_bDefaultWsolaTimeStretch;
	m_bWsolaQuickSeek   = g_bDefaultWsolaQuickSeek;

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


// Operational properties.
unsigned int qtractorAudioBuffer::bufferSize (void) const
{
	return m_iBufferSize;
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

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	const unsigned int iSampleRate = pSession->sampleRate();

	// Get proper file type class...
	m_pFile = qtractorAudioFileFactory::createAudioFile(
		sFilename, m_iChannels, iSampleRate);
	if (m_pFile == NULL)
		return false;

	// Go open it...
	if (!m_pFile->open(sFilename, iMode)) {
		delete m_pFile;
		m_pFile = NULL;
		return false;
	}

	// Check samplerate and how many channels there really are.
	const unsigned short iBuffers = m_pFile->channels();

	// Just one more sanity check...
	if (iBuffers < 1 || m_pFile->sampleRate() < 1) {
		m_pFile->close();
		delete m_pFile;
		m_pFile = NULL;
		return false;
	}

#ifdef CONFIG_LIBSAMPLERATE
	// Compute sample rate converter stuff.
	m_iInputPending  = 0;
	m_fResampleRatio = 1.0f;
	m_bResample = (iSampleRate != m_pFile->sampleRate());
	if (m_bResample) {
		m_fResampleRatio = float(iSampleRate) / float(m_pFile->sampleRate());
		m_ppInBuffer  = new float *     [iBuffers];
		m_ppOutBuffer = new float *     [iBuffers];
		m_ppSrcState  = new SRC_STATE * [iBuffers];
	}
#endif

	// FIXME: default logical length gets it total...
	if (m_iLength == 0) {
		m_iLength = frames();
		if (m_iOffset < m_iLength)
			m_iLength -= m_iOffset;
		else
			m_iOffset = 0;
	}

	// Allocate ring-buffer now.
	unsigned int iBufferSize = m_iLength;
	if (iBufferSize == 0)
		iBufferSize = (iSampleRate >> 1);
	else
	if (iBufferSize > (iSampleRate << 2))
		iBufferSize = (iSampleRate << 2);

	m_pRingBuffer = new qtractorRingBuffer<float> (iBuffers, iBufferSize);
	m_iThreshold  = (m_pRingBuffer->bufferSize() >> 2);
	m_iBufferSize = (m_iThreshold >> 2);

#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample && m_fResampleRatio < 1.0f) {
		iBufferSize = (unsigned int) framesOut(m_iBufferSize);
		while (m_iBufferSize < iBufferSize)
			m_iBufferSize <<= 1;
	}
#endif

	// Allocate actual buffer stuff...
	unsigned short i;

	m_ppFrames = new float * [iBuffers];
	for (i = 0; i < iBuffers; ++i)
		m_ppFrames[i] = new float [m_iBufferSize];

	// Allocate time-stretch engine whether needed...
	if (m_bTimeStretch || m_bPitchShift) {
		unsigned int iFlags = qtractorTimeStretcher::None;
		if (m_bWsolaTimeStretch)
			iFlags |= qtractorTimeStretcher::WsolaTimeStretch;
		if (m_bWsolaQuickSeek)
			iFlags |= qtractorTimeStretcher::WsolaQuickSeek;
		m_pTimeStretcher = new qtractorTimeStretcher(iBuffers, iSampleRate,
			m_fTimeStretch, m_fPitchShift, iFlags, m_iBufferSize);
	}

#ifdef CONFIG_LIBSAMPLERATE
	// Sample rate converter stuff, whether needed...
	if (m_bResample) {
		int err = 0;
		for (i = 0; i < iBuffers; ++i) {
			m_ppInBuffer[i]  = m_ppFrames[i];
			m_ppOutBuffer[i] = new float [m_iBufferSize];
			m_ppSrcState[i]  = src_new(g_iDefaultResampleType, 1, &err);
		}
	}
#endif

	// Consider it done when recording...
	if (m_pFile->mode() & qtractorAudioFile::Write) {
		setSyncFlag(InitSync);
	} else {
		// Get a reasonablebuffer size for readMix()...
		iBufferSize = pSession->audioEngine()->bufferSize();
		if (iBufferSize < (m_iBufferSize >> 2))
			iBufferSize = (m_iBufferSize >> 2);
		// Allocate those minimal buffers for readMix()...
		m_ppBuffer = new float * [iBuffers];
		for (i = 0; i < iBuffers; ++i)
			m_ppBuffer[i] = new float [iBufferSize];
	}

	// Rebuild the whole panning-gain array...
	m_pfGains = new float [iBuffers];

	// (Re)compute stereo-panning/balance gains...
	float afGains[2] = { 1.0f, 1.0f };
	const float fPan = 0.5f * (1.0f + m_fPanning);
	if (fPan < 0.499f || fPan > 0.501f) {
		afGains[0] = ::cosf(fPan * M_PI_2);
		afGains[1] = ::sinf(fPan * M_PI_2);
    }

	// Apply to multi-channel gain array (paired fashion)...
	const unsigned short k = (iBuffers - (iBuffers & 1));
	for (i = 0 ; i < k; ++i)
		m_pfGains[i] = afGains[i & 1];
	for ( ; i < iBuffers; ++i)
		m_pfGains[i] = 1.0f;

	// Make it sync-managed...
	if (m_pSyncThread)
		m_pSyncThread->sync(this);
	
	return true;
}


// Operational buffer terminator.
void qtractorAudioBuffer::close (void)
{
	if (m_pFile == NULL)
		return;

	// Wait for regular file close...
	if (m_pSyncThread) {
		setSyncFlag(CloseSync);
		m_pSyncThread->sync(this);
		do QThread::yieldCurrentThread();
		while (isSyncFlag(CloseSync));
	}

	// Delete old panning-gains holders...
	if (m_pfGains) {
		delete [] m_pfGains;
		m_pfGains = NULL;
	}

	// Take careof remains, if applicable...
	if (m_pFile->mode() & qtractorAudioFile::Write) {
		// Close on-the-fly peak file, if applicable...
		if (m_pPeakFile) {
			m_pPeakFile->closeWrite();
			m_pPeakFile = NULL;
		}
	}


	// Deallocate any buffer stuff...
	if (m_pTimeStretcher) {
		delete m_pTimeStretcher;
		m_pTimeStretcher = NULL;
	}

	// Release internal I/O buffers.
	if (m_ppBuffer && m_pRingBuffer) {
		const unsigned short iBuffers = m_pRingBuffer->channels();
		for (unsigned short i = 0; i < iBuffers; ++i) {
			if (m_ppBuffer[i]) {
				delete [] m_ppBuffer[i];
				m_ppBuffer[i] = NULL;
			}
		}
		delete [] m_ppBuffer;
		m_ppBuffer = NULL;
	}

	if (m_pRingBuffer) {
		deleteIOBuffers();
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

	m_syncFlags    = 0;

	m_iReadOffset  = 0;
	m_iWriteOffset = 0;
	m_iFileLength  = 0;
	m_bIntegral    = false;

	m_iSeekOffset  = 0;

	ATOMIC_SET(&m_seekPending, 0);

	m_fNextGain = 0.0f;
	m_iRampGain = 0;

	m_pPeakFile = NULL;
}


// Buffer data read.
int qtractorAudioBuffer::read ( float **ppFrames, unsigned int iFrames,
	unsigned int iOffset )
{
	if (m_pRingBuffer == NULL)
		return -1;

	int nread;

	unsigned long ro = m_iReadOffset;
	unsigned long ls = m_iLoopStart;
	unsigned long le = m_iLoopEnd;

	// Are we off decoded file limits (EoS)?
	if (ro >= m_iFileLength) {
		nread = iFrames;
		if (ls < le) {
			if (m_bIntegral) {
				const unsigned int ri = m_pRingBuffer->readIndex();
				while (ri < le && ri + nread >= le) {
					nread -= le - ri;
					ro = m_iOffset + ls;
					m_pRingBuffer->setReadIndex(ls);
				}
			} else {
				ls += m_iOffset;
				le += m_iOffset;
				while (ro < le && ro + nread >= le) {
					nread -= le - ro;
					ro = ls;
				}
			}
		}
		m_iReadOffset = (ro + nread);
		// Force out-of-sync...
		setSyncFlag(ReadSync, false);
		return nread;
	}

	// Are we in the middle of the loop range ?
	if (ls < le) {
		if (m_bIntegral) {
			const unsigned int ri = m_pRingBuffer->readIndex();
			while (ri < le && ri + iFrames >= le) {
				nread = m_pRingBuffer->read(ppFrames, le - ri, iOffset);
				iFrames -= nread;
				iOffset += nread;
				ro = m_iOffset + ls;
				m_pRingBuffer->setReadIndex(ls);
			}
		} else {
			ls += m_iOffset;
			le += m_iOffset;
			while (ro < le && ro + iFrames >= le) {
				nread = m_pRingBuffer->read(ppFrames, le - ro, iOffset);
				iFrames -= nread;
				iOffset += nread;
				ro = ls;
			}
		}
	}

	// Move the (remaining) data around...
	nread = m_pRingBuffer->read(ppFrames, iFrames, iOffset);
	m_iReadOffset = (ro + nread);
	if (m_iReadOffset >= m_iOffset + m_iLength) {
		// Force out-of-sync...
		setSyncFlag(ReadSync, false);
	}

	// Time to sync()?
	if (!m_bIntegral &&
		m_pSyncThread && m_pRingBuffer->writable() > m_iThreshold)
		m_pSyncThread->sync(this);

	return nread;
}


// Buffer data write.
int qtractorAudioBuffer::write ( float **ppFrames, unsigned int iFrames,
	unsigned short iChannels, unsigned int iOffset )
{
	if (m_pRingBuffer == NULL)
		return -1;

	const unsigned short iBuffers = m_pRingBuffer->channels();
	if (iChannels < 1)
		iChannels = iBuffers;

	unsigned int nwrite = iFrames;

	if (iChannels == iBuffers) {
		// Direct write...
		nwrite = m_pRingBuffer->write(ppFrames, nwrite, iOffset);
	} else {
		// Multiplexed write...
		const unsigned int ws = m_pRingBuffer->writable();
		if (nwrite > ws)
			nwrite = ws;
		if (nwrite > 0) {
			const unsigned int w = m_pRingBuffer->writeIndex();
			const unsigned int bs = m_pRingBuffer->bufferSize();
			unsigned int n, n1, n2;
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
				for (i = 0, j = 0; i < iBuffers; ++i, ++j) {
					for (n = 0; n < n1; ++n)
						ppBuffer[j][n + w] = ppFrames[i][n] + iOffset;
					for (n = 0; n < n2; ++n)
						ppBuffer[j][n] = ppFrames[i][n + n1] + iOffset;
				}
				for (j = 0; i < iChannels; ++i) {
					for (n = 0; n < n1; ++n)
						ppBuffer[j][n + w] += ppFrames[i][n] + iOffset;
					for (n = 0; n < n2; ++n)
						ppBuffer[j][n] += ppFrames[i][n + n1] + iOffset;
					if (++j >= iBuffers)
						j = 0;
				}
			} else { // (iChannels < iBuffers)
				i = 0;
				for (j = 0; j < iBuffers; ++j) {
					for (n = 0; n < n1; ++n)
						ppBuffer[j][n + w] = ppFrames[i][n] + iOffset;
					for (n = 0; n < n2; ++n)
						ppBuffer[j][n] = ppFrames[i][n + n1] + iOffset;
					if (++i >= iChannels)
						i = 0;
				}
			}
			m_pRingBuffer->setWriteIndex(w + nwrite);
		}
	}

	// Make it statiscally correct...
	m_iWriteOffset += nwrite;

	// Time to sync()?
	if (m_pSyncThread && m_pRingBuffer->readable() > m_iThreshold)
		m_pSyncThread->sync(this);

	return nwrite;
}


// Special kind of super-read/channel-mix.
int qtractorAudioBuffer::readMix ( float **ppFrames, unsigned int iFrames,
	unsigned short iChannels, unsigned int iOffset, float fGain )
{
	if (m_pRingBuffer == NULL)
		return -1;

	int nread = iFrames;

	unsigned long ro = m_iReadOffset;
	unsigned long ls = m_iLoopStart;
	unsigned long le = m_iLoopEnd;

	// Are we off decoded file limits (EoS)?
	if (ro >= m_iFileLength) {
		if (ls < le) {
			if (m_bIntegral) {
				const unsigned int ri = m_pRingBuffer->readIndex();
				while (ri < le && ri + nread >= le && nread > 0) {
					nread -= le - ri;
					ro = m_iOffset + ls;
					m_pRingBuffer->setReadIndex(ls);
				}
			} else {
				ls += m_iOffset;
				le += m_iOffset;
				while (ro < le && ro + nread >= le && nread > 0) {
					nread -= le - ro;
					ro = ls;
				}
			}
		}
		m_iReadOffset = (ro + nread);
		// Force out-of-sync...
		setSyncFlag(ReadSync, false);
		return nread;
	}

	// Are we in the middle of the loop range ?
	if (ls < le) {
		if (m_bIntegral) {
			const unsigned int ri = m_pRingBuffer->readIndex();
			while (ri < le && ri + iFrames >= le && nread > 0) {
				m_iRampGain = -1;
				nread = readMixFrames(ppFrames, le - ri, iChannels, iOffset, fGain);
				iFrames -= nread;
				iOffset += nread;
				ro = m_iOffset + ls;
				m_pRingBuffer->setReadIndex(ls);
			}
		} else {
			ls += m_iOffset;
			le += m_iOffset;
			while (le >= ro && ro + iFrames >= le && nread > 0) {
				m_iRampGain = -1;
				nread = readMixFrames(ppFrames, le - ro, iChannels, iOffset, fGain);
				iFrames -= nread;
				iOffset += nread;
				ro = ls;
			}
		}
	}

	// Take care of end-of-stream...
	const unsigned long re = m_iOffset + m_iLength;
	if (ro + iFrames >= re)
		m_iRampGain = -1;

	// Mix the (remaining) data around...
	nread = readMixFrames(ppFrames, iFrames, iChannels, iOffset, fGain);
	m_iReadOffset = (ro + nread);
	if (m_iReadOffset >= re) {
		// Force out-of-sync...
		setSyncFlag(ReadSync, false);
	}

	// Time to sync()?
	if (!m_bIntegral &&
		m_pSyncThread && m_pRingBuffer->writable() > m_iThreshold)
		m_pSyncThread->sync(this);

	return nread;
}


// Buffer data seek.
bool qtractorAudioBuffer::seek ( unsigned long iFrame )
{
	if (m_pRingBuffer == NULL)
		return false;

	// Seek is only valid on read-only mode.
	if (m_pFile == NULL)
		return false;
	if (m_pFile->mode() & qtractorAudioFile::Write)
		return false;

	// Must not break while on initial sync...
	if (!isSyncFlag(InitSync))
		return false;

	// Reset running gain...
	m_fNextGain = 0.0f;
	m_iRampGain = (m_iOffset == 0 && iFrame == 0 ? 0 : 1);

	// Are we off-limits?
	if (iFrame >= m_iLength)
		return false;

	// Force (premature) out-of-sync...
	setSyncFlag(ReadSync, false);
	setSyncFlag(WaitSync, false);

	// Special case on integral cached files...
	if (m_bIntegral) {
		m_pRingBuffer->setReadIndex(iFrame);
	//	m_iWriteOffset = m_iOffset + iFrame;
		m_iReadOffset  = m_iOffset + iFrame;
		// Maybe (always) in-sync...
		//setSyncFlag(ReadSync);
		return true;
	}

	// Adjust to logical offset...
	iFrame += m_iOffset;

	// Check if target is already cached...
	const unsigned int  rs = m_pRingBuffer->readable();
	const unsigned int  ri = m_pRingBuffer->readIndex();
	const unsigned long ro = m_iReadOffset;
	if (iFrame >= ro && iFrame < ro + rs) {
		m_pRingBuffer->setReadIndex(ri + iFrame - ro);
	//	m_iWriteOffset += iFrame - ro;
		m_iReadOffset   = iFrame;
		// Maybe (late) in-sync...
		//setSyncFlag(ReadSync);
		return true;
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioBuffer[%p]::seek(%lu) pending(%d, %lu) wo=%lu ro=%lu",
		this, iFrame, ATOMIC_GET(&m_seekPending), m_iSeekOffset,
		m_iWriteOffset, m_iReadOffset);
#endif

	// Bad luck, gotta go straight down to disk...
	//	if (!seekSync(iFrame))
	//		return false;
	// Force (late) out-of-sync...
	m_iReadOffset = m_iOffset + m_iLength + 1; // An unlikely offset!
	m_iSeekOffset = iFrame;

	ATOMIC_INC(&m_seekPending);

	// readSync();
	if (m_pSyncThread)
		m_pSyncThread->sync(this);

	return true;
}


// Sync thread state flags accessors (ought to be inline?).
void qtractorAudioBuffer::setSyncFlag ( SyncFlag flag, bool bOn )
{
	if (bOn)
		m_syncFlags |=  (unsigned char) (flag);
	else
		m_syncFlags &= ~(unsigned char) (flag);
}

bool qtractorAudioBuffer::isSyncFlag ( SyncFlag flag ) const
{
	return (m_syncFlags & (unsigned char) (flag));
}


// Initial thread-sync executive (if file is on read mode,
// check whether it can be cache-loaded integrally).
void qtractorAudioBuffer::initSync (void)
{
	if (m_pRingBuffer == NULL)
		return;

	// Initialization is only valid on read-only mode.
	if (m_pFile == NULL)
		return;

	if (m_pFile->mode() & qtractorAudioFile::Write)
		return;
#if 0
	if (isSyncFlag(CloseSync))
		return;
#endif
	// Reset all relevant state variables.
	setSyncFlag(ReadSync, false);

	m_iReadOffset  = 0;
	m_iWriteOffset = 0;
	m_iFileLength  = 0;
	m_bIntegral    = false;

	m_iSeekOffset  = 0;

	ATOMIC_SET(&m_seekPending, 0);

	// Reset running gain...
	m_fNextGain = 0.0f;
	m_iRampGain = (m_iOffset == 0 ? 0 : 1);

	// Set to initial offset...
	m_iSeekOffset = m_iOffset;

	ATOMIC_INC(&m_seekPending);

	// Initial buffer read in...
	readSync();

	// We're mostly done with initialization...
	// m_bInitSync = true;
	setSyncFlag(InitSync);

	// Check if fitted integrally...
	if (m_iFileLength < m_iOffset + m_pRingBuffer->bufferSize() - 1) {
		m_bIntegral = true;
		deleteIOBuffers();
	}
	else // Re-sync if loop falls short in initial area... 
	if (m_iLoopStart < m_iLoopEnd
		&& m_iWriteOffset >= m_iOffset + m_iLoopEnd) {
		// Will do it again, but now we're
		// sure we aren't fit integral...
		m_iSeekOffset = m_iOffset;
		ATOMIC_INC(&m_seekPending);
		// Initial buffer re-read in...
		readSync();
	}

	// Make sure we're not closing anymore,
	// of course, don't be ridiculous...
	setSyncFlag(CloseSync, false);
}


// Base-mode sync executive.
void qtractorAudioBuffer::sync (void)
{
	if (m_pFile == NULL)
		return;

	if (!isSyncFlag(WaitSync))
		return;

	if (!isSyncFlag(InitSync)) {
		initSync();
		setSyncFlag(WaitSync, false);
	} else {
		setSyncFlag(WaitSync, false);
		const int mode = m_pFile->mode();
		if (mode & qtractorAudioFile::Read)
			readSync();
		else
		if (mode & qtractorAudioFile::Write)
			writeSync();
		if (isSyncFlag(CloseSync)) {
			m_pFile->close();
			setSyncFlag(CloseSync, false);
		}
	}
}


// Audio frame process synchronization predicate method.
bool qtractorAudioBuffer::inSync (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (!isSyncFlag(InitSync))
		return false;

	if (isSyncFlag(ReadSync))
		return true;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioBuffer[%p]::inSync(%lu, %lu) (%ld)",
		this, iFrameStart, iFrameEnd,
		long(m_iReadOffset) - long(iFrameStart + m_iOffset));
#endif

	if (m_iReadOffset == iFrameStart + m_iOffset) {
		setSyncFlag(ReadSync);
		return true;
	}

	seek(iFrameEnd);
	return false;
}


// Export-mode sync executive.
void qtractorAudioBuffer::syncExport (void)
{
	if (m_pSyncThread) m_pSyncThread->syncExport();
}


// Read-mode sync executive.
void qtractorAudioBuffer::readSync (void)
{
	if (m_pRingBuffer == NULL)
		return;

	if (isSyncFlag(CloseSync))
		return;

	// Check whether we have some hard-seek pending...
	if (ATOMIC_TAZ(&m_seekPending)) {
		// Do it...
		if (!seekSync(m_iSeekOffset))
			return;
		// Refill the whole buffer....
		m_pRingBuffer->reset();
		// Override with new intended offset...
		m_iWriteOffset = m_iSeekOffset;
		m_iReadOffset  = m_iSeekOffset;
	}

	const unsigned int ws = m_pRingBuffer->writable();
	if (ws == 0)
		return;

	unsigned int nahead = ws;
	unsigned int ntotal = 0;

	while (nahead > 0 && !ATOMIC_GET(&m_seekPending)) {
		// Take looping into account, if any...
		const unsigned long ls = m_iOffset + m_iLoopStart;
		const unsigned long le = m_iOffset + m_iLoopEnd;
		const bool bLooping
			= (ls < le && m_iWriteOffset < le && isSyncFlag(InitSync));
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
		// (assume end-of-file)
		int nread = -1;
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
			if (m_iFileLength >= m_iOffset + m_pRingBuffer->bufferSize() - 1) {
				const unsigned long offset = (bLooping ? ls : m_iOffset);
				if (seekSync(offset))
					m_iWriteOffset = offset;
			}
		}
	}
}


// Write-mode sync executive.
void qtractorAudioBuffer::writeSync (void)
{
	if (m_pRingBuffer == NULL)
		return;

	const unsigned int rs = m_pRingBuffer->readable();
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
}


// Internal-seek sync executive.
bool qtractorAudioBuffer::seekSync ( unsigned long iFrame )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioBuffer[%p]::seekSync(%lu) pending(%d, %lu) wo=%lu ro=%lu",
		this, iFrame, ATOMIC_GET(&m_seekPending), m_iSeekOffset,
		m_iWriteOffset, m_iReadOffset);
#endif

#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample) {
		m_iInputPending = 0;
		const unsigned short iBuffers = m_pRingBuffer->channels();
		for (unsigned short i = 0; i < iBuffers; ++i) {
			if (m_ppSrcState && m_ppSrcState[i])
				src_reset(m_ppSrcState[i]);
			m_ppInBuffer[i] = m_ppFrames[i];
		}
	}
#endif

	if (m_pTimeStretcher)
		m_pTimeStretcher->reset();

	return m_pFile->seek(framesOut(iFrame));
}


// Last-mile frame buffer-helper processor.
int qtractorAudioBuffer::writeFrames (
	float **ppFrames, unsigned int iFrames )
{
	// Time-stretch processing...
	if (m_pTimeStretcher) {
		int nread = 0;
		m_pTimeStretcher->process(ppFrames, iFrames);
		unsigned int nwrite = m_pRingBuffer->writable();
		unsigned int nahead = m_pTimeStretcher->available();
		while (nahead > 0) {
			if (nahead > m_iBufferSize)
				nahead = m_iBufferSize;
			if (nahead > nwrite)
				nahead = nwrite;
			if (nahead > 0)
				nahead = m_pTimeStretcher->retrieve(ppFrames, nahead);
			if (nahead > 0) {
				nread += m_pRingBuffer->write(ppFrames, nahead);
				nwrite = m_pRingBuffer->writable();
				nahead = m_pTimeStretcher->available();
			}
		}
		// Done with time-stretching...
		return nread;
	}

	// Normal straight processing...
	return m_pRingBuffer->write(ppFrames, iFrames);
}


// Flush buffer-helper processor.
int qtractorAudioBuffer::flushFrames ( float **ppFrames, unsigned int iFrames )
{
	int nread = 0;

	// Flush time-stretch processing...
	if (m_pTimeStretcher) {
		m_pTimeStretcher->flush();
		unsigned int nwrite = m_pRingBuffer->writable();
		unsigned int nahead = m_pTimeStretcher->available();
		while (nahead > 0) {
			if (nahead > m_iBufferSize)
				nahead = m_iBufferSize;
			if (nahead > nwrite)
				nahead = nwrite;
			if (nahead > 0)
				nahead = m_pTimeStretcher->retrieve(ppFrames, nahead);
			if (nahead > 0) {
				nread += m_pRingBuffer->write(ppFrames, nahead);
				nwrite = m_pRingBuffer->writable();
				nahead = m_pTimeStretcher->available();
			}
		}
	}

	// Zero-flush till known end-of-clip (avoid sure drifting)...
	if ((nread < 1) && (m_iWriteOffset + iFrames > m_iOffset + m_iLength)) {
		const unsigned int nahead = (m_iOffset + m_iLength) - m_iWriteOffset;
		const unsigned short iBuffers = m_pRingBuffer->channels();
		for (unsigned short i = 0; i < iBuffers; ++i)
			::memset(ppFrames[i], 0, nahead * sizeof(float));
		nread += m_pRingBuffer->write(ppFrames, nahead);
	}

	return (nread > 0 ? nread : -1);
}


// I/O buffer read process; return -1 on end-of-file.
int qtractorAudioBuffer::readBuffer ( unsigned int iFrames )
{
#ifdef CONFIG_DEBUG_0
	qDebug("+readBuffer(%u)", iFrames);
#endif

	int nread = 0;

#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample) {

		if (iFrames > m_iInputPending)
			nread = m_pFile->read(m_ppInBuffer, iFrames - m_iInputPending);

		nread += m_iInputPending;

		int ngen = 0;
		SRC_DATA src_data;

		const unsigned short iBuffers = m_pRingBuffer->channels();

		for (unsigned short i = 0; i < iBuffers; ++i) {
			// Fill all resampler parameter data...
			src_data.data_in       = m_ppFrames[i];
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
					::memmove(m_ppFrames[i],
						m_ppFrames[i] + src_data.input_frames_used,
						m_iInputPending * sizeof(float));
				}
				m_ppInBuffer[i] = m_ppFrames[i] + m_iInputPending;
			}
		}

		if (ngen > 0)
			nread = writeFrames(m_ppOutBuffer, ngen);
		else
		if (nread < 1)
			nread = flushFrames(m_ppFrames, iFrames); // Maybe EoF!

	} else {
#endif   // CONFIG_LIBSAMPLERATE

		nread = m_pFile->read(m_ppFrames, iFrames);
		if (nread > 0)
			nread = writeFrames(m_ppFrames, nread);
		else
			nread = flushFrames(m_ppFrames, iFrames); // Maybe EoF!

#ifdef CONFIG_LIBSAMPLERATE
	}
#endif   // CONFIG_LIBSAMPLERATE

#ifdef CONFIG_DEBUG_0
	qDebug("-readBuffer(%u) --> nread=%d", iFrames, nread);
#endif

	return nread;
}


// I/O buffer write process.
int qtractorAudioBuffer::writeBuffer ( unsigned int iFrames )
{
	int nwrite = m_pRingBuffer->read(m_ppFrames, iFrames, 0);
	if (nwrite > 0) {
		nwrite = m_pFile->write(m_ppFrames, nwrite);
		if (m_pPeakFile)
			nwrite = m_pPeakFile->write(m_ppFrames, nwrite);
	}

	return nwrite;
}


// Special kind of super-read/channel-mix buffer helper.
int qtractorAudioBuffer::readMixFrames (
	float **ppFrames, unsigned int iFrames, unsigned short iChannels,
	unsigned int iOffset, float fGain )
{
	if (iFrames == 0)
		return 0;

	const int nread = m_pRingBuffer->read(m_ppBuffer, iFrames);
	if (nread == 0)
		return 0;

	const unsigned short iBuffers = m_pRingBuffer->channels();

	unsigned short i, j; int n;
	float fGainIter, fGainStep1, fGainStep2;
	float *pFrames, *pBuffer;

	// HACK: Case of clip ramp in/out-set in this run...
	if (m_iRampGain) {
		const unsigned int nramp
			= (nread < QTRACTOR_RAMP_LENGTH ? nread : QTRACTOR_RAMP_LENGTH);
		const int n0 = (m_iRampGain < 0 ? nread - nramp : nramp);
		const int n1 = (m_iRampGain < 0 ? n0 : 0);
		const int n2 = (m_iRampGain < 0 ? nread : n0);
		fGainStep1 = float(m_iRampGain) / float(nramp);
		for (i = 0; i < iBuffers; ++i) {
			fGainIter = (m_iRampGain < 0 ? 1.0f : 0.0f);
			pBuffer = m_ppBuffer[i] + n1;
			for (n = n1; n < n2; ++n, fGainIter += fGainStep1)
				*pBuffer++ *= fGainIter;
		}
		m_iRampGain = (m_iRampGain < 0 ? 1 : 0);
	//	fPrevGain = fGain;
	}

	// Reset running gain...
	const float fNextGain = m_fGain * fGain;
	const float fPrevGain = (m_fNextGain < 1E-9f ? fNextGain : m_fNextGain);
	m_fNextGain = fNextGain;
	fGainStep1 = (m_fNextGain - fPrevGain) / float(nread);

	if (iChannels == iBuffers) {
		for (i = 0; i < iBuffers; ++i) {
			pFrames = ppFrames[i] + iOffset;
			pBuffer = m_ppBuffer[i];
			fGainIter = fPrevGain * m_pfGains[i];
			fGainStep2 = fGainStep1 * m_pfGains[i];
			for (n = 0; n < nread; ++n, fGainIter += fGainStep2)
				*pFrames++ += fGainIter * *pBuffer++;
		}
	}
	else if (iChannels > iBuffers) {
		j = 0;
		for (i = 0; i < iChannels; ++i) {
			pFrames = ppFrames[i] + iOffset;
			pBuffer = m_ppBuffer[j];
			fGainIter = fPrevGain * m_pfGains[j];
			fGainStep2 = fGainStep1 * m_pfGains[j];
			for (n = 0; n < nread; ++n, fGainIter += fGainStep2)
				*pFrames++ += fGainIter * *pBuffer++;
			if (++j >= iBuffers)
				j = 0;
		}
	}
	else { // (iChannels < iBuffers)
		i = 0;
		for (j = 0; j < iBuffers; ++j) {
			pFrames = ppFrames[i] + iOffset;
			pBuffer = m_ppBuffer[j];
			fGainIter = fPrevGain * m_pfGains[j];
			fGainStep2 = fGainStep1 * m_pfGains[j];
			for (n = 0; n < nread; ++n, fGainIter += fGainStep2)
				*pFrames++ += fGainIter * *pBuffer++;
			if (++i >= iChannels)
				i = 0;
		}
	}

	return nread;
}


// I/O buffer release.
void qtractorAudioBuffer::deleteIOBuffers (void)
{
	const unsigned short iBuffers = m_pRingBuffer->channels();

	unsigned short i;

#ifdef CONFIG_LIBSAMPLERATE
	// Release internal and resampler buffers.
	for (i = 0; i < iBuffers; ++i) {
		if (m_ppSrcState && m_ppSrcState[i])
			m_ppSrcState[i] = src_delete(m_ppSrcState[i]);
		if (m_ppOutBuffer && m_ppOutBuffer[i]) {
			delete [] m_ppOutBuffer[i];
			m_ppOutBuffer[i] = NULL;
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
		for (i = 0; i < iBuffers; ++i) {
			if (m_ppFrames[i]) {
				delete [] m_ppFrames[i];
				m_ppFrames[i] = NULL;
			}
		}
		delete [] m_ppFrames;
		m_ppFrames = NULL;
	}
}


// Reset this buffers state.
void qtractorAudioBuffer::reset ( bool bLooping )
{
	if (m_pRingBuffer == NULL)
		return;

	unsigned long iFrame = 0;

	// If looping, we'll reset to loop-start point,
	// otherwise it's a buffer full-reset...
	if (bLooping && m_iLoopStart < m_iLoopEnd) {
		iFrame = m_iLoopStart;
		// Make sure we're not already there...
		// (force out-of-sync with an unlikely offset!)
		if (!m_bIntegral)
			m_iReadOffset = m_iOffset + m_iLength + 1;
	}

	seek(iFrame);
}


// Frame position converters.
unsigned long qtractorAudioBuffer::framesIn ( unsigned long iFrames ) const
{
#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample)
		iFrames = (unsigned long) (float(iFrames) * m_fResampleRatio);
#endif

	if (m_bTimeStretch)
		iFrames = (unsigned long) (float(iFrames) * m_fTimeStretch);

	return iFrames;
}

unsigned long qtractorAudioBuffer::framesOut ( unsigned long iFrames ) const
{
#ifdef CONFIG_LIBSAMPLERATE
	if (m_bResample)
		iFrames = (unsigned long) (float(iFrames) / m_fResampleRatio);
#endif

	if (m_bTimeStretch)
		iFrames = (unsigned long) (float(iFrames) / m_fTimeStretch);

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


// Local gain/panning accessors.
void qtractorAudioBuffer::setGain ( float fGain )
{
	m_fGain = fGain;
}

float qtractorAudioBuffer::gain (void) const
{
	return m_fGain;
}


void qtractorAudioBuffer::setPanning ( float fPanning )
{
	m_fPanning = fPanning;
}

float qtractorAudioBuffer::panning (void) const
{
	return m_fPanning;
}


float qtractorAudioBuffer::channelGain ( unsigned short i ) const
{
	return m_pfGains[i];
}


// Loop points accessors.
void qtractorAudioBuffer::setLoop (
	unsigned long iLoopStart, unsigned long iLoopEnd )
{
	// Buffer-looping magic check!
	if (iLoopStart < iLoopEnd) {
		m_iLoopStart = iLoopStart;
		m_iLoopEnd   = iLoopEnd;
	} else {
		m_iLoopStart = 0;
		m_iLoopEnd   = 0;
	}

	// Force out-of-sync...
	setSyncFlag(ReadSync, false);
	m_iReadOffset = m_iOffset + m_iLength + 1; // An unlikely offset!
//	seek(m_iReadOffset - m_iOffset);
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
	if (fTimeStretch < 0.1f)
		fTimeStretch = 0.1f;
	else
	if (fTimeStretch > 10.0f)
		fTimeStretch = 10.0f;

	m_bTimeStretch = (fTimeStretch < 1.0f - 1e-3f || fTimeStretch > 1.0f + 1e-3f);
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


// Pitch-shift factor.
void qtractorAudioBuffer::setPitchShift ( float fPitchShift )
{
	if (fPitchShift < 0.1f)
		fPitchShift = 0.1f;
	else
	if (fPitchShift > 10.0f)
		fPitchShift = 10.0f;

	m_bPitchShift = (fPitchShift < 1.0f - 1e-3f || fPitchShift > 1.0f + 1e-3f);
	m_fPitchShift = (m_bPitchShift ? fPitchShift : 1.0f);
}

float qtractorAudioBuffer::pitchShift (void) const
{
	return m_fPitchShift;
}

bool qtractorAudioBuffer::isPitchShift (void) const
{
	return m_bPitchShift;
}


// Internal peak descriptor accessors.
void qtractorAudioBuffer::setPeakFile ( qtractorAudioPeakFile *pPeakFile )
{
	m_pPeakFile = pPeakFile;

 	// Reset for building the peak file on-the-fly...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL
		|| !m_pPeakFile->openWrite(m_iChannels, pSession->sampleRate()))
		m_pPeakFile = NULL;
}

qtractorAudioPeakFile *qtractorAudioBuffer::peakFile (void) const
{
	return m_pPeakFile;
}


// WSOLA time-stretch modes (local options).
void qtractorAudioBuffer::setWsolaTimeStretch ( bool bWsolaTimeStretch )
{
	m_bWsolaTimeStretch = bWsolaTimeStretch;
}

bool qtractorAudioBuffer::isWsolaTimeStretch (void) const
{
	return m_bWsolaTimeStretch;
}


void qtractorAudioBuffer::setWsolaQuickSeek ( bool bWsolaQuickSeek )
{
	m_bWsolaQuickSeek = bWsolaQuickSeek;
}

bool qtractorAudioBuffer::isWsolaQuickSeek (void) const
{
	return m_bWsolaQuickSeek;
}


// Sample-rate converter type (global option).
int qtractorAudioBuffer::g_iDefaultResampleType = 2;	// SRC_SINC_FASTEST;

void qtractorAudioBuffer::setDefaultResampleType ( int iResampleType )
{
	g_iDefaultResampleType = iResampleType;
}

int qtractorAudioBuffer::defaultResampleType (void)
{
	return g_iDefaultResampleType;
}


// WSOLA time-stretch modes (global options).
bool qtractorAudioBuffer::g_bDefaultWsolaTimeStretch = true;
bool qtractorAudioBuffer::g_bDefaultWsolaQuickSeek   = false;

void qtractorAudioBuffer::setDefaultWsolaTimeStretch ( bool bWsolaTimeStretch )
{
	g_bDefaultWsolaTimeStretch = bWsolaTimeStretch;
}

bool qtractorAudioBuffer::isDefaultWsolaTimeStretch (void)
{
	return g_bDefaultWsolaTimeStretch;
}


void qtractorAudioBuffer::setDefaultWsolaQuickSeek ( bool bWsolaQuickSeek )
{
	g_bDefaultWsolaQuickSeek = bWsolaQuickSeek;
}

bool qtractorAudioBuffer::isDefaultWsolaQuickSeek (void)
{
	return g_bDefaultWsolaQuickSeek;
}


// end of qtractorAudioBuffer.cpp
