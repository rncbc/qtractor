// qtractorConnect.h
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

#ifndef __qtractorConnect_h
#define __qtractorConnect_h

#include <QTreeWidget>

#include <QRegExp>
#include <QList>


// Forward declarations.
class qtractorPortListItem;
class qtractorClientListItem;
class qtractorClientListView;
class qtractorConnectorView;
class qtractorConnect;

class QPainter;
class QTimer;

class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class QDragMoveEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QContextMenuEvent;


//----------------------------------------------------------------------
// qtractorPortListItem -- Port list item.
//

class qtractorPortListItem : public QTreeWidgetItem
{
public:

	// Constructor.
	qtractorPortListItem(qtractorClientListItem *pClientItem,
		const QString& sPortName);
	// Default destructor.
	~qtractorPortListItem();

	// Instance accessors.
	void setPortName(const QString& sPortName);
	const QString& clientName() const;
	const QString& portName() const;

	// Complete client:port name helper.
	QString clientPortName();

	// Connections client item accessor.
	qtractorClientListItem *clientItem() const;

	// Client port cleanup marker.
	void markPort(int iMark);
	void markClientPort(int iMark);

	int portMark() const;

	// Connected port list primitives.
	void addConnect(qtractorPortListItem *pPortItem);
	void removeConnect(qtractorPortListItem *pPortItem);
	void cleanConnects();

	// Connected port finders.
	qtractorPortListItem *findConnect(qtractorPortListItem *pPortItem);
	// Connection list accessor.
	const QList<qtractorPortListItem *>& connects() const;

	// To virtually distinguish between list view items.
	int rtti() const;

	// Connectiopn highlight methods.
	void setHilite (bool bHilite);
	bool isHilite() const;

	// Proxy sort override method.
	// - Natural decimal sorting comparator.
	bool operator< (const QTreeWidgetItem& other) const;

private:

	// Instance variables.
	qtractorClientListItem *m_pClientItem;
	
	QString m_sPortName;
	int     m_iPortMark;
	bool    m_bHilite;

	// Connection cache list.
	QList<qtractorPortListItem *> m_connects;
};


//----------------------------------------------------------------------------
// qtractorClientListItem -- Client list item.
//

class qtractorClientListItem : public QTreeWidgetItem
{
public:

	// Constructor.
	qtractorClientListItem(qtractorClientListView *pClientListView,
		const QString& sClientName);
	// Default destructor.
	~qtractorClientListItem();

	// Port finder.
	qtractorPortListItem *findPortItem(const QString& sPortName);

	// Instance accessors.
	void setClientName(const QString& sClientName);
	const QString& clientName() const;

	// Readable flag accessor.
	bool isReadable() const;

	// Client port cleanup marker.
	void markClient(int iMark);
	void markClientPorts(int iMark);
	void cleanClientPorts(int iMark);

	int clientMark() const;

	// Connection highlight methods.
	void setHilite (bool bHilite);
	bool isHilite() const;

	// Client item openness status.
	void setOpen(bool bOpen);
	bool isOpen() const;

	// Proxy sort override method.
	// - Natural decimal sorting comparator.
	bool operator< (const QTreeWidgetItem& other) const;

private:

	// Instance variables.
	QString m_sClientName;
	int     m_iClientMark;
	int     m_iHilite;
};


//----------------------------------------------------------------------------
// qtractorClientListView -- Client list view, supporting drag-n-drop.
//

class qtractorClientListView : public QTreeWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorClientListView(QWidget *pParent = NULL);
	// Default destructor.
	virtual ~qtractorClientListView();

	// Client finder.
	qtractorClientListItem *findClientItem(const QString& sClientName);
	// Client:port finder.
	qtractorPortListItem *findClientPortItem(const QString& sClientPort);

	// Main controller accessors.
	void setBinding(qtractorConnect *pConnect);
	qtractorConnect *binding() const;

	// Readable flag accessors.
	void setReadable(bool bReadable);
	bool isReadable() const;

	// Client name filter helpers.
	void setClientName(const QString& sClientName);
	QString clientName() const;

	// Port name filter helpers.
	void setPortName(const QString& sPortName);
	QString portName() const;

	// Maintained current client name list.
	const QStringList& clientNames() const;

	// Override clear method.
	void clear();

	// Whether items are all open (expanded) or closed (collapsed).
	void setOpenAll(bool bOpen);

	// Client ports cleanup marker.
	void markClientPorts(int iMark);
	void cleanClientPorts(int iMark);

	// Client:port refreshner (return newest item count).
	virtual int updateClientPorts() = 0;

	// Client:port hilite update stabilization.
	void hiliteClientPorts();

	// Redirect this one as public.
	QTreeWidgetItem *itemFromIndex(const QModelIndex& index) const
		{ return QTreeWidget::itemFromIndex(index); }

	// Auto-open timer methods.
	void setAutoOpenTimeout(int iAutoOpenTimeout);
	int autoOpenTimeout() const;

	// Do proper contents refresh/update.
	void refresh();

	// Natural decimal sorting comparator.
	static bool lessThan(
		const QTreeWidgetItem& i1, const QTreeWidgetItem& i2, int col = 0);

protected slots:

	// Auto-open timeout slot.
	void timeoutSlot();

protected:

	// Client:port filter function via regular expression.
	bool isClientName(const QString& sClientName);
	bool isPortName(const QString& sPortName);

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// Drag-n-drop stuff.
	QTreeWidgetItem *dragDropItem(const QPoint& pos);

	// Drag-n-drop stuff -- reimplemented virtual methods.
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *);
	void dropEvent(QDropEvent *pDropEvent);

	// Handle mouse events for drag-and-drop stuff.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

private:

	// Local instance variables.
	qtractorConnect *m_pConnect;
	bool             m_bReadable;

	// Auto-open timer.
	int     m_iAutoOpenTimeout;
	QTimer *m_pAutoOpenTimer;
	// Item we'll eventually drag-and-dropping something.
	QTreeWidgetItem *m_pDragItem;
	QTreeWidgetItem *m_pDropItem;
	// The point from where drag started.
	QPoint m_posDrag;

	// The current highlighted item.
	QTreeWidgetItem *m_pHiliteItem;

	// Client:port regular expression filters.
	QRegExp m_rxClientName;
	QRegExp m_rxPortName;
	
	// Maintained list of client names.
	QStringList m_clientNames;
};


//----------------------------------------------------------------------
// qtractorConnectorView -- Connector view widget.
//

class qtractorConnectorView : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorConnectorView(QWidget *pParent = NULL);
	// Default destructor.
	~qtractorConnectorView();

	// Main controller accessors.
	void setBinding(qtractorConnect *pConnect);
	qtractorConnect *binding() const;
	
public slots:

	// Useful slots (should this be protected?).
	void contentsChanged();

protected:

	// Specific event handlers.
	void paintEvent(QPaintEvent *);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

private:

	// Legal client/port item position helper.
	int itemY(QTreeWidgetItem *pListItem) const;

	// Drawing methods.
	void drawConnectionLine(QPainter *pPainter,
		int x1, int y1, int x2, int y2, int h1, int h2);

	// Local instance variables.
	qtractorConnect *m_pConnect;
};


//----------------------------------------------------------------------------
// qtractorConnect -- Connections controller.
//

class qtractorConnect : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorConnect(
		qtractorClientListView *pOListView,
		qtractorClientListView *pIListView,
		qtractorConnectorView *pConnectorView);

	// Default destructor.
	virtual ~qtractorConnect();

	// QTreeWidgetItem types.
	enum { ClientItem = 1001, PortItem = 1002 };

	// Widget accesors.
	qtractorClientListView *OListView() const     { return m_pOListView; }
	qtractorClientListView *IListView() const     { return m_pIListView; }
	qtractorConnectorView  *ConnectorView() const { return m_pConnectorView; }

	// Connector line style accessors.
	void setBezierLines(bool bBezierLines);
	bool isBezierLines() const;

	// Explicit connection tests.
	bool canConnectSelected();
	bool canDisconnectSelected();
	bool canDisconnectAll();

public slots:

	// Explicit connection slots.
	bool connectSelected();
	bool disconnectSelected();
	bool disconnectAll();

	// Complete/incremental contents rebuilder;
	// check dirty status if incremental.
	void updateContents(bool bClear);

	// Incremental contents refreshner; check dirty status.
	void refresh();

	// Context menu helper.
	void contextMenu(const QPoint& gpos);

signals:

	// Contents change signal.
	void contentsChanged();
	// Connection change signal.
	void connectChanged();

protected:

	// Connect/Disconnection primitives.
	virtual bool connectPorts(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort) = 0;
	virtual bool disconnectPorts(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort) = 0;

	// Update port connection references.
	virtual void updateConnections() = 0;

private:

	// Connect/Disconnection local primitives.
	bool connectPortsEx(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort);
	bool disconnectPortsEx(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort);

	// Connection methods (unguarded).
	bool connectSelectedEx();
	bool disconnectSelectedEx();
	bool disconnectAllEx();

	// Controlled widgets.
	qtractorClientListView *m_pOListView;
	qtractorClientListView *m_pIListView;
	qtractorConnectorView  *m_pConnectorView;

	// How we'll draw connector lines.
	bool m_bBezierLines;
};


#endif  // __qtractorConnect_h

// end of qtractorConnect.h
