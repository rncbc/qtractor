// qtractorAudioPeak.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMainForm.h"
#include "qtractorSession.h"

#include <QApplication>
#include <QFileInfo>
#include <QThread>
#include <QDir>

#include <QDateTime>
#include <QWidget>
#include <QEvent>

#include <math.h>


// Audio file buffer size in frames per channel.
static const unsigned int c_iAudioFrames = (16 * 1024);

// Peak file buffer size in frames per channel.
static const unsigned int c_iPeakFrames = (4 * 1024);

// Default peak period as a digest representation in frames per channel.
static const unsigned short c_iPeakPeriod = 1024;

// Default peak filename extension.
static const QString c_sPeakFileExt = ".peak";

#ifdef QTRACTOR_PEAK_SMOOTHED
// Fixed peak smoothing coeficient (exponential average).
static const float c_fPeakExpCoef = 0.5f;
#endif


//----------------------------------------------------------------------
// class qtractorAudioPeakThread -- Audio Peak file thread.
//

class qtractorAudioPeakThread : public QThread
{
public:

	// Constructor.
	qtractorAudioPeakThread(qtractorAudioPeakFile *pPeakFile);
	// Destructor.
	~qtractorAudioPeakThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

protected:

	// The main thread executive.
	void run();

	// Actual peak file creation methods.
	// (this is just about to be used internally)
	bool writePeakFile();
	void closePeakFile();

private:

	// Whether the thread is logically running.
	bool m_bRunState;

	// The peak file instance reference.
	qtractorAudioPeakFile *m_pPeakFile;

	// The audio file instance.
	qtractorAudioFile *m_pAudioFile;

	// Audio file buffer.
	float **m_ppAudioFrames;
};


// Constructor.
qtractorAudioPeakThread::qtractorAudioPeakThread (
	qtractorAudioPeakFile *pPeakFile )
{
	m_bRunState = false;
	m_pPeakFile = pPeakFile;

	m_pAudioFile
		= qtractorAudioFileFactory::createAudioFile(pPeakFile->filename());

	m_ppAudioFrames = NULL;
}


// Destructor.
qtractorAudioPeakThread::~qtractorAudioPeakThread (void)
{
	closePeakFile();
}


// The main thread executive.
void qtractorAudioPeakThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakThread[%p]::run(): started...", this);
#endif

	m_bRunState = true;

	// Open audio sample file...
	if (m_pAudioFile->open(m_pPeakFile->filename())) {
		unsigned short iChannels   = m_pAudioFile->channels();
		unsigned int   iSampleRate = m_pAudioFile->sampleRate();
		// Create the peak file from the sample audio one...
		if (m_pPeakFile->openWrite(iChannels, iSampleRate)) {
			// Allocate audio file frame buffer
			// and peak period accumulators.
			m_ppAudioFrames = new float* [iChannels];
			for (unsigned short i = 0; i < iChannels; ++i)
				m_ppAudioFrames[i] = new float [c_iAudioFrames];
			// Go ahead with the whole bunch...
			while (m_bRunState && writePeakFile())
				/* empty loop */;
		}
		// We're done.
		closePeakFile();
	}

	// May send notification event, anyway...
	if (m_pPeakFile->notifyWidget()) {
		QApplication::postEvent(m_pPeakFile->notifyWidget(),
			new QEvent(m_pPeakFile->notifyPeakType()));
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakThread[%p]::run(): stopped.\n", this);
#endif
}


// Run state accessor.
void qtractorAudioPeakThread::setRunState ( bool bRunState )
{
	m_bRunState = bRunState;
}

bool qtractorAudioPeakThread::runState (void) const
{
	return m_bRunState;
}


// Create the peak file chunk.
bool qtractorAudioPeakThread::writePeakFile (void)
{
	if (m_ppAudioFrames == NULL)
		return false;

	// Read another bunch of frames from the physical audio file...
	int nread = m_pAudioFile->read(m_ppAudioFrames, c_iAudioFrames);
	if (nread > 0)
		m_pPeakFile->write(m_ppAudioFrames, nread);

	return (nread > 0);
}


// Close the (hopefully) created peak file.
void qtractorAudioPeakThread::closePeakFile (void)
{
	// Always force target file close.
	m_pPeakFile->closeWrite();

	// Get rid of physical used stuff.
	if (m_ppAudioFrames) {
		unsigned short iChannels = m_pAudioFile->channels();
		for (unsigned short i = 0; i < iChannels; ++i)
			delete [] m_ppAudioFrames[i];
		delete [] m_ppAudioFrames;
	}
	m_ppAudioFrames = NULL;

	// Finally the source file too.
	if (m_pAudioFile)
		delete m_pAudioFile;
	m_pAudioFile = NULL;
}


//----------------------------------------------------------------------
// class qtractorAudioPeakFile -- Audio peak file (ref'counted)
//

// Constructor.
qtractorAudioPeakFile::qtractorAudioPeakFile (
	qtractorAudioPeakFactory *pFactory,
	const QString& sFilename, float fTimeStretch )
{
	// Initialize instance variables.
	m_pFactory     = pFactory;
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
	m_peakRms      = NULL;
	m_iPeakPeriod  = 0;
	m_iPeak        = 0;

	m_pPeakThread  = NULL;

	m_iRefCount    = 0;

	// Set peak filename...
	QDir dir;
	qtractorSession  *pSession  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pSession = pMainForm->session();
	if (pSession)
		dir.setPath(pSession->sessionDir());
	m_peakFile.setFileName(
		QFileInfo(dir,
			QFileInfo(sFilename).fileName()).filePath()
			+ '_' + QString::number(fTimeStretch) + c_sPeakFileExt);
}


// Default destructor.
qtractorAudioPeakFile::~qtractorAudioPeakFile (void)
{
	// Check if peak thread is still running.
	bool bAborted = false;
	if (m_pPeakThread) {
		// Try to wait for thread termination...
		if (m_pPeakThread->isRunning()) {
			m_pPeakThread->setRunState(false);
		//	m_pPeakThread->terminate();
			m_pPeakThread->wait();
			bAborted = true;
		}
		delete m_pPeakThread;
		m_pPeakThread = NULL;
	}

	// Close the file, anyway now.
	closeWrite();
	closeRead();

	// Tell master-factory that we're out.
	m_pFactory->removePeak(this);

	// Do we get rid from the filesystem too?
	if (m_pFactory->isAutoRemove() || bAborted)
		m_peakFile.remove();
}


// Open an existing peak file cache.
bool qtractorAudioPeakFile::openRead (void)
{
	// If it's already open, just tell the news.
	if (m_openMode != None)
		return true;

	// Check if we're still on creative thread...
	if (m_pPeakThread) {
		// If running try waiting for it to finnish...
		if (m_pPeakThread->isRunning())
			return false;
		// OK. We're done with our creative thread.
		delete m_pPeakThread;
		m_pPeakThread = NULL;
	} else  {
		// Need some preliminary file information...
		QFileInfo fileInfo(m_sFilename);
		QFileInfo peakInfo(m_peakFile.fileName());
		// Have we a peak file up-to-date,
		// or must the peak file be (re)created?
		if (!peakInfo.exists() || peakInfo.created() < fileInfo.created()) {
		//	|| peakInfo.lastModified() < fileInfo.lastModified()) {
			m_pPeakThread = new qtractorAudioPeakThread(this);
			m_pPeakThread->start();
			return false;
		}
	}

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


// Event notifier widget settings.
QWidget *qtractorAudioPeakFile::notifyWidget (void) const
{
	return m_pFactory->notifyWidget();
}

QEvent::Type qtractorAudioPeakFile::notifyPeakType (void) const
{
	return m_pFactory->notifyPeakType();
}


// Auto-delete property.
bool qtractorAudioPeakFile::isAutoRemove (void) const
{
	return m_pFactory->isAutoRemove();
}


// Read frames from peak file.
qtractorAudioPeakFile::Frame *qtractorAudioPeakFile::read (
	unsigned long iPeakOffset, unsigned int iPeakFrames )
{
	// Must be open for something...
	if (m_openMode == None)
		return NULL;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioPeakFile[%p]::read(%lu, %u) [%lu, %u, %u]", this,
		iPeakOffset, iPeakFrames, m_iBuffOffset, m_iBuffLength, m_iBuffSize);
#endif

	// Cache effect, only valid if we're really reading...
	if (m_openMode == Read) {
		unsigned long iPeakOffset2 = iPeakOffset + iPeakFrames;
		if (iPeakOffset2 >= m_iBuffOffset) {
			if (iPeakOffset >= m_iBuffOffset) {
				unsigned long iBuffOffset2 = m_iBuffOffset + m_iBuffLength;
				if (iBuffOffset2 >= iPeakOffset2)
					return m_pBuffer
						+ m_peakHeader.channels * (iPeakOffset - m_iBuffOffset);
				if (m_iBuffOffset + m_iBuffSize >= iPeakOffset2) {
					iPeakFrames = iPeakOffset2 - iBuffOffset2;
					readBuffer(m_iBuffLength, iBuffOffset2, iPeakFrames);
					m_iBuffLength += iPeakFrames;
					return m_pBuffer
						+ m_peakHeader.channels * (iPeakOffset - m_iBuffOffset);
				}
			} else {
				unsigned int iPeakFrames2 = m_iBuffOffset - iPeakOffset;
				if (m_iBuffLength + iPeakFrames2 < m_iBuffSize) {
					::memmove(
						m_pBuffer + m_peakHeader.channels * iPeakFrames2,
						m_pBuffer,
						m_peakHeader.channels * m_iBuffLength * sizeof(Frame));
					readBuffer(0, iPeakOffset, iPeakFrames2);
					m_iBuffOffset  = iPeakOffset;
					m_iBuffLength += iPeakFrames2;
					return m_pBuffer;
				}
			}
		}
	}

	// Read peak data as requested...
	readBuffer(0, iPeakOffset, iPeakFrames);
	m_iBuffOffset = iPeakOffset;
	m_iBuffLength = iPeakFrames;

	return m_pBuffer;
}


// Read frames from peak file into local buffer offset.
void qtractorAudioPeakFile::readBuffer (
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
	unsigned long iOffset = iPeakOffset * m_peakHeader.channels * sizeof(Frame);
	unsigned int  iLength = iPeakFrames * m_peakHeader.channels * sizeof(Frame);

	int nread = 0;
	if (m_peakFile.seek(sizeof(Header) + iOffset))
		nread = (int) m_peakFile.read(&pBuffer[0], iLength);
	// Zero the remaining...
	if (nread < (int) iLength)
		::memset(&pBuffer[nread], 0, iLength - nread);
}


// Open an new peak file for writing.
bool qtractorAudioPeakFile::openWrite (
	unsigned short iChannels, unsigned int iSampleRate )
{
	// If it's already open, just tell the news.
	if (m_openMode == Write)
		return true;

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
	qDebug("header      = %lu", sizeof(Header));
	qDebug("frame       = %lu", sizeof(Frame));
	qDebug("period      = %d", m_peakHeader.period);
	qDebug("channels    = %d", m_peakHeader.channels);
	qDebug("---");
#endif

	//	Go ahead, it's already open.
	m_iWriteOffset = 0;

#ifdef QTRACTOR_PEAK_SMOOTHED
	m_peakMax = new float [2 * m_peakHeader.channels];
	m_peakRms = new float [2 * m_peakHeader.channels];
	for (unsigned short i = 0; i < m_peakHeader.channels; ++i) {
		m_peakMax[i] = m_peakMax[i + m_peakHeader.channels] = 0.0f;
		m_peakRms[i] = m_peakRms[i + m_peakHeader.channels] = 0.0f;
	}
#else
	m_peakMax = new float [m_peakHeader.channels];
	m_peakRms = new float [m_peakHeader.channels];
	for (unsigned short i = 0; i < m_peakHeader.channels; ++i)
		m_peakMax[i] = m_peakRms[i] = 0.0f;
#endif

	m_iPeakPeriod = c_iPeakPeriod;
	m_iPeak = 0;

	// Get running sample-rate...
	qtractorSession  *pSession  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pSession = pMainForm->session();
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

	for (unsigned int n = 0; n < iAudioFrames; ++n) {
		// Accumulate for this sample frame...
		for (unsigned short i = 0; i < m_peakHeader.channels; ++i) {
			float fSample = ppAudioFrames[i][n];
			if (fSample < 0.0f) // Take absolute value...
				fSample = -(fSample);
			if (m_peakMax[i] < fSample)
				m_peakMax[i] = fSample;
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
#ifdef QTRACTOR_PEAK_SMOOTHED
		// Write the smoothed peak maximum and RMS value...
		unsigned short k = i + m_peakHeader.channels;
		m_peakMax[k] = (1.0f - c_fPeakExpCoef) * m_peakMax[k]
			+ c_fPeakExpCoef * 254.0f * m_peakMax[i];
		frame.max = (unsigned char) (m_peakMax[k] > 254.0f ? 255 : m_peakMax[k]);
		m_peakRms[k] = (1.0f - c_fPeakExpCoef) * m_peakRms[k]
			+ c_fPeakExpCoef * 254.0f * (::sqrtf(m_peakRms[i] / float(m_iPeak)));
		frame.rms = (unsigned char) (m_peakRms[k] > 254.0f ? 255 : m_peakRms[k]);
#else
		// Write the denormalized peak values...
		m_peakMax[i] = 254.0f * m_peakMax[i];
		m_peakRms[i] = 254.0f * ::sqrtf(m_peakRms[i] / float(m_iPeak));
		frame.max = (unsigned char) (m_peakMax[i] > 254.0f ? 255 : m_peakMax[i]);
		frame.rms = (unsigned char) (m_peakRms[i] > 254.0f ? 255 : m_peakRms[i]);
#endif
		// Reset peak period accumulators...
		m_peakMax[i] = 0.0f;
		m_peakRms[i] = 0.0f;
		// Bail out?...
		m_iWriteOffset += m_peakFile.write((const char *) &frame, sizeof(Frame));
	}

	// We'll reset.
	m_iPeak = 0;
}


// Reference count methods.
void qtractorAudioPeakFile::addRef (void)
{
	m_iRefCount++;
}

void qtractorAudioPeakFile::removeRef (void)
{
	if (--m_iRefCount == 0)
		delete this;
}


//----------------------------------------------------------------------
// class qtractorAudioPeak -- Audio Peak file pseudo-cache.
//

// Constructor.
qtractorAudioPeak::qtractorAudioPeak ( qtractorAudioPeakFile *pPeakFile )
{
	m_pPeakFile = pPeakFile;
	m_pPeakFile->addRef();
}

// Copy constructor.
qtractorAudioPeak::qtractorAudioPeak ( const qtractorAudioPeak& peak )
{
	m_pPeakFile = peak.m_pPeakFile;
	m_pPeakFile->addRef();
}

// Default destructor.
qtractorAudioPeak::~qtractorAudioPeak (void)
{
	m_pPeakFile->removeRef();
}


// Peak file accessors.
const QString& qtractorAudioPeak::filename (void) const
{
	return m_pPeakFile->filename();
}

unsigned short qtractorAudioPeak::period (void) const
{
	return m_pPeakFile->period();
}

unsigned short qtractorAudioPeak::channels (void) const
{
	return m_pPeakFile->channels();
}


// Peak cache file methods.
bool qtractorAudioPeak::openRead (void)
{
	return m_pPeakFile->openRead();
}

qtractorAudioPeakFile::Frame *qtractorAudioPeak::read (
	unsigned long iPeakOffset, unsigned int iPeakFrames )
{
	return m_pPeakFile->read(iPeakOffset, iPeakFrames);
}

void qtractorAudioPeak::closeRead (void)
{
	return m_pPeakFile->closeRead();
}


// Write peak from audio frame methods.
bool qtractorAudioPeak::openWrite (
	unsigned short iChannels, unsigned int iSampleRate )
{
	return m_pPeakFile->openWrite(iChannels, iSampleRate);
}

void qtractorAudioPeak::write (
	float **ppAudioFrames, unsigned int iAudioFrames )
{
	m_pPeakFile->write(ppAudioFrames, iAudioFrames);
}

void qtractorAudioPeak::closeWrite (void)
{
	return m_pPeakFile->closeWrite();
}


//----------------------------------------------------------------------
// class qtractorAudioPeakFactory -- Audio peak file factory (singleton).
//

// Constructor.
qtractorAudioPeakFactory::qtractorAudioPeakFactory (void)
{
	m_pNotifyWidget   = NULL;
	m_eNotifyPeakType = QEvent::None;
	
	m_bAutoRemove = false;
}


// Default destructor.
qtractorAudioPeakFactory::~qtractorAudioPeakFactory (void)
{
	QMutableHashIterator<QString, qtractorAudioPeakFile *> iter(m_peaks);
	while (iter.hasNext()) {
		qtractorAudioPeakFile *pPeakFile = iter.next().value();
		iter.remove();
		delete pPeakFile;
	}

	m_peaks.clear();
}


// The peak file factory-methods.
qtractorAudioPeak* qtractorAudioPeakFactory::createPeak (
	const QString& sFilename, float fTimeStretch )
{
	QMutexLocker locker(&m_mutex);

	const QString sKey = sFilename + '_' + QString::number(fTimeStretch);
	qtractorAudioPeakFile* pPeakFile = m_peaks.value(sKey, NULL);
	if (pPeakFile == NULL) {
		pPeakFile = new qtractorAudioPeakFile(this, sFilename, fTimeStretch);
		m_peaks.insert(sKey, pPeakFile);
	}

	return new qtractorAudioPeak(pPeakFile);
}

void qtractorAudioPeakFactory::removePeak ( qtractorAudioPeakFile *pPeakFile )
{
	QMutexLocker locker(&m_mutex);

	m_peaks.remove(pPeakFile->filename()
		+ '_' + QString::number(pPeakFile->timeStretch()));
}


// Event notifier widget settings.
void qtractorAudioPeakFactory::setNotifyWidget ( QWidget *pNotifyWidget )
{
	m_pNotifyWidget = pNotifyWidget;
}

void qtractorAudioPeakFactory::setNotifyPeakType ( QEvent::Type eNotifyPeakType )
{
	m_eNotifyPeakType = eNotifyPeakType;
}


QWidget *qtractorAudioPeakFactory::notifyWidget (void) const
{
	return m_pNotifyWidget;
}

QEvent::Type qtractorAudioPeakFactory::notifyPeakType (void) const
{
	return m_eNotifyPeakType;
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


// end of qtractorAudioPeak.cpp
