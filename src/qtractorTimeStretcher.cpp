// qtractorTimeStretcher.cpp
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

#include "qtractorTimeStretcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Constructor.
qtractorTimeStretcher::qtractorTimeStretcher (
	unsigned short iChannels, unsigned int iSampleRate,
	float fTimeStretch, float fPitchShift, unsigned int iFlags )
	: m_pTimeStretch(NULL)
#ifdef CONFIG_LIBRUBBERBAND
	, m_pRubberBandStretcher(NULL)
	, m_iRubberBandChannels(iChannels)
	, m_ppRubberBandFrames(NULL)
	, m_bRubberBandFlush(false)
#endif
{
	if (fTimeStretch > 0.0f && (iFlags & WsolaTimeStretch)) {
		m_pTimeStretch = new qtractorTimeStretch(iChannels, iSampleRate);
		m_pTimeStretch->setTempo(1.0f / fTimeStretch);
		m_pTimeStretch->setQuickSeek(iFlags & WsolaQuickSeek);
		fTimeStretch = 0.0f;
	}
#ifdef CONFIG_LIBRUBBERBAND
	if (fTimeStretch > 0.0f ||
		(fPitchShift > 0.1f && fPitchShift < 1.0f - 1e-3f) ||
		(fPitchShift > 1.0f + 1e-3f && fPitchShift < 4.0f)) {
		if (fTimeStretch < 0.1f)
			fTimeStretch = 1.0f;
		m_pRubberBandStretcher
			= new RubberBand::RubberBandStretcher(
				iSampleRate, iChannels,
				RubberBand::RubberBandStretcher::OptionProcessRealTime,
				fTimeStretch, fPitchShift);
		m_ppRubberBandFrames = new float * [m_iRubberBandChannels];
	}
#endif
}

// Destructor.
qtractorTimeStretcher::~qtractorTimeStretcher()
{
	if (m_pTimeStretch)
		delete m_pTimeStretch;
#ifdef CONFIG_LIBRUBBERBAND
	if (m_ppRubberBandFrames)
		delete [] m_ppRubberBandFrames;
	if (m_pRubberBandStretcher)
		delete m_pRubberBandStretcher;
#endif
}


// Adds frames of samples into the input buffer.
void qtractorTimeStretcher::process (
	float **ppFrames, unsigned int iFrames )
{
	if (m_pTimeStretch) {
		m_pTimeStretch->putFrames(ppFrames, iFrames);
#ifdef CONFIG_LIBRUBBERBAND
		if (m_pRubberBandStretcher) {
			unsigned int noffs = 0;
			unsigned int nread = iFrames;
			while (nread > 0 && noffs < iFrames) {
				for (unsigned short i = 0; i < m_iRubberBandChannels; ++i)
					m_ppRubberBandFrames[i] = ppFrames[i] + noffs;
				nread = m_pTimeStretch->receiveFrames(
					m_ppRubberBandFrames, iFrames - noffs);
				noffs += nread;
			}
			iFrames = noffs;
		}
	}
	if (m_pRubberBandStretcher) {
		m_pRubberBandStretcher->process(ppFrames, iFrames, false);
#endif
	}
}


// Copies requested frames output buffer and removes them
// from the sample buffer. If there are less than available()
// samples in the buffer, returns all that available. duh?
unsigned int qtractorTimeStretcher::retrieve (
	float **ppFrames, unsigned int iFrames )
{
#ifdef CONFIG_LIBRUBBERBAND
	if (m_pRubberBandStretcher)
		return m_pRubberBandStretcher->retrieve(ppFrames, iFrames);
#endif
	return (m_pTimeStretch ? m_pTimeStretch->receiveFrames(ppFrames, iFrames) : 0);
}


// Returns number of frames currently available.
unsigned int qtractorTimeStretcher::available (void) const
{
	int iAvailable = 0;

#ifdef CONFIG_LIBRUBBERBAND
	if (m_pRubberBandStretcher)
		iAvailable = m_pRubberBandStretcher->available();
	else
#endif
	if (m_pTimeStretch)
		iAvailable = m_pTimeStretch->frames();

	return (iAvailable > 0 ? iAvailable : 0);
}


// Flush any last samples that are hiding
// in the internal processing pipeline.
void qtractorTimeStretcher::flush (void)
{
	if (m_pTimeStretch)
		m_pTimeStretch->flushInput();
#ifdef CONFIG_LIBRUBBERBAND
	if (m_pRubberBandStretcher && !m_bRubberBandFlush) {
		// Prepare a dummy empty buffer...
		float dummy[256];
		::memset(&dummy[0], 0, sizeof(dummy));
		for (unsigned short i = 0; i < m_iRubberBandChannels; ++i)
			m_ppRubberBandFrames[i] = &dummy[0];
		m_pRubberBandStretcher->process(m_ppRubberBandFrames, 256, true);
		m_bRubberBandFlush = true;
	}
#endif
}


// Clears all buffers.
void qtractorTimeStretcher::reset (void)
{
	if (m_pTimeStretch)
		m_pTimeStretch->clear();
#ifdef CONFIG_LIBRUBBERBAND
	if (m_pRubberBandStretcher) {
		m_pRubberBandStretcher->reset();
		m_bRubberBandFlush = false;
	}
#endif
}


// end of qtractorTimeStretcher.cpp
