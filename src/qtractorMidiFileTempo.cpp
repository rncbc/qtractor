// qtractorMidiFileTempo.cpp
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

#include "qtractorMidiFileTempo.h"

#include "qtractorMidiFile.h"
#include "qtractorTimeScale.h"


//----------------------------------------------------------------------
// class qtractorMidiFileTempo -- MIDI tempo/time-signature map class.
//

// (Re)initializer method.
void qtractorMidiFileTempo::clear (void)
{
	m_nodes.setAutoDelete(true);
	m_markers.setAutoDelete(true);

	// Clear/reset tempo-map and location markers...
	m_nodes.clear();
	m_markers.clear();

	// There must always be one node, always.
	addNode(0);
}


// Update node coefficient divisor factors.
void qtractorMidiFileTempo::Node::update ( qtractorMidiFile *pMidiFile )
{
	ticksPerBeat = pMidiFile->ticksPerBeat();
	if (beatDivisor > 2) {
		ticksPerBeat >>= (beatDivisor - 2);
	} else if (beatDivisor < 2) {
		ticksPerBeat <<= (2 - beatDivisor);
	}
}


// Update tempo-map node position metrics.
void qtractorMidiFileTempo::Node::reset ( qtractorMidiFileTempo::Node *pNode )
{
	if (bar > pNode->bar)
		tick = pNode->tickFromBar(bar);
	else
		bar = pNode->barFromTick(tick);
}


// Tempo-map node seeker (by tick).
qtractorMidiFileTempo::Node *qtractorMidiFileTempo::seekNode ( unsigned long iTick ) const
{
	Node *pNode = m_nodes.first();

	// Seek tick forward...
	while (pNode && pNode->next() && iTick >= (pNode->next())->tick)
		pNode = pNode->next();

	return pNode;
}


// Node list specifics.
qtractorMidiFileTempo::Node *qtractorMidiFileTempo::addNode (
	unsigned long iTick, float fTempo,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
{
	Node *pNode = 0;

	// Seek for the nearest preceding node...
	Node *pPrev = seekNode(iTick);
	// Snap to nearest bar...
	if (pPrev) {
		iTick = pPrev->tickSnapToBar(iTick);
		pPrev = seekNode(iTick);
	}
	// Either update existing node or add new one...
	Node *pNext = (pPrev ? pPrev->next() : 0);
	if (pPrev && pPrev->tick == iTick) {
		// Update exact matching node...
		pNode = pPrev;
		pNode->tempo = fTempo;
		pNode->beatsPerBar = iBeatsPerBar;
		pNode->beatDivisor = iBeatDivisor;
	} else if (pPrev && pPrev->tempo == fTempo
		&& pPrev->beatsPerBar == iBeatsPerBar
		&& pPrev->beatDivisor == iBeatDivisor) {
		// No need for a new node...
		return pPrev;
	} else if (pNext && pNext->tempo == fTempo
		&& pNext->beatsPerBar == iBeatsPerBar
		&& pNext->beatDivisor == iBeatDivisor) {
		// Update next exact matching node...
		pNode = pNext;
		pNode->tick = iTick;
		pNode->bar = 0;
	} else {
		// Add/insert a new node...
		pNode = new Node(iTick, fTempo, iBeatsPerBar, iBeatDivisor);
		if (pPrev)
			m_nodes.insertAfter(pNode, pPrev);
		else
			m_nodes.append(pNode);
	}

	// Update coefficients and positioning thereafter...
	updateNode(pNode);

	return pNode;
}


void qtractorMidiFileTempo::updateNode ( qtractorMidiFileTempo::Node *pNode )
{
	// Update coefficients...
	pNode->update(m_pMidiFile);

	// Update positioning on all nodes thereafter...
	Node *pPrev = pNode->prev();
	while (pNode) {
		if (pPrev) pNode->reset(pPrev);
		pPrev = pNode;
		pNode = pNode->next();
	}
}


void qtractorMidiFileTempo::removeNode ( qtractorMidiFileTempo::Node *pNode )
{
	// Don't ever remove the very first node... 
	Node *pPrev = pNode->prev();
	if (pPrev == 0)
		return;

	// Update positioning on all nodes thereafter...
	Node *pNext = pNode->next();
	while (pNext) {
		if (pPrev) pNext->reset(pPrev);
		pPrev = pNext;
		pNext = pNext->next();
	}

	// Actually remove/unlink the node...
	m_nodes.remove(pNode);
}


// Location marker seeker (by tick).
qtractorMidiFileTempo::Marker *qtractorMidiFileTempo::seekMarker ( unsigned long iTick ) const
{
	Marker *pMarker = m_markers.first();

	// Seek tick forward...
	while (pMarker && pMarker->next() && iTick >= (pMarker->next())->tick)
		pMarker = pMarker->next();

	return pMarker;
}


// Marker list specifics.
qtractorMidiFileTempo::Marker *qtractorMidiFileTempo::addMarker (
	unsigned long iTick, const QString& sText )
{
	Marker *pMarker = 0;

	// Snap to nearest bar...
	Node *pNodePrev = seekNode(iTick);
	if (pNodePrev)
		iTick = pNodePrev->tickSnapToBar(iTick);

	// Seek for the nearest preceding node...
	Marker *pMarkerPrev = seekMarker(iTick);
	// Either update existing marker or add new one...
	if (pMarkerPrev && pMarkerPrev->tick == iTick) {
		// Update exact matching node...
		pMarker = pMarkerPrev;
		pMarker->text = sText;
	} else {
		// Add/insert a new marker...
		pMarker = new Marker(iTick, sText);
		if (pMarkerPrev)
			m_markers.insertAfter(pMarker, pMarkerPrev);
		else
			m_markers.append(pMarker);
	}

	return pMarker;
}


void qtractorMidiFileTempo::removeMarker ( qtractorMidiFileTempo::Marker *pMarker )
{
	// Actually remove/unlink the marker
	m_markers.remove(pMarker);
}


// Time-scale sync methods.
void qtractorMidiFileTempo::fromTimeScale (
	qtractorTimeScale *pTimeScale, unsigned long iTimeOffset )
{
	if (pTimeScale == NULL)
		return;

	const unsigned short p = pTimeScale->ticksPerBeat();
	const unsigned short q = m_pMidiFile->ticksPerBeat();

	if (q < 1) return;

	// Copy tempo-map nodes...
	m_nodes.clear();

	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekTick(iTimeOffset);
	while (pNode) {
		unsigned long iTime = uint64_t(pNode->tick) * p / q;
		iTime = (iTime > iTimeOffset ? iTime - iTimeOffset : 0);
		addNode(iTime, pNode->tempo,
			pNode->beatsPerBar,
			pNode->beatDivisor);
		pNode = pNode->next();
	}

	// Copy location markers...
	m_markers.clear();

	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekTick(iTimeOffset);
	while (pMarker) {
		unsigned long iTick = pTimeScale->tickFromFrame(pMarker->frame);
		unsigned long iTime = uint64_t(iTick) * p / q;
		iTime = (iTime > iTimeOffset ? iTime - iTimeOffset : 0);
		addMarker(iTime, pMarker->text);
		pMarker = pMarker->next();
	}
}

void qtractorMidiFileTempo::intoTimeScale (
	qtractorTimeScale *pTimeScale, unsigned long iTimeOffset )
{
	if (pTimeScale == NULL)
		return;

	const unsigned short p = m_pMidiFile->ticksPerBeat();
	const unsigned short q = pTimeScale->ticksPerBeat();

	if (q < 1) return;

	pTimeScale->reset();

	// Copy tempo-map nodes...
	qtractorMidiFileTempo::Node *pNode = m_nodes.first();
	while (pNode) {
		unsigned long iTime	= uint64_t(pNode->tick) * p / q;
		pTimeScale->addNode(
			pTimeScale->frameFromTick(iTime + iTimeOffset),
			pNode->tempo, 2,
			pNode->beatsPerBar,
			pNode->beatDivisor);
		pNode = pNode->next();
	}

	// Copy location markers...
	qtractorMidiFileTempo::Marker *pMarker = m_markers.first();
	while (pMarker) {
		unsigned long iTime	= uint64_t(pMarker->tick) * p / q;
		pTimeScale->addMarker(
			pTimeScale->frameFromTick(iTime + iTimeOffset),
			pMarker->text);
		pMarker = pMarker->next();
	}
}


// end of qtractorMidiFileTempo.cpp
