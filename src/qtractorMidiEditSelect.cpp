// qtractorMidiEditSelect.cpp
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

#include "qtractorMidiEditSelect.h"
#include "qtractorMidiSequence.h"


//-------------------------------------------------------------------------
// qtractorMidiEditSelect -- MIDI event selection capsule.

// Constructor.
qtractorMidiEditSelect::qtractorMidiEditSelect (void)
{
}


// Default destructor.
qtractorMidiEditSelect::~qtractorMidiEditSelect (void)
{
	clear();
}


// Event selection item lookup.
qtractorMidiEditSelect::Item *qtractorMidiEditSelect::findItem (
	qtractorMidiEvent *pEvent )
{
	// Check if this very event already exists...
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (pItem->event == pEvent)
			return pItem;
	}
	// Not found.
	return NULL;
}


// Item insertion method.
void qtractorMidiEditSelect::addItem ( qtractorMidiEvent *pEvent,
	const QRect& rectEvent, const QRect& rectView )
{
	m_items.append(new Item(pEvent, rectEvent, rectView));

	m_rectEvent = m_rectEvent.unite(rectEvent);
	m_rectView = m_rectView.unite(rectView);
}


// Item selection method.
void qtractorMidiEditSelect::selectItem ( qtractorMidiEvent *pEvent,
	const QRect& rectEvent, const QRect& rectView,
	bool bSelect, bool bToggle )
{
	Item *pItem = findItem(pEvent);
	if (pItem) {
		unsigned int flags = pItem->flags;
		if ( (!bSelect && (flags & 2) == 0) ||
			(( bSelect && (flags & 3) == 3) && bToggle))
			pItem->flags &= ~1;
		else
		if ( ( bSelect && (flags & 2) == 0) ||
			((!bSelect && (flags & 3) == 2) && bToggle))
			pItem->flags |=  1;
	} 
	else if (bSelect)
		addItem(pEvent, rectEvent, rectView);
}


// Selection commit method.
void qtractorMidiEditSelect::update ( bool bCommit )
{
	// Remove unselected...
	int iUpdate = 0;
	QMutableListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (bCommit) {
			if (pItem->flags & 1)
				pItem->flags |=  2;
			else
				pItem->flags &= ~2;
		}
		if ((pItem->flags & 3) == 0) {
			iter.remove();
			delete pItem;
			iUpdate++;
		}
	}

	// Did we remove any?
	if (iUpdate > 0)
		commit();
}


// Selection commit method.
void qtractorMidiEditSelect::commit (void)
{
	// Reset united selection rectangle...
	m_rectEvent.setRect(0, 0, 0, 0);
	m_rectView.setRect(0, 0, 0, 0);

	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		m_rectEvent = m_rectEvent.unite(pItem->rectEvent);
		m_rectView = m_rectView.unite(pItem->rectView);
	}
}


// Reset event selection.
void qtractorMidiEditSelect::clear (void)
{
	m_rectEvent.setRect(0, 0, 0, 0);
	m_rectView.setRect(0, 0, 0, 0);

	qDeleteAll(m_items);
	m_items.clear();
}


// end of qtractorMidiEditSelect.cpp

