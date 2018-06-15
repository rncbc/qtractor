// qtractorTempoCurve.cpp
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2018, spog aka Samo Pogačnik. All rights reserved.

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

#include "qtractorSession.h"
#include "qtractorTempoCurve.h"

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
// class qtractorTempoCurve -- The generic curve declaration.
//

// Constructor.
qtractorTempoCurve::qtractorTempoCurve ( qtractorTimeScale *pTimeScale, qtractorSubject *pSubject )
		: /*m_pTimeScale(pTimeScale), */m_observer(pSubject, this), m_state(Idle)/*, m_cursor(this)*/,
		m_color(Qt::darkRed), m_pEditList(NULL)
{

	if (pTimeScale)
		m_pTimeScale = new qtractorTimeScale(*pTimeScale);
	else
		m_pTimeScale = new qtractorTimeScale();

	m_pEditList = new qtractorTempoCurveEditList(this);

	m_observer.setTempoCurve(this);

}

// Destructor.
qtractorTempoCurve::~qtractorTempoCurve (void)
{

	m_observer.setCurve(NULL);

}


// Common interpolate method.
float qtractorTempoCurve::value ( const qtractorTimeScale::Node *pNode, unsigned long iFrame ) const
{
	if (pNode == NULL) {
		pNode = m_pTimeScale->nodes().last();
		if (pNode == NULL)
			return 0;
	}

	const float x = float(pNode->frame) - float(iFrame);

	float y = pNode->currTempo();
	if (x > 0.0f) {
		y = pNode->prevTempo();
	}

	return y;
}


float qtractorTempoCurve::value ( unsigned long iFrame )
{
	return value(m_pTimeScale->cursor().seekFrame(iFrame), iFrame);
}


// Normalized scale converters.
float qtractorTempoCurve::valueFromScale ( float fScale ) const 
{
	return m_observer.valueFromScale(fScale);
}


float qtractorTempoCurve::scaleFromValue ( float fValue ) const
{
	return m_observer.scaleFromValue(fValue);
}


void qtractorTempoCurve::setProcess ( bool bProcess )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTempoCurve[%p]::setProcess(%d)", this, int(bProcess));
#endif

	const bool bOldProcess = (m_state & Process);
	m_state = State(bProcess ? (m_state | Process) : (m_state & ~Process));
	if ((bProcess && !bOldProcess) || (!bProcess && bOldProcess)) {
		// notify auto-plugin-deactivate
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pSession->autoDeactivatePlugins();
	}
}


//----------------------------------------------------------------------
// qtractorTempoCurveEditList -- Curve node edit list.

// Curve edit list command executive.
bool qtractorTempoCurveEditList::execute ( bool bRedo )
{
	if (m_pTempoCurve == NULL)
		return false;

	QListIterator<Item *> iter(m_items);
	if (!bRedo)
		iter.toBack();
	while (bRedo ? iter.hasNext() : iter.hasPrevious()) {
		Item *pItem = (bRedo ? iter.next() : iter.previous());
		// Execute the command item...
		switch (pItem->command)	{
		case AddNode: {
			pItem->autoDelete = !bRedo;
			break;
		}
		case MoveNode: {
			qtractorTimeScale::Node *pNode = pItem->node;
			pNode->tempo = pItem->tempo;
			pNode->beatsPerBar = pItem->beatsPerBar;
			pNode->beatDivisor = pItem->beatDivisor;
			break;
		}
		case RemoveNode: {
			pItem->autoDelete = bRedo;
			break;
		}
		default:
			break;
		}
	}

	return true;
}


// end of qtractorTempoCurve.cpp
