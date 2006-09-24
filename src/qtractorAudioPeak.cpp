// qtractorAudioPeak.cpp
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAudioPeak.h"
#include "qtractorAudioFile.h"

#include <qapplication.h>
#include <qfileinfo.h>
#include <qthread.h>
#include <qdir.h>

#include <math.h>

#if defined(__BORLANDC__)
// BCC32 doesn't have these float versions...
static inline float sqrtf ( float x )	{ return float(::sqrt(x)); }
static inline float fabsf ( float x )	{ return float(::fabs(x)); }
#endif

// Audio file buffer size in frames per channel.
static const unsigned int c_iAudioBufSize = (4 * 1024);

// Peak file buffer size in frames per channel.
static const unsigned int c_iPeakBufSize = (4 * 1024);

// Default peak period as a digest representation in frames per channel.
static const unsigned short c_iPeakPeriod = 128;

// Fixed peak smoothing coeficient (exponential average).
static const float c_fPeakExpCoef = 0.5;

// Default peak filename extension.
static const QString c_sPeakFileExt = ".peak";


//----------------------------------------------------------------------
// struct qtractorAudioPeakHeader -- Audio Peak file header.
//

struct qtractorAudioPeakHeader
{
	unsigned short peakPeriod;
	unsigned short peakChannels;
	unsigned long  peakFrames;
};


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
	virtual void run();

	// Actual peak file creation methods.
	// (this is just about to be used internally)
	bool createPeakFileChunk();
	void writePeakFileFrame();
	void closePeakFile();

private:

	// Whether the thread is logically running.
	bool m_bRunState;

	// The peak file instance reference.
	qtractorAudioPeakFile *m_pPeakFile;
	// The audio file instance.
	qtractorAudioFile *m_pAudioFile;
	// Peak file creative variables.
	QFile   m_peakFile;
	float **m_ppAudioBuffer;
	float  *m_peakMax;
	float  *m_peakMin;
	float  *m_peakRms;
	unsigned short m_iPeriod;
	unsigned short m_iPeak;
};


// Constructor.
qtractorAudioPeakThread::qtractorAudioPeakThread (
	qtractorAudioPeakFile *pPeakFile )
	: m_peakFile(pPeakFile->name())
{
	m_bRunState = false;
	m_pPeakFile = pPeakFile;
	m_pAudioFile
		= qtractorAudioFileFactory::createAudioFile(pPeakFile->filename());

	m_ppAudioBuffer = NULL;
	m_peakMax = NULL;
	m_peakMin = NULL;
	m_peakRms = NULL;
	m_iPeriod = 0;
	m_iPeak   = 0;
}


// Destructor.
qtractorAudioPeakThread::~qtractorAudioPeakThread (void)
{
	closePeakFile();
}


// The main thread executive.
void qtractorAudioPeakThread::run (void)
{
#ifdef DEBUG
	fprintf(stderr, "qtractorAudioPeakThread::run(%p): started.\n", this);
#endif

	m_bRunState = true;

	// Create the peak file from the sample audio one...
	if (m_peakFile.open(IO_ReadWrite | IO_Truncate)) {
		// Open audio sample file...
		if (m_pAudioFile->open(m_pPeakFile->filename())) {
			unsigned short iChannels = m_pAudioFile->channels();
			unsigned long  iFrames   = m_pAudioFile->frames();
			// Allocate audio file frame buffer
			// and peak period accumulators.
			m_ppAudioBuffer = new float* [iChannels];
			m_peakMax = new float [2 * iChannels];
			m_peakMin = new float [2 * iChannels];
			m_peakRms = new float [2 * iChannels];
			for (unsigned short i = 0; i < iChannels; i++) {
				m_ppAudioBuffer[i] = new float [c_iAudioBufSize];
				m_peakMax[i] = m_peakMax[i + iChannels] = 0.0f;
				m_peakMin[i] = m_peakMin[i + iChannels] = 0.0f;
				m_peakRms[i] = m_peakRms[i + iChannels] = 0.0f;
			}
			m_iPeak = 0;
			// The resample-aware internal peak period...
			m_iPeriod = (unsigned short) (c_iPeakPeriod
				* float(m_pAudioFile->sampleRate())
				/ float(m_pPeakFile->sampleRate()));
			// Write peak file header.
			qtractorAudioPeakHeader hdr;
			hdr.peakPeriod   = c_iPeakPeriod;
			hdr.peakChannels = iChannels;
			hdr.peakFrames   = (iFrames / m_iPeriod);
			// Maybe not perfectly divisible...
			if (iFrames % m_iPeriod)
				hdr.peakFrames++;
			m_peakFile.writeBlock((const char *) &hdr, sizeof(hdr));
			// Go ahead with the whole bunch...
			while (m_bRunState && createPeakFileChunk());
		}
		// We're done.
		closePeakFile();
	}

	// May send notification event, anyway...
	if (m_pPeakFile->notifyWidget()) {
		QApplication::postEvent(m_pPeakFile->notifyWidget(),
			new QCustomEvent(m_pPeakFile->notifyPeakType(), m_pPeakFile));
	}

#ifdef DEBUG
	fprintf(stderr, "qtractorAudioPeakThread::run(%p): stopped.\n", this);
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
bool qtractorAudioPeakThread::createPeakFileChunk (void)
{
	if (m_ppAudioBuffer == NULL)
		return false;

	// Read another bunch of frames from the physical audio file...
	int nread = m_pAudioFile->read(m_ppAudioBuffer, c_iAudioBufSize);
	if (nread > 0) {
		unsigned short iChannels = m_pAudioFile->channels();
		for (unsigned int n = 0; n < (unsigned int) nread; n++) {
			for (unsigned short i = 0; i < iChannels; i++) {
				// Accumulate for this sample frame...
				float fSample = m_ppAudioBuffer[i][n];
				if (m_peakMax[i] < fSample)
					m_peakMax[i] = fSample;
				else if (m_peakMin[i] > fSample)
					m_peakMin[i] = fSample;
				m_peakRms[i] += (fSample * fSample);
			}
			// Have we reached the peak accumulative period?
			if (++m_iPeak >= m_iPeriod)
				writePeakFileFrame();
		}
	}

	// End of file reached? Bail out the remaining peak tuple...
	if (nread < 1 && m_iPeak > 0)
		writePeakFileFrame();

	return (nread > 0);
}

	
// Write a single peak frame and reset period accumulators.
void qtractorAudioPeakThread::writePeakFileFrame (void)
{
	qtractorAudioPeakFrame frame;

	// Each channel has a peak frame...
	unsigned short iChannels = m_pAudioFile->channels();
	for (unsigned short i = 0; i < iChannels; i++) {
		// Write the smoothed peak maximum value...
		unsigned short k = i + iChannels;
		m_peakMax[k]  = (1.0f - c_fPeakExpCoef) * m_peakMax[k]
			+ c_fPeakExpCoef * 254.0f * m_peakMax[i];
		frame.peakMax = (unsigned char) (m_peakMax[k] > 254.0f ? 255 : m_peakMax[k]);
		// Write the smoothed peak minimum value...
		m_peakMin[k]  = (1.0f - c_fPeakExpCoef) * m_peakMin[k]
			+ c_fPeakExpCoef * 254.0f * ::fabsf(m_peakMin[i]);
		frame.peakMin = (unsigned char) (m_peakMin[k] > 254.0f ? 255 : m_peakMin[k]);
		// Write the smoothed RMS value...
		m_peakRms[k]  = (1.0f - c_fPeakExpCoef) * m_peakRms[k]
			+ c_fPeakExpCoef * 254.0f * (::sqrtf(m_peakRms[i] / (float) m_iPeak));
		frame.peakRms = (unsigned char) (m_peakRms[k] > 254.0f ? 255 : m_peakRms[k]);
		// Bail out...
		m_peakFile.writeBlock((const char *) &frame, sizeof(frame));
		// Reset peak period accumulators...
		m_peakMax[i] = 0.0f;
		m_peakMin[i] = 0.0f;
		m_peakRms[i] = 0.0f;
	}
	// We'll reset.
	m_iPeak = 0;
}


// Close the (hopefully) created peak file.
void qtractorAudioPeakThread::closePeakFile (void)
{
	// Always force target file close.
	m_peakFile.close();

	// Get rid of physical used stuff.
	if (m_ppAudioBuffer) {
		unsigned short iChannels = m_pAudioFile->channels();
		for (unsigned short i = 0; i < iChannels; i++)
			delete [] m_ppAudioBuffer[i];
		delete [] m_ppAudioBuffer;
	}
	m_ppAudioBuffer = NULL;

	if (m_peakMax)
		delete [] m_peakMax;
	m_peakMax = NULL;

	if (m_peakMin)
		delete [] m_peakMin;
	m_peakMin = NULL;

	if (m_peakRms)
		delete [] m_peakRms;
	m_peakRms = NULL;

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
	qtractorAudioPeakFactory *pFactory, const QString& sFilename,
	unsigned int iSampleRate, const QString& sSessionDir )
	: m_peakFile( // Set peak filename...
		QFileInfo(sSessionDir, QFileInfo(sFilename).fileName()).filePath()
		+ c_sPeakFileExt)
{
	// Initialize instance variables.
	m_pFactory      = pFactory;
	m_sFilename     = sFilename;
	m_iSampleRate   = iSampleRate;

	m_iPeakPeriod   = 0;
	m_iPeakChannels = 0;
	m_iPeakFrames   = 0;
	m_iPeakOffset   = 0;
	m_iPeakBufSize  = 0;
	m_pPeakBuffer   = NULL;

	m_pPeakThread   = NULL;

	m_iRefCount     = 0;

}


// Default destructor.
qtractorAudioPeakFile::~qtractorAudioPeakFile (void)
{
	// Check if peak thread is still running.
	bool bAborted = false;
	if (m_pPeakThread) {
		// Try to wait for thread termination...
		m_pPeakThread->setRunState(false);
		if (m_pPeakThread->running()) {
		//	m_pPeakThread->terminate();
			m_pPeakThread->wait();
			bAborted = true;
		}
		delete m_pPeakThread;
		m_pPeakThread = NULL;
	}

	// Close the file, now.
	closePeakFile();

	// Tell master-factory that we're out.
	m_pFactory->removePeak(this);

	// Do we get rid from the filesystem too?
	if (m_pFactory->autoRemove() || bAborted)
		m_peakFile.remove();
}


// Open an existing peak file cache.
bool qtractorAudioPeakFile::openPeakFile (void)
{
	// If it's already open, just tell the news.
	if (m_peakFile.isOpen())
		return true;

	// Check if we're still on creative thread...
	if (m_pPeakThread) {
		// If running try waiting just 10 msec for it to finnish...
		if (m_pPeakThread->running()/*&& !m_pPeakThread->wait(10) */)
			return false;
		// OK. We're done with our creative thread.
		delete m_pPeakThread;
		m_pPeakThread = NULL;
	} else {
		// Need some preliminary file information...
		QFileInfo fileInfo(m_sFilename);
		QFileInfo peakInfo(m_peakFile.name());
		// Have we a peak file up-to-date,
		// or must the peak file be (re)created?
		if (!peakInfo.exists() ||
			peakInfo.lastModified() < fileInfo.lastModified()) {
			m_pPeakThread = new qtractorAudioPeakThread(this);
			m_pPeakThread->start();
			return false;
		}
	}

	// Just open and go ahead with first bunch...
	if (!m_peakFile.open(IO_Raw | IO_ReadOnly))
		return false;

	// Read peak file header.
	qtractorAudioPeakHeader hdr;
	if (m_peakFile.readBlock((char *) &hdr, sizeof(hdr))
		!= (Q_LONG) sizeof(hdr)) {
		m_peakFile.close();
		return false;
	}

#ifdef DEBUG_0
	fprintf(stderr, "--- openPeakFile ---\n");
	fprintf(stderr, "name       = %s\n", m_peakFile.name().latin1());
	fprintf(stderr, "filename   = %s\n", m_sFilename.latin1());
	fprintf(stderr, "sampleRate = %u\n", m_iSampleRate);
	fprintf(stderr, "header     = %d\n", sizeof(qtractorAudioPeakHeader));
	fprintf(stderr, "frame      = %d\n", sizeof(qtractorAudioPeakFrame));
	fprintf(stderr, "period     = %d\n", hdr.peakPeriod);
	fprintf(stderr, "channels   = %d\n", hdr.peakChannels);
	fprintf(stderr, "frames     = %lu\n", hdr.peakFrames);
	fprintf(stderr, "--------------------\n");
#endif

	//	Go ahead, it's already open.
	m_iPeakPeriod   = hdr.peakPeriod;
	m_iPeakChannels = hdr.peakChannels;
	m_iPeakFrames   = hdr.peakFrames;
	m_iPeakOffset   = 0;
	m_iPeakBufSize  = sizeof(qtractorAudioPeakFrame) * m_iPeakChannels
		* (m_iPeakFrames < c_iPeakBufSize ? m_iPeakFrames : c_iPeakBufSize);
	m_pPeakBuffer   = new char [m_iPeakBufSize];

	// Request the first (or only) bunch into the peak buffer.
	readPeakChunk();

	// Its a certain success...
	return true;
}


// Free all attended resources for this peak file.
void qtractorAudioPeakFile::closePeakFile (void)
{
	// Always force file close.
	m_peakFile.close();
	
	// And the logical stuff too...
	if (m_pPeakBuffer)
		delete [] m_pPeakBuffer;
	m_pPeakBuffer = NULL;
}


// Peak file accessors.
const QString& qtractorAudioPeakFile::filename (void) const
{
	return m_sFilename;
}

unsigned int qtractorAudioPeakFile::sampleRate (void) const
{
	return m_iSampleRate;
}

// Lazy-evaluated properties.
unsigned short qtractorAudioPeakFile::period (void)
{
	return (openPeakFile() ? m_iPeakPeriod : 0);
}

unsigned short qtractorAudioPeakFile::channels (void)
{
	return (openPeakFile() ? m_iPeakChannels : 0);
}

unsigned long qtractorAudioPeakFile::frames (void)
{
	return (openPeakFile() ? m_iPeakFrames : 0);
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
bool qtractorAudioPeakFile::autoRemove (void) const
{
	return m_pFactory->autoRemove();
}


//
// Time for some thoughts...
//
// Assuming that the peak file buffer is big enough for the most purposes,
// it will take the form of a sliding cache, that responds to arbitrary 
// segment requests. The cache effect is achieved if several requests are
// satisfied with in-memory data. If a given request cannot be satisfied 
// immediately, the actual data will be retrieved synchronously, but in
// complete and contiguous buffer chuncks.
// 

void qtractorAudioPeakFile::getPeak ( qtractorAudioPeakFrame *pframes, 
	unsigned long iframe, unsigned int nframes )
{
	// Read by lazy-openning the peak file...
	if (!openPeakFile())
		return;
	
	const unsigned int cFrameSize = sizeof(qtractorAudioPeakFrame);

	char         *pBuffer = (char *) pframes;
	unsigned long iOffset = cFrameSize * m_iPeakChannels * iframe;
	unsigned int  iLength = cFrameSize * m_iPeakChannels * nframes;
	unsigned int  nread   = 0;
	unsigned int  nahead  = iLength;
	
	while (nread < iLength) {
		if (nahead > m_iPeakBufSize)
			nahead = m_iPeakBufSize;
		readPeak(&pBuffer[nread], iOffset + nread, nahead);
		nread += nahead;
		nahead = iLength - nread;
	}
}

void qtractorAudioPeakFile::readPeak ( char *pBuffer, 
	unsigned long iOffset, unsigned int iLength )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioPeakFile::readPeak(%d,%d)\n", iOffset, iLength);
#endif

	// Excessive care to a zero buffer...
	if (iLength > m_iPeakBufSize) {
		::memset(&pBuffer[m_iPeakBufSize], 0, iLength - m_iPeakBufSize);	
		iLength = m_iPeakBufSize;
	}
	// Depending on the offset + length of the high-level request,
	// determine in if we need to invalidade the current cache buffer...
	unsigned int ndelta, ncache;
	if (iOffset < m_iPeakOffset) {
		if (iOffset + iLength > m_iPeakOffset) {
			// Backward cache request...
			ndelta = m_iPeakOffset - iOffset;
			::memcpy(&pBuffer[ndelta], &m_pPeakBuffer[0], iLength - ndelta);	    	
			if (m_iPeakOffset > m_iPeakBufSize)
				m_iPeakOffset -= m_iPeakBufSize;
			else
				m_iPeakOffset = 0;
			// Get the new bunch...
			readPeakChunk();
			ncache = iOffset - m_iPeakOffset;
			::memcpy(&pBuffer[0], &m_pPeakBuffer[ncache], ndelta);	    	
		} else {
			// Far-behind cache request...
			m_iPeakOffset = iOffset;
			readPeakChunk();
			::memcpy(&pBuffer[0], &m_pPeakBuffer[0], iLength);
		}
	}	// Full intra-cache...
	else if (iOffset + iLength <= m_iPeakOffset + m_iPeakBufSize) {
		::memcpy(&pBuffer[0], &m_pPeakBuffer[iOffset - m_iPeakOffset], iLength);
	}
	else if (iOffset > m_iPeakOffset + m_iPeakBufSize) {
		// Far-beyond cache request...
		m_iPeakOffset = iOffset;
		readPeakChunk();
		::memcpy(&pBuffer[0], &m_pPeakBuffer[0], iLength);
	} else {
		// Forward cache request...
		ndelta = iOffset - m_iPeakOffset;
		ncache = m_iPeakBufSize - ndelta;
		::memcpy(&pBuffer[0], &m_pPeakBuffer[ndelta], ncache);
		m_iPeakOffset = iOffset;
		readPeakChunk();
		::memcpy(&pBuffer[ncache], &m_pPeakBuffer[ncache], iLength - ncache);
	}
}


// Read a whole peak file buffer.
void qtractorAudioPeakFile::readPeakChunk (void)
{
	// Set position and read peak frames from there.
	int nread = 0;
	if (m_peakFile.at(sizeof(qtractorAudioPeakHeader) + m_iPeakOffset))
		nread = (int) m_peakFile.readBlock(&m_pPeakBuffer[0], m_iPeakBufSize);
	// Zero the remaining...
	if (nread < (int) m_iPeakBufSize)
		::memset(&m_pPeakBuffer[nread], 0, m_iPeakBufSize - nread);
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

unsigned long qtractorAudioPeak::frames (void) const
{
	return m_pPeakFile->frames();
}


// Peak file methods.
void qtractorAudioPeak::getPeak ( qtractorAudioPeakFrame *pframes,
	unsigned long iframe, unsigned int nframes )
{
	m_pPeakFile->getPeak(pframes, iframe, nframes);
}


//----------------------------------------------------------------------
// class qtractorAudioPeakFactory -- Audio peak file factory (singleton).
//

// Constructor.
qtractorAudioPeakFactory::qtractorAudioPeakFactory (void)
{
	m_peaks.setAutoDelete(false);

	m_pNotifyWidget   = NULL;
	m_eNotifyPeakType = QEvent::None;
	
	m_bAutoRemove = false;
}


// Default destructor.
qtractorAudioPeakFactory::~qtractorAudioPeakFactory (void)
{
	QDictIterator<qtractorAudioPeakFile> iter(m_peaks);
	for ( ; iter.current(); ++iter)
		delete iter.current();

	m_peaks.clear();
}


// The peak file factory-methods.
qtractorAudioPeak* qtractorAudioPeakFactory::createPeak (
	const QString& sFilename, unsigned int iSampleRate,
	const QString& sSessionDir )
{
	qtractorAudioPeakFile* pPeakFile = m_peaks.find(sFilename);
	if (pPeakFile == NULL) {
		pPeakFile = new qtractorAudioPeakFile(this,
			sFilename, iSampleRate, sSessionDir);
		m_peaks.insert(sFilename, pPeakFile);
	}

	return new qtractorAudioPeak(pPeakFile);
}

void qtractorAudioPeakFactory::removePeak ( qtractorAudioPeakFile *pPeakFile )
{
	m_peaks.remove(pPeakFile->filename());
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

bool qtractorAudioPeakFactory::autoRemove (void) const
{
	return m_bAutoRemove;
}


// end of qtractorAudioPeak.cpp
