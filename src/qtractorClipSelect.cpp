// qtractorClipSelect.cpp
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

#include "qtractorClipSelect.h"

#include "qtractorSession.h"
#include "qtractorTrack.h"
#include "qtractorClip.h"


//-------------------------------------------------------------------------
// qtractorClipSelect -- Track clip selection capsule.

// Constructor.
qtractorClipSelect::qtractorClipSelect (void)
{
	m_clips.setAutoDelete(true);

	m_bTrackSingle = false;
	m_pTrackSingle = NULL;
}


// Default constructor.
qtractorClipSelect::~qtractorClipSelect (void)
{
	clear();
}


// Clip selection method.
void qtractorClipSelect::selectClip ( qtractorClip *pClip,
	const QRect& rect, bool bSelect )
{
	// Add/remove clip selection...
	Item *pClipItem = findClip(pClip);
	if (pClipItem && !bSelect) {
	    m_clips.remove(pClipItem);
		pClip->setClipSelected(false);
		m_bTrackSingle = false;
		// Reset united selection rectangle...
		m_rect.setRect(0, 0, 0, 0);
		for (pClipItem = m_clips.first();
			pClipItem; pClipItem = m_clips.next())
			m_rect = m_rect.unite(pClipItem->rectClip);
		// Done with clip deselection.
	} else if (bSelect) {
		pClip->setClipSelected(true);
		if (pClipItem == NULL)
			m_clips.append(new Item(pClip, rect));
		// Special optimization: no need to recache
		// our single track reference if we add some outsider clip...
		if (m_bTrackSingle && m_pTrackSingle
			&& m_pTrackSingle != pClip->track())
			m_pTrackSingle = NULL;
		// Unite whole selection reactangular area...
		m_rect = m_rect.unite(rect);
	}
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
		for (Item *pClipItem = m_clips.first();
				pClipItem; pClipItem = m_clips.next()) {
			if (m_pTrackSingle == NULL)
				m_pTrackSingle = (pClipItem->clip)->track();
			else if (m_pTrackSingle != (pClipItem->clip)->track()) {
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
QPtrList<qtractorClipSelect::Item>& qtractorClipSelect::clips (void)
{
	return m_clips;
}


// Clip selection item lookup.
qtractorClipSelect::Item *qtractorClipSelect::findClip ( qtractorClip *pClip )
{
	// Check if this very clip already exists...
	for (Item *pClipItem = m_clips.first();
			pClipItem; pClipItem = m_clips.next()) {
		if (pClipItem->clip == pClip)
			return pClipItem;
	}
	// Not found.
	return NULL;
}


// Reset clip selection.
void qtractorClipSelect::clear (void)
{
	m_bTrackSingle = false;
	m_pTrackSingle = NULL;

	for (Item *pClipItem = m_clips.first();
	    	pClipItem; pClipItem = m_clips.next()) {
		(pClipItem->clip)->setClipSelected(false);
	}

	m_rect.setRect(0, 0, 0, 0);
	m_clips.clear();
}


// end of qtractorClipSelect.h
