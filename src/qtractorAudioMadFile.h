// qtractorAudioMadFile.h
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

#ifndef __qtractorAudioMadFile_h
#define __qtractorAudioMadFile_h

#include "qtractorAudioFile.h"

#include <QList>

#include <stdio.h>

#ifdef CONFIG_LIBMAD
// libmad API
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

	// Special decode method.
	bool input();
	bool decode();

	// Internal ring-buffer helper methods.
	unsigned int readable() const;
	unsigned int writable() const;

private:

	// Instance variables.
	int               m_iMode;
	FILE             *m_pFile;
	unsigned int      m_iBitRate;
	unsigned short    m_iChannels;
	unsigned int      m_iSampleRate;
	unsigned long     m_iFramesEst;
	bool              m_bEndOfStream;

#ifdef CONFIG_LIBMAD
	struct mad_stream m_madStream;
	struct mad_frame  m_madFrame;
	struct mad_synth  m_madSynth;
#endif

	// Input buffer stuff.
	unsigned int      m_iInputBufferSize;
	unsigned char    *m_pInputBuffer;

	// Output ring-buffer stuff.
	unsigned int      m_iRingBufferSize;
	unsigned int      m_iRingBufferMask;
	unsigned int      m_iRingBufferRead;
	unsigned int      m_iRingBufferWrite;
	float           **m_ppRingBuffer;

	// Decoding frame mapping for sample-accurate seeking.
	unsigned long     m_iSeekOffset;

	// Decoded frame node.
	struct FrameNode {
		// Member constructor.
		FrameNode(unsigned long i = 0, unsigned long o = 0, unsigned int c = 0)
			: iInputOffset(i), iOutputOffset(o), iDecodeCount(c) {}
		// Member fields.
		unsigned long iInputOffset;     // Bytes from input file.
		unsigned long iOutputOffset;    // Sample frames on output.
		unsigned int  iDecodeCount;     // Decoder iteration count.
	};

	// Decoded frame list type.
	typedef QList<FrameNode> FrameList;

	// Frame list factory method.
	static FrameList *createFrameList(const QString& sFilename);

	// Frame list instance; 
	FrameList *m_pFrameList;
	// Current decoded frame node.
	FrameNode m_curr;
};


#endif  // __qtractorAudioMadFile_h


// end of qtractorAudioMadFile.h
