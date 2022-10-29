// qtractorTimeScaleCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2020, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorCurveCommand.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"


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
		m_bAutoTimeStretch(false), m_pClipCommand(nullptr)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		m_bAutoTimeStretch = pSession->isAutoTimeStretch();

	setClearSelectReset(true);
}


// Destructor.
qtractorTimeScaleNodeCommand::~qtractorTimeScaleNodeCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;

	qDeleteAll(m_curveEditCommands);
	m_curveEditCommands.clear();
}


// Add time-scale node command method.
bool qtractorTimeScaleNodeCommand::addNode (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	qtractorTimeScale::Cursor& cursor = m_pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iFrame);
	if (pNode == nullptr)
		return false;

	// If currently playing, we need to do a stop and go...
	const bool bPlaying = pSession->isPlaying();
	const unsigned long iPlayHead = pSession->playHead();

	pSession->lock();

	if (bPlaying)
		pSession->seek(0, false);

	const float fOldTempo = pNode->tempo;
	const float fNewTempo = m_fTempo;

	const bool bRedoClipCommand = (m_pClipCommand == nullptr);
	if (bRedoClipCommand)
		m_pClipCommand = createClipCommand(pNode, fNewTempo, fOldTempo);
	else
	if (m_pClipCommand) {
		m_pClipCommand->undo();
		delete m_pClipCommand;
		m_pClipCommand = nullptr;
	}

	pNode = m_pTimeScale->addNode(
		m_iFrame, m_fTempo, m_iBeatType, m_iBeatsPerBar, m_iBeatDivisor);

	const bool bRedoCurveEditCommands = m_curveEditCommands.isEmpty();
	if (bRedoCurveEditCommands) {
		addCurveEditCommands(pNode, fNewTempo, fOldTempo);
	} else {
		QListIterator<qtractorCurveEditCommand *> undos(m_curveEditCommands);
		while (undos.hasNext())
			undos.next()->undo();
		qDeleteAll(m_curveEditCommands);
		m_curveEditCommands.clear();
	}

	if (m_pClipCommand && bRedoClipCommand)
		m_pClipCommand->redo();

	if (bRedoCurveEditCommands) {
		QListIterator<qtractorCurveEditCommand *> redos(m_curveEditCommands);
		while (redos.hasNext())
			redos.next()->redo();
	}

	// Restore playback state, if needed...
	if (bPlaying) {
		// The Audio engine too...
		if (pSession->audioEngine())
			pSession->audioEngine()->resetMetro();
		// The MIDI engine queue needs a reset...
		if (pSession->midiEngine())
			pSession->midiEngine()->resetTempo();
		// Resync all clips on play-head...
		pSession->seek(iPlayHead, true);
	} else {
		// Force JACK Timebase state, if applicable...
		if (pSession->audioEngine())
			pSession->audioEngine()->resetTimebase();
	}

	pSession->unlock();

	return true;
}


// Update time-scale node command method.
bool qtractorTimeScaleNodeCommand::updateNode (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iFrame);
	if (pNode == nullptr)
		return false;
	if (pNode->frame != m_iFrame)
		return false;

	// If currently playing, we need to do a stop and go...
	const bool bPlaying = pSession->isPlaying();
	const unsigned long iPlayHead = pSession->playHead();

	pSession->lock();

	if (bPlaying)
		pSession->seek(0, false);

	const float          fTempo       = pNode->tempo;
	const unsigned short iBeatType    = pNode->beatType;
	const unsigned short iBeatsPerBar = pNode->beatsPerBar;
	const unsigned short iBeatDivisor = pNode->beatDivisor;

	const float fOldTempo = pNode->tempo;
	const float fNewTempo = m_fTempo;

	const bool bRedoClipCommand = (m_pClipCommand == nullptr);
	if (bRedoClipCommand)
		m_pClipCommand = createClipCommand(pNode, fNewTempo, fOldTempo);
	else
	if (m_pClipCommand) {
		m_pClipCommand->undo();
		delete m_pClipCommand;
		m_pClipCommand = nullptr;
	}

	const bool bRedoCurveEditCommands = m_curveEditCommands.isEmpty();
	if (bRedoCurveEditCommands) {
		addCurveEditCommands(pNode, fNewTempo, fOldTempo);
	} else {
		QListIterator<qtractorCurveEditCommand *> undos(m_curveEditCommands);
		while (undos.hasNext())
			undos.next()->undo();
		qDeleteAll(m_curveEditCommands);
		m_curveEditCommands.clear();
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

	if (m_pClipCommand && bRedoClipCommand)
		m_pClipCommand->redo();

	if (bRedoCurveEditCommands) {
		QListIterator<qtractorCurveEditCommand *> redos(m_curveEditCommands);
		while (redos.hasNext())
			redos.next()->redo();
	}

	// Restore playback state, if needed...
	if (bPlaying) {
		// Resync all clips on play-head...
		pSession->seek(iPlayHead, true);
		// The Audio engine too...
		if (pSession->audioEngine())
			pSession->audioEngine()->resetMetro();
		// The MIDI engine queue needs a reset...
		if (pSession->midiEngine())
			pSession->midiEngine()->resetTempo();
	} else {
		// Force JACK Timebase state, if applicable...
		if (pSession->audioEngine())
			pSession->audioEngine()->resetTimebase();
	}

	pSession->unlock();

	return true;
}


// Remove time-scale node command method.
bool qtractorTimeScaleNodeCommand::removeNode (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iFrame);
	if (pNode == nullptr)
		return false;
	if (pNode->frame != m_iFrame)
		return false;

	// If currently playing, we need to do a stop and go...
	const bool bPlaying = pSession->isPlaying();
	const unsigned long iPlayHead = pSession->playHead();

	pSession->lock();

	if (bPlaying)
		pSession->seek(0, false);

	qtractorTimeScale::Node *pPrev = pNode->prev();
	const float fOldTempo = pNode->tempo;
	const float fNewTempo = (pPrev ? pPrev->tempo : m_pTimeScale->tempo());

	const bool bRedoClipCommand = (m_pClipCommand == nullptr);
	if (bRedoClipCommand)
		m_pClipCommand = createClipCommand(pNode, fNewTempo, fOldTempo);
	else
	if (m_pClipCommand) {
		m_pClipCommand->undo();
		delete m_pClipCommand;
		m_pClipCommand = nullptr;
	}

	const bool bRedoCurveEditCommands = m_curveEditCommands.isEmpty();
	if (bRedoCurveEditCommands) {
		addCurveEditCommands(pNode, fNewTempo, fOldTempo);
	} else {
		QListIterator<qtractorCurveEditCommand *> undos(m_curveEditCommands);
		while (undos.hasNext())
			undos.next()->undo();
		qDeleteAll(m_curveEditCommands);
		m_curveEditCommands.clear();
	}

	m_fTempo       = pNode->tempo;
	m_iBeatType    = pNode->beatType;
	m_iBeatsPerBar = pNode->beatsPerBar;
	m_iBeatDivisor = pNode->beatDivisor;

	m_pTimeScale->removeNode(pNode);

	if (m_pClipCommand && bRedoClipCommand)
		m_pClipCommand->redo();

	if (bRedoCurveEditCommands) {
		QListIterator<qtractorCurveEditCommand *> redos(m_curveEditCommands);
		while (redos.hasNext())
			redos.next()->redo();
	}

	// Restore playback state, if needed...
	if (bPlaying) {
		// The Audio engine too...
		if (pSession->audioEngine())
			pSession->audioEngine()->resetMetro();
		// The MIDI engine queue needs a reset...
		if (pSession->midiEngine())
			pSession->midiEngine()->resetTempo();
		// Resync all clips on play-head...
		pSession->seek(iPlayHead, true);
	} else {
		// Force JACK Timebase state, if applicable...
		if (pSession->audioEngine())
			pSession->audioEngine()->resetTimebase();
	}

	pSession->unlock();

	return true;
}


// Make it automatic clip time-stretching command (static).
qtractorClipCommand *qtractorTimeScaleNodeCommand::createClipCommand (
	qtractorTimeScale::Node *pNode, float fNewTempo, float fOldTempo )
{
	if (pNode == nullptr)
		return nullptr;
	if (fNewTempo == fOldTempo)
		return nullptr;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return nullptr;

	qtractorTimeScale::Node *pNext = pNode->next();
	const unsigned long iFrameStart = pNode->frame;
	const unsigned long iFrameEnd = (pNext ? pNext->frame : pSession->sessionEnd());

	qtractorClipCommand *pClipCommand = nullptr;

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
					if (pClipCommand == nullptr)
						pClipCommand = new qtractorClipCommand(name());
					if (m_bAutoTimeStretch) {
						const float fTimeStretch
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
		pClipCommand = nullptr;
	}
	
	return pClipCommand;
}


// Automation curve time-stretching command (static).
void qtractorTimeScaleNodeCommand::addCurveEditCommands (
	qtractorTimeScale::Node *pNode, float fNewTempo, float fOldTempo )
{
	if (pNode == nullptr)
		return;
	if (fNewTempo == fOldTempo)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	const unsigned long iFrameStart = pNode->frame;
	const float fFactor = (fOldTempo / fNewTempo);
	const bool bReverse = (fOldTempo > fNewTempo);

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		qtractorCurveList *pCurveList = pTrack->curveList();
		if (pCurveList == nullptr)
			continue;
		qtractorCurve *pCurve = pCurveList->first();
		while (pCurve) {
			int iCurveEditUpdate = 0;
			qtractorCurveEditCommand *pCurveEditCommand
				= new qtractorCurveEditCommand(QString(), pCurve);
			qtractorCurve::Node *pCurveNode = (bReverse
				? pCurve->nodes().last() : pCurve->seek(iFrameStart));
			while (pCurveNode) {
				if (pCurveNode->frame >= iFrameStart) {
					const unsigned long iFrame = iFrameStart + (unsigned long)
						(float(pCurveNode->frame - iFrameStart) * fFactor);
					const float fValue = pCurveNode->value;
					pCurveEditCommand->moveNode(pCurveNode, iFrame, fValue);
					++iCurveEditUpdate;
				} else if (bReverse)
					break;
				if (bReverse)
					pCurveNode = pCurveNode->prev();
				else
					pCurveNode = pCurveNode->next();
			}
			if (iCurveEditUpdate > 0)
				m_curveEditCommands.append(pCurveEditCommand);
			else
				delete pCurveEditCommand;
			pCurve = pCurve->next();
		}
	}
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
	if (pTimeScale == nullptr)
		return false;

	const unsigned long iNewFrame = m_iNewFrame;
	const unsigned long iOldFrame = m_iOldFrame;

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
	if (pTimeScale == nullptr)
		return false;

	const bool bResult = redo();

	if (bResult && m_bOldNode) {
		pTimeScale->addNode(m_iNewFrame, m_fOldTempo,
			m_iOldBeatType, m_iOldBeatsPerBar, m_iOldBeatDivisor);
	}

	return bResult;
}


//----------------------------------------------------------------------
// class qtractorTimeScaleMarkerCommand - implementation.
//

// Constructors.
qtractorTimeScaleMarkerCommand::qtractorTimeScaleMarkerCommand (
	const QString& sName, qtractorTimeScale *pTimeScale, unsigned long iFrame,
	const QString& sText, const QColor& rgbColor,
	int iAccidentals, int iMode )
	: qtractorCommand(sName), m_pTimeScale(pTimeScale),
		m_iFrame(iFrame), m_sText(sText), m_rgbColor(rgbColor),
		m_iAccidentals(iAccidentals), m_iMode(iMode)
{
}


// Add time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::addMarker (void)
{
	return (m_pTimeScale->addMarker(m_iFrame, m_sText, m_rgbColor) != nullptr);
}


// Update time-scale marker command method.
bool qtractorTimeScaleMarkerCommand::updateMarker (void)
{
	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(m_iFrame);
	if (pMarker == nullptr)
		return false;
	if (pMarker->frame != m_iFrame)
		return false;

	const QString sText    = pMarker->text;
	const QColor  rgbColor = pMarker->color;

	pMarker->text  = m_sText;
	pMarker->color = m_rgbColor;

	m_pTimeScale->updateMarker(pMarker);

	m_sText    = sText;
	m_rgbColor = rgbColor;

	return true;
}


// Add key-signature command method.
bool qtractorTimeScaleMarkerCommand::addKeySignature (void)
{
	return (m_pTimeScale->addKeySignature(m_iFrame, m_iAccidentals, m_iMode) != nullptr);
}


// Update key-signature command method.
bool qtractorTimeScaleMarkerCommand::updateKeySignature (void)
{
	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(m_iFrame);
	if (pMarker == nullptr)
		return false;
	if (pMarker->frame != m_iFrame)
		return false;

	const int iAccidentals = pMarker->accidentals;
	const int iMode        = pMarker->mode;

	pMarker->accidentals = m_iAccidentals;
	pMarker->mode        = m_iMode;

	m_pTimeScale->updateMarker(pMarker);

	m_iAccidentals = iAccidentals;
	m_iMode        = iMode;

	return true;
}


// Remove time-scale marker/key-signature command method.
bool qtractorTimeScaleMarkerCommand::removeMarker (void)
{
	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(m_iFrame);
	if (pMarker == nullptr)
		return false;
	if (pMarker->frame != m_iFrame)
		return false;

//	m_iFrame   = pMarker->frame;
	m_sText    = pMarker->text;
	m_rgbColor = pMarker->color;
	m_iAccidentals = pMarker->accidentals;
	m_iMode    = pMarker->mode;

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
// class qtractorTimeScaleAddKeySignatureCommand - implementation.
//

// Constructor.
qtractorTimeScaleAddKeySignatureCommand::qtractorTimeScaleAddKeySignatureCommand (
	qtractorTimeScale *pTimeScale, unsigned long iFrame,
	int iAccidentals, int iMode )
	: qtractorTimeScaleMarkerCommand(
		QObject::tr("add key signature"), pTimeScale,
			iFrame, QString(), Qt::darkGray, iAccidentals, iMode)
{
}

// Time-scale marker command methods.
bool qtractorTimeScaleAddKeySignatureCommand::redo (void) { return addKeySignature(); }
bool qtractorTimeScaleAddKeySignatureCommand::undo (void) { return removeMarker(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleUpdateKeySignatureCommand - implementation.
//

// Constructor.
qtractorTimeScaleUpdateKeySignatureCommand::qtractorTimeScaleUpdateKeySignatureCommand (
	qtractorTimeScale *pTimeScale, unsigned long iFrame,
	int iAccidentals, int iMode )
	: qtractorTimeScaleMarkerCommand(
		QObject::tr("update key signature"), pTimeScale,
			iFrame, QString(), Qt::darkGray, iAccidentals, iMode)
{
}

// Time-scale marker command methods.
bool qtractorTimeScaleUpdateKeySignatureCommand::redo (void) { return updateKeySignature(); }
bool qtractorTimeScaleUpdateKeySignatureCommand::undo (void) { return redo(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleRemoveKeySignatureCommand - implementation.
//

// Constructor.
qtractorTimeScaleRemoveKeySignatureCommand::qtractorTimeScaleRemoveKeySignatureCommand (
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Marker *pMarker )
	: qtractorTimeScaleMarkerCommand(
		QObject::tr("remove key signature"), pTimeScale, pMarker->frame)
{
}

// Time-scale marker command methods.
bool qtractorTimeScaleRemoveKeySignatureCommand::redo (void) { return removeMarker(); }
bool qtractorTimeScaleRemoveKeySignatureCommand::undo (void) { return addKeySignature(); }


//----------------------------------------------------------------------
// class qtractorTimeScaleMoveMarkerCommand - implementation.
//

// Constructor.
qtractorTimeScaleMoveMarkerCommand::qtractorTimeScaleMoveMarkerCommand (
	qtractorTimeScale *pTimeScale, qtractorTimeScale::Marker *pMarker,
	unsigned long iFrame ) : qtractorTimeScaleMarkerCommand(
		QObject::tr("move marker"), pTimeScale,
		pMarker->frame, pMarker->text, pMarker->color,
		pMarker->accidentals, pMarker->mode)
{
	// The new location.
	m_iNewFrame = pTimeScale->frameFromBar(pTimeScale->barFromFrame(iFrame));
	m_iOldFrame = frame();

	// Replaced marker salvage.
	pMarker	= pTimeScale->markers().seekFrame(m_iNewFrame);
	if (pMarker && pMarker->frame == m_iNewFrame) {
		m_bOldMarker  = true;
		m_sOldText    = pMarker->text;
		m_rgbOldColor = pMarker->color;
		m_iOldAccidentals = pMarker->accidentals;
		m_iOldMode    = pMarker->mode;
	} else {
		m_bOldMarker  = false;
	}
}


// Time-scale marker command methods.
bool qtractorTimeScaleMoveMarkerCommand::redo (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return false;

	const unsigned long iNewFrame = m_iNewFrame;
	const unsigned long iOldFrame = m_iOldFrame;

	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekFrame(iOldFrame);
	if (pMarker && pMarker->frame == iOldFrame)
		pTimeScale->removeMarker(pMarker);

	if (!text().isEmpty())
		pTimeScale->addMarker(iNewFrame, text(), color());
	if (accidentals() || mode())
		pTimeScale->addKeySignature(iNewFrame, accidentals(), mode());

	m_iNewFrame = iOldFrame;
	m_iOldFrame = iNewFrame;

	return true;
}


bool qtractorTimeScaleMoveMarkerCommand::undo (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return false;

	const bool bResult = redo();

	if (bResult && m_bOldMarker) {
		if (!m_sOldText.isEmpty())
			pTimeScale->addMarker(m_iNewFrame, m_sOldText, m_rgbOldColor);
		if (m_iOldAccidentals || m_iOldMode)
			pTimeScale->addKeySignature(m_iNewFrame, m_iOldAccidentals, m_iOldMode);
	}

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


//----------------------------------------------------------------------
// class qtractorTimeScaleCommand - declaration.
//

// Constructor.
qtractorTimeScaleTimeSig2Command::qtractorTimeScaleTimeSig2Command (
	qtractorTimeScale *pTimeScale, qtractorMidiClip *pMidiClip,
	unsigned short iBeatsPerBar2, unsigned short iBeatDivisor2 )
	: qtractorCommand(QObject::tr("change time-sig.")),
		m_pTimeScale(pTimeScale), m_pMidiClip(pMidiClip),
		m_iBeatsPerBar2(iBeatsPerBar2),
		m_iBeatDivisor2(iBeatDivisor2)
{
}


// Time-scale command methods.
bool qtractorTimeScaleTimeSig2Command::redo (void)
{
	if (m_pTimeScale == nullptr)
		return false;

	const unsigned short iBeatsPerBar2 = m_pTimeScale->beatsPerBar2();
	const unsigned short iBeatDivisor2 = m_pTimeScale->beatDivisor2();

	m_pTimeScale->setBeatsPerBar2(m_iBeatsPerBar2);
	m_pTimeScale->setBeatDivisor2(m_iBeatDivisor2);

	if (m_pMidiClip) {
		m_pMidiClip->setBeatsPerBar2(m_iBeatsPerBar2);
		m_pMidiClip->setBeatDivisor2(m_iBeatDivisor2);
	}

	m_iBeatsPerBar2 = iBeatsPerBar2;
	m_iBeatDivisor2 = iBeatDivisor2;

	return true;
}


bool qtractorTimeScaleTimeSig2Command::undo (void)
{
	return redo();
}


// end of qtractorTimeScaleCommand.cpp
