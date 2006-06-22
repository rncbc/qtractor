// qtractorClipCommand.cpp
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
#include "qtractorClipCommand.h"

#include "qtractorMainForm.h"

#include "qtractorSession.h"
#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"
#include "qtractorFiles.h"
#include "qtractorTracks.h"
#include "qtractorTrackView.h"


//----------------------------------------------------------------------
// class qtractorClipCommand - declaration.
//

// Constructor.
qtractorClipCommand::qtractorClipCommand ( qtractorMainForm *pMainForm,
	const QString& sName ) : qtractorCommand(pMainForm, sName)
{
	m_clips.setAutoDelete(true);
}

// Destructor.
qtractorClipCommand::~qtractorClipCommand (void)
{
	if (isAutoDelete()) {
		for (Item *pClipItem = m_clips.first();
				pClipItem; pClipItem = m_clips.next()) {
			delete pClipItem->clip;
		}
	}

	m_clips.clear();
}


// Add clip to command list.
void qtractorClipCommand::addClip ( qtractorClip *pClip,
	qtractorTrack *pTrack, unsigned long iClipStart )
{
	m_clips.append(new Item(pClip, pTrack, iClipStart));
}


// Clip command methods.
bool qtractorClipCommand::addClipItems (void)
{
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

	for (Item *pClipItem = m_clips.first();
			pClipItem; pClipItem = m_clips.next()) {
	    (pClipItem->track)->addClip(pClipItem->clip);
		pSession->updateTrack(pClipItem->track);
	}

	setAutoDelete(false);

	return true;
}

bool qtractorClipCommand::removeClipItems (void)
{
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

	for (Item *pClipItem = m_clips.first();
			pClipItem; pClipItem = m_clips.next()) {
	    (pClipItem->track)->unlinkClip(pClipItem->clip);
		pSession->updateTrack(pClipItem->track);
	}

	setAutoDelete(true);

	return true;
}

bool qtractorClipCommand::moveClipItems (void)
{
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

	for (Item *pClipItem = m_clips.first();
			pClipItem; pClipItem = m_clips.next()) {
		qtractorClip  *pClip = pClipItem->clip;
		qtractorTrack *pOldTrack = pClip->track();
		unsigned long  iOldStart = pClip->clipStart();
		qtractorTrack *pNewTrack = pClipItem->track;
	    pOldTrack->unlinkClip(pClip);
	    pClip->setClipStart(pClipItem->clipStart);
	    pNewTrack->addClip(pClip);
	    pClipItem->track = pOldTrack;
	    pClipItem->clipStart = iOldStart;
		if (pOldTrack != pNewTrack)
			pSession->updateTrack(pOldTrack);
		pSession->updateTrack(pNewTrack);
	}

	return true;
}

//----------------------------------------------------------------------
// class qtractorAddClipCommand - declaration.
//

// Constructor.
qtractorAddClipCommand::qtractorAddClipCommand (
	qtractorMainForm *pMainForm )
	: qtractorClipCommand(pMainForm, QObject::tr("add clip"))
{
}


// Special clip record nethod.
bool qtractorAddClipCommand::addClipRecord ( qtractorTrack *pTrack )
{
	qtractorClip *pClip = pTrack->clipRecord();
	if (pClip == NULL)
		return false;

	// Time to close the clip...
	pClip->close();

	// Check final length...
	if (pClip->clipLength() == 0)
		return false;

	// Reference for immediate file addition...
	qtractorFiles *pFiles = mainForm()->files();

	// Now, its imperative to make a proper copy of those clips...
	unsigned long iClipStart = pClip->clipStart();
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio: {
		qtractorAudioClip *pAudioClip
			= static_cast<qtractorAudioClip *> (pClip);
		if (pAudioClip) {
			pAudioClip = new qtractorAudioClip(*pAudioClip);
			pAudioClip->setClipStart(iClipStart);
			addClip(pAudioClip, pTrack, iClipStart);
			if (pFiles)
				pFiles->addAudioFile(pAudioClip->filename());
		}
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			pMidiClip = new qtractorMidiClip(*pMidiClip);
			pMidiClip->setClipStart(iClipStart);
			addClip(pMidiClip, pTrack, iClipStart);
			if (pFiles)
				pFiles->addMidiFile(pMidiClip->filename());
		}
		break;
	}
	default:
		return false;
	}

	// Can get rid of the recorded clip.
	pTrack->setClipRecord(NULL);
	// Done.
	return true;
}


// Clip insertion command methods.
bool qtractorAddClipCommand::redo (void)
{
	return addClipItems();
}

bool qtractorAddClipCommand::undo (void)
{
	return removeClipItems();
}


//----------------------------------------------------------------------
// class qtractorRemoveClipCommand - declaration.
//

// Constructor.
qtractorRemoveClipCommand::qtractorRemoveClipCommand (
	qtractorMainForm *pMainForm )
	: qtractorClipCommand(pMainForm, QObject::tr("remove clip"))
{
}

// Clip removal command methods.
bool qtractorRemoveClipCommand::redo (void)
{
	return removeClipItems();
}

bool qtractorRemoveClipCommand::undo (void)
{
	return addClipItems();
}


//----------------------------------------------------------------------
// class qtractorMoveClipCommand - declaration.
//

// Constructor.
qtractorMoveClipCommand::qtractorMoveClipCommand (
	qtractorMainForm *pMainForm )
	: qtractorClipCommand(pMainForm, QObject::tr("move clip"))
{
}

// Clip move command methods.
bool qtractorMoveClipCommand::redo (void)
{
	return moveClipItems();
}

bool qtractorMoveClipCommand::undo (void)
{
	// As we swap the prev/clip this is non-idempotent.
	return redo();
}


// end of qtractorClipCommand.cpp
