// qtractorSessionCommand.cpp
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

#include "qtractorAbout.h"
#include "qtractorSessionCommand.h"

#include "qtractorMidiEngine.h"


//----------------------------------------------------------------------
// class qtractorSessionCommand - implementation
//

// Constructor.
qtractorSessionCommand::qtractorSessionCommand ( const QString& sName,
	qtractorSession *pSession ) : qtractorCommand(sName), m_pSession(pSession)
{
}


// Destructor.
qtractorSessionCommand::~qtractorSessionCommand (void)
{
}


//----------------------------------------------------------------------
// class qtractorSessionTempoCommand - implementation
//

// Constructor.
qtractorSessionTempoCommand::qtractorSessionTempoCommand (
	qtractorSession *pSession, float fTempo )
	: qtractorSessionCommand(QObject::tr("session tempo"), pSession)
{
	m_fTempo = fTempo;
}


// Session-tempo command methods.
bool qtractorSessionTempoCommand::redo (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Save the previous session tempo alright...
	float fTempo = pSession->tempo();

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

	// Just set new one...
	pSession->setTempo(m_fTempo);

	// Restore playback state, if needed...
	if (bPlaying) {
		// The MIDI engine queue needs a reset...
		if (pSession->midiEngine())
			pSession->midiEngine()->resetTempo();
		pSession->unlock();
	}

	// Swap it nice, finally.
	m_fTempo = fTempo;

	return true;
}

bool qtractorSessionTempoCommand::undo (void)
{
	// As we swap the prev/tempo this is non-idempotent.
	return redo();
}


// end of qtractorSessionCommand.cpp
