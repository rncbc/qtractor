// qtractorMonitor.h
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

#ifndef __qtractorMonitor_h
#define __qtractorMonitor_h

#include <math.h>

#if defined(__BORLANDC__)
// BCC32 doesn't have these float versions...
static inline float cosf   ( float x )  { return float(::cos(x)); }
static inline float sinf   ( float x )  { return float(::sin(x)); }
static inline float sqrtf  ( float x )  { return float(::sqrt(x)); }
static inline float log10f ( float x )  { return float(::log(x)) / M_LN10; }
static inline float powf ( float x, float y ) { return float(::pow(x, y)); }
#endif


//----------------------------------------------------------------------------
// qtractorMonitor -- Monitor bridge value processor.

class qtractorMonitor
{
public:

	// Constructor.
	qtractorMonitor(unsigned short iChannels) : m_iChannels(0),
		m_fGain(1.0f), m_fPanning(0.0f), m_pfValues(0), m_pfGains(0)
	{ setChannels(iChannels); }

	// Destructor.
	~qtractorMonitor()
		{ setChannels(0); }

	// Gain accessors.
	float gain() const
		{ return m_fGain; }
	void setGain(float fGain)
		{ m_fGain = fGain; resetGains(); }

	// Stereo panning accessors.
	float panning() const
		{ return m_fPanning; }
	void setPanning(float fPanning)
		{ m_fPanning = fPanning; resetGains(); }

	// Channel property accessors.
	unsigned short channels() const
		{ return m_iChannels; }

	void setChannels(unsigned short iChannels)
	{
		// Delete old value holders...
		if (m_pfValues) {
			delete [] m_pfValues;
			m_pfValues = NULL;
		}
		// Delete old panning-gains holders...
		if (m_pfGains) {
			delete [] m_pfGains;
			m_pfGains = NULL;
		}	
		// Set new value holders...
		m_iChannels = iChannels;
		if (m_iChannels > 0) {
			m_pfValues = new float [m_iChannels];
			for (unsigned short i = 0; i < m_iChannels; i++)
				m_pfValues[i] = 0.0f;
			m_pfGains = new float [m_iChannels];
			resetGains();
		}
	}		

	// Value holder accessors.
	float value(unsigned short iChannel) const
	{
		float fValue = m_pfValues[iChannel];
		m_pfValues[iChannel] = 0.0f;
		return fValue;
	}

	void setValue(unsigned short iChannel, float fValue)
	{
		if (m_pfValues[iChannel] < fValue)
			m_pfValues[iChannel] = fValue;
	}

	// Batch processor.
	void process(float **ppFrames, unsigned int iFrames)
	{
		for (unsigned short i = 0; i < m_iChannels; i++) {
			for (unsigned int n = 0; n < iFrames; n++)
				setValue(i, ppFrames[i][n] *= m_pfGains[i]);
		}
	}

protected:

	// Rebuild the whole panning-gain array...
	void resetGains()
	{
		// (Re)compute equal-power stereo-panning gains...
		const float fPan = 0.5f * (1.0f + m_fPanning);
		float afGains[2] = { m_fGain, m_fGain };
		if (m_fPanning < -0.001f || m_fPanning > +0.001f) {
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
			m_pfGains[i++] = m_fGain;
	}

private:

	// Instance variables.
	unsigned short m_iChannels;
	float          m_fGain;
	float          m_fPanning;
	float         *m_pfValues;
	float         *m_pfGains;
};


#endif  // __qtractorMonitor_h

// end of qtractorMonitor.h
