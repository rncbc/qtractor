// qtractorConnect.h
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

#ifndef __qtractorConnect_h
#define __qtractorConnect_h

#include <qdragobject.h>
#include <qlistview.h>
#include <qptrlist.h>
#include <qpainter.h>
#include <qtooltip.h>
#include <qregexp.h>

// QListViewItem::rtti return values.
#define QTRACTOR_CLIENT_ITEM	1001
#define QTRACTOR_PORT_ITEM		1002


// Forward declarations.
class qtractorPortListItem;
class qtractorClientListItem;
class qtractorClientListView;
class qtractorConnectorView;
class qtractorConnect;


//----------------------------------------------------------------------
// qtractorConnectToolTip -- custom list view tooltips.
//

class qtractorConnectToolTip : public QToolTip
{
public:

	// Constructor.
	qtractorConnectToolTip(qtractorClientListView *pListView);
	// Virtual destructor.
	virtual ~qtractorConnectToolTip() {}

protected:

	// Tooltip handler.
	void maybeTip(const QPoint& pos);

private:

	// The actual parent widget holder.
	qtractorClientListView *m_pListView;
};


//----------------------------------------------------------------------
// qtractorPortListItem -- Port list item.
//

class qtractorPortListItem : public QListViewItem
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

	int portMark();

	// Connected port list primitives.
	void addConnect(qtractorPortListItem *pPortItem);
	void removeConnect(qtractorPortListItem *pPortItem);
	void cleanConnects();

	// Connected port finders.
	qtractorPortListItem *findConnect(qtractorPortListItem *pPortItemPtr);
	// Connection list accessor.
	QPtrList<qtractorPortListItem>& connects();

	// To virtually distinguish between list view items.
	int rtti() const;

    // Connectiopn highlight methods.
    bool isHilite();
    void setHilite (bool bHilite);

    // Special port name sorting virtual comparator.
    int compare (QListViewItem* pPortItem, int iColumn, bool bAscending) const;

protected:

    // To highlight current connected ports when complementary-selected.
    void paintCell(QPainter *pPainter, const QColorGroup& cg,
		int iColumn, int iWidth, int iAlign);

private:

	// Instance variables.
	qtractorClientListItem *m_pClientItem;
	
	QString m_sPortName;
	int     m_iPortMark;
    bool    m_bHilite;

	// Connection cache list.
	QPtrList<qtractorPortListItem> m_connects;
};


//----------------------------------------------------------------------------
// qtractorClientListItem -- Client list item.
//

class qtractorClientListItem : public QListViewItem
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
	bool isReadable();

	// Client port cleanup marker.
	void markClient(int iMark);
	void markClientPorts(int iMark);
	void cleanClientPorts(int iMark);

	int clientMark() const;

	// To virtually distinguish between list view items.
	int rtti() const;

    // Connectiopn highlight methods.
    bool isHilite();
    void setHilite (bool bHilite);

    // Special port name sorting virtual comparator.
    int compare (QListViewItem* pPortItem, int iColumn, bool bAscending) const;

protected:

    // To highlight current connected clients when complementary-selected.
    void paintCell(QPainter *pPainter, const QColorGroup& cg,
		int iColumn, int iWidth, int iAlign);

private:

	// Instance variables.
	QString m_sClientName;
	int     m_iClientMark;
    int     m_iHilite;
};


//----------------------------------------------------------------------------
// qtractorClientListView -- Client list view, supporting drag-n-drop.
//

class qtractorClientListView : public QListView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorClientListView(QWidget *pParent = 0, const char *pszName = 0);
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

	// Maintained current client name list.
	const QStringList& clientNames() const;
	void clearClientNames();

	// Retrieve all cached connections from given client name.
	QStringList connects(const QString& sClientName);
	
	// Client ports cleanup marker.
	void markClientPorts(int iMark);
	void cleanClientPorts(int iMark);

	// Client:port refreshner (return newest item count).
	virtual int updateClientPorts() = 0;

    // Client:port hilite update stabilization.
    void hiliteClientPorts();

	// Auto-open timer methods.
	void setAutoOpenTimeout(int iAutoOpenTimeout);
	int autoOpenTimeout();

    // Natural decimal sorting comparator helper.
    static int compare (const QString& s1, const QString& s2, bool bAscending);

protected slots:

	// Auto-open timeout slot.
	void timeoutSlot();

protected:

	// Client filter function via regular expression.
	bool isClientName(const QString& sClientName);

	// Drag-n-drop stuff -- reimplemented virtual methods.
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *);
	void dropEvent(QDropEvent *pDropEvent);
	QDragObject *dragObject();
	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

private:

	// Drag-n-drop stuff.
	QListViewItem *dragDropItem(const QPoint& pos);

	// Local instance variables.
	qtractorConnect *m_pConnect;
	bool             m_bReadable;

	// Auto-open timer.
	int     m_iAutoOpenTimeout;
	QTimer *m_pAutoOpenTimer;
	// Item we'll eventually drop something.
	QListViewItem *m_pDragDropItem;

	// The current highlighted item.
    QListViewItem *m_pHiliteItem;

	// Client filter regular expression.
	QRegExp m_rxClientName;
	
	// Maintained list of client names.
	QStringList m_clientNames;

	// Listview item tooltip.
	qtractorConnectToolTip *m_pToolTip;
};


//----------------------------------------------------------------------
// qtractorConnectorView -- Connector view widget.
//

class qtractorConnectorView : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorConnectorView(QWidget *pParent = 0, const char *pszName = 0);
	// Default destructor.
	~qtractorConnectorView();

	// Main controller accessors.
	void setBinding(qtractorConnect *pConnect);
	qtractorConnect *binding() const;
	
public slots:

	// Useful slots (should this be protected?).
	void contentsChanged();
	void contentsMoved(int, int);

protected:

	// Specific event handlers.
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);
	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

private:

	// Legal client/port item position helper.
	int itemY(QListViewItem *pListItem) const;

	// Drawing methods.
	void drawConnectionLine(QPainter& p,
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

	// Widget accesors.
	qtractorClientListView *OListView()     { return m_pOListView; }
	qtractorClientListView *IListView()     { return m_pIListView; }
	qtractorConnectorView  *ConnectorView() { return m_pConnectorView; }

	// Connector line style accessors.
	void setBezierLines(bool bBezierLines);
	bool isBezierLines();

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
	void updateContents (bool bClear);

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
