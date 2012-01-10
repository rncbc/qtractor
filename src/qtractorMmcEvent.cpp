// qtractorMmcEvent.cpp
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

#include "qtractorMmcEvent.h"


//----------------------------------------------------------------------
// qtractorMmcEvent - MMC custom event.
//

	// Retrieve MMC time-code and standard frame position (SMPTE?).
unsigned long qtractorMmcEvent::locate (void) const
{
	unsigned long iLocate = 0;
	unsigned char *data = (unsigned char *) m_data.constData();

	if (m_cmd == LOCATE && m_data.length() > 4 && data[0] == 0x01) {
		iLocate = (3600 * 30) * int(data[1] & 0x1f)	// hh - hours    [0..23]
				+ (  60 * 30) * int(data[2])		// mm - minutes  [0..59]
				+ (       30) * int(data[3])		// ss - seconds  [0..59]
				+               int(data[4]);		// ff - frames   [0..29]
	}

	return iLocate;
}


// Retrieve MMC shuttle-speed and direction.
float qtractorMmcEvent::shuttle (void) const
{
	float fShuttle = 0.0f;
	unsigned char *data = (unsigned char *) m_data.constData();

	if (m_cmd == SHUTTLE && m_data.length() > 2) {
		unsigned char sh = data[0];
		unsigned char sm = data[1];
		unsigned char sl = data[2];
		unsigned int  n  = (sh & 0x38);
		unsigned int  p  = ((sh & 0x07) << n) | (sm >> (7 - n));
		unsigned int  q  = ((sm << n) << 7) | sl;
		fShuttle = float(p) + float(q) / (1 << (14 - n));
		if (sh & 0x40)
			fShuttle = -(fShuttle);
	}

	return fShuttle;
}


// Retrieve MMC step and direction.
int qtractorMmcEvent::step (void) const
{
	int iStep = 0;
	unsigned char *data = (unsigned char *) m_data.constData();

	if (m_cmd == STEP && m_data.length() > 0) {
		iStep = (data[0] & 0x3f);
		if (data[0] & 0x40)
			iStep = -(iStep);
	}

	return iStep;
}


// Retrieve MMC masked-write sub-command data.
qtractorMmcEvent::SubCommand qtractorMmcEvent::scmd (void) const
{
	SubCommand scmd = TRACK_NONE;
	unsigned char *data = (unsigned char *) m_data.constData();

	if (m_cmd == MASKED_WRITE && m_data.length() > 3)
		scmd = SubCommand(data[0]);

	return scmd;
}

int qtractorMmcEvent::track (void) const
{
	int iTrack = 0;
	unsigned char *data = (unsigned char *) m_data.constData();

	if (m_cmd == MASKED_WRITE && m_data.length() > 3) {
		iTrack = (data[1] > 0 ? (data[1] * 7) : 0) - 5;
		for (int i = 0; i < 7; ++i) {
			int iMask = (1 << i);
			if (data[2] & iMask)
				break;
			++iTrack;
		}
	}

	return iTrack;
}

bool qtractorMmcEvent::isOn (void) const
{
	bool bOn = false;
	unsigned char *data = (unsigned char *) m_data.constData();

	if (m_cmd == MASKED_WRITE && m_data.length() > 3) {
		for (int i = 0; i < 7; ++i) {
			int iMask = (1 << i);
			if (data[2] & iMask) {
				bOn = (data[3] & iMask);
				break;
			}
		}
	}

	return bOn;
}


// end of qtractorMmcEvent.cpp
