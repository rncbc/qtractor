// qtractorAudioSndFile.h
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

#ifndef __qtractorAudioSndFile_h
#define __qtractorAudioSndFile_h

#include "qtractorAudioFile.h"

// libsndfile API.
#include <sndfile.h>


//----------------------------------------------------------------------
// class qtractorAudioSndFile -- Buffered audio file declaration.
//

class qtractorAudioSndFile : public qtractorAudioFile
{
public:

	// Constructor.
	qtractorAudioSndFile(unsigned int iBufferSize = 0,
		unsigned short iChannels = 0, unsigned int iSampleRate = 0);

	// Destructor.
	virtual ~qtractorAudioSndFile();

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

protected:

	// De/interleaving buffer (re)allocation check.
	void allocBufferCheck(unsigned int iBufferSize);

private:

	int           m_iMode;          // open mode (Read|Write).
	SNDFILE      *m_pSndFile;       // libsndfile descriptor.
	SF_INFO       m_sfinfo;         // libsndfile info struct.

	// De/interleaving buffer stuff.
	float        *m_pBuffer;
	unsigned int  m_iBufferSize;
};


#endif  // __qtractorAudioSndFile_h


// end of qtractorAudioSndFile.h
