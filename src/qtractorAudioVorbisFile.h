// qtractorAudioVorbisFile.h
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

#ifndef __qtractorAudioVorbisFile_h
#define __qtractorAudioVorbisFile_h

#include "qtractorAudioFile.h"

#ifdef CONFIG_LIBVORBIS
// libvorbis API.
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#endif


//----------------------------------------------------------------------
// class qtractorAudioVorbisFile -- Buffered audio file declaration.
//

class qtractorAudioVorbisFile : public qtractorAudioFile
{
public:

	// Constructor.
	qtractorAudioVorbisFile(unsigned short iChannels = 0,
		unsigned int iSampleRate = 0, unsigned int iBufferSize = 0);

	// Destructor.
	virtual ~qtractorAudioVorbisFile();

	// Virtual method mockups.
	bool open  (const QString& sFilename, int iMode = Read);
	int  read  (float **ppFrames, unsigned int iFrames);
	int  write (float **ppFrames, unsigned int iFrames);
	bool seek  (unsigned long iOffset);
	void close ();

	// Virtual accessor mockups.
	int mode() const;
	unsigned short channels() const;
	unsigned long  frames() const;

	// Specialty methods.
	unsigned int   sampleRate() const;

protected:

	// Flush encoder buffers.
	void flush(bool fEos = false);

private:

	int              m_iMode;       // open mode (Read only).
	FILE            *m_pFile;       // fopen file descriptor.

	unsigned short   m_iChannels;   // estimated channel count.
	unsigned int     m_iSampleRate; // estimated sample rate;
	unsigned long    m_iFrames;     // estimated encoded frames;

#ifdef CONFIG_LIBVORBIS

	// Common codec variable.
	vorbis_info     *m_ovinfo;      // libvorbisfile info struct.

	// Encoder specific variables.
	vorbis_comment   m_ovcomment;   // struct that stores all the user comments.
	vorbis_dsp_state m_ovdsp;       // central working state for the encoder.
	vorbis_block     m_ovblock;     // local working space for the encoder.
	ogg_stream_state m_ovstate;     // physical pages, logical stream of packets.

	// Decoder specific variables.
	OggVorbis_File   m_ovfile;      // libsvorbisfile descriptor.
	int              m_ovsect;      // libvorbisfile current section.

#endif	// CONFIG_LIBVORBIS

	unsigned int     m_iBufferSize; // estimated buffer size.
};


#endif  // __qtractorAudioVorbisFile_h


// end of qtractorAudioVorbisFile.h
