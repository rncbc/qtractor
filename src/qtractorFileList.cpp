// qtractorFileList.cpp
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorFileList.h"

#include "qtractorFileListView.h"

#include "qtractorClip.h"


//----------------------------------------------------------------------
// class qtractorFileList -- file path registry.
//

// File path registry management.
qtractorFileListItem *qtractorFileList::findFileItem (
	const QString& sPath ) const
{
	Item *pItem = findItem(sPath);
	return (pItem ? pItem->fileItem() : NULL);
}


void qtractorFileList::addFileItem (
	qtractorFileListItem *pFileItem )
{
	Item *pItem = addItem(pFileItem->path());
	if (pItem)
		pItem->setFileItem(pFileItem);
}


void qtractorFileList::removeFileItem (
	qtractorFileListItem *pFileItem )
{
	Item *pItem = findItem(pFileItem->path());
	if (pItem) {
		pItem->setFileItem(NULL);
		removeItem(pItem);
	}
}


// Clip/path registry management.
void qtractorFileList::addClipItem ( qtractorClip *pClip )
{
	Item *pItem = addItem(pClip->filename());
	if (pItem)
		pItem->addClip(pClip);
}


void qtractorFileList::removeClipItem ( qtractorClip *pClip )
{
	Item *pItem = findItem(pClip->filename());
	if (pItem) {
		pItem->removeClip(pClip);
		removeItem(pItem);
	}
}


// File hash table management.
qtractorFileList::Item *qtractorFileList::findItem ( const QString& sPath ) const
{
	return m_items.value(sPath, NULL);
}


qtractorFileList::Item *qtractorFileList::addItem ( const QString& sPath )
{
	Item *pItem = NULL;

	Hash::ConstIterator iter = m_items.constFind(sPath);
	if (iter == m_items.constEnd()) {
		pItem = new Item(sPath);
		m_items.insert(sPath, pItem);
	}
	else pItem = iter.value();

	pItem->addRef();

	return pItem;
}


void qtractorFileList::removeItem ( qtractorFileList::Item *pItem )
{
	pItem->removeRef();

	if (pItem->refCount() < 1) {
		m_items.remove(pItem->path());
		delete pItem;
	}
}


void qtractorFileList::clear (void)
{
	qDeleteAll(m_items);
	m_items.clear();
}


// end of qtractorFileList.cpp

