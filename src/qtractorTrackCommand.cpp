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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorTrackCommand.h"

#include "qtractorMainForm.h"

#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorTrackList.h"
#include "qtractorTrackView.h"
#include "qtractorTrackButton.h"
#include "qtractorMidiEngine.h"
#include "qtractorMixer.h"
#include "qtractorMeter.h"

#include <qlabel.h>


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
	qtractorTrackListItem *pAfterItem
		= pTracks->trackList()->trackItem(pAfterTrack);
	// Link the track into session...
	pSession->insertTrack(m_pTrack, pAfterTrack);
	// And the new track list view item too...
	qtractorTrackListItem *pTrackItem
		= new qtractorTrackListItem(pTracks->trackList(), m_pTrack, pAfterItem);
	// Renumbering of all other remaining items.
	pTracks->trackList()->renumberTrackItems(pAfterItem);
	// Special MIDI track cases...
	if (m_pTrack->trackType() == qtractorTrack::Midi)
	    pTracks->updateMidiTrack(m_pTrack);
	// Make it the current item...
	pTracks->trackList()->setCurrentItem(pTrackItem);

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
	qtractorTrackListItem *pTrackItem
		= pTracks->trackList()->trackItem(m_pTrack);
	if (pTrackItem == NULL)
		return false;

	// First, make it unselected, avoiding excessive signals.
	pTrackItem->setSelected(false);
	// Second, remove from session...
	pSession->unlinkTrack(m_pTrack);
	// Third, remove track from list view...
	delete pTrackItem;
	// OK. Finally, force enumbering of all items.
	pTracks->trackList()->renumberTrackItems();

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
		qtractorTrack *pPrevTrack )
	: qtractorTrackCommand(pMainForm, QObject::tr("move track"), pTrack)
{
	m_pPrevTrack = pPrevTrack;
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

	qtractorTrackListItem *pPrevItem
	    = pTracks->trackList()->trackItem(m_pPrevTrack);
//	if (pPrevItem == NULL)
//	    return false;
	qtractorTrackListItem *pTrackItem
	    = pTracks->trackList()->trackItem(track());
	if (pTrackItem == NULL)
	    return false;

	// Save the previous track alright...
	qtractorTrack *pPrevTrack = track()->prev();

	// Remove and insert back again...
	pSession->tracks().unlink(track());
	delete pTrackItem;
	if (m_pPrevTrack)
		pSession->tracks().insertAfter(track(), m_pPrevTrack);
	else
		pSession->tracks().prepend(track());
	// Make it all set back.
	pSession->reset();
	// Just insert under the track list position...
	pTrackItem = new qtractorTrackListItem(pTracks->trackList(),
		track(), pPrevItem);
	// We'll renumber all items now...
	pTracks->trackList()->renumberTrackItems();
	// Make it the current item...
	pTracks->trackList()->setCurrentItem(pTrackItem);

	// Swap it nice, finally.
	m_pPrevTrack = pPrevTrack;

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
	qtractorMainForm *pMainForm, qtractorTrack *pTrack,	int iItemHeight )
	: qtractorTrackCommand(pMainForm, QObject::tr("resize track"), pTrack)
{
	m_iItemHeight = iItemHeight;
}


// Track-resize command methods.
bool qtractorResizeTrackCommand::redo (void)
{
	qtractorTracks *pTracks = mainForm()->tracks();
	if (pTracks == NULL)
		return false;

	qtractorTrackListItem *pTrackItem
	    = pTracks->trackList()->trackItem(track());
	if (pTrackItem == NULL)
	    return false;

	// Save the previous item height alright...
	int iItemHeight = pTrackItem->height();

	// Just set new one...
	pTrackItem->setItemHeight(m_iItemHeight);

	// Swap it nice, finally.
	m_iItemHeight = iItemHeight;

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
	m_trackCommands.setAutoDelete(true);
	
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

	for (qtractorAddTrackCommand *pTrackCommand = m_trackCommands.first();
	        pTrackCommand; pTrackCommand = m_trackCommands.next()) {
		if (!pTrackCommand->redo())
		    bResult = false;
	}

	return bResult;
}

bool qtractorImportTrackCommand::undo (void)
{
	bool bResult = true;

	for (qtractorAddTrackCommand *pTrackCommand = m_trackCommands.last();
	        pTrackCommand; pTrackCommand = m_trackCommands.prev()) {
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

	qtractorTrackListItem *pTrackItem
	    = pTracks->trackList()->trackItem(m_pTrack);
	if (pTrackItem == NULL)
	    return false;

	// Do the property dance.
	if (!qtractorPropertyCommand<qtractorTrack::Properties>::redo())
		return false;

	// Reopen to assign a probable new bus,
	// which is certainly unchanged on record mode...
	if (!m_pTrack->isRecord() && !m_pTrack->open()) {
		mainForm()->appendMessagesError(
			QObject::tr("Track assignment failed:\n\n"
				"Track: \"%1\" Bus: \"%2\"")
				.arg(m_pTrack->trackName())
				.arg(m_pTrack->busName()));
	}

	// Refresh track item, at least the names...
	pTrackItem->setText(qtractorTrackList::Name, m_pTrack->trackName());
	pTrackItem->setText(qtractorTrackList::Bus,  m_pTrack->busName());
	pTrackItem->repaint();

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


// Track-button command method.
bool qtractorTrackButtonCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;
	
	bool bOn = false;	

	switch (m_toolType) {
	case qtractorTrack::Record:
		bOn = pTrack->isRecord();
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
	if (pTracks) {
		qtractorTrackListItem *pTrackItem
			= pTracks->trackList()->trackItem(pTrack);
		if (pTrackItem)
			pTrackItem->updateTrackButtons();
	}

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


//----------------------------------------------------------------------
// class qtractorTrackGainCommand - implementation.
//

// Constructor.
qtractorTrackGainCommand::qtractorTrackGainCommand (
	qtractorMainForm *pMainForm, qtractorTrack *pTrack, float fGain )
	: qtractorTrackCommand(pMainForm, "track gain", pTrack)
{
	m_fGain = fGain;

	setRefresh(false);
}


// Track-gain command method.
bool qtractorTrackGainCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	// Set track gain (repective monitor gets set too...)
	float fGain = pTrack->gain();	
fprintf(stderr, "qtractorTrackGainCommand::redo() gain=%.3g -> %.3g\n", fGain, m_fGain);
	pTrack->setGain(m_fGain);
	m_fGain = fGain;

	// MIDI tracks are special...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Now we gotta make sure of proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->bus());
		if (pMidiBus)
			pMidiBus->setVolume(pTrack->midiChannel(), pTrack->gain());
	}

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
	: qtractorTrackCommand(pMainForm, "track panning", pTrack)
{
	m_fPanning = fPanning;
	
	setRefresh(false);
}


// Track-panning command method.
bool qtractorTrackPanningCommand::redo (void)
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	// Set track panning (repective monitor gets set too...)
	float fPanning = pTrack->panning();	
	pTrack->setPanning(m_fPanning);
	m_fPanning = fPanning;

	// MIDI tracks are special...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Now we gotta make sure of proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->bus());
		if (pMidiBus)
			pMidiBus->setPanning(pTrack->midiChannel(), pTrack->panning());
	}

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
