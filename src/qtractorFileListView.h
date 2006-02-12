// qtractorFileListView.h
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorFileListView_h
#define __qtractorFileListView_h

#include <qdragobject.h>
#include <qlistview.h>
#include <qheader.h>
#include <qptrlist.h>
#include <qtooltip.h>


// Forward declarations.
class qtractorFileListView;
class qtractorDocument;

class QDomElement;
class QAction;


//----------------------------------------------------------------------
// class qtractorFileListViewToolTip -- custom list view tooltips.
//

class qtractorFileListViewToolTip : public QToolTip
{
public:

	// Constructor.
	qtractorFileListViewToolTip(qtractorFileListView *pListView);
	// Virtual destructor.
	virtual ~qtractorFileListViewToolTip() {}

protected:

	// Tooltip handler.
	void maybeTip(const QPoint& pos);

private:

	// The actual parent widget holder.
	qtractorFileListView *m_pListView;
};


//----------------------------------------------------------------------
// class qtractorFileGroupItem -- custom group list view item.
//

class qtractorFileGroupItem : public QListViewItem
{
public:

	// Constructors.
	qtractorFileGroupItem(qtractorFileListView *pListView,
		const QString& sName);
	qtractorFileGroupItem(qtractorFileGroupItem *pGroupItem,
		const QString& sName);
	// Default destructor.
	virtual ~qtractorFileGroupItem();

	// Instance accessors.
	void setName(const QString& sName);
	QString name() const;

	qtractorFileListView  *listView() const;
	qtractorFileGroupItem *groupItem() const;

	// To show up whether its open or not.
	virtual void setOpen(bool bOpen);

	// To virtually distinguish between list view items.
	virtual int rtti() const;

	// Virtual tooltip renderer.
	virtual QString toolTip() const;
};


//----------------------------------------------------------------------
// class qtractorFileListItem -- custom file list view item.
//

class qtractorFileListItem : public qtractorFileGroupItem
{
public:

	// Constructors.
	qtractorFileListItem(qtractorFileListView *pListView,
		const QString& sPath);
	qtractorFileListItem(qtractorFileGroupItem *pGroupItem,
		const QString& sPath);
	// Default destructor.
	virtual ~qtractorFileListItem();

	// To virtually distinguish between list view items.
	virtual int rtti() const;

	// Full path accessor.
	const QString& path() const;

private:

	// File item full path.
	QString m_sPath;
};


//----------------------------------------------------------------------
// class qtractorFileChannelItem -- custom channel list view item.
//

class qtractorFileChannelItem : public qtractorFileGroupItem
{
public:

	// Constructors.
	qtractorFileChannelItem(qtractorFileListItem *pFileItem,
		const QString& sName, unsigned short iChannel);
	// Default destructor.
	virtual ~qtractorFileChannelItem();

	// To virtually distinguish between list view items.
	virtual int rtti() const;

	// Filoe chhannel accessor.
	unsigned short channel() const;

private:

	// File channel identifier.
	unsigned short m_iChannel;
};


//----------------------------------------------------------------------
// class qtractorFileChannelDrag -- custom file channel drag object.
//

class qtractorFileChannelDrag : public QStoredDrag
{
public:

	// Constructor.
	qtractorFileChannelDrag(const QString& sPath, unsigned short iChannel,
		QWidget *pDragSource = NULL, const char *pszName = NULL);

	// Decode methods.
	static bool canDecode(const QMimeSource *pMimeSource);
	static bool decode(const QMimeSource *pMimeSource,
		QString& sPath, unsigned short *piChannel);
};


//----------------------------------------------------------------------------
// qtractorFileListView -- Group/File list view, supporting drag-n-drop.
//

class qtractorFileListView : public QListView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorFileListView(QWidget *pParent, const char *pszName = NULL);
	// Default destructor.
	virtual ~qtractorFileListView();

	// QListViewItem::rtti() return values.
	enum ItemType { GroupItem = 1001, FileItem = 1002, ChannelItem = 1003 };

	// Prompt for proper file list open.
	QStringList openFileNames();

	// Add a new group/file item, optionally under a given group.
	qtractorFileGroupItem *addGroupItem(const QString& sName,
		qtractorFileGroupItem *pParentItem = NULL);
	qtractorFileListItem *addFileItem(const QString& sPath,
		qtractorFileGroupItem *pParentItem = NULL);

	// Find a group/file item, given its name.
	qtractorFileGroupItem *findGroupItem(const QString& sName) const;
	qtractorFileListItem  *findFileItem(const QString& sPath) const;

	// Auto-open timer methods.
	void setAutoOpenTimeout(int iAutoOpenTimeout);
	int autoOpenTimeout() const;

	// Recently used directory, if any.
	void setRecentDir(const QString& sRecentDir);
	const QString& recentDir() const;

	// Elemental loader/saver...
	bool loadElement(qtractorDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument,
		QDomElement *pElement);

signals:

	// File entry activated.
	void activated(const QString& sFilename);
	// Contents change signal;
	void contentsChanged();

public slots:

	// Add a new file item below the current group one.
	void openFileSlot();

protected slots:

	// Add a new group item below the current one.
	void newGroupSlot();

	// Rename current group/file item.
	void renameItemSlot();
	// Remove current group/file item.
	void deleteItemSlot();

	// In-place selection slot.
	void selectionChangedSlot();
	// In-place activate slot.
	void activatedSlot(QListViewItem *pItem);
	// In-place aliasing slot.
	void renamedSlot(QListViewItem *pItem);

	// Auto-open timeout slot.
	void timeoutSlot();

protected:

	// Find a list view item, given its type and name.
	QListViewItem *findItem(ItemType type, const QString& sText) const;

	// Find and return the nearest group item...
	qtractorFileGroupItem *groupItem(QListViewItem *pItem) const;

	// Pure virtual file item creation;
	// must be implemented in derived classes.
	virtual qtractorFileListItem *createFileItem(const QString& sPath,
		qtractorFileGroupItem *pParentItem) = 0;

	// Prompt for proper file list open (pure virtual).
	virtual QStringList getOpenFileNames() = 0;

	// Drag-n-drop stuff -- reimplemented virtual methods.
	QDragObject *dragObject();
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *);
	void dropEvent(QDropEvent *pDropEvent);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Internal recursive loaders/savers...
	bool loadListElement(qtractorDocument *pDocument,
		QDomElement *pElement, QListViewItem *pItem);
	bool saveListElement(qtractorDocument *pDocument,
		QDomElement *pElement, QListViewItem *pItem);

private:

	// Drag-n-drop stuff.
	bool canDecodeEvent(QDropEvent *pDropEvent);
	bool canDropItem(QListViewItem *pItem) const;
	QListViewItem *dragDropItem(const QPoint& pos);

	// Auto-open timer.
	int     m_iAutoOpenTimeout;
	QTimer *m_pAutoOpenTimer;

	// Item we'll eventually drag around.
	QListViewItem *m_pDragItem;
	// Item we'll eventually drop something.
	QListViewItem *m_pDropItem;

	// Listview item tooltip.
	qtractorFileListViewToolTip *m_pToolTip;

	// List view actions.
	QAction *m_pNewGroupAction;
	QAction *m_pOpenFileAction;
	QAction *m_pRenameItemAction;
	QAction *m_pDeleteItemAction;
	
	// Last recently used directory.
	QString m_sRecentDir;
};


#endif  // __qtractorFileListView_h

// end of qtractorFileListView.h
