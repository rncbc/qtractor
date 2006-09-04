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
	m_items.setAutoDelete(true);
}

// Destructor.
qtractorClipCommand::~qtractorClipCommand (void)
{
	for (Item *pItem = m_items.first();
			pItem; pItem = m_items.next()) {
		if (pItem->autoDelete)
			delete pItem->clip;
	}

	m_items.clear();
}


// Add primitive clip item to command list.
void qtractorClipCommand::addItem ( qtractorClipCommand::CommandType cmd,
	qtractorClip *pClip, qtractorTrack *pTrack, unsigned long iClipStart,
	unsigned long iClipOffset, unsigned long iClipLength )
{
	m_items.append(
		new Item(cmd, pClip, pTrack, iClipStart, iClipOffset, iClipLength));
}


// Common executive method.
bool qtractorClipCommand::execute ( bool bRedo )
{
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

	for (Item *pItem = m_items.first();
			pItem; pItem = m_items.next()) {

		// Execute the command item...
		switch (pItem->command) {
			case AddClip: {
				if (bRedo)
					(pItem->track)->addClip(pItem->clip);
				else
					(pItem->track)->unlinkClip(pItem->clip);
				pItem->autoDelete = !bRedo;
				break;
			}
			case RemoveClip: {
				if (bRedo)
					(pItem->track)->unlinkClip(pItem->clip);
				else
					(pItem->track)->addClip(pItem->clip);
				pItem->autoDelete = bRedo;
				break;
			}
			case MoveClip: {
				qtractorClip  *pClip = pItem->clip;
				qtractorTrack *pOldTrack = pClip->track();
				unsigned long  iOldStart = pClip->clipStart();
				qtractorTrack *pNewTrack = pItem->track;
				pOldTrack->unlinkClip(pClip);
				pClip->setClipStart(pItem->clipStart);
				pNewTrack->addClip(pClip);
				pItem->track = pOldTrack;
				pItem->clipStart = iOldStart;
				if (pOldTrack != pNewTrack)
					pSession->updateTrack(pOldTrack);
				pSession->updateTrack(pNewTrack);
				break;
			}
			case ChangeClip:
			default:
				break;
		}
		// Always update the target track...
		pSession->updateTrack(pItem->track);
	}

	return true;
}


// Virtual command methods.
bool qtractorClipCommand::redo (void)
{
	return execute(true);
}

bool qtractorClipCommand::undo (void)
{
	return execute(false);
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
			addItem(pAudioClip, pTrack);
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
			addItem(pMidiClip, pTrack);
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


// Add clip item to command list.
void qtractorAddClipCommand::addItem ( qtractorClip *pClip,
	qtractorTrack *pTrack )
{
	qtractorClipCommand::addItem(AddClip, pClip, pTrack);
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


// Add clip item to command list.
void qtractorRemoveClipCommand::addItem ( qtractorClip *pClip )
{
	qtractorClipCommand::addItem(RemoveClip, pClip, pClip->track());
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


// Add clip item to command list.
void qtractorMoveClipCommand::addItem ( qtractorClip *pClip,
	qtractorTrack *pTrack, unsigned long iClipStart )
{
	qtractorClipCommand::addItem(MoveClip, pClip, pTrack, iClipStart);
}


// end of qtractorClipCommand.cpp
