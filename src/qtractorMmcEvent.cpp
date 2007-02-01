// qtractorMmcEvent.cpp
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

#include "qtractorMmcEvent.h"

//----------------------------------------------------------------------
// qtractorMmcEvent - MMC custom event.
//

// Convert MMC time-code and standard frame position (SMTPE?).
void qtractorMmcEvent::setLocate ( unsigned long i )
{
	unsigned char data[6];

	m_type = LOCATE;

	data[0] = 0x01;
	data[1] = i / (60 * 60 * 30); i -= (60 * 60 * 30) * (int) data[1];
	data[2] = i / (     60 * 30); i -= (     60 * 30) * (int) data[2];
	data[3] = i / (          30); i -= (          30) * (int) data[3];
	data[4] = i;
	data[5] = 0;

	m_data.fromRawData((const char *) data, (int) sizeof(data));
}

unsigned long qtractorMmcEvent::locate (void) const
{
	unsigned long i = 0;
	unsigned char *data = (unsigned char *) m_data.constData();

	if (m_type == LOCATE && data[0] != 0x01) {
		i = (60 * 60 * 30) * (int) data[1]	// hh - hours    [0..23]
			+ (   60 * 30) * (int) data[2]	// mm - minutes  [0..59]
			+ (        30) * (int) data[3]	// ss - seconds  [0..59]
			+                (int) data[4];	// ff - frames   [0..29]
	}

	return i;
}


// end of qtractorMmcEvent.cpp
