// qtractorMidiSequence.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

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
}


// Add event to a channel sequence; preserve vent time sort order.
void qtractorMidiSequence::addEvent ( qtractorMidiEvent *pEvent )
{
	qtractorMidiEvent *pEventAfter = m_events.last();
	while (pEventAfter && pEventAfter->time() > pEvent->time())
		pEventAfter = pEventAfter->prev();

	m_events.insertAfter(pEvent, pEventAfter);

	switch (pEvent->type()) {
		case qtractorMidiEvent::NOTEON: {
			unsigned char note = pEvent->note();
			if (m_noteMin > note || m_noteMin == 0)
				m_noteMin = note;
			if (m_noteMax < note || m_noteMax == 0)
				m_noteMax = note;
			break;
		}
		case qtractorMidiEvent::CONTROLLER: {
			unsigned char controller = pEvent->controller();
			if (controller == 0x00) {
				m_iBank &= 0x00ff;	// Bank MSB.
				m_iBank |= (int) pEvent->value() << 8;
			} else if (controller == 0x20) {
				m_iBank &= 0xff00;	// Bank LSB.
				m_iBank |= (int) pEvent->value();
			}
			break;
		}
		case qtractorMidiEvent::PGMCHANGE: {
			m_iProgram = (int) pEvent->program();
			break;
		}
		default:
			break;
	}
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


// Update sequence duration helper.
void qtractorMidiSequence::setEventDuration ( qtractorMidiEvent *pEvent,
	unsigned long duration )
{
	// Set event duration.
	pEvent->setDuration(duration);
	// update sequence duration, if applicable.
	duration += pEvent->time();
	if (m_duration < duration)
	    m_duration = duration;
}


// end of qtractorMidiSequence.cpp
