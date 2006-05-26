// qtractorAudioMonitor.cpp
//
/****************************************************************************
   Copyright (C) 2006, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAudioMonitor.h"

#include <math.h>

#if defined(__BORLANDC__)
// BCC32 doesn't have these float versions...
static inline float cosf  ( float x )  { return float(::cos(x));  }
static inline float sinf  ( float x )  { return float(::sin(x));  }
static inline float sqrtf ( float x )  { return float(::sqrt(x)); }
#endif


//----------------------------------------------------------------------------
// qtractorAudioMonitor -- Audio monitor bridge value processor.

// Constructor.
qtractorAudioMonitor::qtractorAudioMonitor ( unsigned short iChannels,
	float fGain, float fPanning ) : qtractorMonitor(fGain, fPanning),
	m_iChannels(0), m_pfValues(0), m_pfGains(0)
{
	setChannels(iChannels);
}


// Destructor.
qtractorAudioMonitor::~qtractorAudioMonitor (void)
{
	setChannels(0);
}


// Channel property accessors.
void qtractorAudioMonitor::setChannels ( unsigned short iChannels )
{
	// Delete old value holders...
	if (m_pfValues) {
		delete [] m_pfValues;
		m_pfValues = 0;
	}
	// Delete old panning-gains holders...
	if (m_pfGains) {
		delete [] m_pfGains;
		m_pfGains = 0;
	}
	// Set new value holders...
	m_iChannels = iChannels;
	if (m_iChannels > 0) {
		m_pfValues = new float [m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; i++)
			m_pfValues[i] = 0.0f;
		m_pfGains = new float [m_iChannels];
		update();
	}
}

unsigned short qtractorAudioMonitor::channels (void) const
{
	return m_iChannels;
}


// Value holder accessors.
void qtractorAudioMonitor::setValue ( unsigned short iChannel, float fValue )
{
	if (m_pfValues[iChannel] < fValue)
		m_pfValues[iChannel] = fValue;
}

float qtractorAudioMonitor::value ( unsigned short iChannel ) const
{
	float fValue = m_pfValues[iChannel];
	m_pfValues[iChannel] = 0.0f;
	return fValue;
}


// Batch processor.
void qtractorAudioMonitor::process ( float **ppFrames, unsigned int iFrames )
{
	for (unsigned short i = 0; i < m_iChannels; i++) {
		for (unsigned int n = 0; n < iFrames; n++)
			setValue(i, ppFrames[i][n] *= m_pfGains[i]);
	}
}


// Rebuild the whole panning-gain array...
void qtractorAudioMonitor::update (void)
{
	// (Re)compute equal-power stereo-panning gains...
	const float fPan = 0.5f * (1.0f + panning());
	float afGains[2] = { gain(), gain() };
	if (panning() < -0.001f || panning() > +0.001f) {
#ifdef QTRACTOR_MONITOR_PANNING_SQRT
		afGains[0] *= M_SQRT2 * ::sqrtf(1.0f - fPan);
		afGains[1] *= M_SQRT2 * ::sqrtf(fPan);
#else
		afGains[0] *= M_SQRT2 * ::cosf(fPan * M_PI_2);
		afGains[1] *= M_SQRT2 * ::sinf(fPan * M_PI_2);
#endif
	}
	// Apply to multi-channel gain array (paired fashion)...
	unsigned short i;
	unsigned short iChannels = (m_iChannels - (m_iChannels % 2));
	for (i = 0; i < iChannels; i++)
		m_pfGains[i] = afGains[i % 2];
	while (i < m_iChannels)
		m_pfGains[i++] = gain();
}


// end of qtractorAudioMonitor.cpp
