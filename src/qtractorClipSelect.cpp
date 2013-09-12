// qtractorClipSelect.cpp
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

#include "qtractorClipSelect.h"

#include "qtractorSession.h"
#include "qtractorTrack.h"
#include "qtractorClip.h"


//-------------------------------------------------------------------------
// qtractorClipSelect -- Track clip selection capsule.

// Constructor.
qtractorClipSelect::qtractorClipSelect (void)
{
	m_bTrackSingle = false;
	m_pTrackSingle = NULL;
}


// Default constructor.
qtractorClipSelect::~qtractorClipSelect (void)
{
	clear();
}


// Clip selection method.
void qtractorClipSelect::selectItem ( qtractorClip *pClip,
	const QRect& rect, bool bSelect )
{
	// Add/remove clip selection...
	Item *pClipItem = NULL;

	ItemList::Iterator iter = m_items.find(pClip);
	if (iter != m_items.end())
		pClipItem = iter.value();

	if (pClipItem && !bSelect) {
	    iter = m_items.erase(iter);
		delete pClipItem;
		pClip->setClipSelected(false);
		m_bTrackSingle = false;
		// Reset united selection rectangle...
		m_rect.setRect(0, 0, 0, 0);
		const ItemList::Iterator& iter_end = m_items.end();
		for (iter = m_items.begin(); iter != iter_end; ++iter)
			m_rect = m_rect.united(iter.value()->rectClip);
		// Done with clip deselection.
	} else if (bSelect) {
		pClip->setClipSelected(true);
		if (pClipItem)
			pClipItem->rectClip = rect;
		else
			m_items.insert(pClip, new Item(rect));
		// Special optimization: no need to recache
		// our single track reference if we add some outsider clip...
		if (m_bTrackSingle) {
			if (m_pTrackSingle && m_pTrackSingle != pClip->track())
				m_pTrackSingle = NULL;
		} else if (m_pTrackSingle == NULL) {
			m_pTrackSingle = pClip->track();
			m_bTrackSingle = true;
		}
		// Unite whole selection reactangular area...
		m_rect = m_rect.united(rect);
	}
}


// Clip addition (no actual selection).
void qtractorClipSelect::addItem ( qtractorClip *pClip, const QRect& rect )
{
	m_items.insert(pClip, new Item(rect));
	m_rect = m_rect.united(rect);
}


// The united selection rectangle.
const QRect& qtractorClipSelect::rect (void) const
{
	return m_rect;
}


// Dynamic helper:
// Do all selected clips belong to the same track?
qtractorTrack *qtractorClipSelect::singleTrack (void)
{
	// Check if predicate is already cached...
	if (!m_bTrackSingle) {
		m_pTrackSingle = NULL;
		ItemList::ConstIterator iter = m_items.constBegin();
		const ItemList::ConstIterator& iter_end = m_items.constEnd();
		for ( ; iter != iter_end; ++iter) {
			qtractorClip *pClip = iter.key();
			if (m_pTrackSingle == NULL)
				m_pTrackSingle = pClip->track();
			else if (m_pTrackSingle != pClip->track()) {
				m_pTrackSingle = NULL;
				break;
			}
		}
		m_bTrackSingle = true;
	}

	// Return current selected single track (if any)...
	return m_pTrackSingle;
}


// Selection list accessor.
const qtractorClipSelect::ItemList& qtractorClipSelect::items (void) const
{
	return m_items;
}


// Clip selection item lookup.
qtractorClipSelect::Item *qtractorClipSelect::findItem ( qtractorClip *pClip ) const
{
	return m_items.value(pClip, NULL);
}


// Reset clip selection.
void qtractorClipSelect::reset (void)
{
	m_bTrackSingle = false;
	m_pTrackSingle = NULL;

	m_rect.setRect(0, 0, 0, 0);

	qDeleteAll(m_items);
	m_items.clear();
}


// Clear clip selection.
void qtractorClipSelect::clear (void)
{
	ItemList::ConstIterator iter = m_items.constBegin();
	const ItemList::ConstIterator& iter_end = m_items.constEnd();
	for ( ; iter != iter_end; ++iter)
		iter.key()->setClipSelected(false);

	reset();
}


// end of qtractorClipSelect.cpp
