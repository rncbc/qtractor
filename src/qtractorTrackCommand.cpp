// qtractorTrackCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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
qtractorTrackCommand::qtractorTrackCommand ( const QString& sName,
	qtractorTrack *pTrack ) : qtractorCommand(sName), m_pTrack(pTrack)
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
	qDebug("qtractorTrackCommand::addTrack(%p)", m_pTrack);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
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
	iTrack = pTracks->trackList()->insertTrack(iTrack, m_pTrack);
	if (iTrack >= 0)
		pTracks->trackList()->setCurrentTrackRow(iTrack);
	// Special MIDI track cases...
	if (m_pTrack->trackType() == qtractorTrack::Midi)
	    pTracks->updateMidiTrack(m_pTrack);

	// Mixer turn...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateTracks();

	// Avoid disposal of the track reference.
	setAutoDelete(false);

	return true;
}


bool qtractorTrackCommand::removeTrack (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTrackCommand::removeTrack(%p)", m_pTrack);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return false;

	// Get the list view item reference of the intended track...
	int iTrack = pTracks->trackList()->trackRow(m_pTrack);
	if (iTrack < 0)
		return false;

	// Second, remove from session...
	pSession->unlinkTrack(m_pTrack);
	// Third, remove track from list view...
	iTrack = pTracks->trackList()->removeTrack(iTrack);
	if (iTrack >= 0)
		pTracks->trackList()->setCurrentTrackRow(iTrack);

	// Mixer turn...
	qtractorMixer *pMixer = pMainForm->mixer();
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
qtractorAddTrackCommand::qtractorAddTrackCommand ( qtractorTrack *pTrack )
	: qtractorTrackCommand(QObject::tr("add track"), pTrack)
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
qtractorRemoveTrackCommand::qtractorRemoveTrackCommand ( qtractorTrack *pTrack )
	: qtractorTrackCommand(QObject::tr("remove track"), pTrack)
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
	qtractorTrack *pTrack, qtractorTrack *pNextTrack )
	: qtractorTrackCommand(QObject::tr("move track"), pTrack)
{
	m_pNextTrack = pNextTrack;
}


// Track-move command methods.
bool qtractorMoveTrackCommand::redo (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return false;

	int iTrack = pTracks->trackList()->trackRow(track());
	if (iTrack < 0)
	    return false;

	// Save the next track alright...
	qtractorTrack *pTrack = track();
	qtractorTrack *pNextTrack = pTrack->next();

	// Remove and insert back again...
	pTracks->trackList()->removeTrack(iTrack);
	// Get actual index of new position...
	int iNextTrack = pTracks->trackList()->trackRow(m_pNextTrack);
	// Make it all set back.
	pSession->moveTrack(pTrack, m_pNextTrack);
	// Just insert under the track list position...
	// We'll renumber all items now...
	iNextTrack = pTracks->trackList()->insertTrack(iNextTrack, pTrack);
	if (iNextTrack >= 0)
		pTracks->trackList()->setCurrentTrackRow(iNextTrack);

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
	qtractorTrack *pTrack, int iZoomHeight )
	: qtractorTrackCommand(QObject::tr("resize track"), pTrack)
{
	m_iZoomHeight = iZoomHeight;
}


// Track-resize command methods.
bool qtractorResizeTrackCommand::redo (void)
{
	// Save the previous item height alright...
	int iZoomHeight = track()->zoomHeight();

	// Just set new one...
	track()->setZoomHeight(m_iZoomHeight);

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
qtractorImportTrackCommand::qtractorImportTrackCommand (void)
	: qtractorCommand(QObject::tr("import track"))
{
	// Session properties backup preparation.
	m_iSaveCount   = 0;
	m_pSaveCommand = NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		m_sessionProps = pSession->properties();
		m_pSaveCommand
			= new qtractorPropertyCommand<qtractorSession::Properties>
				(name(), pSession->properties(), m_sessionProps);
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
		new qtractorAddTrackCommand(pTrack));
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
	qtractorTrack *pTrack, const qtractorTrack::Properties& props )
	: qtractorPropertyCommand<qtractorTrack::Properties> (
		QObject::tr("track properties"), pTrack->properties(), props)
{
	m_pTrack = pTrack;
}


// Overridden track-edit command methods.
bool qtractorEditTrackCommand::redo (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return false;

	// Howdy, maybe we're already have a name on recording...
	bool bRecord = m_pTrack->isRecord();
	if (bRecord)
		pSession->trackRecord(m_pTrack, false);

	// Make the track property change...
	bool bResult = qtractorPropertyCommand<qtractorTrack::Properties>::redo();
	// Reopen to assign a probable new bus...
	if (bResult)
		bResult = m_pTrack->open();

	if (!bResult) {
		pMainForm->appendMessagesError(
			QObject::tr("Track assignment failed:\n\n"
				"Track: \"%1\" Input: \"%2\" Output: \"%3\"")
				.arg(m_pTrack->trackName())
				.arg(m_pTrack->inputBusName())
				.arg(m_pTrack->outputBusName()));
	}
	else // Reassign recording...
	if (bRecord)
		pSession->trackRecord(m_pTrack, true);

	// Refresh track item, at least the names...
	pTracks->trackList()->updateTrack(m_pTrack);

	// Special MIDI track cases...
	if (m_pTrack->trackType() == qtractorTrack::Midi)
	    pTracks->updateMidiTrack(m_pTrack);

	// Finally update any outstanding clip editors...
	m_pTrack->updateClipEditors();

	return bResult;
}

bool qtractorEditTrackCommand::undo (void)
{
	return redo();
}


//----------------------------------------------------------------------
// class qtractorTrackButtonCommand - implementation.
//

// Constructor.
qtractorTrackButtonCommand::qtractorTrackButtonCommand (
	qtractorTrackButton *pTrackButton, bool bOn )
	: qtractorTrackCommand(QString::null, pTrackButton->track())
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

	// Toggle/update all other?
	Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
	if (m_toolType != qtractorTrack::Record
		&& (modifiers & (Qt::ShiftModifier | Qt::ControlModifier))) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) {
			if (modifiers & Qt::ControlModifier)
				bOn = !bOn;
			for (qtractorTrack *pTrackEx = pSession->tracks().first();
					pTrackEx; pTrackEx = pTrackEx->next()) {
				if (pTrackEx != track())
					m_tracks.append(new TrackItem(pTrackEx, bOn));
			}
		}
	}

	setRefresh(false);
}

// Destructor.
qtractorTrackButtonCommand::~qtractorTrackButtonCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;

	qDeleteAll(m_tracks);
}


// Track-button command method.
bool qtractorTrackButtonCommand::redo (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	bool bOn = false;
	qtractorMmcEvent::SubCommand scmd = qtractorMmcEvent::TRACK_NONE;

	switch (m_toolType) {
	case qtractorTrack::Record:
		scmd = qtractorMmcEvent::TRACK_RECORD;
		// Special stuffing if currently recording at first place...
		bOn = pTrack->isRecord();
		if (bOn && !m_bOn
			&& m_pClipCommand == NULL && m_iRecordCount == 0) {
			m_pClipCommand = new qtractorClipCommand(QString::null);
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
		scmd = qtractorMmcEvent::TRACK_MUTE;
		bOn = pTrack->isMute();
		pTrack->setMute(m_bOn);
		break;
	case qtractorTrack::Solo:
		scmd = qtractorMmcEvent::TRACK_SOLO;
		bOn = pTrack->isSolo();
		pTrack->setSolo(m_bOn);
		break;
	}

	// Track-list and mixer update...
	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	qtractorTrackList  *pTrackList  = pMainForm->tracks()->trackList();

	// Send MMC MASKED_WRITE command...
	pMidiEngine->sendMmcMaskedWrite(scmd,
		pTrackList->trackRow(pTrack), m_bOn);
	// Update track list item...
	pTrackList->updateTrack(pTrack);

	// Reset for undo.
	m_bOn = bOn;

	// Toggle/update all other?
	qtractorMixerRack *pTrackRack = pMainForm->mixer()->trackRack();
	if (m_tracks.isEmpty()) {
		// Update one track mixer strip...
		qtractorMixerStrip *pStrip
			= pTrackRack->findStrip(pTrack->monitor());
		if (pStrip)
			pStrip->updateTrackButtons();
		// Done with single mode.
	} else {
		// Exclusive mode.
		QListIterator<TrackItem *> iter(m_tracks);
		while (iter.hasNext()) {
			TrackItem *pTrackItem = iter.next();
			pTrack = pTrackItem->track;
			bOn = false;
			if (m_toolType == qtractorTrack::Mute) {
				bOn = pTrack->isMute();
				pTrack->setMute(pTrackItem->on);
			}
			else
			if (m_toolType == qtractorTrack::Solo) {
				bOn = pTrack->isSolo();
				pTrack->setSolo(pTrackItem->on);
			}
			// Send MMC MASKED_WRITE command...
			pMidiEngine->sendMmcMaskedWrite(scmd,
				pTrackList->trackRow(pTrack), pTrackItem->on);
			// Update track list item...
			pTrackList->updateTrack(pTrack);
			// Swap for undo...
			pTrackItem->on = bOn;
		}
		// Update all track mixer strips...
		pTrackRack->updateTrackButtons();
		// Done with exclusive mode.
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
// class qtractorTrackMonitorCommand - implementation.
//

// Constructor.
qtractorTrackMonitorCommand::qtractorTrackMonitorCommand (
	qtractorTrack *pTrack, bool bMonitor )
	: qtractorTrackCommand(QObject::tr("track monitor"), pTrack)
{
	m_bMonitor = bMonitor;

	// Toggle/update all other?
	Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
	if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) {
			if (modifiers & Qt::ControlModifier)
				bMonitor = !bMonitor;
			for (qtractorTrack *pTrackEx = pSession->tracks().first();
					pTrackEx; pTrackEx = pTrackEx->next()) {
				if (pTrackEx != pTrack)
					m_tracks.append(new TrackItem(pTrackEx, bMonitor));
			}
		}
	}

	setRefresh(false);
}


// Destructor.
qtractorTrackMonitorCommand::~qtractorTrackMonitorCommand (void)
{
	qDeleteAll(m_tracks);
}


// Track-monitor command method.
bool qtractorTrackMonitorCommand::redo (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	// Save undo value...
	bool bMonitor = pTrack->isMonitor();
	// Set track monitoring...
	pTrack->setMonitor(m_bMonitor);
	// Set undo value...
	m_bMonitor = bMonitor;

	// Toggle/update all other?
	qtractorMixerRack *pTrackRack = pMainForm->mixer()->trackRack();
	if (m_tracks.isEmpty()) {
		// Update one track mixer strip...
		qtractorMixerStrip *pStrip
			= pTrackRack->findStrip(pTrack->monitor());
		if (pStrip)
			pStrip->updateMonitorButton();
		// Done with single mode.
	} else {
		// Exclusive mode.
		QListIterator<TrackItem *> iter(m_tracks);
		while (iter.hasNext()) {
			TrackItem *pTrackItem = iter.next();
			pTrack = pTrackItem->track;
			bMonitor = pTrack->isMonitor();
			pTrack->setMonitor(pTrackItem->on);
			// Swap for undo...
			pTrackItem->on = bMonitor;
		}
		// Update all track mixer strips...
		pTrackRack->updateMonitorButtons();
		// Done with exclusive mode.
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorTrackGainCommand - implementation.
//
// Constructor.
qtractorTrackGainCommand::qtractorTrackGainCommand (
	qtractorTrack *pTrack, float fGain )
	: qtractorTrackCommand(QObject::tr("track gain"), pTrack)
{
	m_fGain = fGain;
	m_fPrevGain = 1.0f;
	m_bPrevGain = false;

	setRefresh(false);

	// Try replacing an previously equivalent command...
	static qtractorTrackGainCommand *s_pPrevGainCommand = NULL;
	if (s_pPrevGainCommand) {
		qtractorSession *pSession = qtractorSession::getInstance();
		qtractorCommand *pLastCommand
			= (pSession->commands())->lastCommand();
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
					(pSession->commands())->removeLastCommand();
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
			pMidiBus->setVolume(pTrack, m_fGain);
	}

	// Set undo value...
	m_bPrevGain = false;
	m_fPrevGain = m_fGain;
	m_fGain     = fGain;

	// Mixer/Meter turn...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer) {
			qtractorMixerStrip *pStrip
				= pMixer->trackRack()->findStrip(pTrack->monitor());
			if (pStrip && pStrip->meter())
				pStrip->meter()->updateGain();
		}
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorTrackPanningCommand - implementation.
//

// Constructor.
qtractorTrackPanningCommand::qtractorTrackPanningCommand (
	qtractorTrack *pTrack, float fPanning )
	: qtractorTrackCommand(QObject::tr("track pan"), pTrack)
{
	m_fPanning = fPanning;
	m_fPrevPanning = 0.0f;
	m_bPrevPanning = false;

	setRefresh(false);

	// Try replacing an previously equivalent command...
	static qtractorTrackPanningCommand *s_pPrevPanningCommand = NULL;
	if (s_pPrevPanningCommand) {
		qtractorSession *pSession = qtractorSession::getInstance();
		qtractorCommand *pLastCommand
			= (pSession->commands())->lastCommand();
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
					(pSession->commands())->removeLastCommand();
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
			pMidiBus->setPanning(pTrack, m_fPanning);
	}
	
	// Set undo value...
	m_bPrevPanning = false;
	m_fPrevPanning = m_fPanning;
	m_fPanning     = fPanning;

	// Mixer/Meter turn...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer) {
			qtractorMixerStrip *pStrip
				= pMixer->trackRack()->findStrip(pTrack->monitor());
			if (pStrip && pStrip->meter())
				pStrip->meter()->updatePanning();
		}
	}

	return true;
}


// end of qtractorTrackCommand.cpp
