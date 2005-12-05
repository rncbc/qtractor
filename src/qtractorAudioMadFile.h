// qtractorAudioMadFile.h
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

#ifndef __qtractorAudioMadFile_h
#define __qtractorAudioMadFile_h

#include "qtractorAudioFile.h"

#ifdef CONFIG_LIBMAD
#include <mad.h>
#endif


//----------------------------------------------------------------------
// class qtractorAudioMadFile -- Buffered audio file declaration.
//

class qtractorAudioMadFile : public qtractorAudioFile
{
public:

	// Constructor.
	qtractorAudioMadFile(unsigned int iBufferSize = 0);

	// Destructor.
	virtual ~qtractorAudioMadFile();

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

	// Spacial decode method.
	bool decode();

	// Internal ring-buffer helper methods.
	void createBuffer(unsigned int iBufferSize);
	void deleteBuffer();
	unsigned int readable() const;
	unsigned int writable() const;

private:

	// Instance variables.
	int               m_iMode;

	unsigned short    m_iChannels;
	unsigned int      m_iSampleRate;
	unsigned int      m_iBitRate;
	unsigned long     m_iFramesEst;
	FILE             *m_pFile;
	unsigned long     m_iFileSize;
	bool              m_bEndOfStream;
#ifdef CONFIG_LIBMAD
	void             *m_pFileMmap;
	struct mad_stream m_madStream;
	struct mad_frame  m_madFrame;
	struct mad_synth  m_madSynth;
#endif
	// Ring-buffer stuff.
	unsigned int      m_iBufferSize;
	unsigned int      m_iBufferMask;
	unsigned int      m_iReadIndex;
	unsigned int      m_iWriteIndex;
	float           **m_ppBuffer;
};


#endif  // __qtractorAudioMadFile_h


// end of qtractorAudioMadFile.h
