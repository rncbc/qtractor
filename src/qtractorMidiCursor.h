// qtractorMidiCursor.h
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

#ifndef __qtractorMidiCursor_h
#define __qtractorMidiCursor_h


// Forward declarations.
class qtractorMidiEvent;
class qtractorMidiSequence;


//-------------------------------------------------------------------------
// qtractorMidiCursor -- MIDI event cursor capsule.

class qtractorMidiCursor
{
public:

	// Constructor.
	qtractorMidiCursor();

	// Intra-sequence tick/time positioning seek.
	qtractorMidiEvent *seek(
		qtractorMidiSequence *pSeq, unsigned long iTime);

	// Intra-sequence tick/time positioning reset (seek forward).
	qtractorMidiEvent *reset(
		qtractorMidiSequence *pSeq,	unsigned long iTime = 0);

private:

	// Current event cursor variables.
	qtractorMidiEvent *m_pEvent;
	unsigned long      m_iTime;	
};


#endif  // __qtractorMidiCursor_h

// end of qtractorMidiCursor.h
