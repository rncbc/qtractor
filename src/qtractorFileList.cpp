// qtractorFileList.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
void qtractorFileList::addFileItem (
	qtractorFileList::Type iType, const QString& sPath, bool bAutoRemove )
{
	Item *pItem = addItem(iType, sPath, bAutoRemove);
	if (pItem) {
		pItem->addRef();
#ifdef CONFIG_DEBUG_0
		qDebug("qtractorFileList::addFileItem(%d, \"%s\", %d) refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(), int(bAutoRemove),
			pItem->refCount(), pItem->clipRefCount(), int(pItem->isAutoRemove()));
#endif
	}
}


void qtractorFileList::removeFileItem (
	qtractorFileList::Type iType, const QString& sPath )
{
	Item *pItem = findItem(iType, sPath);
	if (pItem) {
		pItem->removeRef();
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorFileList::removeFileItem(%d, \"%s\") refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(),
			pItem->refCount(), pItem->clipRefCount(), int(pItem->isAutoRemove()));
	#endif
		removeItem(pItem);
	}
}


// Clip/path registry management.
void qtractorFileList::addClipItem (
	qtractorFileList::Type iType, const QString& sPath, bool bAutoRemove )
{
	Item *pItem = addItem(iType, sPath, bAutoRemove);
	if (pItem) {
		pItem->addClipRef();
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorFileList::addClipItem(%d, \"%s\", %d) refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(), int(bAutoRemove),
			pItem->refCount(), pItem->clipRefCount(), int(pItem->isAutoRemove()));
	#endif
	}
}


void qtractorFileList::addClipItem (
	qtractorFileList::Type iType, qtractorClip *pClip, bool bAutoRemove )
{
	addClipItem(iType, pClip->filename(), bAutoRemove);
}


void qtractorFileList::addClipItemEx (
	qtractorFileList::Type iType, qtractorClip *pClip, bool bAutoRemove )
{
	Item *pItem = addItem(iType, pClip->filename(), bAutoRemove);
	if (pItem) {
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorFileList::addClipItemEx(%d, \"%s\", %d) refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(), int(bAutoRemove),
			pItem->refCount(), pItem->clipRefCount(), int(pItem->isAutoRemove()));
	#endif
	}
}


void qtractorFileList::removeClipItem (
	qtractorFileList::Type iType, qtractorClip *pClip )
{
	Item *pItem = findItem(iType, pClip->filename());
	if (pItem) {
		pItem->removeClipRef();
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorFileList::removeClipItem(%d, \"%s\") refCount=%d clips=%d (%d)",
			int(pItem->type()), pItem->path().toUtf8().constData(),
			pItem->refCount(), pItem->clipRefCount(), int(pItem->isAutoRemove()));
	#endif
	}
}


// File hash table management.
qtractorFileList::Item *qtractorFileList::findItem (
	qtractorFileList::Type iType, const QString& sPath ) const
{
	return m_items.value(Key(iType, sPath), nullptr);
}


qtractorFileList::Item *qtractorFileList::addItem (
	qtractorFileList::Type iType, const QString& sPath, bool bAutoRemove )
{
	Key key(iType, sPath);
	Item *pItem = m_items.value(key, nullptr);
	if (pItem == nullptr) {
		pItem = new Item(key, bAutoRemove);
		m_items.insert(key, pItem);
	}
	else
	if (bAutoRemove && !pItem->isAutoRemove())
		pItem->setAutoRemove(true);

	return pItem;
}


void qtractorFileList::removeItem ( qtractorFileList::Item *pItem )
{
//	pItem->removeRef();

	if (pItem->refCount() < 1) {
		m_items.remove(Key(pItem->type(), pItem->path()));
		delete pItem;
	}
}


void qtractorFileList::cleanup ( bool bForce )
{
	Hash::ConstIterator iter = m_items.constBegin();
	const Hash::ConstIterator& iter_end = m_items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		Item *pItem = iter.value();
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorFileList::cleanup(%d, \"%s\") autoRemove=%d refCount=%d clips=%d",
			int(pItem->type()), pItem->path().toUtf8().constData(),
			int(pItem->isAutoRemove()), pItem->refCount(), pItem->clipRefCount());
	#endif
		if (pItem->isAutoRemove()) {
			if (!bForce && pItem->clipRefCount() > 0) {
				pItem->setAutoRemove(false);
			//	pItem->removeClipRef();
			}
			else
			// Time for the kill...?
			if (bForce/* && pItem->clipRefCount() < 1*/) {
				QFile::remove(pItem->path());
			}
		}
	}
}


void qtractorFileList::clear (void)
{
	qDeleteAll(m_items);
	m_items.clear();
}


// end of qtractorFileList.cpp
