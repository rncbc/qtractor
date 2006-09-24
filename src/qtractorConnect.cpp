// qtractorConnect.cpp
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

#include "qtractorConnect.h"

#include <qpopupmenu.h>
#include <qheader.h>
#include <qtimer.h>


//----------------------------------------------------------------------
// qtractorConnectToolTip -- custom list view tooltips.
//

// Constructor.
qtractorConnectToolTip::qtractorConnectToolTip (
	qtractorClientListView *pListView ) : QToolTip(pListView->viewport())
{
	m_pListView = pListView;
}

// Tooltip handler.
void qtractorConnectToolTip::maybeTip ( const QPoint& pos )
{
	QListViewItem *pItem = m_pListView->itemAt(pos);
	if (pItem == 0)
		return;

	QRect rect(m_pListView->itemRect(pItem));
	if (!rect.isValid())
		return;

	if (pItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pClient = (qtractorClientListItem *) pItem;
		QToolTip::tip(rect, pClient->clientName());
	} else {
		qtractorPortListItem *pPort = (qtractorPortListItem *) pItem;
		QToolTip::tip(rect, pPort->portName());
	}
}


//----------------------------------------------------------------------
// qtractorPortListItem -- Port list item.
//

// Constructor.
qtractorPortListItem::qtractorPortListItem (
	qtractorClientListItem *pClientItem, const QString& sPortName )
	: QListViewItem(pClientItem, sPortName)
{
	m_pClientItem = pClientItem;
	m_sPortName   = sPortName;
	m_iPortMark   = 0;
	m_bHilite     = false;

	QListViewItem::setDragEnabled(true);
	QListViewItem::setDropEnabled(true);

	m_connects.setAutoDelete(false);
}

// Default destructor.
qtractorPortListItem::~qtractorPortListItem (void)
{
	m_connects.clear();
}


// Instance accessors.
void qtractorPortListItem::setPortName ( const QString& sPortName )
{
	QListViewItem::setText(0, sPortName);

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

int qtractorPortListItem::portMark (void)
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
    pPortItem->setHilite(false);
	m_connects.remove(pPortItem);
}


// Clear the connection list, taking care of hilighting...
void qtractorPortListItem::cleanConnects (void)
{
	for (qtractorPortListItem *pPortItem = m_connects.first();
			pPortItem; pPortItem = m_connects.next())
		pPortItem->removeConnect(this);
}


// Connected port finder.
qtractorPortListItem *qtractorPortListItem::findConnect (
	qtractorPortListItem *pPortItemPtr )
{
	for (qtractorPortListItem *pPortItem = m_connects.first();
			pPortItem; pPortItem = m_connects.next()) {
		if (pPortItemPtr == pPortItem)
			return pPortItem;
	}

	return 0;
}


// Connection cache list accessor.
QPtrList<qtractorPortListItem>& qtractorPortListItem::connects (void)
{
	return m_connects;
}


// To virtually distinguish between list view items.
int qtractorPortListItem::rtti (void) const
{
	return QTRACTOR_PORT_ITEM;
}


// Connectiopn highlight methods.
bool qtractorPortListItem::isHilite (void)
{
    return m_bHilite;
}

void qtractorPortListItem::setHilite ( bool bHilite )
{
    // Update the port highlightning if changed...
    if ((m_bHilite && !bHilite) || (!m_bHilite && bHilite)) {
        m_bHilite = bHilite;
        QListViewItem::repaint();
        // Propagate this to the parent...
        m_pClientItem->setHilite(bHilite);
    }
}


// To highlight current connected ports when complementary-selected.
void qtractorPortListItem::paintCell ( QPainter *pPainter,
	const QColorGroup& cg, int iColumn, int iWidth, int iAlign )
{
    QColorGroup cgCell(cg);
    if (m_bHilite)
        cgCell.setColor(QColorGroup::Text, Qt::blue);
    QListViewItem::paintCell(pPainter, cgCell, iColumn, iWidth, iAlign);
}


// Special port name sorting virtual comparator.
int qtractorPortListItem::compare ( QListViewItem* pPortItem,
	int iColumn, bool bAscending) const
{
	return qtractorClientListView::compare(text(iColumn),
		pPortItem->text(iColumn), bAscending);
}


//----------------------------------------------------------------------------
// qtractorClientListItem -- Client list item.
//

// Constructor.
qtractorClientListItem::qtractorClientListItem (
	qtractorClientListView *pClientListView, const QString& sClientName )
	: QListViewItem(pClientListView, sClientName)
{
	m_sClientName = sClientName;
	m_iClientMark = 0;
    m_iHilite     = 0;

	QListViewItem::setDragEnabled(true);
	QListViewItem::setDropEnabled(true);

//  QListViewItem::setSelectable(false);
}

// Default destructor.
qtractorClientListItem::~qtractorClientListItem (void)
{
}


// Port finder.
qtractorPortListItem *qtractorClientListItem::findPortItem (
	const QString& sPortName )
{
	QListViewItem *pListItem = QListViewItem::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_PORT_ITEM) {
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pListItem);
		if (pPortItem && pPortItem->portName() == sPortName)
			return pPortItem;
		pListItem = pListItem->nextSibling();
	}

	return 0;
}


// Instance accessors.
void qtractorClientListItem::setClientName ( const QString& sClientName )
{
	QListViewItem::setText(0, sClientName);

	m_sClientName = sClientName;
}

const QString& qtractorClientListItem::clientName (void) const
{
	return m_sClientName;
}


// Readable flag client accessor.
bool qtractorClientListItem::isReadable (void)
{
	qtractorClientListView *pClientListView
		= static_cast<qtractorClientListView *> (listView());
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

	QListViewItem *pListItem = QListViewItem::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_PORT_ITEM) {
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pListItem);
		if (pPortItem)
			pPortItem->markPort(iMark);
		pListItem = pListItem->nextSibling();
	}
}

void qtractorClientListItem::cleanClientPorts ( int iMark )
{
	QListViewItem *pListItem = QListViewItem::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_PORT_ITEM) {
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pListItem);
		pListItem = pListItem->nextSibling();
		if (pPortItem && pPortItem->portMark() == iMark) {
			pPortItem->cleanConnects();
			delete pPortItem;
		}
	}
}

int qtractorClientListItem::clientMark (void) const
{
	return m_iClientMark;
}


// To virtually distinguish between list view items.
int qtractorClientListItem::rtti (void) const
{
	return QTRACTOR_CLIENT_ITEM;
}


// Connectiopn highlight methods.
bool qtractorClientListItem::isHilite (void)
{
    return (m_iHilite > 0);
}

void qtractorClientListItem::setHilite ( bool bHilite )
{
    int iHilite = m_iHilite;
    if (bHilite)
        m_iHilite++;
    else
    if (m_iHilite > 0)
        m_iHilite--;
    // Update the client highlightning if changed...
    if (iHilite == 0 || m_iHilite == 0)
        QListViewItem::repaint();
}


// To highlight current connected clients when complementary-selected.
void qtractorClientListItem::paintCell ( QPainter *pPainter,
	const QColorGroup& cg, int iColumn, int iWidth, int iAlign )
{
    QColorGroup cgCell(cg);
    if (m_iHilite > 0)
        cgCell.setColor(QColorGroup::Text, Qt::darkBlue);
    QListViewItem::paintCell(pPainter, cgCell, iColumn, iWidth, iAlign);
}


// Special client name sorting virtual comparator.
int qtractorClientListItem::compare ( QListViewItem* pClientItem,
	int iColumn, bool bAscending ) const
{
	return qtractorClientListView::compare(text(iColumn),
		pClientItem->text(iColumn), bAscending);
}


//----------------------------------------------------------------------------
// qtractorClientListView -- Client list view, supporting drag-n-drop.
//

// Constructor.
qtractorClientListView::qtractorClientListView ( QWidget *pParent,
	const char *pszName ) : QListView(pParent, pszName)
{
	m_pConnect  = 0;
	m_bReadable = false;

	m_pAutoOpenTimer   = 0;
	m_iAutoOpenTimeout = 0;
	m_pDragDropItem    = 0;

    m_pHiliteItem = 0;

	m_pToolTip = new qtractorConnectToolTip(this);

	QListView::header()->setClickEnabled(false);
	QListView::header()->setResizeEnabled(false);
	QListView::setMinimumWidth(120);
	QListView::setAllColumnsShowFocus(true);
	QListView::setColumnWidthMode(0, QListView::Maximum);
	QListView::setRootIsDecorated(true);
	QListView::setResizeMode(QListView::AllColumns);
	QListView::setAcceptDrops(true);
	QListView::setDragAutoScroll(true);
	QListView::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	QListView::setShowToolTips(false);

	setAutoOpenTimeout(800);
}


// Default destructor.
qtractorClientListView::~qtractorClientListView (void)
{
	setAutoOpenTimeout(0);

	delete m_pToolTip;
}


// Client item finder.
qtractorClientListItem *qtractorClientListView::findClientItem (
	const QString& sClientName )
{
	QListViewItem *pListItem = QListView::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pClientItem
			= static_cast<qtractorClientListItem *> (pListItem);
		if (pClientItem && pClientItem->clientName() == sClientName)
			return pClientItem;
		pListItem = pListItem->nextSibling();
	}

	return 0;
}

// Client:port finder.
qtractorPortListItem *qtractorClientListView::findClientPortItem (
	const QString& sClientPort )
{
	qtractorPortListItem *pPortItem = 0;

	int iColon = sClientPort.find(':');
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
		sText = tr("Readable Clients") + " / " + tr("Output Ports");
	else
		sText = tr("Writable Clients") + " / " + tr("Input Ports");

	if (QListView::columns() > 0)
		QListView::setColumnText(0, sText);
	else
		QListView::addColumn(sText);
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
	m_pHiliteItem = 0;
	m_clientNames.clear();	

	QListView::clear();
}


// Client filter regular expression;
// take the chance to add to maintained client name list.
bool qtractorClientListView::isClientName ( const QString& sClientName )
{
	if (m_clientNames.find(sClientName) == m_clientNames.end())
		m_clientNames.append(sClientName);

	return (m_rxClientName.isEmpty()
		|| m_rxClientName.exactMatch(sClientName));
}


// Port filter regular expression;
bool qtractorClientListView::isPortName ( const QString& sPortName )
{
	return (m_rxClientName.isEmpty()
		|| m_rxPortName.isEmpty()
		|| m_rxPortName.exactMatch(sPortName));
}


// Whether items are all open (expanded) or closed (collapsed).
void qtractorClientListView::setOpenAll ( bool bOpen )
{
	QListViewItem *pListItem = QListView::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		QListViewItem *pItem = pListItem->firstChild();
		while (pItem && pItem->rtti() == QTRACTOR_PORT_ITEM) {
			qtractorPortListItem *pPortItem
				= static_cast<qtractorPortListItem *> (pItem);
			if (pPortItem) {
				pListItem->setOpen(bOpen);
				qtractorPortListItem *p = pPortItem->connects().first();
				while (p) {
					p->clientItem()->setOpen(bOpen);
					p = pPortItem->connects().next();
				}
			}
			pItem = pItem->nextSibling();
		}
		pListItem = pListItem->nextSibling();
	}
}


// Client:port set housekeeping marker.
void qtractorClientListView::markClientPorts ( int iMark )
{
	m_clientNames.clear();
	m_pHiliteItem = 0;

	QListViewItem *pListItem = QListView::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pClientItem
			= static_cast<qtractorClientListItem *> (pListItem);
		if (pClientItem)
			pClientItem->markClientPorts(iMark);
		pListItem = pListItem->nextSibling();
	}
}

void qtractorClientListView::cleanClientPorts ( int iMark )
{
	QListViewItem *pListItem = QListView::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pClientItem
			= static_cast<qtractorClientListItem *> (pListItem);
		pListItem = pListItem->nextSibling();
		if (pClientItem) {
			if (pClientItem->clientMark() == iMark) {
				delete pClientItem;
			} else {
				pClientItem->cleanClientPorts(iMark);
			}
		}
	}
}


// Client:port hilite update stabilization.
void qtractorClientListView::hiliteClientPorts (void)
{
    QListViewItem *pSelectedItem = QListView::selectedItem();

    // Dehilite the previous selected items.
    if (m_pHiliteItem && pSelectedItem != m_pHiliteItem) {
        if (m_pHiliteItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			QListViewItem *pListItem = m_pHiliteItem->firstChild();
			while (pListItem && pListItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pPortItem
					= static_cast<qtractorPortListItem *> (pListItem);
				if (pPortItem) {
					qtractorPortListItem *p = pPortItem->connects().first();
					while (p) {
						p->setHilite(false);
						p = pPortItem->connects().next();
					}
				}
				pListItem = pListItem->nextSibling();
            }
        } else {
            qtractorPortListItem *pPortItem
				= static_cast<qtractorPortListItem *> (m_pHiliteItem);
			if (pPortItem) {
				qtractorPortListItem *p = pPortItem->connects().first();
				while (p) {
					p->setHilite(false);
					p = pPortItem->connects().next();
				}
			}
        }
    }

    // Hilite the now current selected items.
    if (pSelectedItem && pSelectedItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		QListViewItem *pListItem = pSelectedItem->firstChild();
		while (pListItem && pListItem->rtti() == QTRACTOR_PORT_ITEM) {
			qtractorPortListItem *pPortItem
				= static_cast<qtractorPortListItem *> (pListItem);
			if (pPortItem) {
				qtractorPortListItem *p = pPortItem->connects().first();
				while (p) {
					p->setHilite(true);
					p = pPortItem->connects().next();
				}
			}
			pListItem = pListItem->nextSibling();
		}
	} else {
		qtractorPortListItem *pPortItem
			= static_cast<qtractorPortListItem *> (pSelectedItem);
		if (pPortItem) {
			qtractorPortListItem *p = pPortItem->connects().first();
			while (p) {
				p->setHilite(true);
				p = pPortItem->connects().next();
			}
		}
	}

    // Do remember this one, ever.
    m_pHiliteItem = pSelectedItem;
}


// Auto-open timeout method.
void qtractorClientListView::setAutoOpenTimeout ( int iAutoOpenTimeout )
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
int qtractorClientListView::autoOpenTimeout (void)
{
	return m_iAutoOpenTimeout;
}


// Auto-open timer slot.
void qtractorClientListView::timeoutSlot (void)
{
	if (m_pAutoOpenTimer) {
		m_pAutoOpenTimer->stop();
		if (m_pDragDropItem && !m_pDragDropItem->isOpen()) {
			m_pDragDropItem->setOpen(true);
			m_pDragDropItem->repaint();
		}
	}
}

// Drag-n-drop stuff.
QListViewItem *qtractorClientListView::dragDropItem ( const QPoint& pos )
{
	QPoint vpos(pos);
	int m = QListView::header()->sectionRect(0).height();
	vpos.setY(vpos.y() - m);
	QListViewItem *pItem = QListView::itemAt(vpos);
	if (pItem) {
		if (m_pDragDropItem != pItem) {
			QListView::setSelected(pItem, true);
			m_pDragDropItem = pItem;
			if (m_pAutoOpenTimer)
				m_pAutoOpenTimer->start(m_iAutoOpenTimeout);
			qtractorConnect *pConnect = binding();
			if (!pItem->dropEnabled() || pConnect == 0 || !pConnect->canConnectSelected())
				pItem = 0;
		}
	} else {
		m_pDragDropItem = 0;
		if (m_pAutoOpenTimer)
			m_pAutoOpenTimer->stop();
	}
	vpos = QListView::viewportToContents(vpos);
	QListView::ensureVisible(vpos.x(), vpos.y(), m, m);
	return pItem;
}

void qtractorClientListView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
	if (pDragEnterEvent->source() != this &&
		QTextDrag::canDecode(pDragEnterEvent) &&
		dragDropItem(pDragEnterEvent->pos())) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


void qtractorClientListView::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	QListViewItem *pItem = 0;
	if (pDragMoveEvent->source() != this)
		pItem = dragDropItem(pDragMoveEvent->pos());
	if (pItem) {
		pDragMoveEvent->accept(QListView::itemRect(pItem));
	} else {
		pDragMoveEvent->ignore();
	}
}


void qtractorClientListView::dragLeaveEvent ( QDragLeaveEvent * )
{
	m_pDragDropItem = 0;
	if (m_pAutoOpenTimer)
		m_pAutoOpenTimer->stop();
}


void qtractorClientListView::dropEvent( QDropEvent *pDropEvent )
{
	if (pDropEvent->source() != this) {
		QString sText;
		if (QTextDrag::decode(pDropEvent, sText)
			&& dragDropItem(pDropEvent->pos())) {
			if (m_pConnect)
				m_pConnect->connectSelected();
		}
	}

	dragLeaveEvent(0);
}


QDragObject *qtractorClientListView::dragObject (void)
{
	QTextDrag *pDragObject = 0;
	if (m_pConnect) {
		QListViewItem *pItem = QListView::currentItem();
		if (pItem && pItem->dragEnabled()) {
			pDragObject = new QTextDrag(pItem->text(0), this);
			const QPixmap *pPixmap = pItem->pixmap(0);
			if (pPixmap)
				pDragObject->setPixmap(*pPixmap, QPoint(-4, -12));
		}
	}

	return pDragObject;
}


// Context menu request event handler.
void qtractorClientListView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	if (m_pConnect)
		m_pConnect->contextMenu(pContextMenuEvent->globalPos());
}


// Natural decimal sorting comparator.
int qtractorClientListView::compare ( const QString& s1, const QString& s2,
 bool bAscending )
{
    int ich1, ich2;

    int cch1 = s1.length();
    int cch2 = s2.length();

    for (ich1 = ich2 = 0; ich1 < cch1 && ich2 < cch2; ich1++, ich2++) {

        // Skip (white)spaces...
        while (s1.at(ich1).isSpace())
            ich1++;
        while (s2.at(ich2).isSpace())
            ich2++;

		// Normalize (to uppercase) the next characters...
        QChar ch1 = s1.at(ich1).upper();
        QChar ch2 = s2.at(ich2).upper();

        if (ch1.isDigit() && ch2.isDigit()) {
            // Find the whole length numbers...
            int iDigits1 = ich1++;
            while (s1.at(ich1).isDigit())
                ich1++;
            int iDigits2 = ich2++;
            while (s2.at(ich2).isDigit())
                ich2++;
            // Compare as natural decimal-numbers...
            int iNumber1 = s1.mid(iDigits1, ich1 - iDigits1).toInt();
            int iNumber2 = s2.mid(iDigits2, ich2 - iDigits2).toInt();
            if (iNumber1 < iNumber2)
                return (bAscending ? -1 :  1);
            else if (iNumber1 > iNumber2)
                return (bAscending ?  1 : -1);
            // Go on with this next char...
            ch1 = s1.at(ich1).upper();
            ch2 = s2.at(ich2).upper();
        }

        // Compare this char...
        if (ch1 < ch2)
            return (bAscending ? -1 :  1);
        else if (ch1 > ch2)
            return (bAscending ?  1 : -1);
    }

    // Both strings seem to match, but longer is greater.
    if (cch1 < cch2)
        return (bAscending ? -1 :  1);
    else if (cch1 > cch2)
        return (bAscending ?  1 : -1);

    // Exact match.
    return 0;
}


//----------------------------------------------------------------------
// qtractorConnectorView -- Connector view widget.
//

// Constructor.
qtractorConnectorView::qtractorConnectorView ( QWidget *pParent,
	const char *pszName ) : QWidget(pParent, pszName)
{
	m_pConnect = 0;

	QWidget::setMinimumWidth(20);
//  QWidget::setMaximumWidth(120);
	QWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));
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
int qtractorConnectorView::itemY ( QListViewItem *pItem ) const
{
	QListViewItem *pParent = pItem->parent();
	if (pParent == 0 || pParent->isOpen()) {
		return (pItem->listView())->itemPos(pItem)
			+ pItem->height() / 2 - (pItem->listView())->contentsY();
	} else {
		return (pParent->listView())->itemPos(pParent)
			+ pParent->height() / 2 - (pParent->listView())->contentsY();
	}
}


// Draw visible port connection relation lines
void qtractorConnectorView::drawConnectionLine ( QPainter& p,
	int x1, int y1, int x2, int y2, int h1, int h2 )
{
	// Account for list view headers.
	y1 += h1;
	y2 += h2;

	// Invisible output ports don't get a connecting dot.
	if (y1 > h1)
		p.drawLine(x1, y1, x1 + 4, y1);

	// How do we'll draw it?
	if (m_pConnect->isBezierLines()) {
		// Setup control points
		QPointArray spline(4);
		int cp = (int)((double)(x2 - x1 - 8) * 0.4);
		spline.putPoints(0, 4,
			x1 + 4, y1, x1 + 4 + cp, y1, 
			x2 - 4 - cp, y2, x2 - 4, y2);
		// The connection line, it self.
		p.drawCubicBezier(spline);
	}
	else p.drawLine(x1 + 4, y1, x2 - 4, y2);

	// Invisible input ports don't get a connecting dot.
	if (y2 > h2)
		p.drawLine(x2 - 4, y2, x2, y2);
}


// Draw visible port connection relation arrows.
void qtractorConnectorView::paintEvent ( QPaintEvent * )
{
	if (m_pConnect == 0)
		return;
	if (m_pConnect->OListView() == 0 || m_pConnect->IListView() == 0)
		return;

	int yc = pos().y();
	int yo = m_pConnect->OListView()->pos().y();
	int yi = m_pConnect->IListView()->pos().y();

	QPainter p(this);
	int x1, y1, h1;
	int x2, y2, h2;
	int i, rgb[3] = { 0x33, 0x66, 0x99 };

	// Initialize color changer.
	i = 0;
	// Almost constants.
	x1 = 0;
	x2 = width();
	h1 = ((m_pConnect->OListView())->header())->sectionRect(0).height();
	h2 = ((m_pConnect->IListView())->header())->sectionRect(0).height();
	// For each output client item...
	QListViewItem *pOClientItem = m_pConnect->OListView()->firstChild();
	while (pOClientItem && pOClientItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		// Set new connector color.
		++i; p.setPen(QColor(rgb[i % 3], rgb[(i / 3) % 3], rgb[(i / 9) % 3]));
		// For each port item
		QListViewItem *pOPortItem = pOClientItem->firstChild();
		while (pOPortItem && pOPortItem->rtti() == QTRACTOR_PORT_ITEM) {
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pOPortItem);
			if (pOPort) {
				// Get starting connector arrow coordinates.
				y1 = itemY(pOPort) + (yo - yc); 
				// Get port connections...
				for (qtractorPortListItem *pIPort = pOPort->connects().first();
						pIPort; pIPort = pOPort->connects().next()) {
					// Obviously, should be a connection
					// from pOPort to pIPort items:
					y2 = itemY(pIPort) + (yi - yc);
					drawConnectionLine(p, x1, y1, x2, y2, h1, h2);
				}
			}
			pOPortItem = pOPortItem->nextSibling();
		}
		pOClientItem = pOClientItem->nextSibling();
	}
}


void qtractorConnectorView::resizeEvent ( QResizeEvent * )
{
	QWidget::repaint(true);
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


void qtractorConnectorView::contentsMoved ( int, int )
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
	m_pOListView     = pOListView;
	m_pIListView     = pIListView;
	m_pConnectorView = pConnectorView;

	m_bBezierLines = false;

	m_pOListView->setBinding(this);
	m_pOListView->setReadable(true);
	m_pIListView->setBinding(this);
	m_pIListView->setReadable(false);
	m_pConnectorView->setBinding(this);

	QObject::connect(m_pOListView, SIGNAL(expanded(QListViewItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pOListView, SIGNAL(collapsed(QListViewItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pOListView, SIGNAL(contentsMoving(int, int)),
		m_pConnectorView, SLOT(contentsMoved(int, int)));

	QObject::connect(m_pIListView, SIGNAL(expanded(QListViewItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pIListView, SIGNAL(collapsed(QListViewItem *)),
		m_pConnectorView, SLOT(contentsChanged()));
	QObject::connect(m_pIListView, SIGNAL(contentsMoving(int, int)),
		m_pConnectorView, SLOT(contentsMoved(int, int)));
}


// Default destructor.
qtractorConnect::~qtractorConnect (void)
{
	// Force end of works here.
	m_pOListView->setBinding(0);
	m_pIListView->setBinding(0);
	m_pConnectorView->setBinding(0);
}


// Connector line style accessors.
void qtractorConnect::setBezierLines ( bool bBezierLines )
{
	m_bBezierLines = bBezierLines;
}

bool qtractorConnect::isBezierLines (void)
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
	if (pOPort->findConnect(pIPort) == 0)
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
	QListViewItem *pOItem = m_pOListView->selectedItem();
	if (pOItem == 0)
		return false;

	QListViewItem *pIItem = m_pIListView->selectedItem();
	if (pIItem == 0)
		return false;

	if (pOItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pOItem = pOClient->firstChild();
			pIItem = pIClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM
				&& pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (pOPort && pIPort && pOPort->findConnect(pIPort) == 0)
					return true;
				pOItem = pOItem->nextSibling();
				pIItem = pIItem->nextSibling();
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == 0)
				return false;
			pOItem = pOClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				if (pOPort && pOPort->findConnect(pIPort) == 0)
					return true;
				pOItem = pOItem->nextSibling();
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pIItem = pIClient->firstChild();
			while (pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (pIPort && pOPort->findConnect(pIPort) == 0)
					return true;
				pIItem = pIItem->nextSibling();
			}
		} else {
			// One-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort && pOPort->findConnect(pIPort) == 0)
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
	QListViewItem *pOItem = m_pOListView->selectedItem();
	if (pOItem == 0)
		return false;

	QListViewItem *pIItem = m_pIListView->selectedItem();
	if (pIItem == 0)
		return false;

	if (pOItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pOItem = pOClient->firstChild();
			pIItem = pIClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM
				&& pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (!connectPortsEx(pOPort, pIPort))
					return false;
				pOItem = pOItem->nextSibling();
				pIItem = pIItem->nextSibling();
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == 0)
				return false;
			pOItem = pOClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				if (!connectPortsEx(pOPort, pIPort))
					return false;
				pOItem = pOItem->nextSibling();
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pIItem = pIClient->firstChild();
			while (pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (!connectPortsEx(pOPort, pIPort))
					return false;
				pIItem = pIItem->nextSibling();
			}
		} else {
			// One-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (!connectPortsEx(pOPort, pIPort))
				return false;
		}
	}

	return true;
}


// Test if selected ports are disconnectable.
bool qtractorConnect::canDisconnectSelected (void)
{
	// Now with our predicate work...
	QListViewItem *pOItem = m_pOListView->selectedItem();
	if (pOItem == 0)
		return false;

	QListViewItem *pIItem = m_pIListView->selectedItem();
	if (pIItem == 0)
		return false;

	if (pOItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pOItem = pOClient->firstChild();
			pIItem = pIClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM
				&& pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (pOPort && pIPort && pOPort->findConnect(pIPort))
					return true;
				pOItem = pOItem->nextSibling();
				pIItem = pIItem->nextSibling();
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == 0)
				return false;
			pOItem = pOClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				if (pOPort && pOPort->findConnect(pIPort))
					return true;
				pOItem = pOItem->nextSibling();
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pIItem = pIClient->firstChild();
			while (pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (pIPort && pOPort->findConnect(pIPort))
					return true;
				pIItem = pIItem->nextSibling();
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
	QListViewItem *pOItem = m_pOListView->selectedItem();
	if (pOItem == 0)
		return false;

	QListViewItem *pIItem = m_pIListView->selectedItem();
	if (pIItem == 0)
		return false;

	if (pOItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorClientListItem *pOClient
			= static_cast<qtractorClientListItem *> (pOItem);
		if (pOClient == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// Each-to-each connections...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pOItem = pOClient->firstChild();
			pIItem = pIClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM
				&& pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (!disconnectPortsEx(pOPort, pIPort))
					return false;
				pOItem = pOItem->nextSibling();
				pIItem = pIItem->nextSibling();
			}
		} else {
			// Many(all)-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (pIPort == 0)
				return false;
			pOItem = pOClient->firstChild();
			while (pOItem && pOItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pOPort
					= static_cast<qtractorPortListItem *> (pOItem);
				if (!disconnectPortsEx(pOPort, pIPort))
					return false;
				pOItem = pOItem->nextSibling();
			}
		}
	} else {
		qtractorPortListItem *pOPort
			= static_cast<qtractorPortListItem *> (pOItem);
		if (pOPort == 0)
			return false;
		if (pIItem->rtti() == QTRACTOR_CLIENT_ITEM) {
			// One-to-many(all) connection...
			qtractorClientListItem *pIClient
				= static_cast<qtractorClientListItem *> (pIItem);
			if (pIClient == 0)
				return false;
			pIItem = pIClient->firstChild();
			while (pIItem && pIItem->rtti() == QTRACTOR_PORT_ITEM) {
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIItem);
				if (!disconnectPortsEx(pOPort, pIPort))
					return false;
				pIItem = pIItem->nextSibling();
			}
		} else {
			// One-to-one connection...
			qtractorPortListItem *pIPort
				= static_cast<qtractorPortListItem *> (pIItem);
			if (!disconnectPortsEx(pOPort, pIPort))
				return false;
		}
	}

	return true;
}


// Test if any port is disconnectable.
bool qtractorConnect::canDisconnectAll (void)
{
	QListViewItem *pOClientItem = m_pOListView->firstChild();
	while (pOClientItem && pOClientItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		QListViewItem *pOPortItem = pOClientItem->firstChild();
		while (pOPortItem && pOPortItem->rtti() == QTRACTOR_PORT_ITEM) {
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pOPortItem);
			if (pOPort && pOPort->connects().count() > 0)
				return true;
			pOPortItem = pOPortItem->nextSibling();
		}
		pOClientItem = pOClientItem->nextSibling();
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
	QListViewItem *pOClientItem = m_pOListView->firstChild();
	while (pOClientItem && pOClientItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		QListViewItem *pOPortItem = pOClientItem->firstChild();
		while (pOPortItem && pOPortItem->rtti() == QTRACTOR_PORT_ITEM) {
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pOPortItem);
			if (pOPort) {
				qtractorPortListItem *pIPort;
				while ((pIPort = pOPort->connects().first()) != 0)
					disconnectPortsEx(pOPort, pIPort);
			}
			pOPortItem = pOPortItem->nextSibling();
		}
		pOClientItem = pOClientItem->nextSibling();
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
	iDirtyCount += m_pOListView->updateClientPorts();
	iDirtyCount += m_pIListView->updateClientPorts();
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
	int iItemID;
	QPopupMenu* pContextMenu = new QPopupMenu(m_pConnectorView);

	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formConnect.png")),
		tr("Connect"), this, SLOT(connectSelected()));
	pContextMenu->setItemEnabled(iItemID, canConnectSelected());
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formDisconnect.png")),
		tr("Disconnect"), this, SLOT(disconnectSelected()));
	pContextMenu->setItemEnabled(iItemID, canDisconnectSelected());
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formDisconnectAll.png")),
		tr("Disconnect All"), this, SLOT(disconnectAll()));
	pContextMenu->setItemEnabled(iItemID, canDisconnectAll());
	pContextMenu->insertSeparator();
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formRefresh.png")),
		tr("Refresh"), this, SLOT(refresh()));

	pContextMenu->exec(gpos);

	delete pContextMenu;
}


// end of qtractorConnect.cpp
