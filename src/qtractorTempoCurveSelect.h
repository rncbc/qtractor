// qtractorTempoCurveSelect.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2018, spog aka Samo Pogačnik. All rights reserved.

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

#ifndef __qtractorTempoCurveSelect_h
#define __qtractorTempoCurveSelect_h

#include "qtractorTempoCurve.h"

#include <QHash>
#include <QRect>


//-------------------------------------------------------------------------
// qtractorTempoCurveSelect -- MIDI event selection capsule.

class qtractorTempoCurveSelect
{
public:

	// Constructor.
	qtractorTempoCurveSelect();

	// Default destructor.
	~qtractorTempoCurveSelect();

	// Selection item struct.
	struct Item
	{
		// Item constructor.
		Item(const QRect& rect): rectNode(rect), flags(1) {}

		// Item members.
		QRect rectNode;
		unsigned int flags;
	};

	typedef QHash<qtractorTimeScale::Node *, Item *> ItemList;

	// Event selection item lookup.
	Item *findItem(qtractorTimeScale::Node *pNode);

	// Event insertion method.
	void addItem(qtractorTimeScale::Node *pNode, const QRect& rectNode);

	// Event removal method.
	void removeItem(qtractorTimeScale::Node *pNode);

	// Event selection method.
	void selectItem(qtractorTempoCurve *pTempoCurve,
		qtractorTimeScale::Node *pNode, const QRect& rectNode,
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
	void setTempoCurve ( qtractorTempoCurve *pTempoCurve )
		{ m_pTempoCurve = pTempoCurve; }
	qtractorTempoCurve *tempoCurve() const
		{ return m_pTempoCurve; }

	qtractorTimeScale::Node *anchorNode() const
		{ return m_pAnchorNode; }

	// Selection curve testimony.
	bool isCurrentCurve ( qtractorTempoCurve *pTempoCurve ) const
		{ return (m_pTempoCurve && m_pTempoCurve == pTempoCurve); }

private:

	// The node selection list.
	ItemList m_items;

	// The united selection rectangle.
	QRect m_rect;

	// There must be only one curve.
	qtractorTempoCurve *m_pTempoCurve;

	// The most probable anchor node.
	qtractorTimeScale::Node *m_pAnchorNode;
};


#endif  // __qtractorTempoCurveSelect_h


// end of qtractorTempoCurveSelect.h
