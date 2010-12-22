// qtractorTimeStretcher.h
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorTimeStretcher_h
#define __qtractorTimeStretcher_h

#include "qtractorAbout.h"
#include "qtractorTimeStretch.h"

#ifdef CONFIG_LIBRUBBERBAND
#include <rubberband/RubberBandStretcher.h>
#endif


//---------------------------------------------------------------------------
// qtractorTimeStretcher - Time/Pitch-stretcher processor interface.
//

class qtractorTimeStretcher
{
public:

	// Constructor flags.
	enum Flags { None = 0, WsolaTimeStretch = 1, WsolaQuickSeek = 2 };

	// Constructor.
	qtractorTimeStretcher(
		unsigned short iChannels = 2, unsigned int iSampleRate = 44100,
		float fTimeStretch = 1.0f, float fPitchShift = 1.0f,
		unsigned int iFlags = None, unsigned int iBufferSize = 4096);

	// Destructor.
	~qtractorTimeStretcher();

	// Adds frames of samples into the input buffer.
	void process(float **ppFrames, unsigned int iFrames);

	// Copies requested frames output buffer and removes them
	// from the sample buffer. If there are less than available()
	// samples in the buffer, returns all that available. duh?
	unsigned int retrieve(float **ppFrames, unsigned int iFrames);

	// Returns number of frames currently available.
	unsigned int available() const;

	// Flush any last samples that are hiding
	// in the internal processing pipeline.
	void flush();

	// Clears all buffers.
	void reset();

private:

	// Instance variables.
	qtractorTimeStretch *m_pTimeStretch;

#ifdef CONFIG_LIBRUBBERBAND
	RubberBand::RubberBandStretcher *m_pRubberBandStretcher;
	unsigned short m_iRubberBandChannels;
	unsigned int m_iRubberBandLatency;
	unsigned int m_iRubberBandFrames;
	float **m_ppRubberBandFrames;
	float **m_ppRubberBandBuffer;
	bool m_bRubberBandFlush;
#endif
};


#endif  // __qtractorTimeStretcher_h


// end of qtractorTimeStretcher.h
