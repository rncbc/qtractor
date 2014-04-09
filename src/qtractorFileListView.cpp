// qtractorFileListView.cpp
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

#include "qtractorAbout.h"
#include "qtractorFileListView.h"

#include "qtractorRubberBand.h"
#include "qtractorDocument.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorClip.h"

#include <QDomDocument>

#include <QMessageBox>
#include <QApplication>
#include <QHeaderView>
#include <QClipboard>
#include <QFileInfo>
#include <QToolTip>
#include <QTimer>
#include <QUrl>
#include <QDir>

#include <QMouseEvent>
#include <QDragLeaveEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#if QT_VERSION >= 0x050000
#include <QMimeData>
#include <QDrag>
#endif


//----------------------------------------------------------------------
// class qtractorFileGroupItem -- custom group list view item.
//

// Constructors.
qtractorFileGroupItem::qtractorFileGroupItem (
	const QString& sName, int iType )
	: QTreeWidgetItem(iType)
{
	QTreeWidgetItem::setIcon(0, QIcon(":/images/itemGroup.png"));
	QTreeWidgetItem::setText(0, sName);

	Qt::ItemFlags flags = QTreeWidgetItem::flags();
	if (iType == qtractorFileListView::GroupItem) {
		flags |=  Qt::ItemIsEditable;
		flags &= ~Qt::ItemIsSelectable;
	}
	QTreeWidgetItem::setFlags(flags);
}


// Default destructor.
qtractorFileGroupItem::~qtractorFileGroupItem (void)
{
}


// Instance accessors.
void qtractorFileGroupItem::setName ( const QString& sName )
{
	QTreeWidgetItem::setText(0, sName);
}


QString qtractorFileGroupItem::name (void) const
{
	return QTreeWidgetItem::text(0);
}


qtractorFileGroupItem *qtractorFileGroupItem::groupItem (void) const
{
	QTreeWidgetItem *pParent = QTreeWidgetItem::parent();
	while (pParent && pParent->type() != qtractorFileListView::GroupItem)
		pParent = pParent->parent();
	return static_cast<qtractorFileGroupItem *> (pParent);
}


qtractorFileListView *qtractorFileGroupItem::listView (void) const
{
	return static_cast<qtractorFileListView *> (QTreeWidgetItem::treeWidget());
}


// To show up whether its open or not.
void qtractorFileGroupItem::setOpen ( bool bOpen )
{
	// Set the proper pixmap of this...
	if (type() == qtractorFileListView::GroupItem) {
		QTreeWidgetItem::setIcon(0, QIcon(
			bOpen ? ":/images/itemGroupOpen.png" : ":/images/itemGroup.png"));
	}

	// Open it up...
	QTreeWidgetItem::setExpanded(bOpen);

	// All ancestors should be also visible.
	if (bOpen) {
		qtractorFileGroupItem *pGroupItem = groupItem();
		if (pGroupItem)
			pGroupItem->setOpen(true);
	}
}


bool qtractorFileGroupItem::isOpen (void) const
{
	return QTreeWidgetItem::isExpanded();
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
qtractorFileListItem::qtractorFileListItem ( const QString& sPath )
	: qtractorFileGroupItem(QFileInfo(sPath).fileName(),
		qtractorFileListView::FileItem)
{
	m_sPath = sPath;
}


// Default destructor.
qtractorFileListItem::~qtractorFileListItem (void)
{
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
	: qtractorFileGroupItem(sName, qtractorFileListView::ChannelItem)
{
	m_iChannel = iChannel;

	pFileItem->addChild(this);
}


// Default destructor.
qtractorFileChannelItem::~qtractorFileChannelItem (void)
{
}


// File channel accessor.
unsigned short qtractorFileChannelItem::channel (void) const
{
	return m_iChannel;
}


// Full path accessor.
const QString qtractorFileChannelItem::path (void) const
{
	QString sPath;

	qtractorFileListItem *pFileItem
		= static_cast<qtractorFileListItem *> (QTreeWidgetItem::parent());
	if (pFileItem)
		sPath = pFileItem->path();

	return sPath;
}


//----------------------------------------------------------------------
// class qtractorFileChannelDrag -- custom file channel drag object.
//

static const char *c_pszFileChannelMimeType = "qtractor/file-channel";

// Encoder method.
void qtractorFileChannelDrag::encode ( QMimeData *pMimeData,
	const qtractorFileChannelDrag::List& items )
{
	// First, compute total serialized data size...
	unsigned int iSize = sizeof(unsigned short);
	QListIterator<Item> iter(items);
	while (iter.hasNext()) {
		const Item& item = iter.next();
		iSize += sizeof(unsigned short);
		iSize += item.path.length() + 1;
	}

	// Allocate the data array...
	QByteArray data(iSize, (char) 0);
	char *pData = data.data();

	// Header says how much items there are...
	unsigned short iCount = items.count();
	::memcpy(pData, &iCount, sizeof(unsigned short));
	pData += sizeof(unsigned short);

	// No for actual serialization...
	iter.toFront();
	while (iter.hasNext()) {
		const Item& item = iter.next();
		::memcpy(pData, &item.channel, sizeof(unsigned short));
		pData += sizeof(unsigned short);
		unsigned int cchPath = item.path.length() + 1;
		::memcpy(pData, item.path.toUtf8().constData(), cchPath);
		pData += cchPath;
	}

	// Ok, done.
	pMimeData->setData(c_pszFileChannelMimeType, data);
}

// Decode trial method.
bool qtractorFileChannelDrag::canDecode ( const QMimeData *pMimeData )
{
	return pMimeData->hasFormat(c_pszFileChannelMimeType);
}

// Decode method.
qtractorFileChannelDrag::List qtractorFileChannelDrag::decode (
	const QMimeData *pMimeData )
{
	List items;

	QByteArray data = pMimeData->data(c_pszFileChannelMimeType);
	if (data.size() < (int) sizeof(unsigned short) + 1)
		return items;

	const char *pData = data.constData();

	unsigned short iCount = 0;
	::memcpy(&iCount, pData, sizeof(unsigned short));
	pData += sizeof(unsigned short);

	Item item;
	for (unsigned short i = 0; i < iCount; ++i) {
		::memcpy(&item.channel, pData, sizeof(unsigned short));
		pData += sizeof(unsigned short);
		item.path = pData;
		pData += item.path.length() + 1;
		items.append(item);
	}

	return items;
}


//----------------------------------------------------------------------------
// qtractorFileListView -- Group/File list view, supporting drag-n-drop.
//

// Constructor.
qtractorFileListView::qtractorFileListView (
	qtractorFileList::Type iFileType, QWidget *pParent )
	: QTreeWidget(pParent), m_iFileType(iFileType)
{
	m_pAutoOpenTimer   = NULL;
	m_iAutoOpenTimeout = 0;

	m_pDragItem = NULL;
	m_pDropItem = NULL;

	m_pRubberBand = NULL;

	QTreeWidget::setRootIsDecorated(false);
	QTreeWidget::setUniformRowHeights(true);
	QTreeWidget::setAlternatingRowColors(true);
//	QTreeWidget::setDragEnabled(true);
	QTreeWidget::setAcceptDrops(true);
	QTreeWidget::setDropIndicatorShown(true);
	QTreeWidget::setAutoScroll(true);
	QTreeWidget::setSelectionMode(QAbstractItemView::ExtendedSelection);
	QTreeWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	QTreeWidget::setSortingEnabled(false);

	QHeaderView *pHeader = QTreeWidget::header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
//	pHeader->setDefaultSectionSize(160);
#if QT_VERSION >= 0x050000
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setMovable(false);
#endif
	pHeader->setStretchLastSection(true);

	// Trap for help/tool-tips events.
	QTreeWidget::viewport()->installEventFilter(this);

	setAutoOpenTimeout(800);

	QObject::connect(this,
		SIGNAL(itemClicked(QTreeWidgetItem*,int)),
		SLOT(itemClickedSlot(QTreeWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
		SLOT(itemActivatedSlot(QTreeWidgetItem*)));
//	QObject::connect(this,
//		SIGNAL(itemActivated(QTreeWidgetItem*,int)),
//		SLOT(itemActivatedSlot(QTreeWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemExpanded(QTreeWidgetItem*)),
		SLOT(itemExpandedSlot(QTreeWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemCollapsed(QTreeWidgetItem*)),
		SLOT(itemCollapsedSlot(QTreeWidgetItem*)));
	QObject::connect(QTreeWidget::itemDelegate(),
		SIGNAL(commitData(QWidget*)),
		SLOT(itemRenamedSlot()));
}


// Default destructor.
qtractorFileListView::~qtractorFileListView (void)
{
	clear();

	setAutoOpenTimeout(0);
}


// File list type property.
void qtractorFileListView::setFileType ( qtractorFileList::Type iFileType )
{
	m_iFileType = iFileType;
}

qtractorFileList::Type qtractorFileListView::fileType (void) const
{
	return m_iFileType;
}


// Add a new file item, optionally under a given group.
qtractorFileListItem *qtractorFileListView::addFileItem (
	const QString& sPath, qtractorFileGroupItem *pParentItem )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	qtractorFileListItem *pFileItem = findFileItem(sPath);
	if (pFileItem == NULL) {
		pFileItem = createFileItem(sPath);
		if (pFileItem) {
			// Add to file/path registry...
			pSession->files()->addFileItem(m_iFileType, pFileItem);
			// Insert the new file item in place...
			if (pParentItem) {
				if (pParentItem->type() == GroupItem) {
					pParentItem->insertChild(0, pFileItem);
				} else {
					// It must be a group item...
					QTreeWidgetItem *pItem
						= static_cast<QTreeWidgetItem *> (pParentItem);
					pParentItem = groupItem(pParentItem);
					if (pParentItem) {
						int iItem = pParentItem->indexOfChild(pItem);
						if (iItem >= 0)
							pParentItem->insertChild(iItem + 1, pFileItem);
						else
							pItem->addChild(pFileItem);
					} else {
						int iItem = QTreeWidget::indexOfTopLevelItem(pItem);
						if (iItem >= 0)
							QTreeWidget::insertTopLevelItem(iItem + 1, pFileItem);
						else
							QTreeWidget::addTopLevelItem(pFileItem);
					}
				}
			}
			else QTreeWidget::addTopLevelItem(pFileItem);
		}
	}
#if 0
	if (pFileItem)
		QTreeWidget::setCurrentItem(pFileItem);
#endif
	return pFileItem;
}


// Add a new group item, optionally under another group.
qtractorFileGroupItem *qtractorFileListView::addGroupItem (
	const QString& sName, qtractorFileGroupItem *pParentItem )
{
	qtractorFileGroupItem *pGroupItem = findGroupItem(sName);
	if (pGroupItem == NULL) {
		pGroupItem = new qtractorFileGroupItem(sName);
		if (pParentItem)
			pParentItem->addChild(pGroupItem);
		else
			addTopLevelItem(pGroupItem);
		emit contentsChanged();
	}
#if 0
	QTreeWidget::setCurrentItem(pGroupItem);
#endif
	return pGroupItem;
}


// Current group item accessor...
qtractorFileGroupItem *qtractorFileListView::currentGroupItem (void) const
{
	return groupItem(QTreeWidget::currentItem());
}


// Current file item accessor...
qtractorFileListItem *qtractorFileListView::currentFileItem (void) const
{
	QTreeWidgetItem *pItem = QTreeWidget::currentItem();
	if (pItem && pItem->type() == FileItem)
		return static_cast<qtractorFileListItem *> (pItem);
	
	return NULL;
}


// Make as current selection an existing file item.
qtractorFileListItem *qtractorFileListView::selectFileItem (
	const QString& sPath, int iChannel )
{
	qtractorFileListItem *pFileItem = findFileItem(sPath);
	if (pFileItem == NULL)
		return NULL;

	// Shall we go deep further intto a channel item?
	if (iChannel > 0) {
		// Open file item...
		pFileItem->setOpen(true);
		// Select channel item...
		int iChildCount = pFileItem->childCount();
		for (int i = 0; i < iChildCount; ++i) {
			QTreeWidgetItem *pItem = pFileItem->child(i);
			if (pItem->type() == qtractorFileListView::ChannelItem) {
				qtractorFileChannelItem *pChannelItem
					= static_cast<qtractorFileChannelItem *> (pItem);
				if (pChannelItem && pChannelItem->channel() == iChannel) {
					QTreeWidget::setCurrentItem(pChannelItem);
					return pFileItem;
				}
			}
		}
	}

	// Open file group, if any...
	qtractorFileGroupItem *pGroupItem = pFileItem->groupItem();
	if (pGroupItem)
		pGroupItem->setOpen(true);

	// Nothing else than select file item...
	QTreeWidget::setCurrentItem(pFileItem);

	return pFileItem;
}


// Open and add a new file item below the current group one.
void qtractorFileListView::openFile (void)
{
	// Ask for the filename to open...
	QStringList files = openFileNames();

	// Check if its a valid file...
	if (files.isEmpty())
		return;

	// Find a proper group parent group item...
	qtractorFileListItem *pFileItem = NULL;
	qtractorFileGroupItem *pParentItem = currentGroupItem();
	// Pick each one of the selected files...
	int iUpdate = 0;
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		const QString& sPath = iter.next();
		// Add the new file item...
		pFileItem = addFileItem(sPath, pParentItem);
		// Make all this new open and visible.
		if (pFileItem) {
			++iUpdate;
			if (pParentItem)
				pParentItem->setOpen(true);
		}
	}

	// Make the last one current...
	if (pFileItem)
		QTreeWidget::setCurrentItem(pFileItem);
		
	// Make proper notifications...
	if (iUpdate > 0)
		emit contentsChanged();
}


// Add a new group item below the current one.
void qtractorFileListView::newGroup (void)
{
	qtractorFileGroupItem *pParentItem = currentGroupItem();
	if (pParentItem && !pParentItem->isOpen())
		pParentItem = pParentItem->groupItem();
	qtractorFileGroupItem *pGroupItem
		= addGroupItem(tr("New Group"), pParentItem);
	if (pGroupItem) {
		pParentItem = pGroupItem->groupItem();
		if (pParentItem)
			pParentItem->setOpen(true);
		editItem(pGroupItem, 0);
	}
}


// Copy/cut current file item(s) to clipboard.
void qtractorFileListView::copyItem ( bool bCut )
{
	// Build URL list...
	QList<QUrl> urls;

	int iUpdate = 0;
	QList<QTreeWidgetItem *> items = selectedItems();
	QListIterator<QTreeWidgetItem *> iter(items);
	while (iter.hasNext()) {
		QTreeWidgetItem *pItem = iter.next();
		if (pItem->type() == qtractorFileListView::FileItem) {
			qtractorFileListItem *pFileItem
				= static_cast<qtractorFileListItem *> (pItem);
			if (pFileItem) {
				urls.append(QUrl::fromLocalFile(pFileItem->path()));
				if (bCut) {
					delete pFileItem;
					++iUpdate;
				}
			}
		}
	}

	//	Copy it to system clipboard...
	if (!urls.isEmpty()) {
		QMimeData *pMimeData = new QMimeData();
		pMimeData->setUrls(urls);
		QApplication::clipboard()->setMimeData(pMimeData);
	}

	// Make proper notifications...
	if (iUpdate > 0)
		emit contentsChanged();
}


// Paste file item(s9 from clipboard.
void qtractorFileListView::pasteItem (void)
{
	const QMimeData *pMimeData = QApplication::clipboard()->mimeData();
	if (!pMimeData->hasUrls())
		return;

	// Find a proper group parent group item...
	qtractorFileGroupItem *pParentItem = currentGroupItem();
	int iUpdate = 0;

	QListIterator<QUrl> iter(pMimeData->urls());
	while (iter.hasNext()) {
		const QString& sPath = iter.next().toLocalFile();
		if (!sPath.isEmpty()) {
			qtractorFileListItem *pFileItem	= addFileItem(sPath, pParentItem);
			// Make all this new open and visible.
			if (pFileItem) {
				++iUpdate;
				if (pParentItem)
					pParentItem->setOpen(true);
			}
		}
	}

	// Make proper notifications...
	if (iUpdate > 0)
		emit contentsChanged();
}


// Rename current group/file item.
void qtractorFileListView::renameItem (void)
{
	qtractorFileGroupItem *pGroupItem = currentGroupItem();
	if (pGroupItem)
		editItem(pGroupItem, 0);
}


// Remove current group/file item.
void qtractorFileListView::removeItem (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	int iUpdate = 0;
	QList<QTreeWidgetItem *> items = selectedItems();
	if (items.count() > 0) {
		// Prompt user if he/she's sure about this...
		if (pOptions->bConfirmRemove) {
			if (QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("About to remove %1 file item(s).\n\n"
				"Are you sure?")
				.arg(items.count()),
				QMessageBox::Ok | QMessageBox::Cancel)
				== QMessageBox::Cancel)
				return;
		}
		// Definite multi-delete...
		QListIterator<QTreeWidgetItem *> iter(items);
		while (iter.hasNext()) {
			QTreeWidgetItem *pItem = iter.next();
			// Remove from file registry, when applicable...
			if (pItem->type() == FileItem) {
				qtractorFileListItem *pFileItem
					= static_cast<qtractorFileListItem *> (pItem);
				if (pFileItem)
					pSession->files()->removeFileItem(m_iFileType, pFileItem);
			}
			// Scrap view item...
			delete pItem;
			++iUpdate;
		}
	} else {
		QTreeWidgetItem *pItem = QTreeWidget::currentItem();
		if (pItem) {
			// Prompt user if he/she's sure about this...
			if (pOptions->bConfirmRemove) {
				if (QMessageBox::warning(this,
					tr("Warning") + " - " QTRACTOR_TITLE,
					tr("About to remove %1 item:\n\n"
					"\"%2\"\n\n"
					"Are you sure?")
					.arg(pItem->type() == GroupItem ? tr("group") : tr("file"))
					.arg(pItem->text(0)),
					QMessageBox::Ok | QMessageBox::Cancel)
					== QMessageBox::Cancel)
					return;
			}
			// Remove from file registry, when applicable...
			if (pItem->type() == FileItem) {
				if (pItem->type() == FileItem) {
					qtractorFileListItem *pFileItem
						= static_cast<qtractorFileListItem *> (pItem);
					if (pFileItem)
						pSession->files()->removeFileItem(m_iFileType, pFileItem);
				}
			}
			// Scrap view item...
			delete pItem;
			++iUpdate;
		}
	}

	if (iUpdate > 0)
		emit contentsChanged();
}


// Emit actiovation signal for given item...
void qtractorFileListView::activateItem ( QTreeWidgetItem *pItem )
{
	if (pItem == NULL)
		pItem = QTreeWidget::currentItem();
	if (pItem == NULL)
		return;

	if (pItem->type() == FileItem) {
		qtractorFileListItem *pFileItem
			= static_cast<qtractorFileListItem *> (pItem);
		if (pFileItem)
			emit activated(pFileItem->path(), -1);
	}
	else
	if (pItem->type() == ChannelItem) {
		qtractorFileChannelItem *pChannelItem
			= static_cast<qtractorFileChannelItem *> (pItem);
		if (pChannelItem)
			emit activated(pChannelItem->path(), pChannelItem->channel());
	}
}


// Clean-up unused file items.
void qtractorFileListView::cleanupItem ( QTreeWidgetItem *pItem )
{
	if (pItem == NULL)
		return;

	switch (pItem->type()) {
	case GroupItem: {
		int iChildCount = pItem->childCount();
		for (int i = 0; i < iChildCount; ++i)
			cleanupItem(pItem->child(i));
		break;
	}
	case FileItem: {
		qtractorSession *pSession = qtractorSession::getInstance();
		qtractorFileListItem *pFileItem
			= static_cast<qtractorFileListItem *> (pItem);
		if (pSession && pFileItem) {
			const QString& sPath = pFileItem->path();
			qtractorFileList::Item *pFileListItem
				= pSession->files()->findItem(m_iFileType, sPath);
			if (pFileListItem && pFileListItem->clipRefCount() < 1)
				pItem->setSelected(true);
		}
		break;
	}
	default:
		break;
	}
}


void qtractorFileListView::cleanup (void)
{
	QTreeWidget::setCurrentItem(NULL);
	QTreeWidget::selectionModel()->clearSelection();

	int iItemCount = QTreeWidget::topLevelItemCount();
	for (int i = 0; i < iItemCount; ++i)
		cleanupItem(QTreeWidget::topLevelItem(i));

	removeItem();
}


// Master clean-up.
void qtractorFileListView::clear (void)
{
	dragLeaveEvent(NULL);
	m_pDragItem = NULL;

	QTreeWidget::clear();
}


// Find a group item, given its name.
qtractorFileGroupItem *qtractorFileListView::findGroupItem (
	const QString& sName ) const
{
	return static_cast<qtractorFileGroupItem *> (findItem(sName, GroupItem));
}


// Find a file item, given its name.
qtractorFileListItem *qtractorFileListView::findFileItem (
	const QString& sPath ) const
{
	return static_cast<qtractorFileListItem *> (findItem(sPath, FileItem));
}


// Find a list view item, given its type and name.
QTreeWidgetItem *qtractorFileListView::findItem (
	const QString& sText, int iType ) const
{
	// Iterate all over the place to search for the item...
	QList<QTreeWidgetItem *> items = QTreeWidget::findItems(sText,
		Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive),
		(iType == GroupItem ? 0 : pathColumn()));
		
	// Really check if it's of the intended type...
	QListIterator<QTreeWidgetItem *> iter(items);
	while (iter.hasNext()) {
		QTreeWidgetItem *pItem = iter.next();
		if (pItem->type() == iType)
			return pItem;
	}

	// Not found.
	return NULL;
}


// Find and return the nearest group item...
qtractorFileGroupItem *qtractorFileListView::groupItem (
	QTreeWidgetItem *pItem ) const
{
	while (pItem && pItem->type() != GroupItem)
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
	    setRecentDir(QFileInfo(files.first()).absolutePath());

	return files;
}


// In-place toggle slot.
void qtractorFileListView::itemClickedSlot ( QTreeWidgetItem *pItem )
{
	if (pItem == NULL)
		return;

	if (pItem->type() == GroupItem) {
		qtractorFileGroupItem *pGroupItem
			= static_cast<qtractorFileGroupItem *> (pItem);
		if (pGroupItem)
			pGroupItem->setOpen(!pGroupItem->isOpen());
	}
	else
	if (pItem->type() == FileItem) {
		qtractorFileListItem *pFileItem
			= static_cast<qtractorFileListItem *> (pItem);
		if (pFileItem)
			emit selected(pFileItem->path(), -1, pFileItem->isSelected());
		if (pFileItem && pFileItem->childCount() > 0)
			pFileItem->setOpen(!pFileItem->isOpen());
	}
	else
	if (pItem->type() == ChannelItem) {
		qtractorFileChannelItem *pChannelItem
			= static_cast<qtractorFileChannelItem *> (pItem);
		if (pChannelItem)
			emit selected(pChannelItem->path(),
				pChannelItem->channel(),
				pChannelItem->isSelected());
	}
}


// In-place activation slot.
void qtractorFileListView::itemActivatedSlot ( QTreeWidgetItem *pItem )
{
	activateItem(pItem);
}


// In-place open/close slot.
void qtractorFileListView::itemExpandedSlot ( QTreeWidgetItem *pItem )
{
	if (pItem->type() == GroupItem)
		pItem->setIcon(0, QIcon(":/images/itemGroupOpen.png"));
}

void qtractorFileListView::itemCollapsedSlot ( QTreeWidgetItem *pItem )
{
	if (pItem->type() == GroupItem)
		pItem->setIcon(0, QIcon(":/images/itemGroup.png"));
}


// Tracking of item changes (e.g in-place edits).
void qtractorFileListView::itemRenamedSlot (void)
{
	// We just know that something is in edit mode...
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
		QObject::connect(m_pAutoOpenTimer,
			SIGNAL(timeout()),
			SLOT(timeoutSlot()));
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
		qtractorFileGroupItem *pGroupItem = groupItem(m_pDropItem);
		if (pGroupItem && !pGroupItem->isOpen())
			pGroupItem->setOpen(true);
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


// Trap for help/tool-tip events.
bool qtractorFileListView::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	QWidget *pViewport = QTreeWidget::viewport();
	if (static_cast<QWidget *> (pObject) == pViewport
		&& pEvent->type() == QEvent::ToolTip) {
		QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
		if (pHelpEvent) {
			QTreeWidgetItem *pItem = QTreeWidget::itemAt(pHelpEvent->pos());
			qtractorFileGroupItem *pFileItem
				= static_cast<qtractorFileGroupItem *> (pItem);
			if (pFileItem) {
				QToolTip::showText(pHelpEvent->globalPos(),
					pFileItem->toolTip(), pViewport);
				return true;
			}
		}
	}

	// Not handled here.
	return QTreeWidget::eventFilter(pObject, pEvent);
}


// Handle mouse events for drag-and-drop stuff.
void qtractorFileListView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	dragLeaveEvent(NULL);
	m_pDragItem = NULL;

	if (pMouseEvent->button() == Qt::LeftButton) {
		m_posDrag   = pMouseEvent->pos();
		m_pDragItem = QTreeWidget::itemAt(m_posDrag);
#if 0
		if (m_pDragItem && m_pDragItem->isSelected()
			&& (pMouseEvent->modifiers()
				& (Qt::ShiftModifier | Qt::ControlModifier)) == 0)
			return;
#endif
	}

	QTreeWidget::mousePressEvent(pMouseEvent);
}


void qtractorFileListView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	QTreeWidget::mouseMoveEvent(pMouseEvent);

	if ((pMouseEvent->buttons() & Qt::LeftButton) && m_pDragItem
		&& ((pMouseEvent->pos() - m_posDrag).manhattanLength()
			>= QApplication::startDragDistance())) {
		// We'll start dragging something alright...
		QMimeData *pMimeData = NULL;
		switch (m_pDragItem->type()) {
		case GroupItem: {
			pMimeData = new QMimeData();
			pMimeData->setText(m_pDragItem->text(0));
			break;
		}
		case FileItem: {
			// Selected items must only be file items...
			QList<QUrl> urls;
			QListIterator<QTreeWidgetItem *> iter(selectedItems());
			while (iter.hasNext()) {
				QTreeWidgetItem *pItem = iter.next();
				if (pItem->type() == FileItem) {
					qtractorFileListItem *pFileItem
						= static_cast<qtractorFileListItem *> (pItem);
					if (pFileItem)
						urls.append(QUrl::fromLocalFile(pFileItem->path()));
				}
			}
			// Have we got something?
			if (!urls.isEmpty()) {
				pMimeData = new QMimeData();
				pMimeData->setUrls(urls);
			}
			break;
		}
		case ChannelItem: {
			qtractorFileChannelDrag::List items;
			QListIterator<QTreeWidgetItem *> iter(selectedItems());
			while (iter.hasNext()) {
				QTreeWidgetItem *pItem = iter.next();
				if (pItem->type() == ChannelItem) {
					qtractorFileChannelItem *pChannelItem
						= static_cast<qtractorFileChannelItem *> (pItem);
					if (pChannelItem) {
						qtractorFileChannelDrag::Item item;
						item.channel = pChannelItem->channel();
						item.path = pChannelItem->path();
						items.append(item);
					}
				}
			}
			if (!items.isEmpty()) {
				pMimeData = new QMimeData();
				qtractorFileChannelDrag::encode(pMimeData, items);
			}
			break;
		}
		default:
			break;
		}
		// Have we got it right?
		if (pMimeData) {
			QDrag *pDrag = new QDrag(this);
			pDrag->setMimeData(pMimeData);
			pDrag->setPixmap(m_pDragItem->icon(0).pixmap(16));
			pDrag->setHotSpot(QPoint(-4, -12));
			pDrag->start(Qt::CopyAction | Qt:: MoveAction);
			// We've dragged and maybe dropped it by now...
			dragLeaveEvent(NULL);
			m_pDragItem = NULL;
		}
	}
}


void qtractorFileListView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QTreeWidget::mouseReleaseEvent(pMouseEvent);

	dragLeaveEvent(NULL);
	m_pDragItem = NULL;
}


// Drag-n-drop stuff.
bool qtractorFileListView::canDecodeEvent ( QDropEvent *pDropEvent )
{
	if (m_pDragItem && pDropEvent->source() != this)
		m_pDragItem = NULL;

	return (pDropEvent->mimeData()->hasText()
		|| pDropEvent->mimeData()->hasUrls());
}


bool qtractorFileListView::canDropItem ( QTreeWidgetItem *pDropItem ) const
{
//	if (pDropItem == NULL)
//		return false;

	if (m_pDragItem) {
		while (pDropItem) {
			if (pDropItem == m_pDragItem)
				return false;
			QTreeWidgetItem *pParentItem = pDropItem->parent();
			if (pParentItem == m_pDragItem)
				return false;
			pDropItem = pParentItem;
		}
	}

	return true;
}


// Ensure given item is brought to viewport visibility...
void qtractorFileListView::ensureVisibleItem ( QTreeWidgetItem *pItem )
{
	QTreeWidgetItem *pItemAbove = pItem->parent();
	if (pItemAbove) {
		int iItem = pItemAbove->indexOfChild(pItem);
		if (iItem > 0)
			pItemAbove = pItemAbove->child(iItem - 1);
	} else {
		int iItem = QTreeWidget::indexOfTopLevelItem(pItem);
		if (iItem > 0) {
			pItemAbove = QTreeWidget::topLevelItem(iItem - 1);
			if (pItemAbove) {
				int iItemCount = pItemAbove->childCount();
				if (pItemAbove->isExpanded())
					pItemAbove = pItemAbove->child(iItemCount - 1);
			}
		}
	}

	if (pItemAbove)
		QTreeWidget::scrollToItem(pItemAbove);

	QTreeWidget::scrollToItem(pItem);
}


// Track on the current drop item position.
QTreeWidgetItem *qtractorFileListView::dragDropItem ( const QPoint& pos )
{
	QTreeWidgetItem *pDropItem = QTreeWidget::itemAt(pos);

	if (pDropItem && pDropItem->type() != ChannelItem) {
		if (pDropItem != m_pDropItem) {
			m_pDropItem = pDropItem;
			ensureVisibleItem(m_pDropItem);
			if (m_pAutoOpenTimer)
				m_pAutoOpenTimer->start(m_iAutoOpenTimeout);
		}
	} else {
		m_pDropItem = NULL;
		if (m_pAutoOpenTimer)
			m_pAutoOpenTimer->stop();
	}

	moveRubberBand(m_pDropItem, pos.x() < QTreeWidget::indentation());

	return pDropItem;
}


void qtractorFileListView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
#if 0
	if (!canDecodeEvent(pDragEnterEvent)) {
		pDragEnterEvent->ignore();
		return;
	}

	QTreeWidgetItem *pDropItem = dragDropItem(pDragEnterEvent->pos());
	if (canDropItem(pDropItem)) {
		if (!pDragEnterEvent->isAccepted()) {
			pDragEnterEvent->setDropAction(Qt::MoveAction);
			pDragEnterEvent->accept();
		}
	} else {
		pDragEnterEvent->ignore();
	}
#else
	// Always accept the drag-enter event,
	// so let we deal with it during move later...
	pDragEnterEvent->accept();
#endif
}


void qtractorFileListView::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	if (!canDecodeEvent(pDragMoveEvent)) {
		pDragMoveEvent->ignore();
		return;
	}

	QTreeWidgetItem *pDropItem = dragDropItem(pDragMoveEvent->pos());
	if (canDropItem(pDropItem)) {
		if (!pDragMoveEvent->isAccepted()) {
			pDragMoveEvent->setDropAction(Qt::MoveAction);
			pDragMoveEvent->accept();
		}
	} else {
		pDragMoveEvent->ignore();
	}
}


void qtractorFileListView::dragLeaveEvent ( QDragLeaveEvent */*pDragLeaveEvent*/ )
{
	if (m_pRubberBand)
		delete m_pRubberBand;
	m_pRubberBand = NULL;

	m_pDropItem = NULL;
	if (m_pAutoOpenTimer)
		m_pAutoOpenTimer->stop();
}


void qtractorFileListView::dropEvent ( QDropEvent *pDropEvent )
{
	QTreeWidgetItem *pDropItem = dragDropItem(pDropEvent->pos());
	if (!canDropItem(pDropItem)) {
		dragLeaveEvent(NULL);
		m_pDragItem = NULL;
		return;
	}

	// Not quite parent, but the exact drop target...
	qtractorFileGroupItem *pParentItem
		=  static_cast<qtractorFileGroupItem *> (pDropItem);
	bool bOutdent = (pDropEvent->pos().x() < QTreeWidget::indentation());

	// Get access to the pertinent drop data...
	int iUpdate = 0;
	const QMimeData *pMimeData = pDropEvent->mimeData();
	// Let's see how many files there are...
	if (pMimeData->hasUrls()) {
		QListIterator<QUrl> iter(pMimeData->urls());
		iter.toBack();
		while (iter.hasPrevious()) {
			const QString& sPath = iter.previous().toLocalFile();
			// Is it one from ourselves (file item) ?...
			if (m_pDragItem && m_pDragItem->type() == FileItem) {
				if (dropItem(pDropItem, findItem(sPath, FileItem), bOutdent))
					++iUpdate;
			} else if (addFileItem(sPath, pParentItem))
				++iUpdate;
		}
	} else if (pMimeData->hasText()) {
		// Maybe its just a new convenience group...
		const QString& sText = pMimeData->text();
		// Is it one from ourselves (group item) ?...
		if (m_pDragItem && m_pDragItem->type() == GroupItem) {
			if (dropItem(pDropItem, findItem(sText, GroupItem), bOutdent))
				++iUpdate;
		} else if (addGroupItem(sText, pParentItem))
			++iUpdate;
	}

	// Teke care of change modification...
	if (iUpdate > 0) {
		// Make parent open, anyway...
		qtractorFileGroupItem *pGroupItem = groupItem(pDropItem);
		if (pGroupItem)
			pGroupItem->setOpen(true);
		// Notify that we've changed something.
		emit contentsChanged();
	}

	dragLeaveEvent(NULL);
	m_pDragItem = NULL;
}


// Drag-and-drop target method...
QTreeWidgetItem *qtractorFileListView::dropItem (
	QTreeWidgetItem *pDropItem, QTreeWidgetItem *pDragItem, bool bOutdent )
{
	// We must be dropping something...
	if (pDragItem == NULL)
		return NULL;

	// Take the item from list...
	int iItem;
	QTreeWidgetItem *pParentItem = pDragItem->parent();
	if (pParentItem) {
		iItem = pParentItem->indexOfChild(pDragItem);
		if (iItem >= 0)
			pDragItem = pParentItem->takeChild(iItem);
	} else {
		iItem = QTreeWidget::indexOfTopLevelItem(pDragItem);
		if (iItem >= 0)
			pDragItem = QTreeWidget::takeTopLevelItem(iItem);
	}

	// Insert it back...
	if (pDropItem) {
		if (pDropItem->type() == GroupItem && !bOutdent) {
			pDropItem->insertChild(0, pDragItem);
		} else {
			pParentItem = pDropItem->parent();
			if (pParentItem) {
				iItem = pParentItem->indexOfChild(pDropItem);
				pParentItem->insertChild(iItem + 1, pDragItem);
			} else {
				iItem = QTreeWidget::indexOfTopLevelItem(pDropItem);
				QTreeWidget::insertTopLevelItem(iItem + 1, pDragItem);
			}
		}
	} else {
		QTreeWidget::addTopLevelItem(pDragItem);
	}

	// Return the new item...
	return pDragItem;
}

// Draw a dragging separator line.
void qtractorFileListView::moveRubberBand ( QTreeWidgetItem *pDropItem, bool bOutdent )
{
	// Is there any item upon we migh drop anything?
	if (pDropItem == NULL) {
		if (m_pRubberBand)
			m_pRubberBand->hide();
		return;
	}

	// Create the rubber-band if there's none...
	if (m_pRubberBand == NULL) {
		m_pRubberBand = new qtractorRubberBand(
			QRubberBand::Line, QTreeWidget::viewport());
	//	QPalette pal(m_pRubberBand->palette());
	//	pal.setColor(m_pRubberBand->foregroundRole(), Qt::blue);
	//	m_pRubberBand->setPalette(pal);
	//	m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	}

	// Just move it
	QRect rect = QTreeWidget::visualItemRect(pDropItem);
	if (pDropItem->type() == GroupItem && !bOutdent)
		rect.setX(rect.x() + QTreeWidget::indentation());
	rect.setTop(rect.bottom());
	rect.setHeight(2);
	m_pRubberBand->setGeometry(rect);

	// Ah, and make it visible, of course...
	if (!m_pRubberBand->isVisible())
		m_pRubberBand->show();
}


// Custom file list loaders.
bool qtractorFileListView::loadListElement ( qtractorDocument *pDocument,
	QDomElement *pElement, QTreeWidgetItem *pItem )
{
	if (pItem && pItem->type() != GroupItem)
		return false;
	qtractorFileGroupItem *pParentItem
		= static_cast<qtractorFileGroupItem *> (pItem);

	// Make it all relative to session directory...
	QDir dir;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		dir.setPath(pSession->sessionDir());

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
				= new qtractorFileGroupItem(eChild.attribute("name"));
			if (pParentItem)
				pParentItem->addChild(pGroupItem);
			else
				addTopLevelItem(pGroupItem);
			if (!loadListElement(pDocument, &eChild, pGroupItem))
				return false;
			pGroupItem->setOpen(
				qtractorDocument::boolFromText(eChild.attribute("open")));
		}
		else
		if (eChild.tagName() == "file") {
			qtractorFileListItem *pFileItem = createFileItem(
				QDir::cleanPath(dir.absoluteFilePath(eChild.text())));
			if (pFileItem) {
				pFileItem->setText(0, eChild.attribute("name"));
				if (pParentItem)
					pParentItem->addChild(pFileItem);
				else
					QTreeWidget::addTopLevelItem(pFileItem);
				if (pSession)
					pSession->files()->addFileItem(m_iFileType, pFileItem);
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
	QDomElement *pElement, QTreeWidgetItem *pItem )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Create the new child element...
	switch (pItem->type()) {
	case GroupItem: {
		qtractorFileGroupItem *pGroupItem
			= static_cast<qtractorFileGroupItem *> (pItem);
		QDomElement eGroup = pDocument->document()->createElement("group");
		eGroup.setAttribute("name",	pGroupItem->text(0));
		eGroup.setAttribute("open",
			qtractorDocument::textFromBool(pGroupItem->isOpen()));
		int iChildCount = pItem->childCount();
		for (int i = 0; i < iChildCount; ++i)
			saveListElement(pDocument, &eGroup, pGroupItem->child(i));
		pElement->appendChild(eGroup);
		break;
	}
	case FileItem: {
		qtractorFileListItem *pFileItem
			= static_cast<qtractorFileListItem *> (pItem);
		QDomElement eFile = pDocument->document()->createElement("file");
		eFile.setAttribute("name", pFileItem->text(0));
		// Make it all relative to archive or session directory...
		QString sPath = pFileItem->path();
		if (pDocument->isArchive()) {
			qtractorFileList::Item *pFileListItem
				= pSession->files()->findItem(m_iFileType, sPath);
			if (pFileListItem && pFileListItem->clipRefCount() > 0)
				sPath = pDocument->addFile(sPath);
		} else {
			QDir dir;
			dir.setPath(pSession->sessionDir());
			sPath = dir.relativeFilePath(sPath);
		}
		eFile.appendChild(pDocument->document()->createTextNode(sPath));
		pElement->appendChild(eFile);
		break;
	}}

	return true;
}


// The elemental saver implementation.
bool qtractorFileListView::saveElement ( qtractorDocument *pDocument,
	QDomElement *pElement )
{
	int iItemCount = QTreeWidget::topLevelItemCount();
	for (int i = 0; i < iItemCount; ++i)
		saveListElement(pDocument, pElement, QTreeWidget::topLevelItem(i));
	
	return true;
}


// end of qtractorFileListView.cpp

