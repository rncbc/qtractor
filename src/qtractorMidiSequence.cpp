// qtractorMidiSequence.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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
	m_iBank    = -1;
	m_iProgram = -1;

	m_noteMax  = 0;
	m_noteMin  = 0;

	m_duration = 0;

	m_events.clear();
	m_notes.clear();
}


// Add event to a channel sequence; preserve vent time sort order.
void qtractorMidiSequence::addEvent ( qtractorMidiEvent *pEvent )
{
	// NOTE: Find previous note event and compute duration...
	if (pEvent->type() == qtractorMidiEvent::NOTEOFF ||
		pEvent->type() == qtractorMidiEvent::NOTEON) {
		unsigned char note = pEvent->note();
		NoteMap::Iterator iter = m_notes.find(note);
		if (iter != m_notes.end()) {
			unsigned long iTime = (*iter)->time();
			unsigned long iDuration = pEvent->time() - iTime;
			(*iter)->setDuration(iDuration);
			iDuration += iTime;
			if (m_duration < iDuration)
				m_duration = iDuration;
			m_notes.erase(iter);
		}
		// NOTEON: Keep note stats and make it pending on a NOTEOFF...
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			if (m_noteMin > note || m_noteMin == 0)
				m_noteMin = note;
			if (m_noteMax < note || m_noteMax == 0)
				m_noteMax = note;
			// Add to lingering notes...
			m_notes[note] = pEvent;
		} else {
			// NOTEOFF: Won't own this any longer...
			delete pEvent;
			return;
		}
	}

	// Find the proper position in time sequence...
	qtractorMidiEvent *pEventAfter = m_events.last();
	while (pEventAfter && pEventAfter->time() > pEvent->time())
		pEventAfter = pEventAfter->prev();

	// Add it...
	m_events.insertAfter(pEvent, pEventAfter);
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
	for (NoteMap::Iterator iter = m_notes.begin();
			iter != m_notes.end(); ++iter) {
		(*iter)->setDuration(m_duration - (*iter)->time());
	}

	// Reset all pending notes.
	m_notes.clear();
}


// end of qtractorMidiSequence.cpp
