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

		qtractorClip  *pClip  = pItem->clip;
		qtractorTrack *pTrack = pItem->track;
	
		// Execute the command item...
		switch (pItem->command) {
		case AddClip: {
			if (bRedo)
				pTrack->addClip(pClip);
			else
				pTrack->unlinkClip(pClip);
			pItem->autoDelete = !bRedo;
			break;
		}
		case RemoveClip: {
			if (bRedo)
				pTrack->unlinkClip(pClip);
			else
				pTrack->addClip(pClip);
			pItem->autoDelete = bRedo;
			break;
		}
		case MoveClip: {
			qtractorTrack *pOldTrack = pClip->track();
			unsigned long  iOldStart = pClip->clipStart();
			unsigned long iOldOffset = pClip->clipOffset();
			unsigned long iOldLength = pClip->clipLength();
			pOldTrack->unlinkClip(pClip);
			pClip->setClipStart(pItem->clipStart);
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pTrack->addClip(pClip);
			pItem->track      = pOldTrack;
			pItem->clipStart  = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			if (pOldTrack != pTrack)
				pSession->updateTrack(pOldTrack);
			break;
		}
		case ChangeClip: {
			unsigned long iOldStart  = pClip->clipStart();
			unsigned long iOldOffset = pClip->clipOffset();
			unsigned long iOldLength = pClip->clipLength();
			pClip->setClipStart(pItem->clipStart);
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pClip->open();
			pItem->clipStart  = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			break;
		}
		default:
			break;
		}

		// Always update the target track...
		pSession->updateTrack(pTrack);
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


// end of qtractorClipCommand.cpp
