// qtractorSessionCommand.cpp
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
#include "qtractorClipCommand.h"

#include "qtractorMidiEngine.h"
#include "qtractorAudioClip.h"


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
	qtractorSession *pSession, float fTempo, unsigned short iTicksPerBeat )
	: qtractorSessionCommand(QObject::tr("session tempo"), pSession),
		m_fTempo(0.0f), m_iTicksPerBeat(0), m_pClipCommand(NULL)
{
	// Tempo changes...
	if (fTempo > 0.0f && fTempo != pSession->tempo())
		m_fTempo = fTempo;

	// Time resolution changes...
	if (iTicksPerBeat > 0 && iTicksPerBeat != pSession->ticksPerBeat())
		m_iTicksPerBeat = iTicksPerBeat;

	// Take care of time-stretching of all audio-clips...
	if (m_fTempo > 0.0f) {
		for (qtractorTrack *pTrack = pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			if (pTrack->trackType() == qtractorTrack::Audio) {
				for (qtractorClip *pClip = pTrack->clips().first();
						pClip; pClip = pClip->next()) {
					qtractorAudioClip *pAudioClip
						= static_cast<qtractorAudioClip *> (pClip);
					if (pAudioClip) {
						if (m_pClipCommand == NULL)
							m_pClipCommand = new qtractorClipCommand(name());
						float fTimeStretch
							= (m_fTempo * pAudioClip->timeStretch())
								/ pSession->tempo();
						m_pClipCommand->timeStretchClip(pClip, fTimeStretch);
					}
				}
			}
		}
	}
}

// Desstructor.
qtractorSessionTempoCommand::~qtractorSessionTempoCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;
}


// Session-tempo command methods.
bool qtractorSessionTempoCommand::redo (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

	// Tempo changes...
	float fTempo = 0.0f;
	if (m_fTempo > 0.0f) {
		fTempo = pSession->tempo();
		pSession->setTempo(m_fTempo);
	}

	// Time resolution changes...
	unsigned short iTicksPerBeat = 0;
	if (m_iTicksPerBeat > 0) {
		iTicksPerBeat = pSession->ticksPerBeat();
		pSession->setTicksPerBeat(m_iTicksPerBeat);
		pSession->updateTimeResolution();
	}

	// In case we have audio clips around...
	if (m_pClipCommand)
		m_pClipCommand->redo();

	// Restore playback state, if needed...
	if (bPlaying) {
		// The MIDI engine queue needs a reset...
		if (pSession->midiEngine())
			pSession->midiEngine()->resetTempo();
		pSession->unlock();
	}

	// Swap it nice, finally.
	if (fTempo > 0.0f)
		m_fTempo = fTempo;
	if (iTicksPerBeat > 0)
		m_iTicksPerBeat = iTicksPerBeat;

	return true;
}

bool qtractorSessionTempoCommand::undo (void)
{
	// As we swap the prev/tempo this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorSessionLoopCommand - implementation
//

// Constructor.
qtractorSessionLoopCommand::qtractorSessionLoopCommand (
	qtractorSession *pSession, unsigned long iLoopStart, unsigned long iLoopEnd )
	: qtractorSessionCommand(QObject::tr("session loop"), pSession)
{
	m_iLoopStart = iLoopStart;
	m_iLoopEnd   = iLoopEnd;
}


// Session-loop command methods.
bool qtractorSessionLoopCommand::redo (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Save the previous session loop state alright...
	unsigned long iLoopStart = pSession->loopStart();
	unsigned long iLoopEnd   = pSession->loopEnd();

	// Just set new bounds...
	pSession->setLoop(m_iLoopStart, m_iLoopEnd);

	// Restore edit cursors too...
	if (m_iLoopStart < m_iLoopEnd) {
		pSession->setEditHead(m_iLoopStart);
		pSession->setEditTail(m_iLoopEnd);
	}

	// Swap it nice, finally.
	m_iLoopStart = iLoopStart;
	m_iLoopEnd   = iLoopEnd;

	return true;
}

bool qtractorSessionLoopCommand::undo (void)
{
	// As we swap the prev/tempo this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorSessionEditCommand - implementation
//

// Constructor.
qtractorSessionEditCommand::qtractorSessionEditCommand (
	qtractorSession *pSession, const qtractorSession::Properties& properties )
	: qtractorPropertyCommand<qtractorSession::Properties> (
		QObject::tr("session properties"), pSession->properties(), properties),
			m_pTempoCommand(NULL)
{
	// Append tempo/resolution changes too...
	float fTempo = properties.timeScale.tempo();
	unsigned short iTicksPerBeat = properties.timeScale.ticksPerBeat();
	if (pSession->tempo() != fTempo || pSession->ticksPerBeat() != iTicksPerBeat)
		m_pTempoCommand = new qtractorSessionTempoCommand(pSession, fTempo, iTicksPerBeat);

}

// Destructor.
qtractorSessionEditCommand::~qtractorSessionEditCommand (void)
{
	if (m_pTempoCommand)
		delete m_pTempoCommand;
}


// Session-edit command methods.
bool qtractorSessionEditCommand::redo (void)
{
	bool bResult = true;

	if (m_pTempoCommand)
		bResult = m_pTempoCommand->redo();

	if (bResult)
		bResult = qtractorPropertyCommand<qtractorSession::Properties>::redo();

	return bResult;
}

bool qtractorSessionEditCommand::undo (void)
{
	bool bResult = qtractorPropertyCommand<qtractorSession::Properties>::undo();

	if (bResult && m_pTempoCommand)
		bResult = m_pTempoCommand->undo();

	return bResult;
}


// end of qtractorSessionCommand.cpp
