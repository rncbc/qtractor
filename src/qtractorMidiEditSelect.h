// qtractorMidiEditSelect.h
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

#ifndef __qtractorMidiEditSelect_h
#define __qtractorMidiEditSelect_h

#include <QHash>
#include <QRect>

// Forward declarations.
class qtractorMidiEvent;


//-------------------------------------------------------------------------
// qtractorMidiEditSelect -- MIDI event selection capsule.

class qtractorMidiEditSelect
{
public:

	// Constructor.
	qtractorMidiEditSelect();

	// Default destructor.
	~qtractorMidiEditSelect();

	// Selection item struct.
	struct Item
	{
		// Item constructor.
		Item(const QRect& re, const QRect& rv, unsigned long dt = 0)
			: rectEvent(re), rectView(rv), delta(dt), flags(1) {}
		// Item members.
		QRect rectEvent;
		QRect rectView;
		unsigned long delta;
		unsigned int flags;
	};

	typedef QHash<qtractorMidiEvent *, Item *> ItemList;

	// Event selection item lookup.
	Item *findItem(qtractorMidiEvent *pEvent);

	// Event insertion method.
	void addItem(qtractorMidiEvent *pEvent,
		const QRect& rectEvent, const QRect& rectView,
		unsigned long iDeltaTime = 0);

	// Event selection method.
	void selectItem(qtractorMidiEvent *pEvent,
		const QRect& rectEvent, const QRect& rectView,
		bool bSelect = true, bool bToggle = false);

	// Item update method (visual rects).
	void updateItem ( Item *pItem )
	{
		m_rectEvent = m_rectEvent.united(pItem->rectEvent);
		m_rectView  = m_rectView.united(pItem->rectView);
	}

	// The united selection rectangle.
	const QRect& rectEvent() const { return m_rectEvent; }
	const QRect& rectView()  const { return m_rectView;  }

	// Selection list accessor.
	const ItemList& items() const { return m_items; }

	// Selection update method.
	void update(bool bCommit);

	// Selection commit method.
	void commit();

	// Reset event selection.
	void clear();

	qtractorMidiEvent *anchorEvent() const { return m_pAnchorEvent; }
	
private:

	// The clip selection list.
	ItemList m_items;

	// The united selection rectangle.
	QRect m_rectEvent;
	QRect m_rectView;

	// The most probable anchor event.
	qtractorMidiEvent *m_pAnchorEvent;
};


#endif  // __qtractorMidiEditSelect_h


// end of qtractorMidiEditSelect.h
