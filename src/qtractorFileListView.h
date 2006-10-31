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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __qtractorFileListView_h
#define __qtractorFileListView_h

#include <QTreeWidget>


// Forward declarations.
class qtractorFileListView;
class qtractorFileGroupItem;
class qtractorFileListItem;
class qtractorDocument;

class QDomElement;
class QAction;


//----------------------------------------------------------------------------
// qtractorFileListView -- Group/File list view, supporting drag-n-drop.
//

class qtractorFileListView : public QTreeWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorFileListView(QWidget *pParent = 0);
	// Default destructor.
	virtual ~qtractorFileListView();

	// QListViewItem::type() return values.
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

	// Make as current selection an existing file item.
	qtractorFileListItem *selectFileItem(const QString& sPath,
		int iChannel = -1);

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
	void currentItemChangedSlot();
	// In-place activate slot.
	void itemActivatedSlot(QTreeWidgetItem *pItem);
	// In-place open/close slot.
	void itemExpandedSlot(QTreeWidgetItem *pItem);
	void itemCollapsedSlot(QTreeWidgetItem *pItem);
	// Tracking of item changes (e.g in-place edits).
	void itemRenamedSlot();

	// Auto-open timeout slot.
	void timeoutSlot();

protected:

	// Find and return the nearest group item...
	qtractorFileGroupItem *groupItem(QTreeWidgetItem *pItem) const;

	// Find a list view item, given its type and name.
	QTreeWidgetItem *findItem(const QString& sText, int iType) const;

	// Which column is the complete file path?
	virtual int pathColumn() const = 0;

	// Pure virtual file item creation;
	// must be implemented in derived classes.
	virtual qtractorFileListItem *createFileItem(const QString& sPath,
		qtractorFileGroupItem *pParentItem) = 0;

	// Prompt for proper file list open (pure virtual).
	virtual QStringList getOpenFileNames() = 0;

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// Drag-n-drop stuff -- reimplemented virtual methods.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *);
	void dropEvent(QDropEvent *pDropEvent);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Internal recursive loaders/savers...
	bool loadListElement(qtractorDocument *pDocument,
		QDomElement *pElement, QTreeWidgetItem *pItem);
	bool saveListElement(qtractorDocument *pDocument,
		QDomElement *pElement, QTreeWidgetItem *pItem);

private:

	// Drag-n-drop stuff.
	bool canDecodeEvent(QDropEvent *pDropEvent);
	bool canDropItem(QTreeWidgetItem *pDropItem) const;
	QTreeWidgetItem *dragDropItem(const QPoint& pos);

	// Auto-open timer.
	int     m_iAutoOpenTimeout;
	QTimer *m_pAutoOpenTimer;

	// The point from where drag started.
	QPoint m_posDrag;
	// Item we'll eventually drag around.
	QTreeWidgetItem *m_pDragItem;
	// Item we'll eventually drop something.
	QTreeWidgetItem *m_pDropItem;

	// List view actions.
	QAction *m_pNewGroupAction;
	QAction *m_pOpenFileAction;
	QAction *m_pRenameItemAction;
	QAction *m_pDeleteItemAction;
	
	// Last recently used directory.
	QString m_sRecentDir;
};


//----------------------------------------------------------------------
// class qtractorFileGroupItem -- custom group list view item.
//

class qtractorFileGroupItem : public QTreeWidgetItem
{
public:

	// Constructors.
	qtractorFileGroupItem(qtractorFileListView *pListView,
		const QString& sName, int iType = qtractorFileListView::GroupItem);
	qtractorFileGroupItem(qtractorFileGroupItem *pGroupItem,
		const QString& sName, int iType = qtractorFileListView::GroupItem);
	// Default destructor.
	virtual ~qtractorFileGroupItem();

	// Instance accessors.
	void setName(const QString& sName);
	QString name() const;

	qtractorFileListView  *listView() const;
	qtractorFileGroupItem *groupItem() const;

	// To show up whether its open or not.
	void setOpen(bool bOpen);

	// Virtual tooltip renderer.
	virtual QString toolTip() const;

protected:

	// Common group-item initializer.
	void initFileGroupItem(const QString& sName, int iType);
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
	~qtractorFileListItem();

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
	~qtractorFileChannelItem();

	// Filoe chhannel accessor.
	unsigned short channel() const;

private:

	// File channel identifier.
	unsigned short m_iChannel;
};


//----------------------------------------------------------------------
// class qtractorFileChannelDrag -- custom file channel drag object.
//

class qtractorFileChannelDrag
{
public:

	// Encode method.
	static void encode(QMimeData *pMimeData,
		const QString& sPath, unsigned short iChannel);

	// Decode methods.
	static bool canDecode(const QMimeData *pMimeData);
	static bool decode(const QMimeData *pMimeData,
		QString& sPath, unsigned short *piChannel);
};


#endif  // __qtractorFileListView_h

// end of qtractorFileListView.h
