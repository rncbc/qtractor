// qtractorFileList.h
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

#ifndef __qtractorFileList_h
#define __qtractorFileList_h

#include <QString>
#include <QFileInfo>
#include <QHash>


// Forward declarations.
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
			: m_iType(iType), m_info(sPath) {}
		Key(const Key& key)
			: m_iType(key.type()), m_info(key.info()) {}

		// Key accessors.
		Type type() const
			{ return m_iType; }
		const QFileInfo& info() const
			{ return m_info; }

		// Key helpers.
		QString path() const
			{ return m_info.absoluteFilePath(); }

		// Match descriminator.
		bool operator== (const Key& key) const
			{ return m_iType == key.type() && m_info == key.info(); }

	private:

		// Interesting variables.
		Type m_iType;
		QFileInfo m_info;
	};

	// File hash item.
	//
	class Item
	{
	public:

		// Constructor.
		Item(const Key& key, bool bAutoRemove = false)
			: m_key(key), m_bAutoRemove(bAutoRemove),
				m_iClipRefCount(0), m_iRefCount(0) {}

		// Key accessors.
		Type type() const
			{ return m_key.type(); }
		const QFileInfo& info() const
			{ return m_key.info(); }

		// Key helpers.
		QString path() const
			{ return m_key.path(); }

		// Auto-destruction flag accessor.
		void setAutoRemove(bool bAutoRemove)
			{ m_bAutoRemove = bAutoRemove; }
		bool isAutoRemove() const
			{ return m_bAutoRemove; }

		// Clip ref-counting methods.
		void addClipRef()
			{ ++m_iClipRefCount; }
		void removeClipRef()
			{ --m_iClipRefCount; }

		unsigned int clipRefCount() const
			{ return m_iClipRefCount; }

		// Ref-counting methods.
		unsigned int refCount() const
			{ return m_iRefCount; }

		void addRef()
			{ ++m_iRefCount; }
		void removeRef()
			{ --m_iRefCount; }

	private:

		// Most interesting variables.
		Key m_key;
		bool m_bAutoRemove;
		unsigned int m_iClipRefCount;
		unsigned int m_iRefCount;
	};

	typedef QHash<Key, Item *> Hash;

	// File/path registry management.
	void addFileItem(Type iType, const QString& sPath, bool bAutoRemove = false);
	void removeFileItem(Type iType, const QString& sPath);

	// Clip/path registry management.
	void addClipItem(Type iType, const QString& sPath, bool bAutoRemove = false);
	void addClipItem(Type iType, qtractorClip *pClip, bool bAutoRemove = false);
	void addClipItemEx(Type iType, qtractorClip *pClip, bool bAutoRemove = false);
	void removeClipItem(Type iType, qtractorClip *pClip);

	Item *findItem(Type iType, const QString& sPath) const;

	// Cleanup (dtors).
	void cleanup(bool bForce);
	void clear();

protected:

	// File hash table management.
	Item *addItem(Type iType, const QString& sPath, bool bAutoRemove);
	void removeItem(Item *pItem);

private:

	// File hash table.
	Hash m_items;
};

#endif  // __qtractorFileList_h


// end of qtractorFileList.h
