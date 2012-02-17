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

#include <QFile>


//---------------------------------------------------------------------
// class qtractorFileList::Key -- file hash key.
//

uint qHash ( const qtractorFileList::Key& key )
{
	return qHash(key.type()) ^ qHash(key.path());
}


//----------------------------------------------------------------------
// class qtractorFileList -- file path registry.
//

// File path registry management.
qtractorFileListItem *qtractorFileList::findFileItem (
	qtractorFileList::Type iType, const QString& sPath ) const
{
	Item *pItem = findItem(iType, sPath);
	return (pItem ? pItem->fileItem() : NULL);
}


void qtractorFileList::addFileItem (
	qtractorFileList::Type iType, qtractorFileListItem *pFileItem )
{
	Item *pItem = addItem(iType, pFileItem->path(), false);
	if (pItem) {
		pItem->setFileItem(pFileItem);
	#ifdef CONFIG_DEBUG
		qDebug("qtractorFileList::addFileItem(%d, \"%s\") refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(),
			pItem->refCount(), pItem->clips().count(),
			int(pItem->isAutoRemove()));
	#endif
	}
}


void qtractorFileList::removeFileItem (
	qtractorFileList::Type iType, qtractorFileListItem *pFileItem )
{
	Item *pItem = findItem(iType, pFileItem->path());
	if (pItem) {
		pItem->setFileItem(NULL);
	#ifdef CONFIG_DEBUG
		qDebug("qtractorFileList::removeFileItem(%d, \"%s\") refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(),
			pItem->refCount() - 1, pItem->clips().count(),
			int(pItem->isAutoRemove()));
	#endif
		removeItem(pItem);
	}
}


// Clip/path registry management.
qtractorFileList::Item *qtractorFileList::findClipItem (
	qtractorFileList::Type iType, qtractorClip *pClip ) const
{
	Item *pItem = findItem(iType, pClip->filename());
	return (pItem && pItem->clips().contains(pClip) ? pItem : NULL);
}


void qtractorFileList::addClipItem (
	qtractorFileList::Type iType, qtractorClip *pClip, bool bAutoRemove )
{
	Item *pItem = addItem(iType, pClip->filename(), bAutoRemove);
	if (pItem) {
		pItem->addClip(pClip);
	#ifdef CONFIG_DEBUG
		qDebug("qtractorFileList::addClipItem(%d, \"%s\") refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(),
			pItem->refCount(), pItem->clips().count(),
			int(pItem->isAutoRemove()));
	#endif
	}
}


void qtractorFileList::removeClipItem (
	qtractorFileList::Type iType, qtractorClip *pClip )
{
	Item *pItem = findItem(iType, pClip->filename());
	if (pItem) {
		pItem->removeClip(pClip);
	#ifdef CONFIG_DEBUG
		qDebug("qtractorFileList::removeClipItem(%d, \"%s\") refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(),
			pItem->refCount() - 1, pItem->clips().count(),
			int(pItem->isAutoRemove()));
	#endif
		removeItem(pItem);
	}
}


// File hash table management.
qtractorFileList::Item *qtractorFileList::findItem (
	qtractorFileList::Type iType, const QString& sPath ) const
{
	return m_items.value(Key(iType, sPath), NULL);
}


qtractorFileList::Item *qtractorFileList::addItem (
	qtractorFileList::Type iType, const QString& sPath, bool bAutoRemove )
{
	Item *pItem = NULL;

	Key key(iType, sPath);
	Hash::ConstIterator iter = m_items.constFind(key);
	if (iter == m_items.constEnd()) {
		pItem = new Item(key, bAutoRemove);
		m_items.insert(key, pItem);
		if (bAutoRemove) // HACK!
			pItem->addRef();
	}
	else pItem = iter.value();

	pItem->addRef();

	return pItem;
}


void qtractorFileList::removeItem ( qtractorFileList::Item *pItem )
{
	pItem->removeRef();

	if (pItem->refCount() < 1) {
		m_items.remove(Key(pItem->type(), pItem->path()));
		delete pItem;
	}
}


void qtractorFileList::cleanup (void)
{
	Hash::ConstIterator iter = m_items.begin();
	for (; iter != m_items.constEnd(); ++iter) {
		Item *pItem = iter.value();
		if (pItem->isAutoRemove()) {
			const QString& sPath = pItem->path();
		#ifdef CONFIG_DEBUG
			qDebug("qtractorFileList::cleanup(%d, \"%s\") refCount=%d clips=%d",
				int(pItem->type()), pItem->path().toUtf8().constData(),
				pItem->refCount(), pItem->clips().count());
		#endif
			QFile::remove(sPath); // kill!
			pItem->setAutoRemove(false);
			pItem->removeRef();
		}
	}
}


void qtractorFileList::clear (void)
{
	qDeleteAll(m_items);
	m_items.clear();
}


// end of qtractorFileList.cpp

