// qtractorMmcEvent.h
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

#ifndef __qtractorMmcEvent_h
#define __qtractorMmcEvent_h

#include <QByteArray>


//----------------------------------------------------------------------
// qtractorMmcEvent - MMC custom event.
//

class qtractorMmcEvent
{
public:

	// MMC command codes.
	enum Command {
		STOP                    = 0x01,
		PLAY                    = 0x02,
		DEFERRED_PLAY           = 0x03,
		FAST_FORWARD            = 0x04,
		REWIND                  = 0x05,
		RECORD_STROBE           = 0x06,
		RECORD_EXIT             = 0x07,
		RECORD_PAUSE            = 0x08,
		PAUSE                   = 0x09,
		EJECT                   = 0x0a,
		CHASE                   = 0x0b,
		COMMAND_ERROR_RESET     = 0x0c,
		MMC_RESET               = 0x0d,
		JOG_START               = 0x20,
		JOG_STOP                = 0x21,
		WRITE                   = 0x40,
		MASKED_WRITE            = 0x41,
		READ                    = 0x42,
		UPDATE                  = 0x43,
		LOCATE                  = 0x44,
		VARIABLE_PLAY           = 0x45,
		SEARCH                  = 0x46,
		SHUTTLE                 = 0x47,
		STEP                    = 0x48,
		ASSIGN_SYSTEM_MASTER    = 0x49,
		GENERATOR_COMMAND       = 0x4a,
		MTC_COMMAND             = 0x4b,
		MOVE                    = 0x4c,
		ADD                     = 0x4d,
		SUBTRACT                = 0x4e,
		DROP_FRAME_ADJUST       = 0x4f,
		PROCEDURE               = 0x50,
		EVENT                   = 0x51,
		GROUP                   = 0x52,
		COMMAND_SEGMENT         = 0x53,
		DEFERRED_VARIABLE_PLAY  = 0x54,
		RECORD_STROBE_VARIABLE  = 0x55,
		WAIT                    = 0x7c,
		RESUME                  = 0x7f
	};

	// MMC sub-command codes (as for MASKED_WRITE).
	enum SubCommand {
		TRACK_NONE              = 0x00,
		TRACK_RECORD            = 0x4f,
		TRACK_MUTE              = 0x62,
		TRACK_SOLO              = 0x66 // Custom-implementation ;)
	};

	// Default contructor (fake).
	qtractorMmcEvent() : m_cmd(Command(0)) {}
		
	// Contructor.
	qtractorMmcEvent(unsigned char *pSysex)
		: m_cmd(Command(pSysex[4])),
			m_data((const char *) &pSysex[6], (int) pSysex[5]) {}

	// Copy contructor.
	qtractorMmcEvent(const qtractorMmcEvent& mmce)
		: m_cmd(mmce.m_cmd), m_data(mmce.m_data) {}
	
	// Accessors.
	Command        cmd()  const { return m_cmd; }
	unsigned char *data() const { return (unsigned char *) m_data.constData(); }
	unsigned short len()  const { return (unsigned short)  m_data.length();    }

	// Retrieve MMC time-code and standard frame position (SMPTE?).
	unsigned long locate() const;

	// Retrieve MMC shuttle-speed and direction.
	float shuttle() const;

	// Retrieve MMC step and direction.
	int step() const;

	// Retrieve MMC masked-write sub-command data.
	SubCommand scmd() const;
	int track() const;
	bool isOn() const;

private:

	// Instance variables.
	Command    m_cmd;
	QByteArray m_data;
};


#endif  // __qtractorMmcEvent_h


// end of qtractorMmcEvent.h
