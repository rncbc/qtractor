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

#include "qtractorMidiSequence.h"


//----------------------------------------------------------------------
// class qtractorMidiEditCommand - implementation.
//

// Constructor.
qtractorMidiEditCommand::qtractorMidiEditCommand (
	qtractorMidiSequence *pSeq, const QString& sName )
	: qtractorCommand(sName), m_pSeq(pSeq)
{
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
	unsigned long iTime )
{
	m_items.append(new Item(ResizeEventTime, pEvent, 0, iTime));
}


void qtractorMidiEditCommand::resizeEventTime2 ( qtractorMidiEvent *pEvent,
	unsigned long iTime, unsigned long iDuration )
{
	m_items.append(new Item(ResizeEventTime2, pEvent, 0, iTime, iDuration));
}


void qtractorMidiEditCommand::resizeEventDuration ( qtractorMidiEvent *pEvent,
	unsigned long iDuration )
{
	m_items.append(new Item(ResizeEventDuration, pEvent, 0, 0, iDuration));
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
	if (m_pSeq == NULL)
		return false;

	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		qtractorMidiEvent *pEvent = pItem->event;
		// Execute the command item...
		switch (pItem->command) {
		case InsertEvent: {
			if (bRedo)
				m_pSeq->insertEvent(pEvent);
			else
				m_pSeq->unlinkEvent(pEvent);
			pItem->autoDelete = !bRedo;
			break;
		}
		case MoveEvent: {
			int iOldNote = int(pEvent->note());
			unsigned long iOldTime = pEvent->time();
			m_pSeq->unlinkEvent(pEvent);
			pEvent->setNote(pItem->note);
			pEvent->setTime(pItem->time);
			m_pSeq->insertEvent(pEvent);
			pItem->note = iOldNote;
			pItem->time = iOldTime;
			break;
		}
		case ResizeEventTime: {
			unsigned long iOldTime = pEvent->time();
			m_pSeq->unlinkEvent(pEvent);
			pEvent->setTime(pItem->time);
			m_pSeq->insertEvent(pEvent);
			pItem->time = iOldTime;
			break;
		}
		case ResizeEventTime2: {
			unsigned long iOldTime = pEvent->time();
			unsigned long iOldDuration = pEvent->duration();
			m_pSeq->unlinkEvent(pEvent);
			pEvent->setTime(pItem->time);
			pEvent->setDuration(pItem->duration);
			m_pSeq->insertEvent(pEvent);
			pItem->time = iOldTime;
			pItem->duration = iOldDuration;
			break;
		}
		case ResizeEventDuration: {
			unsigned long iOldDuration = pEvent->duration();
			pEvent->setDuration(pItem->duration);
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
				m_pSeq->unlinkEvent(pEvent);
			else
				m_pSeq->insertEvent(pEvent);
			pItem->autoDelete = bRedo;
			break;
		}
		default:
			break;
		}
	}

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
