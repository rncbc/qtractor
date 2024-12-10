// qtractorTimeScale.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTimeScale.h"


//----------------------------------------------------------------------
// class qtractorTimeScale -- Time scale conversion helper class.
//

// Node list cleaner.
void qtractorTimeScale::reset (void)
{
	m_nodes.setAutoDelete(true);
	m_markers.setAutoDelete(true);

	// Clear/reset location-markers...
	m_markers.clear();
	m_markerCursor.reset();

	// Try to preserve previous tempo/time-sig, if any...
	Node *pNode = m_nodes.first();
	const float fTempo = (pNode ? pNode->tempo : 120.0f);
	const unsigned short iBeatType = (pNode ? pNode->beatType : 2);
	const unsigned short iBeatsPerBar = (pNode ? pNode->beatsPerBar : 4);
	const unsigned short iBeatDivisor = (pNode ? pNode->beatDivisor : 2);

	// Clear/reset tempo-map...
	m_nodes.clear();
	m_cursor.reset();

	// There must always be one node, always at zero-frame...
	addNode(0, fTempo, iBeatType, iBeatsPerBar, iBeatDivisor);

	// Commit new scale...
	updateScale();
}


// (Re)nitializer method.
void qtractorTimeScale::clear (void)
{
	m_iSnapPerBeat    = 4;
	m_iHorizontalZoom = 100;
	m_iVerticalZoom   = 100;

//	m_displayFormat   = Frames;

//	m_iSampleRate     = 44100;
	m_iTicksPerBeat   = TICKS_PER_BEAT_DEF;
	m_iPixelsPerBeat  = 32;

	m_iBeatsPerBar2   = 0;
	m_iBeatDivisor2   = 0;

	// Clear/reset tempo-map...
	reset();
}


// Sync method.
void qtractorTimeScale::sync ( const qtractorTimeScale& ts )
{
	// Copy master parameters...
	m_iSampleRate    = ts.m_iSampleRate;
	m_iTicksPerBeat  = ts.m_iTicksPerBeat;
	m_iPixelsPerBeat = ts.m_iPixelsPerBeat;

	// Copy location markers...
	m_markers.clear();
	Marker *pMarker = ts.m_markers.first();
	while (pMarker) {
		m_markers.append(new Marker(*pMarker));
		pMarker = pMarker->next();
	}
	m_markerCursor.reset();

	// Copy tempo-map nodes...
	m_nodes.clear();
	Node *pNode = ts.nodes().first();
	while (pNode) {
		m_nodes.append(new Node(this, pNode->frame,
			pNode->tempo, pNode->beatType,
			pNode->beatsPerBar, pNode->beatDivisor));
		pNode = pNode->next();
	}
	m_cursor.reset();

	updateScale();
}


// Copy method.
qtractorTimeScale& qtractorTimeScale::copy ( const qtractorTimeScale& ts )
{
	if (&ts != this) {

		m_nodes.setAutoDelete(true);
		m_markers.setAutoDelete(true);

		m_iSnapPerBeat    = ts.m_iSnapPerBeat;
		m_iHorizontalZoom = ts.m_iHorizontalZoom;
		m_iVerticalZoom   = ts.m_iVerticalZoom;

		m_displayFormat   = ts.m_displayFormat;

		m_iBeatsPerBar2   = ts.m_iBeatsPerBar2;
		m_iBeatDivisor2   = ts.m_iBeatDivisor2;

		// Sync/copy tempo-map nodes...
		sync(ts);
	}

	return *this;
}


// Update scale coefficient divisor factors.
void qtractorTimeScale::Node::update (void)
{
	ticksPerBeat = ts->ticksPerBeat();
	tickRate = tempo * ticksPerBeat;
	beatRate = tempo;
#if 1// nb. standard MIDI tempo (BPM) is beatType=2 (quarter notes) per minute.
	if (beatDivisor > beatType) {
		const unsigned short n = (beatDivisor - beatType);
		ticksPerBeat >>= n;
		beatRate *= float(1 << n);
	} else if (beatDivisor < beatType) {
		const unsigned short n = (beatType - beatDivisor);
		ticksPerBeat <<= n;
		beatRate /= float(1 << n);
	}
#endif
}


// Update time-scale node position metrics.
void qtractorTimeScale::Node::reset ( qtractorTimeScale::Node *pNode )
{
	if (bar > pNode->bar)
		frame = pNode->frameFromBar(bar);
	else
		bar = pNode->barFromFrame(frame);

	beat = pNode->beatFromFrame(frame);
 	tick = pNode->tickFromFrame(frame);

	pixel = ts->pixelFromFrame(frame);
}


// Tempo accessor/convertors. (default's quarter notes per minute)
void qtractorTimeScale::Node::setTempoEx (
	float fTempo, unsigned short iBeatType )
{
	if (iBeatType > beatType)
		fTempo /= float(1 << (iBeatType - beatType));
	else if (beatType > iBeatType)
		fTempo *= float(1 << (beatType - iBeatType));
	tempo = fTempo;
}

float qtractorTimeScale::Node::tempoEx ( unsigned short iBeatType ) const
{
	float fTempo = tempo;
	if (beatType > iBeatType)
		fTempo /= float(1 << (beatType - iBeatType));
	else if (iBeatType > beatType)
		fTempo *= float(1 << (iBeatType - beatType));
	return fTempo;
}


// Beat/frame snap filters.
unsigned long qtractorTimeScale::Node::tickSnap (
	unsigned long iTick, unsigned short p ) const
{
#if 0
	unsigned long iTickSnap = iTick - tick;
	if (ts->snapPerBeat() > 0) {
		const unsigned long q = ticksPerBeat / ts->snapPerBeat();
		iTickSnap = q * ((iTickSnap + (q >> p)) / q);
	}
	return tick + iTickSnap;
#else
	const unsigned short iTicksPerBar
		= ticksPerBeat * beatsPerBar;
	const unsigned long iTickFromBar
		= tick + iTicksPerBar * ((iTick - tick) / iTicksPerBar);

	const unsigned short iBeatsPerBar2 = beatsPerBar2();
	unsigned short iTicksPerBeat2 = iTicksPerBar / iBeatsPerBar2;

	unsigned long iTickSnap = iTick - iTickFromBar;
	if (ts->snapPerBeat() > 0) {
		const unsigned long q = iTicksPerBeat2 / ts->snapPerBeat();
		iTickSnap = q * ((iTickSnap + (q >> p)) / q);
	}

	return iTickFromBar + iTickSnap;
#endif
}


// Alternate (secondary) time-sig helper methods
unsigned short qtractorTimeScale::Node::beatsPerBar2 (void) const
{
	unsigned short iBeatsPerBar2 = ts->beatsPerBar2();
	if (iBeatsPerBar2 < 1)
		iBeatsPerBar2 = beatsPerBar;

	const unsigned short iBeatDivisor2 = ts->beatDivisor2();
	if (iBeatDivisor2 > 0) {
		if (beatDivisor > iBeatDivisor2)
			iBeatsPerBar2 >>= (beatDivisor - iBeatDivisor2);
		else
		if (beatDivisor < iBeatDivisor2)
			iBeatsPerBar2 <<= (iBeatDivisor2 - beatDivisor);
	}

	return iBeatsPerBar2;
}

unsigned short qtractorTimeScale::Node::ticksPerBeat2 (void) const
{
	return (ticksPerBeat * beatsPerBar) / beatsPerBar2();
}


// Time-scale cursor frame positioning reset.
void qtractorTimeScale::Cursor::reset ( qtractorTimeScale::Node *pNode )
{
	node = (pNode ? pNode : ts->nodes().first());
}


// Time-scale cursor node seeker (by frame).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekFrame (
	unsigned long iFrame )
{
	if (node == nullptr) {
		node = ts->nodes().first();
		if (node == nullptr)
			return nullptr;
	}

	if (iFrame > node->frame) {
		// Seek frame forward...
		while (node && node->next() && iFrame >= (node->next())->frame)
			node = node->next();
	}
	else
	if (iFrame < node->frame) {
		// Seek frame backward...
		while (node && node->frame > iFrame)
			node = node->prev();
		if (node == nullptr)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by bar).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekBar (
	unsigned short iBar )
{
	if (node == nullptr) {
		node = ts->nodes().first();
		if (node == nullptr)
			return nullptr;
	}

	if (iBar > node->bar) {
		// Seek bar forward...
		while (node && node->next() && iBar >= (node->next())->bar)
			node = node->next();
	}
	else
	if (iBar < node->bar) {
		// Seek bar backward...
		while (node && node->bar > iBar)
			node = node->prev();
		if (node == nullptr)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by beat).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekBeat (
	unsigned int iBeat )
{
	if (node == nullptr) {
		node = ts->nodes().first();
		if (node == nullptr)
			return nullptr;
	}

	if (iBeat > node->beat) {
		// Seek beat forward...
		while (node && node->next() && iBeat >= (node->next())->beat)
			node = node->next();
	}
	else
	if (iBeat < node->beat) {
		// Seek beat backward...
		while (node && node->beat > iBeat)
			node = node->prev();
		if (node == nullptr)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by tick).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekTick (
	unsigned long iTick )
{
	if (node == nullptr) {
		node = ts->nodes().first();
		if (node == nullptr)
			return nullptr;
	}

	if (iTick > node->tick) {
		// Seek tick forward...
		while (node && node->next() && iTick >= (node->next())->tick)
			node = node->next();
	}
	else
	if (iTick < node->tick) {
		// Seek tick backward...
		while (node && node->tick > iTick)
			node = node->prev();
		if (node == nullptr)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by pixel).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekPixel ( int x )
{
	if (node == nullptr) {
		node = ts->nodes().first();
		if (node == nullptr)
			return nullptr;
	}

	if (x > node->pixel) {
		// Seek pixel forward...
		while (node && node->next() && x >= (node->next())->pixel)
			node = node->next();
	}
	else
	if (x < node->pixel) {
		// Seek tick backward...
		while (node && node->pixel > x)
			node = node->prev();
		if (node == nullptr)
			node = ts->nodes().first();
	}

	return node;
}


// Node list specifics.
qtractorTimeScale::Node *qtractorTimeScale::addNode (
	unsigned long iFrame, float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
{
	Node *pNode	= nullptr;

	// Seek for the nearest preceding node...
	Node *pPrev = m_cursor.seekFrame(iFrame);
	// Snap frame to nearest bar...
	if (pPrev) {
		iFrame = pPrev->frameSnapToBar(iFrame);
		pPrev = m_cursor.seekFrame(iFrame);
	}
	// Either update existing node or add new one...
	Node *pNext = (pPrev ? pPrev->next() : nullptr);
	if (pPrev && pPrev->frame == iFrame) {
		// Update exact matching node...
		pNode = pPrev;
		pNode->tempo = fTempo;
		pNode->beatType = iBeatType;
		pNode->beatsPerBar = iBeatsPerBar;
		pNode->beatDivisor = iBeatDivisor;
	} else if (pPrev && qAbs(pPrev->tempo - fTempo) < 0.001f
		&& pPrev->beatType == iBeatType
		&& pPrev->beatsPerBar == iBeatsPerBar
		&& pPrev->beatDivisor == iBeatDivisor) {
		// No need for a new node...
		return pPrev;
	} else if (pNext && qAbs(pNext->tempo - fTempo) < 0.001f
		&& pNext->beatType == iBeatType
		&& pNext->beatsPerBar == iBeatsPerBar
		&& pNext->beatDivisor == iBeatDivisor) {
		// Update next exact matching node...
		pNode = pNext;
		pNode->frame = iFrame;
		pNode->bar = 0;
	} else {
		// Add/insert a new node...
		pNode = new Node(this,
			iFrame, fTempo, iBeatType, iBeatsPerBar, iBeatDivisor);
		if (pPrev)
			m_nodes.insertAfter(pNode, pPrev);
		else
			m_nodes.append(pNode);
	}

	// Update coefficients and positioning thereafter...
	updateNode(pNode);

	return pNode;
}


void qtractorTimeScale::updateNode ( qtractorTimeScale::Node *pNode )
{
	// Update coefficients...
	pNode->update();

	// Relocate internal cursor...
	m_cursor.reset(pNode);

	// Update positioning on all nodes thereafter...
	Node *pNext = pNode;
	Node *pPrev = pNext->prev();
	while (pNext) {
		if (pPrev) pNext->reset(pPrev);
		pPrev = pNext;
		pNext = pNext->next();
	}

	// And update marker/bar positions too...
	updateMarkers(pNode->prev());
}


void qtractorTimeScale::removeNode ( qtractorTimeScale::Node *pNode )
{
	// Don't ever remove the very first node... 
	Node *pNodePrev = pNode->prev();
	if (pNodePrev == nullptr)
		return;

	// Relocate internal cursor...
	m_cursor.reset(pNodePrev);

	// Update positioning on all nodes thereafter...
	Node *pPrev = pNodePrev;
	Node *pNext = pNode->next();
	while (pNext) {
		if (pPrev) pNext->reset(pPrev);
		pPrev = pNext;
		pNext = pNext->next();
	}

	// Actually remove/unlink the node...
	m_nodes.remove(pNode);

	// Then update marker/bar positions too...
	updateMarkers(pNodePrev);
}


// Complete time-scale update method.
void qtractorTimeScale::updateScale (void)
{
	// Update time-map independent coefficients...
	m_fPixelRate = 1.20f * float(m_iHorizontalZoom * m_iPixelsPerBeat);
	m_fFrameRate = 60.0f * float(m_iSampleRate);

	// Update all nodes thereafter...
	Node *pPrev = nullptr;
	Node *pNext = m_nodes.first();
	while (pNext) {
		pNext->update();
		if (pPrev) pNext->reset(pPrev);
		pPrev = pNext;
		pNext = pNext->next();
	}

	// Also update all marker/bar positions too...
	updateMarkers(m_nodes.first());
}


// Convert frames to time string and vice-versa.
unsigned long qtractorTimeScale::frameFromTextEx (
	DisplayFormat displayFormat,
	const QString& sText, bool bDelta, unsigned long iFrame )
{
	switch (displayFormat) {

		case BBT:
		{
			// Time frame code in bars.beats.ticks ...
			unsigned short bars  = sText.section('.', 0, 0).toUShort();
			unsigned int   beats = sText.section('.', 1, 1).toUInt();
			unsigned long  ticks = sText.section('.', 2).toULong();
			Node *pNode;
			if (bDelta) {
				pNode = m_cursor.seekFrame(iFrame);
				if (pNode) {
					beats += bars  * pNode->beatsPerBar;
					ticks += beats * pNode->ticksPerBeat2();
					ticks += pNode->tickFromFrame(iFrame);
					iFrame = pNode->frameFromTick(ticks) - iFrame;
				}
			} else {
				if (bars > 0)
					--bars;
				if (beats > 0)
					--beats;
				pNode = m_cursor.seekBar(bars);
				if (pNode) {
					beats += (bars - pNode->bar) * pNode->beatsPerBar2();
					ticks += pNode->tick + beats * pNode->ticksPerBeat2();
					iFrame = pNode->frameFromTick(ticks);
				}
			}
			break;
		}

		case Time:
		{
			// Time frame code in hh:mm:ss.zzz ...
			unsigned int hh = sText.section(':', 0, 0).toUInt();
			unsigned int mm = sText.section(':', 1, 1).toUInt();
			float secs = sText.section(':', 2).toFloat();
			mm   += 60 * hh;
			secs += 60.f * float(mm);
			iFrame = uroundf(secs * float(m_iSampleRate));
			break;
		}

		case Frames:
		{
			iFrame = sText.toULong();
			break;
		}
	}

	return iFrame;
}


unsigned long qtractorTimeScale::frameFromText (
	const QString& sText, bool bDelta, unsigned long iFrame )
{
	return frameFromTextEx(m_displayFormat, sText, bDelta, iFrame);
}


QString qtractorTimeScale::textFromFrameEx (
	DisplayFormat displayFormat,
	unsigned long iFrame, bool bDelta, unsigned long iDelta )
{
	QString sText;

	switch (displayFormat) {

		case BBT:
		{
			// Time frame code in bars.beats.ticks ...
			unsigned short bars  = 0;
			unsigned int   beats = 0;
			unsigned long  ticks = 0;
			Node *pNode = m_cursor.seekFrame(iFrame);
			if (pNode) {
				const unsigned long t0 = pNode->tickFromFrame(iFrame);
				if (bDelta) {
					const unsigned long iFrameEnd = iFrame + iDelta;
					pNode = m_cursor.seekFrame(iFrameEnd);
					ticks = pNode->tickFromFrame(iFrameEnd) - t0;
				} else {
					ticks = t0 - pNode->tick;
				}
				if (ticks >= (unsigned long) pNode->ticksPerBeat) {
					beats  = (unsigned int) (ticks / pNode->ticksPerBeat);
					ticks -= (unsigned long) (beats * pNode->ticksPerBeat);
				}
				if (beats >= (unsigned int) pNode->beatsPerBar) {
					bars   = (unsigned short) (beats / pNode->beatsPerBar);
					beats -= (unsigned int) (bars * pNode->beatsPerBar);
				}
				if (beatsPerBar2() > 0 || beatDivisor2() > 0) {
					const unsigned short iTicksPerBeat2 = pNode->ticksPerBeat2();
					ticks += (unsigned long) (beats * pNode->ticksPerBeat);
					if (ticks >= (unsigned long) iTicksPerBeat2) {
						beats  = (unsigned int) (ticks / iTicksPerBeat2);
						ticks -= (unsigned long) (beats * iTicksPerBeat2);
					}
				}
				if (!bDelta)
					bars += pNode->bar;
			}
			if (!bDelta) {
				++bars;
				++beats;
			}
		#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
			sText.sprintf("%u.%u.%03lu", bars, beats, ticks);
		#else
			sText = QString::asprintf("%u.%u.%03lu", bars, beats, ticks);
		#endif
			break;
		}

		case Time:
		{
			// Time frame code in hh:mm:ss.zzz ...
			unsigned int hh, mm, ss, zzz;
			float secs = float(bDelta ? iDelta : iFrame) / float(m_iSampleRate);
			hh = mm = ss = 0;
			if (secs >= 3600.0f) {
				hh = (unsigned int) (secs / 3600.0f);
				secs -= float(hh) * 3600.0f;
			}
			if (secs >= 60.0f) {
				mm = (unsigned int) (secs / 60.0f);
				secs -= float(mm) * 60.0f;
			}
			if (secs >= 0.0f) {
				ss = (unsigned int) secs;
				secs -= float(ss);
			}
			zzz = (unsigned int) (secs * 1000.0f);
		#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
			sText.sprintf("%02u:%02u:%02u.%03u", hh, mm, ss, zzz);
		#else
			sText = QString::asprintf("%02u:%02u:%02u.%03u", hh, mm, ss, zzz);
		#endif
			break;
		}

		case Frames:
		{
			sText = QString::number(bDelta ? iDelta : iFrame);
			break;
		}
	}

	return sText;
}


QString qtractorTimeScale::textFromFrame (
	unsigned long iFrame, bool bDelta, unsigned long iDelta )
{
	return textFromFrameEx(m_displayFormat, iFrame, bDelta, iDelta);
}


// Convert ticks to time string and vice-versa.
unsigned long qtractorTimeScale::tickFromText (
	const QString& sText, bool bDelta, unsigned long iTick )
{
	unsigned long iFrame = 0;
	if (bDelta) {
		Node *pNode = m_cursor.seekTick(iTick);
		iFrame = (pNode ? pNode->frameFromTick(iTick) : 0);
	}
	return tickFromFrame(frameFromText(sText, bDelta, iFrame));
}


QString qtractorTimeScale::textFromTick (
	unsigned long iTick, bool bDelta, unsigned long iDelta )
{
	Node *pNode = m_cursor.seekTick(iTick);
	const unsigned long iFrame
		= (pNode ? pNode->frameFromTick(iTick) : 0);
	if (bDelta > 0 && pNode) {
		iTick += iDelta;
		pNode  = m_cursor.seekTick(iTick);
		iDelta = (pNode ? pNode->frameFromTick(iTick) - iFrame : 0);
	}
	return textFromFrame(iFrame, bDelta, iDelta);
}


// Beat divisor (snap index) map.
static int s_aiSnapPerBeat[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 21, 24, 28, 32, 48, 64, 96
};

const int c_iSnapItemCount = sizeof(s_aiSnapPerBeat) / sizeof(int);

// Beat divisor (snap index) accessors.
unsigned short qtractorTimeScale::snapFromIndex ( int iSnap )
{
	return s_aiSnapPerBeat[iSnap];
}


// Beat divisor (snap index) accessors.
int qtractorTimeScale::indexFromSnap ( unsigned short iSnapPerBeat )
{
	for (int iSnap = 0; iSnap < c_iSnapItemCount; ++iSnap) {
		if (s_aiSnapPerBeat[iSnap] == iSnapPerBeat)
			return iSnap;
	}

	return 0;
}


// Beat divisor (snap index) text item list.
QStringList qtractorTimeScale::snapItems (void)
{
	QStringList items;

	int iSnap = 0;
	items.append(QObject::tr("None"));
	++iSnap;

	QString sPrefix = QObject::tr("Beat");
	items.append(sPrefix);
	++iSnap;

	sPrefix += "/%1";
	while (iSnap < c_iSnapItemCount)
		items.append(sPrefix.arg(s_aiSnapPerBeat[iSnap++]));

	return items;
}


// Tick/Frame range conversion (delta conversion).
unsigned long qtractorTimeScale::frameFromTickRange (
	unsigned long iTickStart, unsigned long iTickEnd, bool bOffset )
{
	Node *pNode = m_cursor.seekTick(iTickStart);
	const unsigned long iFrameStart
		= (pNode ? pNode->frameFromTick(iTickStart) : 0);
	if (!bOffset) pNode = m_cursor.seekTick(iTickEnd);
	const unsigned long iFrameEnd
		= (pNode ? pNode->frameFromTick(iTickEnd) : 0);
	return (iFrameEnd > iFrameStart ? iFrameEnd - iFrameStart : 0);
}


unsigned long qtractorTimeScale::tickFromFrameRange (
	unsigned long iFrameStart, unsigned long iFrameEnd, bool bOffset )
{
	Node *pNode = m_cursor.seekFrame(iFrameStart);
	const unsigned long iTickStart
		= (pNode ? pNode->tickFromFrame(iFrameStart) : 0);
	if (!bOffset) pNode = m_cursor.seekFrame(iFrameEnd);
	const unsigned long iTickEnd
		= (pNode ? pNode->tickFromFrame(iFrameEnd) : 0);
	return (iTickEnd > iTickStart ? iTickEnd - iTickStart : 0);
}


// Location marker reset method.
void qtractorTimeScale::MarkerCursor::reset (
	qtractorTimeScale::Marker *pMarker )
{
	marker = (pMarker ? pMarker : ts->markers().first());
}


// Location marker seek methods.
qtractorTimeScale::Marker *qtractorTimeScale::MarkerCursor::seekFrame (
	unsigned long iFrame )
{
	if (marker == nullptr) {
		marker = ts->markers().first();
		if (marker == nullptr)
			return nullptr;
	}

	if (iFrame > marker->frame) {
		// Seek frame forward...
		while (marker && marker->next() && iFrame >= (marker->next())->frame)
			marker = marker->next();
	}
	else
	if (iFrame < marker->frame) {
		// Seek frame backward...
		while (marker && marker->frame > iFrame)
			marker = marker->prev();
		if (marker == nullptr)
			marker = ts->markers().first();
	}

	return marker;
}


qtractorTimeScale::Marker *qtractorTimeScale::MarkerCursor::seekBar (
	unsigned short iBar )
{
	return seekFrame(ts->frameFromBar(iBar));
}


qtractorTimeScale::Marker *qtractorTimeScale::MarkerCursor::seekBeat (
	unsigned int iBeat )
{
	return seekFrame(ts->frameFromBeat(iBeat));
}


qtractorTimeScale::Marker *qtractorTimeScale::MarkerCursor::seekTick (
	unsigned long iTick )
{
	return seekFrame(ts->frameFromTick(iTick));
}


qtractorTimeScale::Marker *qtractorTimeScale::MarkerCursor::seekPixel ( int x )
{
	return seekFrame(ts->frameFromPixel(x));
}



// Location markers list specifics.
qtractorTimeScale::Marker *qtractorTimeScale::addMarker (
	unsigned long iFrame, const QString& sText, const QColor& rgbColor )
{
	Marker *pMarker	= nullptr;

	// Snap to nearest bar...
	unsigned short iBar = 0;

	Node *pNodePrev = m_cursor.seekFrame(iFrame);
	if (pNodePrev) {
		iBar = pNodePrev->barFromFrame(iFrame);
		iFrame = pNodePrev->frameFromBar(iBar);
	}

	// Seek for the nearest marker...
	Marker *pMarkerNear = m_markerCursor.seekFrame(iFrame);
	// Either update existing marker or add new one...
	if (pMarkerNear && pMarkerNear->frame == iFrame) {
		// Update exact matching marker...
		pMarker = pMarkerNear;
		pMarker->bar = iBar;
		pMarker->text = sText;
		pMarker->color = rgbColor;
	} else {
		// Add/insert a new marker...
		pMarker = new Marker(iFrame, iBar, sText, rgbColor);
		if (pMarkerNear && pMarkerNear->frame > iFrame)
			m_markers.insertBefore(pMarker, pMarkerNear);
		else
		if (pMarkerNear && pMarkerNear->frame < iFrame)
			m_markers.insertAfter(pMarker, pMarkerNear);
		else
			m_markers.append(pMarker);
	}

	// Update positioning...
	updateMarker(pMarker);

	return pMarker;
}


// Key-signature list specifics.
qtractorTimeScale::Marker *qtractorTimeScale::addKeySignature (
	unsigned long iFrame, int iAccidentals, int iMode )
{
	Marker *pMarker	= nullptr;

	// Snap to nearest bar...
	unsigned short iBar = 0;

	Node *pNodePrev = m_cursor.seekFrame(iFrame);
	if (pNodePrev) {
		iBar = pNodePrev->barFromFrame(iFrame);
		iFrame = pNodePrev->frameFromBar(iBar);
	}

	// Seek for the nearest marker...
	Marker *pMarkerNear = m_markerCursor.seekFrame(iFrame);
	// Either update existing marker or add new one...
	if (pMarkerNear && pMarkerNear->frame == iFrame) {
		// Update exact matching marker...
		pMarker = pMarkerNear;
		pMarker->bar = iBar;
		pMarker->accidentals = iAccidentals;
		pMarker->mode = iMode;
	} else {
		// Add/insert a new marker...
		pMarker = new Marker(iFrame, iBar, iAccidentals, iMode);
		if (pMarkerNear && pMarkerNear->frame > iFrame)
			m_markers.insertBefore(pMarker, pMarkerNear);
		else
		if (pMarkerNear && pMarkerNear->frame < iFrame)
			m_markers.insertAfter(pMarker, pMarkerNear);
		else
			m_markers.append(pMarker);
	}

	// Update positioning...
	updateMarker(pMarker);

	return pMarker;
}


void qtractorTimeScale::updateMarker ( qtractorTimeScale::Marker *pMarker )
{
	// Relocate internal cursor...
	m_markerCursor.reset(pMarker);
}


void qtractorTimeScale::removeMarker ( qtractorTimeScale::Marker *pMarker )
{
	// Actually remove/unlink the marker
	// and relocate internal cursor...
	Marker *pMarkerPrev = pMarker->prev();
	m_markers.remove(pMarker);
	m_markerCursor.reset(pMarkerPrev);
}


// Update markers from given node position.
void qtractorTimeScale::updateMarkers ( qtractorTimeScale::Node *pNode )
{
	if (pNode == nullptr)
		pNode = m_nodes.first();
	if (pNode == nullptr)
		return;

	Marker *pMarker = m_markerCursor.seekFrame(pNode->frame);
	while (pMarker) {
		while (pNode->next() && pMarker->frame > pNode->next()->frame)
			pNode = pNode->next();
		if (pMarker->frame >= pNode->frame)
			pMarker->frame  = pNode->frameFromBar(pMarker->bar);
		pMarker = pMarker->next();
	}
}


// Key signature map accessor.
//
bool qtractorTimeScale::isKeySignature ( int iAccidentals, int iMode )
{
	return (MinAccidentals <  iAccidentals
		&&  MaxAccidentals >= iAccidentals && iMode >= 0);
}


QString qtractorTimeScale::keySignatureName (
	int iAccidentals, int iMode, char chMinor )
{
	static struct
	{
		const char *natural;
		const char *sharp;
		const char *flat;

	} s_aAccidentalsTab[] = {

		{ QT_TR_NOOP("C"), QT_TR_NOOP("B#"), nullptr          },
		{ nullptr,         QT_TR_NOOP("C#"), QT_TR_NOOP("Db") },
		{ QT_TR_NOOP("D"), nullptr,          nullptr          },
		{ nullptr,         QT_TR_NOOP("D#"), QT_TR_NOOP("Eb") },
		{ QT_TR_NOOP("E"), nullptr,          QT_TR_NOOP("Fb") },
		{ QT_TR_NOOP("F"), QT_TR_NOOP("E#"), nullptr          },
		{ nullptr,         QT_TR_NOOP("F#"), QT_TR_NOOP("Gb") },
		{ QT_TR_NOOP("G"), nullptr,          nullptr          },
		{ nullptr,         QT_TR_NOOP("G#"), QT_TR_NOOP("Ab") },
		{ QT_TR_NOOP("A"), nullptr,          nullptr          },
		{ nullptr,         QT_TR_NOOP("A#"), QT_TR_NOOP("Bb") },
		{ QT_TR_NOOP("B"), nullptr,          QT_TR_NOOP("Cb") }
	};

	QString sKeySignature('-');

	if (!isKeySignature(iAccidentals, iMode))
		return sKeySignature;

	const int i
		= (iAccidentals * (iAccidentals < 0 ? -5 : 7)
		+ (iMode > 0 ? 9 : 0)) % 12;

	const char *name = nullptr;
	if (iAccidentals < 0)
		name = s_aAccidentalsTab[i].flat;
	else
	if (iAccidentals > 0)
		name = s_aAccidentalsTab[i].sharp;
	if (name == nullptr)
		name = s_aAccidentalsTab[i].natural;

	sKeySignature = tr(name);

	if (iMode && chMinor)
		sKeySignature += chMinor;

	return sKeySignature;
}


// end of qtractorTimeScale.cpp
