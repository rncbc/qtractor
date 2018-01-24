// qtractorCurve.cpp
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSession.h"

#include <math.h>


// Ref. P.448. Approximate cube root of an IEEE float
// Hacker's Delight (2nd Edition), by Henry S. Warren
// http://www.hackersdelight.org/hdcodetxt/acbrt.c.txt
//
static inline float cbrtf2 ( float x )
{
#ifdef CONFIG_FLOAT32//_NOP
	// Avoid strict-aliasing optimization (gcc -O2).
	union { float f; int i; } u;
	u.f  = x;
	u.i  = (u.i >> 4) + (u.i >> 2);
	u.i += (u.i >> 4) + 0x2a6497f8;
	return 0.33333333f * (2.0f * u.f + x / (u.f * u.f));
//	return u.f;
#else
	return ::cbrtf(x);
#endif
}

static inline float cubef2 ( float x )
{
	return x * x * x;
}


//----------------------------------------------------------------------
// qtractorCurve::updateNode -- Node coefficients computation helpers.
//

// (Sample &)Hold.
static inline void updateNodeHold ( qtractorCurve::Node *pNode, float y0,
	const qtractorCurve::Node *pPrev, const qtractorCurve::Node */*pNext*/ )
{
	if (pPrev)
		pNode->a = pPrev->value;
	else
		pNode->a = y0;

	pNode->b = pNode->c = pNode->d = 0.0f;
}


// Linear.
static inline void updateNodeLinear ( qtractorCurve::Node *pNode, float y0,
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
static inline void updateNodeSpline ( qtractorCurve::Node *pNode, float y0,
	const qtractorCurve::Node *pPrev, const qtractorCurve::Node *pNext )
{
	// Shamelessly using the same reference source article as Ardour ;)
	// CJC Kuger, "Constrained Cubic Spline Interpolation", August 2002
	// http://www.korf.co.uk/spline.pdf
	const float fZero = 1e-9f;

	const float x0 = float(pNode->frame);

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

	if (::fabsf(y1 - y2) > fZero)
		s1 =  x1 / (y1 - y2);

	if (pPrev) {
		pPrev = pPrev->prev();
		if (pPrev && (::fabsf(y1 - pPrev->value) > fZero)) {
			const float s0
				= (x1 - float(pPrev->frame) - x0) / (y1 - pPrev->value);
			if (s1 * s0 > 0.0f && ::fabsf(s1 + s0) > fZero)
				f1 = 2.0f / (s1 + s0);
		}
	}

	if (pNext && (::fabsf(pNext->value - y2) > fZero))
		s2 = (float(pNext->frame) - x0) / (pNext->value - y2);
	if (s2 * s1 > 0.0f && ::fabsf(s2 + s1) > fZero)
		f2 = 2.0f / (s2 + s1);

	const float x12 = x1 * x1;
	const float dy = y2 - y1;

	// Compute second derivative for either side of control point...
	const float ff1
		= (((2.0f * (f2 + (2.0f * f1))) / x1)) + ((6.0f * dy) / x12);
	const float ff2
		= (-2.0f * ((2.0f * f2) + f1) / x1) - ((6.0f * dy) / x12);

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
static inline float valueHold ( const qtractorCurve::Node *pNode, float /*x*/ )
{
	return pNode->a;
}


// Linear.
static inline float valueLinear ( const qtractorCurve::Node *pNode, float x )
{
	return pNode->a * x + pNode->b;
}


// Spline.
static inline float valueSpline ( const qtractorCurve::Node *pNode, float x )
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
		m_bLogarithmic(false), m_color(Qt::darkRed), m_pEditList(NULL)
{
	m_nodes.setAutoDelete(true);

	m_pEditList = new qtractorCurveEditList(this);

	m_tail.frame = 0;
//	m_tail.value = m_observer.defaultValue();

	clear();

	m_observer.setCurve(this);

	if (m_pList)
		m_pList->append(this);
}

// Destructor.
qtractorCurve::~qtractorCurve (void)
{
//	if (m_pList)
//		m_pList->remove(this);

	m_observer.setCurve(NULL);

	clear();

	delete m_pEditList;
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
	m_cursor.reset(NULL);

	updateNodeEx(NULL);
}


// Insert a new node, in frame order.
qtractorCurve::Node *qtractorCurve::addNode (
	unsigned long iFrame, float fValue, qtractorCurveEditList *pEditList )
{
	fValue = m_observer.safeValue(fValue);

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::addNode(%lu, %g, %p)", this,
		iFrame, fValue, pEditList);
#endif

	Node *pNode = NULL;
	Node *pNext = m_cursor.seek(iFrame);
	Node *pPrev = (pNext ? pNext->prev() : m_nodes.last());

	if (pNext && isMinFrameDist(pNext, iFrame, fValue))
		pNode = pNext;
	else
	if (pPrev && isMinFrameDist(pPrev, iFrame, fValue))
		pNode = pPrev;
	else
	if (m_mode != Hold && m_observer.isDecimal()) {
		// Smoothing...
		const float fThreshold = 0.5f; // (m_bLogarithmic ? 0.1f : 0.5f);
		float x0 = (pPrev ? float(pPrev->frame) : 0.0f);
		float x1 = float(iFrame);
		float x2 = (pNext ? float(pNext->frame) : m_tail.frame);
		float y0 = (pPrev ? pPrev->value : m_tail.value);
		float y1 = fValue;
		float y2 = (pNext ? pNext->value : fValue);
		float s1 = (x1 > x0 ? (y1 - y0) / (x1 - x0) : 0.0f);
		float y3 = (x2 > x1 ? s1 * (x2 - x1) + y1 : y1);
		if (::fabsf(y3 - y2) < fThreshold * ::fabsf(y3 - y1))
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
			if (::fabsf(y3 - y2) > fThreshold * ::fabsf(y3 - y1))
				pNode = NULL;
		}
	}

	if (pNode) {
		// Move/update the existing one as average...
		if (pEditList)
			pEditList->moveNode(pNode, pNode->frame, pNode->value);
		pNode->frame = iFrame;
		pNode->value = fValue;
	} else {
		// Create a brand new node,
		// insert it in the right frame...
		pNode = new Node(iFrame, fValue);
		if (pNext)
			m_nodes.insertBefore(pNode, pNext);
		else
			m_nodes.append(pNode);
		if (pEditList)
			pEditList->addNode(pNode);
	}

	updateNode(pNode);
	
	// Dirty up...
	if (m_pList)
		m_pList->notify();

	return pNode;
}


// Insert curve node in correct frame order.
void qtractorCurve::insertNode ( Node *pNode )
{
	if (pNode == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::insertNode(%p)", this, pNode);
#endif

	Node *pNext = m_cursor.seek(pNode->frame);

	if (pNext)
		m_nodes.insertBefore(pNode, pNext);
	else
		m_nodes.append(pNode);

	updateNode(pNode);

	// Dirty up...
	if (m_pList)
		m_pList->notify();
}


// Unlink an existing node from curve.
void qtractorCurve::unlinkNode ( Node *pNode )
{
	if (pNode == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::unlinkNode(%p)", this, pNode);
#endif

	m_cursor.reset(pNode);

	Node *pNext = pNode->next();
	m_nodes.unlink(pNode);
	updateNode(pNext);

	// Dirty up...
	if (m_pList)
		m_pList->notify();
}


// Remove an existing node from curve.
void qtractorCurve::removeNode ( Node *pNode )
{
	if (pNode == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::removeNode(%p)", this, pNode);
#endif

	m_cursor.reset(pNode);

	Node *pNext = pNode->next();
	m_nodes.remove(pNode);
	updateNode(pNext);

	// Dirty up...
	if (m_pList)
		m_pList->notify();
}


// Whether to snap to minimum distance frame.
bool qtractorCurve::isMinFrameDist (
	Node *pNode, unsigned long iFrame, float fValue ) const
{
	const float fThreshold = 0.025f
		* (m_observer.maxValue() - m_observer.minValue());

	const bool bMinValueDist
		= (fValue > pNode->value - fThreshold
		&& fValue < pNode->value + fThreshold);

	const bool bMinFrameDist
		= (iFrame > pNode->frame - m_iMinFrameDist
		&& iFrame < pNode->frame + m_iMinFrameDist);

	if (m_mode == Hold || !m_observer.isDecimal())
		return bMinFrameDist || bMinValueDist;
	else
		return bMinFrameDist && bMinValueDist;
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
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::update()", this);
#endif

	for (Node *pNode = m_nodes.first(); pNode; pNode = pNode->next())
		updateNodeEx(pNode);

	updateNodeEx(NULL);

	if (m_pList)
		m_pList->notify();
}


// Default value accessors.
void qtractorCurve::setDefaultValue ( float fDefaultValue )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::setDefaultValue(%g)", this, fDefaultValue);
#endif

	m_tail.value = fDefaultValue;

	Node *pLast = m_nodes.last();
	updateNode(pLast);

	Node *pFirst = m_nodes.first();
	if (pFirst != pLast)
		updateNode(pFirst);

	if (m_pList)
		m_pList->notify();
}


// Default length accessors.
void qtractorCurve::setLength ( unsigned long iLength )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::setLength(%ld)", this, iLength);
#endif

	m_tail.frame = iLength;

	Node *pNode = m_nodes.last();
	while (pNode && pNode->frame > m_tail.frame) {
		Node *pPrev = pNode->prev();
		m_nodes.remove(pNode);
		pNode = pPrev;
	}

	updateNode(pNode);

	if (m_pList)
		m_pList->notify();
}


// Intra-curve frame positioning node seeker.
qtractorCurve::Node *qtractorCurve::Cursor::seek ( unsigned long iFrame )
{
	Node *pNode = m_pNode;

	if (iFrame > m_iFrame) {
		// Seek forward...
		if (pNode == NULL)
			pNode = m_pCurve->nodes().first();
		while (pNode && pNode->frame < iFrame)
			pNode = pNode->next();
	} else {
		// Seek backward...
		if (pNode == NULL)
			pNode = m_pCurve->nodes().last();
		while (pNode && pNode->prev() && (pNode->prev())->frame > iFrame)
			pNode = pNode->prev();
	}

	m_iFrame = iFrame;
	m_pNode = pNode;

	return (pNode && pNode->frame >= iFrame ? pNode : NULL);
}


// Intra-curve frame positioning reset.
void qtractorCurve::Cursor::reset ( qtractorCurve::Node *pNode )
{
	m_pNode  = (pNode ? pNode->next() : NULL);
	m_iFrame = (m_pNode ? m_pNode->frame : 0);
}


// Common interpolate method.
float qtractorCurve::value ( const Node *pNode, unsigned long iFrame ) const
{
	if (pNode == NULL) {
		pNode = m_nodes.last();
		if (pNode == NULL)
			pNode = &m_tail;
	}

	const float x = float(pNode->frame) - float(iFrame);

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


// Normalized scale converters.
float qtractorCurve::valueFromScale ( float fScale ) const 
{
	if (m_bLogarithmic) {
		const float fMaxValue = m_observer.maxValue();
		const float fMinValue = m_observer.minValue();
		if (fMinValue < 0.0f && fMaxValue > 0.0f) {
			const float fMidScale = fMinValue / (fMinValue - fMaxValue);
			if (fScale > fMidScale) {
				const float fMaxScale = (1.0f - fMidScale);
				fScale = (fScale - fMidScale) / fMaxScale;
				fScale = fMidScale + fMaxScale * ::cubef2(fScale); 
			} else {
				fScale = (fMidScale - fScale) / fMidScale;
				fScale = fMidScale - fMidScale * ::cubef2(fScale); 
			}
		}
		else fScale = ::cubef2(fScale);
	}

	return m_observer.valueFromScale(fScale);
}


float qtractorCurve::scaleFromValue ( float fValue ) const
{
	float fScale = m_observer.scaleFromValue(fValue);

	if (m_bLogarithmic) {
		const float fMaxValue = m_observer.maxValue();
		const float fMinValue = m_observer.minValue();
		if (fMinValue < 0.0f && fMaxValue > 0.0f) {
			const float fMidScale = fMinValue / (fMinValue - fMaxValue);
			if (fScale > fMidScale) {
				const float fMaxScale = (1.0f - fMidScale);
				fScale = (fScale - fMidScale) / fMaxScale;
				fScale = fMidScale + fMaxScale * ::cbrtf2(fScale); 
			} else {
				fScale = (fMidScale - fScale) / fMidScale;
				fScale = fMidScale - fMidScale * ::cbrtf2(fScale); 
			}
		}
		else fScale = ::cbrtf2(fScale);
	}

	return fScale;
}


// Copy all events from another curve (raw-copy).
void qtractorCurve::copyNodes ( qtractorCurve *pCurve )
{
	// Check whether we're the same...
	if (pCurve == this)
		return;

	// Remove existing nodes.
	m_nodes.clear();

	// Clone new ones...
	Node *pNode = pCurve->nodes().first();
	for (; pNode; pNode = pNode->next())
		m_nodes.append(new Node(pNode->frame, pNode->value));

	// Done.
	update();
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
	if (pSeq->channel() == iChannel) {
		qtractorMidiEvent *pEvent = pSeq->events().first();
		while (pEvent) {
			if (pEvent->type() == ctype && pEvent->note() == iParam) {
				const unsigned long iFrame = ts.frameFromTick(pEvent->time());
				float fScale;
				if (ctype == qtractorMidiEvent::PITCHBEND)
					fScale = float(pEvent->pitchBend()) / float(0x3fff);
				else
				if (ctype == qtractorMidiEvent::REGPARAM    ||
					ctype == qtractorMidiEvent::NONREGPARAM ||
					ctype == qtractorMidiEvent::CONTROL14)
					fScale = float(pEvent->value()) / float(0x3fff);
				else
					fScale = float(pEvent->value()) / float(0x7f);
				const float fValue = m_observer.valueFromScale(fScale);
				m_nodes.append(new Node(iFrame, fValue));
			}
			pEvent = pEvent->next();
		}
	}

	update();
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

	// Set proper sequence channel...
	pSeq->setChannel(iChannel);

	// Cleanup existing nodes...
	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		qtractorMidiEvent *pEventNext = pEvent->next();
		if (pEvent->type() == ctype && pEvent->note() == iParam)
			pSeq->removeEvent(pEvent);
		pEvent = pEventNext;
	}

	// Convert nodes to events...
	qtractorCurve::Node *pNode = m_nodes.first();
	while (pNode) {
		const unsigned long iTime = ts.tickFromFrame(pNode->frame);
		qtractorMidiEvent *pEvent = new qtractorMidiEvent(iTime, ctype, iParam);
		const float fScale = m_observer.scaleFromValue(pNode->value);
		if (ctype == qtractorMidiEvent::PITCHBEND)
			pEvent->setPitchBend(float(0x3fff) * fScale);
		else
		if (ctype == qtractorMidiEvent::REGPARAM    ||
			ctype == qtractorMidiEvent::NONREGPARAM ||
			ctype == qtractorMidiEvent::CONTROL14)
			pEvent->setValue(float(0x3fff) * fScale);
		else
			pEvent->setValue(float(0x7f) * fScale);
		pSeq->insertEvent(pEvent);
		pNode = pNode->next();
	}

	pSeq->close();
}


void qtractorCurve::setCapture ( bool bCapture )
{
	if (m_pList == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::setCapture(%d)", this, int(bCapture));
#endif

	const bool bOldCapture = (m_state & Capture);
	m_state = State(bCapture ? (m_state | Capture) : (m_state & ~Capture));
	if ((bCapture && !bOldCapture) || (!bCapture && bOldCapture)) {
		m_pList->updateCapture(bCapture);
		// notify auto-plugin-deactivate
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pSession->autoDeactivatePlugins();
	}
}


void qtractorCurve::setProcess ( bool bProcess )
{
	if (m_pList == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::setProcess(%d)", this, int(bProcess));
#endif

	const bool bOldProcess = (m_state & Process);
	m_state = State(bProcess ? (m_state | Process) : (m_state & ~Process));
	if ((bProcess && !bOldProcess) || (!bProcess && bOldProcess)) {
		m_pList->updateProcess(bProcess);
		// notify auto-plugin-deactivate
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pSession->autoDeactivatePlugins();
	}
}


void qtractorCurve::setLocked ( bool bLocked )
{
	if (m_pList == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorCurve[%p]::setLocked(%d)", this, int(bLocked));
#endif

	const bool bOldLocked = (m_state & Locked);
	m_state = State(bLocked ? (m_state | Locked) : (m_state & ~Locked));
	if ((bLocked && !bOldLocked) || (!bLocked && bOldLocked))
		m_pList->updateLocked(bLocked);
}


//----------------------------------------------------------------------
// qtractorCurveEditList -- Curve node edit list.

// Curve edit list command executive.
bool qtractorCurveEditList::execute ( bool bRedo )
{
	if (m_pCurve == NULL)
		return false;

	QListIterator<Item *> iter(m_items);
	if (!bRedo)
		iter.toBack();
	while (bRedo ? iter.hasNext() : iter.hasPrevious()) {
		Item *pItem = (bRedo ? iter.next() : iter.previous());
		// Execute the command item...
		switch (pItem->command)	{
		case AddNode: {
			if (bRedo)
				m_pCurve->insertNode(pItem->node);
			else
				m_pCurve->unlinkNode(pItem->node);
			pItem->autoDelete = !bRedo;
			break;
		}
		case MoveNode: {
			qtractorCurve::Node *pNode = pItem->node;
			const unsigned long iFrame = pNode->frame;
			const float fValue = pNode->value;
			pNode->frame = pItem->frame;
			pNode->value = pItem->value;
			pItem->frame = iFrame;
			pItem->value = fValue;
			break;
		}
		case RemoveNode: {
			if (bRedo)
				m_pCurve->unlinkNode(pItem->node);
			else
				m_pCurve->insertNode(pItem->node);
			pItem->autoDelete = bRedo;
			break;
		}
		default:
			break;
		}
	}

	m_pCurve->update();

	return true;
}


// end of qtractorCurve.cpp
