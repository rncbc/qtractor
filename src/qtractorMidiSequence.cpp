// qtractorMidiSequence.cpp
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

#include "qtractorMidiSequence.h"


//----------------------------------------------------------------------
// class qtractorMidiSequence -- The generic MIDI event sequence buffer.
//

// Constructor.
qtractorMidiSequence::qtractorMidiSequence ( const QString& sName,
	unsigned short iChannel, unsigned short iTicksPerBeat )
{
	m_sName = sName;
	m_iChannel = iChannel;
	m_iTicksPerBeat = iTicksPerBeat;

	m_iTimeOffset = 0;
	m_iTimeLength = 0;

	m_events.setAutoDelete(true);

	m_noteMax = 0;
	m_noteMin = 0;

	clear();
}


// Destructor.
qtractorMidiSequence::~qtractorMidiSequence (void)
{
	clear();
}


// Sequencer reset method.
void qtractorMidiSequence::clear (void)
{
	m_iBankSelMethod = -1;

	m_iBank = -1;
	m_iProg = -1;

//	m_noteMax = 0;
//	m_noteMin = 0;

	m_duration = 0;

	m_events.clear();
	m_notes.clear();
}


// Add event to a channel sequence, in time sort order.
void qtractorMidiSequence::addEvent ( qtractorMidiEvent *pEvent )
{
	// Adjust to sequence offset...
	pEvent->adjustTime(m_iTimeOffset);

	// NOTE: Find previous note event and compute duration...
	if (pEvent->type() == qtractorMidiEvent::NOTEOFF) {
		const unsigned char note = pEvent->note();
		NoteMap::Iterator iter = m_notes.find(note);
		const NoteMap::Iterator& iter_end = m_notes.end();
		NoteMap::Iterator iter_last;
		qtractorMidiEvent *pNoteEvent = NULL;
		for ( ; iter != iter_end && iter.key() == note; ++iter) {
			pNoteEvent = iter.value();
			iter_last = iter;
		}
		if (pNoteEvent) {
			const unsigned long t1 = pNoteEvent->time();	// NOTEON
			const unsigned long t2 = pEvent->time();		// NOTEOFF
			if (t1 > t2) {
				pNoteEvent->setDuration(m_duration - t1);
			} else {
				pNoteEvent->setDuration(t2 - t1);
				if (m_duration < t2)
					m_duration = t2;
			}
			m_notes.erase(iter_last);
		}
		// NOTEOFF: Won't own this any longer...
		delete pEvent;
		return;
	}
	else
	if (pEvent->type() == qtractorMidiEvent::NOTEON) {
		// NOTEON: Just add to lingering notes...
		m_notes.insert(pEvent->note(), pEvent);
	}
	else
	if (pEvent->type() == qtractorMidiEvent::SYSEX) {
		// SYSEX: add enough slack...
		const unsigned long t1 = pEvent->time() + (m_iTicksPerBeat >> 3);
		if (m_duration < t1)
			m_duration = t1;
	}

	// Add it...
	insertEvent(pEvent);
}


// Insert event in correct time sort order.
void qtractorMidiSequence::insertEvent ( qtractorMidiEvent *pEvent )
{
	// Find the proper position in time sequence...
	qtractorMidiEvent *pEventAfter = m_events.last();
	while (pEventAfter && pEventAfter->time() > pEvent->time())
		pEventAfter = pEventAfter->prev();

	// Insert it...
	if (pEventAfter)
		m_events.insertAfter(pEvent, pEventAfter);
	else
		m_events.prepend(pEvent);

	unsigned long iTime = pEvent->time();
	// NOTEON: Keep note stats and make it pending on a NOTEOFF...
	if (pEvent->type() == qtractorMidiEvent::NOTEON) {
		const unsigned char note = pEvent->note();
		if (m_noteMin > note || m_noteMin == 0)
			m_noteMin = note;
		if (m_noteMax < note || m_noteMax == 0)
			m_noteMax = note;
		iTime += pEvent->duration();
	}
	if (m_duration < iTime)
		m_duration = iTime;
}


// Unlink event from a channel sequence.
void qtractorMidiSequence::unlinkEvent ( qtractorMidiEvent *pEvent )
{
	m_events.unlink(pEvent);
}


// Remove event from a channel sequence.
void qtractorMidiSequence::removeEvent ( qtractorMidiEvent *pEvent )
{
	m_events.remove(pEvent);
}


// Sequence closure method.
void qtractorMidiSequence::close (void)
{
	// Commit sequence length...
	if (m_duration < m_iTimeLength)
		m_duration = m_iTimeLength;
	else if (m_iTimeLength == 0)
		m_iTimeLength = m_duration;

	// Finish all pending notes...
	NoteMap::ConstIterator iter = m_notes.constBegin();
	const NoteMap::ConstIterator& iter_end = m_notes.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = *iter;
		pEvent->setDuration(m_duration - pEvent->time());
	}

	// Reset all pending notes.
	m_notes.clear();
}


// Replace events from another sequence in given range.
void qtractorMidiSequence::replaceEvents ( qtractorMidiSequence *pSeq,
	unsigned long iTimeOffset, unsigned long iTimeLength )
{
	// Sanitize range as default...
	if (iTimeOffset < 1 && iTimeLength < 1) {
		iTimeOffset = pSeq->timeOffset();
		iTimeLength = pSeq->timeLength();
	}

	// Set the given replacement range...
	const unsigned short iTicksPerBeat
		= pSeq->ticksPerBeat();
	const unsigned long iTimeStart
		= timeq(iTimeOffset, iTicksPerBeat);
	const unsigned long iTimeEnd
		= timeq(iTimeOffset + iTimeLength, iTicksPerBeat);

	// Remove existing events in the given range...
	qtractorMidiEvent *pEvent = m_events.first();
	while (pEvent) {
		qtractorMidiEvent *pNextEvent = pEvent->next();
		if (pEvent->time() >= iTimeStart &&	pEvent->time() < iTimeEnd)
			removeEvent(pEvent);
		pEvent = pNextEvent;
	}

	// Insert new (cloned and adjusted) ones...
	for (pEvent = pSeq->events().first(); pEvent; pEvent = pEvent->next()) {
		qtractorMidiEvent *pNewEvent = new qtractorMidiEvent(*pEvent);
		pNewEvent->setTime(timeq(iTimeOffset + pEvent->time(), iTicksPerBeat));
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			pNewEvent->setDuration(timeq(pEvent->duration(), iTicksPerBeat));
		}
		insertEvent(pNewEvent);
	}

	// Done.
}


// Copy all events from another sequence (raw-copy).
void qtractorMidiSequence::copyEvents ( qtractorMidiSequence *pSeq )
{
	// Remove existing events.
	m_events.clear();
	
	// Clone new ones...
	qtractorMidiEvent *pEvent = pSeq->events().first();
	for (; pEvent; pEvent = pEvent->next())
		m_events.append(new qtractorMidiEvent(*pEvent));

	// Done.
}


// end of qtractorMidiSequence.cpp
