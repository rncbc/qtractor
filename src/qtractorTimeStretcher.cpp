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
	float fTimeStretch, float fPitchShift,
	unsigned int iFlags, unsigned int iBufferSize )
	: m_pTimeStretch(NULL)
#ifdef CONFIG_LIBRUBBERBAND
	, m_pRubberBandStretcher(NULL)
	, m_iRubberBandChannels(iChannels)
	, m_iRubberBandLatency(0)
	, m_iRubberBandFrames(0)
	, m_ppRubberBandFrames(NULL)
	, m_ppRubberBandBuffer(NULL)
	, m_bRubberBandFlush(false)
#endif
{
	if ((fTimeStretch > 0.1f && fTimeStretch < 1.0f - 1e-3f) ||
		(fTimeStretch > 1.0f + 1e-3f && fTimeStretch < 4.0f)) {
		if (iFlags & WsolaTimeStretch) {
			m_pTimeStretch = new qtractorTimeStretch(iChannels, iSampleRate);
			m_pTimeStretch->setTempo(1.0f / fTimeStretch);
			m_pTimeStretch->setQuickSeek(iFlags & WsolaQuickSeek);
			fTimeStretch = 0.0f;
		}
	}
	else fTimeStretch = 0.0f;
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
		m_pRubberBandStretcher->setMaxProcessSize(iBufferSize);
		m_ppRubberBandBuffer = new float * [m_iRubberBandChannels];
		m_iRubberBandLatency = m_pRubberBandStretcher->getLatency();
		m_iRubberBandFrames = m_iRubberBandLatency;
		if (m_iRubberBandFrames > 0) {
			m_ppRubberBandFrames = new float * [m_iRubberBandChannels];
			m_ppRubberBandFrames[0] = new float [m_iRubberBandFrames];
			::memset(m_ppRubberBandFrames[0], 0, m_iRubberBandFrames * sizeof(float));
			for (unsigned short i = 1; i < m_iRubberBandChannels; ++i)
				m_ppRubberBandFrames[i] = m_ppRubberBandFrames[0];
		}
	}
#endif
}

// Destructor.
qtractorTimeStretcher::~qtractorTimeStretcher()
{
	if (m_pTimeStretch)
		delete m_pTimeStretch;
#ifdef CONFIG_LIBRUBBERBAND
	if (m_ppRubberBandBuffer)
		delete [] m_ppRubberBandBuffer;
	if (m_ppRubberBandFrames) {
		delete [] m_ppRubberBandFrames[0];
		delete [] m_ppRubberBandFrames;
	}
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
					m_ppRubberBandBuffer[i] = ppFrames[i] + noffs;
				nread = m_pTimeStretch->receiveFrames(
					m_ppRubberBandBuffer, iFrames - noffs);
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
	if (m_pRubberBandStretcher) {
		unsigned int nread = m_pRubberBandStretcher->retrieve(ppFrames, iFrames);
		if (nread > 0 && m_iRubberBandLatency > 0) {
			if (m_iRubberBandLatency > nread) {
				m_iRubberBandLatency -= nread;
				nread = 0;
			} else {
				unsigned int noffs = m_iRubberBandLatency;
				nread -= m_iRubberBandLatency;
				m_iRubberBandLatency = 0;
				for (unsigned int i = 0; i < m_iRubberBandChannels; ++i) {
					float *pFrames = ppFrames[i];
					::memmove(pFrames, pFrames + noffs, nread * sizeof(float));
				}
			}
		}
		return nread;
	}
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
		// Process a last dummy empty buffer...
		if (m_iRubberBandFrames > 0) {
			m_pRubberBandStretcher->process(
				m_ppRubberBandFrames, m_iRubberBandFrames, true);
		}
		m_iRubberBandLatency = 0;
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
		m_iRubberBandLatency = m_pRubberBandStretcher->getLatency();
		m_iRubberBandFrames = m_iRubberBandLatency;
		if (m_iRubberBandFrames > 0) {
			if (m_ppRubberBandFrames) {
				delete [] m_ppRubberBandFrames[0];
			//	delete [] m_ppRubberBandFrames;
			}
		//	m_ppRubberBandFrames = new float * [m_iRubberBandChannels];
			m_ppRubberBandFrames[0] = new float [m_iRubberBandFrames];
			::memset(m_ppRubberBandFrames[0], 0, m_iRubberBandFrames * sizeof(float));
			for (unsigned short i = 1; i < m_iRubberBandChannels; ++i)
				m_ppRubberBandFrames[i] = m_ppRubberBandFrames[0];
		}
		m_bRubberBandFlush = false;
	}
#endif
}


// end of qtractorTimeStretcher.cpp
