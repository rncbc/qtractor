// qtractorAudioConnect.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioEngine.h"

#include "qtractorSession.h"

#include <QIcon>


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
		QTreeWidgetItem::setIcon(0,
			qtractorAudioConnect::icon(ulPortFlags & JackPortIsPhysical ?
				qtractorAudioConnect::PortPhysIn : qtractorAudioConnect::PortIn));
	} else {
		QTreeWidgetItem::setIcon(0,
			qtractorAudioConnect::icon(ulPortFlags & JackPortIsPhysical ?
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
	return (pClientItem ? pClientItem->jackClient() : NULL);
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
		QTreeWidgetItem::setIcon(0,
			qtractorAudioConnect::icon(qtractorAudioConnect::ClientOut));
	} else {
		QTreeWidgetItem::setIcon(0,
			qtractorAudioConnect::icon(qtractorAudioConnect::ClientIn));
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
		= static_cast<qtractorAudioClientListView *> (QTreeWidgetItem::treeWidget());
	return (pClientListView ? pClientListView->jackClient() : NULL);
}


//----------------------------------------------------------------------
// qtractorAudioClientListView -- Jack client list view.
//

// Constructor.
qtractorAudioClientListView::qtractorAudioClientListView (
	QWidget *pParent ) : qtractorClientListView(pParent)
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
	return (pAudioConnect ? pAudioConnect->jackClient() : NULL);
}


// Client:port refreshner.
int qtractorAudioClientListView::updateClientPorts (void)
{
	jack_client_t *pJackClient = jackClient();
	if (pJackClient == NULL)
		return 0;

	int iDirtyCount = 0;
	
	markClientPorts(0);

	const char **ppszClientPorts = jack_get_ports(pJackClient,
		0, JACK_DEFAULT_AUDIO_TYPE,
		isReadable() ? JackPortIsOutput : JackPortIsInput);
	if (ppszClientPorts) {
		int iClientPort = 0;
		while (ppszClientPorts[iClientPort]) {
			const QString sClientPort
				= QString::fromUtf8(ppszClientPorts[iClientPort]);
			qtractorAudioClientItem *pClientItem = NULL;
			qtractorAudioPortItem   *pPortItem   = NULL;
			int iColon = sClientPort.indexOf(':');
			if (iColon >= 0) {
				const QString sClientName = sClientPort.left(iColon);
				if (isClientName(sClientName)) {
					const QString sPortName
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
						if (pClientItem == NULL) {
							pClientItem = new qtractorAudioClientItem(this,
								sClientName);
							++iDirtyCount;
						}
						if (pClientItem && pPortItem == NULL) {
							jack_port_t *pJackPort = jack_port_by_name(
								pJackClient, ppszClientPorts[iClientPort]);
							if (pJackPort) {
								pPortItem = new qtractorAudioPortItem(
									pClientItem, sPortName, pJackPort);
								++iDirtyCount;
							}
						}
						if (pPortItem)
							pPortItem->markClientPort(1);
					}
				}
			}
			++iClientPort;
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
	createIcons();
}

// Default destructor.
qtractorAudioConnect::~qtractorAudioConnect (void)
{
	deleteIcons();
}


// Local icon-set janitor methods.
QIcon *qtractorAudioConnect::g_apIcons[qtractorAudioConnect::IconCount];
int    qtractorAudioConnect::g_iIconsRefCount = 0;

void qtractorAudioConnect::createIcons (void)
{
	if (++g_iIconsRefCount == 1) {
		g_apIcons[ClientIn]
			= new QIcon(":/images/itemAudioClientIn.png");
		g_apIcons[ClientOut]
			= new QIcon(":/images/itemAudioClientOut.png");
		g_apIcons[PortIn]
			= new QIcon(":/images/itemAudioPortIn.png");
		g_apIcons[PortOut]
			= new QIcon(":/images/itemAudioPortOut.png");
		g_apIcons[PortPhysIn]
			= new QIcon(":/images/itemAudioPortPhysIn.png");
		g_apIcons[PortPhysOut]
			= new QIcon(":/images/itemAudioPortPhysOut.png");
	}
	
}

void qtractorAudioConnect::deleteIcons (void)
{
	if (--g_iIconsRefCount == 0) {
		for (int i = 0; i < IconCount; ++i) {
			if (g_apIcons[i])
				delete g_apIcons[i];
			g_apIcons[i] = NULL;
		}
	}
}


// Common icon accessor (static).
const QIcon& qtractorAudioConnect::icon ( int iIcon )
{
	return *g_apIcons[iIcon];
}


// JACK client accessor.
jack_client_t *qtractorAudioConnect::jackClient (void) const
{
	jack_client_t *pJackClient = NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession && pSession->audioEngine())
		pJackClient = (pSession->audioEngine())->jackClient();

	return pJackClient;
}


// Connection primitive.
bool qtractorAudioConnect::connectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorAudioPortItem *pOAudioPort
		= static_cast<qtractorAudioPortItem *> (pOPort);
	qtractorAudioPortItem *pIAudioPort
		= static_cast<qtractorAudioPortItem *> (pIPort);

	if (pOAudioPort == NULL || pIAudioPort == NULL)
		return false;

	jack_client_t *pJackClient = jackClient();
	if (pJackClient == NULL)
		return false;

	return (jack_connect(pJackClient,
		pOAudioPort->clientPortName().toUtf8().constData(),
		pIAudioPort->clientPortName().toUtf8().constData()) == 0);
}


// Disconnection primitive.
bool qtractorAudioConnect::disconnectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorAudioPortItem *pOAudioPort
		= static_cast<qtractorAudioPortItem *> (pOPort);
	qtractorAudioPortItem *pIAudioPort
		= static_cast<qtractorAudioPortItem *> (pIPort);

	if (pOAudioPort == NULL || pIAudioPort == NULL)
		return false;

	jack_client_t *pJackClient = jackClient();
	if (pJackClient == NULL)
		return false;

	disconnectPortsUpdate(pOPort, pIPort);

	return (jack_disconnect(pJackClient,
		pOAudioPort->clientPortName().toUtf8().constData(),
		pIAudioPort->clientPortName().toUtf8().constData()) == 0);
}


// Update (clear) audio-buses connect lists (non-virtual).
void qtractorAudioConnect::disconnectPortsUpdate (
	qtractorPortListItem *pOPort, qtractorPortListItem *pIPort )
{
	qtractorAudioEngine *pAudioEngine = NULL;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	QString sPortName;
	qtractorBus::BusMode busMode = qtractorBus::None;

	if (pOPort->clientName() == pAudioEngine->clientName()) {
		busMode = qtractorBus::Output;
		sPortName = pOPort->portName().section('/', 0, 0);
	}
	else 
	if (pIPort->clientName() == pAudioEngine->clientName()) {
		busMode = qtractorBus::Input;
		sPortName = pIPort->portName().section('/', 0, 0);
	}

	if (busMode == qtractorBus::None)
		return;

	for (qtractorBus *pBus = pAudioEngine->buses().first();
			pBus; pBus = pBus->next()) {
		if ((pBus->busMode() & busMode) == 0)
			continue;
		if (sPortName == pBus->busName()) {
			if (busMode & qtractorBus::Input) {
				pBus->inputs().clear();
			} else {
				pBus->outputs().clear();
			}
			break;
		}
	}
}


// Update port connection references.
void qtractorAudioConnect::updateConnections (void)
{
	jack_client_t *pJackClient = jackClient();
	if (pJackClient == NULL)
		return;

	// For each client item...
	int iItemCount = OListView()->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = OListView()->topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorAudioClientItem *pOClient
			= static_cast<qtractorAudioClientItem *> (pItem);
		if (pOClient == NULL)
			continue;
		// For each port item
		int iChildCount = pOClient->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChild = pOClient->child(iChild);
			if (pChild->type() != qtractorConnect::PortItem)
				continue;
			qtractorAudioPortItem *pOPort
				= static_cast<qtractorAudioPortItem *> (pChild);
			if (pOPort == NULL)
				continue;
			// Are there already any connections?
			if (pOPort->connects().count() > 0)
				continue;
			// Get port connections...
			const char **ppszClientPorts = jack_port_get_all_connections(
				pJackClient, pOPort->jackPort());
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
					++iClientPort;
				}
				::free(ppszClientPorts);
			}
		}
	}
}


// end of qtractorAudioConnect.cpp

