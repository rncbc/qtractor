// qtractorClipCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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
}

// Destructor.
qtractorClipCommand::~qtractorClipCommand (void)
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (pItem->autoDelete)
			delete pItem->clip;
	}

	qDeleteAll(m_items);
	m_items.clear();
}


// Primitive command methods.
void qtractorClipCommand::addClip ( qtractorClip *pClip,
	qtractorTrack *pTrack )
{
	m_items.append(new Item(AddClip, pClip, pTrack));
}


void qtractorClipCommand::removeClip ( qtractorClip *pClip )
{
	m_items.append(new Item(RemoveClip, pClip, pClip->track()));
}


void qtractorClipCommand::moveClip ( qtractorClip *pClip,
	qtractorTrack *pTrack, unsigned long iClipStart,
	unsigned long iClipOffset, unsigned long iClipLength )
{
	Item *pItem = new Item(MoveClip, pClip, pTrack);
	pItem->clipStart  = iClipStart;
	pItem->clipOffset = iClipOffset;
	pItem->clipLength = iClipLength;
	if (iClipOffset == pClip->clipOffset())
		pItem->fadeInLength = pClip->fadeInLength();
	if (iClipOffset + iClipLength == pClip->clipOffset() + pClip->clipLength())
		pItem->fadeOutLength = pClip->fadeOutLength();
	m_items.append(pItem);
}


void qtractorClipCommand::resizeClip ( qtractorClip *pClip,
	unsigned long iClipStart, unsigned long iClipOffset,
	unsigned long iClipLength )
{
	Item *pItem = new Item(ResizeClip, pClip, pClip->track());
	pItem->clipStart  = iClipStart;
	pItem->clipOffset = iClipOffset;
	pItem->clipLength = iClipLength;
	if (iClipOffset == pClip->clipOffset())
		pItem->fadeInLength = pClip->fadeInLength();
	if (iClipOffset + iClipLength == pClip->clipOffset() + pClip->clipLength())
		pItem->fadeOutLength = pClip->fadeOutLength();
	m_items.append(pItem);
}


void qtractorClipCommand::fadeInClip ( qtractorClip *pClip,
	unsigned long iFadeInLength )
{
	Item *pItem = new Item(FadeInClip, pClip, pClip->track());
	pItem->fadeInLength = iFadeInLength;
	m_items.append(pItem);
}


void qtractorClipCommand::fadeOutClip ( qtractorClip *pClip,
	unsigned long iFadeOutLength )
{
	Item *pItem = new Item(FadeOutClip, pClip, pClip->track());
	pItem->fadeOutLength = iFadeOutLength;
	m_items.append(pItem);
}


// Special clip record nethod.
bool qtractorClipCommand::addClipRecord ( qtractorTrack *pTrack )
{
	qtractorClip *pClip = pTrack->clipRecord();
	if (pClip == NULL)
		return false;

	// Time to close the clip...
	pClip->close(true);

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
			addClip(pAudioClip, pTrack);
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
			addClip(pMidiClip, pTrack);
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


// Common executive method.
bool qtractorClipCommand::execute ( bool bRedo )
{
	qtractorSession *pSession = mainForm()->session();
	if (pSession == NULL)
		return false;

//	pSession->lock();

	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
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
			pSession->updateTrack(pTrack);
			break;
		}
		case RemoveClip: {
			if (bRedo)
				pTrack->unlinkClip(pClip);
			else
				pTrack->addClip(pClip);
			pItem->autoDelete = bRedo;
			pSession->updateTrack(pTrack);
			break;
		}
		case MoveClip: {
			qtractorTrack *pOldTrack = pClip->track();
			unsigned long  iOldStart = pClip->clipStart();
			unsigned long iOldOffset = pClip->clipOffset();
			unsigned long iOldLength = pClip->clipLength();
			unsigned long iOldFadeIn = pClip->fadeInLength();
			unsigned long iOldFadeOut = pClip->fadeOutLength();
			pOldTrack->unlinkClip(pClip);
			pClip->setClipStart(pItem->clipStart);
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pClip->setFadeInLength(pItem->fadeInLength);
			pClip->setFadeOutLength(pItem->fadeOutLength);
			pTrack->addClip(pClip);
			pItem->track      = pOldTrack;
			pItem->clipStart  = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeOutLength = iOldFadeOut;
			if (pOldTrack != pTrack)
				pSession->updateTrack(pOldTrack);
			pSession->updateTrack(pTrack);
			break;
		}
		case ResizeClip: {
			unsigned long iOldStart  = pClip->clipStart();
			unsigned long iOldOffset = pClip->clipOffset();
			unsigned long iOldLength = pClip->clipLength();
			unsigned long iOldFadeIn = pClip->fadeInLength();
			unsigned long iOldFadeOut = pClip->fadeOutLength();
			pClip->setClipStart(pItem->clipStart);
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pClip->setFadeInLength(pItem->fadeInLength);
			pClip->setFadeOutLength(pItem->fadeOutLength);
			pClip->open();
			pItem->clipStart  = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeOutLength = iOldFadeOut;
			pSession->updateTrack(pTrack);
			break;
		}
		case FadeInClip: {
			unsigned long iOldFadeIn = pClip->fadeInLength();
			pClip->setFadeInLength(pItem->fadeInLength);
			pItem->fadeInLength = iOldFadeIn;
			break;
		}
		case FadeOutClip: {
			unsigned long iOldFadeOut = pClip->fadeOutLength();
			pClip->setFadeOutLength(pItem->fadeOutLength);
			pItem->fadeOutLength = iOldFadeOut;
			break;
		}
		default:
			break;
		}
	}

//	pSession->unlock();

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


// end of qtractorClipCommand.cpp
