// qtractorMidiCursor.cpp
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

#include "qtractorMidiCursor.h"
#include "qtractorMidiSequence.h"


//-------------------------------------------------------------------------
// qtractorMidiCursor -- MIDI event cursor capsule.

// Constructor.
qtractorMidiCursor::qtractorMidiCursor (void)
	: m_pEvent(NULL), m_iTime(0)
{
}


// Intra-sequence tick/time positioning seek.
qtractorMidiEvent *qtractorMidiCursor::seek (
	qtractorMidiSequence *pSeq, unsigned long iTime )
{
	// Plain reset...
	if (iTime == 0) {
		m_pEvent = pSeq->events().first();
	}
	else
	if (iTime > m_iTime) {
		// Seek forward...
		if (m_pEvent == NULL)
			m_pEvent = pSeq->events().first();
		while (m_pEvent && m_pEvent->next()
			&& (m_pEvent->next())->time() < iTime)
			m_pEvent = m_pEvent->next();
		if (m_pEvent == NULL)
			m_pEvent = pSeq->events().last();
	}
	else
	if (iTime < m_iTime) {
		// Seek backward...
		if (m_pEvent == NULL)
			m_pEvent = pSeq->events().last();
		while (m_pEvent && m_pEvent->time() >= iTime)
			m_pEvent = m_pEvent->prev();
		if (m_pEvent == NULL)
			m_pEvent = pSeq->events().first();
	}
	// Done.
	m_iTime = iTime;
	return m_pEvent;
}


// Intra-seuqnce tick/time positioning reset (seek forward).
qtractorMidiEvent *qtractorMidiCursor::reset (
	qtractorMidiSequence *pSeq, unsigned long iTime )
{
	// Reset-seek forward...
	if (m_iTime >= iTime)
		m_pEvent = NULL;
	if (m_pEvent == NULL)
		m_pEvent = pSeq->events().first();
	while (m_pEvent && m_pEvent->time() + m_pEvent->duration() < iTime)
		m_pEvent = m_pEvent->next();
	while (m_pEvent && m_pEvent->time() > iTime)
		m_pEvent = m_pEvent->prev();
	if (m_pEvent == NULL)
		m_pEvent = pSeq->events().first();
	// That was it...
	m_iTime = iTime;
	return m_pEvent;
}


// end of qtractorMidiCursor.cpp

