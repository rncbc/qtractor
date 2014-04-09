// qtractorFileListView.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorFileList.h"

#include <QTreeWidget>


// Forward declarations.
class qtractorFileListView;
class qtractorFileListItem;
class qtractorFileGroupItem;
class qtractorRubberBand;
class qtractorDocument;

class QDomElement;


//----------------------------------------------------------------------------
// qtractorFileListView -- Group/File list view, supporting drag-n-drop.
//

class qtractorFileListView : public QTreeWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorFileListView(qtractorFileList::Type iFileType, QWidget *pParent = 0);

	// Default destructor.
	virtual ~qtractorFileListView();

	// File list type property.
	void setFileType(qtractorFileList::Type iFileType);
	qtractorFileList::Type fileType() const;

	// QListViewItem::type() return values.
	enum ItemType { GroupItem = 1001, FileItem = 1002, ChannelItem = 1003 };

	// Prompt for proper file list open.
	QStringList openFileNames();

	// Add a new group/file item, optionally under a given group.
	qtractorFileGroupItem *addGroupItem(const QString& sName,
		qtractorFileGroupItem *pParentItem = NULL);
	qtractorFileListItem *addFileItem(const QString& sPath,
		qtractorFileGroupItem *pParentItem = NULL);

	// Current group/file item accessors...
	qtractorFileGroupItem *currentGroupItem() const;
	qtractorFileListItem  *currentFileItem() const;

	// Find a group/file item, given its name.
	qtractorFileGroupItem *findGroupItem(const QString& sName) const;
	qtractorFileListItem  *findFileItem(const QString& sPath) const;

	// Make as current selection an existing file item.
	qtractorFileListItem *selectFileItem(const QString& sPath,
		int iChannel = -1);

	// Add a new group item below the current one.
	void newGroup();
	// Add a new file item below the current group one.
	void openFile();
	// Copy/cut current file item(s) to clipboard.
	void copyItem(bool bCut = false);
	// Paste file item(s) from clipboard.
	void pasteItem();
	// Rename current group/file item.
	void renameItem();
	// Remove current group/file item(s).
	void removeItem();

	// Clean-up unused file items.
	void cleanupItem(QTreeWidgetItem *pItem);
	void cleanup();

	// Emit actiovation signal for given item...
	void activateItem(QTreeWidgetItem *pItem = NULL);

	// Master clean-up.
	void clear();

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
	void selected(const QString& sFilename, int iTrackChannel, bool bSelect);
	// File entry activated.
	void activated(const QString& sFilename, int iTrackChannel);

	// Contents change signal;
	void contentsChanged();

protected slots:

	// In-place toggle slot.
	void itemClickedSlot(QTreeWidgetItem *pItem);
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
	virtual qtractorFileListItem *createFileItem(const QString& sPath) = 0;

	// Prompt for proper file list open (pure virtual).
	virtual QStringList getOpenFileNames() = 0;

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// Drag-n-drop stuff -- reimplemented virtual methods.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *pDragLeaveEvent);
	void dropEvent(QDropEvent *pDropEvent);

	// Drag-n-drop stuff.
	bool canDecodeEvent(QDropEvent *pDropEvent);
	bool canDropItem(QTreeWidgetItem *pDropItem) const;
	QTreeWidgetItem *dragDropItem(const QPoint& pos);

	// Ensure given item is brought to viewport visibility...
	void ensureVisibleItem(QTreeWidgetItem *pItem);

	// Show and move rubber-band item.
	void moveRubberBand(QTreeWidgetItem *pDropItem, bool bOutdent = false);

	// Drag-and-drop target method...
	QTreeWidgetItem *dropItem(QTreeWidgetItem *pDropItem,
		QTreeWidgetItem *pDragItem, bool bOutdent = false);

	// Internal recursive loaders/savers...
	bool loadListElement(qtractorDocument *pDocument,
		QDomElement *pElement, QTreeWidgetItem *pItem);
	bool saveListElement(qtractorDocument *pDocument,
		QDomElement *pElement, QTreeWidgetItem *pItem);

private:

	// Major file type property.
	qtractorFileList::Type m_iFileType;

	// Auto-open timer.
	int     m_iAutoOpenTimeout;
	QTimer *m_pAutoOpenTimer;

	// The point from where drag started.
	QPoint m_posDrag;
	// Item we'll eventually drag around.
	QTreeWidgetItem *m_pDragItem;
	// Item we'll eventually drop something.
	QTreeWidgetItem *m_pDropItem;

	// To show the point where drop will go.
	qtractorRubberBand *m_pRubberBand;

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
	qtractorFileGroupItem(const QString& sName,
		int iType = qtractorFileListView::GroupItem);
	// Default destructor.
	virtual ~qtractorFileGroupItem();

	// Instance accessors.
	void setName(const QString& sName);
	QString name() const;

	qtractorFileListView  *listView() const;
	qtractorFileGroupItem *groupItem() const;

	// To show up whether its open or not.
	void setOpen(bool bOpen);
	bool isOpen() const;

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
	qtractorFileListItem(const QString& sPath);
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

	// File chhannel accessor.
	unsigned short channel() const;

	// Full path accessor.
	const QString path() const;

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

	struct Item {
		QString path;
		unsigned short channel;
	};

	typedef QList<Item> List;

	// Encode method.
	static void encode(QMimeData *pMimeData, const List& items);

	// Decode methods.
	static bool canDecode(const QMimeData *pMimeData);
	static List decode(const QMimeData *pMimeData);
};


#endif  // __qtractorFileListView_h

// end of qtractorFileListView.h
