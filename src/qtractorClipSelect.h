// qtractorClipSelect.h
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

#ifndef __qtractorClipSelect_h
#define __qtractorClipSelect_h

#include "qtractorClip.h"

#include "qtractorRubberBand.h"

#include <QMultiHash>
#include <QRect>


//-------------------------------------------------------------------------
// qtractorClipSelect -- Track clip selection capsule.

class qtractorClipSelect
{
public:

	// Constructor.
	qtractorClipSelect();
	// Default constructor.
	~qtractorClipSelect();

	// Selection item struct.
	struct Item
	{
		// Item constructor.
		Item(const QRect& rect) : rectClip(rect), rubberBand(NULL) {}
		// Item destructor.
		~Item() {
			if (rubberBand) {
				rubberBand->hide();
				delete rubberBand;
			}
		}
		// Item members.
		QRect rectClip;
		qtractorRubberBand *rubberBand;
	};

	// Selection list definition.
	typedef QMultiHash<qtractorClip *, Item *> ItemList;

	// Clip selection method.
	void selectItem(qtractorClip *pClip,
		const QRect& rect, bool bSelect = true);

	// Clip addition (no actual selection).
	void addItem(qtractorClip *pClip, const QRect& rect);

	// The united selection rectangle.
	const QRect& rect() const;

	// Dynamic helper:
	// Do all selected clips belong to the same track?
	qtractorTrack *singleTrack();

	// Selection list accessor.
	const ItemList& items() const;

	// Clip selection item lookup.
	Item *findItem(qtractorClip *pClip) const;

	// Reset clip selection.
	void reset();

	// Clear clip selection.
	void clear();

private:

	// The clip selection list.
	ItemList m_items;

	// The united selection rectangle.
	QRect m_rect;

	// To cache single track selection.
	qtractorTrack *m_pTrackSingle;
	bool           m_bTrackSingle;
};


#endif  // __qtractorClipSelect_h


// end of qtractorClipSelect.h
