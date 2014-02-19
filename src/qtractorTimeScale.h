// qtractorTimeScale.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorList.h"

#include <QStringList>
#include <QColor>


//----------------------------------------------------------------------
// class qtractorTimeScale -- Time scale conversion helper class.
//

class qtractorTimeScale
{
public:

	// Available display-formats.
	enum DisplayFormat { Frames = 0, Time, BBT };

	// Default constructor.
	qtractorTimeScale() : m_displayFormat(Frames),
		m_cursor(this), m_markerCursor(this) { clear(); }

	// Copy constructor.
	qtractorTimeScale(const qtractorTimeScale& ts)
		: m_cursor(this), m_markerCursor(this) { copy(ts); }

	// Assignment operator,
	qtractorTimeScale& operator=(const qtractorTimeScale& ts)
		{ return copy(ts); }

	// Node list cleaner.
	void reset();

	// (Re)nitializer method.
	void clear();

	// Sync method.
	void sync(const qtractorTimeScale& ts);

	// Copy method.
	qtractorTimeScale& copy(const qtractorTimeScale& ts);

	// Sample rate (frames per second)
	void setSampleRate(unsigned int iSampleRate)
		{ m_iSampleRate = iSampleRate; }
	unsigned int sampleRate() const { return m_iSampleRate; }

	// Resolution (ticks per quarter note; PPQN)
	void setTicksPerBeat(unsigned short iTicksPerBeat)
		{ m_iTicksPerBeat = iTicksPerBeat; }
	unsigned short ticksPerBeat() const { return m_iTicksPerBeat; }

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

	// Fastest rounding-from-float helper.
	static unsigned long uroundf(float x)
		{ return (unsigned long) (x < 0.0f ? x - 0.5f : x + 0.5f); }

	// Beat divisor (snap index) accessors.
	static unsigned short snapFromIndex(int iSnap);
	static int indexFromSnap(unsigned short iSnapPerBeat);

	// Beat divisor (snap index) text item list.
	static QStringList snapItems(int iSnap = 0);

	// Time scale node declaration.
	class Node : public qtractorList<Node>::Link
	{
	public:

		// Constructor.
		Node(qtractorTimeScale *pTimeScale,
			unsigned long iFrame = 0,
			float fTempo = 120.0f,
			unsigned short iBeatType = 2,
			unsigned short iBeatsPerBar = 4,
			unsigned short iBeatDivisor = 2)
			: frame(iFrame),
				bar(0), beat(0), tick(0), pixel(0),
				tempo(fTempo), beatType(iBeatType),
				beatsPerBar(iBeatsPerBar),
				beatDivisor(iBeatDivisor),
				ticksPerBeat(0), ts(pTimeScale),
				tickRate(1.0f), beatRate(1.0f) {}

		// Update node scale coefficients.
		void update();

		// Update node position metrics.
		void reset(Node *pNode);

		// Tempo accessor/convertors.
		void setTempoEx(float fTempo, unsigned short iBeatType = 2);
		float tempoEx(unsigned short iBeatType = 2) const;

		// Frame/bar convertors.
		unsigned short barFromFrame(unsigned long iFrame) const
			{ return bar + uroundf(
				(beatRate * (iFrame - frame)) / (ts->frameRate() * beatsPerBar)); }
		unsigned long frameFromBar(unsigned short iBar) const
			{ return frame + uroundf(
				(ts->frameRate() * beatsPerBar * (iBar - bar)) / beatRate); }

		// Frame/beat convertors.
		unsigned int beatFromFrame(unsigned long iFrame) const
			{ return beat + uroundf(
				(beatRate * (iFrame - frame)) / ts->frameRate()); }
		unsigned long frameFromBeat(unsigned int iBeat) const
			{ return frame + uroundf(
				(ts->frameRate() * (iBeat - beat)) / beatRate); }

		// Frame/tick convertors.
		unsigned long tickFromFrame(unsigned long iFrame) const
			{ return tick + uroundf(
				(tickRate * (iFrame - frame)) / ts->frameRate()); }
		unsigned long frameFromTick(unsigned long iTick) const
			{ return frame + uroundf(
				(ts->frameRate() * (iTick - tick)) / tickRate); }

		// Tick/beat convertors.
		unsigned int beatFromTick(unsigned long iTick) const
			{ return beat + ((iTick - tick) / ticksPerBeat); }
		unsigned long tickFromBeat(unsigned int iBeat) const
			{ return tick + (ticksPerBeat * (iBeat - beat)); }

		// Tick/bar convertors.
		unsigned short barFromTick(unsigned long iTick) const
			{ return bar + ((iTick - tick) / (ticksPerBeat * beatsPerBar)); }
		unsigned long tickFromBar(unsigned short iBar) const
			{ return tick + (ticksPerBeat * beatsPerBar * (iBar - bar)) ; }

		// Tick/pixel convertors.
		unsigned long tickFromPixel(int x) const
			{ return tick + uroundf(
				(tickRate * (x - pixel)) / ts->pixelRate()); }
		int pixelFromTick(unsigned long iTick) const
			{ return pixel + uroundf(
				(ts->pixelRate() * (iTick - tick)) / tickRate); }

		// Beat/pixel convertors.
		unsigned int beatFromPixel(int x) const
			{ return beat + uroundf(
				(beatRate * (x - pixel)) / ts->pixelRate()); }
		int pixelFromBeat(unsigned int iBeat) const
			{ return pixel + uroundf(
				(ts->pixelRate() * (iBeat - beat)) / beatRate); }

		// Pixel/beat rate convertor.
		unsigned short pixelsPerBeat() const
			{ return uroundf(ts->pixelRate() / beatRate); }

		// Bar/pixel convertors.
		unsigned short barFromPixel(int x) const
			{ return bar + uroundf(
				(beatRate * (x - pixel)) / (ts->pixelRate() * beatsPerBar)); }
		int pixelFromBar(unsigned short iBar) const
			{ return pixel + uroundf(
				(ts->pixelRate() * beatsPerBar * (iBar - bar)) / beatRate); }

		// Bar/beat convertors.
		unsigned short barFromBeat(unsigned int iBeat) const
			{ return bar + ((iBeat - beat) / beatsPerBar); }
		unsigned int beatFromBar(unsigned short iBar) const
			{ return beat + (beatsPerBar * (iBar - bar)); }

		bool beatIsBar(unsigned int iBeat) const
			{ return ((iBeat - beat) % beatsPerBar) == 0; }

		// Frame/bar quantizer.
		unsigned long frameSnapToBar(unsigned long iFrame) const
			{ return frameFromBar(barFromFrame(iFrame)); }

		// Beat snap filters.
		unsigned long tickSnap(unsigned long iTick, unsigned short p = 1) const;

		unsigned long frameSnap(unsigned long iFrame) const
			{ return frameFromTick(tickSnap(tickFromFrame(iFrame))); }
		int pixelSnap(int x) const
			{ return pixelFromTick(tickSnap(tickFromPixel(x))); }

		// Node keys.
		unsigned long  frame;
		unsigned short bar;
		unsigned int   beat;
		unsigned long  tick;
		int            pixel;

		// Node payload.
		float          tempo;
		unsigned short beatType;
		unsigned short beatsPerBar;
		unsigned short beatDivisor;

		unsigned short ticksPerBeat;

	protected:

		// Node owner.
		qtractorTimeScale *ts;

		// Node cached coefficients.
		float tickRate;
		float beatRate;
	};

	// Node list accessor.
	const qtractorList<Node>& nodes() const { return m_nodes; }

	// To optimize and keep track of current frame
	// position, mostly like an sequence cursor/iterator.
	class Cursor
	{
	public:

		// Constructor.
		Cursor(qtractorTimeScale *pTimeScale)
			: ts(pTimeScale), node(0) {}

		// Time scale accessor.
		qtractorTimeScale *timeScale() const { return ts; }

		// Reset method.
		void reset(Node *pNode = 0);

		// Seek methods.
		Node *seekFrame(unsigned long iFrame);
		Node *seekBar(unsigned short iBar);
		Node *seekBeat(unsigned int iBeat);
		Node *seekTick(unsigned long iTick);
		Node *seekPixel(int x);

	protected:

		// Member variables.
		qtractorTimeScale *ts;
		Node *node;
	};

	// Internal cursor accessor.
	Cursor& cursor() { return m_cursor; }

	// Node list specifics.
	Node *addNode(
		unsigned long iFrame = 0,
		float fTempo = 120.0f,
		unsigned short iBeatType = 2,
		unsigned short iBeatsPerBar = 4,
		unsigned short iBeatDivisor = 2);
	void updateNode(Node *pNode);
	void removeNode(Node *pNode);

	// Complete time-scale update method.
	void updateScale();

	// Frame/pixel convertors.
	int pixelFromFrame(unsigned long iFrame) const
		{ return uroundf((m_fPixelRate * iFrame) / m_fFrameRate); }
	unsigned long frameFromPixel(int x) const
		{ return uroundf((m_fFrameRate * x) / m_fPixelRate); }

	// Frame/bar general converters.
	unsigned short barFromFrame(unsigned long iFrame)
	{
		Node *pNode = m_cursor.seekFrame(iFrame);
		return (pNode ? pNode->barFromFrame(iFrame) : 0);
	}

	unsigned long frameFromBar(unsigned short iBar)
	{
		Node *pNode = m_cursor.seekBar(iBar);
		return (pNode ? pNode->frameFromBar(iBar) : 0);
	}

	// Frame/beat general converters.
	unsigned int beatFromFrame(unsigned long iFrame)
	{
		Node *pNode = m_cursor.seekFrame(iFrame);
		return (pNode ? pNode->beatFromFrame(iFrame) : 0);
	}

	unsigned long frameFromBeat(unsigned int iBeat)
	{
		Node *pNode = m_cursor.seekBeat(iBeat);
		return (pNode ? pNode->frameFromBeat(iBeat) : 0);
	}

	// Frame/tick general converters.
	unsigned long tickFromFrame(unsigned long iFrame)
	{
		Node *pNode = m_cursor.seekFrame(iFrame);
		return (pNode ? pNode->tickFromFrame(iFrame) : 0);
	}

	unsigned long frameFromTick(unsigned long iTick)
	{
		Node *pNode = m_cursor.seekTick(iTick);
		return (pNode ? pNode->frameFromTick(iTick) : 0);
	}

	// Tick/pixel general converters.
	unsigned long tickFromPixel(int x)
	{
		Node *pNode = m_cursor.seekPixel(x);
		return (pNode ? pNode->tickFromPixel(x) : 0);
	}

	int pixelFromTick(unsigned long iTick)
	{
		Node *pNode = m_cursor.seekTick(iTick);
		return (pNode ? pNode->pixelFromTick(iTick) : 0);
	}

	// Beat/pixel composite converters.
	unsigned int beatFromPixel(int x)
	{
		Node *pNode = m_cursor.seekPixel(x);
		return (pNode ? pNode->beatFromPixel(x) : 0);
	}

	int pixelFromBeat(unsigned int iBeat)
	{
		Node *pNode = m_cursor.seekBeat(iBeat);
		return (pNode ? pNode->pixelFromBeat(iBeat) : 0);
	}

	// Bar/beat predicate.
	bool beatIsBar(unsigned int iBeat)
	{
		Node *pNode = m_cursor.seekBeat(iBeat);
		return (pNode ? pNode->beatIsBar(iBeat) : false);
	}

	// Snap functions.
	unsigned long tickSnap(unsigned long iTick)
	{
		Node *pNode = m_cursor.seekTick(iTick);
		return (pNode ? pNode->tickSnap(iTick) : iTick);
	}

	unsigned long frameSnap(unsigned long iFrame)
	{
		Node *pNode = m_cursor.seekFrame(iFrame);
		return (pNode ? pNode->frameSnap(iFrame) : iFrame);
	}

	int pixelSnap(int x)
	{
		Node *pNode = m_cursor.seekPixel(x);
		return (pNode ? pNode->pixelSnap(x) : x);
	}

	// Display-format accessors.
	void setDisplayFormat(DisplayFormat displayFormat)
		{ m_displayFormat = displayFormat; }
	DisplayFormat displayFormat() const
		{ return m_displayFormat; }

	// Convert frames to time string and vice-versa.
	QString textFromFrameEx(DisplayFormat displayFormat,
		unsigned long iFrame, bool bDelta = false, unsigned long iDelta = 0);
	QString textFromFrame(
		unsigned long iFrame, bool bDelta = false, unsigned long iDelta = 0);

	unsigned long frameFromTextEx(DisplayFormat displayFormat,
		const QString& sText, bool bDelta = false, unsigned long iFrame = 0);
	unsigned long frameFromText(
		const QString& sText, bool bDelta = false, unsigned long iFrame = 0);

	// Convert ticks to time string and vice-versa.
	QString textFromTick(
		unsigned long iTick, bool bDelta = false, unsigned long iDelta = 0);
	unsigned long tickFromText(
		const QString& sText, bool bDelta = false, unsigned long iTick = 0);

	// Tempo (beats per minute; BPM)
	void setTempo(float fTempo)
	{
		Node *pNode = m_nodes.first();
		if (pNode) pNode->tempo = fTempo;
	}

	float tempo() const
	{
		Node *pNode = m_nodes.first();
		return (pNode ? pNode->tempo : 120.0f);
	}

	// Tempo convertors (default's quarter notes per minute)
	void setTempoEx(float fTempo, unsigned short iBeatType = 2)
	{
		Node *pNode = m_nodes.first();
		if (pNode) pNode->setTempoEx(fTempo, iBeatType);
	}

	float tempoEx(unsigned short iBeatType = 2) const
	{
		Node *pNode = m_nodes.first();
		return (pNode ? pNode->tempoEx(iBeatType) : 120.0f);
	}

	// Tempo beat type (if not standard 2=quarter note)
	void setBeatType(unsigned short iBeatType)
	{
		Node *pNode = m_nodes.first();
		if (pNode) pNode->beatType = iBeatType;
	}

	unsigned short beatType() const
	{
		Node *pNode = m_nodes.first();
		return (pNode ? pNode->beatType : 2);
	}

	// Time signature (numerator)
	void setBeatsPerBar(unsigned short iBeatsPerBar)
	{
		Node *pNode = m_nodes.first();
		if (pNode) pNode->beatsPerBar = iBeatsPerBar;
	}

	unsigned short beatsPerBar() const
	{
		Node *pNode = m_nodes.first();
		return (pNode ? pNode->beatsPerBar : 4);
	}

	// Time signature (denominator)
	void setBeatDivisor(unsigned short iBeatDivisor)
	{
		Node *pNode = m_nodes.first();
		if (pNode) pNode->beatDivisor = iBeatDivisor;
	}

	unsigned short beatDivisor() const
	{
		Node *pNode = m_nodes.first();
		return (pNode ? pNode->beatDivisor : 2);
	}

	// Tick/Frame range conversion (delta conversion).
	unsigned long frameFromTickRange(
	    unsigned long iTickStart, unsigned long iTickEnd);
	unsigned long tickFromFrameRange(
	    unsigned long iFrameStart, unsigned long iFrameEnd);

	// Location marker declaration.
	class Marker : public qtractorList<Marker>::Link
	{
	public:

		// Constructor.
		Marker(unsigned long iFrame, unsigned short iBar,
			const QString& sText, const QColor& rgbColor = Qt::darkGray)
			: frame(iFrame), bar(iBar), text(sText), color(rgbColor) {}

		// Copy constructor.
		Marker(const Marker& marker) : frame(marker.frame),
			bar(marker.bar), text(marker.text), color(marker.color) {}

		// Marker keys.
		unsigned long  frame;
		unsigned short bar;

		// Marker payload.
		QString text;
		QColor  color;
	};

	// To optimize and keep track of current frame
	// position, mostly like an sequence cursor/iterator.
	class MarkerCursor
	{
	public:

		// Constructor.
		MarkerCursor(qtractorTimeScale *pTimeScale)
			: ts(pTimeScale), marker(0) {}

		// Time scale accessor.
		qtractorTimeScale *timeScale() const { return ts; }

		// Reset method.
		void reset(Marker *pMarker = 0);

		// Seek methods.
		Marker *seekFrame(unsigned long iFrame);
		Marker *seekBar(unsigned short iBar);
		Marker *seekBeat(unsigned int iBeat);
		Marker *seekTick(unsigned long iTick);
		Marker *seekPixel(int x);

		// Notable markers accessors
		Marker *first() const { return ts->m_markers.first(); }
		Marker *last()  const { return ts->m_markers.last();  }

	protected:

		// Member variables.
		qtractorTimeScale *ts;
		Marker *marker;
	};

	// Markers list accessor.
	MarkerCursor& markers() { return m_markerCursor; }

	// Marker list specifics.
	Marker *addMarker(
		unsigned long iFrame,
		const QString& sText,
		const QColor& rgbColor = Qt::darkGray);
	void updateMarker(Marker *pMarker);
	void removeMarker(Marker *pMarker);

	// Update markers from given node position.
	void updateMarkers(Node *pNode);

protected:

	// Tempo-map independent coefficients.
	float pixelRate() const { return m_fPixelRate; }
	float frameRate() const { return m_fFrameRate; }

private:

	unsigned short m_iSnapPerBeat;      // Snap per beat (divisor).
	unsigned short m_iHorizontalZoom;   // Horizontal zoom factor.
	unsigned short m_iVerticalZoom;     // Vertical zoom factor.

	DisplayFormat  m_displayFormat;     // Textual display format.

	unsigned int   m_iSampleRate;       // Sample rate (frames per second)
	unsigned short m_iTicksPerBeat;     // Tticks per quarter note (PPQN)
	unsigned short m_iPixelsPerBeat;    // Pixels per beat (width).

	// Tempo-map node list.
	qtractorList<Node> m_nodes;

	// Internal node cursor.
	Cursor m_cursor;

	// Tempo-map independent coefficients.
	float m_fPixelRate;
	float m_fFrameRate;

	// Location marker list.
	qtractorList<Marker> m_markers;

	// Internal node cursor.
	MarkerCursor m_markerCursor;
};

#endif	// __qtractorTimeScale_h


// end of qtractorTimeScale.h
