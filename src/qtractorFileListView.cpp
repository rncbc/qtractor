// qtractorFileListView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorDocument.h"

#include "qtractorMainForm.h"
#include "qtractorOptions.h"

#include <QMessageBox>
#include <QApplication>
#include <QHeaderView>
#include <QFileInfo>
#include <QToolTip>
#include <QAction>
#include <QTimer>
#include <QMenu>
#include <QUrl>

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QDragLeaveEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>


//----------------------------------------------------------------------
// class qtractorFileGroupItem -- custom group list view item.
//

// Constructors.
qtractorFileGroupItem::qtractorFileGroupItem (
	qtractorFileListView *pListView, const QString& sName, int iType )
	: QTreeWidgetItem(pListView, iType)
{
	initFileGroupItem(sName, iType);
}


qtractorFileGroupItem::qtractorFileGroupItem (
	qtractorFileGroupItem *pGroupItem, const QString& sName, int iType )
	: QTreeWidgetItem(pGroupItem, iType)
{
	initFileGroupItem(sName, iType);
}


// Default destructor.
qtractorFileGroupItem::~qtractorFileGroupItem (void)
{
}


// Common group-item initializer.
void qtractorFileGroupItem::initFileGroupItem (
	const QString& sName, int iType )
{
	QTreeWidgetItem::setIcon(0, QIcon(":/icons/itemGroup.png"));
	QTreeWidgetItem::setText(0, sName);

	if (iType == qtractorFileListView::GroupItem ||
		iType == qtractorFileListView::FileItem) {
		QTreeWidgetItem::setFlags(
			QTreeWidgetItem::flags() | Qt::ItemIsEditable);
	}
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
			bOpen ? ":/icons/itemGroupOpen.png" : ":/icons/itemGroup.png"));
	}

	// Open it up...
#if QT_VERSION >= 0x040201
	QTreeWidgetItem::setExpanded(bOpen);
#else
	QTreeWidgetItem::treeWidget()->setItemExpanded(this, bOpen);
#endif

	// All ancestors should be also visible.
	if (bOpen) {
		qtractorFileGroupItem *pGroupItem = groupItem();
		if (pGroupItem)
			pGroupItem->setOpen(true);
	}
}


bool qtractorFileGroupItem::isOpen (void) const
{
#if QT_VERSION >= 0x040201
	return QTreeWidgetItem::isExpanded();
#else
	return QTreeWidgetItem::treeWidget()->isItemExpanded(this);
#endif
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
	: qtractorFileGroupItem(pListView, QFileInfo(sPath).fileName(),
		qtractorFileListView::FileItem)
{
	m_sPath = sPath;
}

qtractorFileListItem::qtractorFileListItem (
	qtractorFileGroupItem *pGroupItem, const QString& sPath )
	: qtractorFileGroupItem(pGroupItem, QFileInfo(sPath).fileName(),
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
	: qtractorFileGroupItem(pFileItem, sName,
		qtractorFileListView::ChannelItem)
{
	m_iChannel = iChannel;
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


//----------------------------------------------------------------------
// class qtractorFileChannelDrag -- custom file channel drag object.
//

static const char *c_pszFileChannelMimeType = "qtractor/file-channel";

// Encoder method.
void qtractorFileChannelDrag::encode ( QMimeData *pMimeData,
	const QString& sPath, unsigned short iChannel )
{
	QByteArray data(sizeof(iChannel) + sPath.length() + 1, (char) 0);
	char *pData = data.data();
	::memcpy(pData, &iChannel, sizeof(iChannel));
	::memcpy(pData + sizeof(iChannel),
		sPath.toUtf8().constData(), sPath.length() + 1);
	pMimeData->setData(c_pszFileChannelMimeType, data);
}

// Decode trial method.
bool qtractorFileChannelDrag::canDecode ( const QMimeData *pMimeData )
{
	return pMimeData->hasFormat(c_pszFileChannelMimeType);
}

// Decode method.
bool qtractorFileChannelDrag::decode ( const QMimeData *pMimeData,
	QString& sPath, unsigned short *piChannel )
{
	QByteArray data = pMimeData->data(c_pszFileChannelMimeType);
	if (data.size() < (int) sizeof(unsigned short) + 1)
		return false;

	const char *pData = data.constData();
	sPath = (pData + sizeof(unsigned short));
	if (piChannel)
		::memcpy(piChannel, pData, sizeof(unsigned short));

	return true;
}


//----------------------------------------------------------------------------
// qtractorFileListView -- Group/File list view, supporting drag-n-drop.
//

// Constructor.
qtractorFileListView::qtractorFileListView ( QWidget *pParent )
	: QTreeWidget(pParent)
{
	m_pAutoOpenTimer   = NULL;
	m_iAutoOpenTimeout = 0;

	m_pDragItem = NULL;
	m_pDropItem = NULL;

	m_pNewGroupAction   = new QAction(tr("New &Group"), this);
	m_pOpenFileAction   = new QAction(tr("Add &Files..."), this);
	m_pRenameItemAction = new QAction(tr("R&ename"), this);
	m_pDeleteItemAction = new QAction(tr("&Delete"), this);

	m_pNewGroupAction->setShortcut(tr("Ctrl+G"));
	m_pOpenFileAction->setShortcut(tr("Ctrl+F"));
	m_pRenameItemAction->setShortcut(tr("Ctrl+E"));
	m_pDeleteItemAction->setShortcut(tr("Ctrl+D"));

	QTreeWidget::setRootIsDecorated(false);
	QTreeWidget::setUniformRowHeights(true);
//	QTreeWidget::setDragEnabled(true);
	QTreeWidget::setAcceptDrops(true);
	QTreeWidget::setDropIndicatorShown(true);
	QTreeWidget::setAutoScroll(true);
	QTreeWidget::setSelectionMode(QAbstractItemView::SingleSelection);
	QTreeWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	QTreeWidget::setSortingEnabled(false);

	QHeaderView *pHeader = QTreeWidget::header();
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
//	pHeader->setDefaultSectionSize(160);
	pHeader->setStretchLastSection(true);
	pHeader->setMovable(false);

	// Trap for help/tool-tips events.
	QTreeWidget::viewport()->installEventFilter(this);

	// Some actions surely need those
	// shortcuts firmly attached...
	QTreeWidget::addAction(m_pNewGroupAction);
	QTreeWidget::addAction(m_pOpenFileAction);
	QTreeWidget::addAction(m_pRenameItemAction);
	QTreeWidget::addAction(m_pDeleteItemAction);

	setAutoOpenTimeout(800);

	QObject::connect(m_pNewGroupAction,
		SIGNAL(triggered(bool)),
		SLOT(newGroupSlot()));
	QObject::connect(m_pOpenFileAction,
		SIGNAL(triggered(bool)),
		SLOT(openFileSlot()));
	QObject::connect(m_pRenameItemAction,
		SIGNAL(triggered(bool)),
		SLOT(renameItemSlot()));
	QObject::connect(m_pDeleteItemAction,
		SIGNAL(triggered(bool)),
		SLOT(deleteItemSlot()));

	QObject::connect(this,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(currentItemChangedSlot()));
	QObject::connect(this,
		SIGNAL(itemClicked(QTreeWidgetItem*,int)),
		SLOT(itemClickedSlot(QTreeWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
		SLOT(itemActivatedSlot(QTreeWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemActivated(QTreeWidgetItem*,int)),
		SLOT(itemActivatedSlot(QTreeWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemExpanded(QTreeWidgetItem*)),
		SLOT(itemExpandedSlot(QTreeWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemCollapsed(QTreeWidgetItem*)),
		SLOT(itemCollapsedSlot(QTreeWidgetItem*)));
	QObject::connect(QTreeWidget::itemDelegate(),
		SIGNAL(commitData(QWidget*)),
		SLOT(itemRenamedSlot()));

	currentItemChangedSlot();
}


// Default destructor.
qtractorFileListView::~qtractorFileListView (void)
{
	setAutoOpenTimeout(0);

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
		QTreeWidget::setCurrentItem(pFileItem);

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

	QTreeWidget::setCurrentItem(pGroupItem);

	return pGroupItem;
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
					// Done.
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
		Qt::MatchFlags(
			Qt::MatchExactly | Qt::CaseSensitive | Qt::MatchRecursive),
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
		= groupItem(QTreeWidget::currentItem());
	// Pick each one of the selected files...
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		const QString& sPath = iter.next();
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
		= groupItem(QTreeWidget::currentItem());
	qtractorFileGroupItem *pGroupItem
		= addGroupItem(tr("New Group"), pParentItem);
	if (pParentItem)
		pParentItem->setOpen(true);
	if (pGroupItem)
		QTreeWidget::editItem(pGroupItem, 0);
}


// Rename current group/file item.
void qtractorFileListView::renameItemSlot (void)
{
	QTreeWidgetItem *pItem = QTreeWidget::currentItem();
	if (pItem)
		QTreeWidget::editItem(pItem, 0);
}


// Remove current group/file item.
void qtractorFileListView::deleteItemSlot (void)
{
	QTreeWidgetItem *pItem = QTreeWidget::currentItem();
	if (pItem == NULL)
		return;

	// Prompt user if he/she's sure about this...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions && pOptions->bConfirmRemove) {
			if (QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("About to remove %1 item:\n\n"
				"\"%2\"\n\n"
				"Are you sure?")
				.arg(pItem->type() == GroupItem ? tr("group") : tr("file"))
				.arg(pItem->text(0)),
				tr("OK"), tr("Cancel")) > 0)
				return;
		}
	}

	// Definitive delete...
	delete pItem;
	emit contentsChanged();
}


// In-place selection slot.
void qtractorFileListView::currentItemChangedSlot (void)
{
	QTreeWidgetItem *pItem = QTreeWidget::currentItem();
	bool bEnabled = (pItem && pItem->type() != ChannelItem);
	m_pRenameItemAction->setEnabled(bEnabled);
	m_pDeleteItemAction->setEnabled(bEnabled);
}


// In-place toggle slot.
void qtractorFileListView::itemClickedSlot ( QTreeWidgetItem *pItem )
{
	if (pItem && pItem->childCount() > 0) {
		qtractorFileGroupItem *pGroupItem
			= static_cast<qtractorFileGroupItem *> (pItem);
		if (pGroupItem)
			pGroupItem->setOpen(!pGroupItem->isOpen());
	}
}


// In-place activation slot.
void qtractorFileListView::itemActivatedSlot ( QTreeWidgetItem *pItem )
{
	if (pItem && pItem->type() == FileItem) {
		qtractorFileListItem *pFileItem
			= static_cast<qtractorFileListItem *> (pItem);
		if (pFileItem)
			emit activated(pFileItem->path());
	}
}


// In-place open/close slot.
void qtractorFileListView::itemExpandedSlot ( QTreeWidgetItem *pItem )
{
	if (pItem->type() == GroupItem)
		pItem->setIcon(0, QIcon(":/icons/itemGroupOpen.png"));
}

void qtractorFileListView::itemCollapsedSlot ( QTreeWidgetItem *pItem )
{
	if (pItem->type() == GroupItem)
		pItem->setIcon(0, QIcon(":/icons/itemGroup.png"));
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
	QTreeWidget::mousePressEvent(pMouseEvent);

	if (pMouseEvent->button() == Qt::LeftButton) {
		m_posDrag   = pMouseEvent->pos();
		m_pDragItem = QTreeWidget::itemAt(m_posDrag);
	}
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
				qtractorFileListItem *pFileItem
					= static_cast<qtractorFileListItem *> (m_pDragItem);
				if (pFileItem) {
					QList<QUrl> urls;
					urls.append(QUrl::fromLocalFile(pFileItem->path()));
					pMimeData = new QMimeData();
					pMimeData->setUrls(urls);
				}
				break;
			}
			case ChannelItem: {
				qtractorFileListItem *pFileItem
					= static_cast<qtractorFileListItem *> (m_pDragItem->parent());
				qtractorFileChannelItem *pChannelItem
					= static_cast<qtractorFileChannelItem *> (m_pDragItem);
				if (pFileItem && pChannelItem) {
					pMimeData = new QMimeData();
					qtractorFileChannelDrag::encode(pMimeData,
						pFileItem->path(), pChannelItem->channel());
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
			m_pDragItem = NULL;
		}
	}
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
//	if (pItem == NULL)
//		return false;

	if (m_pDragItem) {
		while (pDropItem) {
			QTreeWidgetItem *pParentItem = pDropItem->parent();
			if (pParentItem == m_pDragItem)
				return false;
			pDropItem = pParentItem;
		}
	}

	return true;
}


QTreeWidgetItem *qtractorFileListView::dragDropItem ( const QPoint& pos )
{
	QTreeWidgetItem *pDropItem = QTreeWidget::itemAt(pos);

	if (pDropItem && pDropItem->type() != ChannelItem) {
		if (pDropItem != m_pDropItem) {
			m_pDropItem = pDropItem;
			QTreeWidget::setCurrentItem(m_pDropItem);
			if (m_pAutoOpenTimer)
				m_pAutoOpenTimer->start(m_iAutoOpenTimeout);
		}
	} else {
		QTreeWidget::setItemSelected(m_pDropItem, false);
		m_pDropItem = NULL;
		if (m_pAutoOpenTimer)
			m_pAutoOpenTimer->stop();
	}

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


void qtractorFileListView::dragLeaveEvent ( QDragLeaveEvent * )
{
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
	}

	// It's one from ourselves?...
	if (m_pDragItem) {
		// Take the item from list...
		int iItem;
		QTreeWidgetItem *pDragItem = NULL;
		QTreeWidgetItem *pParentItem = m_pDragItem->parent();
		if (pParentItem) {
			iItem = pParentItem->indexOfChild(m_pDragItem);
			if (iItem >= 0)
				pDragItem = pParentItem->takeChild(iItem);
		} else {
			iItem = QTreeWidget::indexOfTopLevelItem(m_pDragItem);
			if (iItem >= 0)
				pDragItem = QTreeWidget::takeTopLevelItem(iItem);
		}
		// Insert it back...
		if (pDragItem) {
			if (pDropItem) {
				if (pDropItem->type() == GroupItem) {
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
			// Make parent open, anyway...
			qtractorFileGroupItem *pGroupItem = groupItem(pDropItem);
			if (pGroupItem)
				pGroupItem->setOpen(true);
			// And make it new current/selected...
			QTreeWidget::setCurrentItem(pDragItem);
		}
		// Notify that we've changed something.
		emit contentsChanged();
	} else {
		// Let's see how many files there are...
		const QMimeData *pMimeData = pDropEvent->mimeData();
		if (pMimeData->hasUrls()) {
			QList<QUrl> list = pMimeData->urls();
			QListIterator<QUrl> iter(list);
			while (iter.hasNext()) {
				const QString& sPath = iter.next().toLocalFile();
				addFileItem(sPath,
					static_cast<qtractorFileGroupItem *> (pDropItem));
			}
		} else if (pMimeData->hasText()) {
			// Maybe its just a new convenience group...
			const QString& sText = pMimeData->text();
			addGroupItem(sText,
				static_cast<qtractorFileGroupItem *> (pDropItem));
		}
	}

	dragLeaveEvent(NULL);
	m_pDragItem = NULL;
}


// Context menu request event handler.
void qtractorFileListView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	QMenu menu(this);

	// Construct context menu.
	menu.addAction(m_pNewGroupAction);
	menu.addSeparator();
	menu.addAction(m_pOpenFileAction);
	menu.addAction(m_pRenameItemAction);
	menu.addAction(m_pDeleteItemAction);

	menu.exec(pContextMenuEvent->globalPos());
}


// Custom file list loaders.
bool qtractorFileListView::loadListElement ( qtractorDocument *pDocument,
	QDomElement *pElement, QTreeWidgetItem *pItem )
{
	if (pItem && pItem->type() != GroupItem)
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
				if (!loadListElement(pDocument, &eChild, pGroupItem))
					return false;
				pGroupItem->setOpen(
					pDocument->boolFromText(eChild.attribute("open")));
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
	QDomElement *pElement, QTreeWidgetItem *pItem )
{
	// Create the new child element...
	switch (pItem->type()) {
	case GroupItem: {
		qtractorFileGroupItem *pGroupItem
			= static_cast<qtractorFileGroupItem *> (pItem);
		QDomElement eGroup = pDocument->document()->createElement("group");
		eGroup.setAttribute("name",	pGroupItem->text(0));
		eGroup.setAttribute("open",	pDocument->textFromBool(pGroupItem->isOpen()));
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
		eFile.appendChild(pDocument->document()->createTextNode(
			pFileItem->path()));
		pElement->appendChild(eFile);
		break;
	}
	}

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

