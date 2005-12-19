// qtractorMidiEvent.h
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

#ifndef __qtractorMidiEvent_h
#define __qtractorMidiEvent_h

#include "qtractorList.h"

#include <stdio.h>
#include <string.h>


//----------------------------------------------------------------------
// class qtractorMidiEvent -- The generic MIDI event element.
//

class qtractorMidiEvent : public qtractorList<qtractorMidiEvent>::Link
{
public:

	// Typical event types.
	enum EventType {
		NOTEOFF     = 0x80,
		NOTEON      = 0x90,
		KEYPRESS    = 0xa0,
		CONTROLLER  = 0xb0,
		PGMCHANGE   = 0xc0,
		CHANPRESS   = 0xd0,
		PITCHBEND   = 0xe0,
		SYSEX       = 0xf0,
		META        = 0xff
	};

	// Meta event types.
	enum MetaType {
		SEQUENCE    = 0x00,
		TEXT        = 0x01,
		COPYRIGHT   = 0x02,
		TRACKNAME   = 0x03,
		INSTRUMENT  = 0x04,
		LYRIC       = 0x05,
		MARKER      = 0x06,
		CUE         = 0x07,
		CHANNEL     = 0x20,
		PORT        = 0x21,
		EOT         = 0x2f,
		TEMPO       = 0x51,
		SMPTE       = 0x54,
		TIME        = 0x58,
		KEY         = 0x59,
		PROPRIETARY = 0x7f
	};

	// Constructor.
	qtractorMidiEvent(unsigned long time, EventType type,
		unsigned char data1 = 0, unsigned char data2 = 0,
		unsigned long duration = 0)
		: m_time(time), m_type(type), m_duration(duration), m_pSysex(NULL)
		{ m_data[0] = data1; m_data[1] = data2; }

	// Destructor.
	~qtractorMidiEvent() { if (m_pSysex) delete [] m_pSysex; }

	// Event properties accessors.
	unsigned long time()       const { return m_time; }
	EventType     type()       const { return m_type; }
	// Underloaded accessors.
	unsigned char note()       const { return m_data[0]; }
	unsigned char velocity()   const { return m_data[1]; }
	unsigned char controller() const { return m_data[0]; }
	unsigned char value()      const { return m_data[1]; }

	// Other special accessor.
	void setDuration(unsigned long duration) { m_duration = duration; }
	unsigned long duration()   const { return m_duration; }

	// Sysex data accessors.
	unsigned char *sysex()     const { return m_pSysex; }
	unsigned short sysex_len() const { return *(unsigned short *)(&m_data[0]); }

	// Allocate and set a new sysex buffer.
	void setSysex(unsigned char *pSysex, unsigned short iSysex)
	{
		if (m_pSysex) delete [] m_pSysex;
		*(unsigned short *)(&m_data[0]) = iSysex;
		m_pSysex = new unsigned char [iSysex];
		::memcpy(m_pSysex, pSysex, iSysex);
	}

private:

	// Event instance members.
	unsigned long  m_time;
	EventType      m_type;
	unsigned char  m_data[2];
	unsigned long  m_duration;
	// Sysex data (m_data has the length).
	unsigned char *m_pSysex;
};


#endif  // __qtractorMidiEvent_h


// end of qtractorMidiEvent.h
