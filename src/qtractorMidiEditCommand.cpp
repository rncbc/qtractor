// qtractorMidiEditCommand.cpp
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
	: qtractorCommand(sName), m_pMidiClip(pMidiClip), m_bAdjusted(false),
		m_iDuration((pMidiClip->sequence())->duration())		
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
	unsigned long iTime, unsigned long iDuration )
{
	m_items.append(new Item(ResizeEventTime, pEvent, 0, iTime, iDuration));
}


void qtractorMidiEditCommand::resizeEventValue ( qtractorMidiEvent *pEvent,
	int iValue )
{
	if (pEvent->type() == qtractorMidiEvent::NOTEON && iValue < 1)
		iValue = 1;	// Avoid zero velocity (aka. NOTEOFF)

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
#if 0
	if (pSession && pSession->isPlaying())
		pSession->midiEngine()->trackMute(pTrack, true);
#endif
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
			if (pEvent->type() == qtractorMidiEvent::NOTEON)
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
	m_pMidiClip->setDirtyEx(true);

	// Have we changed on something less durable?
	if (m_iDuration != iOldDuration) {
		pSeq->setDuration(m_iDuration);
		m_iDuration = iOldDuration;
	}

	// Adjust edit-command result to prevent event overlapping.
	if (bRedo && !m_bAdjusted) m_bAdjusted = adjust();

	// Or are we changing something more durable?
	if (pSeq->duration() != iOldDuration) {
		pSeq->setTimeLength(pSeq->duration());
		if (pSession) {
			m_pMidiClip->setClipLengthEx(
				pSession->frameFromTick(pSession->tickFromFrame(
					m_pMidiClip->clipStart()) + pSeq->duration())
				- m_pMidiClip->clipStart());
		}
		m_pMidiClip->updateEditorEx(iSelectClear > 0);
	}	// Just reset editor internals...
	else m_pMidiClip->resetEditorEx(iSelectClear > 0);

	// Re-enqueue dropped events...
	if (pSession && pSession->isPlaying()) {
		// Reset all current running event cursors,
		// make them play it right and sound again...
		m_pMidiClip->reset(pSession->isLooping());
		// Re-enqueueing in proper sense...
		pSession->midiEngine()->trackMute(pTrack, false);
	}

	return true;
}


// Adjust edit-command result to prevent event overlapping.
bool qtractorMidiEditCommand::adjust (void)
{
	if (m_pMidiClip == NULL)
		return false;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return false;

	// HACK: What we're going to do here is about checking the
	// whole sequence, fixing any overlapping (note) events and
	// adjusting the issued command for proper undo/redo...
	QHash<int, qtractorMidiEvent *> events;

	// For each event, do rescan...
	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		qtractorMidiEvent *pNextEvent = pEvent->next();
		// Whether event is (time) adjustable...
		int key = int(pEvent->type()) << 7;
		switch (pEvent->type()) {
		case qtractorMidiEvent::NOTEON:
		case qtractorMidiEvent::NOTEOFF:
		case qtractorMidiEvent::KEYPRESS:
			key += int(pEvent->note());
			break;
		case qtractorMidiEvent::CONTROLLER:
			key += int(pEvent->controller());
			break;
		case qtractorMidiEvent::CHANPRESS:
		case qtractorMidiEvent::PITCHBEND:
			break;
		default:
			key = 0; // Non adjustable!
			break;
		}
		// Adjustable?
		if (key) {
			// Already there?
			qtractorMidiEvent *pPrevEvent = events.value(key, NULL);
			if (pPrevEvent) {
				unsigned long iTime = pEvent->time();
				unsigned long iPrevTime = pPrevEvent->time();
				// NOTEON: Find previous note event and check overlaps...
				if (pEvent->type() == qtractorMidiEvent::NOTEON) {
					unsigned long iTimeEnd = iTime + pEvent->duration();
					unsigned long iPrevTimeEnd = iPrevTime + pPrevEvent->duration();
					// Inner operlap...
					if (iTime > iPrevTime && iTime < iPrevTimeEnd) {
						// Left-side outer event...
						unsigned long iDuration = pPrevEvent->duration();
						pPrevEvent->setDuration(iTime - iPrevTime);
						if (!findEvent(pPrevEvent, ResizeEventTime))
							resizeEventTime(pPrevEvent, iPrevTime, iDuration);
						// Right-side outer event...
						if (iTimeEnd < iPrevTimeEnd) {
							qtractorMidiEvent *pNewEvent
								= new qtractorMidiEvent(*pPrevEvent);
							pNewEvent->setTime(iTimeEnd);
							pNewEvent->setDuration(iPrevTimeEnd - iTimeEnd);
							insertEvent(pNewEvent);
							pSeq->insertEvent(pNewEvent);
							pNextEvent = pNewEvent->next();
							// Keep or set as last note...
							pEvent = pNewEvent;
						}
					}
					else
					// Loose overlap?...
					if (iTime == iPrevTime) {
						// Exact overlap...
						if (iTimeEnd == iPrevTimeEnd) {
							pSeq->unlinkEvent(pPrevEvent);
							if (!findEvent(pPrevEvent, RemoveEvent))
								removeEvent(pPrevEvent);
						} else {
							// Partial overlap...
							if (iTimeEnd < iPrevTimeEnd) {
								// Short over large...
								unsigned long iDuration = pPrevEvent->duration();
								pPrevEvent->setDuration(pEvent->duration());
								if (!findEvent(pPrevEvent, ResizeEventTime))
									resizeEventTime(pPrevEvent, iPrevTime, iDuration);
								iDuration = pEvent->duration();
								pSeq->unlinkEvent(pEvent);
								pEvent->setTime(iTimeEnd);
								pEvent->setDuration(iPrevTimeEnd - iTimeEnd);
								pSeq->insertEvent(pEvent);
								if (!findEvent(pEvent, ResizeEventTime)) {
									resizeEventTime(pEvent, iTime, iDuration);
								}
							} else {
								// Large over short...
								unsigned long iDuration = pEvent->duration();
								pSeq->unlinkEvent(pEvent);
								pEvent->setTime(iPrevTimeEnd);
								pEvent->setDuration(iTimeEnd - iPrevTimeEnd);
								pSeq->insertEvent(pEvent);
								if (!findEvent(pEvent, ResizeEventTime)) {
									resizeEventTime(pEvent, iTime, iDuration);
								}
							}
							// We've move it ahead...
							pEvent = pPrevEvent;
						}
					}
				}
				else
				// All other channel events...
				if (iTime == iPrevTime) {
					pSeq->unlinkEvent(pPrevEvent);
					if (!findEvent(pPrevEvent, RemoveEvent))
						removeEvent(pPrevEvent);
				}
			}
			// Remember last one...
			events.insert(key, pEvent);
		}
		// Iterate next...
		pEvent = pNextEvent;
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
