// qtractorTimeScaleCommand.cpp
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

#include "qtractorAbout.h"
#include "qtractorTimeScaleCommand.h"

#include "qtractorClipCommand.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorAudioClip.h"


//----------------------------------------------------------------------
// class qtractorTimeScaleNodeCommand - implementation.
//

// Constructor.
qtractorTimeScaleNodeCommand::qtractorTimeScaleNodeCommand (
	const QString& sName, qtractorTimeScale *pTimeScale,
	qtractorTimeScale::Node *pNode, unsigned long iFrame,
	float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor)
	: qtractorCommand(sName), m_pTimeScale(pTimeScale), m_pNode(pNode),
		m_iFrame(iFrame), m_fTempo(fTempo), m_iBeatType(iBeatType),
		m_iBeatsPerBar(iBeatsPerBar), m_iBeatDivisor(iBeatDivisor),
		m_pClipCommand(NULL), m_bAutoTimeStretch(false)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		m_bAutoTimeStretch = pSession->isAutoTimeStretch();
}


// Destructor.
qtractorTimeScaleNodeCommand::~qtractorTimeScaleNodeCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;
}


// Add time-scale node command method.
bool qtractorTimeScaleNodeCommand::addNode (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	if (m_pNode)
		return false;

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

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

	return (m_pNode != NULL);
}


// Update time-scale node command method.
bool qtractorTimeScaleNodeCommand::updateNode (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	m_pNode = qtractorTimeScale::Cursor(m_pTimeScale).seekFrame(m_iFrame);
	if (m_pNode == NULL)
		return false;
	if (m_pNode->frame != m_iFrame)
		return false;

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

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

	return true;
}


// Remove time-scale node command method.
bool qtractorTimeScaleNodeCommand::removeNode (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	if (m_pNode == NULL)
		return false;

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

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

	return true;
}


// Make it automatic clip time-stretching command (static).
qtractorClipCommand *qtractorTimeScaleNodeCommand::createClipCommand (
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
	unsigned long iFrameEnd = (pNext ? pNext->frame : pSession->sessionEnd());

	qtractorClipCommand *pClipCommand = NULL;

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			if (pClip->clipStart() <  iFrameStart ||
				pClip->clipStart() >= iFrameEnd)
				continue;
			if (pTrack->trackType() == qtractorTrack::Audio) {
				qtractorAudioClip *pAudioClip
					= static_cast<qtractorAudioClip *> (pClip);
				if (pAudioClip) {
					if (pClipCommand == NULL)
						pClipCommand = new qtractorClipCommand(sName);
					if (m_bAutoTimeStretch) {
						float fTimeStretch
							= (fOldTempo * pAudioClip->timeStretch()) / fNewTempo;
						pClipCommand->timeStretchClip(pClip, fTimeStretch);
					} else {
						pClipCommand->resetClip(pClip);
					}
				}
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
	: qtractorTimeScaleNodeCommand(
		QObject::tr("add tempo node"), pTimeScale, NULL,
		iFrame, fTempo, iBeatType, iBeatsPerBar, iBeatDivisor)
{
}

// Time-scale node command methods.
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
	: qtractorTimeScaleNodeCommand(
		QObject::tr("update tempo node"), pTimeScale, NULL,
		iFrame, fTempo, iBeatType, iBeatsPerBar, iBeatDivisor)
{
}

// Time-scale node command methods.
bool qtractorTimeScaleUpdateNodeCommand::redo (void) { return updateNode(); }
bool qtractorTimeScaleUpdateNodeCommand::undo (void) { return redo(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleRemoveNodeCommand - implementation.
//

// Constructor.
qtractorTimeScaleRemoveNodeCommand::qtractorTimeScaleRemoveNodeCommand (
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Node *pNode )
	: qtractorTimeScaleNodeCommand(
		QObject::tr("remove tempo node"), pTimeScale, pNode)
{
}

// Time-scale node command methods.
bool qtractorTimeScaleRemoveNodeCommand::redo (void) { return removeNode(); }
bool qtractorTimeScaleRemoveNodeCommand::undo (void) { return addNode(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleMarkerCommand - implementation.
//

// Constructor.
qtractorTimeScaleMarkerCommand::qtractorTimeScaleMarkerCommand (
	const QString& sName, qtractorTimeScale *pTimeScale,
	qtractorTimeScale::Marker *pMarker, unsigned long iFrame,
	const QString& sText, const QColor& rgbColor )
	: qtractorCommand(sName), m_pTimeScale(pTimeScale), m_pMarker(pMarker),
		m_iFrame(iFrame), m_sText(sText), m_rgbColor(rgbColor)
{
}


// Add time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::addMarker (void)
{
	if (m_pMarker)
		return false;

	m_pMarker = m_pTimeScale->addMarker(m_iFrame, m_sText, m_rgbColor);

	return (m_pMarker != NULL);
}


// Update time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::updateMarker (void)
{
	m_pMarker = qtractorTimeScale::MarkerCursor(m_pTimeScale).seekFrame(m_iFrame);
	if (m_pMarker == NULL)
		return false;
	if (m_pMarker->frame != m_iFrame)
		return false;

	QString sText    = m_pMarker->text;
	QColor  rgbColor = m_pMarker->color;

	m_pMarker->text  = m_sText;
	m_pMarker->color = m_rgbColor;

	m_pTimeScale->updateMarker(m_pMarker);

	m_sText    = sText;
	m_rgbColor = rgbColor;

	return true;
}


// Remove time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::removeMarker (void)
{
	if (m_pMarker == NULL)
		return false;

	m_iFrame   = m_pMarker->frame;
	m_sText    = m_pMarker->text;
	m_rgbColor = m_pMarker->color;

	m_pTimeScale->removeMarker(m_pMarker);

	m_pMarker = NULL;

	return true;
}


//----------------------------------------------------------------------
// class qtractorTimeScaleAddMarkerCommand - implementation.
//

// Constructor.
qtractorTimeScaleAddMarkerCommand::qtractorTimeScaleAddMarkerCommand (
	qtractorTimeScale *pTimeScale, unsigned long iFrame,
	const QString& sText, const QColor& rgbColor )
	: qtractorTimeScaleMarkerCommand(
		QObject::tr("add marker"), pTimeScale, NULL,
			iFrame, sText, rgbColor)
{
}

// Time-scale marker command methods.
bool qtractorTimeScaleAddMarkerCommand::redo (void) { return addMarker(); }
bool qtractorTimeScaleAddMarkerCommand::undo (void) { return removeMarker(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleUpdateMarkerCommand - implementation.
//

// Constructor.
qtractorTimeScaleUpdateMarkerCommand::qtractorTimeScaleUpdateMarkerCommand (
	qtractorTimeScale *pTimeScale, unsigned long iFrame,
	const QString& sText, const QColor& rgbColor )
	: qtractorTimeScaleMarkerCommand(
		QObject::tr("update marker"), pTimeScale, NULL,
			iFrame, sText, rgbColor)
{
}

// Time-scale marker command methods.
bool qtractorTimeScaleUpdateMarkerCommand::redo (void) { return updateMarker(); }
bool qtractorTimeScaleUpdateMarkerCommand::undo (void) { return redo(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleRemoveMarkerCommand - implementation.
//

// Constructor.
qtractorTimeScaleRemoveMarkerCommand::qtractorTimeScaleRemoveMarkerCommand (
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Marker *pMarker )
	: qtractorTimeScaleMarkerCommand(
		QObject::tr("remove marker"), pTimeScale, pMarker)
{
}

// Time-scale marker command methods.
bool qtractorTimeScaleRemoveMarkerCommand::redo (void) { return removeMarker(); }
bool qtractorTimeScaleRemoveMarkerCommand::undo (void) { return addMarker(); }



// end of qtractorTimeScaleCommand.cpp
