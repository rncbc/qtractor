// qtractorTempoCurveSelect.cpp
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

#include "qtractorTempoCurveSelect.h"


//-------------------------------------------------------------------------
// qtractorTempoCurveSelect -- MIDI event selection capsule.

// Constructor.
qtractorTempoCurveSelect::qtractorTempoCurveSelect (void)
{
	m_pTempoCurve = NULL;
	m_pAnchorNode = NULL;
}


// Default destructor.
qtractorTempoCurveSelect::~qtractorTempoCurveSelect (void)
{
	clear();
}


// Event selection item lookup.
qtractorTempoCurveSelect::Item *qtractorTempoCurveSelect::findItem (
	qtractorTimeScale::Node *pNode )
{
	// Check if this very event already exists...
	return m_items.value(pNode, NULL);
}


// Item insertion method.
void qtractorTempoCurveSelect::addItem (
	qtractorTimeScale::Node *pNode, const QRect& rectNode )
{
	m_items.insert(pNode, new Item(rectNode));

	m_rect = m_rect.united(rectNode);
	
	if (m_pAnchorNode == NULL || m_pAnchorNode->frame > pNode->frame)
		m_pAnchorNode = pNode;
}


// Item removal method.
void qtractorTempoCurveSelect::removeItem ( qtractorTimeScale::Node *pNode )
{
	ItemList::Iterator iter = m_items.find(pNode);
	if (iter != m_items.end()) {
		delete iter.value();
		m_items.erase(iter);
		commit();
	}
}


// Item selection method.
void qtractorTempoCurveSelect::selectItem ( qtractorTempoCurve *pTempoCurve,
	qtractorTimeScale::Node *pNode, const QRect& rectNode,
	bool bSelect, bool bToggle )
{
	Item *pItem = findItem(pNode);
	if (pItem) {
		const unsigned int flags = pItem->flags;
		if ( (!bSelect && (flags & 2) == 0) ||
			(( bSelect && (flags & 3) == 3) && bToggle))
			pItem->flags &= ~1;
		else
		if ( ( bSelect && (flags & 2) == 0) ||
			((!bSelect && (flags & 3) == 2) && bToggle))
			pItem->flags |=  1;
	}
	else
	if (bSelect) {
		if (m_pTempoCurve == NULL)
			m_pTempoCurve  = pTempoCurve;
		if (m_pTempoCurve == pTempoCurve)
			addItem(pNode, rectNode);
	}
}


// Selection commit method.
void qtractorTempoCurveSelect::update ( bool bCommit )
{
	// Remove unselected...
	int iUpdate = 0;

	ItemList::Iterator iter = m_items.begin();
	const ItemList::Iterator& iter_end = m_items.end();
	while (iter != iter_end) {
		Item *pItem = iter.value();
		if (bCommit) {
			if (pItem->flags & 1)
				pItem->flags |=  2;
			else
				pItem->flags &= ~2;
		}
		if ((pItem->flags & 3) == 0) {
			delete pItem;
			iter = m_items.erase(iter);
			++iUpdate;
		}
		else ++iter;
	}

	// Did we remove any?
	if (iUpdate > 0)
		commit();
}


// Selection commit method.
void qtractorTempoCurveSelect::commit (void)
{
	// Reset united selection rectangle...
	m_rect.setRect(0, 0, 0, 0);

	ItemList::ConstIterator iter = m_items.constBegin();
	const ItemList::ConstIterator iter_end = m_items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		Item *pItem = iter.value();
		m_rect = m_rect.united(pItem->rectNode);
	}

	if (m_items.isEmpty()) {
		m_pAnchorNode = NULL;
		m_pTempoCurve = NULL;
	}
}


// Reset event selection.
void qtractorTempoCurveSelect::clear (void)
{
	m_rect.setRect(0, 0, 0, 0);

	qDeleteAll(m_items);
	m_items.clear();

	m_pAnchorNode = NULL;
	m_pTempoCurve = NULL;
}


// end of qtractorTempoCurveSelect.cpp
