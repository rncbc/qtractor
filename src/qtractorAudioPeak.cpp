// qtractorAudioPeak.cpp
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
#include "qtractorAudioPeak.h"
#include "qtractorAudioFile.h"
#include "qtractorAudioEngine.h"

#include "qtractorSession.h"

#include <QApplication>
#include <QFileInfo>
#include <QDir>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <QDateTime>

#include <math.h>


// Audio file buffer size in frames per channel.
static const unsigned int c_iAudioFrames = (32 * 1024);

// Peak file buffer size in frames per channel.
static const unsigned int c_iPeakFrames = (8 * 1024);

// Default peak period as a digest representation in frames per channel.
static const unsigned short c_iPeakPeriod = 1024;

// Default peak filename extension.
static const QString c_sPeakFileExt = ".peak";


//----------------------------------------------------------------------
// class qtractorAudioPeakThread -- Audio Peak file thread.
//

class qtractorAudioPeakThread : public QThread
{
public:

	// Constructor.
	qtractorAudioPeakThread(unsigned int iSyncSize = 128);
	// Destructor.
	~qtractorAudioPeakThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition.
	void sync(qtractorAudioPeakFile *pPeakFile = NULL);

protected:

	// The main thread executive.
	void run();

	// Actual peak file creation methods.
	// (this is just about to be used internally)
	bool openPeakFile();
	bool writePeakFile();
	void closePeakFile();

	void notifyPeakEvent() const;

private:

	// The peak file queue instance reference.
	unsigned int            m_iSyncSize;
	unsigned int            m_iSyncMask;
	qtractorAudioPeakFile **m_ppSyncItems;

	volatile unsigned int   m_iSyncRead;
	volatile unsigned int   m_iSyncWrite;

	// Whether the thread is logically running.
	volatile bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;

	// Current audio peak file instance.
	qtractorAudioPeakFile *m_pPeakFile;

	// Current audio file instance.
	qtractorAudioFile *m_pAudioFile;

	// Current audio file buffer.
	float **m_ppAudioFrames;
};


// Constructor.
qtractorAudioPeakThread::qtractorAudioPeakThread ( unsigned int iSyncSize )
{
	m_iSyncSize = (64 << 1);
	while (m_iSyncSize < iSyncSize)
		m_iSyncSize <<= 1;
	m_iSyncMask = (m_iSyncSize - 1);
	m_ppSyncItems = new qtractorAudioPeakFile * [m_iSyncSize];
	m_iSyncRead   = 0;
	m_iSyncWrite  = 0;

	::memset(m_ppSyncItems, 0, m_iSyncSize * sizeof(qtractorAudioPeakFile *));

	m_bRunState = false;

	m_pPeakFile  = NULL;
	m_pAudioFile = NULL;
	m_ppAudioFrames = NULL;
}


// Destructor.
qtractorAudioPeakThread::~qtractorAudioPeakThread (void)
{
	delete [] m_ppSyncItems;
}


// Run state accessor.
void qtractorAudioPeakThread::setRunState ( bool bRunState )
{
//	QMutexLocker locker(&m_mutex);

	m_bRunState = bRunState;
}

bool qtractorAudioPeakThread::runState (void) const
{
	return m_bRunState;
}


// Wake from executive wait condition.
void qtractorAudioPeakThread::sync ( qtractorAudioPeakFile *pPeakFile )
{
	if (pPeakFile == NULL) {
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			qtractorAudioPeakFile *pSyncItem = m_ppSyncItems[r];
			if (pSyncItem)
				pSyncItem->setWaitSync(false);
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
	//	m_iSyncRead = r;
	} else {
		// !pPeakFile->isWaitSync()
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
			pPeakFile->setWaitSync(true);
			m_ppSyncItems[w] = pPeakFile;
			m_iSyncWrite = (w + 1) & m_iSyncMask;
		}
	}

	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef CONFIG_DEBUG_0
	else qDebug("qtractorAudioPeakThread[%p]::sync(): tryLock() failed.", this);
#endif
}


// The main thread executive cycle.
void qtractorAudioPeakThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakThread[%p]::run(): started...", this);
#endif

	m_mutex.lock();

	m_bRunState = true;

	while (m_bRunState) {
		// Do whatever we must, then wait for more...
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (m_bRunState && r != w) {
			m_pPeakFile = m_ppSyncItems[r];
			if (m_pPeakFile && m_pPeakFile->isWaitSync()) {
				if (openPeakFile()) {
					// Go ahead with the whole bunch...
					while (writePeakFile());
					// We're done.
					closePeakFile();
				}
				m_pPeakFile->setWaitSync(false);
				m_pPeakFile = NULL;
			}
			m_ppSyncItems[r] = NULL;
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
		// Are we still in the game?
		if (m_bRunState) {
			// Send notification event, anyway...
			notifyPeakEvent();
			// Wait for sync...
			m_cond.wait(&m_mutex);
		}
	}

	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakThread[%p]::run(): stopped.\n", this);
#endif
}


// Open the peak file for create.
bool qtractorAudioPeakThread::openPeakFile (void)
{
	m_pAudioFile
		= qtractorAudioFileFactory::createAudioFile(m_pPeakFile->filename());
	if (m_pAudioFile == NULL)
		return false;

	if (!m_pAudioFile->open(m_pPeakFile->filename())) {
		delete m_pAudioFile;
		m_pAudioFile = NULL;
		return false;
	}

	const unsigned short iChannels = m_pAudioFile->channels();
	const unsigned int iSampleRate = m_pAudioFile->sampleRate();

	if (!m_pPeakFile->openWrite(iChannels, iSampleRate)) {
		delete m_pAudioFile;
		m_pAudioFile = NULL;
		return false;
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakThread::openPeakFile(%p)", m_pPeakFile);
#endif

	// Allocate audio file frame buffer
	// and peak period accumulators.
	m_ppAudioFrames = new float* [iChannels];
	for (unsigned short i = 0; i < iChannels; ++i)
		m_ppAudioFrames[i] = new float [c_iAudioFrames];

	// Make sure audio file decoder makes no head-start...
	m_pAudioFile->seek(0);

	return true;
}


// Create the peak file chunk.
bool qtractorAudioPeakThread::writePeakFile (void)
{
	if (!m_bRunState)
		return false;

	if (m_ppAudioFrames == NULL)
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakThread::writePeakFile(%p)", m_pPeakFile);
#endif

	// Read another bunch of frames from the physical audio file...
	int nread = m_pAudioFile->read(m_ppAudioFrames, c_iAudioFrames);
	if (nread > 0)
		nread = m_pPeakFile->write(m_ppAudioFrames, nread);

	return (nread > 0);
}


// Close the (hopefully) created peak file.
void qtractorAudioPeakThread::closePeakFile (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakThread::closePeakFile(%p)", m_pPeakFile);
#endif

	// Always force target file close.
	m_pPeakFile->closeWrite();

	// Get rid of physical used stuff.
	if (m_ppAudioFrames) {
		const unsigned short iChannels = m_pAudioFile->channels();
		for (unsigned short k = 0; k < iChannels; ++k)
			delete [] m_ppAudioFrames[k];
		delete [] m_ppAudioFrames;
		m_ppAudioFrames = NULL;
	}

	// Finally the source file too.
	if (m_pAudioFile) {
		delete m_pAudioFile;
		m_pAudioFile = NULL;
	}

	// Send notification event, someway...
	notifyPeakEvent();
}


// Send notification event, someway...
void qtractorAudioPeakThread::notifyPeakEvent (void) const
{
	if (!m_bRunState)
		return;

	qtractorAudioPeakFactory *pPeakFactory
		= qtractorAudioPeakFactory::getInstance();
	if (pPeakFactory)
		pPeakFactory->notifyPeakEvent();
}


//----------------------------------------------------------------------
// class qtractorAudioPeakFile -- Audio peak file (ref'counted)
//

// Constructor.
qtractorAudioPeakFile::qtractorAudioPeakFile (
	const QString& sFilename, float fTimeStretch )
{
	// Initialize instance variables.
	m_sFilename    = sFilename;
	m_fTimeStretch = fTimeStretch;

	m_openMode = None;

	m_peakHeader.period   = 0;
	m_peakHeader.channels = 0;

	m_pBuffer      = NULL;
	m_iBuffSize    = 0;
	m_iBuffLength  = 0;
	m_iBuffOffset  = 0;

	m_bWaitSync = false;

	m_iRefCount = 0;

	m_pWriter = NULL;

	// Set (unique) peak filename...
	QDir dir;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		dir.setPath(pSession->sessionDir());

	const QFileInfo fileInfo(sFilename);
	const QString& sPeakFilePrefix
		= QFileInfo(dir, fileInfo.fileName()).filePath();
	const QString& sPeakName = peakName(sFilename, fTimeStretch);
	const QFileInfo peakInfo(sPeakFilePrefix + '_'
		+ QString::number(qHash(sPeakName), 16)
		+ c_sPeakFileExt);

	m_peakFile.setFileName(peakInfo.absoluteFilePath());
}


// Default destructor.
qtractorAudioPeakFile::~qtractorAudioPeakFile (void)
{
	cleanup();
}


// Open an existing peak file cache.
bool qtractorAudioPeakFile::openRead (void)
{
	// If it's already open, just tell the news.
	if (m_openMode != None)
		return true;

	// Are we still waiting for its creation?
	if (m_bWaitSync)
		return false;

	// Need some preliminary file information...
	QFileInfo fileInfo(m_sFilename);
	QFileInfo peakInfo(m_peakFile.fileName());
	// Have we a peak file up-to-date,
	// or must the peak file be (re)created?
	if (!peakInfo.exists() || peakInfo.created() < fileInfo.created()) {
	//	|| peakInfo.lastModified() < fileInfo.lastModified()) {
		qtractorAudioPeakFactory *pPeakFactory
			= qtractorAudioPeakFactory::getInstance();
		if (pPeakFactory)
			pPeakFactory->sync(this);
		// Think again...
		return false;
	}

	// Make things critical...
	QMutexLocker locker(&m_mutex);

	// Just open and go ahead with first bunch...
	if (!m_peakFile.open(QIODevice::ReadOnly))
		return false;

	if (m_peakFile.read((char *) &m_peakHeader, sizeof(Header))
			!= qint64(sizeof(Header))) {
		m_peakFile.close();
		return false;
	}

	// Set open mode...
	m_openMode = Read;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakFile[%p]::openRead() ---", this);
	qDebug("name        = %s", m_peakFile.fileName().toUtf8().constData());
	qDebug("filename    = %s", m_sFilename.toUtf8().constData());
	qDebug("timeStretch = %g", m_fTimeStretch);
	qDebug("header      = %lu", sizeof(Header));
	qDebug("frame       = %lu", sizeof(Frame));
	qDebug("period      = %d", m_peakHeader.period);
	qDebug("channels    = %d", m_peakHeader.channels);
	qDebug("---");
#endif

	// Its a certain success...
	return true;
}


// Free all attended resources for this peak file.
void qtractorAudioPeakFile::closeRead (void)
{
	// Make things critical...
	QMutexLocker locker(&m_mutex);

	// Close file.
	if (m_openMode == Read) {
		m_peakFile.close();
		m_openMode = None;
	}

	if (m_pBuffer)
		delete [] m_pBuffer;
	m_pBuffer = NULL;

	m_iBuffSize   = 0;
	m_iBuffLength = 0;
	m_iBuffOffset = 0;
}


// Audio properties accessors.
const QString& qtractorAudioPeakFile::filename (void) const
{
	return m_sFilename;
}

float qtractorAudioPeakFile::timeStretch (void) const
{
	return m_fTimeStretch;
}


QString qtractorAudioPeakFile::peakName (void) const
{
	return peakName(m_sFilename, m_fTimeStretch);
}


// Peak cache properties accessors.
QString qtractorAudioPeakFile::name (void) const
{
	return m_peakFile.fileName();
}

unsigned short qtractorAudioPeakFile::period (void)
{
	return m_peakHeader.period;
}

unsigned short qtractorAudioPeakFile::channels (void)
{
	return m_peakHeader.channels;
}


// Read frames from peak file.
qtractorAudioPeakFile::Frame *qtractorAudioPeakFile::read (
	unsigned long iPeakOffset, unsigned int iPeakLength )
{
	// Must be open for something...
	if (m_openMode == None)
		return NULL;

	// Make things critical...
	QMutexLocker locker(&m_mutex);

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakFile[%p]::read(%lu, %u) [%lu, %u, %u]", this,
		iPeakOffset, iPeakLength, m_iBuffOffset, m_iBuffLength, m_iBuffSize);
#endif

	// Cache effect, only valid if we're really reading...
	const unsigned long iPeakEnd = iPeakOffset + iPeakLength;
	if (iPeakOffset >= m_iBuffOffset && m_iBuffOffset < iPeakEnd) {
		const unsigned long iBuffEnd = m_iBuffOffset + m_iBuffLength;
		const unsigned long iBuffOffset
			= m_peakHeader.channels * (iPeakOffset - m_iBuffOffset);
		if (iBuffEnd >= iPeakEnd)
			return m_pBuffer + iBuffOffset;
		if (m_iBuffOffset + m_iBuffSize >= iPeakEnd) {
			m_iBuffLength
				+= readBuffer(m_iBuffLength, iBuffEnd, iPeakEnd - iBuffEnd);
			return m_pBuffer + iBuffOffset;
		}
	}

	// Read peak data as requested...
	m_iBuffLength = readBuffer(0, iPeakOffset, iPeakLength);
	m_iBuffOffset = iPeakOffset;

	return m_pBuffer;
}


// Read frames from peak file into local buffer cache.
unsigned int qtractorAudioPeakFile::readBuffer (
	unsigned int iBuffOffset, unsigned long iPeakOffset, unsigned int iPeakLength )
{
	const unsigned int nsize = m_peakHeader.channels * sizeof(Frame);

	// Shall we reallocate?
	if (iBuffOffset + iPeakLength > m_iBuffSize) {
		Frame *pOldBuffer = m_pBuffer;
		m_iBuffSize += (iPeakLength << 1);
		m_pBuffer = new Frame [m_peakHeader.channels * m_iBuffSize];
		if (pOldBuffer) {
			::memcpy(m_pBuffer, pOldBuffer, m_iBuffLength * nsize);
			delete [] pOldBuffer;
		}
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakFile[%p]::readBuffer(%u, %lu, %u) [%lu, %u, %u]",
		this, iBuffOffset, iPeakOffset, iPeakLength,
		m_iBuffOffset, m_iBuffLength, m_iBuffSize);
#endif

	// Grab new contents from peak file...
	char *pBuffer = (char *) (m_pBuffer + m_peakHeader.channels * iBuffOffset);
	const unsigned long iOffset	= iPeakOffset * nsize;
	const unsigned int iLength  = iPeakLength * nsize;

	int nread = 0;
	if (m_peakFile.seek(sizeof(Header) + iOffset))
		nread = int(m_peakFile.read(&pBuffer[0], iLength));

	// Zero the remaining...
	if (nread < int(iLength))
		::memset(&pBuffer[nread], 0, iLength - nread);

	return nread / nsize;
}


// Open an new peak file for writing.
bool qtractorAudioPeakFile::openWrite (
	unsigned short iChannels, unsigned int iSampleRate )
{
	// We need the master peak period reference.
	qtractorAudioPeakFactory *pPeakFactory
		= qtractorAudioPeakFactory::getInstance();
	if (pPeakFactory == NULL)
		return false;

	// If it's already open, just tell the news.
	if (m_openMode == Write)
		return true;

	// Make things critical...
	QMutexLocker locker(&m_mutex);

	// We'll force (re)open if already reading (duh?)
	if (m_openMode == Read) {
		m_peakFile.close();
		m_openMode = None;
	}

	// Just open and go ahead with it...
	if (!m_peakFile.open(QIODevice::ReadWrite | QIODevice::Truncate))
		return false;

	// Set open mode...
	m_openMode = Write;

	// Initialize header...
	m_peakHeader.period   = pPeakFactory->peakPeriod();
	m_peakHeader.channels = iChannels;

	// Write peak file header.
	if (m_peakFile.write((const char *) &m_peakHeader, sizeof(Header))
			!= qint64(sizeof(Header))) {
		m_peakFile.close();
		return false;
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakFile[%p]::openWrite() ---", this);
	qDebug("name        = %s", m_peakFile.fileName().toUtf8().constData());
	qDebug("filename    = %s", m_sFilename.toUtf8().constData());
	qDebug("timeStretch = %g", m_fTimeStretch);
	qDebug("header      = %u", sizeof(Header));
	qDebug("frame       = %u", sizeof(Frame));
	qDebug("period      = %d", m_peakHeader.period);
	qDebug("channels    = %d", m_peakHeader.channels);
	qDebug("---");
#endif

	//	Go ahead, it's already open.
	m_pWriter = new Writer();

	m_pWriter->offset = 0;

	m_pWriter->amax = new float [m_peakHeader.channels];
	m_pWriter->amin = new float [m_peakHeader.channels];
	m_pWriter->arms = new float [m_peakHeader.channels];
	for (unsigned short i = 0; i < m_peakHeader.channels; ++i)
		m_pWriter->amax[i] = m_pWriter->amin[i] = m_pWriter->arms[i] = 0.0f;

	// Get resample/timestretch-aware internal peak period ratio...
	m_pWriter->period_p = iSampleRate;
	qtractorAudioEngine *pAudioEngine = NULL;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) pAudioEngine = pSession->audioEngine();
	if (pAudioEngine) {
		const unsigned int num = (iSampleRate * m_peakHeader.period);
		const unsigned int den = (unsigned int)
			::rintf(float(pAudioEngine->sampleRate() * m_fTimeStretch));
		m_pWriter->period_q = (num / den);
		m_pWriter->period_r = (num % den);
		if (m_pWriter->period_r > 0) {
			m_pWriter->period_r += m_peakHeader.period;
			m_pWriter->period_r /= m_peakHeader.period;
		}
	} else {
		m_pWriter->period_q = m_peakHeader.period;
		m_pWriter->period_r = 0;
	}

	// Start counting for peak generator and writer...
	m_pWriter->nframe = 0;
	m_pWriter->npeak  = 0;
	m_pWriter->nread  = 0;
	m_pWriter->nwrite = m_pWriter->period_q;

	// It's a certain success...
	return true;
}


// Close the (hopefully) created peak file.
void qtractorAudioPeakFile::closeWrite (void)
{
	// Make things critical...
	QMutexLocker locker(&m_mutex);

	// Flush and close...
	if (m_openMode == Write) {
		if (m_pWriter && m_pWriter->npeak > 0)
			writeFrame();
		m_peakFile.close();
		m_openMode = None;
	}

	if (m_pWriter) {
		delete [] m_pWriter->amax;
		delete [] m_pWriter->amin;
		delete [] m_pWriter->arms;
		delete m_pWriter;
		m_pWriter = NULL;
	}
}


// Write peak from audio frame methods.
int qtractorAudioPeakFile::write (
	float **ppAudioFrames, unsigned int iAudioFrames )
{
	// Make things critical...
	QMutexLocker locker(&m_mutex);

	// We should be actually open for writing...
	if (m_openMode != Write || m_pWriter == NULL)
		return 0;

	for (unsigned int n = 0; n < iAudioFrames; ++n) {
		// Accumulate for this sample frame...
		for (unsigned short k = 0; k < m_peakHeader.channels; ++k) {
			const float fSample = ppAudioFrames[k][n];
			if (m_pWriter->amax[k] < fSample || m_pWriter->npeak == 0)
				m_pWriter->amax[k] = fSample;
			if (m_pWriter->amin[k] > fSample || m_pWriter->npeak == 0)
				m_pWriter->amin[k] = fSample;
			m_pWriter->arms[k] += (fSample * fSample);
		}
		// Count peak frames (incremental)...
		++m_pWriter->npeak;
		// Have we reached the peak accumulative period?
		if (++m_pWriter->nread >= m_pWriter->nwrite) {
			// Estimate next stop...
			m_pWriter->nwrite += m_pWriter->period_q;
			// Apply rounding fraction, if any...
			if (m_pWriter->period_r > 0) {
				m_pWriter->nframe += m_pWriter->period_q;
				if (m_pWriter->nframe >= m_pWriter->period_p) {
					m_pWriter->nwrite += m_pWriter->period_r;
					m_pWriter->nframe  = 0;
				}
			}
			// Write peak frame out.
			writeFrame();
			// We'll reset counter.
			m_pWriter->npeak = 0;
		}
	}

	return iAudioFrames;
}


static inline unsigned char unormf ( const float x )
{
	const int i = int(255.0f * x);
	return (i > 255 ? 255 : i);
}


void qtractorAudioPeakFile::writeFrame (void)
{
	if (m_pWriter == NULL)
		return;

	if (!m_peakFile.seek(sizeof(Header) + m_pWriter->offset))
		return;

	Frame frame;
	for (unsigned short k = 0; k < m_peakHeader.channels; ++k) {
		// Write the denormalized peak values...
		float& fmax = m_pWriter->amax[k];
		float& fmin = m_pWriter->amin[k];
		float& frms = m_pWriter->arms[k];
		frame.max = unormf(::fabsf(fmax));
		frame.min = unormf(::fabsf(fmin));
		frame.rms = unormf(::sqrtf(frms / float(m_pWriter->npeak)));
		// Reset peak period accumulators...
		fmax = fmin = frms = 0.0f;
		// Bail out?...
		m_pWriter->offset += m_peakFile.write((const char *) &frame, sizeof(Frame));
	}
}


// Reference count methods.
void qtractorAudioPeakFile::addRef (void)
{
	++m_iRefCount;
}

void qtractorAudioPeakFile::removeRef (void)
{
	if (--m_iRefCount == 0)
		cleanup();
}


// Physical removal.
void qtractorAudioPeakFile::remove (void)
{
	m_peakFile.remove();
}


// Clean/close method.
void qtractorAudioPeakFile::cleanup ( bool bAutoRemove )
{
	// Check if it's aborting (ought to be atomic)...
	const bool bAborted = (m_bWaitSync || bAutoRemove);
	m_bWaitSync = false;

	// Close the file, anyway now.
	closeWrite();
	closeRead();

	// Physically remove the file if aborted...
	if (bAborted)
		remove();
}


// Sync thread state flags accessors.
void qtractorAudioPeakFile::setWaitSync ( bool bWaitSync )
{
	m_bWaitSync = bWaitSync;
}

bool qtractorAudioPeakFile::isWaitSync (void) const
{
	return m_bWaitSync;
}


// Peak filename standard.
QString qtractorAudioPeakFile::peakName (
	const QString& sFilename, float fTimeStretch )
{
	return sFilename + '_' + QString::number(fTimeStretch);
}


//----------------------------------------------------------------------
// class qtractorAudioPeak -- Audio Peak file pseudo-cache.
//

// Constructor.
qtractorAudioPeak::qtractorAudioPeak ( qtractorAudioPeakFile *pPeakFile )
	: m_pPeakFile(pPeakFile), m_pPeakFrames(NULL),
		m_iPeakLength(0), m_iPeakHash(0)
{
	m_pPeakFile->addRef();
}


// Copy contructor.
qtractorAudioPeak::qtractorAudioPeak ( const qtractorAudioPeak& peak )
	: m_pPeakFile(peak.m_pPeakFile), m_pPeakFrames(NULL),
		m_iPeakLength(0), m_iPeakHash(0)
{
	m_pPeakFile->addRef();
}


// Default destructor.
qtractorAudioPeak::~qtractorAudioPeak (void)
{
	m_pPeakFile->removeRef();

	if (m_pPeakFrames)
		delete [] m_pPeakFrames;
}


// Peak frame buffer reader-cache executive.
qtractorAudioPeakFile::Frame *qtractorAudioPeak::peakFrames (
	unsigned long iFrameOffset, unsigned long iFrameLength, int width )
{
	// Skip empty blanks...
	if (width < 1)
		return NULL;

	// Try open current peak file as is...
	if (!m_pPeakFile->openRead())
		return NULL;

	// Just in case resolutions might change...
	const unsigned short iPeakPeriod = m_pPeakFile->period();
	if (iPeakPeriod < 1)
		return NULL;

	// Peak frames length estimation...
	const unsigned int iPeakLength = (iFrameLength / iPeakPeriod);
	if (iPeakLength < 1)
		return NULL;

	// We'll get a brand new peak frames alright...
	const unsigned short iChannels = m_pPeakFile->channels();
	if (iChannels < 1)
		return NULL;

	// Have we been here before?
	if (m_pPeakFrames) {
		// Check if we have the same previous hash...
		if (!m_pPeakFile->isWaitSync()) {
			const unsigned int iPeakHash
				= qHash(iPeakPeriod)
				^ qHash(iFrameOffset)
				^ qHash(iFrameLength)
				^ qHash(width);
			if (m_iPeakHash == iPeakHash)
				return m_pPeakFrames;
			m_iPeakHash = iPeakHash;
		}
		// Clenup previous frame-buffers...
		delete [] m_pPeakFrames;
		m_pPeakFrames = NULL;
		m_iPeakLength = 0;
	}

	// Grab them in...
	const unsigned long iPeakOffset = (iFrameOffset / iPeakPeriod);
	qtractorAudioPeakFile::Frame *pPeakFrames
		= m_pPeakFile->read(iPeakOffset, iPeakLength);
	if (pPeakFrames == NULL)
		return NULL;

	// Check if we better aggregate over the frame buffer....
	const int p1 = int(iPeakLength);
	const int n1 = iChannels * p1;

	if (width < p1 && width > 1) {
		const int w2 = (width >> 1) + 1;
		const int n2 = iChannels * w2;
		m_pPeakFrames = new qtractorAudioPeakFile::Frame [n2];
		int n = 0; int i = 0;
		while (n < n2) {
			const int i2 = (n * p1) / w2;
			for (unsigned short k = 0; k < iChannels; ++k) {
				qtractorAudioPeakFile::Frame *pNewFrame = &m_pPeakFrames[n++];
				qtractorAudioPeakFile::Frame *pOldFrame = &pPeakFrames[i + k];
				pNewFrame->max = pOldFrame->max;
				pNewFrame->min = pOldFrame->min;
				pNewFrame->rms = pOldFrame->rms;
				for (int j = i + 1; j < i2; j += iChannels) {
					pOldFrame += iChannels;
					if (pNewFrame->max < pOldFrame->max)
						pNewFrame->max = pOldFrame->max;
					if (pNewFrame->min < pOldFrame->min)
						pNewFrame->min = pOldFrame->min;
					if (pNewFrame->rms < pOldFrame->rms)
						pNewFrame->rms = pOldFrame->rms;
				}
			}
			i = i2;
		}
		// New-indirect frame buffer length...
		m_iPeakLength = n / iChannels;
		// Done-indirect.
	} else {
		// Direct-copy frame-buffer...
		m_pPeakFrames = new qtractorAudioPeakFile::Frame [n1];
		::memcpy(m_pPeakFrames, pPeakFrames,
			n1 * sizeof(qtractorAudioPeakFile::Frame));
		// New-direct frame buffer length...
		m_iPeakLength = iPeakLength;
		// Done-direct.
	}

	return m_pPeakFrames;
}


//----------------------------------------------------------------------
// class qtractorAudioPeakFactory -- Audio peak file factory (singleton).
//

// Singleton instance pointer.
qtractorAudioPeakFactory *qtractorAudioPeakFactory::g_pPeakFactory = NULL;

// Singleton instance accessor (static).
qtractorAudioPeakFactory *qtractorAudioPeakFactory::getInstance (void)
{
	return g_pPeakFactory;
}


// Constructor.
qtractorAudioPeakFactory::qtractorAudioPeakFactory ( QObject *pParent )
	: QObject(pParent), m_bAutoRemove(false),
		m_pPeakThread(NULL), m_iPeakPeriod(c_iPeakPeriod)
{
	// Pseudo-singleton reference setup.
	g_pPeakFactory = this;
}


// Default destructor.
qtractorAudioPeakFactory::~qtractorAudioPeakFactory (void)
{
	if (m_pPeakThread) {
		if (m_pPeakThread->isRunning()) do {
			m_pPeakThread->setRunState(false);
		//	m_pPeakThread->terminate();
			m_pPeakThread->sync();
		} while (!m_pPeakThread->wait(100));
		delete m_pPeakThread;
		m_pPeakThread = NULL;
	}

	cleanup();

	// Pseudo-singleton reference shut-down.
	g_pPeakFactory = NULL;
}


// The peak period accessors.
void qtractorAudioPeakFactory::setPeakPeriod ( unsigned short iPeakPeriod )
{
	if (iPeakPeriod == m_iPeakPeriod)
		return;

	QMutexLocker locker(&m_mutex);

	sync(NULL);

	m_iPeakPeriod = iPeakPeriod;

	// Refresh all current peak files (asynchronously)...
	PeakFiles::ConstIterator iter = m_peaks.constBegin();
	const PeakFiles::ConstIterator& iter_end = m_peaks.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorAudioPeakFile *pPeakFile = iter.value();
		pPeakFile->cleanup(true);
		sync(pPeakFile);
	}
}


unsigned short qtractorAudioPeakFactory::peakPeriod (void) const
{
	return m_iPeakPeriod;
}


// The peak file factory-methods.
qtractorAudioPeak* qtractorAudioPeakFactory::createPeak (
	const QString& sFilename, float fTimeStretch )
{
	QMutexLocker locker(&m_mutex);

	if (m_pPeakThread == NULL) {
		m_pPeakThread = new qtractorAudioPeakThread();
		m_pPeakThread->start();
	}

	const QString& sPeakName
		= qtractorAudioPeakFile::peakName(sFilename, fTimeStretch);
	qtractorAudioPeakFile *pPeakFile = m_peaks.value(sPeakName);
	if (pPeakFile == NULL) {
		pPeakFile = new qtractorAudioPeakFile(sFilename, fTimeStretch);
		m_peaks.insert(sPeakName, pPeakFile);
	}

	return new qtractorAudioPeak(pPeakFile);
}


// Auto-delete property.
void qtractorAudioPeakFactory::setAutoRemove ( bool bAutoRemove )
{
	m_bAutoRemove = bAutoRemove;
}

bool qtractorAudioPeakFactory::isAutoRemove (void) const
{
	return m_bAutoRemove;
}


// Event notifier.
void qtractorAudioPeakFactory::notifyPeakEvent (void)
{
	emit peakEvent();
}


// Base sync method.
void qtractorAudioPeakFactory::sync ( qtractorAudioPeakFile *pPeakFile )
{
	if (m_pPeakThread) m_pPeakThread->sync(pPeakFile);
}


// Cleanup method.
void qtractorAudioPeakFactory::cleanup (void)
{
	QMutexLocker locker(&m_mutex);

	sync(NULL);

	// Cleanup all current registered peak files...
	PeakFiles::ConstIterator iter = m_peaks.constBegin();
	const PeakFiles::ConstIterator& iter_end = m_peaks.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorAudioPeakFile *pPeakFile = iter.value();
		pPeakFile->cleanup(m_bAutoRemove);
	}

	qDeleteAll(m_peaks);
	m_peaks.clear();

	// Reset to default resolution...
	m_iPeakPeriod = c_iPeakPeriod;
}


// end of qtractorAudioPeak.cpp
