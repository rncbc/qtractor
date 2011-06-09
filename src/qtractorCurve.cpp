// qtractorCurve.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorCurve.h"

#include "qtractorTimeScale.h"

#include <math.h>


//----------------------------------------------------------------------
// qtractorCurve::updateNode -- Node coefficients computation helpers.
//

// (Sample &)Hold.
inline void updateNodeHold ( qtractorCurve::Node *pNode, float y0,
	const qtractorCurve::Node *pPrev, const qtractorCurve::Node */*pNext*/ )
{
	if (pPrev)
		pNode->a = pPrev->value;
	else
		pNode->a = y0;

	pNode->b = pNode->c = pNode->d = 0.0f;
}


// Linear.
inline void updateNodeLinear ( qtractorCurve::Node *pNode, float y0,
	const qtractorCurve::Node *pPrev, const qtractorCurve::Node */*pNext*/ )
{
	float y1, x1 = float(pNode->frame);

	if (pPrev) {
		x1 -= float(pPrev->frame);
		y1 = pPrev->value;
	} else {
		y1 = y0;
	}

	pNode->b = pNode->value;

	if (x1 > 0.0f)
		pNode->a = (y1 - pNode->b) / x1;
	else
		pNode->a = 0.0f;

	pNode->c = pNode->d = 0.0f;
}


// Spline.
inline void updateNodeSpline ( qtractorCurve::Node *pNode, float y0,
	const qtractorCurve::Node *pPrev, const qtractorCurve::Node *pNext )
{
	// Shamelessly using the same reference source article as Ardour ;)
	// CJC Kuger, "Constrained Cubic Spline Interpolation", August 2002
	// http://www.korf.co.uk/spline.pdf
	const float FZERO = 1e-9f;

	float x0 = float(pNode->frame);

	float y1, x1 = x0;
	float s1 = 0.0f;
	float f1 = 0.0f;

	float y2 = pNode->value;
	float s2 = 0.0f;
	float f2 = 0.0f;

	if (pPrev) {
		x1 -= float(pPrev->frame);
		y1 = pPrev->value;
	} else {
		y1 = y0;
	}

	if (fabs(y1 - y2) > FZERO)
		s1 =  x1 / (y1 - y2);

	if (pPrev) {
		pPrev = pPrev->prev();
		if (pPrev && (fabs(y1 - pPrev->value) > FZERO)) {
			float s0 = (x1 - float(pPrev->frame) - x0) / (y1 - pPrev->value);
			if (s1 * s0 > 0.0f && fabs(s1 + s0) > FZERO)
				f1 = 2.0f / (s1 + s0);
		}
	}

	if (pNext && (fabs(pNext->value - y2) > FZERO))
		s2 = (float(pNext->frame) - x0) / (pNext->value - y2);
	if (s2 * s1 > 0.0f && fabs(s2 + s1) > FZERO)
		f2 = 2.0f / (s2 + s1);

	float x12 = x1 * x1;
	float dy = y2 - y1;

	// Compute second derivative for either side of control point...
	float ff1 = (((2.0f * (f2 + (2.0f * f1))) / x1)) + ((6.0f * dy) / x12);
	float ff2 = (-2.0f * ((2.0f * f2) + f1) / x1) - ((6.0f * dy) / x12);

	// Compute and store polynomial coefficients...
	pNode->a = (ff1 - ff2) / (6.0f * x1);
	pNode->b = ff2 / 2.0f;
	pNode->c = - (dy / x1 + (pNode->b * x1) + (pNode->a * x12));
	pNode->d = y1 - (pNode->c * x1) - (pNode->b * x12) - (pNode->a * x12 * x1);
}


//----------------------------------------------------------------------
// qtractorCurve::value -- Interpolation computation helpers.
//

// (Sample &)Hold.
inline float valueHold ( const qtractorCurve::Node *pNode, float /*x*/ )
{
	return pNode->a;
}


// Linear.
inline float valueLinear ( const qtractorCurve::Node *pNode, float x )
{
	return pNode->a * x + pNode->b;
}


// Spline.
inline float valueSpline ( const qtractorCurve::Node *pNode, float x )
{
	return ((pNode->a * x + pNode->b) * x + pNode->c) * x + pNode->d;
}


//----------------------------------------------------------------------
// class qtractorCurve -- The generic curve declaration.
//

// Constructor.
qtractorCurve::qtractorCurve ( qtractorCurveList *pList,
	qtractorSubject *pSubject, Mode mode, unsigned int iMinFrameDist )
	: m_pList(pList), m_mode(mode), m_iMinFrameDist(iMinFrameDist),
		m_observer(pSubject, this), m_state(Idle), m_cursor(this),
		m_color(Qt::red), m_bToggled(false)
{
	m_nodes.setAutoDelete(true);
	m_cursors.setAutoDelete(false);

	m_tail.frame = 0;
//	m_tail.value = m_observer.defaultValue();

	clear();

	m_pList->addCurve(this);
}

// Destructor.
qtractorCurve::~qtractorCurve (void)
{
#if 0	
	m_pList->removeCurve(this);

	clear();
#endif
}


// Curve list reset method.
void qtractorCurve::clear (void)
{
//	m_state = Idle;

//	m_tail.frame = 0;
	m_tail.value = m_observer.defaultValue();

	m_tail.a = 0.0f;
	m_tail.b = 0.0f;
	m_tail.c = 0.0f;
	m_tail.d = 0.0f;

	m_nodes.clear();

	resetNode(NULL);

	m_iDirtyCount = 0;
}


// Insert a new node, in frame order.
qtractorCurve::Node *qtractorCurve::insertNode (
	unsigned long iFrame, float fValue, bool bSmooth )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::insertNode(%lu, %g, %d)",
		this, iFrame, fValue, int(bSmooth));
#endif

	const float fMaxValue = m_observer.maxValue();
	const float fMinValue = m_observer.minValue();

	if (m_bToggled) {
		const float fMidValue = 0.5f * (fMaxValue + fMinValue);
		fValue = (fValue > fMidValue ?  fMaxValue : fMinValue);
	} 
	else 
	if (fValue > fMaxValue)
		fValue = fMaxValue;
	else
	if (fValue < fMinValue)
		fValue = fMinValue;

	Node *pNode = NULL;
	Node *pNext = m_cursor.seek(frameDist(iFrame));
	Node *pPrev = (pNext ? pNext->prev() : m_nodes.last());

	if (pNext && pNext->frame == m_cursor.frame())
		pNode = pNext;
	else
	if (pPrev && pPrev->frame == m_cursor.frame())
		pNode = pPrev;
	else
	if (bSmooth) {
		const float fThreshold = 0.01f * (fMaxValue - fMinValue);
		float y0 = (pPrev ? pPrev->value : m_tail.value);
		float y1 = fValue;
		float y2 = (pNext ? pNext->value : m_tail.value);
		if (m_mode == Hold || m_bToggled) {
			if (fabs(y1 - y0) < fThreshold)
				return NULL;
			if (fabs(y2 - y1) < fThreshold)
				pNode = pNext;
		} else {
			float x0 = (pPrev ? float(pPrev->frame) : 0.0f);
			float x1 = float(iFrame);
			float x2 = (pNext ? float(pNext->frame) : m_tail.frame);
			float s1 = (x1 > x0 ? (y1 - y0) / (x1 - x0) : 0.0f);
			float y3 = (x2 > x1 ? s1 * (x2 - x1) + y1 : y1);
			if (fabs(y3 - y2) < fThreshold)
				return NULL;
			if (pPrev) {
				pNode = pPrev;
				pPrev = pNode->prev();
				x0 = (pPrev ? float(pPrev->frame) : 0.0f);
				y0 = (pPrev ? pPrev->value : m_tail.value);
				x1 = float(pNode->frame);
				y1 = pNode->value;
				x2 = float(iFrame);
				y2 = fValue;
				s1 = (y1 - y0) / (x1 - x0);
				y3 = s1 * (x2 - x1) + y1;
				if (fabs(y3 - y2) > fThreshold)
					pNode = NULL;
			}
		}
	}

	if (pNode) {
		// Move/update the existing one as average...
		pNode->frame = m_cursor.frame();
		pNode->value = fValue;
	} else {
		// Create a brand new node,
		// insert it in the right frame...
		pNode = new Node(m_cursor.frame(), fValue);
		if (pNext)
			m_nodes.insertBefore(pNode, pNext);
		else
			m_nodes.append(pNode);
	}

	// Done.
	return pNode;
}


qtractorCurve::Node *qtractorCurve::addNode ( unsigned long iFrame, float fValue )
{
	Node *pNode = insertNode(iFrame, fValue, true);

	if (pNode)
		updateNode(pNode);

	// Dirty up...
	++m_iDirtyCount;

	m_pList->notify();

	return pNode;
}


// Remove an existing node from curve.
void qtractorCurve::removeNode ( unsigned long iFrame )
{
	removeNode(m_cursor.seek(frameDist(iFrame)));
}


void qtractorCurve::removeNode ( Node *pNode )
{
	if (pNode == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::removeNode(%p)", this, pNode);
#endif

	resetNode(pNode);

	Node *pNext = pNode->next();
	m_nodes.remove(pNode);
	updateNode(pNext);

	// Dirty up...
	++m_iDirtyCount;

	m_pList->notify();
}


// Snap to minimum distance frame.
unsigned long qtractorCurve::frameDist ( unsigned long iFrame ) const
{
	if (m_iMinFrameDist > 0) {
		const unsigned long q = m_iMinFrameDist;
		iFrame = q * ((iFrame + (q >> 1)) / q);
	}

	return iFrame;
}


// Update node and reset all cursors.
void qtractorCurve::resetNode ( qtractorCurve::Node *pNode )
{
	Cursor *pCursor = m_cursors.first();
	while (pCursor) {
		pCursor->reset(pNode);
		pCursor = pCursor->next();
	}
}


// Node interpolation coefficients updater.
void qtractorCurve::updateNode ( qtractorCurve::Node *pNode )
{
	updateNodeEx(pNode);
	if (pNode)
		updateNodeEx(pNode->next());
}


void qtractorCurve::updateNodeEx ( qtractorCurve::Node *pNode )
{
	Node *pPrev, *pNext = NULL;
	if (pNode) {
		pPrev = pNode->prev();
		pNext = pNode->next();
		if (pNext == NULL)
			pNext = &m_tail;
	} else {
		pNode = &m_tail;
		pPrev = m_nodes.last();
	}

	if (m_tail.frame < pNode->frame)
		m_tail.frame = pNode->frame;

	switch (m_mode) {
	case Hold:
		updateNodeHold(pNode, m_tail.value, pPrev, pNext);
		break;
	case Linear:
		updateNodeLinear(pNode, m_tail.value, pPrev, pNext);
		break;
	case Spline:
		updateNodeSpline(pNode, m_tail.value, pPrev, pNext);
		break;
	}
}


// Refresh all coefficients.
void qtractorCurve::update (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::update()", this);
#endif

	for (Node *pNode = m_nodes.first(); pNode; pNode = pNode->next())
		updateNodeEx(pNode);

	updateNodeEx(NULL);

	m_pList->notify();
}


// Default value accessors.
void qtractorCurve::setDefaultValue ( float fDefaultValue )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::setDefaultValue(%g)", this, fDefaultValue);
#endif

	m_tail.value = fDefaultValue;

	Node *pLast = m_nodes.last();
	updateNode(pLast);

	Node *pFirst = m_nodes.first();
	if (pFirst != pLast)
		updateNode(pFirst);

	m_pList->notify();
}


// Default length accessors.
void qtractorCurve::setLength ( unsigned long iLength )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::setLength(%ld)", this, iLength);
#endif

	m_tail.frame = iLength;

	Node *pNode = m_nodes.last();
	while (pNode && pNode->frame > m_tail.frame) {
		Node *pPrev = pNode->prev();
		m_nodes.remove(pNode);
		pNode = pPrev;
		++m_iDirtyCount;
	}

	updateNode(pNode);

	m_pList->notify();
}


// Intra-curve frame positioning value seeker.
qtractorCurve::Node *qtractorCurve::Cursor::seek ( unsigned long iFrame )
{
	if (iFrame < m_iFrame) {
		// Seek backward...
		if (m_pNode == NULL)
			m_pNode = m_pCurve->nodes().last();
		while (m_pNode && m_pNode->prev() && (m_pNode->prev())->frame > iFrame)
			m_pNode = m_pNode->prev();
	} else {
		// Seek forward...
		if (m_pNode == NULL)
			m_pNode = m_pCurve->nodes().first();
		while (m_pNode && m_pNode->frame < iFrame)
			m_pNode = m_pNode->next();
	}

	m_iFrame = iFrame;

	return (m_pNode && m_pNode->frame >= m_iFrame ? m_pNode : NULL);
}


// Intra-curve frame positioning reset.
void qtractorCurve::Cursor::reset ( qtractorCurve::Node *pNode )
{
	if (m_pNode == pNode || pNode == NULL) {
		m_pNode  = (pNode ? pNode->next() : NULL);
		m_iFrame = (m_pNode ? m_pNode->frame : 0);
	}
}


// Common interpolate method.
float qtractorCurve::value ( const Node *pNode, unsigned long iFrame ) const
{
	if (pNode == NULL) {
		pNode = m_nodes.last();
		if (pNode == NULL)
			pNode = &m_tail;
	}

	float x = float(pNode->frame) - float(iFrame);
	float y = pNode->value;

	if (x > 0.0f) {
		switch (m_mode) {
		case Hold:
			y = valueHold(pNode, x);
			break;
		case Linear:
			y = valueLinear(pNode, x);
			break;
		case Spline:
			y = valueSpline(pNode, x);
			break;
		}
	}

	return y;
}


float qtractorCurve::value ( unsigned long iFrame )
{
	return value(m_cursor.seek(iFrame), iFrame);
}


// Convert MIDI sequence events to curve nodes.
void qtractorCurve::readMidiSequence ( qtractorMidiSequence *pSeq,
	qtractorMidiEvent::EventType ctype, unsigned short iChannel,
	unsigned short iParam, qtractorTimeScale *pTimeScale )
{
	qtractorTimeScale ts;
	if (pTimeScale)
		ts.copy(*pTimeScale);
	ts.setTicksPerBeat(pSeq->ticksPerBeat());

	// Cleanup all existing nodes...
	clear();

	// Convert events to nodes...
	const float fMinValue = m_observer.minValue();
	const float fMaxValue = m_observer.maxValue();

	if (pSeq->channel() == iChannel) {
		qtractorMidiEvent *pEvent = pSeq->events().first();
		while (pEvent) {
			if (pEvent->type() == ctype && pEvent->note() == iParam) {
				unsigned long iFrame = ts.frameFromTick(pEvent->time());
				float fScale;
				if (ctype == qtractorMidiEvent::PITCHBEND)
					fScale = float(pEvent->pitchBend()) / float(0x3fff);
				else
					fScale = float(pEvent->value()) / float(0x7f);
				float fValue = fMinValue + fScale * (fMaxValue - fMinValue);
				insertNode(iFrame, fValue, false);
			}
			pEvent = pEvent->next();
		}
	}

	update();

	// Not dirty by now...
	m_iDirtyCount = 0;
}


// Convert curve node to MIDI sequence events.
void qtractorCurve::writeMidiSequence ( qtractorMidiSequence *pSeq,
	qtractorMidiEvent::EventType ctype, unsigned short iChannel,
	unsigned short iParam, qtractorTimeScale *pTimeScale ) const
{
	qtractorTimeScale ts;
	if (pTimeScale)
		ts.copy(*pTimeScale);
	ts.setTicksPerBeat(pSeq->ticksPerBeat());

	// Clenup all existing events...
	if (pSeq->channel() == iChannel) {
		qtractorMidiEvent *pEvent = pSeq->events().first();
		while (pEvent) {
			qtractorMidiEvent *pEventNext = pEvent->next();
			if (pEvent->type() == ctype && pEvent->note() == iParam) {
				pSeq->removeEvent(pEvent);
			}
			pEvent = pEventNext;
		}
	}

	// Convert nodes to events...
	const float fMinValue = m_observer.minValue();
	const float fMaxValue = m_observer.maxValue();

	qtractorCurve::Node *pNode = m_nodes.first();
	while (pNode) {
		unsigned long iTime = ts.tickFromFrame(pNode->frame);
		qtractorMidiEvent *pEvent = new qtractorMidiEvent(iTime, ctype, iParam);
		float fScale = (pNode->value - fMinValue) / (fMaxValue - fMinValue);
		if (ctype == qtractorMidiEvent::PITCHBEND)
			pEvent->setPitchBend(float(0x3fff) * fScale);
		else
			pEvent->setValue(float(0x7f) * fScale);
		pSeq->insertEvent(pEvent);
		pNode = pNode->next();
	}

	pSeq->close();
}


// Enabled/capture/process state setlers.
void qtractorCurve::setEnabled ( bool bEnabled )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::setEnabled(%d)", this, int(bEnabled));
#endif

	m_state = State(bEnabled ? (m_state | Enabled) : (m_state & ~Enabled));
	m_pList->updateEnabled(m_state & Enabled);
}


void qtractorCurve::setCapture ( bool bCapture )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::setCapture(%d)", this, int(bCapture));
#endif

	m_state = State(bCapture ? (m_state | Capture) : (m_state & ~Capture));
	m_pList->updateCapture(m_state & Capture);
}


void qtractorCurve::setProcess ( bool bProcess )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorCurve[%p]::setProcess(%d)", this, int(bProcess));
#endif

	m_state = State(bProcess ? (m_state | Process) : (m_state & ~Process));
	m_pList->updateProcess(m_state & Process);
}


// end of qtractorCurve.cpp
