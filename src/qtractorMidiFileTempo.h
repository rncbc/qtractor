// qtractorMidiFileTempo.h
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiFileTempo_h
#define __qtractorMidiFileTempo_h

#include "qtractorList.h"

#include <QString>


// Forward decls.
class qtractorMidiFile;
class qtractorTimeScale;


//----------------------------------------------------------------------
// class qtractorMidiFileTempo -- MIDI tempo/time-signature map class.
//

class qtractorMidiFileTempo
{
public:

	//	Constructor.
	qtractorMidiFileTempo(qtractorMidiFile *pMidiFile)
		: m_pMidiFile(pMidiFile) { clear(); }

	//	Destructor.
	~qtractorMidiFileTempo()
		{ m_nodes.clear(); m_markers.clear(); }

	// (Re)nitializer method.
	void clear();

	// Tempo-map node declaration.
	class Node : public qtractorList<Node>::Link
	{
	public:

		// Constructor.
		Node(unsigned long iTick = 0,
			float fTempo = 120.0f,
			unsigned short iBeatsPerBar = 4,
			unsigned short iBeatDivisor = 2)
			: tick(iTick), bar(0),
				tempo(fTempo),
				beatsPerBar(iBeatsPerBar),
				beatDivisor(iBeatDivisor),
				ticksPerBeat(0) {}

		// Update node coefficients.
		void update(qtractorMidiFile *pMidiFile);

		// Update node position metrics.
		void reset(Node *pNode);

		// Tick/bar convertors.
		unsigned short barFromTick(unsigned long iTick) const
			{ return bar + ((iTick - tick) / (ticksPerBeat * beatsPerBar)); }
		unsigned long tickFromBar(unsigned short iBar) const
			{ return tick + (ticksPerBeat * beatsPerBar * (iBar - bar)) ; }

		// Tick/bar quantizer.
		unsigned long tickSnapToBar(unsigned long iTick) const
			{ return tickFromBar(barFromTick(iTick)); }

		// Node keys.
		unsigned long  tick;
		unsigned short bar;

		// Node payload.
		float          tempo;
		unsigned short beatsPerBar;
		unsigned short beatDivisor;

		// Node coefficients.
		unsigned short ticksPerBeat;
	};

	// Node list accessors.
	const qtractorList<Node>& nodes() const { return m_nodes; }

	// Node list seeker.
	Node *seekNode(unsigned long iTick) const;

	// Node list specifics.
	Node *addNode(
		unsigned long iTick = 0,
		float fTempo = 120.0f,
		unsigned short iBeatsPerBar = 4,
		unsigned short iBeatDivisor = 2);
	void updateNode(Node *pNode);
	void removeNode(Node *pNode);

	// More node list specifics (fit to corresp.META events).
	Node *addNodeTempo(unsigned long iTick, float fTempo)
	{
		Node *pNode = seekNode(iTick);
		return addNode(iTick, fTempo,
			(pNode ? pNode->beatsPerBar : 4),
			(pNode ? pNode->beatDivisor : 2));
	}

	Node *addNodeTime(unsigned long iTick,
		unsigned short iBeatsPerBar, unsigned short iBeatDivisor)
	{
		Node *pNode = seekNode(iTick);
		return addNode(iTick,
			(pNode ? pNode->tempo : 120.0f),
			iBeatsPerBar, iBeatDivisor);
	}


	// Tempo convertors (default's quarter notes per minute)
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

	// Resolution (ticks per beat)
	unsigned short ticksPerBeat() const
	{
		Node *pNode = m_nodes.first();
		return (pNode ? pNode->ticksPerBeat : 960);
	}

	// Location marker declaration.
	class Marker : public qtractorList<Marker>::Link
	{
	public:

		// Constructor.
		Marker(unsigned long iTick, const QString& sText)
			: tick(iTick), text(sText) {}

		// Marker key.
		unsigned long tick;

		// Marker payload.
		QString text;
	};

	// Marker list accessors.
	const qtractorList<Marker>& markers() const { return m_markers; }

	// Location marker seeker (by tick).
	Marker *seekMarker(unsigned long iTick) const;

	// Marker list specifics.
	Marker *addMarker(
		unsigned long iTick,
		const QString& sText);
	void removeMarker(Marker *pMarker);

	// Time-scale sync methods.
	void fromTimeScale(
		qtractorTimeScale *pTimeScale, unsigned long iTimeOffset = 0);
	void intoTimeScale(
		qtractorTimeScale *pTimeScale, unsigned long iTimeOffset = 0);

private:

	// Tempo-map owner.
	qtractorMidiFile *m_pMidiFile;

	// Tempo-map node list.
	qtractorList<Node> m_nodes;

	// Location markers list.
	qtractorList<Marker> m_markers;
};


#endif  // __qtractorMidiFileTempo_h


// end of qtractorMidiFileTempo.h
