// qtractorCurveSelect.h
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

#ifndef __qtractorCurveSelect_h
#define __qtractorCurveSelect_h

#include "qtractorCurve.h"

#include <QHash>
#include <QRect>


//-------------------------------------------------------------------------
// qtractorCurveSelect -- MIDI event selection capsule.

class qtractorCurveSelect
{
public:

	// Constructor.
	qtractorCurveSelect();

	// Default destructor.
	~qtractorCurveSelect();

	// Selection item struct.
	struct Item
	{
		// Item constructor.
		Item(const QRect& rect): rectNode(rect), flags(1) {}

		// Item members.
		QRect rectNode;
		unsigned int flags;
	};

	typedef QHash<qtractorCurve::Node *, Item *> ItemList;

	// Event selection item lookup.
	Item *findItem(qtractorCurve::Node *pNode);

	// Event insertion method.
	void addItem(qtractorCurve::Node *pNode, const QRect& rectNode);

	// Event removal method.
	void removeItem(qtractorCurve::Node *pNode);

	// Event selection method.
	void selectItem(qtractorCurve *pCurve,
		qtractorCurve::Node *pNode, const QRect& rectNode,
		bool bSelect = true, bool bToggle = false);

	// The united selection rectangle.
	const QRect& rect() const { return m_rect; }

	// Selection list accessor.
	const ItemList& items() const { return m_items; }

	// Selection update method.
	void update(bool bCommit);

	// Selection commit method.
	void commit();

	// Reset event selection.
	void clear();

	// Selection curve accessors.
	void setCurve ( qtractorCurve *pCurve )
		{ m_pCurve = pCurve; }
	qtractorCurve *curve() const
		{ return m_pCurve; }

	qtractorCurve::Node *anchorNode() const
		{ return m_pAnchorNode; }

	// Selection curve testimony.
	bool isCurrentCurve ( qtractorCurve *pCurve ) const
		{ return (m_pCurve && m_pCurve == pCurve); }

private:

	// The node selection list.
	ItemList m_items;

	// The united selection rectangle.
	QRect m_rect;

	// There must be only one curve.
	qtractorCurve *m_pCurve;

	// The most probable anchor node.
	qtractorCurve::Node *m_pAnchorNode;
};


#endif  // __qtractorCurveSelect_h


// end of qtractorCurveSelect.h
