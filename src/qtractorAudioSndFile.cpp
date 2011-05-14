// qtractorAudioSndFile.cpp
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
#include "qtractorAudioSndFile.h"


//----------------------------------------------------------------------
// class qtractorAudioSndFile -- Buffered audio file implementation.
//

// Constructor.
qtractorAudioSndFile::qtractorAudioSndFile ( unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	// Need a minimum of specification, at least for write mode.
	::memset(&m_sfinfo, 0, sizeof(m_sfinfo));
	m_sfinfo.channels   = iChannels;
	m_sfinfo.samplerate = iSampleRate;

	// Initialize other stuff.
	m_pSndFile    = NULL;
	m_iMode       = qtractorAudioSndFile::None;
	m_pBuffer     = NULL;
	m_iBufferSize = 1024;

	// Adjust size the next nearest power-of-two.
	while (m_iBufferSize < iBufferSize)
		m_iBufferSize <<= 1;
}

// Destructor.
qtractorAudioSndFile::~qtractorAudioSndFile (void)
{
	close();
}


// Open method.
bool qtractorAudioSndFile::open ( const QString& sFilename, int iMode )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioSndFile::open(\"%s\", %d)",
		sFilename.toUtf8().constData(), iMode);
#endif
	close();

	// Whether for Read or Write...
	int sfmode;
	switch (iMode) {
	case qtractorAudioSndFile::Read:
		sfmode = SFM_READ;
		break;
	case qtractorAudioSndFile::Write:
		sfmode = SFM_WRITE;
		break;
	default:
		return false;
	}

	// As said, need a minimum of specification for write mode.
	if (sfmode & SFM_WRITE) {
		if (m_sfinfo.channels == 0 || m_sfinfo.samplerate == 0)
			return false;
		m_sfinfo.format = qtractorAudioFileFactory::defaultFormat();
	}

	// Now open it.
	QByteArray aFilename = sFilename.toUtf8();
	m_pSndFile = ::sf_open(aFilename.constData(), sfmode, &m_sfinfo);
	if (m_pSndFile == NULL)
		return false;

	// Set open mode (deterministically).
	m_iMode = iMode;

	// Allocate initial de/interleaving buffer stuff.
	m_pBuffer = new float [m_sfinfo.channels * m_iBufferSize];

	return true;
}


// Read method.
int qtractorAudioSndFile::read ( float **ppFrames, unsigned int iFrames )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioSndFile::read(%p, %d)", ppFrames, iFrames);
#endif
	allocBufferCheck(iFrames);
	int nread = ::sf_readf_float(m_pSndFile, m_pBuffer, iFrames);
	if (nread > 0) {
		unsigned short i;
		unsigned int n, k = 0;
		for (n = 0; n < (unsigned int) nread; ++n) {
			for (i = 0; i < (unsigned short) m_sfinfo.channels; ++i)
				ppFrames[i][n] = m_pBuffer[k++];
		}
	}
	return nread;
}


// Write method.
int qtractorAudioSndFile::write ( float **ppFrames, unsigned int iFrames )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioSndFile::write(%p, %d)", ppFrames, iFrames);
#endif
	allocBufferCheck(iFrames);
	unsigned short i;
	unsigned int n, k = 0;
	for (n = 0; n < iFrames; ++n) {
		for (i = 0; i < (unsigned short) m_sfinfo.channels; ++i)
			m_pBuffer[k++] = ppFrames[i][n];
	}
	return ::sf_writef_float(m_pSndFile, m_pBuffer, iFrames);
}


// Seek method.
bool qtractorAudioSndFile::seek ( unsigned long iOffset )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioSndFile::seek(%d)", iOffset);
#endif
	return (::sf_seek(m_pSndFile, iOffset, SEEK_SET) == long(iOffset));
}


// Close method.
void qtractorAudioSndFile::close (void)
{
#ifdef DEBUG_0
	qDebug("qtractorAudioSndFile::close()");
#endif

	if (m_pSndFile) {
		::sf_close(m_pSndFile);
		m_pSndFile = NULL;
		m_iMode = qtractorAudioSndFile::None;
	}

	if (m_pBuffer) {
		delete [] m_pBuffer;
		m_pBuffer = NULL;
	}
}


// Open mode accessor.
int qtractorAudioSndFile::mode (void) const
{
	return m_iMode;
}


// Open channel(s) accessor.
unsigned short qtractorAudioSndFile::channels (void) const
{
	return m_sfinfo.channels;
}


// Total number of frames specialty.
unsigned long qtractorAudioSndFile::frames (void) const
{
	return m_sfinfo.frames;
}


// Sample rate specialty.
unsigned int qtractorAudioSndFile::sampleRate (void) const
{
	return m_sfinfo.samplerate;
}


// De/interleaving buffer stuff.
void qtractorAudioSndFile::allocBufferCheck ( unsigned int iBufferSize )
{
	// Only reallocate a new buffer if new size is greater...
	if (iBufferSize > m_iBufferSize && m_sfinfo.channels > 0) {
		// Destroy previously existing buffer.
		if (m_pBuffer)
			delete [] m_pBuffer;
		// Allocate new extended one; again,
		// adjust size the next nearest power-of-two.
		while (m_iBufferSize < iBufferSize)
			m_iBufferSize <<= 1;
		m_pBuffer = new float [m_sfinfo.channels * m_iBufferSize];
	}
}


// end of qtractorAudioSndFile.cpp
