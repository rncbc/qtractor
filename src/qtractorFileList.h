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

	// File types.
	enum Type { Audio = 0, Midi = 1 };

	// File hash key.
	//
	class Key
	{
	public:

		// Constructors.
		Key(Type iType, const QString& sPath)
			: m_iType(iType), m_sPath(sPath) {}
		Key(const Key& key)
			: m_iType(key.type()), m_sPath(key.path()) {}

		// Key accessors.
		Type type() const
			{ return m_iType; }
		const QString& path() const
			{ return m_sPath; }

		// Match descriminator.
		bool operator== (const Key& other) const
		{
			return m_iType == other.type()
				&& m_sPath == other.path();
		}

	private:

		// Interesting variables.
		Type    m_iType;
		QString m_sPath;
	};

	// File hash item.
	//
	class Item
	{
	public:

		// Constructor.
		Item(const Key& key, bool bAutoRemove = false)
			: m_key(key), m_bAutoRemove(bAutoRemove),
				m_iRefCount(0), m_pFileItem(0) {}

		// Key accessors.
		Type type() const
			{ return m_key.type(); }
		const QString& path() const
			{ return m_key.path(); }

		// Auto-destruction flag accessor.
		void setAutoRemove(bool bAutoRemove)
			{ m_bAutoRemove = bAutoRemove; }
		bool isAutoRemove() const
			{ return m_bAutoRemove; }

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
		Key m_key;
		bool m_bAutoRemove;
		unsigned int m_iRefCount;

		// Payload variables.
		qtractorFileListItem *m_pFileItem;
		QList<qtractorClip *> m_clips;
	};

	typedef QHash<Key, Item *> Hash;

	// File/path registry management.
	qtractorFileListItem *findFileItem(Type iType, const QString& sPath) const;
	void addFileItem(Type iType, qtractorFileListItem *pFileItem);
	void removeFileItem(Type iType, qtractorFileListItem *pFileItem);

	// Clip/path registry management.
	Item *findClipItem(Type iType, qtractorClip *pClip) const;
	void addClipItem(Type iType, qtractorClip *pClip, bool bAutoRemove = false);
	void removeClipItem(Type iType, qtractorClip *pClip);

	// Cleanup (dtors).
	void cleanup();
	void clear();

protected:

	// File hash table management.
	Item *findItem(Type iType, const QString& sPath) const;
	Item *addItem(Type iType, const QString& sPath, bool bAutoRemove);
	void removeItem(Item *pItem);

private:

	// File hash table.
	Hash m_items;
};

#endif  // __qtractorFileList_h


// end of qtractorFileList.h
