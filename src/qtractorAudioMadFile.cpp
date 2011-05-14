// qtractorAudioMadFile.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioMadFile.h"

#include <sys/stat.h>


//----------------------------------------------------------------------
// class qtractorAudioMadFile -- Buffered audio file implementation.
//

// Constructor.
qtractorAudioMadFile::qtractorAudioMadFile ( unsigned int iBufferSize )
{
	// Initialize state variables.
	m_iMode        = qtractorAudioMadFile::None;
	m_pFile        = NULL;
	m_iBitRate     = 0;
	m_iChannels    = 0;
	m_iSampleRate  = 0;
	m_iFramesEst   = 0;
	m_bEndOfStream = false;

	// Input buffer stuff.
	m_iInputBufferSize = iBufferSize;
	m_pInputBuffer     = NULL;

	// Output ring-buffer stuff.
	m_iRingBufferSize  = 0;
	m_iRingBufferMask  = 0;
	m_iRingBufferRead  = 0;
	m_iRingBufferWrite = 0;
	m_ppRingBuffer     = NULL;

	// Frame mapping for sample-accurate seeking.
	m_iSeekOffset = 0;
}

// Destructor.
qtractorAudioMadFile::~qtractorAudioMadFile (void)
{
	close();
}


// Open method.
bool qtractorAudioMadFile::open ( const QString& sFilename, int iMode )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioMadFile::open(\"%s\", %d)",
		sFilename.toUtf8().constData(), iMode);
#endif
	close();

	// Whether for Read or Write... sorry only read is allowed.
	if (iMode != qtractorAudioMadFile::Read)
		return false;

	QByteArray aFilename = sFilename.toUtf8();
	m_pFile = ::fopen(aFilename.constData(), "rb");
	if (m_pFile == NULL)
		return false;

	// Create the decoded frame list.
	m_pFrameList = createFrameList(sFilename);
	if (m_pFrameList == NULL) {
		close();
		return false;
	}

#ifdef CONFIG_LIBMAD
	mad_stream_init(&m_madStream);
	mad_frame_init(&m_madFrame);
	mad_synth_init(&m_madSynth);
#endif  // CONFIG_LIBMAD

	int fdFile = fileno(m_pFile);
	struct stat st;
	if (::fstat(fdFile, &st) < 0 || st.st_size == 0) {
		close();
		return false;
	}

	// Read the very first bunch of raw-data...
	if (!input()) {
		close();
		return false;
	}

#ifdef CONFIG_LIBMAD
	if (mad_header_decode(&m_madFrame.header, &m_madStream) < 0) {
		if (m_madStream.error == MAD_ERROR_BUFLEN) {
			close();
			return false;
		}
		if (!MAD_RECOVERABLE(m_madStream.error)) {
			close();
			return false;
		}
	}
#endif  // CONFIG_LIBMAD

	// Do the very first frame decoding...
	m_bEndOfStream = !decode();

	// Get a rough estimate of the total decoded length of the file...
	if (m_iBitRate > 0) {
		m_iFramesEst = (unsigned long)
			((float) m_iSampleRate * st.st_size * 8.0f / (float) m_iBitRate);
	}

#ifdef DEBUG_0
	qDebug("qtractorAudioMadFile::open(\"%s\", %d) bit_rate=%u farmes_est=%lu",
		sFilename.toUtf8().constData(), iMode, m_iBitRate, m_iFramesEst);
#endif

	// Set open mode (deterministically).
	m_iMode = iMode;

	return true;
}


// Local input method.
bool qtractorAudioMadFile::input (void)
{
#ifdef DEBUG_0
	qDebug("qtractorAudioMadFile::input()");
#endif

#ifdef CONFIG_LIBMAD

	// Can't go on if EOF.
	if (feof(m_pFile))
		return false;

	// Allocate input buffer if not already.
	if (m_pInputBuffer == NULL) {
		unsigned int iBufferSize = (4096 << 1);
		while (iBufferSize < m_iInputBufferSize)
			iBufferSize <<= 1;
		m_iInputBufferSize = iBufferSize;
		m_pInputBuffer = new unsigned char [iBufferSize + MAD_BUFFER_GUARD];
		m_curr.iInputOffset = 0;
	}

	unsigned long  iRemaining;
	unsigned char *pReadStart;
	unsigned long  iReadSize;

	if (m_madStream.next_frame) {
		iRemaining = m_madStream.bufend - m_madStream.next_frame;
		::memmove(m_pInputBuffer, m_madStream.next_frame, iRemaining);
		pReadStart = m_pInputBuffer + iRemaining;
		iReadSize  = m_iInputBufferSize - iRemaining;
	} else {
		iRemaining = 0;
		pReadStart = m_pInputBuffer;
		iReadSize  = m_iInputBufferSize;
	}

	long iRead = ::fread(pReadStart, 1, iReadSize, m_pFile);
	if (iRead > 0) {
		// Update the input offset, as for next time...
		m_curr.iInputOffset += iRead;
		// Time to add some frame mapping, on each 3rd iteration...
		if ((++m_curr.iDecodeCount % 3) == 0 && (m_pFrameList->count() < 1
			|| m_pFrameList->last().iOutputOffset < m_curr.iOutputOffset)) {
			unsigned long iInputOffset = m_curr.iInputOffset - iRemaining;
			m_pFrameList->append(FrameNode(iInputOffset,
				m_curr.iOutputOffset, m_curr.iDecodeCount));
		}
		// Add some decode buffer guard...
		if (iRead < (int) iReadSize) {
			::memset(pReadStart + iRead, 0, MAD_BUFFER_GUARD);
			iRead += MAD_BUFFER_GUARD;
		}
		mad_stream_buffer(&m_madStream, m_pInputBuffer, iRead + iRemaining);
	}

	return (iRead > 0);

#else	// CONFIG_LIBMAD

	return false;

#endif
}


// Local decode method.
bool qtractorAudioMadFile::decode (void)
{
#ifdef DEBUG_0
	qDebug("qtractorAudioMadFile::decode()");
#endif

#ifdef CONFIG_LIBMAD

	bool bError = (mad_frame_decode(&m_madFrame, &m_madStream) < 0);
	while (bError && (m_madStream.error == MAD_ERROR_BUFLEN
		|| MAD_RECOVERABLE(m_madStream.error))) {
		if (!input())
			return false;
		bError = (mad_frame_decode(&m_madFrame, &m_madStream) < 0);
	}

#ifdef DEBUG_0
	if (bError) {
		qDebug("qtractorAudioMadFile::decode()"
			" ERROR[%lu]: madStream.error=%d (0x%04x)",
			m_curr.iOutputOffset,
			m_madStream.error,
			m_madStream.error);
	}
#endif

	if (bError)
		return MAD_RECOVERABLE(m_madStream.error);

	mad_synth_frame(&m_madSynth, &m_madFrame);

	unsigned int iFrames = m_madSynth.pcm.length;
	if (m_ppRingBuffer == NULL) {
		// Set initial stream parameters.
		m_iBitRate    = m_madFrame.header.bitrate;
		m_iChannels   = m_madSynth.pcm.channels;
		m_iSampleRate = m_madSynth.pcm.samplerate;
		// Create/allocate internal output ring-buffer.
		m_iRingBufferSize = (4096 << 1);
		while (m_iRingBufferSize < m_iInputBufferSize)
			m_iRingBufferSize <<= 1;
		m_iRingBufferMask = (m_iRingBufferSize - 1);
		// Allocate actual buffer stuff...
		m_ppRingBuffer = new float* [m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; ++i)
			m_ppRingBuffer[i] = new float [m_iRingBufferSize];
		// Reset ring-buffer pointers.
		m_iRingBufferRead  = 0;
		m_iRingBufferWrite = 0;
		// Decoder mapping initialization.
		m_curr.iInputOffset  = 0;
		m_curr.iOutputOffset = 0;
		m_curr.iDecodeCount  = 0;
	}

	const float fScale = (float) (1L << MAD_F_FRACBITS);
	for (unsigned int n = 0; n < iFrames; ++n) {
		if (m_curr.iOutputOffset >= m_iSeekOffset) {
			for (unsigned short i = 0; i < m_iChannels; ++i) {
				int iSample = bError ? 0 : *(m_madSynth.pcm.samples[i] + n);
				m_ppRingBuffer[i][m_iRingBufferWrite] = (float) iSample / fScale;
			}
			++m_iRingBufferWrite &= m_iRingBufferMask;
		}
#ifdef DEBUG_0
		else if (n == 0) {
			qDebug("qtractorAudioMadFile::decode(%lu) i=%lu o=%lu c=%u",
				m_iSeekOffset,
				m_curr.iInputOffset,
				m_curr.iOutputOffset,
				m_curr.iDecodeCount);
		}
#endif
		++m_curr.iOutputOffset;
	}

	return true;

#else	// CONFIG_LIBMAD

	return false;

#endif
}


// Read method.
int qtractorAudioMadFile::read ( float **ppFrames, unsigned int iFrames )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioMadFile::read(%p, %d)", ppFrames, iFrames);
#endif

	unsigned int nread = 0;

	if (m_ppRingBuffer) {
		if (iFrames > (m_iRingBufferSize >> 1))
			iFrames = (m_iRingBufferSize >> 1);
		while ((nread = readable()) < iFrames && !m_bEndOfStream)
			m_bEndOfStream = !decode();
		if (nread > iFrames)
			nread = iFrames;
		// Move the data around...
		unsigned int r = m_iRingBufferRead;
		unsigned int n1, n2;
		if (r + nread > m_iRingBufferSize) {
			n1 = (m_iRingBufferSize - r);
			n2 = (r + nread) & m_iRingBufferMask;
		} else {
			n1 = nread;
			n2 = 0;
		}
		for (unsigned short i = 0; i < m_iChannels; ++i) {
			::memcpy(ppFrames[i], (float *)(m_ppRingBuffer[i] + r),
				n1 * sizeof(float));
			if (n2 > 0) {
				::memcpy((float *)(ppFrames[i] + n1), m_ppRingBuffer[i],
					n2 * sizeof(float));
			}
		}
		m_iRingBufferRead = (r + nread) & m_iRingBufferMask;
		m_iSeekOffset += nread;
	}

	return nread;
}


// Write method.
int qtractorAudioMadFile::write ( float **/*ppFrames*/, unsigned int /*iFrames*/ )
{
	return 0;
}


// Seek method.
bool qtractorAudioMadFile::seek ( unsigned long iOffset )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioMadFile::seek(%lu)", iOffset);
#endif

	// Avoid unprecise seeks...
	if (iOffset == m_iSeekOffset)
		return true;

	// This is the target situation...
	m_iSeekOffset = iOffset;

	// Are qe seeking backward or forward 
	// from last known decoded position?
	if (m_pFrameList->count() > 0
		&& m_pFrameList->last().iOutputOffset > iOffset) {
		// Assume the worst case (seek to very beggining...)
		m_curr.iInputOffset  = 0;
		m_curr.iOutputOffset = 0;
		m_curr.iDecodeCount  = 0;
		// Find the previous mapped 3rd frame that fits location...
		QListIterator<FrameNode> iter(*m_pFrameList);
		iter.toBack();
		while (iter.hasPrevious()) {
			if (iter.previous().iOutputOffset < iOffset) {
				if (iter.hasPrevious())
					m_curr = iter.previous();
				break;
			}
		}
#ifdef DEBUG_0
		qDebug("qtractorAudioMadFile::seek(%lu) i=%lu o=%lu c=%u",
			iOffset,
			m_curr.iInputOffset,
			m_curr.iOutputOffset,
			m_curr.iDecodeCount);
#endif
		// Rewind file position...
		if (::fseek(m_pFile, m_curr.iInputOffset, SEEK_SET))
			return false;
#ifdef CONFIG_LIBMAD
		// Release MAD structs...
		mad_synth_finish(&m_madSynth);
		mad_frame_finish(&m_madFrame);
		mad_stream_finish(&m_madStream);
		// Reset MAD structs...
		mad_stream_init(&m_madStream);
		mad_frame_init(&m_madFrame);
		mad_synth_init(&m_madSynth);
#endif	// CONFIG_LIBMAD
		// Reread first seeked input bunch...
		if (!input())
			return false;
	}

	// Reset ring-buffer pointers.
	m_iRingBufferRead  = 0;
	m_iRingBufferWrite = 0;
	m_bEndOfStream = false;

	// Now loop until we find the target offset...
	while (m_curr.iOutputOffset < m_iSeekOffset && !m_bEndOfStream)
		m_bEndOfStream = !decode();

	return !m_bEndOfStream;
}


// Close method.
void qtractorAudioMadFile::close (void)
{
#ifdef DEBUG_0
	qDebug("qtractorAudioMadFile::close()");
#endif

	// Free allocated buffers, if any.
	if (m_ppRingBuffer) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			delete [] m_ppRingBuffer[i];
		delete [] m_ppRingBuffer;
		m_ppRingBuffer = NULL;
	}

	if (m_pInputBuffer) {
		delete [] m_pInputBuffer;
		m_pInputBuffer = NULL;
	}

	if (m_pFile) {
#ifdef CONFIG_LIBMAD
		mad_synth_finish(&m_madSynth);
		mad_frame_finish(&m_madFrame);
		mad_stream_finish(&m_madStream);
#endif  // CONFIG_LIBMAD
		::fclose(m_pFile);
		m_pFile = NULL;
	}

	// Frame lists are never destroyed here
	// (they're cached for whole life-time of the program).
	m_pFrameList = NULL;

	// Reset all other state relevant variables.
	m_bEndOfStream = false;
	m_iFramesEst   = 0;
	m_iSampleRate  = 0;
	m_iChannels    = 0;
	m_iBitRate     = 0;
	m_iMode        = qtractorAudioMadFile::None;
}


// Open mode accessor.
int qtractorAudioMadFile::mode (void) const
{
	return m_iMode;
}


// Open channel(s) accessor.
unsigned short qtractorAudioMadFile::channels (void) const
{
	return m_iChannels;
}


// Estimated number of frames specialty (aprox. 8secs).
unsigned long qtractorAudioMadFile::frames (void) const
{
	return m_iFramesEst;
}


// Sample rate specialty.
unsigned int qtractorAudioMadFile::sampleRate (void) const
{
	return m_iSampleRate;
}


// Internal ring-buffer helper methods.
unsigned int qtractorAudioMadFile::readable (void) const
{
	unsigned int w = m_iRingBufferWrite;
	unsigned int r = m_iRingBufferRead;
	if (w > r) {
		return (w - r);
	} else {
		return (w - r + m_iRingBufferSize) & m_iRingBufferMask;
	}
}


unsigned int qtractorAudioMadFile::writable (void) const
{
	unsigned int w = m_iRingBufferWrite;
	unsigned int r = m_iRingBufferRead;
	if (w > r){
		return ((r - w + m_iRingBufferSize) & m_iRingBufferMask) - 1;
	} else if (r > w) {
		return (r - w) - 1;
	} else {
		return m_iRingBufferSize - 1;
	}
}


// Frame list factory method.
qtractorAudioMadFile::FrameList *qtractorAudioMadFile::createFrameList (
	const QString& sFilename )
{
	// Frame list hash repository
	// (declared here for proper cleanup).
	class FrameListFactory : public QHash<QString, FrameList *>
	{
	public:
		// Destructor.
		~FrameListFactory()	{
			QMutableHashIterator<QString, FrameList *> iter(*this);
			while (iter.hasNext()) {
				FrameList *pFrameList = iter.next().value();
				iter.remove();
				delete pFrameList;
			}
		}
	};

	// Do the factory thing here...
	static FrameListFactory s_lists;

	FrameList *pFrameList = s_lists.value(sFilename, NULL);
	if (pFrameList == NULL) {
		pFrameList = new FrameList();
		s_lists.insert(sFilename, pFrameList);
	}

	return pFrameList;
}


// end of qtractorAudioMadFile.cpp
