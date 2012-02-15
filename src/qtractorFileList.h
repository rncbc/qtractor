// qtractorFileList.h
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

#ifndef __qtractorFileList_h
#define __qtractorFileList_h

#include <QString>
#include <QHash>


// Forward declarations.
class qtractorFileListItem;
class qtractorClip;


//----------------------------------------------------------------------
// class qtractorFileList -- file path registry.
//
class qtractorFileList
{
public:

	// Constructor.
	qtractorFileList() {}

	// Destructor.
	~qtractorFileList() { clear(); }

	class Item
	{
	public:

		// Constructor.
		Item(const QString& sPath)
			: m_sPath(sPath), m_iRefCount(0), m_pFileItem(0) {}

		// Key accessors.
		const QString& path() const
			{ return m_sPath; }

		// Payload accessors.
		void setFileItem(qtractorFileListItem *pFileItem)
			{ m_pFileItem = pFileItem; }
		qtractorFileListItem *fileItem() const
			{ return m_pFileItem; }

		void addClip(qtractorClip *pClip)
			{ m_clips.append(pClip); }
		void removeClip(qtractorClip *pClip)
			{ m_clips.removeAll(pClip); }

		const QList<qtractorClip *>& clips() const
			{ return m_clips; }

		// Ref-counting accesor.
		unsigned int refCount() const
			{ return m_iRefCount; }

		// Ref-counting methods.
		void addRef()
			{ ++m_iRefCount; }
		void removeRef()
			{ --m_iRefCount; }

	private:

		// Most interesting variables.
		QString m_sPath;

		unsigned int m_iRefCount;

		// Payload variables.
		qtractorFileListItem *m_pFileItem;

		QList<qtractorClip *> m_clips;
	};


	typedef QHash<QString, Item *> Hash;

	// File/path registry management.
	qtractorFileListItem *findFileItem (const QString& sPath) const;

	void addFileItem(qtractorFileListItem *pFileItem);
	void removeFileItem(qtractorFileListItem *pFileItem);

	// Clip/path registry management.
	void addClipItem(qtractorClip *pClip);
	void removeClipItem(qtractorClip *pClip);

	// Cleanup (dtor).
	void clear();

	// File hash table management.
	Item *findItem(const QString& sPath) const;

protected:

	Item *addItem(const QString& sPath);
	void removeItem(Item *pItem);

private:

	// File hash table.
	Hash m_items;
};

#endif  // __qtractorFileList_h


// end of qtractorFileList.h
