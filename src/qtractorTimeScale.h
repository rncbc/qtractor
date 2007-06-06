// qtractorTimeScale.h
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorTimeScale_h
#define __qtractorTimeScale_h

// Need this for lroundf.
#include <math.h>


//----------------------------------------------------------------------
// class qtractorTimeScale -- Time scale conversion helper class.
//

class qtractorTimeScale
{
public:

	// Default constructor.
	qtractorTimeScale()
	{
		m_iSampleRate     = 44100;
		m_fTempo          = 120.0f;
		m_iTicksPerBeat   = 96;
		m_iBeatsPerBar    = 4;
		m_iPixelsPerBeat  = 32;
		m_iSnapPerBeat    = 4;
		m_iHorizontalZoom = 100;
		m_iVerticalZoom   = 100;

		updateScale();
	}

	// Copy constructor.
	qtractorTimeScale(const qtractorTimeScale& ts)
	{
		m_iSampleRate     = ts.m_iSampleRate;
		m_fTempo          = ts.m_fTempo;
		m_iTicksPerBeat   = ts.m_iTicksPerBeat;
		m_iBeatsPerBar    = ts.m_iBeatsPerBar;
		m_iPixelsPerBeat  = ts.m_iPixelsPerBeat;
		m_iSnapPerBeat    = ts.m_iSnapPerBeat;
		m_iHorizontalZoom = ts.m_iHorizontalZoom;
		m_iVerticalZoom   = ts.m_iVerticalZoom;

		updateScale();
	}

	// Sample rate (frames per second)
	void setSampleRate(unsigned int iSampleRate)
		{ m_iSampleRate = iSampleRate; }
	unsigned int sampleRate() const { return m_iSampleRate; }

	// Tempo (beats per minute; BPM)
	void setTempo(float fTempo)
		{ m_fTempo = fTempo; }
	float tempo() const { return m_fTempo; }

	// Resolution (pulses per beat; PPQN)
	void setTicksPerBeat(unsigned short iTicksPerBeat)
		{ m_iTicksPerBeat = iTicksPerBeat; }
	unsigned short ticksPerBeat() const { return m_iTicksPerBeat; }

	// Measure (beats per bar)
	void setBeatsPerBar(unsigned short iBeatsPerBar)
		{ m_iBeatsPerBar = iBeatsPerBar; }
	unsigned short beatsPerBar() const { return m_iBeatsPerBar; }
		
	// Pixels per beat (width).	
	void setPixelsPerBeat(unsigned short iPixelsPerBeat)
		{ m_iPixelsPerBeat = iPixelsPerBeat; }
	unsigned short pixelsPerBeat() const { return m_iPixelsPerBeat; }

	// Beat divisor (snap) accessors.
	void setSnapPerBeat(unsigned short iSnapPerBeat)
		{ m_iSnapPerBeat = iSnapPerBeat; }
	unsigned short snapPerBeat(void) const { return m_iSnapPerBeat; }

	// Horizontal zoom factor.
	void setHorizontalZoom(unsigned short iHorizontalZoom)
		{ m_iHorizontalZoom = iHorizontalZoom; }
	unsigned short horizontalZoom() const { return m_iHorizontalZoom; }

	// Vertical zoom factor.
	void setVerticalZoom(unsigned short iVerticalZoom)
		{ m_iVerticalZoom = iVerticalZoom; }
	unsigned short verticalZoom() const { return m_iVerticalZoom; }

	// Tell whether a beat is a bar.
	bool beatIsBar(unsigned int iBeat) const
		{ return ((iBeat % m_iBeatsPerBar) == 0); }

	// Pixel/Beat number conversion.
	unsigned int beatFromPixel(unsigned int x) const
		{ return ((100 * x) / m_iScale_a); }
	unsigned int pixelFromBeat(unsigned int iBeat) const
		{ return ((iBeat * m_iScale_a) / 100); }

	// Pixel/Tick number conversion.
	unsigned int tickFromPixel(unsigned int x) const
		{ return (unsigned int) ::lroundf((m_fScale_d * x) / m_fScale_b); }
	unsigned int pixelFromTick(unsigned int iTick) const
		{ return (unsigned int) ::lroundf((m_fScale_b * iTick) / m_fScale_d); }

	// Pixel/Frame number conversion.
	unsigned long frameFromPixel(unsigned int x) const
		{ return (unsigned long) ::lroundf((m_fScale_c * x) / m_fScale_b); }
	unsigned int pixelFromFrame(unsigned long iFrame) const
		{ return (unsigned int) ::lroundf((m_fScale_b * iFrame) / m_fScale_c); }

	// Beat/frame conversion.
	unsigned long frameFromBeat(unsigned int iBeat) const
		{ return (unsigned long) ::lroundf((m_fScale_c * iBeat) / m_fTempo); }
	unsigned int beatFromFrame(unsigned long iFrame) const
		{ return (unsigned int) ::lroundf((m_fTempo * iFrame) / m_fScale_c); }

	// Tick/Frame number conversion.
	unsigned long frameFromTick(unsigned int iTick) const
		{ return (unsigned long) ::lroundf((m_fScale_c * iTick) / m_fScale_d); }
	unsigned int tickFromFrame(unsigned long iFrame) const
		{ return (unsigned int) ::lroundf((m_fScale_d * iFrame) / m_fScale_c); }

	// Beat/frame snap filters.
	unsigned long tickSnap(unsigned long iTick) const
	{
		unsigned long iTickSnap = iTick;
		if (m_iSnapPerBeat > 0) {
			unsigned long q = m_iTicksPerBeat / m_iSnapPerBeat;
			iTickSnap = q * ((iTickSnap + (q >> 1)) / q);
		}
		return iTickSnap;
	}

	unsigned long frameSnap(unsigned long iFrame) const
		{ return frameFromTick(tickSnap(tickFromFrame(iFrame))); }
	unsigned int pixelSnap(unsigned int x) const
		{ return pixelFromTick(tickSnap(tickFromPixel(x))); }

	// Beat divisor (snap index) accessors.
	unsigned short snapFromIndex(int iSnap) const
	{
		unsigned short iSnapPerBeat = 0;
		if (iSnap > 0)
			iSnapPerBeat++;
		for (int i = 1; i < iSnap; i++)
			iSnapPerBeat <<= 1;
		return iSnapPerBeat;
	}

	int indexFromSnap(unsigned short iSnapPerBeat) const
	{
		int iSnap = 0;
		for (unsigned short n = 1; n <= iSnapPerBeat; n <<= 1)
			++iSnap;
		return iSnap;
	}

	// Update scale divisor factors.
	void updateScale()
	{
		m_iScale_a = (unsigned int) (m_iHorizontalZoom * m_iPixelsPerBeat);
		m_fScale_b = (float) (0.01f * m_fTempo * m_iScale_a);
		m_fScale_c = (float) (60.0f * m_iSampleRate);
		m_fScale_d = (float) (m_fTempo * m_iTicksPerBeat);
	}
	
private:

	unsigned int   m_iSampleRate;       // Sample rate (frames per second)
	float          m_fTempo;            // Tempo (beats per minute; BPM)
	unsigned short m_iTicksPerBeat;     // Resolution (pulses per beat; PPQN)
	unsigned short m_iBeatsPerBar;      // Measure (beats per bar)
	unsigned short m_iPixelsPerBeat;    // Pixels per beat (width).
	unsigned short m_iSnapPerBeat;      // Snap per beat (divisor).
	unsigned short m_iHorizontalZoom;   // Horizontal zoom factor.
	unsigned short m_iVerticalZoom;     // Vertical zoom factor.

	unsigned long  m_iScale_a;
	float          m_fScale_b;
	float          m_fScale_c;
	float          m_fScale_d;
};


#endif  // __qtractorTimeScale_h


// end of qtractorTimeScale.h
