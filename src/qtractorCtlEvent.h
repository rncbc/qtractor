// qtractorCtlEvent.h
//
/****************************************************************************
   Copyright (C) 2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorCtlEvent_h
#define __qtractorCtlEvent_h

#include "qtractorMidiEvent.h"


//----------------------------------------------------------------------
// qtractorCtlEvent - MIDI Control custom event.
//

class qtractorCtlEvent
{
public:

	// Contructor.
	qtractorCtlEvent(qtractorMidiEvent::EventType ctype
		= qtractorMidiEvent::CONTROLLER, unsigned short iChannel = 0,
		unsigned short iParam = 0, unsigned short iValue = 0)
		: m_ctype(ctype), m_channel(iChannel),
			m_param(iParam), m_value(iValue) {}

	// Copy constructor.
	qtractorCtlEvent(const qtractorCtlEvent& ctle)
		: m_ctype(ctle.m_ctype), m_channel(ctle.m_channel),
			m_param(ctle.m_param), m_value(ctle.m_value) {}


	// Accessors.
	qtractorMidiEvent::EventType type() const { return m_ctype; }
	unsigned short channel() const { return m_channel; }
	unsigned short param() const { return m_param; }
	unsigned short value() const { return m_value; }

private:

	// Instance variables.
	qtractorMidiEvent::EventType m_ctype;
	unsigned short m_channel;
	unsigned short m_param;
	unsigned short m_value;
};


#endif  // __qtractorCtlEvent_h

// end of qtractorCtlEvent.h
