// qtractorConnect.cpp
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

#include "qtractorConnect.h"

#include <QApplication>
#include <QHeaderView>
#include <QScrollBar>
#include <QToolTip>
#include <QPainter>
#include <QPolygon>
#include <QPainterPath>
#include <QTimer>
#include <QMenu>

#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QDragLeaveEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QContextMenuEvent>

#if QT_VERSION >= 0x050000
#include <QMimeData>
#include <QDrag>
#endif


//----------------------------------------------------------------------
// qtractorPortListItem -- Port list item.
//

// Constructor.
qtractorPortListItem::qtractorPortListItem (
	qtractorClientListItem *pClientItem, const QString& sPortName )
	: QTreeWidgetItem(pClientItem, qtractorConnect::PortItem)
{
	m_pClientItem = pClientItem;
	m_sPortName   = sPortName;
	m_iPortMark   = 0;
	m_bHilite     = false;

	QTreeWidgetItem::setText(0, m_sPortName);
}

// Default destructor.
qtractorPortListItem::~qtractorPortListItem (void)
{
	m_connects.clear();
}


// Instance accessors.
void qtractorPortListItem::setPortName ( const QString& sPortName )
{
	QTreeWidgetItem::setText(0, sPortName);

	m_sPortName = sPortName;
}

const QString& qtractorPortListItem::clientName (void) const
{
	return m_pClientItem->clientName();
}

const QString& qtractorPortListItem::portName (void) const
{
	return m_sPortName;
}


// Complete client:port name helper.
QString qtractorPortListItem::clientPortName (void)
{
	return m_pClientItem->clientName() + ':' + m_sPortName;
}


// Connect client item accessor.
qtractorClientListItem *qtractorPortListItem::clientItem (void) const
{
	return m_pClientItem;
}


// Client:port set housekeeping marker.
void qtractorPortListItem::markPort ( int iMark )
{
	setHilite(false);

	m_iPortMark = iMark;
	if (iMark > 0)
		m_connects.clear();
}

void qtractorPortListItem::markClientPort ( int iMark )
{
	markPort(iMark);

	m_pClientItem->markClient(iMark);
}

int qtractorPortListItem::portMark (void) const
{
	return m_iPortMark;
}


// Connected port list primitives.
void qtractorPortListItem::addConnect ( qtractorPortListItem *pPortItem )
{
	m_connects.append(pPortItem);
}

void qtractorPortListItem::removeConnect ( qtractorPortListItem *pPortItem )
{
	QMutableListIterator<qtractorPortListItem *> iter(m_connects);
	while (iter.hasNext()) {
		qtractorPortListItem *pPortItemPtr = iter.next();
		if (pPortItemPtr == pPortItem) {
			pPortItem->setHilite(false);
			iter.remove();
			break;
		}
	}
}


// Clear the connection list, taking care of hilighting...
void qtractorPortListItem::cleanConnects (void)
{
	QMutableListIterator<qtractorPortListItem *> iter(m_connects);
	while (iter.hasNext()) {
		qtractorPortListItem *pPortItem = iter.next();
		pPortItem->removeConnect(this);
		iter.remove();
	}
}


// Connected port finder.
qtractorPortListItem *qtractorPortListItem::findConnect (
	qtractorPortListItem *pPortItem )
{
	QListIterator<qtractorPortListItem *> iter(m_connects);
	while (iter.hasNext()) {
		qtractorPortListItem *pPortItemPtr = iter.next();
		if (pPortItemPtr == pPortItem)
			return pPortItem;
	}

	return NULL;
}


// Connection cache list accessor.
const QList<qtractorPortListItem *>& qtractorPortListItem::connects (void) const
{
	return m_connects;
}


// Connectiopn highlight methods.
void qtractorPortListItem::setHilite ( bool bHilite )
{
	// Update the port highlightning if changed...
	if ((m_bHilite && !bHilite) || (!m_bHilite && bHilite)) {
		m_bHilite = bHilite;
		// Propagate this to the parent...
		m_pClientItem->setHilite(bHilite);
	}
	
	// Set the new color.
	const QPalette& pal = QTreeWidgetItem::treeWidget()->palette();
	QTreeWidgetItem::setForeground(0, m_bHilite
		? (pal.base().color().value() < 0x7f ? Qt::cyan : Qt::blue)
		: pal.text().color());
}

bool qtractorPortListItem::isHilite (void) const
{
	return m_bHilite;
}


// Proxy sort override method.
// - Natural decimal sorting comparator.
bool qtractorPortListItem::operator< ( const QTreeWidgetItem& other ) const
{
	QTreeWidget *pTreeWidget = QTreeWidgetItem::treeWidget();
	if (pTreeWidget == NULL)
		return false;

	const int col = pTreeWidget->sortColumn();
	if (col < 0)
		return false;

	return qtractorClientListView::lessThan(*this, other, col);
}


//----------------------------------------------------------------------------
// qtractorClientListItem -- Client list item.
//

// Constructor.
qtractorClientListItem::qtractorClientListItem (
	qtractorClientListView *pClientListView, const QString& sClientName )
	: QTreeWidgetItem(pClientListView, qtractorConnect::ClientItem)
{
	m_sClientName = sClientName;
	m_iClientMark = 0;
	m_iHilite     = 0;
	
	QTreeWidgetItem::setText(0, m_sClientName);
}

// Default destructor.
qtractorClientListItem::~qtractorClientListItem (void)
{
}


// Port finder.
qtractorPortListItem *qtractorClientListItem::findPortItem (
	const QString& sPortName )
{
	const int iChildCount = QTreeWidgetItem::childCount();
	for (int iChild = 0; iChild < iChildCount; ++iChild) {
		QTreeWidgetItem *pChild = QTreeWidgetItem::child(iChild);
		if (pChild->type() != qtractorConnect::PortItem)
			continue;
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pChild);
		if (pPortItem && pPortItem->portName() == sPortName)
			return pPortItem;
	}

	return NULL;
}


// Instance accessors.
void qtractorClientListItem::setClientName ( const QString& sClientName )
{
	QTreeWidgetItem::setText(0, sClientName);

	m_sClientName = sClientName;
}

const QString& qtractorClientListItem::clientName (void) const
{
	return m_sClientName;
}


// Readable flag client accessor.
bool qtractorClientListItem::isReadable (void) const
{
	qtractorClientListView *pClientListView
		= static_cast<qtractorClientListView *> (QTreeWidgetItem::treeWidget());
	return (pClientListView ? pClientListView->isReadable() : false);
}


// Client:port set housekeeping marker.
void qtractorClientListItem::markClient ( int iMark )
{
	setHilite(false);

	m_iClientMark = iMark;
}

void qtractorClientListItem::markClientPorts ( int iMark )
{
	markClient(iMark);

	const int iChildCount = QTreeWidgetItem::childCount();
	for (int iChild = 0; iChild < iChildCount; ++iChild) {
		QTreeWidgetItem *pChild = QTreeWidgetItem::child(iChild);
		if (pChild->type() != qtractorConnect::PortItem)
			continue;
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pChild);
		if (pPortItem)
			pPortItem->markPort(iMark);
	}
}

void qtractorClientListItem::cleanClientPorts ( int iMark )
{
	int iChildCount = QTreeWidgetItem::childCount();
	for (int iChild = 0; iChild < iChildCount; ++iChild) {
		QTreeWidgetItem *pChild = QTreeWidgetItem::child(iChild);
		if (pChild->type() != qtractorConnect::PortItem)
			continue;
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pChild);
		if (pPortItem && pPortItem->portMark() == iMark) {
			pPortItem->cleanConnects();
			delete pPortItem;
			--iChildCount;
			--iChild;
		}
	}
}

int qtractorClientListItem::clientMark (void) const
{
	return m_iClientMark;
}


// Connectiopn highlight methods.
void qtractorClientListItem::setHilite ( bool bHilite )
{
	// Update the client highlightning if changed...
	if (bHilite)
		++m_iHilite;
	else
	if (m_iHilite > 0)
		--m_iHilite;

	// Set the new color.
	const QPalette& pal = QTreeWidgetItem::treeWidget()->palette();
	QTreeWidgetItem::setForeground(0, m_iHilite > 0
		? (pal.base().color().value() < 0x7f ? Qt::darkCyan : Qt::darkBlue)
		: pal.text().color());
}

bool qtractorClientListItem::isHilite (void) const
{
	return (m_iHilite > 0);
}


// Client item openness status.
void qtractorClientListItem::setOpen ( bool bOpen )
{
	QTreeWidgetItem::setExpanded(bOpen);
}


bool qtractorClientListItem::isOpen (void) const
{
	return QTreeWidgetItem::isExpanded();
}


// Proxy sort override method.
// - Natural decimal sorting comparator.
bool qtractorClientListItem::operator< ( const QTreeWidgetItem& other ) const
{
	return qtractorClientListView::lessThan(*this, other);
}


//----------------------------------------------------------------------------
// qtractorClientListView -- Client list view, supporting drag-n-drop.
//

// Constructor.
qtractorClientListView::qtractorClientListView ( QWidget *pParent )
	: QTreeWidget(pParent)
{
	m_pConnect  = NULL;
	m_bReadable = false;

	m_pAutoOpenTimer   = NULL;
	m_iAutoOpenTimeout = 0;

	m_pDragItem = NULL;
	m_pDropItem = NULL;

	m_pHiliteItem = NULL;

	QHeaderView *pHeader = QTreeWidget::header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
//	pHeader->setDefaultSectionSize(120);
#if QT_VERSION >= 0x050000
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionsMovable(false);
	pHeader->setSectionsClickable(true);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setMovable(false);
	pHeader->setClickable(true);
#endif
	pHeader->setSortIndicatorShown(true);
	pHeader->setStretchLastSection(true);

	QTreeWidget::setRootIsDecorated(true);
	QTreeWidget::setUniformRowHeights(true);
//	QTreeWidget::setDragEnabled(true);
	QTreeWidget::setAcceptDrops(true);
	QTreeWidget::setDropIndicatorShown(true);
	QTreeWidget::setAutoScroll(true);
	QTreeWidget::setSelectionMode(QAbstractItemView::SingleSelection);
	QTreeWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	QTreeWidget::setSortingEnabled(true);
	QTreeWidget::setMinimumWidth(120);
	QTreeWidget::setColumnCount(1);

	// Trap for help/tool-tips events.
	QTreeWidget::viewport()->installEventFilter(this);

	setAutoOpenTimeout(800);
}


// Default destructor.
qtractorClientListView::~qtractorClientListView (void)
{
	setAutoOpenTimeout(0);
}


// Client item finder.
qtractorClientListItem *qtractorClientListView::findClientItem (
	const QString& sClientName )
{
	const int iItemCount = QTreeWidget::topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = QTreeWidget::topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorClientListItem *pClientItem
			= static_cast<qtractorClientListItem *> (pItem);
		if (pClientItem && pClientItem->clientName() == sClientName)
			return pClientItem;
	}

	return NULL;
}

// Client:port finder.
qtractorPortListItem *qtractorClientListView::findClientPortItem (
	const QString& sClientPort )
{
	qtractorPortListItem *pPortItem = NULL;

	const int iColon = sClientPort.indexOf(':');
	if (iColon >= 0) {
		qtractorClientListItem *pClientItem
			= findClientItem(sClientPort.left(iColon));
		if (pClientItem) {
			pPortItem = pClientItem->findPortItem(
				sClientPort.right(sClientPort.length() - iColon - 1));
		}
	}

	return pPortItem;
}


// Main controller accessors.
void qtractorClientListView::setBinding ( qtractorConnect *pConnect )
{
	m_pConnect = pConnect;
}

qtractorConnect *qtractorClientListView::binding (void) const
{
	return m_pConnect;
}
	

// Readable flag client accessors.
void qtractorClientListView::setReadable ( bool bReadable )
{
	m_bReadable = bReadable;

	QString sText;
	if (m_bReadable)
		sText = tr("Readable Clients / Output Ports");
	else
		sText = tr("Writable Clients / Input Ports");
	QTreeWidget::headerItem()->setText(0, sText);
	QTreeWidget::sortItems(0, Qt::AscendingOrder);
	QTreeWidget::setToolTip(sText);
}

bool qtractorClientListView::isReadable (void) const
{
	return m_bReadable;
}


// Client name filter helpers.
void qtractorClientListView::setClientName ( const QString& sClientName )
{
	m_rxClientName.setPattern(sClientName);

	if (sClientName.isEmpty())
		m_rxPortName.setPattern(QString::null);
}

QString qtractorClientListView::clientName (void) const
{
	return m_rxClientName.pattern();
}


// Client name filter helpers.
void qtractorClientListView::setPortName ( const QString& sPortName )
{
	m_rxPortName.setPattern(sPortName);
}

QString qtractorClientListView::portName (void) const
{
	return m_rxPortName.pattern();
}


// Maintained current client name list.
const QStringList& qtractorClientListView::clientNames (void) const
{
	return m_clientNames;
}


// Override clear method.
void qtractorClientListView::clear (void)
{
	m_pHiliteItem = NULL;
	m_clientNames.clear();	

	QTreeWidget::clear();
}


// Client filter regular expression;
// take the chance to add to maintained client name list.
bool qtractorClientListView::isClientName ( const QString& sClientName )
{
	if (m_clientNames.indexOf(sClientName) < 0)
		m_clientNames.append(sClientName);

	return (m_rxClientName.isEmpty()
		|| m_rxClientName.exactMatch(sClientName));
}


// Port filter regular expression;
bool qtractorClientListView::isPortName ( const QString& sPortName )
{
	return (m_rxClientName.isEmpty() || m_rxPortName.isEmpty()
		|| m_rxPortName.exactMatch(sPortName));
}


// Whether items are all open (expanded) or closed (collapsed).
void qtractorClientListView::setOpenAll ( bool bOpen )
{
	// For each client item...
	const int iItemCount = QTreeWidget::topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = QTreeWidget::topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorClientListItem *pClientItem
			= static_cast<qtractorClientListItem *> (pItem);
		if (pClientItem == NULL)
			continue;
		// For each port item...
		const int iChildCount = pClientItem->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChildItem = pClientItem->child(iChild);
			if (pChildItem->type() != qtractorConnect::PortItem)
				continue;
			qtractorPortListItem *pPortItem
				= static_cast<qtractorPortListItem *> (pChildItem);
			if (pPortItem) {
				pClientItem->setOpen(bOpen);
				QListIterator<qtractorPortListItem *> iter(pPortItem->connects());
				while (iter.hasNext()) {
					qtractorPortListItem *pConnectItem = iter.next();
					pConnectItem->clientItem()->setOpen(bOpen);
				}
			}
		}
	}
}


// Client:port set housekeeping marker.
void qtractorClientListView::markClientPorts ( int iMark )
{
	m_clientNames.clear();
	m_pHiliteItem = NULL;

	const int iItemCount = QTreeWidget::topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = QTreeWidget::topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorClientListItem *pClientItem
			= static_cast<qtractorClientListItem *> (pItem);
		if (pClientItem)
			pClientItem->markClientPorts(iMark);
	}
}

void qtractorClientListView::cleanClientPorts ( int iMark )
{
	int iItemCount = QTreeWidget::topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = QTreeWidget::topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorClientListItem *pClientItem
			= static_cast<qtractorClientListItem *> (pItem);
		if (pClientItem) {
			if (pClientItem->clientMark() == iMark) {
				delete pClientItem;
				--iItemCount;
				--iItem;
			} else {
				pClientItem->cleanClientPorts(iMark);
			}
		}
	}
}


// Client:port hilite update stabilization.
void qtractorClientListView::hiliteClientPorts (void)
{
	QTreeWidgetItem *pCurrentItem = QTreeWidget::currentItem();

	// Dehilite the previous selected items.
	if (m_pHiliteItem && pCurrentItem != m_pHiliteItem) {
		if (m_pHiliteItem->type() == qtractorConnect::ClientItem) {
			const int iChildCount = m_pHiliteItem->childCount();
			for (int iChild = 0; iChild < iChildCount; ++iChild) {
				QTreeWidgetItem *pChild = m_pHiliteItem->child(iChild);
				if (pChild->type() != qtractorConnect::PortItem)
					continue;
				qtractorPortListItem *pPortItem
					= static_cast<qtractorPortListItem *> (pChild);
				if (pPortItem) {
					QListIterator<qtractorPortListItem *> iter(pPortItem->connects());
					while (iter.hasNext())
						iter.next()->setHilite(false);
				}
			}
		} else {
			qtractorPortListItem *pPortItem
				= static_cast<qtractorPortListItem *> (m_pHiliteItem);
			if (pPortItem) {
				QListIterator<qtractorPortListItem *> iter(pPortItem->connects());
				while (iter.hasNext())
					iter.next()->setHilite(false);
			}
		}
	}

	// Hilite the now current selected items.
	if (pCurrentItem && pCurrentItem->type() == qtractorConnect::ClientItem) {
		const int iChildCount = pCurrentItem->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChild = pCurrentItem->child(iChild);
			if (pChild->type() != qtractorConnect::PortItem)
				continue;
			qtractorPortListItem *pPortItem
				= static_cast<qtractorPortListItem *> (pChild);
			if (pPortItem) {
				QListIterator<qtractorPortListItem *> iter(pPortItem->connects());
				while (iter.hasNext())
					iter.next()->setHilite(true);
			}
		}
	} else {
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pCurrentItem);
		if (pPortItem) {
			QListIterator<qtractorPortListItem *> iter(pPortItem->connects());
			while (iter.hasNext())
				iter.next()->setHilite(true);
		}
	}

	// Do remember this one, ever.
	m_pHiliteItem = pCurrentItem;
}


// Auto-open timeout method.
void qtractorClientListView::setAutoOpenTimeout ( int iAutoOpenTimeout )
{
	m_iAutoOpenTimeout = iAutoOpenTimeout;

	if (m_pAutoOpenTimer)
		delete m_pAutoOpenTimer;
	m_pAutoOpenTimer = NULL;

	if (m_iAutoOpenTimeout > 0) {
		m_pAutoOpenTimer = new QTimer(this);
		QObject::connect(m_pAutoOpenTimer,
			SIGNAL(timeout()),
			SLOT(timeoutSlot()));
	}
}


// Auto-open timeout accessor.
int qtractorClientListView::autoOpenTimeout (void) const
{
	return m_iAutoOpenTimeout;
}


// Auto-open timer slot.
void qtractorClientListView::timeoutSlot (void)
{
	if (m_pAutoOpenTimer) {
		m_pAutoOpenTimer->stop();
		if (m_pDropItem
			&& m_pDropItem->type() == qtractorConnect::ClientItem) {
			qtractorClientListItem *pClientItem
				= static_cast<qtractorClientListItem *> (m_pDropItem);
			if (pClientItem && !pClientItem->isOpen())
				pClientItem->setOpen(true);
		}
	}
}


// Trap for help/tool-tip events.
bool qtractorClientListView::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	QWidget *pViewport = QTreeWidget::viewport();
	if (static_cast<QWidget *> (pObject) == pViewport
		&& pEvent->type() == QEvent::ToolTip) {
		QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
		if (pHelpEvent) {
			QTreeWidgetItem *pItem = QTreeWidget::itemAt(pHelpEvent->pos());
			if (pItem && pItem->type() == qtractorConnect::ClientItem) {
				qtractorClientListItem *pClientItem
					= static_cast<qtractorClientListItem *> (pItem);
				if (pClientItem) {
					QToolTip::showText(pHelpEvent->globalPos(),
						pClientItem->clientName(), pViewport);
					return true;
				}
			}
			else
			if (pItem && pItem->type() == qtractorConnect::PortItem) {
				qtractorPortListItem *pPortItem
					= static_cast<qtractorPortListItem *> (pItem);
				if (pPortItem) {
					QToolTip::showText(pHelpEvent->globalPos(),
						pPortItem->portName(), pViewport);
					return true;
				}
			}
		}
	}

	// Not handled here.
	return QTreeWidget::eventFilter(pObject, pEvent);
}


// Drag-n-drop stuff.
QTreeWidgetItem *qtractorClientListView::dragDropItem ( const QPoint& pos )
{
	QTreeWidgetItem *pItem = QTreeWidget::itemAt(pos);
	if (pItem) {
		if (m_pDropItem != pItem) {
			QTreeWidget::setCurrentItem(pItem);
			m_pDropItem = pItem;
			if (m_pAutoOpenTimer)
				m_pAutoOpenTimer->start(m_iAutoOpenTimeout);
			qtractorConnect *pConnect = binding();
			if (pConnect == NULL || !pConnect->canConnectSelected())
				pItem = NULL;
		}
	} else {
		m_pDropItem = NULL;
		if (m_pAutoOpenTimer)
			m_pAutoOpenTimer->stop();
	}

	return pItem;
}

void qtractorClientListView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
	if (pDragEnterEvent->source() != this &&
		pDragEnterEvent->mimeData()->hasText() &&
		dragDropItem(pDragEnterEvent->pos())) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


void qtractorClientListView::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	if (pDragMoveEvent->source() != this &&
		pDragMoveEvent->mimeData()->hasText() &&
		dragDropItem(pDragMoveEvent->pos())) {
		pDragMoveEvent->accept();
	} else {
		pDragMoveEvent->ignore();
	}
}


void qtractorClientListView::dragLeaveEvent ( QDragLeaveEvent * )
{
	m_pDropItem = NULL;

	if (m_pAutoOpenTimer)
		m_pAutoOpenTimer->stop();
}


void qtractorClientListView::dropEvent( QDropEvent *pDropEvent )
{
	if (pDropEvent->source() != this &&
		pDropEvent->mimeData()->hasText() &&
		dragDropItem(pDropEvent->pos())) {
		const QString sText = pDropEvent->mimeData()->text();
		if (!sText.isEmpty() && m_pConnect)
			m_pConnect->connectSelected();
	}

	dragLeaveEvent(0);
}


// Handle mouse events for drag-and-drop stuff.
void qtractorClientListView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	QTreeWidget::mousePressEvent(pMouseEvent);

	if (pMouseEvent->button() == Qt::LeftButton) {
		m_posDrag   = pMouseEvent->pos();
		m_pDragItem = QTreeWidget::itemAt(m_posDrag);
	}
}


void qtractorClientListView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	QTreeWidget::mouseMoveEvent(pMouseEvent);

	if ((pMouseEvent->buttons() & Qt::LeftButton) && m_pDragItem
		&& ((pMouseEvent->pos() - m_posDrag).manhattanLength()
			>= QApplication::startDragDistance())) {
		// We'll start dragging something alright...
		QMimeData *pMimeData = new QMimeData();
		pMimeData->setText(m_pDragItem->text(0));
		QDrag *pDrag = new QDrag(this);
		pDrag->setMimeData(pMimeData);
		pDrag->setPixmap(m_pDragItem->icon(0).pixmap(16));
		pDrag->setHotSpot(QPoint(-4, -12));
		pDrag->start(Qt::LinkAction);
		// We've dragged and maybe dropped it by now...
		m_pDragItem = NULL;
	}
}


// Context menu request event handler.
void qtractorClientListView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	if (m_pConnect)
		m_pConnect->contextMenu(pContextMenuEvent->globalPos());
}


// Natural decimal sorting comparator.
bool qtractorClientListView::lessThan (
	const QTreeWidgetItem& i1, const QTreeWidgetItem& i2, int col )
{
	const QString& s1 = i1.text(col);
	const QString& s2 = i2.text(col);

	const int cch1 = s1.length();
	const int cch2 = s2.length();

	int ich1, ich2;

	for (ich1 = ich2 = 0; ich1 < cch1 && ich2 < cch2; ++ich1, ++ich2) {

		// Skip (white)spaces...
		while (s1.at(ich1).isSpace())
			++ich1;
		while (s2.at(ich2).isSpace())
			++ich2;

		// Normalize (to uppercase) the next characters...
		QChar ch1 = s1.at(ich1).toUpper();
		QChar ch2 = s2.at(ich2).toUpper();

		if (ch1.isDigit() && ch2.isDigit()) {
			// Find the whole length numbers...
			int iDigits1 = ich1++;
			while (ich1 < cch1 && s1.at(ich1).isDigit())
				++ich1;
			int iDigits2 = ich2++;
			while (ich2 < cch2 && s2.at(ich2).isDigit())
				++ich2;
			// Compare as natural decimal-numbers...
			int n1 = s1.mid(iDigits1, ich1 - iDigits1).toInt();
			int n2 = s2.mid(iDigits2, ich2 - iDigits2).toInt();
			if (n1 != n2)
				return (n1 < n2);
			// Never go out of bounds...
			if (ich1 >= cch1 || ich1 >= cch2)
				break;
			// Go on with this next char...
			ch1 = s1.at(ich1).toUpper();
			ch2 = s2.at(ich2).toUpper();
		}

		// Compare this char...
		if (ch1 != ch2)
			return (ch1 < ch2);
	}

	// Probable exact match.
	return false;
}


// Do proper contents refresh/update.
void qtractorClientListView::refresh (void)
{
	QHeaderView *pHeader = QTreeWidget::header();
	QTreeWidget::sortItems(
		pHeader->sortIndicatorSection(),
		pHeader->sortIndicatorOrder());
}


//----------------------------------------------------------------------
// qtractorConnectorView -- Connector view widget.
//

// Constructor.
qtractorConnectorView::qtractorConnectorView ( QWidget *pParent )
	: QWidget(pParent)
{
	m_pConnect = NULL;

	QWidget::setMinimumWidth(20);
//  QWidget::setMaximumWidth(120);
	QWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
}

// Default destructor.
qtractorConnectorView::~qtractorConnectorView (void)
{
}


// Main controller accessors.
void qtractorConnectorView::setBinding ( qtractorConnect *pConnect )
{
	m_pConnect = pConnect;
}

qtractorConnect *qtractorConnectorView::binding (void) const
{
	return m_pConnect;
}
	

// Legal client/port item position helper.
int qtractorConnectorView::itemY ( QTreeWidgetItem *pItem ) const
{
	QRect rect;
	QTreeWidget *pList = pItem->treeWidget();
	QTreeWidgetItem *pParent = pItem->parent();
	qtractorClientListItem *pClientItem = NULL;
	if (pParent && pParent->type() == qtractorConnect::ClientItem)
		pClientItem = static_cast<qtractorClientListItem *> (pParent);
	if (pClientItem && !pClientItem->isOpen()) {
		rect = pList->visualItemRect(pClientItem);
	} else {
		rect = pList->visualItemRect(pItem);
	}
	return rect.top() + rect.height() / 2;
}


// Draw visible port connection relation lines
void qtractorConnectorView::drawConnectionLine ( QPainter *pPainter,
	int x1, int y1, int x2, int y2, int h1, int h2 )
{
	// Account for list view headers.
	y1 += h1;
	y2 += h2;

	// Invisible output ports don't get a connecting dot.
	if (y1 > h1)
		pPainter->drawLine(x1, y1, x1 + 4, y1);

	// How do we'll draw it?
	if (m_pConnect->isBezierLines()) {
		// Setup control points
		QPolygon spline(4);
		int cp = int(float(x2 - x1 - 8) * 0.4f);
		spline.putPoints(0, 4,
			x1 + 4, y1, x1 + 4 + cp, y1, 
			x2 - 4 - cp, y2, x2 - 4, y2);
		// The connection line, it self.
		QPainterPath path;
		path.moveTo(spline.at(0));
		path.cubicTo(spline.at(1), spline.at(2), spline.at(3));
		pPainter->strokePath(path, pPainter->pen());
	}
	else pPainter->drawLine(x1 + 4, y1, x2 - 4, y2);

	// Invisible input ports don't get a connecting dot.
	if (y2 > h2)
		pPainter->drawLine(x2 - 4, y2, x2, y2);
}


// Draw visible port connection relation arrows.
void qtractorConnectorView::paintEvent ( QPaintEvent * )
{
	if (m_pConnect == NULL)
		return;
	if (m_pConnect->OListView() == NULL || m_pConnect->IListView() == NULL)
		return;

	qtractorClientListView *pOListView = m_pConnect->OListView();
	qtractorClientListView *pIListView = m_pConnect->IListView();

	const int yc = QWidget::pos().y();
	const int yo = pOListView->pos().y();
	const int yi = pIListView->pos().y();

	QPainter painter(this);
	int x1, y1, h1;
	int x2, y2, h2;
	int i, rgb[3] = { 0x33, 0x66, 0x99 };

	// Inline adaptive to darker background themes...
	if (QWidget::palette().window().color().value() < 0x7f)
		for (i = 0; i < 3; ++i) rgb[i] += 0x33;

	// Initialize color changer.
	i = 0;
	// Almost constants.
	x1 = 0;
	x2 = QWidget::width();
	h1 = (pOListView->header())->sizeHint().height();
	h2 = (pIListView->header())->sizeHint().height();
	// For each output client item...
	const int iItemCount = pOListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = pOListView->topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pItem);
		if (pOClient == NULL)
			continue;
		// Set new connector color.
		++i;
		painter.setPen(QColor(rgb[i % 3], rgb[(i / 3) % 3], rgb[(i / 9) % 3]));
		// For each port item
		const int iChildCount = pOClient->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChild = pOClient->child(iChild);
			if (pChild->type() != qtractorConnect::PortItem)
				continue;
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pChild);
			if (pOPort) {
				// Get starting connector arrow coordinates.
				y1 = itemY(pOPort) + (yo - yc);
				// Get port connections...
				QListIterator<qtractorPortListItem *> iter(pOPort->connects());
				while (iter.hasNext()) {
					qtractorPortListItem *pIPort = iter.next();
					// Obviously, should be a connection
					// from pOPort to pIPort items:
					y2 = itemY(pIPort) + (yi - yc);
					drawConnectionLine(&painter, x1, y1, x2, y2, h1, h2);
				}
			}
		}
	}
}


// Context menu request event handler.
void qtractorConnectorView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	if (m_pConnect)
		m_pConnect->contextMenu(pContextMenuEvent->globalPos());
}


// Widget event slots...
void qtractorConnectorView::contentsChanged (void)
{
	QWidget::update();
}


//----------------------------------------------------------------------------
// qtractorConnect -- Connections controller.
//

// Constructor.
qtractorConnect::qtractorConnect (
		qtractorClientListView *pOListView,
		qtractorClientListView *pIListView,
		qtractorConnectorView *pConnectorView )
{
	m_pOListView = pOListView;
	m_pIListView = pIListView;
	m_pConnectorView = pConnectorView;

	m_bBezierLines = false;

	m_pOListView->setBinding(this);
	m_pOListView->setReadable(true);
	m_pIListView->setBinding(this);
	m_pIListView->setReadable(false);
	m_pConnectorView->setBinding(this);

	QObject::connect(m_pOListView, SIGNAL(itemExpanded(QTreeWidgetItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pOListView, SIGNAL(itemCollapsed(QTreeWidgetItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pOListView->verticalScrollBar(), SIGNAL(valueChanged(int)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pOListView->header(), SIGNAL(sectionClicked(int)),
		m_pConnectorView, SLOT(contentsChanged()));

	QObject::connect(m_pIListView, SIGNAL(itemExpanded(QTreeWidgetItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pIListView, SIGNAL(itemCollapsed(QTreeWidgetItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pIListView->verticalScrollBar(), SIGNAL(valueChanged(int)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pIListView->header(), SIGNAL(sectionClicked(int)),
		m_pConnectorView, SLOT(contentsChanged()));
}


// Default destructor.
qtractorConnect::~qtractorConnect (void)
{
	// Force end of works here.
	m_pOListView->setBinding(NULL);
	m_pIListView->setBinding(NULL);
	m_pConnectorView->setBinding(NULL);
}


// Connector line style accessors.
void qtractorConnect::setBezierLines ( bool bBezierLines )
{
	m_bBezierLines = bBezierLines;
}

bool qtractorConnect::isBezierLines (void) const
{
	return m_bBezierLines;
}


// Connection primitive.
bool qtractorConnect::connectPortsEx (
	qtractorPortListItem *pOPort, qtractorPortListItem *pIPort )
{
	if (pOPort->findConnect(pIPort))
		return false;
	if (!connectPorts(pOPort, pIPort))
		return false;
	pOPort->addConnect(pIPort);
	pIPort->addConnect(pOPort);
	return true;
}


// Disconnection primitive.
bool qtractorConnect::disconnectPortsEx (
	qtractorPortListItem *pOPort, qtractorPortListItem *pIPort )
{
	if (pOPort->findConnect(pIPort) == NULL)
		return false;
	if (!disconnectPorts(pOPort, pIPort))
		return false;
	pOPort->removeConnect(pIPort);
	pIPort->removeConnect(pOPort);
	return true;
}


// Test if selected ports are connectable.
bool qtractorConnect::canConnectSelected (void)
{
	// Take this opportunity to highlight any current selections.
	m_pOListView->hiliteClientPorts();
	m_pIListView->hiliteClientPorts();

	// Now with our predicate work...
	QTreeWidgetItem *pOItem = m_pOListView->currentItem();
	if (pOItem == NULL)
		return false;

	QTreeWidgetItem *pIItem = m_pIListView->currentItem();
	if (pIItem == NULL)
		return false;

	if (pOItem->type() == qtractorConnect::ClientItem) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			const int iICount = pIClient->childCount();
			int iOItem  = 0;
			int iIItem  = 0;
			while (iIItem < iICount && iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				pIItem = pIClient->child(iIItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem &&
					pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					if (pOPort && pIPort && pOPort->findConnect(pIPort) == NULL)
						return true;
				}
				++iOItem;
				++iIItem;
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			int iOItem  = 0;
			while (iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					if (pOPort && pOPort->findConnect(pIPort) == NULL)
						return true;
				}
				++iOItem;
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iICount = pIClient->childCount();
			int iIItem  = 0;
			while (iIItem < iICount) {
				pIItem = pIClient->child(iIItem);
				if (pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					if (pIPort && pOPort->findConnect(pIPort) == NULL)
						return true;
				}
				++iIItem;
			}
		} else {
			// One-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort && pOPort->findConnect(pIPort) == NULL)
				return true;
		}
	}

	return false;
}


// Connect current selected ports.
bool qtractorConnect::connectSelected (void)
{
	bool bResult = connectSelectedEx();

	m_pConnectorView->update();
	if (bResult)
		emit connectChanged();

	return bResult;
}

bool qtractorConnect::connectSelectedEx (void)
{
	// Now with our predicate work...
	QTreeWidgetItem *pOItem = m_pOListView->currentItem();
	if (pOItem == NULL)
		return false;

	QTreeWidgetItem *pIItem = m_pIListView->currentItem();
	if (pIItem == NULL)
		return false;

	if (pOItem->type() == qtractorConnect::ClientItem) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			const int iICount = pIClient->childCount();
			int iOItem  = 0;
			int iIItem  = 0;
			while (iIItem < iICount && iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				pIItem = pIClient->child(iIItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem &&
					pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					connectPortsEx(pOPort, pIPort);
				}
				++iOItem;
				++iIItem;
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			int iOItem  = 0;
			while (iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					connectPortsEx(pOPort, pIPort);
				}
				++iOItem;
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iICount = pIClient->childCount();
			int iIItem  = 0;
			while (iIItem < iICount) {
				pIItem = pIClient->child(iIItem);
				if (pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					connectPortsEx(pOPort, pIPort);
				}
				++iIItem;
			}
		} else {
			// One-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			connectPortsEx(pOPort, pIPort);
		}
	}

	return true;
}


// Test if selected ports are disconnectable.
bool qtractorConnect::canDisconnectSelected (void)
{
	// Now with our predicate work...
	QTreeWidgetItem *pOItem = m_pOListView->currentItem();
	if (pOItem == NULL)
		return false;

	QTreeWidgetItem *pIItem = m_pIListView->currentItem();
	if (pIItem == NULL)
		return false;

	if (pOItem->type() == qtractorConnect::ClientItem) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			const int iICount = pIClient->childCount();
			int iOItem  = 0;
			int iIItem  = 0;
			while (iIItem < iICount && iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				pIItem = pIClient->child(iIItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem &&
					pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					if (pOPort && pIPort && pOPort->findConnect(pIPort))
						return true;
				}
				++iOItem;
				++iIItem;
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			int iOItem  = 0;
			while (iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					if (pOPort && pOPort->findConnect(pIPort))
						return true;
				}
				++iOItem;
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iICount = pIClient->childCount();
			int iIItem  = 0;
			while (iIItem < iICount) {
				pIItem = pIClient->child(iIItem);
				if (pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					if (pIPort && pOPort->findConnect(pIPort))
						return true;
				}
				++iIItem;
			}
		} else {
			// One-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort && pOPort->findConnect(pIPort))
				return true;
		}
	}

	return false;
}


// Disconnect current selected ports.
bool qtractorConnect::disconnectSelected (void)
{
	bool bResult = disconnectSelectedEx();

	m_pConnectorView->update();
	if (bResult)
		emit connectChanged();

	return bResult;
}

bool qtractorConnect::disconnectSelectedEx (void)
{
	// Now with our predicate work...
	QTreeWidgetItem *pOItem = m_pOListView->currentItem();
	if (pOItem == NULL)
		return false;

	QTreeWidgetItem *pIItem = m_pIListView->currentItem();
	if (pIItem == NULL)
		return false;

	if (pOItem->type() == qtractorConnect::ClientItem) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			const int iICount = pIClient->childCount();
			int iOItem  = 0;
			int iIItem  = 0;
			while (iIItem < iICount && iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				pIItem = pIClient->child(iIItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem &&
					pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					disconnectPortsEx(pOPort, pIPort);
				}
				++iOItem;
				++iIItem;
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == NULL)
				return false;
			const int iOCount = pOClient->childCount();
			int iOItem  = 0;
			while (iOItem < iOCount) {
				pOItem = pOClient->child(iOItem);
				if (pOItem && pOItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pOPort
						= static_cast<qtractorPortListItem *> (pOItem);
					disconnectPortsEx(pOPort, pIPort);
				}
				++iOItem;
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == NULL)
			return false;
		if (pIItem->type() == qtractorConnect::ClientItem) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == NULL)
				return false;
			const int iICount = pIClient->childCount();
			int iIItem  = 0;
			while (iIItem < iICount) {
				pIItem = pIClient->child(iIItem);
				if (pIItem && pIItem->type() == qtractorConnect::PortItem) {
					qtractorPortListItem *pIPort
						= static_cast<qtractorPortListItem *> (pIItem);
					disconnectPortsEx(pOPort, pIPort);
				}
				++iIItem;
			}
		} else {
			// One-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			disconnectPortsEx(pOPort, pIPort);
		}
	}

	return true;
}


// Test if any port is disconnectable.
bool qtractorConnect::canDisconnectAll (void)
{
	// For each output client item...
	const int iItemCount = m_pOListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = m_pOListView->topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pItem);
		if (pOClient == NULL)
			continue;
		// For each output port item...
		const int iChildCount = pOClient->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChild = pOClient->child(iChild);
			if (pChild->type() != qtractorConnect::PortItem)
				continue;
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pChild);
			if (pOPort && pOPort->connects().count() > 0)
				return true;
		}
	}
	return false;
}


// Disconnect all ports.
bool qtractorConnect::disconnectAll (void)
{
	bool bResult = disconnectAllEx();

	m_pConnectorView->update();
	if (bResult)
		emit connectChanged();

	return bResult;
}

bool qtractorConnect::disconnectAllEx (void)
{
	// For each output client item...
	const int iItemCount = m_pOListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = m_pOListView->topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pItem);
		if (pOClient == NULL)
			continue;
		// For each output port item...
		const int iChildCount = pOClient->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChild = pOClient->child(iChild);
			if (pChild->type() != qtractorConnect::PortItem)
				continue;
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pChild);
			if (pOPort) {
				QList<qtractorPortListItem *> connects
					= pOPort->connects();
				QListIterator<qtractorPortListItem *> iter(connects);
				while (iter.hasNext()) {
					qtractorPortListItem *pIPort = iter.next();
					disconnectPortsEx(pOPort, pIPort);
				}
			}
		}
	}

	return true;
}


// Complete/incremental contents rebuilder;
// check dirty status if incremental.
void qtractorConnect::updateContents ( bool bClear )
{
	int iDirtyCount = 0;

	// Do we do a complete rebuild?
	if (bClear) {
		m_pOListView->clear();
		m_pIListView->clear();
	}

	// Add (newer) client:ports and respective connections...
	if (m_pOListView->updateClientPorts() > 0) {
		m_pOListView->refresh();
		++iDirtyCount;
	}
	if (m_pIListView->updateClientPorts() > 0) {
		m_pIListView->refresh();
		++iDirtyCount;
	}
	updateConnections();

	m_pConnectorView->update();

	if (!bClear && iDirtyCount > 0)
		emit contentsChanged();
}


// Incremental contents rebuilder;
// check dirty status.
void qtractorConnect::refresh (void)
{
	updateContents(false);
}


// Context menu helper.
void qtractorConnect::contextMenu ( const QPoint& gpos )
{
	QMenu menu(m_pConnectorView);
	QAction *pAction;

	pAction = menu.addAction(
		QIcon(":/images/formConnect.png"),
		tr("Connect"), this, SLOT(connectSelected()));
	pAction->setEnabled(canConnectSelected());

	pAction = menu.addAction(
		QIcon(":/images/formDisconnect.png"),
		tr("Disconnect"), this, SLOT(disconnectSelected()));
	pAction->setEnabled(canDisconnectSelected());

	pAction = menu.addAction(
		QIcon(":/images/formDisconnectAll.png"),
		tr("Disconnect All"), this, SLOT(disconnectAll()));
	pAction->setEnabled(canDisconnectAll());

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/images/formRefresh.png"),
		tr("Refresh"), this, SLOT(refresh()));

	menu.exec(gpos);
}


// end of qtractorConnect.cpp
