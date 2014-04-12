// qtractorAudioPeak.cpp
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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
	unsigned int           m_iSyncSize;
	unsigned int           m_iSyncMask;
	qtractorAudioPeak    **m_ppSyncItems;

	volatile unsigned int  m_iSyncRead;
	volatile unsigned int  m_iSyncWrite;

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
	m_ppSyncItems = new qtractorAudioPeak * [m_iSyncSize];
	m_iSyncRead   = 0;
	m_iSyncWrite  = 0;

	::memset(m_ppSyncItems, 0, m_iSyncSize * sizeof(qtractorAudioPeak *));

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
	QMutexLocker locker(&m_mutex);

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
			qtractorAudioPeak *pSyncItem = m_ppSyncItems[r];
			if (pSyncItem)
				pSyncItem->peakFile()->setWaitSync(false);
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
			m_ppSyncItems[w] = new qtractorAudioPeak(pPeakFile);
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
		while (r != w) {
			qtractorAudioPeak *pSyncItem = m_ppSyncItems[r];
			m_pPeakFile = pSyncItem->peakFile();
			if (m_pPeakFile->isWaitSync()) {
				if (openPeakFile()) {
					// Go ahead with the whole bunch...
					while (writePeakFile())
						/* empty loop */;
					// We're done.
					closePeakFile();
				}
				m_pPeakFile->setWaitSync(false);
			}
			delete pSyncItem;
			m_ppSyncItems[r] = NULL;
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
		// Send notification event, anyway...
		notifyPeakEvent();
		// Wait for sync...
		m_cond.wait(&m_mutex);
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
	const int nread = m_pAudioFile->read(m_ppAudioFrames, c_iAudioFrames);
	if (nread > 0)
		m_pPeakFile->write(m_ppAudioFrames, nread);

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
		for (unsigned short i = 0; i < iChannels; ++i)
			delete [] m_ppAudioFrames[i];
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

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorAudioPeakFactory *pPeakFactory
			= pSession->audioPeakFactory();
		if (pPeakFactory)
			pPeakFactory->notifyPeakEvent();
	}
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

	m_peakHeader.period   = c_iPeakPeriod;
	m_peakHeader.channels = 0;

	m_pBuffer      = NULL;
	m_iBuffSize    = 0;
	m_iBuffLength  = 0;
	m_iBuffOffset  = 0;

	m_iWriteOffset = 0;

	m_peakMax      = NULL;
	m_peakMin      = NULL;
	m_peakRms      = NULL;
	m_iPeakPeriod  = 0;
	m_iPeak        = 0;

	m_bWaitSync    = false;

	m_iRefCount    = 0;

	// Set (unique) peak filename...
	QDir dir;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		dir.setPath(pSession->sessionDir());

	const QFileInfo fileInfo(sFilename);
	const QString& sPeakFilePrefix
		= QFileInfo(dir, fileInfo.fileName()).filePath();
	const QString& sPeakName
		= qtractorAudioPeakFactory::peakName(sFilename, fTimeStretch);
	const QFileInfo peakInfo(sPeakFilePrefix + '_'
		+ QString::number(qHash(sPeakName), 16)
		+ c_sPeakFileExt);

	m_peakFile.setFileName(peakInfo.absoluteFilePath());
}


// Default destructor.
qtractorAudioPeakFile::~qtractorAudioPeakFile (void)
{
	// Check if it's aborting (ought to be atomic)...
	const bool bAborted = m_bWaitSync;
	m_bWaitSync = false;

	// Close the file, anyway now.
	closeWrite();
	closeRead();

	// Tell master-factory that we're out immediately (if aborted)...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorAudioPeakFactory *pPeakFactory
			= pSession->audioPeakFactory();
		if (pPeakFactory)
			pPeakFactory->removePeak(this, bAborted);
	}
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
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) {
			qtractorAudioPeakFactory *pPeakFactory
				= pSession->audioPeakFactory();
			if (pPeakFactory)
				pPeakFactory->sync(this);
		}
		return false;
	}

	// Make things critical...
	QMutexLocker locker(&m_mutex);

	// Just open and go ahead with first bunch...
	if (!m_peakFile.open(QIODevice::ReadOnly))
		return false;

	if (m_peakFile.read((char *) &m_peakHeader, sizeof(Header))
		!= (qint64) sizeof(Header)) {
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
	return qtractorAudioPeakFactory::peakName(m_sFilename, m_fTimeStretch);
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
	unsigned long iPeakOffset, unsigned int iPeakFrames )
{
	// Must be open for something...
	if (m_openMode == None)
		return NULL;

	// Make things critical...
	QMutexLocker locker(&m_mutex);

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakFile[%p]::read(%lu, %u) [%lu, %u, %u]", this,
		iPeakOffset, iPeakFrames, m_iBuffOffset, m_iBuffLength, m_iBuffSize);
#endif

	// Cache effect, only valid if we're really reading...
	const unsigned long iPeakEnd = iPeakOffset + iPeakFrames;
	if (iPeakOffset >= m_iBuffOffset && m_iBuffOffset < iPeakEnd) {
		unsigned long iBuffEnd = m_iBuffOffset + m_iBuffLength;
		if (iBuffEnd >= iPeakEnd)
			return m_pBuffer
				+ m_peakHeader.channels * (iPeakOffset - m_iBuffOffset);
		if (m_iBuffOffset + m_iBuffSize >= iPeakEnd) {
			m_iBuffLength
				+= readBuffer(m_iBuffLength, iBuffEnd, iPeakEnd - iBuffEnd);
			return m_pBuffer
				+ m_peakHeader.channels * (iPeakOffset - m_iBuffOffset);
		}
	}

	// Read peak data as requested...
	m_iBuffLength = readBuffer(0, iPeakOffset, iPeakFrames);
	m_iBuffOffset = iPeakOffset;

	return m_pBuffer;
}


// Read frames from peak file into local buffer cache.
unsigned int qtractorAudioPeakFile::readBuffer (
	unsigned int iBuffOffset, unsigned long iPeakOffset, unsigned int iPeakFrames )
{
	// Shall we reallocate?
	if (iBuffOffset + iPeakFrames > m_iBuffSize) {
		Frame *pOldBuffer = m_pBuffer;
		m_iBuffSize += (iPeakFrames << 1);
		m_pBuffer = new Frame [m_peakHeader.channels * m_iBuffSize];
		if (pOldBuffer) {
			::memcpy(m_pBuffer, pOldBuffer,
				m_peakHeader.channels * m_iBuffLength * sizeof(Frame));
			delete [] pOldBuffer;
		}
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakFile[%p]::readBuffer(%u, %lu, %u) [%lu, %u, %u]",
		this, iBuffOffset, iPeakOffset, iPeakFrames,
		m_iBuffOffset, m_iBuffLength, m_iBuffSize);
#endif

	// Grab new contents from peak file...
	char *pBuffer = (char *) (m_pBuffer + m_peakHeader.channels * iBuffOffset);
	const unsigned long iOffset
		= iPeakOffset * m_peakHeader.channels * sizeof(Frame);
	const unsigned int iLength
		= iPeakFrames * m_peakHeader.channels * sizeof(Frame);

	int nread = 0;
	if (m_peakFile.seek(sizeof(Header) + iOffset))
		nread = (int) m_peakFile.read(&pBuffer[0], iLength);

	// Zero the remaining...
	if (nread < (int) iLength)
		::memset(&pBuffer[nread], 0, iLength - nread);

	return nread / (m_peakHeader.channels * sizeof(Frame));
}


// Open an new peak file for writing.
bool qtractorAudioPeakFile::openWrite (
	unsigned short iChannels, unsigned int iSampleRate )
{
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
	m_peakHeader.period   = c_iPeakPeriod;
	m_peakHeader.channels = iChannels;

	// Write peak file header.
	if (m_peakFile.write((const char *) &m_peakHeader, sizeof(Header))
		!= (qint64) sizeof(Header)) {
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
	m_iWriteOffset = 0;

	m_peakMax = new float [m_peakHeader.channels];
	m_peakMin = new float [m_peakHeader.channels];
	m_peakRms = new float [m_peakHeader.channels];
	for (unsigned short i = 0; i < m_peakHeader.channels; ++i)
		m_peakMax[i] = m_peakMin[i] = m_peakRms[i] = 0.0f;

	m_iPeakPeriod = c_iPeakPeriod;
	m_iPeak = 0;

	// Get running sample-rate...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// The resample/timestretch-aware internal peak period...
		m_iPeakPeriod = (unsigned short) ::lroundf(
			(float(c_iPeakPeriod * iSampleRate)) /
			(float(pSession->sampleRate()) * m_fTimeStretch));
	}

	// Its a certain success...
	return true;
}


// Close the (hopefully) created peak file.
void qtractorAudioPeakFile::closeWrite (void)
{
	// Make things critical...
	QMutexLocker locker(&m_mutex);

	// Flush and close...
	if (m_openMode == Write) {
		if (m_iPeak > 0)
			writeFrame();
		m_peakFile.close();
		m_openMode = None;
	}

	if (m_peakMax)
		delete [] m_peakMax;
	m_peakMax = NULL;

	if (m_peakMin)
		delete [] m_peakMin;
	m_peakMin = NULL;

	if (m_peakRms)
		delete [] m_peakRms;
	m_peakRms = NULL;

	m_iPeakPeriod = 0;
	m_iPeak = 0;
}


// Write peak from audio frame methods.
void qtractorAudioPeakFile::write (
	float **ppAudioFrames, unsigned int iAudioFrames )
{
	if (m_openMode != Write)
		return;

	// Make things critical...
	QMutexLocker locker(&m_mutex);

	for (unsigned int n = 0; n < iAudioFrames; ++n) {
		// Accumulate for this sample frame...
		for (unsigned short i = 0; i < m_peakHeader.channels; ++i) {
			const float fSample = ppAudioFrames[i][n];
			if (m_peakMax[i] < fSample)
				m_peakMax[i] = fSample;
			if (m_peakMin[i] > fSample)
				m_peakMin[i] = fSample;
			m_peakRms[i] += (fSample * fSample);
		}
		// Have we reached the peak accumulative period?
		if (++m_iPeak >= m_iPeakPeriod)
			writeFrame();
	}
}


void qtractorAudioPeakFile::writeFrame (void)
{
	if (!m_peakFile.seek(sizeof(Header) + m_iWriteOffset))
		return;

	for (unsigned short i = 0; i < m_peakHeader.channels; ++i) {
		Frame frame;
		// Write the denormalized peak values...
		m_peakMax[i] = 255.0f * ::fabs(m_peakMax[i]);
		m_peakMin[i] = 255.0f * ::fabs(m_peakMin[i]);
		m_peakRms[i] = 255.0f * ::sqrtf(m_peakRms[i] / float(m_iPeak));
		frame.max = (unsigned char) (m_peakMax[i] > 255.0f ? 255 : m_peakMax[i]);
		frame.min = (unsigned char) (m_peakMin[i] > 255.0f ? 255 : m_peakMin[i]);
		frame.rms = (unsigned char) (m_peakRms[i] > 255.0f ? 255 : m_peakRms[i]);
		// Reset peak period accumulators...
		m_peakMax[i] = m_peakMin[i] = m_peakRms[i] = 0.0f;
		// Bail out?...
		m_iWriteOffset += m_peakFile.write((const char *) &frame, sizeof(Frame));
	}

	// We'll reset.
	m_iPeak = 0;
}


// Reference count methods.
void qtractorAudioPeakFile::addRef (void)
{
	++m_iRefCount;
}

void qtractorAudioPeakFile::removeRef (void)
{
	if (--m_iRefCount == 0)
		delete this;
}


// Physical removal.
void qtractorAudioPeakFile::remove (void)
{
	m_peakFile.remove();
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


//----------------------------------------------------------------------
// class qtractorAudioPeakFactory -- Audio peak file factory (singleton).
//

// Constructor.
qtractorAudioPeakFactory::qtractorAudioPeakFactory ( QObject *pParent )
	: QObject(pParent), m_bAutoRemove(false), m_pPeakThread(NULL)
{
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

	qDeleteAll(m_peaks);
	m_peaks.clear();
}


// The peak file factory-methods.
QString qtractorAudioPeakFactory::peakName (
	const QString& sFilename, float fTimeStretch )
{
	return sFilename + '_' + QString::number(fTimeStretch);
}


qtractorAudioPeak* qtractorAudioPeakFactory::createPeak (
	const QString& sFilename, float fTimeStretch )
{
	QMutexLocker locker(&m_mutex);

	if (m_pPeakThread == NULL) {
		m_pPeakThread = new qtractorAudioPeakThread();
		m_pPeakThread->start();
	}

	const QString& sPeakName = peakName(sFilename, fTimeStretch);
	qtractorAudioPeakFile *pPeakFile = m_peaks.value(sPeakName);
	if (pPeakFile == NULL) {
		pPeakFile = new qtractorAudioPeakFile(sFilename, fTimeStretch);
		m_peaks.insert(sPeakName, pPeakFile);
	}

	return new qtractorAudioPeak(pPeakFile);
}


void qtractorAudioPeakFactory::removePeak (
	qtractorAudioPeakFile *pPeakFile, bool bAborted )
{
	QMutexLocker locker(&m_mutex);

	m_peaks.remove(pPeakFile->peakName());

	if (bAborted)
		pPeakFile->remove();
	else
	if (m_bAutoRemove)
		m_files.append(pPeakFile->name());
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
	QStringListIterator iter(m_files);
	while (iter.hasNext())
		QFile::remove(iter.next());
	m_files.clear();
}


// end of qtractorAudioPeak.cpp
