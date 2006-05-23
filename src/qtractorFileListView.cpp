// qtractorFileListView.cpp
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

#include "qtractorFileListView.h"

#include "qtractorDocument.h"

#include <qtimer.h>
#include <qpixmap.h>
#include <qaction.h>
#include <qfileinfo.h>
#include <qfiledialog.h>
#include <qpopupmenu.h>


//----------------------------------------------------------------------
// class qtractorFileListViewToolTip -- custom list view tooltips.
//

// Constructor.
qtractorFileListViewToolTip::qtractorFileListViewToolTip (
	qtractorFileListView *pListView )
	: QToolTip(pListView->viewport())
{
	m_pListView = pListView;
}


// Tooltip handler.
void qtractorFileListViewToolTip::maybeTip ( const QPoint& pos )
{
	QListViewItem *pItem = m_pListView->itemAt(pos);
	if (pItem == 0)
		return;

	QRect rect(m_pListView->itemRect(pItem));
	if (!rect.isValid())
		return;

	qtractorFileGroupItem *pGroupItem =
		static_cast<qtractorFileGroupItem *>(pItem);
	if (pGroupItem)
		QToolTip::tip(rect, pGroupItem->toolTip());
}


//----------------------------------------------------------------------
// class qtractorFileGroupItem -- custom group list view item.
//

// Constructors.
qtractorFileGroupItem::qtractorFileGroupItem (
	qtractorFileListView *pListView, const QString& sName )
	: QListViewItem(pListView, sName)
{
	QListViewItem::setDragEnabled(true);
	QListViewItem::setDropEnabled(true);

	QListViewItem::setRenameEnabled(0, true);

	QListViewItem::setPixmap(0, QPixmap::fromMimeSource("itemGroup.png"));
}


qtractorFileGroupItem::qtractorFileGroupItem (
	qtractorFileGroupItem *pGroupItem, const QString& sName )
	: QListViewItem(pGroupItem, sName)
{
	QListViewItem::setDragEnabled(true);
	QListViewItem::setDropEnabled(true);

	QListViewItem::setRenameEnabled(0, true);

	QListViewItem::setPixmap(0, QPixmap::fromMimeSource("itemGroup.png"));
}


// Default destructor.
qtractorFileGroupItem::~qtractorFileGroupItem (void)
{
}


// Instance accessors.
void qtractorFileGroupItem::setName ( const QString& sName )
{
	QListViewItem::setText(0, sName);
}


QString qtractorFileGroupItem::name (void) const
{
	return QListViewItem::text(0);
}


qtractorFileGroupItem *qtractorFileGroupItem::groupItem (void) const
{
	QListViewItem *pParent = QListViewItem::parent();
	while (pParent && pParent->rtti() != qtractorFileListView::GroupItem)
		pParent = pParent->parent();
	return static_cast<qtractorFileGroupItem *>(pParent);
}


qtractorFileListView *qtractorFileGroupItem::listView (void) const
{
	return static_cast<qtractorFileListView *>(QListViewItem::listView());
}


// To show up whether its open or not.
void qtractorFileGroupItem::setOpen ( bool bOpen )
{
	// Set the proper pixmap of this...
	if (rtti() == qtractorFileListView::GroupItem) {
		QListViewItem::setPixmap(0, QPixmap::fromMimeSource(
				bOpen ? "itemGroupOpen.png" : "itemGroup.png"));
	}
	// Open it up...
	QListViewItem::setOpen(bOpen);

	// All ancestors should be also visible.
	if (bOpen && QListViewItem::parent())
		QListViewItem::parent()->setOpen(true);
}


// To virtually distinguish between list view items.
int qtractorFileGroupItem::rtti (void) const
{
	return qtractorFileListView::GroupItem;
}


// Tooltip renderer.
QString qtractorFileGroupItem::toolTip (void) const
{
	return name();
}


//----------------------------------------------------------------------
// class qtractorFileListItem -- custom file list view item.
//

// Constructors.
qtractorFileListItem::qtractorFileListItem (
	qtractorFileListView *pListView, const QString& sPath )
	: qtractorFileGroupItem(pListView, QFileInfo(sPath).fileName())
{
	m_sPath = sPath;
}

qtractorFileListItem::qtractorFileListItem (
	qtractorFileGroupItem *pGroupItem, const QString& sPath )
	: qtractorFileGroupItem(pGroupItem, QFileInfo(sPath).fileName())
{
	m_sPath = sPath;
}


// Default destructor.
qtractorFileListItem::~qtractorFileListItem (void)
{
}


// To virtually distinguish between list view items.
int qtractorFileListItem::rtti (void) const
{
	return qtractorFileListView::FileItem;
}


// Full path accessor.
const QString& qtractorFileListItem::path (void) const
{
	return m_sPath;
}


//----------------------------------------------------------------------
// class qtractorFileChannelItem -- custom channel list view item.
//

// Constructors.
qtractorFileChannelItem::qtractorFileChannelItem (
	qtractorFileListItem *pFileItem, const QString& sName,
	unsigned short iChannel )
	: qtractorFileGroupItem(pFileItem, sName)
{
	m_iChannel = iChannel;

	QListViewItem::setRenameEnabled(0, false);
}


// Default destructor.
qtractorFileChannelItem::~qtractorFileChannelItem (void)
{
}


// To virtually distinguish between list view items.
int qtractorFileChannelItem::rtti (void) const
{
	return qtractorFileListView::ChannelItem;
}


// Filoe chhannel accessor.
unsigned short qtractorFileChannelItem::channel (void) const
{
	return m_iChannel;
}


//----------------------------------------------------------------------
// class qtractorFileChannelDrag -- custom file channel drag object.
//
static const char *c_pszFileChannelMimeType = "qtractor/file-channel";

// Constructor.
qtractorFileChannelDrag::qtractorFileChannelDrag ( const QString& sPath,
	unsigned short iChannel, QWidget *pDragSource, const char *pszName )
	: QStoredDrag(c_pszFileChannelMimeType, pDragSource, pszName)
{
	QByteArray data(sizeof(iChannel) + sPath.length() + 1);
	char *pData = data.data();
	::memcpy(pData, &iChannel, sizeof(iChannel));
	::memcpy(pData + sizeof(iChannel), sPath.latin1(), sPath.length() + 1);
	QStoredDrag::setEncodedData(data);
}


// Decode trial method.
bool qtractorFileChannelDrag::canDecode ( const QMimeSource *pMimeSource )
{
	return pMimeSource->provides(c_pszFileChannelMimeType);
}


// Decode method.
bool qtractorFileChannelDrag::decode ( const QMimeSource *pMimeSource,
	QString& sPath, unsigned short *piChannel )
{
	QByteArray data = pMimeSource->encodedData(c_pszFileChannelMimeType);
	if (data.size() < sizeof(unsigned short) + 1)
		return false;

	char *pData = data.data();
	sPath = (const char *) (pData + sizeof(unsigned short));
	if (piChannel)
		::memcpy(piChannel, pData, sizeof(unsigned short));

	return true;
}


//----------------------------------------------------------------------------
// qtractorFileListView -- Group/File list view, supporting drag-n-drop.
//

// Constructor.
qtractorFileListView::qtractorFileListView (
	QWidget *pParent, const char *pszName )
	: QListView(pParent, pszName)
{
	m_pAutoOpenTimer   = NULL;
	m_iAutoOpenTimeout = 0;

	m_pDragItem = NULL;
	m_pDropItem = NULL;

	m_pToolTip = new qtractorFileListViewToolTip(this);

	QListView::setRootIsDecorated(true);
	QListView::setResizeMode(QListView::NoColumn);
	QListView::setAcceptDrops(true);
	QListView::setDragAutoScroll(true);
	QListView::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	QListView::setShowToolTips(false);
	QListView::setSortColumn(-1);

	setAutoOpenTimeout(800);

	m_pNewGroupAction   = new QAction(tr("New &Group"), tr("Ctrl+G"), this);
	m_pOpenFileAction   = new QAction(tr("&Open..."), tr("Ctrl+O"), this);
	m_pRenameItemAction = new QAction(tr("Re&name"), tr("Ctrl+N"), this);
	m_pDeleteItemAction = new QAction(tr("&Delete"), tr("Ctrl+D"), this);

	QObject::connect(m_pNewGroupAction, SIGNAL(activated()),
		this, SLOT(newGroupSlot()));
	QObject::connect(m_pOpenFileAction, SIGNAL(activated()),
		this, SLOT(openFileSlot()));
	QObject::connect(m_pRenameItemAction, SIGNAL(activated()),
		this, SLOT(renameItemSlot()));
	QObject::connect(m_pDeleteItemAction, SIGNAL(activated()),
		this, SLOT(deleteItemSlot()));

	QObject::connect(this, SIGNAL(selectionChanged()),
		this, SLOT(selectionChangedSlot()));
	QObject::connect(this, SIGNAL(doubleClicked(QListViewItem*,const QPoint&,int)),
		this, SLOT(activatedSlot(QListViewItem*)));
	QObject::connect(this, SIGNAL(returnPressed(QListViewItem*)),
		this, SLOT(activatedSlot(QListViewItem*)));
	QObject::connect(this, SIGNAL(itemRenamed(QListViewItem*,int)),
		this, SLOT(renamedSlot(QListViewItem*)));

	selectionChangedSlot();
}


// Default destructor.
qtractorFileListView::~qtractorFileListView (void)
{
	setAutoOpenTimeout(0);

	delete m_pToolTip;

	delete m_pNewGroupAction;
	delete m_pOpenFileAction;
	delete m_pRenameItemAction;
	delete m_pDeleteItemAction;
}


// Add a new file item, optionally under a given group.
qtractorFileListItem *qtractorFileListView::addFileItem (
	const QString& sPath, qtractorFileGroupItem *pParentItem )
{
	qtractorFileListItem *pFileItem = findFileItem(sPath);
	if (pFileItem == NULL) {
		pFileItem = createFileItem(sPath, pParentItem);
		if (pFileItem)
			emit contentsChanged();
	}

	if (pFileItem)
		QListView::setSelected(pFileItem, true);

	return pFileItem;
}


// Add a new group item, optionally under another group.
qtractorFileGroupItem *qtractorFileListView::addGroupItem (
	const QString& sName, qtractorFileGroupItem *pParentItem )
{
	qtractorFileGroupItem *pGroupItem = findGroupItem(sName);
	if (pGroupItem == NULL) {
		if (pParentItem)
			pGroupItem = new qtractorFileGroupItem(pParentItem, sName);
		else
			pGroupItem = new qtractorFileGroupItem(this, sName);
		emit contentsChanged();
	}
	QListView::setSelected(pGroupItem, true);
	return pGroupItem;
}


// Make as current selection an existing file item.
qtractorFileListItem *qtractorFileListView::selectFileItem (
	const QString& sPath )
{
	qtractorFileListItem *pFileItem = findFileItem(sPath);
	if (pFileItem) {
		qtractorFileGroupItem *pGroupItem = pFileItem->groupItem();
		if (pGroupItem)
			pGroupItem->setOpen(true);
		QListView::setSelected(pFileItem, true);
	}

	return pFileItem;
}


// Find a group item, given its name.
qtractorFileGroupItem *qtractorFileListView::findGroupItem (
	const QString& sName ) const
{
	return static_cast<qtractorFileGroupItem *>(findItem(GroupItem, sName));
}


// Find a file item, given its name.
qtractorFileListItem *qtractorFileListView::findFileItem (
	const QString& sPath ) const
{
	return static_cast<qtractorFileListItem *>(findItem(FileItem, sPath));
}


// Find a list view item, given its type and name.
QListViewItem *qtractorFileListView::findItem (
	ItemType type, const QString& sText ) const
{
	// Iterate all over the place to search for the item.
	QListViewItemIterator iter((QListView *) this);
	while (iter.current()) {
		QListViewItem *pItem = iter.current();
		if (pItem->rtti() == type) {
			switch (pItem->rtti()) {
				case GroupItem: {
					if (pItem->text(0) == sText)
						return pItem;
					break;
				}
				case qtractorFileListView::FileItem: {
					qtractorFileListItem *pFileItem =
						static_cast<qtractorFileListItem *>(pItem);
					if (pFileItem && pFileItem->path() == sText)
						return pItem;
					break;
				}
			}
		}
		++iter;
	}
	// Not found.
	return NULL;
}


// Find and return the nearest group item...
qtractorFileGroupItem *qtractorFileListView::groupItem ( QListViewItem *pItem ) const
{
	while (pItem && pItem->rtti() != GroupItem)
		pItem = pItem->parent();
	return static_cast<qtractorFileGroupItem *> (pItem);
}


// Prompt for proper file list open.
QStringList qtractorFileListView::openFileNames (void)
{
	// Ask for the filename to open...
	QStringList files = getOpenFileNames();

	// Remember recent directory...
	if (!files.isEmpty())
	    setRecentDir(QFileInfo(files[0]).dirPath(true));

	return files;
}


// Open and add a new file item below the current group one.
void qtractorFileListView::openFileSlot (void)
{
	// Ask for the filename to open...
	QStringList files = openFileNames();

	// Check if its a valid file...
	if (files.isEmpty())
		return;

	// Find a proper group parent  group item...
	qtractorFileGroupItem *pParentItem
		= groupItem(QListView::selectedItem());
	// Pick each one of the selected files...
	for (QStringList::ConstIterator iter = files.begin();
			iter != files.end(); ++iter) {
		const QString& sPath = *iter;
		// Add the new file item...
		qtractorFileListItem *pFileItem
			= addFileItem(sPath, pParentItem);
		if (pFileItem) {
			// Make all this new open and visible.
			if (pParentItem)
				pParentItem->setOpen(true);
		}
	}
}


// Add a new group item below the current one.
void qtractorFileListView::newGroupSlot (void)
{
	qtractorFileGroupItem *pParentItem
		= groupItem(QListView::selectedItem());
	qtractorFileGroupItem *pGroupItem
		= addGroupItem(tr("New Group"), pParentItem);
	if (pParentItem)
		pParentItem->setOpen(true);
	if (pGroupItem)
		pGroupItem->startRename(0);
}


// Rename current group/file item.
void qtractorFileListView::renameItemSlot (void)
{
	QListViewItem *pItem = QListView::selectedItem();
	if (pItem)
		pItem->startRename(0);
}


// Remove current group/file item.
void qtractorFileListView::deleteItemSlot (void)
{
	QListViewItem *pItem = QListView::selectedItem();
	if (pItem) {
		delete pItem;
		emit contentsChanged();
	}
}


// In-place selection slot.
void qtractorFileListView::selectionChangedSlot (void)
{
	QListViewItem *pItem = QListView::selectedItem();
	bool bEnabled = (pItem && pItem->rtti() != ChannelItem);
	m_pRenameItemAction->setEnabled(bEnabled);
	m_pDeleteItemAction->setEnabled(bEnabled);
}


// In-place activation slot.
void qtractorFileListView::activatedSlot ( QListViewItem *pItem )
{
	if (pItem && pItem->rtti() == FileItem) {
		qtractorFileListItem *pFileItem =
			static_cast<qtractorFileListItem *> (pItem);
		if (pFileItem)
			emit activated(pFileItem->path());
	}
}


// In-place aliasing slot.
void qtractorFileListView::renamedSlot ( QListViewItem *pItem )
{
	if (pItem)
		emit contentsChanged();
}


// Auto-open timeout method.
void qtractorFileListView::setAutoOpenTimeout ( int iAutoOpenTimeout )
{
	m_iAutoOpenTimeout = iAutoOpenTimeout;

	if (m_pAutoOpenTimer)
		delete m_pAutoOpenTimer;
	m_pAutoOpenTimer = 0;

	if (m_iAutoOpenTimeout > 0) {
		m_pAutoOpenTimer = new QTimer(this);
		QObject::connect(m_pAutoOpenTimer, SIGNAL(timeout()),
			this, SLOT(timeoutSlot()));
	}
}


// Auto-open timeout accessor.
int qtractorFileListView::autoOpenTimeout (void) const
{
	return m_iAutoOpenTimeout;
}


// Auto-open timer slot.
void qtractorFileListView::timeoutSlot (void)
{
	if (m_pAutoOpenTimer) {
		m_pAutoOpenTimer->stop();
		if (m_pDropItem && !m_pDropItem->isOpen()) {
			m_pDropItem->setOpen(true);
			m_pDropItem->repaint();
		}
	}
}


// Recently used directory, if any.
void qtractorFileListView::setRecentDir ( const QString& sRecentDir )
{
    m_sRecentDir = sRecentDir;
}

const QString& qtractorFileListView::recentDir (void) const
{
	return m_sRecentDir;
}


QDragObject *qtractorFileListView::dragObject (void)
{
	QDragObject *pDragObject = NULL;
	QListViewItem *pItem = QListView::selectedItem();
	if (pItem && pItem->dragEnabled()) {
		// Check what are we dragging around?...
		switch (pItem->rtti()) {
			case GroupItem: {
				pDragObject = new QTextDrag(pItem->text(0), this);
				break;
			}
			case FileItem: {
				qtractorFileListItem *pFileItem =
					static_cast<qtractorFileListItem *>(pItem);
				if (pFileItem) {
					QUriDrag *pUriDrag = new QUriDrag(this);
					pUriDrag->setFileNames(pFileItem->path());
					pDragObject = pUriDrag;
				}
				break;
			}
			case ChannelItem: {
				qtractorFileListItem *pFileItem =
					static_cast<qtractorFileListItem *>(pItem->parent());
				qtractorFileChannelItem *pChannelItem =
					static_cast<qtractorFileChannelItem *>(pItem);
				if (pFileItem && pChannelItem) {
					pDragObject = new qtractorFileChannelDrag(
						pFileItem->path(), pChannelItem->channel(), this);
				}
				break;
			}
		}
		// Are we really dragging something?...
		if (pDragObject) {
			const QPixmap *pPixmap = pItem->pixmap(0);
			if (pPixmap)
				pDragObject->setPixmap(*pPixmap, QPoint(-4, -12));
			// This is it:
			m_pDragItem = pItem;
		}
	}

	return pDragObject;
}


// Drag-n-drop stuff.
bool qtractorFileListView::canDecodeEvent ( QDropEvent *pDropEvent )
{
	if (m_pDragItem && pDropEvent->source() != this)
		m_pDragItem = NULL;

	return (QUriDrag::canDecode(pDropEvent)
		|| QTextDrag::canDecode(pDropEvent));
}

bool qtractorFileListView::canDropItem ( QListViewItem *pItem ) const
{
//	if (pItem == NULL)
//		return false;

	if (m_pDragItem) {
		if (pItem == m_pDragItem->parent())
			return false;
		while (pItem) {
			if (pItem == m_pDragItem)
				return false;
			pItem = pItem->parent();
		}
	}

	return true;
}

QListViewItem *qtractorFileListView::dragDropItem ( const QPoint& pos )
{
	QPoint vpos(pos);
	int m = QListView::header()->sectionRect(0).height();
	vpos.setY(vpos.y() - m);

	qtractorFileGroupItem *pItem
		= groupItem(QListView::itemAt(vpos));

	if (pItem) {
		if (pItem != m_pDropItem) {
			QListView::setSelected(pItem, true);
			m_pDropItem = pItem;
			if (m_pAutoOpenTimer)
				m_pAutoOpenTimer->start(m_iAutoOpenTimeout);
		}
	} else {
		pItem = NULL;
		if (m_pDropItem)
			QListView::setSelected(m_pDropItem, false);
		m_pDropItem = NULL;
		if (m_pAutoOpenTimer)
			m_pAutoOpenTimer->stop();
	}

	vpos = QListView::viewportToContents(vpos);
	QListView::ensureVisible(vpos.x(), vpos.y(), m, m);

	return pItem;
}


void qtractorFileListView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
	if (!canDecodeEvent(pDragEnterEvent)) {
		pDragEnterEvent->ignore();
		return;
	}

	QListViewItem *pDropItem = dragDropItem(pDragEnterEvent->pos());
	if (canDropItem(pDropItem)) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


void qtractorFileListView::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	if (!canDecodeEvent(pDragMoveEvent)) {
		pDragMoveEvent->ignore();
		return;
	}

	QListViewItem *pDropItem = dragDropItem(pDragMoveEvent->pos());
	if (canDropItem(pDropItem)) {
		if (pDropItem) {
			pDragMoveEvent->accept(QListView::itemRect(pDropItem));
		} else {
			pDragMoveEvent->accept();
		}
	} else {
		if (pDropItem) {
			pDragMoveEvent->ignore(QListView::itemRect(pDropItem));
		} else {
			pDragMoveEvent->ignore();
		}
	}
}


void qtractorFileListView::dragLeaveEvent ( QDragLeaveEvent * )
{
	m_pDropItem = NULL;
	if (m_pAutoOpenTimer)
		m_pAutoOpenTimer->stop();
}


void qtractorFileListView::dropEvent ( QDropEvent *pDropEvent )
{
	QListViewItem *pDropItem = dragDropItem(pDropEvent->pos());

	// It's one from ourselves?...
	if (m_pDragItem) {
		// Take the item from list...
		QListViewItem *pParentItem = m_pDragItem->parent();
		if (pParentItem) {
			pParentItem->takeItem(m_pDragItem);
		} else {
			QListView::takeItem(m_pDragItem);
		}
		// Insert it back...
		if (pDropItem) {
			pDropItem->insertItem(m_pDragItem);
			if (pDropItem)
				pDropItem->setOpen(true);
		} else {
			QListView::insertItem(m_pDragItem);
		}
		// Notify that we've changed something.
		emit contentsChanged();
	} else {
		// Let's see how many files there are...
		QStringList files;
		if (QUriDrag::decodeLocalFiles(pDropEvent, files)) {
			for (QStringList::ConstIterator iter = files.begin();
					iter != files.end(); ++iter) {
				addFileItem(*iter,
					static_cast<qtractorFileGroupItem *> (pDropItem));
			}
		} else {
			// Maybe its just a new convenience group...
			QString sText;
			if (QTextDrag::decode(pDropEvent, sText)) {
				addGroupItem(sText,
					static_cast<qtractorFileGroupItem *> (pDropItem));
			}
		}
	}

	dragLeaveEvent(NULL);
	m_pDragItem = NULL;
}


// Context menu request event handler.
void qtractorFileListView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	QPopupMenu* pContextMenu = new QPopupMenu(this);

	// Construct context menu.
	m_pNewGroupAction->addTo(pContextMenu);
	pContextMenu->insertSeparator();
	m_pOpenFileAction->addTo(pContextMenu);
	m_pRenameItemAction->addTo(pContextMenu);
	m_pDeleteItemAction->addTo(pContextMenu);

	pContextMenu->exec(pContextMenuEvent->globalPos());

	delete pContextMenu;
}


// Custom file list loaders.
bool qtractorFileListView::loadListElement ( qtractorDocument *pDocument,
	QDomElement *pElement, QListViewItem *pItem )
{
	if (pItem && pItem->rtti() != GroupItem)
		return false;
	qtractorFileGroupItem *pParentItem
		= static_cast<qtractorFileGroupItem *> (pItem);

	// Load children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull(); nChild = nChild.nextSibling()) {
		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;
		// Now it depends on item tag/type...
		if (eChild.tagName() == "group") {
			qtractorFileGroupItem *pGroupItem
				= addGroupItem(eChild.attribute("name"), pParentItem);
			if (pGroupItem) {
				pGroupItem->setOpen(
					pDocument->boolFromText(eChild.attribute("open")));
				if (!loadListElement(pDocument, &eChild, pGroupItem))
					return false;
			}
		}
		else
		if (eChild.tagName() == "file") {
			qtractorFileListItem *pFileItem
				= addFileItem(eChild.text(), pParentItem);
			if (pFileItem) {
				pFileItem->setText(0, eChild.attribute("name"));
			}
		}
	}

	return true;
}


// The elemental loader implementation.
bool qtractorFileListView::loadElement ( qtractorDocument *pDocument,
	QDomElement *pElement )
{
	clear();

	return loadListElement(pDocument, pElement, NULL);
}


// Custom file list saver.
bool qtractorFileListView::saveListElement ( qtractorDocument *pDocument,
	QDomElement *pElement, QListViewItem *pItem )
{
	// Save group children...
	while (pItem) {
		// Create the new child element...
		switch (pItem->rtti()) {
			case GroupItem: {
				qtractorFileGroupItem *pGroupItem
					= static_cast<qtractorFileGroupItem *> (pItem);
				QDomElement eGroup = pDocument->document()->createElement("group");
				eGroup.setAttribute("name",	pGroupItem->text(0));
				eGroup.setAttribute("open",
					pDocument->textFromBool(pGroupItem->isOpen()));
				if (!saveListElement(pDocument, &eGroup, pGroupItem->firstChild()))
					return false;
				pElement->appendChild(eGroup);
				break;
			}
			case FileItem: {
				qtractorFileListItem *pFileItem
					= static_cast<qtractorFileListItem *> (pItem);
				QDomElement eFile = pDocument->document()->createElement("file");
				eFile.setAttribute("name", pFileItem->text(0));
				eFile.appendChild(pDocument->document()->createTextNode(
					pFileItem->path()));
				pElement->appendChild(eFile);
				break;
			}
		}
		// Continue...
		pItem = pItem->nextSibling();
	}

	return true;
}


// The elemental saver implementation.
bool qtractorFileListView::saveElement ( qtractorDocument *pDocument,
	QDomElement *pElement )
{
	return saveListElement(pDocument, pElement, firstChild());
}


// end of qtractorFileListView.cpp

