// qtractorTrackCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioClip.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiControl.h"
#include "qtractorMidiManager.h"
#include "qtractorMidiClip.h"
#include "qtractorMixer.h"

#include "qtractorMonitor.h"
#include "qtractorPlugin.h"
#include "qtractorCurve.h"


//----------------------------------------------------------------------
// class qtractorTrackCommand - implementation
//

// Constructor.
qtractorTrackCommand::qtractorTrackCommand ( const QString& sName,
	qtractorTrack *pTrack, qtractorTrack *pAfterTrack )
	: qtractorCommand(sName), m_pTrack(pTrack), m_pAfterTrack(pAfterTrack)
{
	setClearSelectReset(true);
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
	qDebug("qtractorTrackCommand::addTrack(%p, %p)", m_pTrack, m_pAfterTrack);
#endif

	if (m_pTrack == nullptr)
		return false;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return false;

	qtractorTrackList *pTrackList = pTracks->trackList();
	if (pTrackList == nullptr)
		return false;

	// Guess which item we're adding after...
#if 0
	if (m_pAfterTrack == nullptr)
		m_pAfterTrack = m_pTrack->prev();
#else
	if (m_pAfterTrack == nullptr)
		m_pAfterTrack = pSession->tracks().last();
#endif
	int iTrack = pSession->tracks().find(m_pAfterTrack) + 1;
	// Link the track into session...
	pSession->insertTrack(m_pTrack, m_pAfterTrack);

	// And the new track list view item too...
	iTrack = pTrackList->insertTrack(iTrack, m_pTrack);

	// Update track-list items...
	m_pTrack->updateTrack();

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
	qDebug("qtractorTrackCommand::removeTrack(%p, %p)", m_pTrack, m_pAfterTrack);
#endif

	if (m_pTrack == nullptr)
		return false;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return false;

	qtractorTrackList *pTrackList = pTracks->trackList();
	if (pTrackList == nullptr)
		return false;

	// Save which item we're adding after...
	if (m_pAfterTrack == nullptr)
		m_pAfterTrack = m_pTrack->prev();
#if 0
	if (m_pAfterTrack == nullptr)
		m_pAfterTrack = pSession->tracks().last();
#endif
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
	iTrack = pTrackList->removeTrack(iTrack);

	// Clear track-view clipboard whther applicable...
	if (qtractorTrackView::singleTrackClipboard() == nullptr)
		qtractorTrackView::clearClipboard();

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
	: qtractorTrackCommand(QObject::tr("add track"),
		pTrack, pAfterTrack)
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
	: qtractorTrackCommand(QObject::tr("remove track"),
		pTrack, pTrack->prev())
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
// class qtractorCopyTrackCommand - implementation
//

// Constructor.
qtractorCopyTrackCommand::qtractorCopyTrackCommand (
	qtractorTrack *pTrack, qtractorTrack *pAfterTrack  )
	: qtractorTrackCommand(QObject::tr("duplicate track"),
		pTrack, pAfterTrack), m_iCopyCount(0)
{
}


// Track insertion command methods.
bool qtractorCopyTrackCommand::redo (void)
{
	const bool bResult = addTrack();

	if (++m_iCopyCount > 1)
		return bResult;

	// One-time copy...
	qtractorTrack *pTrack = afterTrack();
	qtractorTrack *pNewTrack = track();

	// Reset state properties...
	pNewTrack->resetProperties();

	// Copy all former clips depending of track type...
	const qtractorTrack::TrackType trackType = pTrack->trackType();
	for (qtractorClip *pClip = pTrack->clips().first();
			pClip; pClip = pClip->next()) {
		switch (trackType) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip
				= static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip) {
				qtractorAudioClip *pNewAudioClip
					= new qtractorAudioClip(*pAudioClip);
				pNewAudioClip->setClipStart(pAudioClip->clipStart());
				pNewAudioClip->setClipOffset(pAudioClip->clipOffset());
				pNewAudioClip->setClipLength(pAudioClip->clipLength());
				pNewAudioClip->setFadeInLength(pAudioClip->fadeInLength());
				pNewAudioClip->setFadeOutLength(pAudioClip->fadeOutLength());
				pNewAudioClip->setPitchShift(pAudioClip->pitchShift());
				pNewAudioClip->setTimeStretch(pAudioClip->timeStretch());
				pNewTrack->addClipEx(pNewAudioClip);
			}
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip) {
				qtractorMidiClip *pNewMidiClip
					= new qtractorMidiClip(*pMidiClip);
				pNewMidiClip->setClipStart(pMidiClip->clipStart());
				pNewMidiClip->setClipOffset(pMidiClip->clipOffset());
				pNewMidiClip->setClipLength(pMidiClip->clipLength());
				pNewMidiClip->setFadeInLength(pMidiClip->fadeInLength());
				pNewMidiClip->setFadeOutLength(pMidiClip->fadeOutLength());
				pNewTrack->addClipEx(pNewMidiClip);
			}
			break;
		}
		default:
			break;
		}
	}

	// Let same old statistics prevail...
	pNewTrack->setMidiNoteMax(pTrack->midiNoteMax());
	pNewTrack->setMidiNoteMin(pTrack->midiNoteMin());

	// About to copy all automation/curves...
	qtractorCurveList *pCurveList = pTrack->curveList();
	qtractorCurveList *pNewCurveList = pNewTrack->curveList();

	// About to find and set current automation/curve...
	qtractorCurve *pCurve, *pCurrentCurve = pTrack->currentCurve();
	qtractorCurve *pNewCurve, *pNewCurrentCurve = nullptr;

	// Copy all former plugins and respective automation/curves...
	qtractorPluginList *pPluginList = pTrack->pluginList();
	qtractorPluginList *pNewPluginList = pNewTrack->pluginList();

	qtractorMidiManager *pMidiManager = nullptr;
	qtractorMidiManager *pNewMidiManager = nullptr;

	if (pPluginList && pNewPluginList) {
		pMidiManager = pPluginList->midiManager();
		pNewMidiManager = pNewPluginList->midiManager();
		for (qtractorPlugin *pPlugin = pPluginList->first();
				pPlugin; pPlugin = pPlugin->next()) {
			// Copy new plugin...
			qtractorPlugin *pNewPlugin = pNewPluginList->copyPlugin(pPlugin);
			if (pNewPlugin == nullptr)
				continue;
			pNewPluginList->insertPlugin(pNewPlugin, nullptr);
			// Activation automation/curves...
			if (pCurveList == nullptr || pNewCurveList == nullptr)
				continue;
			pCurve = pPlugin->activateSubject()->curve();
			if (pCurve && pCurve->list() == pCurveList) {
				pNewCurve = cloneCurve(pNewCurveList,
					pNewPlugin->activateSubject(), pCurve);
				if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
					pNewCurrentCurve = pNewCurve;
			}
			// Copy plugin parameters automation/curves...
			const qtractorPlugin::Params& params = pPlugin->params();
			qtractorPlugin::Params::ConstIterator param = params.constBegin();
			const qtractorPlugin::Params::ConstIterator& param_end
				= params.constEnd();
			for ( ; param != param_end; ++param) {
				qtractorPlugin::Param *pParam = param.value();
				pCurve = pParam->subject()->curve();
				if (pCurve && pCurve->list() == pCurveList) {
					qtractorPlugin::Param *pNewParam
						= pNewPlugin->findParam(pParam->index());
					if (pNewParam == nullptr)
						continue;
					pNewCurve = cloneCurve(pNewCurveList,
						pNewParam->subject(), pCurve);
					if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
						pNewCurrentCurve = pNewCurve;
				}
			}
		}
		// And other MIDI specific plugins-list properties as well
		if (pMidiManager && pNewMidiManager) {
			// The basic ones...
			pNewPluginList->setMidiBank(
				pPluginList->midiBank());
			pNewPluginList->setMidiProg(
				pPluginList->midiProg());
			pNewPluginList->setAudioOutputBusName(
				pPluginList->audioOutputBusName());
			pNewPluginList->setAudioOutputAutoConnect(
				pPluginList->isAudioOutputAutoConnect());
			pNewPluginList->setAudioOutputBus(
				pPluginList->isAudioOutputBus());
			// The effective ones...
			pNewMidiManager->setAudioOutputBusName(
				pMidiManager->audioOutputBusName());
			pNewMidiManager->setAudioOutputAutoConnect(
				pMidiManager->isAudioOutputAutoConnect());
			pNewMidiManager->setAudioOutputBus(
				pMidiManager->isAudioOutputBus());
			pNewMidiManager->setAudioOutputMonitor(
				pMidiManager->isAudioOutputMonitor());
		}
	}

	// Copy all former plugins and respective automation...
	if (pCurveList && pNewCurveList) {
		pCurve = pTrack->monitorSubject()->curve();
		if (pCurve) {
			pNewCurve = cloneCurve(pNewCurveList,
				pNewTrack->monitorSubject(), pCurve);
			if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
				pNewCurrentCurve = pNewCurve;
		}
		pCurve = pTrack->monitor()->panningSubject()->curve();
		if (pCurve) {
			pNewCurve = cloneCurve(pNewCurveList,
				pNewTrack->monitor()->panningSubject(), pCurve);
			if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
				pNewCurrentCurve = pNewCurve;
		}
		pCurve = pTrack->monitor()->gainSubject()->curve();
		if (pCurve) {
			pNewCurve = cloneCurve(pNewCurveList,
				pNewTrack->monitor()->gainSubject(), pCurve);
			if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
				pNewCurrentCurve = pNewCurve;
		}
		pCurve = pTrack->recordSubject()->curve();
		if (pCurve) {
			pNewCurve = cloneCurve(pNewCurveList,
				pNewTrack->recordSubject(), pCurve);
			if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
				pNewCurrentCurve = pNewCurve;
		}
		pCurve = pTrack->muteSubject()->curve();
		if (pCurve) {
			pNewCurve = cloneCurve(pNewCurveList,
				pNewTrack->muteSubject(), pCurve);
			if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
				pNewCurrentCurve = pNewCurve;
		}
		pCurve = pTrack->soloSubject()->curve();
		if (pCurve) {
			pNewCurve = cloneCurve(pNewCurveList,
				pNewTrack->soloSubject(), pCurve);
			if (pNewCurrentCurve == nullptr && pCurrentCurve == pCurve)
				pNewCurrentCurve = pNewCurve;
		}
	}

	// Set final new track properties...
	if (pNewCurrentCurve)
		pNewTrack->setCurrentCurve(pNewCurrentCurve);

	// Update monitor volume/panning for new MIDI track...
	if (trackType == qtractorTrack::Midi) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pNewTrack->outputBus());
		if (pMidiBus) {
			pMidiBus->setVolume(pNewTrack, pNewTrack->gain());
			pMidiBus->setPanning(pNewTrack, pNewTrack->panning());
		}
	}

	// Refresh to most recent things...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		// Meters on tracks list...
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks && pNewMidiManager)
			pTracks->updateMidiTrackItem(pNewMidiManager);
		// Meters on mixer strips...
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer && pNewMidiManager)
			pMixer->updateMidiManagerStrip(pNewMidiManager);
		if (pMixer)
			pMixer->updateTrackStrip(pNewTrack);
	}

	return bResult;
}

bool qtractorCopyTrackCommand::undo (void)
{
	return removeTrack();
}


// Clone an existing automation/curve.
qtractorCurve *qtractorCopyTrackCommand::cloneCurve (
	qtractorCurveList *pNewCurveList, qtractorSubject *pNewSubject,
	qtractorCurve *pCurve ) const
{
	qtractorCurve *pNewCurve = new qtractorCurve(pNewCurveList,
		pNewSubject, pCurve->mode(), pCurve->minFrameDist());

	pNewCurve->setDefaultValue(pCurve->defaultValue());
	pNewCurve->setLength(pCurve->length());

	pNewCurve->setCapture(pCurve->isCapture());
	pNewCurve->setProcess(pCurve->isProcess());
	pNewCurve->setLocked(pCurve->isLocked());

	pNewCurve->setLogarithmic(pCurve->isLogarithmic());
	pNewCurve->setColor(pCurve->color());

	pNewCurve->copyNodes(pCurve);

	return pNewCurve;
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
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
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

	// Swap it nice, finally.
	m_pNextTrack = pNextTrack;

	// Mixer turn...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateTracks(true);

	// Make it new current track (updates mixer too)...
	pTrackList->setCurrentTrackRow(iNextTrack);

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
	qtractorTrack *pTrack = track();
	if (pTrack == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return false;

	// Save the previous item height alright...
	const int iZoomHeight = pTrack->zoomHeight();

	// Just set new one...
	pTrack->setZoomHeight(m_iZoomHeight);

	// Swap it nice, finally.
	m_iZoomHeight = iZoomHeight;

	// Update track list item...
	qtractorTrackList *pTrackList = pMainForm->tracks()->trackList();
	pTrackList->updateTrack(pTrack);

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
	m_pSaveCommand = nullptr;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		m_sessionProps = pSession->properties();
		m_pSaveCommand
			= new qtractorPropertyCommand<qtractorSession::Properties>
				(name(), pSession->properties(), m_sessionProps);
	}

	setClearSelectReset(true);
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

	// Check whether we'll need to re-open the track...
	const qtractorTrack::Properties& old_props = pTrack->properties();
	m_bReopen = (
		old_props.inputBusName  != props.inputBusName  ||
		old_props.outputBusName != props.outputBusName);
	m_bLatency = (
		( old_props.pluginListLatency && !props.pluginListLatency) ||
		(!old_props.pluginListLatency &&  props.pluginListLatency));
}


// Overridden track-edit command methods.
bool qtractorEditTrackCommand::redo (void)
{
	if (m_pTrack == nullptr)
		return false;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return false;

	// Howdy, maybe we're already have a name on recording...
	const bool bRecord = m_pTrack->isRecord();
	if (bRecord)
		pSession->trackRecord(m_pTrack, false, 0, 0);

	// Release track-name from uniqueness...
	pSession->releaseTrackName(m_pTrack);

	// Make the track property change...
	bool bResult = qtractorPropertyCommand<qtractorTrack::Properties>::redo();
	// Reopen to assign a probable new bus, latency...
	if (bResult) {
		if (m_bReopen) {
			pSession->lock();
			bResult = m_pTrack->open();
			pSession->unlock();
		}
		else
		if (m_bLatency && m_pTrack->pluginList()) {
			m_pTrack->pluginList()->setLatency(
				m_pTrack->isPluginListLatency());
		}
	}

	// Re-acquire track-name for uniqueness...
	pSession->acquireTrackName(m_pTrack);

	if (!bResult) {
		pMainForm->appendMessagesError(
			QObject::tr("Track assignment failed:\n\n"
				"Track: \"%1\" Input: \"%2\" Output: \"%3\"")
				.arg(m_pTrack->shortTrackName())
				.arg(m_pTrack->inputBusName())
				.arg(m_pTrack->outputBusName()));
	}
	else // Reassign recording...
	if (bRecord) {
		unsigned long iClipStart = pSession->playHead();
		if (pSession->isPunching()) {
			const unsigned long iPunchIn = pSession->punchIn();
			if (iClipStart < iPunchIn)
				iClipStart = iPunchIn;
		}
		const unsigned long iFrameTime = pSession->frameTimeEx();
		pSession->trackRecord(m_pTrack, true, iClipStart, iFrameTime);
	}

	// Update track-list items...
	m_pTrack->updateTrack();

	// Special MIDI track case: update and trap dirty clips...
	if (m_pTrack->trackType() == qtractorTrack::Midi)
		m_pTrack->updateMidiClips();

	// Mixer turn...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateTracks(true);

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

	m_pClipCommand = nullptr;
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

	// Toggle/update all othera?
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks) {
			const Qt::KeyboardModifiers& modifiers
				= QApplication::keyboardModifiers();
			const bool bAllTracks
				= (modifiers & (Qt::ShiftModifier | Qt::ControlModifier));
			if (modifiers & Qt::ControlModifier)
				bOn = !bOn;
			const QList<qtractorTrack *>& tracks
				= pTracks->trackList()->selectedTracks(track(), bAllTracks);
			QListIterator<qtractorTrack *> iter(tracks);
			while (iter.hasNext())
				m_tracks.append(new TrackItem(iter.next(), bOn));
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
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return false;

	bool bOn = false;
	qtractorMmcEvent::SubCommand scmd = qtractorMmcEvent::TRACK_NONE;
	qtractorMidiControl::Command ccmd = qtractorMidiControl::Command(0);

	switch (m_toolType) {
	case qtractorTrack::Record:
		scmd = qtractorMmcEvent::TRACK_RECORD;
		ccmd = qtractorMidiControl::TRACK_RECORD;
		// Special stuffing if currently recording in first place...
		bOn = pTrack->isRecord();
		if (bOn && !m_bOn
			&& m_pClipCommand == nullptr && m_iRecordCount == 0) {
			m_pClipCommand = new qtractorClipCommand(QString());
			// Do all the record stuffing here...
			const unsigned long iFrameTime = pSession->frameTimeEx();
			if (m_pClipCommand->addClipRecord(pTrack, iFrameTime)) {
				// Yes, we've recorded something...
				setRefresh(true);
			} else {
				// nothing was actually recorded...
				delete m_pClipCommand;
				m_pClipCommand = nullptr;
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
			switch (m_toolType) {
			case qtractorTrack::Record:
				bOn = pTrack->isRecord();
				pTrack->setRecord(pTrackItem->on);
				break;
			case qtractorTrack::Mute:
				bOn = pTrack->isMute();
				pTrack->setMute(pTrackItem->on);
				break;
			case qtractorTrack::Solo:
				bOn = pTrack->isSolo();
				pTrack->setSolo(pTrackItem->on);
				break;
			}
			// Send MMC MASKED_WRITE command...
			iTrack = pTrackList->trackRow(pTrack);
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks) {
			const Qt::KeyboardModifiers& modifiers
				= QApplication::keyboardModifiers();
			const bool bAllTracks
				= (modifiers & (Qt::ShiftModifier | Qt::ControlModifier));
			if (modifiers & Qt::ControlModifier)
				bMonitor = !bMonitor;
			const QList<qtractorTrack *>& tracks
				= pTracks->trackList()->selectedTracks(track(), bAllTracks);
			QListIterator<qtractorTrack *> iter(tracks);
			while (iter.hasNext())
				m_tracks.append(new TrackItem(iter.next(), bMonitor));
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
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return false;

	// Save undo value...
	bool bMonitor = pTrack->isMonitor();

	// Set track monitoring...
	pTrack->setMonitor(m_bMonitor);

	// Send MMC MASKED_WRITE command...
	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	const int iTrack = pSession->tracks().find(pTrack);
	if (pMidiEngine) {
		pMidiEngine->sendMmcMaskedWrite(
			qtractorMmcEvent::TRACK_MONITOR, iTrack, m_bMonitor);
	}

	// Send MIDI controller command(s)...
	if (midiControlFeedback()) {
		qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
		if (pMidiControl) {
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
				const int iTrack = pSession->tracks().find(pTrack);
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
	static qtractorTrackGainCommand *s_pPrevGainCommand = nullptr;
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
				const float fPrevGain = pLastGainCommand->prevGain();
				const float fLastGain = pLastGainCommand->gain();
				const int   iPrevSign = (fPrevGain > fLastGain ? +1 : -1);
				const int   iCurrSign = (fPrevGain < m_fGain   ? +1 : -1);
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
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	// Set undo value...
	const float fGain = m_fPrevGain;

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
			const int iTrack = pSession->tracks().find(pTrack);
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
	static qtractorTrackPanningCommand *s_pPrevPanningCommand = nullptr;
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
				const float fPrevPanning = pLastPanningCommand->prevPanning();
				const float fLastPanning = pLastPanningCommand->panning();
				const int   iPrevSign    = (fPrevPanning > fLastPanning ? +1 : -1);
				const int   iCurrSign    = (fPrevPanning < m_fPanning   ? +1 : -1);
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
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	// Set undo value...
	const float fPanning = m_fPrevPanning;

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
			const int iTrack = pSession->tracks().find(pTrack);
			pMidiControl->processTrackCommand(
				qtractorMidiControl::TRACK_PANNING, iTrack, m_fPanning);
		}
	}

	// Set undo value...
	m_fPrevPanning = m_fPanning;
	m_fPanning     = fPanning;

	return true;
}


//----------------------------------------------------------------------
// class qtractorTrackstrumentCommand - implementation.
//

// Constructor.
qtractorTrackInstrumentCommand::qtractorTrackInstrumentCommand (
	qtractorTrack *pTrack, const QString& sInstrumentName, int iBank, int iProg )
	: qtractorTrackCommand(QObject::tr("track instrument"), pTrack),
		m_sInstrumentName(sInstrumentName), m_iBank(iBank), m_iProg(iProg)
{
	setRefresh(false);
}


// Track-instrument command method.
bool qtractorTrackInstrumentCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == nullptr)
		return false;

	if (pTrack->trackType() != qtractorTrack::Midi)
		return false;

	// Gotta make sure we've a proper MIDI bus...
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	// Set undo values...
	const unsigned short iChannel = pTrack->midiChannel();
	const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
	QString sInstrumentName = patch.instrumentName;
	if (sInstrumentName.isEmpty())
		sInstrumentName = pMidiBus->instrumentName();
	int iBankSelMethod = pTrack->midiBankSelMethod();
	if (iBankSelMethod < 0)
		iBankSelMethod = patch.bankSelMethod;

	const int iBank = pTrack->midiBank();
	const int iProg = pTrack->midiProg();

	// Set instrument patch...
	pMidiBus->setPatch(iChannel, m_sInstrumentName,
		iBankSelMethod, m_iBank, m_iProg, pTrack);

	// Set track instrument...
	pTrack->setMidiBankSelMethod(iBankSelMethod);
	pTrack->setMidiBank(m_iBank);
	pTrack->setMidiProg(m_iProg);

	pTrack->updateMidiTrack();
	pTrack->updateMidiClips();

	// Reset undo values...
	m_sInstrumentName = sInstrumentName;

	m_iBank = iBank;
	m_iProg = iProg;

	// Refresh to most recent things...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer)
			pMixer->updateTrackStrip(pTrack);
	}

	return true;
}


// end of qtractorTrackCommand.cpp
