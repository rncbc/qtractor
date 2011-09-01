// qtractorTimeScaleCommand.cpp
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
#include "qtractorTimeScaleCommand.h"

#include "qtractorClipCommand.h"
#include "qtractorSession.h"

#include "qtractorAudioClip.h"

#include <QString>


//----------------------------------------------------------------------
// class qtractorTimeScaleCommand - implementation.
//

// Constructor.
qtractorTimeScaleCommand::qtractorTimeScaleCommand ( const QString& sName, 
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Node *pNode,
	unsigned long iFrame, float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor)
	: qtractorCommand(sName), m_pTimeScale(pTimeScale), m_pNode(pNode),
		m_iFrame(iFrame), m_fTempo(fTempo), m_iBeatType(iBeatType),
		m_iBeatsPerBar(iBeatsPerBar), m_iBeatDivisor(iBeatDivisor),
		m_pClipCommand(NULL)
{
}


// Destructor.
qtractorTimeScaleCommand::~qtractorTimeScaleCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;
}


// Add time-scale node command method.
bool qtractorTimeScaleCommand::addNode (void)
{
	if (m_pNode)
		return false;

	if (m_pClipCommand)
		m_pClipCommand->undo();
	
	m_pNode = m_pTimeScale->addNode(
		m_iFrame,
		m_fTempo,
		m_iBeatType,
		m_iBeatsPerBar,
		m_iBeatDivisor);

	if (m_pClipCommand) {
		delete m_pClipCommand;
		m_pClipCommand = NULL;
	} else {
		qtractorTimeScale::Node *pPrev = m_pNode->prev();
		float fOldTempo = (pPrev ? pPrev->tempo : m_pTimeScale->tempo());
		float fNewTempo = m_pNode->tempo;
		m_pClipCommand = createClipCommand(name(),
			m_pNode, fNewTempo, fOldTempo);
		if (m_pClipCommand)
			m_pClipCommand->redo();
	}

	return (m_pNode != NULL);
}


// Update time-scale node command method.
bool qtractorTimeScaleCommand::updateNode (void)
{
	m_pNode = qtractorTimeScale::Cursor(m_pTimeScale).seekFrame(m_iFrame);
	if (m_pNode == NULL)
		return false;
	if (m_pNode->frame != m_iFrame)
		return false;

	float          fTempo       = m_pNode->tempo;
	unsigned short iBeatType    = m_pNode->beatType;
	unsigned short iBeatsPerBar = m_pNode->beatsPerBar;
	unsigned short iBeatDivisor = m_pNode->beatDivisor;

	if (m_pClipCommand) {
		m_pClipCommand->undo();
		delete m_pClipCommand;
		m_pClipCommand = NULL;
	} else {
		float fOldTempo = m_pNode->tempo;
		float fNewTempo = m_fTempo;
		m_pClipCommand = createClipCommand(name(),
			m_pNode, fNewTempo, fOldTempo);
	}

	m_pNode->tempo       = m_fTempo;
	m_pNode->beatType    = m_iBeatType;
	m_pNode->beatsPerBar = m_iBeatsPerBar;
	m_pNode->beatDivisor = m_iBeatDivisor;

	m_pTimeScale->updateNode(m_pNode);

	m_fTempo       = fTempo;
	m_iBeatType    = iBeatType;
	m_iBeatsPerBar = iBeatsPerBar;
	m_iBeatDivisor = iBeatDivisor;

	if (m_pClipCommand)
		m_pClipCommand->redo();

	return true;
}


// Remove time-scale node command method.
bool qtractorTimeScaleCommand::removeNode (void)
{
	if (m_pNode == NULL)
		return false;

	if (m_pClipCommand) {
		m_pClipCommand->undo();
		delete m_pClipCommand;
		m_pClipCommand = NULL;
	} else {
		qtractorTimeScale::Node *pPrev = m_pNode->prev();
		float fOldTempo = m_pNode->tempo;
		float fNewTempo = (pPrev ? pPrev->tempo : m_pTimeScale->tempo());
		m_pClipCommand = createClipCommand(name(),
			m_pNode, fNewTempo, fOldTempo);
	}

	m_iFrame       = m_pNode->frame;
	m_fTempo       = m_pNode->tempo;
	m_iBeatType    = m_pNode->beatType;
	m_iBeatsPerBar = m_pNode->beatsPerBar;
	m_iBeatDivisor = m_pNode->beatDivisor;

	m_pTimeScale->removeNode(m_pNode);

	m_pNode = NULL;

	if (m_pClipCommand)
		m_pClipCommand->redo();

	return true;
}


// Make it automatic clip time-stretching command (static).
qtractorClipCommand *qtractorTimeScaleCommand::createClipCommand (
	const QString& sName, qtractorTimeScale::Node *pNode,
	float fNewTempo, float fOldTempo )
{
	if (pNode == NULL)
		return NULL;
	if (fNewTempo == fOldTempo)
		return NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	qtractorTimeScale::Node *pNext = pNode->next();
	unsigned long iFrameStart = pNode->frame;
	unsigned long iFrameEnd = (pNext ? pNext->frame : pSession->sessionLength());

	qtractorClipCommand *pClipCommand = NULL;

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			if (pClip->clipStart() <  iFrameStart ||
				pClip->clipStart() >= iFrameEnd)
				continue;
			if (pClipCommand == NULL)
				pClipCommand = new qtractorClipCommand(sName);
			switch (pTrack->trackType()) {
			case qtractorTrack::Audio:
				if (pSession->isAutoTimeStretch()) {
					qtractorAudioClip *pAudioClip
						= static_cast<qtractorAudioClip *> (pClip);
					if (pAudioClip) {
						float fTimeStretch = (fOldTempo
							* pAudioClip->timeStretch()) / fNewTempo;
						pClipCommand->timeStretchClip(pClip, fTimeStretch);
					}
				}
				break;
			case qtractorTrack::Midi:
				if (!pSession->isAutoTimeStretch()) {
					float fTimeStretch = (fNewTempo / fOldTempo);
					pClipCommand->timeStretchClip(pClip, fTimeStretch);
				}
				// Fall thru...
			default:
				break;
			}
		}
	}

	// Take care of possible empty commands...
	if (pClipCommand && pClipCommand->isEmpty()) {
		delete pClipCommand;
		pClipCommand = NULL;
	}
	
	return pClipCommand;
}


//----------------------------------------------------------------------
// class qtractorTimeScaleAddNodeCommand - implementation.
//

// Constructor.
qtractorTimeScaleAddNodeCommand::qtractorTimeScaleAddNodeCommand (
	qtractorTimeScale *pTimeScale, unsigned long iFrame,
	float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
	: qtractorTimeScaleCommand(QObject::tr("add tempo node"), pTimeScale,
		NULL, iFrame, fTempo, iBeatType, iBeatsPerBar, iBeatDivisor)
{
}

// Time-scale command methods.
bool qtractorTimeScaleAddNodeCommand::redo (void) { return addNode(); }
bool qtractorTimeScaleAddNodeCommand::undo (void) { return removeNode(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleUpdateNodeCommand - implementation.
//

// Constructor.
qtractorTimeScaleUpdateNodeCommand::qtractorTimeScaleUpdateNodeCommand (
	qtractorTimeScale *pTimeScale, unsigned long iFrame,
	float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor)
	: qtractorTimeScaleCommand(QObject::tr("update tempo node"), pTimeScale,
		NULL, iFrame, fTempo, iBeatType, iBeatsPerBar, iBeatDivisor)
{
}

// Time-scale command methods.
bool qtractorTimeScaleUpdateNodeCommand::redo (void) { return updateNode(); }
bool qtractorTimeScaleUpdateNodeCommand::undo (void) { return redo(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleRemoveNodeCommand - implementation.
//

// Constructor.
qtractorTimeScaleRemoveNodeCommand::qtractorTimeScaleRemoveNodeCommand (
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Node *pNode )
	: qtractorTimeScaleCommand(QObject::tr("remove tempo node"), pTimeScale, pNode)
{
}

// Time-scale command methods.
bool qtractorTimeScaleRemoveNodeCommand::redo (void) { return removeNode(); }
bool qtractorTimeScaleRemoveNodeCommand::undo (void) { return addNode(); }


// end of qtractorTimeScaleCommand.cpp
