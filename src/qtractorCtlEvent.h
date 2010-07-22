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

//----------------------------------------------------------------------
// qtractorCtlEvent - MIDI Control custom event.
//

class qtractorCtlEvent
{
public:

	// Contructor.
	qtractorCtlEvent(unsigned short iChannel = 0,
		unsigned char controller = 0, unsigned char value = 0)
		: m_channel(iChannel),
			m_controller(controller), m_value(value) {}

	// Copy constructor.
	qtractorCtlEvent(const qtractorCtlEvent& ctle)
		: m_channel(ctle.m_channel),
			m_controller(ctle.m_controller), m_value(ctle.m_value) {}


	// Accessors.
	unsigned short channel()    const { return m_channel; }
	unsigned char  controller() const { return m_controller; }
	unsigned char  value()      const { return m_value; }

private:

	// Instance variables.
	unsigned short m_channel;
	unsigned char  m_controller;
	unsigned char  m_value;
};


#endif  // __qtractorCtlEvent_h

// end of qtractorCtlEvent.h
