// qtractorAudioVorbisFile.h
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

#ifndef __qtractorAudioVorbisFile_h
#define __qtractorAudioVorbisFile_h

#include "qtractorAudioFile.h"

// libvorbisfile API.
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>


//----------------------------------------------------------------------
// class qtractorAudioVorbisFile -- Buffered audio file declaration.
//

class qtractorAudioVorbisFile : public qtractorAudioFile
{
public:

	// Constructor.
	qtractorAudioVorbisFile(unsigned int iBufferSize = 0);

	// Destructor.
	virtual ~qtractorAudioVorbisFile();

	// Virtual method mockups.
	bool open  (const char *pszName, int iMode = Read);
	int  read  (float **ppFrames, unsigned int iFrames);
	int  write (float **ppFrames, unsigned int iFrames);
	bool seek  (unsigned long iOffset);
	void close ();

	// Virtual accessor mockups.
	int mode() const;
	unsigned short channels() const;
	unsigned long  frames() const;

	// Specialty methods.
	unsigned int   samplerate() const;

private:

	int            m_iMode;         // open mode (Read only).
	FILE          *m_pFile;         // fopen file descriptor.
	OggVorbis_File m_ovfile;        // libsvorbisfile descriptor.
	vorbis_info   *m_ovinfo;        // libvorbisfile info struct.
	int            m_ovsect;        // libvorbisfile current section.

	unsigned int   m_iBufferSize;	// estimated buffer size.
};


#endif  // __qtractorAudioVorbisFile_h


// end of qtractorAudioVorbisFile.h
