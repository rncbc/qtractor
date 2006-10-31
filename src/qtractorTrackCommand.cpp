// qtractorTrackCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTrackCommand.h"
#include "qtractorClipCommand.h"

#include "qtractorMainForm.h"

#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorTrackList.h"
#include "qtractorTrackView.h"
#include "qtractorTrackButton.h"
#include "qtractorMidiEngine.h"
#include "qtractorMixer.h"
#include "qtractorMeter.h"


//----------------------------------------------------------------------
// class qtractorTrackCommand - implementation
//

// Constructor.
qtractorTrackCommand::qtractorTrackCommand ( qtractorMainForm *pMainForm,
	const QString& sName, qtractorTrack *pTrack )
	: qtractorCommand(pMainForm, sName), m_pTrack(pTrack)
{
}


// Destructor.
qtractorTrackCommand::~qtractorTrackCommand (void)
{
	if (isAutoDelete())
		delete m_pTrack;
}


// Track command methods.
bool qtractorTrackCommand::addTrack (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackCommand::addTrack(%p)\n", m_pTrack);
#endif
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

	qtractorTracks *pTracks = mainForm()->tracks();
	if (pTracks == NULL)
		return false;

	// Guess which item we're adding after...
	qtractorTrack *pAfterTrack = m_pTrack->prev();
	if (pAfterTrack == NULL && m_pTrack->next() == NULL)
		pAfterTrack = pSession->tracks().last();
	int iTrack = pTracks->trackList()->trackRow(m_pTrack->next());
	// Link the track into session...
	pSession->insertTrack(m_pTrack, pAfterTrack);
	// And the new track list view item too...
	pTracks->trackList()->insertTrack(iTrack, m_pTrack);
	// Special MIDI track cases...
	if (m_pTrack->trackType() == qtractorTrack::Midi)
	    pTracks->updateMidiTrack(m_pTrack);

	// Mixer turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer)
		pMixer->updateTracks();

	// Avoid disposal of the track reference.
	setAutoDelete(false);

	return true;
}


bool qtractorTrackCommand::removeTrack (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackCommand::removeTrack(%p)\n", m_pTrack);
#endif
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

	qtractorTracks *pTracks = mainForm()->tracks();
	if (pTracks == NULL)
		return false;

	// Get the list view item reference of the intended track...
	int iTrack = pTracks->trackList()->trackRow(m_pTrack);
	if (iTrack < 0)
		return false;

	// Second, remove from session...
	pSession->unlinkTrack(m_pTrack);
	// Third, remove track from list view...
	pTracks->trackList()->removeTrack(iTrack);

	// Mixer turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer)
		pMixer->updateTracks();

	// Make ths track reference disposable.
	setAutoDelete(true);

	return true;
}


//----------------------------------------------------------------------
// class qtractorAddTrackCommand - implementation
//

// Constructor.
qtractorAddTrackCommand::qtractorAddTrackCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack )
	: qtractorTrackCommand(pMainForm, QObject::tr("add track"), pTrack)
{
}


// Track insertion command methods.
bool qtractorAddTrackCommand::redo (void)
{
	return addTrack();
}

bool qtractorAddTrackCommand::undo (void)
{
	return removeTrack();
}


//----------------------------------------------------------------------
// class qtractorRemoveTrackCommand - implementation
//

// Constructor.
qtractorRemoveTrackCommand::qtractorRemoveTrackCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack )
	: qtractorTrackCommand(pMainForm, QObject::tr("remove track"), pTrack)
{
}


// Track-removal command methods.
bool qtractorRemoveTrackCommand::redo (void)
{
	return removeTrack();
}

bool qtractorRemoveTrackCommand::undo (void)
{
	return addTrack();
}


//----------------------------------------------------------------------
// class qtractorMoveTrackCommand - implementation
//

// Constructor.
qtractorMoveTrackCommand::qtractorMoveTrackCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack,
		qtractorTrack *pNextTrack )
	: qtractorTrackCommand(pMainForm, QObject::tr("move track"), pTrack)
{
	m_pNextTrack = pNextTrack;
}


// Track-move command methods.
bool qtractorMoveTrackCommand::redo (void)
{
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

	qtractorTracks *pTracks = mainForm()->tracks();
	if (pTracks == NULL)
		return false;

	int iTrack = pTracks->trackList()->trackRow(track());
	if (iTrack < 0)
	    return false;

	// Save the next track alright...
	qtractorTrack *pNextTrack = track()->next();

	// Remove and insert back again...
	pSession->tracks().unlink(track());
	pTracks->trackList()->removeTrack(iTrack);

	// Get actual index of new position...
	int iNextTrack = pTracks->trackList()->trackRow(m_pNextTrack);

	// Make it all set back.
	pSession->tracks().insertBefore(track(), m_pNextTrack);
	pSession->reset();

	// Just insert under the track list position...
	// We'll renumber all items now...
	pTracks->trackList()->insertTrack(iNextTrack, track());
	// God'am, if we'll need this...
	pTracks->trackList()->updateZoomHeight();

	// Swap it nice, finally.
	m_pNextTrack = pNextTrack;

	return true;
}

bool qtractorMoveTrackCommand::undo (void)
{
	// As we swap the prev/track this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorResizeTrackCommand - implementation
//

// Constructor.
qtractorResizeTrackCommand::qtractorResizeTrackCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack, int iZoomHeight )
	: qtractorTrackCommand(pMainForm, QObject::tr("resize track"), pTrack)
{
	m_iZoomHeight = iZoomHeight;
}


// Track-resize command methods.
bool qtractorResizeTrackCommand::redo (void)
{
	qtractorTracks *pTracks = mainForm()->tracks();
	if (pTracks == NULL)
		return false;

	int iTrack = pTracks->trackList()->trackRow(track());
	if (iTrack < 0)
	    return false;

	// Save the previous item height alright...
	int iZoomHeight = track()->zoomHeight();

	// Just set new one...
	track()->setZoomHeight(m_iZoomHeight);
	pTracks->trackList()->setRowHeight(iTrack, m_iZoomHeight);

	// Swap it nice, finally.
	m_iZoomHeight = iZoomHeight;

	return true;
}

bool qtractorResizeTrackCommand::undo (void)
{
	// As we swap the prev/track this is non-identpotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorImportTrackCommand - implementation
//

// Constructor.
qtractorImportTrackCommand::qtractorImportTrackCommand (
	qtractorMainForm *pMainForm )
	: qtractorCommand(pMainForm, QObject::tr("import track"))
{
	// Session properties backup preparation.
	m_iSaveCount   = 0;
	m_pSaveCommand = NULL;
	qtractorSession *pSession = pMainForm->session();
	if (pSession) {
    	m_sessionProps = pSession->properties();
		m_pSaveCommand = new qtractorPropertyCommand<qtractorSession::Properties>
			(pMainForm, name(), pSession->properties(), m_sessionProps);
	}
}

// Destructor.
qtractorImportTrackCommand::~qtractorImportTrackCommand (void)
{
	if (m_pSaveCommand)
	    delete m_pSaveCommand;

	qDeleteAll(m_trackCommands);
	m_trackCommands.clear();
}


// Track-import list methods.
void qtractorImportTrackCommand::addTrack ( qtractorTrack *pTrack )
{
	m_trackCommands.append(
		new qtractorAddTrackCommand(mainForm(), pTrack));
}


// Track-import command methods.
bool qtractorImportTrackCommand::redo (void)
{
	bool bResult = true;

	if (m_pSaveCommand && m_iSaveCount > 0) {
		if (!m_pSaveCommand->redo())
		    bResult = false;
	}
	m_iSaveCount++;

	QListIterator<qtractorAddTrackCommand *> iter(m_trackCommands);
	while (iter.hasNext()) {
	    qtractorAddTrackCommand *pTrackCommand = iter.next();
		if (!pTrackCommand->redo())
		    bResult = false;
	}

	return bResult;
}

bool qtractorImportTrackCommand::undo (void)
{
	bool bResult = true;

	QListIterator<qtractorAddTrackCommand *> iter(m_trackCommands);
	while (iter.hasNext()) {
	    qtractorAddTrackCommand *pTrackCommand = iter.next();
		if (!pTrackCommand->undo())
		    bResult = false;
	}

	if (m_pSaveCommand && !m_pSaveCommand->undo())
		bResult = false;

	return bResult;
}


//----------------------------------------------------------------------
// class qtractorEditTrackCommand - implementation
//

// Constructor.
qtractorEditTrackCommand::qtractorEditTrackCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack,
	const qtractorTrack::Properties& props )
	: qtractorPropertyCommand<qtractorTrack::Properties> (pMainForm,
		QObject::tr("track properties"), pTrack->properties(), props)
{
	m_pTrack = pTrack;
}


// Overridden track-edit command methods.
bool qtractorEditTrackCommand::redo (void)
{
	qtractorTracks *pTracks = mainForm()->tracks();
	if (pTracks == NULL)
		return false;

	// Do the property dance.
	if (!qtractorPropertyCommand<qtractorTrack::Properties>::redo())
		return false;

	// Reopen to assign a probable new bus...
	if (!m_pTrack->open()) {
		mainForm()->appendMessagesError(
			QObject::tr("Track assignment failed:\n\n"
				"Track: \"%1\" Input: \"%2\" Output: \"%3\"")
				.arg(m_pTrack->trackName())
				.arg(m_pTrack->inputBusName())
				.arg(m_pTrack->outputBusName()));
	}

	// Refresh track item, at least the names...
	pTracks->trackList()->updateTrack(m_pTrack);

	// Special MIDI track cases...
	if (m_pTrack->trackType() == qtractorTrack::Midi)
	    pTracks->updateMidiTrack(m_pTrack);

	// Mixer turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer) {
		qtractorMixerStrip *pStrip
			= pMixer->trackRack()->findStrip(m_pTrack->monitor());
		if (pStrip)
			pStrip->setTrack(m_pTrack);
	}
	
	return true;
}


//----------------------------------------------------------------------
// class qtractorTrackButtonCommand - implementation.
//

// Constructor.
qtractorTrackButtonCommand::qtractorTrackButtonCommand (
	qtractorMainForm *pMainForm, qtractorTrackButton *pTrackButton, bool bOn )
	: qtractorTrackCommand(pMainForm, QString::null, pTrackButton->track())
{
	m_toolType = pTrackButton->toolType();
	m_bOn = bOn;

	m_pClipCommand = NULL;
	m_iRecordCount = 0;

	switch (m_toolType) {
	case qtractorTrack::Record:
		qtractorTrackCommand::setName(QObject::tr("track record"));
		break;
	case qtractorTrack::Mute:
		qtractorTrackCommand::setName(QObject::tr("track mute"));
		break;
	case qtractorTrack::Solo:
		qtractorTrackCommand::setName(QObject::tr("track solo"));
		break;
	}

	setRefresh(false);
}

// Destructor.
qtractorTrackButtonCommand::~qtractorTrackButtonCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;
}


// Track-button command method.
bool qtractorTrackButtonCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;
	
	bool bOn = false;	

	switch (m_toolType) {
	case qtractorTrack::Record:
		// Special stuffing if currently recording at first place...
		bOn = pTrack->isRecord();
		if (bOn && !m_bOn
			&& m_pClipCommand == NULL && m_iRecordCount == 0) {
			m_pClipCommand = new qtractorClipCommand(mainForm(), QString::null);
			// Do all the record stuffing here...
			if (m_pClipCommand->addClipRecord(pTrack)) {
				// Yes, we've recorded something...
				setRefresh(true);
			} else {
				// nothing was actually recorded...
				delete m_pClipCommand;
				m_pClipCommand = NULL;
			}
		}
		// Was it before (skip undos)?
		if (m_pClipCommand && (m_iRecordCount % 2) == 0)
			m_pClipCommand->redo();
		m_iRecordCount++;
		// Carry on...
		pTrack->setRecord(m_bOn);
		break;
	case qtractorTrack::Mute:
		bOn = pTrack->isMute();
		pTrack->setMute(m_bOn);
		break;
	case qtractorTrack::Solo:
		bOn = pTrack->isSolo();
		pTrack->setSolo(m_bOn);
		break;
	}
	m_bOn = bOn;

	// Track-list update...
	qtractorTracks *pTracks = mainForm()->tracks();
	if (pTracks)
		pTracks->trackList()->updateTrack(pTrack);

	// Mixer turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer) {
		qtractorMixerStrip *pStrip
			= pMixer->trackRack()->findStrip(pTrack->monitor());
		if (pStrip)
			pStrip->updateTrackButtons();
	}

	return true;
}

bool qtractorTrackButtonCommand::undo (void)
{
	if (m_pClipCommand)
		m_pClipCommand->undo();

	return redo();
}


//----------------------------------------------------------------------
// class qtractorTrackGainCommand - implementation.
//
// Constructor.
qtractorTrackGainCommand::qtractorTrackGainCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack, float fGain )
	: qtractorTrackCommand(pMainForm, "track gain", pTrack)
{
	m_fGain = fGain;
	m_fPrevGain = 1.0f;
	m_bPrevGain = false;

	setRefresh(false);

	// Try replacing an previously equivalent command...
	static qtractorTrackGainCommand *s_pPrevGainCommand = NULL;
	if (s_pPrevGainCommand) {
		qtractorCommand *pLastCommand
			= mainForm()->commands()->lastCommand();
		qtractorCommand *pPrevCommand
			= static_cast<qtractorCommand *> (s_pPrevGainCommand);
		if (pPrevCommand == pLastCommand
			&& s_pPrevGainCommand->track() == pTrack) {
			qtractorTrackGainCommand *pLastGainCommand
				= static_cast<qtractorTrackGainCommand *> (pLastCommand);
			if (pLastGainCommand) {
				// Equivalence means same (sign) direction too...
				float fPrevGain = pLastGainCommand->prevGain();
				float fLastGain = pLastGainCommand->gain();
				int   iPrevSign = (fPrevGain > fLastGain ? +1 : -1);
				int   iCurrSign = (fPrevGain < m_fGain   ? +1 : -1); 
				if (iPrevSign == iCurrSign) {
					m_fPrevGain = fLastGain;
					m_bPrevGain = true;
					mainForm()->commands()->removeLastCommand();
				}
			}
		}
	}
	s_pPrevGainCommand = this;
}


// Track-gain command method.
bool qtractorTrackGainCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	// Set track gain (repective monitor gets set too...)
	float fGain = (m_bPrevGain ? m_fPrevGain : pTrack->gain());	
	pTrack->setGain(m_fGain);
	// MIDI tracks are special...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Now we gotta make sure of proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->outputBus());
		if (pMidiBus)
			pMidiBus->setVolume(pTrack->midiChannel(), m_fGain);
	}

	// Set undo value...
	m_bPrevGain = false;
	m_fPrevGain = m_fGain;
	m_fGain     = fGain;

	// Mixer/Meter turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer) {
		qtractorMixerStrip *pStrip
			= pMixer->trackRack()->findStrip(pTrack->monitor());
		if (pStrip && pStrip->meter())
			pStrip->meter()->updateGain();
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorTrackPanningCommand - implementation.
//

// Constructor.
qtractorTrackPanningCommand::qtractorTrackPanningCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack, float fPanning )
	: qtractorTrackCommand(pMainForm, "track pan", pTrack)
{
	m_fPanning = fPanning;
	m_fPrevPanning = 0.0f;
	m_bPrevPanning = false;

	setRefresh(false);

	// Try replacing an previously equivalent command...
	static qtractorTrackPanningCommand *s_pPrevPanningCommand = NULL;
	if (s_pPrevPanningCommand) {
		qtractorCommand *pLastCommand
			= mainForm()->commands()->lastCommand();
		qtractorCommand *pPrevCommand
			= static_cast<qtractorCommand *> (s_pPrevPanningCommand);
		if (pPrevCommand == pLastCommand
			&& s_pPrevPanningCommand->track() == pTrack) {
			qtractorTrackPanningCommand *pLastPanningCommand
				= static_cast<qtractorTrackPanningCommand *> (pLastCommand);
			if (pLastPanningCommand) {
				// Equivalence means same (sign) direction too...
				float fPrevPanning = pLastPanningCommand->prevPanning();
				float fLastPanning = pLastPanningCommand->panning();
				int   iPrevSign    = (fPrevPanning > fLastPanning ? +1 : -1);
				int   iCurrSign    = (fPrevPanning < m_fPanning   ? +1 : -1); 
				if (iPrevSign == iCurrSign) {
					m_fPrevPanning = fLastPanning;
					m_bPrevPanning = true;
					mainForm()->commands()->removeLastCommand();
				}
			}
		}
	}
	s_pPrevPanningCommand = this;
}


// Track-panning command method.
bool qtractorTrackPanningCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	// Set track panning (repective monitor gets set too...)
	float fPanning = (m_bPrevPanning ? m_fPrevPanning : pTrack->panning());	
	pTrack->setPanning(m_fPanning);
	// MIDI tracks are special...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Now we gotta make sure of proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->outputBus());
		if (pMidiBus)
			pMidiBus->setPanning(pTrack->midiChannel(), m_fPanning);
	}
	
	// Set undo value...
	m_bPrevPanning = false;
	m_fPrevPanning = m_fPanning;
	m_fPanning     = fPanning;

	// Mixer/Meter turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer) {
		qtractorMixerStrip *pStrip
			= pMixer->trackRack()->findStrip(pTrack->monitor());
		if (pStrip && pStrip->meter())
			pStrip->meter()->updatePanning();
	}

	return true;
}


// end of qtractorTrackCommand.cpp
