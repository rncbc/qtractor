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
// class qtractorSessionLoopCommand - implementation
//

// Constructor.
qtractorSessionLoopCommand::qtractorSessionLoopCommand (
	qtractorSession *pSession, unsigned long iLoopStart, unsigned long iLoopEnd )
	: qtractorSessionCommand(QObject::tr("session loop"), pSession),
		m_iLoopStart(iLoopStart), m_iLoopEnd(iLoopEnd)
{
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
#if 0
	// Restore edit cursors too...
	if (m_iLoopStart < m_iLoopEnd) {
		pSession->setEditHead(m_iLoopStart);
		pSession->setEditTail(m_iLoopEnd);
	}
#endif
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
		m_iPunchIn(iPunchIn), m_iPunchOut(iPunchOut)
{
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

	// Just set new bounds...
	pSession->setPunch(m_iPunchIn, m_iPunchOut);
#if 0
	// Restore edit cursors too...
	if (m_iPunchIn < m_iPunchOut) {
		pSession->setEditHead(m_iPunchIn);
		pSession->setEditTail(m_iPunchOut);
	}
#endif
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
	: qtractorSessionCommand(QObject::tr("session properties"), pSession),
	   m_pPropertiesCommand(NULL), m_pTempoCommand(NULL), m_iTicksPerBeat(0)
{
	// Actual properties command...
	m_pPropertiesCommand
		= new qtractorPropertyCommand<qtractorSession::Properties> (
			name(), pSession->properties(), properties);

	// Append tempo/time-siganture changes...
	float fTempo = properties.timeScale.tempo();
	unsigned short iBeatsPerBar = properties.timeScale.beatsPerBar();
	unsigned short iBeatDivisor = properties.timeScale.beatDivisor();
	if (pSession->tempo() != fTempo ||
		pSession->beatsPerBar() != iBeatsPerBar ||
		pSession->beatDivisor() != iBeatDivisor) {
		qtractorTimeScale *pTimeScale = pSession->timeScale();
		qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
		qtractorTimeScale::Node *pNode = cursor.seekFrame(0);
		m_pTempoCommand = new qtractorTimeScaleUpdateNodeCommand(
			pTimeScale, pNode->frame, fTempo, 2, iBeatsPerBar, iBeatDivisor);
	}

	// Append time resolution changes too...
	unsigned short iTicksPerBeat = properties.timeScale.ticksPerBeat();
	if (pSession->ticksPerBeat() != iTicksPerBeat)
		m_iTicksPerBeat = iTicksPerBeat;
}

// Destructor.
qtractorSessionEditCommand::~qtractorSessionEditCommand (void)
{
	if (m_pTempoCommand)
		delete m_pTempoCommand;
	if (m_pPropertiesCommand)
		delete m_pPropertiesCommand;
}


// Session-edit command methods.
bool qtractorSessionEditCommand::redo (void)
{
	bool bResult = false;

	if (m_pPropertiesCommand)
		bResult = m_pPropertiesCommand->redo();

	if (bResult && m_pTempoCommand)
		bResult = m_pTempoCommand->redo();

	if (bResult && m_iTicksPerBeat > 0)
		session()->updateTimeResolution();

	return bResult;
}

bool qtractorSessionEditCommand::undo (void)
{
	bool bResult = false;

	if (m_pPropertiesCommand)
		bResult = m_pPropertiesCommand->undo();

	if (bResult && m_pTempoCommand)
		bResult = m_pTempoCommand->undo();

	if (bResult && m_iTicksPerBeat > 0)
		session()->updateTimeResolution();

	return bResult;
}


// end of qtractorSessionCommand.cpp
