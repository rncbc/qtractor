// qtractorAudioConnect.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioConnect.h"
#include "qtractorAudioEngine.h"

#include "qtractorSession.h"

#include <QIcon>


#ifdef CONFIG_JACK_METADATA

#include <jack/metadata.h>
#include <jack/uuid.h>

static QString prettyName (	jack_uuid_t uuid, const QString& sDefaultName )
{
	QString sPrettyName = sDefaultName;

	char *pszValue = nullptr;
	char *pszType  = nullptr;

	if (::jack_get_property(uuid,
			JACK_METADATA_PRETTY_NAME, &pszValue, &pszType) == 0) {
		if (pszValue) {
			sPrettyName = QString::fromUtf8(pszValue);
			::jack_free(pszValue);
		}
		if (pszType)
			::jack_free(pszType);
	}

	return sPrettyName;
}

#endif


//----------------------------------------------------------------------
// class qtractorAudioPortItem -- Jack port list item.
//

// Constructor.
qtractorAudioPortItem::qtractorAudioPortItem (
	qtractorAudioClientItem *pClientItem, unsigned long ulPortFlags )
	: qtractorPortListItem(pClientItem)
{
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


// Proto-pretty/alias display name method (virtual override).
void qtractorAudioPortItem::updatePortName (void)
{
#ifdef CONFIG_JACK_METADATA
	jack_client_t *pJackClient = nullptr;
	qtractorAudioClientListView *pAudioClientListView
		= static_cast<qtractorAudioClientListView *> (
			QTreeWidgetItem::treeWidget());
	if (pAudioClientListView)
		pJackClient = pAudioClientListView->jackClient();
	if (pJackClient) {
		const QString& sPortName = portName();
		const QString& sClientPort = clientName() + ':' + sPortName;
		const QByteArray aClientPort = sClientPort.toUtf8();
		jack_port_t *pJackPort
			= jack_port_by_name(pJackClient, aClientPort.constData());
		if (pJackPort) {
			jack_uuid_t port_uuid = ::jack_port_uuid(pJackPort);
			setPortText(prettyName(port_uuid, sPortName));
			return;
		}
	}
#endif
	qtractorPortListItem::updatePortName();
}


//----------------------------------------------------------------------
// qtractorAudioClientItem -- Jack client list item.
//

// Constructor.
qtractorAudioClientItem::qtractorAudioClientItem (
	qtractorAudioClientListView *pClientListView )
	: qtractorClientListItem(pClientListView)
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


// Proto-pretty/alias display name method (virtual override).
void qtractorAudioClientItem::updateClientName (void)
{
#ifdef CONFIG_JACK_METADATA
	jack_client_t *pJackClient = nullptr;
	qtractorAudioClientListView *pAudioClientListView
		= static_cast<qtractorAudioClientListView *> (
			QTreeWidgetItem::treeWidget());
	if (pAudioClientListView)
		pJackClient = pAudioClientListView->jackClient();
	if (pJackClient) {
		const QString& sClientName = clientName();
		const QByteArray aClientName = sClientName.toUtf8();
		const char *pszClientUuid
			= ::jack_get_uuid_for_client_name(pJackClient, aClientName.constData());
		if (pszClientUuid) {
			jack_uuid_t client_uuid = 0;
			::jack_uuid_parse(pszClientUuid, &client_uuid);
			setClientText(prettyName(client_uuid, sClientName));
			::jack_free((void *) pszClientUuid);
			return;
		}
	}
#endif
	qtractorClientListItem::updateClientName();
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
	return (pAudioConnect ? pAudioConnect->jackClient() : nullptr);
}


// Client:port refreshner.
int qtractorAudioClientListView::updateClientPorts (void)
{
	jack_client_t *pJackClient = jackClient();
	if (pJackClient == nullptr)
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
			qtractorAudioClientItem *pClientItem = nullptr;
			qtractorAudioPortItem   *pPortItem   = nullptr;
			const int iColon = sClientPort.indexOf(':');
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
						if (pClientItem == nullptr) {
							pClientItem = new qtractorAudioClientItem(this);
							pClientItem->setClientName(sClientName);
							++iDirtyCount;
						}
						if (pClientItem && pPortItem == nullptr) {
							jack_port_t *pJackPort = jack_port_by_name(
								pJackClient, ppszClientPorts[iClientPort]);
							if (pJackPort) {
								pPortItem = new qtractorAudioPortItem(
									pClientItem, jack_port_flags(pJackPort));
								pPortItem->setPortName(sPortName);
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
			= new QIcon(QIcon::fromTheme("itemAudioClientIn"));
		g_apIcons[ClientOut]
			= new QIcon(QIcon::fromTheme("itemAudioClientOut"));
		g_apIcons[PortIn]
			= new QIcon(QIcon::fromTheme("itemAudioPortIn"));
		g_apIcons[PortOut]
			= new QIcon(QIcon::fromTheme("itemAudioPortOut"));
		g_apIcons[PortPhysIn]
			= new QIcon(QIcon::fromTheme("itemAudioPortPhysIn"));
		g_apIcons[PortPhysOut]
			= new QIcon(QIcon::fromTheme("itemAudioPortPhysOut"));
	}
	
}

void qtractorAudioConnect::deleteIcons (void)
{
	if (--g_iIconsRefCount == 0) {
		for (int i = 0; i < IconCount; ++i) {
			if (g_apIcons[i])
				delete g_apIcons[i];
			g_apIcons[i] = nullptr;
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
	jack_client_t *pJackClient = nullptr;

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

	if (pOAudioPort == nullptr || pIAudioPort == nullptr)
		return false;

	jack_client_t *pJackClient = jackClient();
	if (pJackClient == nullptr)
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

	if (pOAudioPort == nullptr || pIAudioPort == nullptr)
		return false;

	jack_client_t *pJackClient = jackClient();
	if (pJackClient == nullptr)
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
	qtractorAudioEngine *pAudioEngine = nullptr;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
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
	if (pJackClient == nullptr)
		return;

	// For each client item...
	const int iItemCount = OListView()->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = OListView()->topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorAudioClientItem *pOClient
			= static_cast<qtractorAudioClientItem *> (pItem);
		if (pOClient == nullptr)
			continue;
		// For each port item
		const int iChildCount = pOClient->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChild = pOClient->child(iChild);
			if (pChild->type() != qtractorConnect::PortItem)
				continue;
			qtractorAudioPortItem *pOPort
				= static_cast<qtractorAudioPortItem *> (pChild);
			if (pOPort == nullptr)
				continue;
			// Are there already any connections?
			if (pOPort->connects().count() > 0)
				continue;
			// Get port connections...
			const QString& sClientPort
				= pOPort->clientName() + ':' + pOPort->portName();
			const QByteArray aClientPort = sClientPort.toUtf8();
			const jack_port_t *pJackPort
				= jack_port_by_name(pJackClient, aClientPort.constData());
			if (pJackPort) {
				const char **ppszClientPorts
					= jack_port_get_all_connections(pJackClient, pJackPort);
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
}


// end of qtractorAudioConnect.cpp
