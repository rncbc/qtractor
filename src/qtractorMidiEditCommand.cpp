// qtractorMidiEditCommand.cpp
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

#include "qtractorMidiEditCommand.h"

#include "qtractorMidiClip.h"
#include "qtractorMidiEngine.h"


//----------------------------------------------------------------------
// class qtractorMidiEditCommand - implementation.
//

// Constructor.
qtractorMidiEditCommand::qtractorMidiEditCommand (
	qtractorMidiClip *pMidiClip, const QString& sName )
	: qtractorCommand(sName), m_pMidiClip(pMidiClip)
{
	m_iDuration = (pMidiClip->sequence())->duration();
}


// Destructor.
qtractorMidiEditCommand::~qtractorMidiEditCommand (void)
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (pItem->autoDelete)
			delete pItem->event;
	}

	qDeleteAll(m_items);
	m_items.clear();
}


// Primitive command methods.
void qtractorMidiEditCommand::insertEvent ( qtractorMidiEvent *pEvent )
{
	m_items.append(new Item(InsertEvent, pEvent));
}


void qtractorMidiEditCommand::moveEvent ( qtractorMidiEvent *pEvent,
	int iNote, unsigned long iTime )
{
	m_items.append(new Item(MoveEvent, pEvent, iNote, iTime));
}


void qtractorMidiEditCommand::resizeEventTime ( qtractorMidiEvent *pEvent,
	unsigned long iTime, unsigned long iDuration )
{
	m_items.append(new Item(ResizeEventTime, pEvent, 0, iTime, iDuration));
}


void qtractorMidiEditCommand::resizeEventValue ( qtractorMidiEvent *pEvent,
	int iValue )
{
	m_items.append(new Item(ResizeEventValue, pEvent, 0, 0, 0, iValue));
}


void qtractorMidiEditCommand::removeEvent ( qtractorMidiEvent *pEvent )
{
	m_items.append(new Item(RemoveEvent, pEvent));
}


// Check whether the event is already in chain.
bool qtractorMidiEditCommand::findEvent ( qtractorMidiEvent *pEvent,
	qtractorMidiEditCommand::CommandType cmd ) const
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (pItem->event == pEvent && pItem->command == cmd)
			return true;
	}
	return false;
}


// Common executive method.
bool qtractorMidiEditCommand::execute ( bool bRedo )
{
	if (m_pMidiClip == NULL)
		return false;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return false;

	// Dropped enqueued events...
	qtractorSession *pSession = NULL;
	qtractorTrack *pTrack = m_pMidiClip->track();
	if (pTrack)
		pSession = pTrack->session();
	if (pSession && pSession->isPlaying())
		pSession->midiEngine()->trackMute(pTrack, true);

	// Track sequence duration changes...
	unsigned long iOldDuration = pSeq->duration();

	// Changes are due...
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		qtractorMidiEvent *pEvent = pItem->event;
		// Execute the command item...
		switch (pItem->command) {
		case InsertEvent: {
			if (bRedo)
				pSeq->insertEvent(pEvent);
			else
				pSeq->unlinkEvent(pEvent);
			pItem->autoDelete = !bRedo;
			break;
		}
		case MoveEvent: {
			int iOldNote = int(pEvent->note());
			unsigned long iOldTime = pEvent->time();
			pSeq->unlinkEvent(pEvent);
			pEvent->setNote(pItem->note);
			pEvent->setTime(pItem->time);
			pSeq->insertEvent(pEvent);
			pItem->note = iOldNote;
			pItem->time = iOldTime;
			break;
		}
		case ResizeEventTime: {
			unsigned long iOldTime = pEvent->time();
			unsigned long iOldDuration = pEvent->duration();
			pSeq->unlinkEvent(pEvent);
			pEvent->setTime(pItem->time);
			pEvent->setDuration(pItem->duration);
			pSeq->insertEvent(pEvent);
			pItem->time = iOldTime;
			pItem->duration = iOldDuration;
			break;
		}
		case ResizeEventValue: {
			int iOldValue;
			if (pEvent->type() == qtractorMidiEvent::PITCHBEND) {
				iOldValue = pEvent->pitchBend();
				pEvent->setPitchBend(pItem->value);
			} else {
				iOldValue = pEvent->value();
				pEvent->setValue(pItem->value);
			}
			pItem->value = iOldValue;
			break;
		}
		case RemoveEvent: {
			if (bRedo)
				pSeq->unlinkEvent(pEvent);
			else
				pSeq->insertEvent(pEvent);
			pItem->autoDelete = bRedo;
			break;
		}
		default:
			break;
		}
	}

	// Have we changed on something less durable?
	if (m_iDuration != iOldDuration) {
		pSeq->setDuration(m_iDuration);
		m_iDuration = iOldDuration;
	}
	// Or are we changing something more durable?
	if (pSeq->duration() != iOldDuration) {
		m_pMidiClip->setClipLength(pSession->frameFromTick(pSeq->duration()));
		m_pMidiClip->updateEditor();
	}

	// Renqueue dropped events...
	if (pSession && pSession->isPlaying())
		pSession->midiEngine()->trackMute(pTrack, false);

	return true;
}


// Virtual command methods.
bool qtractorMidiEditCommand::redo (void)
{
	return execute(true);
}

bool qtractorMidiEditCommand::undo (void)
{
	return execute(false);
}


// end of qtractorMidiEditCommand.cpp
