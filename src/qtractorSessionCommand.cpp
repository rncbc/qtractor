// qtractorSessionCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTimeScaleCommand.h"
#include "qtractorClipCommand.h"

#include "qtractorMidiEngine.h"
#include "qtractorAudioEngine.h"


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
	qtractorSession *pSession, float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor,
	unsigned short iTicksPerBeat )
	: qtractorSessionCommand(QObject::tr("session tempo"), pSession),
		m_fTempo(0.0f), m_iBeatType(0), m_iBeatsPerBar(0), m_iBeatDivisor(0),
		m_iTicksPerBeat(0), m_pClipCommand(NULL)
{
	// Tempo changes...
	if (fTempo > 0.0f && fTempo != pSession->tempo())
		m_fTempo = fTempo;
	if (iBeatType > 0 && iBeatType != pSession->beatType())
		m_iBeatType = iBeatType;

	// Time signature changes...
	if (iBeatsPerBar > 0 && iBeatsPerBar != pSession->beatsPerBar())
		m_iBeatsPerBar = iBeatsPerBar;
	if (iBeatDivisor > 0 && iBeatDivisor != pSession->beatDivisor())
		m_iBeatDivisor = iBeatDivisor;

	// Resolution changes...
	if (iTicksPerBeat > 0 && iTicksPerBeat != pSession->ticksPerBeat())
		m_iTicksPerBeat = iTicksPerBeat;

	// Take care of time-stretching of all audio-clips...
	if (m_fTempo > 0.0f) {
		qtractorTimeScale *pTimeScale = pSession->timeScale();
		if (pTimeScale) {
			qtractorTimeScale::Node *pNode
				= pTimeScale->nodes().first();
			m_pClipCommand = qtractorTimeScaleCommand::createClipCommand(
				name(), pNode, m_fTempo, pNode->tempo);
		}
	}
}

// Desstructor.
qtractorSessionTempoCommand::~qtractorSessionTempoCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;
}


// Session-tempo common executive method.
bool qtractorSessionTempoCommand::execute ( bool bRedo )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

	// Maybe we'll need some drastic changes...
	int iUpdateTimeResolution = 0;

	// Tempo changes...
	float fTempo = 0.0f;
	if (m_fTempo > 0.0f) {
		fTempo = pSession->tempo();
		pSession->setTempo(m_fTempo);
	//	++iUpdateTimeResolution;
	}

	// Time signature changes...
	unsigned short iBeatType = 0;
	if (m_iBeatType > 0) {
		iBeatType = pSession->beatType();
		pSession->setBeatType(m_iBeatType);
		++iUpdateTimeResolution;
	}

	unsigned short iBeatsPerBar = 0;
	if (m_iBeatsPerBar > 0) {
		iBeatsPerBar = pSession->beatsPerBar();
		pSession->setBeatsPerBar(m_iBeatsPerBar);
	//	++iUpdateTimeResolution;
	}

	unsigned short iBeatDivisor = 0;
	if (m_iBeatDivisor > 0) {
		iBeatDivisor = pSession->beatDivisor();
		pSession->setBeatDivisor(m_iBeatDivisor);
	//	++iUpdateTimeResolution;
	}

	// Time resolution changes...
	unsigned short iTicksPerBeat = 0;
	if (m_iTicksPerBeat > 0) {
		iTicksPerBeat = pSession->ticksPerBeat();
		pSession->setTicksPerBeat(m_iTicksPerBeat);
		++iUpdateTimeResolution;
	}

	if (iUpdateTimeResolution > 0)
		pSession->updateTimeResolution();

	// In case we have clips around...
	if (m_pClipCommand) {
		if (bRedo)
			m_pClipCommand->redo();
		else
			m_pClipCommand->undo();
	}

	// Restore playback state, if needed...
	if (bPlaying) {
		// The Audio engine too...
		if (pSession->audioEngine())
			pSession->audioEngine()->resetMetro();
		// The MIDI engine queue needs a reset...
		if (pSession->midiEngine())
			pSession->midiEngine()->resetTempo();
		pSession->unlock();
	}

	// Swap it nice, finally.
	if (fTempo > 0.0f)
		m_fTempo = fTempo;
	if (iBeatType > 0)
		m_iBeatType = iBeatType;
	if (iBeatsPerBar > 0)
		m_iBeatsPerBar = iBeatsPerBar;
	if (iBeatDivisor > 0)
		m_iBeatDivisor = iBeatDivisor;
	if (iTicksPerBeat > 0)
		m_iTicksPerBeat = iTicksPerBeat;

	return true;
}

// Session-tempo command methods.
bool qtractorSessionTempoCommand::redo (void)
{
	return execute(true);
}


bool qtractorSessionTempoCommand::undo (void)
{
	return execute(false);
}


//----------------------------------------------------------------------
// class qtractorSessionLoopCommand - implementation
//

// Constructor.
qtractorSessionLoopCommand::qtractorSessionLoopCommand (
	qtractorSession *pSession, unsigned long iLoopStart, unsigned long iLoopEnd )
	: qtractorSessionCommand(QObject::tr("session loop"), pSession),
		m_iLoopStart(iLoopStart), m_iLoopEnd(iLoopEnd), m_pPunchCommand(NULL)
{
	// Cannot loop and punch at the same time...
	if (pSession->isPunching() && iLoopStart < iLoopEnd)
		m_pPunchCommand = new qtractorSessionPunchCommand(pSession, 0, 0);
}

// Destructor.
qtractorSessionLoopCommand::~qtractorSessionLoopCommand (void)
{
	if (m_pPunchCommand) delete m_pPunchCommand;
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

	// Do cross-command, if any.
	if (m_pPunchCommand)
		m_pPunchCommand->redo();

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
	// As we swap the prev/loop this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorSessionPunchCommand - implementation
//

// Constructor.
qtractorSessionPunchCommand::qtractorSessionPunchCommand (
	qtractorSession *pSession, unsigned long iPunchIn, unsigned long iPunchOut )
	: qtractorSessionCommand(QObject::tr("session punch"), pSession),
		m_iPunchIn(iPunchIn), m_iPunchOut(iPunchOut), m_pLoopCommand(NULL)
{
	// Cannot punch and loop at the same time...
	if (pSession->isLooping() && iPunchIn < iPunchOut)
		m_pLoopCommand = new qtractorSessionLoopCommand(pSession, 0, 0);
}

// Destructor.
qtractorSessionPunchCommand::~qtractorSessionPunchCommand (void)
{
	if (m_pLoopCommand) delete m_pLoopCommand;
}


// Session-punch command methods.
bool qtractorSessionPunchCommand::redo (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Save the previous session punch state alright...
	unsigned long iPunchIn  = pSession->punchIn();
	unsigned long iPunchOut = pSession->punchOut();

	// Do cross-command, if any.
	if (m_pLoopCommand)
		m_pLoopCommand->redo();

	// Just set new bounds...
	pSession->setPunch(m_iPunchIn, m_iPunchOut);

	// Restore edit cursors too...
	if (m_iPunchIn < m_iPunchOut) {
		pSession->setEditHead(m_iPunchIn);
		pSession->setEditTail(m_iPunchOut);
	}

	// Swap it nice, finally.
	m_iPunchIn  = iPunchIn;
	m_iPunchOut = iPunchOut;

	return true;
}

bool qtractorSessionPunchCommand::undo (void)
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
	unsigned short iBeatType = properties.timeScale.beatType();
	unsigned short iBeatsPerBar = properties.timeScale.beatsPerBar();
	unsigned short iBeatDivisor = properties.timeScale.beatDivisor();
	unsigned short iTicksPerBeat = properties.timeScale.ticksPerBeat();
	if (pSession->tempo() != fTempo ||
		pSession->beatType() != iBeatType ||
		pSession->beatsPerBar()  != iBeatsPerBar ||
		pSession->beatDivisor()  != iBeatDivisor ||
		pSession->ticksPerBeat() != iTicksPerBeat) {
		m_pTempoCommand = new qtractorSessionTempoCommand(pSession,
			fTempo, iBeatType, iBeatsPerBar, iBeatDivisor, iTicksPerBeat);
	}

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
