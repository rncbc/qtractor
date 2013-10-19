// qtractorMidiEvent.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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
		META        = 0xff,
		// Extra-ordinary event types...
		REGPARAM    = 0x10,
		NONREGPARAM = 0x20,
		CONTROL14   = 0x30
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
		unsigned short param = 0, unsigned short value = 0,
		unsigned long duration = 0)
		: m_time(time), m_type(type)
		{ m_v.param = param; m_v.value = value; m_u.duration = duration; }

	// Copy constructor.
	qtractorMidiEvent(const qtractorMidiEvent& e)
		: m_time(e.m_time), m_type(e.m_type)
	{
		if (m_type == SYSEX) {
			m_v.iSysex = e.m_v.iSysex;
			m_u.pSysex = new unsigned char [m_v.iSysex];
			::memcpy(m_u.pSysex, e.m_u.pSysex, m_v.iSysex);
		} else {
			m_v.param = e.m_v.param;
			m_v.value = e.m_v.value;
			m_u.duration = e.m_u.duration;
		}
	}

	// Destructor.
	~qtractorMidiEvent()
		{ if (m_type == SYSEX && m_u.pSysex) delete [] m_u.pSysex; }

	// Event properties accessors (getters).
	unsigned long time()       const { return m_time; }
	EventType     type()       const { return m_type; }

	// Underloaded accessors (getters).
	unsigned char note()       const { return m_v.param; }
	unsigned char velocity()   const { return m_v.value; }
	unsigned char controller() const { return m_v.param; }

	unsigned short param()     const { return m_v.param; }
	unsigned short value()     const { return m_v.value; }

	unsigned long  duration()  const { return m_u.duration; }

	// Event properties accessors (setters).
	void setTime(unsigned long time) { m_time = time; }
	void setType(EventType type)     { m_type = type; }

	// Special event time offset adjust.
	void adjustTime(unsigned long iOffset)
		{ m_time = (m_time > iOffset ? m_time - iOffset : 0); }

	// Underloaded accessors (setters).
	void setNote(unsigned char note)             { m_v.param = note; }
	void setVelocity(unsigned char velocity)     { m_v.value = velocity; }
	void setController(unsigned char controller) { m_v.param = controller; }

	void setParam(unsigned short param)          { m_v.param = param; }
	void setValue(unsigned short value)          { m_v.value = value; }

	// Duration accessors (NOTEON).
	void setDuration(unsigned long duration)     { m_u.duration = duration; }

	// Sysex data accessors (SYSEX).
	unsigned char *sysex()     const { return m_u.pSysex; }
	unsigned short sysex_len() const { return m_v.iSysex; }

	// Allocate and set a new sysex buffer.
	void setSysex(unsigned char *pSysex, unsigned short iSysex)
	{
		if (m_type == SYSEX && m_u.pSysex) delete [] m_u.pSysex;
		m_v.iSysex = iSysex;
		m_u.pSysex = new unsigned char [m_v.iSysex];
		::memcpy(m_u.pSysex, pSysex, m_v.iSysex);
	}

	// Special accessors for pitch-bend event types.
	int pitchBend() const
		{ return int(m_v.value) - 0x2000; }
	void setPitchBend(int iPitchBend)
		{ m_v.value = (unsigned short) (0x2000 + iPitchBend); }

private:

	// Event instance members.
	unsigned long  m_time;
	EventType      m_type;

	// Nominal event data.
	union {
		struct {					// type != SYSEX
			unsigned short param;
			unsigned short value;
		};
		unsigned short iSysex;		// type == SYSEX
	} m_v;

	// Extra event data.
	union {
		unsigned long  duration;	// type == NOTEON
		unsigned char *pSysex;		// type == SYSEX
	} m_u;
};


#endif  // __qtractorMidiEvent_h


// end of qtractorMidiEvent.h
