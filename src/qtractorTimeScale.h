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

#include <QString>


//----------------------------------------------------------------------
// class qtractorTimeScale -- Time scale conversion helper class.
//

class qtractorTimeScale
{
public:

	// Default constructor.
	qtractorTimeScale()
		{ clear(); }

	// Copy constructor.
	qtractorTimeScale(const qtractorTimeScale& ts)
		{ copy(ts); }

	// Assignment operator,
	qtractorTimeScale& operator=(const qtractorTimeScale& ts)
		{ return copy(ts); }

	// (Re)nitializer method.
	void clear();

	// Copy method.
	qtractorTimeScale& copy(const qtractorTimeScale& ts);

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
		{ return uroundf((m_fScale_d * x) / m_fScale_b); }
	unsigned int pixelFromTick(unsigned int iTick) const
		{ return uroundf((m_fScale_b * iTick) / m_fScale_d); }

	// Pixel/Frame number conversion.
	unsigned long frameFromPixel(unsigned int x) const
		{ return uroundf((m_fScale_c * x) / m_fScale_b); }
	unsigned int pixelFromFrame(unsigned long iFrame) const
		{ return uroundf((m_fScale_b * iFrame) / m_fScale_c); }

	// Beat/frame conversion.
	unsigned long frameFromBeat(unsigned int iBeat) const
		{ return uroundf((m_fScale_c * iBeat) / m_fTempo); }
	unsigned int beatFromFrame(unsigned long iFrame) const
		{ return uroundf((m_fTempo * iFrame) / m_fScale_c); }

	// Tick/Frame number conversion.
	unsigned long frameFromTick(unsigned int iTick) const
		{ return uroundf((m_fScale_c * iTick) / m_fScale_d); }
	unsigned int tickFromFrame(unsigned long iFrame) const
		{ return uroundf((m_fScale_d * iFrame) / m_fScale_c); }

	// Beat/frame snap filters.
	unsigned long tickSnap(unsigned long iTick) const;

	unsigned long frameSnap(unsigned long iFrame) const
		{ return frameFromTick(tickSnap(tickFromFrame(iFrame))); }
	unsigned int pixelSnap(unsigned int x) const
		{ return pixelFromTick(tickSnap(tickFromPixel(x))); }

	// Available display-formats.
	enum DisplayFormat { Frames, Time, BBT };

	// Display-format accessors.
	void setDisplayFormat(DisplayFormat displayFormat)
		{ m_displayFormat = displayFormat; }
	DisplayFormat displayFormat() const
		{ return m_displayFormat; }

	// Convert frame to time string and vice-versa.
	QString textFromFrame(
		unsigned long iFrame, bool bDelta = false) const;
	unsigned long frameFromText(
		const QString& sText, bool bDelta = false) const;

	// Convert to time string and vice-versa.
	QString textFromTick(unsigned long iTick, bool bDelta = false) const
		{ return textFromFrame(frameFromTick(iTick), bDelta); }
	unsigned long tickFromText(const QString& sText, bool bDelta = false) const
		{ return tickFromFrame(frameFromText(sText, bDelta)); }

	// Update scale divisor factors.
	void updateScale();

	// Fastest rounding-from-float helper.
	static unsigned long uroundf(float x)
		{ return (unsigned long) (x >= 0.0f ? x + 0.5f : x - 0.5f); }

	// Beat divisor (snap index) accessors.
	static unsigned short snapFromIndex(int iSnap);
	static int indexFromSnap(unsigned short iSnapPerBeat);

	// Beat divisor (snap index) text item list.
	static QStringList snapItems(int iSnap = 0);

private:

	unsigned int   m_iSampleRate;       // Sample rate (frames per second)
	float          m_fTempo;            // Tempo (beats per minute; BPM)
	unsigned short m_iTicksPerBeat;     // Resolution (pulses per beat; PPQN)
	unsigned short m_iBeatsPerBar;      // Measure (beats per bar)
	unsigned short m_iPixelsPerBeat;    // Pixels per beat (width).
	unsigned short m_iSnapPerBeat;      // Snap per beat (divisor).
	unsigned short m_iHorizontalZoom;   // Horizontal zoom factor.
	unsigned short m_iVerticalZoom;     // Vertical zoom factor.

	DisplayFormat  m_displayFormat;     // Textual display format.

	// Internal time scaling factors.
	unsigned long  m_iScale_a;
	float          m_fScale_b;
	float          m_fScale_c;
	float          m_fScale_d;
};


#endif  // __qtractorTimeScale_h


// end of qtractorTimeScale.h
