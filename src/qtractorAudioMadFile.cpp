// qtractorAudioMadFile.cpp
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

#include "qtractorAudioMadFile.h"

#include <sys/stat.h>


//----------------------------------------------------------------------
// class qtractorAudioMadFile -- Buffered audio file implementation.
//

// Constructor.
qtractorAudioMadFile::qtractorAudioMadFile ( unsigned int iBufferSize )
{
	// Initialize state variables.
	m_iMode = qtractorAudioMadFile::None;

	m_iChannels    = 0;
	m_iSampleRate  = 0;
	m_iBitRate     = 0;
	m_iFramesEst   = 0;
	m_pFile        = NULL;
	m_iFileSize    = 0;
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
	m_iInputOffset  = 0;
	m_iOutputOffset = 0;
	m_iSeekOffset   = 0;
	m_iDecodeFrame  = 0;
}

// Destructor.
qtractorAudioMadFile::~qtractorAudioMadFile (void)
{
	close();
}


// Open method.
bool qtractorAudioMadFile::open ( const char *pszName, int iMode )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::open(\"%s\", %d)\n", pszName, iMode);
#endif
	close();

	// Whether for Read or Write... sorry only read is allowed.
	if (iMode != qtractorAudioMadFile::Read)
		return false;

	m_pFile = ::fopen(pszName, "rb");
	if (m_pFile == NULL)
		return false;
		
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
	m_iFileSize = st.st_size;

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
			((float) m_iSampleRate * m_iFileSize * 8.0 / (float) m_iBitRate);
	}

	// Set open mode (deterministically).
	m_iMode = iMode;

	return true;
}


// Local input method.
bool qtractorAudioMadFile::input (void)
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::input()\n");
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
		m_iInputOffset = 0;
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
		m_iInputOffset += iRead;
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
	fprintf(stderr, "qtractorAudioMadFile::decode()\n");
#endif

#ifdef CONFIG_LIBMAD

	bool bError = (mad_frame_decode(&m_madFrame, &m_madStream) < 0);
	if (bError) {
		if (m_madStream.error == MAD_ERROR_BUFLEN) {
			if (!input())
				return false;
			bError = (mad_frame_decode(&m_madFrame, &m_madStream) < 0);
		}
		if (bError && !MAD_RECOVERABLE(m_madStream.error))
			return false;
	}

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
		for (unsigned short i = 0; i < m_iChannels; i++)
			m_ppRingBuffer[i] = new float [m_iRingBufferSize];
		// Reset ring-buffer pointers.
		m_iRingBufferRead  = 0;
		m_iRingBufferWrite = 0;
		// Decoder mapping initialization.
		m_iOutputOffset = 0;
		m_iSeekOffset   = 0;
		m_iDecodeFrame  = 0;
		m_frames.clear();
	}

	const float fScale = (float) (1L << MAD_F_FRACBITS);
	for (unsigned int n = 0; n < iFrames; n++) {
		if (m_iOutputOffset >= m_iSeekOffset) {
			for (unsigned short i = 0; i < m_iChannels; i++) {
				int iSample = bError ? 0 : *(m_madSynth.pcm.samples[i] + n);
				m_ppRingBuffer[i][m_iRingBufferWrite] = (float) iSample / fScale;
			}
			++m_iRingBufferWrite &= m_iRingBufferMask;
		}
		++m_iOutputOffset;
	}

	if ((m_frames.count() < 1
		|| m_frames.last().iOutputOffset < m_iOutputOffset)) {
		// Only do mapping accounting each other 3rd decoded frame...
		if ((++m_iDecodeFrame % 3) == 0) {
			unsigned long iInputOffset = m_iInputOffset
				- (m_madStream.bufend - m_madStream.next_frame);
			m_frames.append(FrameNode(iInputOffset, m_iOutputOffset));
		}
	}

	return true;

#else	// CONFIG_LIBMAD

	return false;

#endif
}


// Read method.
int qtractorAudioMadFile::read ( float **ppFrames,
	unsigned int iFrames )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::read(%p, %d)", ppFrames, iFrames);
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
		for (unsigned short i = 0; i < m_iChannels; i++) {
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

#ifdef DEBUG_0
	fprintf(stderr, " --> nread=%d\n", nread);
#endif

	return nread;
}


// Write method.
int qtractorAudioMadFile::write ( float ** /* ppFrames */,
	unsigned int /* iFrames */ )
{
	return 0;
}


// Seek method.
bool qtractorAudioMadFile::seek ( unsigned long iOffset )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::seek(%lu)\n", iOffset);
#endif

	// Avoid unprecise seeks...
	if (iOffset == m_iSeekOffset)
		return true;

	// This is the target situation...
	m_iSeekOffset = iOffset;

	// Are qe seeking backward or forward 
	// from last known decoded position?
	if (m_frames.count() > 0 && m_frames.last().iOutputOffset > iOffset) {
		// assume the worst case (seek to very beggining...)
		m_iInputOffset  = 0;
		m_iOutputOffset = 0;
		// Find the previous mapped 3rd frame that fits location...
		FrameList::ConstIterator iter = m_frames.fromLast();
		while (--iter != m_frames.begin()) {
			const FrameNode& frame = *iter;
			if (frame.iOutputOffset < iOffset) {
				m_iInputOffset  = frame.iInputOffset;
				m_iOutputOffset = frame.iOutputOffset;
				break;
			}
		}
		// Rewind file position...
		if (::fseek(m_pFile, m_iInputOffset, SEEK_SET))
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

	// Now loop until we find the target offset...
	while (m_iOutputOffset < m_iSeekOffset && !m_bEndOfStream)
		m_bEndOfStream = !decode();

	return !m_bEndOfStream;
}


// Close method.
void qtractorAudioMadFile::close()
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::close()\n");
#endif

	if (m_ppRingBuffer) {
		for (unsigned short i = 0; i < m_iChannels; i++)
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
}


// Open mode accessor.
int qtractorAudioMadFile::mode() const
{
	return m_iMode;
}


// Open channel(s) accessor.
unsigned short qtractorAudioMadFile::channels() const
{
	return m_iChannels;
}


// Estimated number of frames specialty (aprox. 8secs).
unsigned long qtractorAudioMadFile::frames() const
{
	return m_iFramesEst;
}


// Sample rate specialty.
unsigned int qtractorAudioMadFile::samplerate() const
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


// end of qtractorAudioMadFile.cpp
