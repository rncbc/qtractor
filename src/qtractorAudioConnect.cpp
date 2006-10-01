// qtractorAudioConnect.cpp
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

#include "qtractorAudioConnect.h"


//----------------------------------------------------------------------
// class qtractorAudioPortItem -- Jack port list item.
//

// Constructor.
qtractorAudioPortItem::qtractorAudioPortItem (
	qtractorAudioClientItem *pClientItem, const QString& sPortName,
	jack_port_t *pJackPort ) : qtractorPortListItem(pClientItem, sPortName)
{
	m_pJackPort = pJackPort;

	unsigned long ulPortFlags = jack_port_flags(m_pJackPort);
	if (ulPortFlags & JackPortIsInput) {
		QListViewItem::setPixmap(0,
			qtractorAudioConnect::pixmap(ulPortFlags & JackPortIsPhysical ?
				qtractorAudioConnect::PortPhysIn : qtractorAudioConnect::PortIn));
	} else {
		QListViewItem::setPixmap(0,
			qtractorAudioConnect::pixmap(ulPortFlags & JackPortIsPhysical ?
				qtractorAudioConnect::PortPhysOut : qtractorAudioConnect::PortOut));
	}
}

// Default destructor.
qtractorAudioPortItem::~qtractorAudioPortItem (void)
{
}


// Jack handles accessors.
jack_client_t *qtractorAudioPortItem::jackClient (void) const
{
	qtractorAudioClientItem *pClientItem
		= static_cast<qtractorAudioClientItem *> (clientItem());
	return (pClientItem ? pClientItem->jackClient() : 0);
}

jack_port_t *qtractorAudioPortItem::jackPort (void) const
{
	return m_pJackPort;
}


//----------------------------------------------------------------------
// qtractorAudioClientItem -- Jack client list item.
//

// Constructor.
qtractorAudioClientItem::qtractorAudioClientItem (
	qtractorAudioClientListView *pClientListView, const QString& sClientName )
	: qtractorClientListItem(pClientListView, sClientName)
{
	if (pClientListView->isReadable()) {
		QListViewItem::setPixmap(0,
			qtractorAudioConnect::pixmap(qtractorAudioConnect::ClientOut));
	} else {
		QListViewItem::setPixmap(0,
			qtractorAudioConnect::pixmap(qtractorAudioConnect::ClientIn));
	}
}

// Default destructor.
qtractorAudioClientItem::~qtractorAudioClientItem (void)
{
}


// Jack client accessor.
jack_client_t *qtractorAudioClientItem::jackClient (void) const
{
	qtractorAudioClientListView *pClientListView
		= static_cast<qtractorAudioClientListView *> (listView());
	return (pClientListView ? pClientListView->jackClient() : 0);
}


//----------------------------------------------------------------------
// qtractorAudioClientListView -- Jack client list view.
//

// Constructor.
qtractorAudioClientListView::qtractorAudioClientListView (
	QWidget *pParent, const char *pszName )
	: qtractorClientListView(pParent, pszName)
{
}

// Default destructor.
qtractorAudioClientListView::~qtractorAudioClientListView (void)
{
}


// Jack client accessor.
jack_client_t *qtractorAudioClientListView::jackClient (void) const
{
	qtractorAudioConnect *pAudioConnect
		= static_cast<qtractorAudioConnect *> (binding());
	return (pAudioConnect ? pAudioConnect->jackClient() : 0);
}


// Client:port refreshner.
int qtractorAudioClientListView::updateClientPorts (void)
{
	jack_client_t *pJackClient = jackClient();
	if (pJackClient == 0)
		return 0;

	int iDirtyCount = 0;
	
	markClientPorts(0);

	const char **ppszClientPorts = jack_get_ports(pJackClient, 0, 0,
		isReadable() ? JackPortIsOutput : JackPortIsInput);
	if (ppszClientPorts) {
		int iClientPort = 0;
		while (ppszClientPorts[iClientPort]) {
			QString sClientPort = ppszClientPorts[iClientPort];
			qtractorAudioClientItem *pClientItem = 0;
			qtractorAudioPortItem   *pPortItem   = 0;
			int iColon = sClientPort.find(":");
			if (iColon >= 0) {
				QString sClientName = sClientPort.left(iColon);
				if (isClientName(sClientName)) {
					QString sPortName
						= sClientPort.right(sClientPort.length() - iColon - 1);
					if (isPortName(sPortName)) {
						pClientItem
							= static_cast<qtractorAudioClientItem *> (
								findClientItem(sClientName));
						if (pClientItem) {
							pPortItem
								= static_cast<qtractorAudioPortItem *> (
									pClientItem->findPortItem(sPortName));
						}
						if (pClientItem == 0) {
							pClientItem = new qtractorAudioClientItem(this,
								sClientName);
							iDirtyCount++;
						}
						if (pClientItem && pPortItem == 0) {
							jack_port_t *pJackPort = jack_port_by_name(
								pJackClient, ppszClientPorts[iClientPort]);
							if (pJackPort) {
								pPortItem = new qtractorAudioPortItem(
									pClientItem, sPortName, pJackPort);
								iDirtyCount++;
							}
						}
						if (pPortItem)
							pPortItem->markClientPort(1);
					}
				}
			}
			iClientPort++;
		}
		::free(ppszClientPorts);
	}

	cleanClientPorts(0);

	return iDirtyCount;
}


//----------------------------------------------------------------------------
// qtractorAudioConnect -- Jack connections model integrated object.
//

// Constructor.
qtractorAudioConnect::qtractorAudioConnect (
	qtractorAudioClientListView *pOListView,
	qtractorAudioClientListView *pIListView,
	qtractorConnectorView *pConnectorView )
	: qtractorConnect(pOListView, pIListView, pConnectorView)
{
	m_pJackClient = 0;

	createIconPixmaps();
}

// Default destructor.
qtractorAudioConnect::~qtractorAudioConnect (void)
{
	deleteIconPixmaps();
}


// Local pixmap-set janitor methods.
QPixmap *qtractorAudioConnect::g_apPixmaps[qtractorAudioConnect::PixmapCount];
int      qtractorAudioConnect::g_iPixmapsRefCount = 0;

void qtractorAudioConnect::createIconPixmaps (void)
{
	if (++g_iPixmapsRefCount == 1) {
		g_apPixmaps[ClientIn]
			= new QPixmap(QPixmap::fromMimeSource("itemAudioClientIn.png"));
		g_apPixmaps[ClientOut]
			= new QPixmap(QPixmap::fromMimeSource("itemAudioClientOut.png"));
		g_apPixmaps[PortIn]
			= new QPixmap(QPixmap::fromMimeSource("itemAudioPortIn.png"));
		g_apPixmaps[PortOut]
			= new QPixmap(QPixmap::fromMimeSource("itemAudioPortOut.png"));
		g_apPixmaps[PortPhysIn]
			= new QPixmap(QPixmap::fromMimeSource("itemAudioPortPhysIn.png"));
		g_apPixmaps[PortPhysOut]
			= new QPixmap(QPixmap::fromMimeSource("itemAudioPortPhysOut.png"));
	}
	
}

void qtractorAudioConnect::deleteIconPixmaps (void)
{
	if (--g_iPixmapsRefCount == 0) {
		for (int i = 0; i < PixmapCount; i++) {
			if (g_apPixmaps[i])
				delete g_apPixmaps[i];
			g_apPixmaps[i] = 0;
		}
	}
}


// Common pixmap accessor (static).
const QPixmap& qtractorAudioConnect::pixmap ( int iPixmap )
{
	return *g_apPixmaps[iPixmap];
}


// Jack client accessors.
void qtractorAudioConnect::setJackClient( jack_client_t *pJackClient )
{
	m_pJackClient = pJackClient;

	updateContents(true);
}

jack_client_t *qtractorAudioConnect::jackClient (void) const
{
	return m_pJackClient;
}


// Connection primitive.
bool qtractorAudioConnect::connectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorAudioPortItem *pOAudioPort
		= static_cast<qtractorAudioPortItem *> (pOPort);
	qtractorAudioPortItem *pIAudioPort
		= static_cast<qtractorAudioPortItem *> (pIPort);

	if (pOAudioPort == 0 || pIAudioPort == 0)
		return false;

	return (jack_connect(pOAudioPort->jackClient(),
		pOAudioPort->clientPortName().latin1(),
		pIAudioPort->clientPortName().latin1()) == 0);
}


// Disconnection primitive.
bool qtractorAudioConnect::disconnectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorAudioPortItem *pOAudioPort
		= static_cast<qtractorAudioPortItem *> (pOPort);
	qtractorAudioPortItem *pIAudioPort
		= static_cast<qtractorAudioPortItem *> (pIPort);

	if (pOAudioPort == 0 || pIAudioPort == 0)
		return false;

	return (jack_disconnect(pOAudioPort->jackClient(),
		pOAudioPort->clientPortName().latin1(),
		pIAudioPort->clientPortName().latin1()) == 0);
}


// Update port connection references.
void qtractorAudioConnect::updateConnections (void)
{
	if (m_pJackClient == 0)
		return;

	// For each output client item...
	QListViewItem *pOClientItem = OListView()->firstChild();
	while (pOClientItem && pOClientItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		// For each output port item...
		QListViewItem *pOPortItem = pOClientItem->firstChild();
		while (pOPortItem && pOPortItem->rtti() == QTRACTOR_PORT_ITEM) {
			// Hava a proper type cast.
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pOPortItem);
			qtractorAudioPortItem *pOAudioPort
				= static_cast<qtractorAudioPortItem *> (pOPort);
			if (pOAudioPort == 0)
				continue;
			// Are there already any connections?
			if (pOAudioPort->connects().count() > 0)
				continue;
			// Get port connections...
			const char **ppszClientPorts = jack_port_get_all_connections(
				pOAudioPort->jackClient(), pOAudioPort->jackPort());
			if (ppszClientPorts) {
				// Now, for each input client port...
				int iClientPort = 0;
				while (ppszClientPorts[iClientPort]) {
					qtractorPortListItem *pIPort
						= IListView()->findClientPortItem(
							ppszClientPorts[iClientPort]);
					if (pIPort) {
						pOPort->addConnect(pIPort);
						pIPort->addConnect(pOPort);
					}
					iClientPort++;
				}
				::free(ppszClientPorts);
			}
			pOPortItem = pOPortItem->nextSibling();
		}
		pOClientItem = pOClientItem->nextSibling();
	}
}


// end of qtractorAudioConnect.cpp

