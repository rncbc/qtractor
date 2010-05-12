// qtractorMidiEditCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSession.h"


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
		if (pItem->event == pEvent
			&& (pItem->command == InsertEvent || pItem->command == cmd))
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
	int iSelectClear = 0;

	// Changes are due...
	QListIterator<Item *> iter(m_items);
	if (!bRedo)
		iter.toBack();
	while (bRedo ? iter.hasNext() : iter.hasPrevious()) {
		Item *pItem = (bRedo ? iter.next() : iter.previous());
		qtractorMidiEvent *pEvent = pItem->event;
		// Execute the command item...
		switch (pItem->command) {
		case InsertEvent: {
			if (bRedo)
				pSeq->insertEvent(pEvent);
			else
				pSeq->unlinkEvent(pEvent);
			pItem->autoDelete = !bRedo;
			++iSelectClear;
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
			++iSelectClear;
			break;
		}
		default:
			break;
		}
	}

	// It's dirty, definitly...
	m_pMidiClip->setDirty(true);

	// Have we changed on something less durable?
	if (m_iDuration != iOldDuration) {
		pSeq->setDuration(m_iDuration);
		m_iDuration = iOldDuration;
	}

	// Or are we changing something more durable?
	if (pSeq->duration() != iOldDuration) {
		pSeq->setTimeLength(pSeq->duration());
		m_pMidiClip->setClipLength(
			pSession->frameFromTick(pSession->tickFromFrame(
				m_pMidiClip->clipStart()) + pSeq->duration())
			- m_pMidiClip->clipStart());
		m_pMidiClip->updateEditor(iSelectClear > 0);
	}	// Just reset editor internals...
	else m_pMidiClip->resetEditor(iSelectClear > 0);

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


//----------------------------------------------------------------------
// class qtractorMidiClipCommand - declaration.
//

// Constructor.
qtractorMidiClipCommand::qtractorMidiClipCommand ( const QString& sName )
	: qtractorCommand(sName)
{
}


// Destructor.
qtractorMidiClipCommand::~qtractorMidiClipCommand (void)
{
	qDeleteAll(m_editCommands);
	m_editCommands.clear();
}


// Composite command methods.
void qtractorMidiClipCommand::addEditCommand ( qtractorMidiEditCommand *pEditCommand )
{
	m_editCommands.append(pEditCommand);
}


// Composite predicate.
bool qtractorMidiClipCommand::isEmpty (void) const
{
	return m_editCommands.isEmpty();
}


// Virtual command methods.
bool qtractorMidiClipCommand::redo (void)
{
	QListIterator<qtractorMidiEditCommand *> iter(m_editCommands);
	while (iter.hasNext()) {
	    if (!iter.next()->redo())
			return false;
	}

	return true;
}

bool qtractorMidiClipCommand::undo (void)
{
	QListIterator<qtractorMidiEditCommand *> iter(m_editCommands);
	iter.toBack();
	while (iter.hasPrevious()) {
	    if (!iter.previous()->undo())
			return false;
	}

	return true;
}


// end of qtractorMidiEditCommand.cpp
