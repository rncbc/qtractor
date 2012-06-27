// qtractorTimeScale.cpp
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

#include "qtractorTimeScale.h"


//----------------------------------------------------------------------
// class qtractorTimeScale -- Time scale conversion helper class.
//

// Node list cleaner.
void qtractorTimeScale::reset (void)
{
	m_nodes.setAutoDelete(true);

	// Clear/reset tempo-map...
	m_nodes.clear();
	m_cursor.reset();

	// There must always be one node, always.
	addNode(0);

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

	m_iSampleRate     = 44100;
	m_iTicksPerBeat   = 960;
	m_iPixelsPerBeat  = 32;

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

	// Copy tempo-map nodes...
	m_nodes.clear();
	Node *pNode = ts.m_nodes.first();
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

		m_iSampleRate     = ts.m_iSampleRate;
		m_iSnapPerBeat    = ts.m_iSnapPerBeat;
		m_iHorizontalZoom = ts.m_iHorizontalZoom;
		m_iVerticalZoom   = ts.m_iVerticalZoom;

		m_displayFormat   = ts.m_displayFormat;

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
#if 0
	if (beatDivisor > beatType) {
		unsigned short n = (beatDivisor - beatType);
		ticksPerBeat >>= n;
		beatRate *= float(1 << n);
	} else if (beatDivisor < beatType) {
		unsigned short n = (beatType - beatDivisor);
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


// Tempo accessor/convertors.
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
	unsigned long iTickSnap = iTick - tick;
	if (ts->snapPerBeat() > 0) {
		unsigned long q = ticksPerBeat / ts->snapPerBeat();
		iTickSnap = q * ((iTickSnap + (q >> p)) / q);
	}
	return tick + iTickSnap;
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
	if (node == 0) {
		node = ts->nodes().first();
		if (node == 0)
			return 0;
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
		if (node == 0)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by bar).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekBar (
	unsigned short iBar )
{
	if (node == 0) {
		node = ts->nodes().first();
		if (node == 0)
			return 0;
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
		if (node == 0)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by beat).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekBeat (
	unsigned int iBeat )
{
	if (node == 0) {
		node = ts->nodes().first();
		if (node == 0)
			return 0;
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
		if (node == 0)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by tick).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekTick (
	unsigned long iTick )
{
	if (node == 0) {
		node = ts->nodes().first();
		if (node == 0)
			return 0;
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
		if (node == 0)
			node = ts->nodes().first();
	}

	return node;
}


// Time-scale cursor node seeker (by pixel).
qtractorTimeScale::Node *qtractorTimeScale::Cursor::seekPixel ( int x )
{
	if (node == 0) {
		node = ts->nodes().first();
		if (node == 0)
			return 0;
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
		if (node == 0)
			node = ts->nodes().first();
	}

	return node;
}


// Node list specifics.
qtractorTimeScale::Node *qtractorTimeScale::addNode (
	unsigned long iFrame, float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
{
	Node *pNode	= 0;

	// Seek for the nearest preceding node...
	Node *pPrev = m_cursor.seekFrame(iFrame);
	// Snap frame to nearest bar...
	if (pPrev) {
		iFrame = pPrev->frameSnapToBar(iFrame);
		pPrev = m_cursor.seekFrame(iFrame);
	}
	// Either update existing node or add new one...
	Node *pNext = (pPrev ? pPrev->next() : 0);
	if (pPrev && pPrev->frame == iFrame) {
		// Update exact matching node...
		pNode = pPrev;
		pNode->tempo = fTempo;
		pNode->beatType = iBeatType;
		pNode->beatsPerBar = iBeatsPerBar;
		pNode->beatDivisor = iBeatDivisor;
	} else if (pPrev && pPrev->tempo == fTempo
		&& pPrev->beatType == iBeatType
		&& pPrev->beatsPerBar == iBeatsPerBar
		&& pPrev->beatDivisor == iBeatDivisor) {
		// No need for a new node...
		return pPrev;
	} else if (pNext && pNext->tempo == fTempo
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
	Node *pPrev = pNode->prev();
	while (pNode) {
		if (pPrev) pNode->reset(pPrev);
		pPrev = pNode;
		pNode = pNode->next();
	}
}


void qtractorTimeScale::removeNode ( qtractorTimeScale::Node *pNode )
{
	// Don't ever remove the very first node... 
	Node *pPrev = pNode->prev();
	if (pPrev == 0)
		return;

	// Relocate internal cursor...
	m_cursor.reset(pPrev);

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



// Complete time-scale update method.
void qtractorTimeScale::updateScale (void)
{
	// Update time-map independent coefficients...
	m_fPixelRate = 1.20f * float(m_iHorizontalZoom * m_iPixelsPerBeat);
	m_fFrameRate = 60.0f * float(m_iSampleRate);

	// Update all nodes thereafter...
	Node *pNode = m_nodes.first();
	while (pNode) {
		pNode->update();
		Node *pPrev = pNode->prev();
		if (pPrev) pNode->reset(pPrev);
		//	pNode->pixel = pPrev->pixelFromFrame(pNode->frame);
		pPrev = pNode;
		pNode = pNode->next();
	}
}


// Convert frames to time string and vice-versa.
unsigned long qtractorTimeScale::frameFromText (
	const QString& sText, bool bDelta, unsigned long iFrame )
{
	switch (m_displayFormat) {

		case BBT:
		{
			// Time frame code in bars.beats.ticks ...
			unsigned short bars  = sText.section('.', 0, 0).toUShort();
			unsigned int   beats = sText.section('.', 1, 1).toUInt();
			unsigned long  ticks = sText.section('.', 2).toULong();
			Node *pNode;
			if (bDelta) {
				pNode = m_cursor.seekFrame(iFrame);
				if (pNode)
					bars += pNode->bar;
			} else {
				if (bars > 0)
					--bars;
				if (beats > 0)
					--beats;
			}
			pNode = m_cursor.seekBar(bars);
			if (pNode) {
				beats += (bars - pNode->bar) * pNode->beatsPerBar;
				ticks += pNode->tick + beats * pNode->ticksPerBeat;
				iFrame = pNode->frameFromTick(ticks);
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
		default:
		{
			iFrame = sText.toULong();
			break;
		}
	}

	return iFrame;
}


QString qtractorTimeScale::textFromFrame (
	unsigned long iFrame, bool bDelta, unsigned long iDelta )
{
	QString sText;

	switch (m_displayFormat) {

		case BBT:
		{
			// Time frame code in bars.beats.ticks ...
			unsigned short bars  = 0;
			unsigned int   beats = 0;
			unsigned long  ticks = 0;
			Node *pNode = m_cursor.seekFrame(iFrame);
			if (pNode) {
				if (bDelta) {
					ticks = pNode->tickFromFrame(iFrame + iDelta)
						  - pNode->tickFromFrame(iFrame);
				} else {
					ticks = pNode->tickFromFrame(iFrame)
						  - pNode->tick;
				}
				if (ticks >= (unsigned long) pNode->ticksPerBeat) {
					beats  = (unsigned int) (ticks / pNode->ticksPerBeat);
					ticks -= (unsigned long) (beats * pNode->ticksPerBeat);
				}
				if (beats >= (unsigned int) pNode->beatsPerBar) {
					bars   = (unsigned short) (beats / pNode->beatsPerBar);
					beats -= (unsigned int) (bars * pNode->beatsPerBar);
				}
				if (!bDelta)
					bars += pNode->bar;
			}
			if (!bDelta) {
				++bars;
				++beats;
			}
			sText.sprintf("%u.%u.%03lu", bars, beats, ticks);
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
			sText.sprintf("%02u:%02u:%02u.%03u", hh, mm, ss, zzz);
			break;
		}

		case Frames:
		default:
		{
			sText = QString::number(bDelta ? iDelta : iFrame);
			break;
		}
	}

	return sText;
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
	unsigned long iFrame = (pNode ? pNode->frameFromTick(iTick) : 0);
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
QStringList qtractorTimeScale::snapItems ( int iSnap )
{
	QStringList items;

	if (iSnap == 0) {
		items.append(QObject::tr("None"));
		++iSnap;
	}

	QString sPrefix = QObject::tr("Beat");
	if (iSnap == 1) {
		items.append(sPrefix);
		++iSnap;
	}

	sPrefix += "/%1";
	while (iSnap < c_iSnapItemCount)
		items.append(sPrefix.arg(s_aiSnapPerBeat[iSnap++]));

	return items;
}


// Tick/Frame range conversion (delta conversion).
unsigned long qtractorTimeScale::frameFromTickRange (
	unsigned long iTickStart, unsigned long iTickEnd )
{
	Node *pNode = m_cursor.seekTick(iTickStart);
	unsigned long iFrameStart = (pNode ? pNode->frameFromTick(iTickStart) : 0);
	pNode = m_cursor.seekTick(iTickEnd);
	unsigned long iFrameEnd = (pNode ? pNode->frameFromTick(iTickEnd) : 0);
	return (iFrameEnd > iFrameStart ? iFrameEnd - iFrameStart : 0);
}


unsigned long qtractorTimeScale::tickFromFrameRange (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	Node *pNode = m_cursor.seekFrame(iFrameStart);
	unsigned long iTickStart = (pNode ? pNode->tickFromFrame(iFrameStart) : 0);
	pNode = m_cursor.seekFrame(iFrameEnd);
	unsigned long iTickEnd = (pNode ? pNode->tickFromFrame(iFrameEnd) : 0);
	return (iTickEnd > iTickStart ? iTickEnd - iTickStart : 0);
}


// end of qtractorTimeScale.cpp
