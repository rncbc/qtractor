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

#ifdef CONFIG_LIBMAD
#include <sys/stat.h>
#include <sys/mman.h>
#endif


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
	m_iSeekOffset  = 0;
	m_fSeekRatio   = 0.0;
	m_bEndOfStream = false;
#ifdef CONFIG_LIBMAD
	m_pFileMmap    = NULL;
#endif
	m_iBufferSize  = iBufferSize;
	m_iBufferMask  = 0;
	m_iReadIndex   = 0;
	m_iWriteIndex  = 0;
	m_ppBuffer     = NULL;
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

	int fdFile = ::fileno(m_pFile);
	struct stat st;
	if (::fstat(fdFile, &st) < 0 || st.st_size == 0) {
		close();
		return false;
	}

	m_iFileSize = st.st_size;
	m_pFileMmap = ::mmap(0, m_iFileSize, PROT_READ, MAP_SHARED, fdFile, 0);
	if (m_pFileMmap == MAP_FAILED) {
		close();
		return false;
	}

	mad_stream_buffer(&m_madStream, (unsigned char *) m_pFileMmap, m_iFileSize);

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


// Local decode method.
bool qtractorAudioMadFile::decode (void)
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::decode() ws=%d rs=%d\n", writable(), readable());
#endif

#ifdef CONFIG_LIBMAD

	bool bError = (mad_frame_decode(&m_madFrame, &m_madStream) < 0);
	if (bError) {
		if (m_madStream.error == MAD_ERROR_BUFLEN)
			return false;
		if (!MAD_RECOVERABLE(m_madStream.error))
		    return false;
	}

	mad_synth_frame(&m_madSynth, &m_madFrame);

	unsigned int iFrames = m_madSynth.pcm.length;
	if (m_ppBuffer == NULL) {
		m_iBitRate    = m_madFrame.header.bitrate;
		m_iChannels   = m_madSynth.pcm.channels;
		m_iSampleRate = m_madSynth.pcm.samplerate;
		m_iSeekOffset = 0;
		m_fSeekRatio  = 0.0;
		if (m_iSampleRate > 0)
			m_fSeekRatio = (float) m_iBitRate / (8.0 * m_iSampleRate);
		createBuffer(iFrames << 4);
	}

	const unsigned long iScale = (1 << 28);
	for (unsigned int n = 0; n < iFrames; n++) {
		for (unsigned short i = 0; i < m_iChannels; i++) {
			int iSample = bError ? 0 : *(m_madSynth.pcm.samples[i] + n);
			m_ppBuffer[i][m_iWriteIndex] = (float) iSample / iScale;
		}
		++m_iWriteIndex &= m_iBufferMask;
	}

	return true;

#else

	return false;

#endif  // CONFIG_LIBMAD
}


// Read method.
int qtractorAudioMadFile::read ( float **ppFrames,
	unsigned int iFrames )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::read(%p, %d)", ppFrames, iFrames);
#endif

	unsigned int nread = 0;
	if (m_ppBuffer) {
		if (iFrames > m_iBufferSize >> 1)
			iFrames = m_iBufferSize >> 1;
		while ((nread = readable()) < iFrames && !m_bEndOfStream)
			m_bEndOfStream = !decode();
		if (nread > iFrames)
			nread = iFrames;
    	// Move the data around...
		unsigned int ri = m_iReadIndex;
		unsigned int n1, n2;
		if (ri + nread > m_iBufferSize) {
			n1 = (m_iBufferSize - ri);
			n2 = (ri + nread) & m_iBufferMask;
		} else {
			n1 = nread;
			n2 = 0;
		}
		for (unsigned short i = 0; i < m_iChannels; i++) {
			::memcpy(ppFrames[i], (float *)(m_ppBuffer[i] + ri),
				n1 * sizeof(float));
			if (n2 > 0) {
				::memcpy((float *)(ppFrames[i] + n1), m_ppBuffer[i],
					n2 * sizeof(float));
			}
		}
		m_iReadIndex = (ri + nread) & m_iBufferMask;
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
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::write(%p, %d)\n", ppFrames, iFrames);
#endif
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

#ifdef CONFIG_LIBMAD

	m_madStream.next_frame = m_madStream.buffer
		+ (unsigned long) (m_fSeekRatio * iOffset);
	mad_stream_sync(&m_madStream);

#endif  // CONFIG_LIBMAD

	// Reset ring-buffer pointers.
	m_iSeekOffset = iOffset;
	m_iReadIndex  = 0;
	m_iWriteIndex = 0;

	m_bEndOfStream = !decode();

	return !m_bEndOfStream;
}


// Close method.
void qtractorAudioMadFile::close()
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::close()\n");
#endif

	deleteBuffer();

#ifdef CONFIG_LIBMAD

	if (m_pFile) {
		mad_synth_finish(&m_madSynth);
		mad_frame_finish(&m_madFrame);
		mad_stream_finish(&m_madStream);
	}

	if (m_pFileMmap) {
		::munmap(m_pFileMmap, m_iFileSize);
		m_pFileMmap = NULL;
	}

#endif  // CONFIG_LIBMAD

	if (m_pFile) {
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
void qtractorAudioMadFile::createBuffer ( unsigned int iBufferSize )
{
	deleteBuffer();

#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::createBuffer(%d)\n", iBufferSize);
#endif

	// The size overflow convenience mask.
	m_iBufferSize = 4096;
	while (m_iBufferSize < iBufferSize)
		m_iBufferSize <<= 1;
	m_iBufferMask = (m_iBufferSize - 1);
	// Allocate actual buffer stuff...
	m_ppBuffer = NULL;
	if (m_iBufferSize > 0) {
		m_ppBuffer = new float* [m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; i++)
			m_ppBuffer[i] = new float [m_iBufferSize];
	}
	// Reset ring-buffer pointers.
	m_iReadIndex  = 0;
	m_iWriteIndex = 0;
}

void qtractorAudioMadFile::deleteBuffer (void)
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioMadFile::deleteBuffer()\n");
#endif

	// Deallocate any buffer stuff...
	if (m_ppBuffer) {
		for (unsigned short i = 0; i < m_iChannels; i++)
			delete [] m_ppBuffer[i];
		delete [] m_ppBuffer;
		m_ppBuffer = NULL;
	}
}


unsigned int qtractorAudioMadFile::readable (void) const
{
	unsigned int wi = m_iWriteIndex;
	unsigned int ri = m_iReadIndex;
	if (wi > ri) {
		return (wi - ri);
	} else {
		return (wi - ri + m_iBufferSize) & m_iBufferMask;
	}
}


unsigned int qtractorAudioMadFile::writable (void) const
{
	unsigned int wi = m_iWriteIndex;
	unsigned int ri = m_iReadIndex;
	if (wi > ri){
		return ((ri - wi + m_iBufferSize) & m_iBufferMask) - 1;
	} else if (ri > wi) {
		return (ri - wi) - 1;
	} else {
		return m_iBufferSize - 1;
	}
}


// end of qtractorAudioMadFile.cpp
