// qtractorTimeScaleCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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
	unsigned long iFrame, float fTempo, unsigned short iBeatType,
	unsigned short iBeatsPerBar, unsigned short iBeatDivisor)
	: qtractorCommand(sName), m_pTimeScale(pTimeScale),
		m_iFrame(iFrame), m_fTempo(fTempo), m_iBeatType(iBeatType),
		m_iBeatsPerBar(iBeatsPerBar), m_iBeatDivisor(iBeatDivisor),
		m_pClipCommand(NULL), m_bAutoTimeStretch(false)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		m_bAutoTimeStretch = pSession->isAutoTimeStretch();

	setClearSelect(true);
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

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

	if (m_pClipCommand)
		m_pClipCommand->undo();
	
	qtractorTimeScale::Node *pNode = m_pTimeScale->addNode(
		m_iFrame, m_fTempo, m_iBeatType, m_iBeatsPerBar, m_iBeatDivisor);

	if (m_pClipCommand) {
		delete m_pClipCommand;
		m_pClipCommand = NULL;
	} else {
		qtractorTimeScale::Node *pPrev = pNode->prev();
		float fOldTempo = (pPrev ? pPrev->tempo : m_pTimeScale->tempo());
		float fNewTempo = pNode->tempo;
		m_pClipCommand = createClipCommand(name(),
			pNode, fNewTempo, fOldTempo);
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

	return true;
}


// Update time-scale node command method.
bool qtractorTimeScaleNodeCommand::updateNode (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iFrame);
	if (pNode == NULL)
		return false;
	if (pNode->frame != m_iFrame)
		return false;

	// If currently playing, we need to do a stop and go...
	bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

	float          fTempo       = pNode->tempo;
	unsigned short iBeatType    = pNode->beatType;
	unsigned short iBeatsPerBar = pNode->beatsPerBar;
	unsigned short iBeatDivisor = pNode->beatDivisor;

	if (m_pClipCommand) {
		m_pClipCommand->undo();
		delete m_pClipCommand;
		m_pClipCommand = NULL;
	} else {
		float fOldTempo = pNode->tempo;
		float fNewTempo = m_fTempo;
		m_pClipCommand = createClipCommand(name(),
			pNode, fNewTempo, fOldTempo);
	}

	pNode->tempo       = m_fTempo;
	pNode->beatType    = m_iBeatType;
	pNode->beatsPerBar = m_iBeatsPerBar;
	pNode->beatDivisor = m_iBeatDivisor;

	m_pTimeScale->updateNode(pNode);

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

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iFrame);
	if (pNode == NULL)
		return false;
	if (pNode->frame != m_iFrame)
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
		qtractorTimeScale::Node *pPrev = pNode->prev();
		float fOldTempo = pNode->tempo;
		float fNewTempo = (pPrev ? pPrev->tempo : m_pTimeScale->tempo());
		m_pClipCommand = createClipCommand(name(),
			pNode, fNewTempo, fOldTempo);
	}

//	m_iFrame       = pNode->frame;
	m_fTempo       = pNode->tempo;
	m_iBeatType    = pNode->beatType;
	m_iBeatsPerBar = pNode->beatsPerBar;
	m_iBeatDivisor = pNode->beatDivisor;

	m_pTimeScale->removeNode(pNode);

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
		QObject::tr("add tempo node"), pTimeScale,
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
		QObject::tr("update tempo node"), pTimeScale,
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
		QObject::tr("remove tempo node"), pTimeScale, pNode->frame)
{
}

// Time-scale node command methods.
bool qtractorTimeScaleRemoveNodeCommand::redo (void) { return removeNode(); }
bool qtractorTimeScaleRemoveNodeCommand::undo (void) { return addNode(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleMoveNodeCommand - implementation.
//

// Constructor.
qtractorTimeScaleMoveNodeCommand::qtractorTimeScaleMoveNodeCommand (
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Node *pNode,
	unsigned long iFrame ) : qtractorTimeScaleNodeCommand(
		QObject::tr("move tempo node"), pTimeScale,
			pNode->frame, pNode->tempo, pNode->beatType,
			pNode->beatsPerBar, pNode->beatDivisor)
{
	// The new location.
	m_iNewFrame = pTimeScale->frameFromBar(pTimeScale->barFromFrame(iFrame));
	m_iOldFrame = frame();

	// Replaced node salvage.
	qtractorTimeScale::Cursor cursor(pTimeScale);
	pNode = cursor.seekFrame(m_iNewFrame);
	if (pNode && pNode->frame == m_iNewFrame) {
		m_bOldNode = true;
		m_fOldTempo = pNode->tempo;
		m_iOldBeatType = pNode->beatType;
		m_iOldBeatsPerBar = pNode->beatsPerBar;
		m_iOldBeatDivisor = pNode->beatDivisor;
	} else {
		m_bOldNode = false;
	}
}


// Time-scale node command methods.
bool qtractorTimeScaleMoveNodeCommand::redo (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == NULL)
		return false;

	unsigned long iNewFrame = m_iNewFrame;
	unsigned long iOldFrame = m_iOldFrame;

	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iOldFrame);
	if (pNode && pNode->frame == iOldFrame)
		pTimeScale->removeNode(pNode);

	pTimeScale->addNode(iNewFrame,
		tempo(), beatType(), beatsPerBar(), beatDivisor());

	m_iNewFrame = iOldFrame;
	m_iOldFrame = iNewFrame;

	return true;
}


bool qtractorTimeScaleMoveNodeCommand::undo (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == NULL)
		return false;

	bool bResult = redo();

	if (bResult && m_bOldNode) {
		pTimeScale->addNode(m_iNewFrame, m_fOldTempo,
			m_iOldBeatType, m_iOldBeatsPerBar, m_iOldBeatDivisor);
	}

	return bResult;
}


//----------------------------------------------------------------------
// class qtractorTimeScaleMarkerCommand - implementation.
//

// Constructor.
qtractorTimeScaleMarkerCommand::qtractorTimeScaleMarkerCommand (
	const QString& sName, qtractorTimeScale *pTimeScale,
	unsigned long iFrame, const QString& sText, const QColor& rgbColor )
	: qtractorCommand(sName), m_pTimeScale(pTimeScale),
		m_iFrame(iFrame), m_sText(sText), m_rgbColor(rgbColor)
{
}


// Add time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::addMarker (void)
{
	return (m_pTimeScale->addMarker(m_iFrame, m_sText, m_rgbColor) != NULL);
}


// Update time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::updateMarker (void)
{
	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(m_iFrame);
	if (pMarker == NULL)
		return false;
	if (pMarker->frame != m_iFrame)
		return false;

	QString sText    = pMarker->text;
	QColor  rgbColor = pMarker->color;

	pMarker->text  = m_sText;
	pMarker->color = m_rgbColor;

	m_pTimeScale->updateMarker(pMarker);

	m_sText    = sText;
	m_rgbColor = rgbColor;

	return true;
}


// Remove time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::removeMarker (void)
{
	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(m_iFrame);
	if (pMarker == NULL)
		return false;
	if (pMarker->frame != m_iFrame)
		return false;

//	m_iFrame   = pMarker->frame;
	m_sText    = pMarker->text;
	m_rgbColor = pMarker->color;

	m_pTimeScale->removeMarker(pMarker);

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
		QObject::tr("add marker"), pTimeScale,
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
		QObject::tr("update marker"), pTimeScale,
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
		QObject::tr("remove marker"), pTimeScale, pMarker->frame)
{
}

// Time-scale marker command methods.
bool qtractorTimeScaleRemoveMarkerCommand::redo (void) { return removeMarker(); }
bool qtractorTimeScaleRemoveMarkerCommand::undo (void) { return addMarker(); }



//----------------------------------------------------------------------
// class qtractorTimeScaleMoveMarkerCommand - implementation.
//

// Constructor.
qtractorTimeScaleMoveMarkerCommand::qtractorTimeScaleMoveMarkerCommand (
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Marker *pMarker,
	unsigned long iFrame ) : qtractorTimeScaleMarkerCommand(
		QObject::tr("move marker"), pTimeScale,
		pMarker->frame, pMarker->text, pMarker->color)
{
	// The new location.
	m_iNewFrame = pTimeScale->frameFromBar(pTimeScale->barFromFrame(iFrame));
	m_iOldFrame = frame();

	// Replaced marker salvage.
	pMarker	= pTimeScale->markers().seekFrame(m_iNewFrame);
	if (pMarker && pMarker->frame == m_iNewFrame) {
		m_bOldMarker = true;
		m_sOldText = pMarker->text;
		m_rgbOldColor = pMarker->color;
	} else {
		m_bOldMarker = false;
	}
}


// Time-scale marker command methods.
bool qtractorTimeScaleMoveMarkerCommand::redo (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == NULL)
		return false;

	unsigned long iNewFrame = m_iNewFrame;
	unsigned long iOldFrame = m_iOldFrame;

	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekFrame(iOldFrame);
	if (pMarker && pMarker->frame == iOldFrame)
		pTimeScale->removeMarker(pMarker);

	pTimeScale->addMarker(iNewFrame, text(), color());

	m_iNewFrame = iOldFrame;
	m_iOldFrame = iNewFrame;

	return true;
}


bool qtractorTimeScaleMoveMarkerCommand::undo (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == NULL)
		return false;

	bool bResult = redo();

	if (bResult && m_bOldMarker)
		pTimeScale->addMarker(m_iNewFrame, m_sOldText, m_rgbOldColor);

	return bResult;
}


//----------------------------------------------------------------------
// class qtractorTimeScaleCommand - declaration.
//

// Constructor.
qtractorTimeScaleCommand::qtractorTimeScaleCommand ( const QString& sName )
	: qtractorCommand(sName)
{
}


// Destructor.
qtractorTimeScaleCommand::~qtractorTimeScaleCommand (void)
{
	qDeleteAll(m_nodeCommands);
	m_nodeCommands.clear();
}


// Node commands.
void qtractorTimeScaleCommand::addNodeCommand (
	qtractorTimeScaleNodeCommand *pNodeCommand )
{
	m_nodeCommands.append(pNodeCommand);
}


// Time-scale command methods.
bool qtractorTimeScaleCommand::redo (void)
{
	int iRedos = 0;

	QListIterator<qtractorTimeScaleNodeCommand *> iter(m_nodeCommands);
	iter.toFront();
	while (iter.hasNext()) {
		if (iter.next()->redo())
			++iRedos;
	}

	return (iRedos > 0);
}


bool qtractorTimeScaleCommand::undo (void)
{
	int iUndos = 0;

	QListIterator<qtractorTimeScaleNodeCommand *> iter(m_nodeCommands);
	iter.toBack();
	while (iter.hasPrevious()) {
		if (iter.previous()->undo())
			++iUndos;
	}

	return (iUndos > 0);
}


// end of qtractorTimeScaleCommand.cpp
