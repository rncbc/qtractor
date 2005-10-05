// qtractorAudioVorbisFile.cpp
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

#include "qtractorAudioVorbisFile.h"


//----------------------------------------------------------------------
// class qtractorAudioVorbisFile -- Buffered audio file implementation.
//

// Constructor.
qtractorAudioVorbisFile::qtractorAudioVorbisFile ( unsigned int iBufferSize )
{
	// Initialize state variables.
	m_iMode  = qtractorAudioVorbisFile::None;
	m_pFile  = NULL;
	m_ovinfo = NULL;
	m_ovsect = 0;

	// Estimate buffer size.
	m_iBufferSize = 4096;
	// Adjust size the next nearest power-of-two.
	while (m_iBufferSize < iBufferSize)
		m_iBufferSize <<= 1;
}

// Destructor.
qtractorAudioVorbisFile::~qtractorAudioVorbisFile (void)
{
	close();
}


// Open method.
bool qtractorAudioVorbisFile::open ( const char *pszName, int iMode )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioVorbisFile::open(\"%s\", %d)\n", pszName, iMode);
#endif
	close();

	// Whether for Read or Write... sorry only read is allowed.
	if (iMode != qtractorAudioVorbisFile::Read)
		return false;

	// Now open it.
	m_pFile = ::fopen(pszName, "rb");
	if (m_pFile == NULL)
		return false;

	// Now's time for the vorbis stuff...
	if (::ov_open(m_pFile, &m_ovfile, NULL, 0) < 0) {
		close();
		return false;
	}

	// Set open mode (deterministically).
	m_iMode = iMode;

	// Grab the vorbis file info...
	m_ovinfo = ::ov_info(&m_ovfile, -1);
	m_ovsect = 0;

	return true;
}


// Read method.
int qtractorAudioVorbisFile::read ( float **ppFrames, unsigned int iFrames )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioVorbisFile::read(%p, %d)", ppFrames, iFrames);
#endif
	float **ppBuffer;
	int nread = ::ov_read_float(&m_ovfile, &ppBuffer, iFrames, &m_ovsect);
	if (nread < 0)	// HACK: Just in case things get go thru...
		nread = ::ov_read_float(&m_ovfile, &ppBuffer, iFrames, &m_ovsect);
	if (nread > 0) {
		unsigned short i;
		for (i = 0; i < (unsigned short) m_ovinfo->channels; i++) {
			::memcpy(ppFrames[i], ppBuffer[i], nread * sizeof(float));
		}
	}
#ifdef DEBUG_0
	fprintf(stderr, " --> nread=%d\n", nread);
#endif
	return nread;
}


// Write method.
int qtractorAudioVorbisFile::write ( float **ppFrames, unsigned int iFrames )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioVorbisFile::write(%p, %d)\n", ppFrames, iFrames);
#endif
	return 0;
}

// Seek method.
bool qtractorAudioVorbisFile::seek ( unsigned long iOffset )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioVorbisFile::seek(%d)\n", iOffset);
#endif
	return (::ov_pcm_seek(&m_ovfile, iOffset) == 0);
}


// Close method.
void qtractorAudioVorbisFile::close()
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorAudioVorbisFile::close()\n");
#endif

	if (m_pFile) {
		::ov_clear(&m_ovfile);
	//	::fclose(m_pFile); -- already closed on ov_clear?
		m_pFile = NULL;
		m_ovinfo = NULL;
		m_iMode = qtractorAudioVorbisFile::None;
	}

	// Reinitialize libvorbisfile stuff.
	::memset(&m_ovfile, 0, sizeof(m_ovfile));
}


// Open mode accessor.
int qtractorAudioVorbisFile::mode() const
{
	return m_iMode;
}


// Open channel(s) accessor.
unsigned short qtractorAudioVorbisFile::channels() const
{
	return (m_ovinfo ? m_ovinfo->channels : 0);
}


// Estimated number of frames specialty (aprox. 8secs).
unsigned long qtractorAudioVorbisFile::frames() const
{
	return ::ov_pcm_total((OggVorbis_File *) &m_ovfile, -1);
}


// Sample rate specialty.
unsigned int qtractorAudioVorbisFile::samplerate() const
{
	return (m_ovinfo ? m_ovinfo->rate : 0);
}


// end of qtractorAudioVorbisFile.cpp
