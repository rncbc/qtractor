// qtractorTrackCommand.cpp
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
#include "qtractorTrackCommand.h"
#include "qtractorClipCommand.h"

#include "qtractorMainForm.h"

#include "qtractorTracks.h"
#include "qtractorTrackList.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiControl.h"
#include "qtractorMidiClip.h"
#include "qtractorMixer.h"


//----------------------------------------------------------------------
// class qtractorTrackCommand - implementation
//

// Constructor.
qtractorTrackCommand::qtractorTrackCommand ( const QString& sName,
	qtractorTrack *pTrack ) : qtractorCommand(sName), m_pTrack(pTrack)
{
	setClearSelect(true);
}


// Destructor.
qtractorTrackCommand::~qtractorTrackCommand (void)
{
	if (isAutoDelete())
		delete m_pTrack;
}


// Track command methods.
bool qtractorTrackCommand::addTrack ( qtractorTrack *pAfterTrack )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTrackCommand::addTrack(%p, %p)", m_pTrack, pAfterTrack);
#endif

	if (m_pTrack == NULL)
		return false;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return false;

	// Guess which item we're adding after...
	if (pAfterTrack == NULL)
		pAfterTrack = m_pTrack->prev();
	if (pAfterTrack == NULL)
		pAfterTrack = pSession->tracks().last();
	int iTrack = pSession->tracks().find(pAfterTrack) + 1;
	// Link the track into session...
	pSession->insertTrack(m_pTrack, pAfterTrack);
	// And the new track list view item too...
	qtractorTrackList *pTrackList = pTracks->trackList();
	iTrack = pTrackList->insertTrack(iTrack, m_pTrack);
	// Special MIDI track cases...
	if (m_pTrack->trackType() == qtractorTrack::Midi)
	    pTracks->updateMidiTrack(m_pTrack);

	// (Re)open all clips...
	qtractorClip *pClip = m_pTrack->clips().first();
	for ( ; pClip; pClip = pClip->next())
		pClip->open();

	// Mixer turn...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateTracks(true);

	// Let the change get visible.
	pTrackList->setCurrentTrackRow(iTrack);

	// ATTN: MIDI controller map feedback.
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl)
		pMidiControl->sendAllControllers(iTrack);

	// Avoid disposal of the track reference.
	setAutoDelete(false);

	return true;
}


bool qtractorTrackCommand::removeTrack (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTrackCommand::removeTrack(%p)", m_pTrack);
#endif

	if (m_pTrack == NULL)
		return false;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return false;

	// Get the list view item reference of the intended track...
	int iTrack = pSession->tracks().find(m_pTrack);
	if (iTrack < 0)
		return false;

	// Close all clips...
	qtractorClip *pClip = m_pTrack->clips().last();
	for ( ; pClip; pClip = pClip->prev())
		pClip->close();

	// Second, remove from session...
	pSession->unlinkTrack(m_pTrack);

	// Third, remove track from list view...
	qtractorTrackList *pTrackList = pTracks->trackList();
	iTrack = pTrackList->removeTrack(iTrack);
	if (iTrack >= 0)
		pTrackList->setCurrentTrackRow(iTrack);

	// Mixer turn...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateTracks();

	// ATTN: MIDI controller map feedback.
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl)
		pMidiControl->sendAllControllers(iTrack);

	// Make ths track reference disposable.
	setAutoDelete(true);

	return true;
}


//----------------------------------------------------------------------
// class qtractorAddTrackCommand - implementation
//

// Constructor.
qtractorAddTrackCommand::qtractorAddTrackCommand (
	qtractorTrack *pTrack, qtractorTrack *pAfterTrack  )
	: qtractorTrackCommand(QObject::tr("add track"), pTrack),
		m_pAfterTrack(pAfterTrack)
{
}


// Track insertion command methods.
bool qtractorAddTrackCommand::redo (void)
{
	return addTrack(m_pAfterTrack);
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
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return false;

	int iTrack = pSession->tracks().find(pTrack);
	if (iTrack < 0)
	    return false;

	// Save the next track alright...
	qtractorTrack *pNextTrack = pTrack->next();

	// Remove and insert back again...
	qtractorTrackList *pTrackList = pTracks->trackList();
	pTrackList->removeTrack(iTrack);
	// Get actual index of new position...
	int iNextTrack = pTrackList->trackRow(m_pNextTrack);
	// Make it all set back.
	pSession->moveTrack(pTrack, m_pNextTrack);
	// Just insert under the track list position...
	// We'll renumber all items now...
	iNextTrack = pTrackList->insertTrack(iNextTrack, pTrack);
	if (iNextTrack >= 0)
		pTrackList->setCurrentTrackRow(iNextTrack);

	// Swap it nice, finally.
	m_pNextTrack = pNextTrack;

	// Mixer turn...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateTracks(true);

	// ATTN: MIDI controller map feedback.
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl) {
		if (iTrack > iNextTrack)
			iTrack = iNextTrack;
		pMidiControl->sendAllControllers(iTrack);
	}

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
qtractorImportTrackCommand::qtractorImportTrackCommand (
	qtractorTrack *pAfterTrack )
	: qtractorCommand(QObject::tr("import track")),
		m_pAfterTrack(pAfterTrack)
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

	setClearSelect(true);
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
		new qtractorAddTrackCommand(pTrack, m_pAfterTrack));

	m_pAfterTrack = pTrack;
}


// Track-import command methods.
bool qtractorImportTrackCommand::redo (void)
{
	bool bResult = true;

	if (m_pSaveCommand && m_iSaveCount > 0) {
		if (!m_pSaveCommand->redo())
		    bResult = false;
	}
	++m_iSaveCount;

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
	iter.toBack();
	while (iter.hasPrevious()) {
		qtractorAddTrackCommand *pTrackCommand = iter.previous();
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
	if (m_pTrack == NULL)
		return false;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Howdy, maybe we're already have a name on recording...
	bool bRecord = m_pTrack->isRecord();
	if (bRecord)
		pSession->trackRecord(m_pTrack, false, 0, 0);

	// Trap dirty clips (only MIDI at this time...)
	if (m_pTrack->trackType() == qtractorTrack::Midi) {
		for (qtractorClip *pClip = m_pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			// Are any dirty changes pending commit?
			if (pClip->isDirty()) {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (pClip);
				if (pMidiClip)
					pMidiClip->saveCopyFile(true);
			}
		}
	}

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
	if (bRecord) {
		unsigned long iClipStart = pSession->playHead();
		if (pSession->isPunching()) {
			unsigned long iPunchIn = pSession->punchIn();
			if (iClipStart < iPunchIn)
				iClipStart = iPunchIn;
		}
		unsigned long iFrameTime = pSession->frameTimeEx();
		pSession->trackRecord(m_pTrack, true, iClipStart, iFrameTime);
	}

	// Refresh track item, at least the names...
	m_pTrack->updateTracks();

	// Special MIDI track cases...
	if (m_pTrack->trackType() == qtractorTrack::Midi) {
		// Re-open all MIDI clips (channel might have changed?)...
		qtractorClip *pClip = m_pTrack->clips().first();
		for ( ; pClip; pClip = pClip->next())
			pClip->open();
	}

	// Finally update any outstanding clip editors...
	m_pTrack->updateClipEditors();

	return bResult;
}

bool qtractorEditTrackCommand::undo (void)
{
	return redo();
}


//----------------------------------------------------------------------
// class qtractorTrackControlCommand - implementation.
//

// Constructor.
qtractorTrackControlCommand::qtractorTrackControlCommand (
	const QString& sName, qtractorTrack *pTrack, bool bMidiControl )
	: qtractorTrackCommand(sName, pTrack),
		m_bMidiControl(bMidiControl), m_iMidiControlFeedback(0)
{
}


// Primitive control predicate.
bool qtractorTrackControlCommand::midiControlFeedback (void)
{
	return (m_bMidiControl ? (++m_iMidiControlFeedback > 1) : true);
}


//----------------------------------------------------------------------
// class qtractorTrackStateCommand - implementation.
//

// Constructor.
qtractorTrackStateCommand::qtractorTrackStateCommand ( qtractorTrack *pTrack,
	qtractorTrack::ToolType toolType, bool bOn, bool bMidiControl )
	: qtractorTrackControlCommand(QString(), pTrack, bMidiControl)
{
	m_toolType = toolType;
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

	setRefresh(m_toolType != qtractorTrack::Record);
}

// Destructor.
qtractorTrackStateCommand::~qtractorTrackStateCommand (void)
{
	if (m_pClipCommand)
		delete m_pClipCommand;

	qDeleteAll(m_tracks);
}


// Track-button command method.
bool qtractorTrackStateCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	bool bOn = false;
	qtractorMmcEvent::SubCommand scmd = qtractorMmcEvent::TRACK_NONE;
	qtractorMidiControl::Command ccmd = qtractorMidiControl::Command(0);

	switch (m_toolType) {
	case qtractorTrack::Record:
		scmd = qtractorMmcEvent::TRACK_RECORD;
		ccmd = qtractorMidiControl::TRACK_RECORD;
		// Special stuffing if currently recording at first place...
		bOn = pTrack->isRecord();
		if (bOn && !m_bOn
			&& m_pClipCommand == NULL && m_iRecordCount == 0) {
			m_pClipCommand = new qtractorClipCommand(QString());
			// Do all the record stuffing here...
			unsigned long iFrameTime = pSession->frameTimeEx();
			if (m_pClipCommand->addClipRecord(pTrack, iFrameTime)) {
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
		++m_iRecordCount;
		// Carry on...
		pTrack->setRecord(m_bOn);
		break;
	case qtractorTrack::Mute:
		scmd = qtractorMmcEvent::TRACK_MUTE;
		ccmd = qtractorMidiControl::TRACK_MUTE;
		bOn = pTrack->isMute();
		pTrack->setMute(m_bOn);
		break;
	case qtractorTrack::Solo:
		scmd = qtractorMmcEvent::TRACK_SOLO;
		ccmd = qtractorMidiControl::TRACK_SOLO;
		bOn = pTrack->isSolo();
		pTrack->setSolo(m_bOn);
		break;
	default:
		// Whaa?
		return false;
	}

	// Send MMC MASKED_WRITE command...
	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	int iTrack = pSession->tracks().find(pTrack);
	if (pMidiEngine)
		pMidiEngine->sendMmcMaskedWrite(scmd, iTrack, m_bOn);

	// Send MIDI controller command...
	if (midiControlFeedback()) {
		qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
		if (pMidiControl)
			pMidiControl->processTrackCommand(ccmd, iTrack, m_bOn);
	}

	// Update track list item...
	qtractorTrackList *pTrackList = pMainForm->tracks()->trackList();
	pTrackList->updateTrack(pTrack);

	// Reset for undo.
	m_bOn = bOn;

	// Toggle/update all other?
	if (!m_tracks.isEmpty()) {
		// Exclusive mode.
		qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
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
			int iTrack = pTrackList->trackRow(pTrack);
			if (pMidiEngine)
				pMidiEngine->sendMmcMaskedWrite(scmd, iTrack, pTrackItem->on);
			// Send MIDI controller command...
			if (pMidiControl)
				pMidiControl->processTrackCommand(ccmd, iTrack, pTrackItem->on);
			// Update track list item...
			pTrackList->updateTrack(pTrack);
			// Swap for undo...
			pTrackItem->on = bOn;
		}
		// Done with exclusive mode.
	}

	return true;
}

bool qtractorTrackStateCommand::undo (void)
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
	qtractorTrack *pTrack, bool bMonitor, bool bMidiControl )
	: qtractorTrackControlCommand(
		QObject::tr("track monitor"), pTrack, bMidiControl)
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
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Save undo value...
	bool bMonitor = pTrack->isMonitor();

	// Set track monitoring...
	pTrack->setMonitor(m_bMonitor);

	// Send MIDI controller command(s)...
	if (midiControlFeedback()) {
		qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
		if (pMidiControl) {
			int iTrack = pSession->tracks().find(pTrack);
			pMidiControl->processTrackCommand(
				qtractorMidiControl::TRACK_MONITOR, iTrack, m_bMonitor);
		}
	}

	// Set undo value...
	m_bMonitor = bMonitor;

	// Toggle/update all other?
	if (!m_tracks.isEmpty()) {
		// Exclusive mode.
		qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
		QListIterator<TrackItem *> iter(m_tracks);
		while (iter.hasNext()) {
			TrackItem *pTrackItem = iter.next();
			pTrack = pTrackItem->track;
			bMonitor = pTrack->isMonitor();
			pTrack->setMonitor(pTrackItem->on);
			// Send MIDI controller command...
			if (pMidiControl) {
				int iTrack = pSession->tracks().find(pTrack);
				pMidiControl->processTrackCommand(
					qtractorMidiControl::TRACK_MONITOR, iTrack, pTrackItem->on);
			}
			// Swap for undo...
			pTrackItem->on = bMonitor;
		}
		// Done with exclusive mode.
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorTrackGainCommand - implementation.
//
// Constructor.
qtractorTrackGainCommand::qtractorTrackGainCommand (
	qtractorTrack *pTrack, float fGain, bool bMidiControl )
	: qtractorTrackControlCommand(
		QObject::tr("track gain"), pTrack, bMidiControl)
{
	m_fGain = fGain;
	m_fPrevGain = pTrack->prevGain();

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
				if (iPrevSign == iCurrSign || m_fGain == m_fPrevGain) {
					m_fPrevGain = fLastGain;
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

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	// Set undo value...
	float fGain = m_fPrevGain;

	// Set track gain (respective monitor gets set too...)
	pTrack->setGain(m_fGain);
#if 0
	// MIDI tracks are special...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Gotta make sure we've a proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->outputBus());
		if (pMidiBus)
			pMidiBus->setVolume(pTrack, m_fGain);
	}
#endif
	// Send MIDI controller command(s)...
	if (midiControlFeedback()) {
		qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
		if (pMidiControl) {
			int iTrack = pSession->tracks().find(pTrack);
			pMidiControl->processTrackCommand(
				qtractorMidiControl::TRACK_GAIN, iTrack, m_fGain,
				pTrack->trackType() == qtractorTrack::Audio);
		}
	}

	// Set undo value...
	m_fPrevGain = m_fGain;
	m_fGain     = fGain;

	return true;
}


//----------------------------------------------------------------------
// class qtractorTrackPanningCommand - implementation.
//

// Constructor.
qtractorTrackPanningCommand::qtractorTrackPanningCommand (
	qtractorTrack *pTrack, float fPanning, bool bMidiControl )
	: qtractorTrackControlCommand(
		QObject::tr("track pan"), pTrack, bMidiControl)
{
	m_fPanning = fPanning;
	m_fPrevPanning = pTrack->prevPanning();

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
				if (iPrevSign == iCurrSign || m_fPanning == m_fPrevPanning) {
					m_fPrevPanning = fLastPanning;
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

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	// Set undo value...
	float fPanning = m_fPrevPanning;

	// Set track panning (respective monitor gets set too...)
	pTrack->setPanning(m_fPanning);

	// MIDI tracks are special...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Gotta make sure we've a proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->outputBus());
		if (pMidiBus)
			pMidiBus->setPanning(pTrack, m_fPanning);
	}

	// Send MIDI controller command(s)...
	if (midiControlFeedback()) {
		qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
		if (pMidiControl) {
			int iTrack = pSession->tracks().find(pTrack);
			pMidiControl->processTrackCommand(
				qtractorMidiControl::TRACK_PANNING, iTrack, m_fPanning);
		}
	}

	// Set undo value...
	m_fPrevPanning = m_fPanning;
	m_fPanning     = fPanning;

	return true;
}


// end of qtractorTrackCommand.cpp
