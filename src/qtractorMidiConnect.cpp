// qtractorMidiConnect.cpp
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

#include "qtractorMidiConnect.h"
#include "qtractorMidiEngine.h"

#include "qtractorSession.h"

#include <QIcon>


//----------------------------------------------------------------------
// qtractorMidiPortItem -- Alsa port list item.
//

// Constructor.
qtractorMidiPortItem::qtractorMidiPortItem (
	qtractorMidiClientItem *pClientItem, const QString& sPortName,
	int iAlsaPort ) : qtractorPortListItem(pClientItem, sPortName)
{
	m_iAlsaPort = iAlsaPort;

	if (pClientItem->isReadable()) {
		QTreeWidgetItem::setIcon(0,
			qtractorMidiConnect::icon(qtractorMidiConnect::PortOut));
	} else {
		QTreeWidgetItem::setIcon(0,
			qtractorMidiConnect::icon(qtractorMidiConnect::PortIn));
	}
}

// Default destructor.
qtractorMidiPortItem::~qtractorMidiPortItem (void)
{
}


// Alsa handles accessors.
int qtractorMidiPortItem::alsaClient (void) const
{
	qtractorMidiClientItem *pClientItem
		= static_cast<qtractorMidiClientItem *> (clientItem());
	return (pClientItem ? pClientItem->alsaClient() : 0);
}

int qtractorMidiPortItem::alsaPort (void) const
{
	return m_iAlsaPort;
}


//----------------------------------------------------------------------
// qtractorMidiClientItem -- Alsa client list item.
//

// Constructor.
qtractorMidiClientItem::qtractorMidiClientItem (
	qtractorMidiClientListView *pClientListView, const QString& sClientName,
	int iAlsaClient ) : qtractorClientListItem(pClientListView, sClientName)
{
	m_iAlsaClient = iAlsaClient;

	if (pClientListView->isReadable()) {
		QTreeWidgetItem::setIcon(0,
			qtractorMidiConnect::icon(qtractorMidiConnect::ClientOut));
	} else {
		QTreeWidgetItem::setIcon(0,
			qtractorMidiConnect::icon(qtractorMidiConnect::ClientIn));
	}
}

// Default destructor.
qtractorMidiClientItem::~qtractorMidiClientItem (void)
{
}


// Jack client accessor.
int qtractorMidiClientItem::alsaClient (void) const
{
	return m_iAlsaClient;
}


// Derived port finder.
qtractorMidiPortItem *qtractorMidiClientItem::findPortItem ( int iAlsaPort )
{
	int iChildCount = QTreeWidgetItem::childCount();
	for (int iChild = 0; iChild < iChildCount; ++iChild) {
		QTreeWidgetItem *pItem = QTreeWidgetItem::child(iChild);
		if (pItem->type() != qtractorConnect::PortItem)
			continue;
		qtractorMidiPortItem *pPortItem
			= static_cast<qtractorMidiPortItem *> (pItem);
		if (pPortItem && pPortItem->alsaPort() == iAlsaPort)
			return pPortItem;
	}

	return NULL;
}


//----------------------------------------------------------------------
// qtractorMidiClientListView -- Alsa client list view.
//

// Constructor.
qtractorMidiClientListView::qtractorMidiClientListView(
	QWidget *pParent ) : qtractorClientListView(pParent)
{
}

// Default destructor.
qtractorMidiClientListView::~qtractorMidiClientListView (void)
{
}


// Alsa sequencer accessor.
snd_seq_t *qtractorMidiClientListView::alsaSeq (void) const
{
	qtractorMidiConnect *pMidiConnect
		= static_cast<qtractorMidiConnect *> (binding());
	return (pMidiConnect ? pMidiConnect->alsaSeq() : NULL);
}


// Client finder by id.
qtractorMidiClientItem *qtractorMidiClientListView::findClientItem (
	int iAlsaClient )
{
	int iItemCount = QTreeWidget::topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = QTreeWidget::topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorMidiClientItem *pClientItem
			= static_cast<qtractorMidiClientItem *> (pItem);
		if (pClientItem && pClientItem->alsaClient() == iAlsaClient)
			return pClientItem;
	}

	return NULL;
}


// Client port finder by id.
qtractorMidiPortItem *qtractorMidiClientListView::findClientPortItem (
	int iAlsaClient, int iAlsaPort )
{
	qtractorMidiClientItem *pClientItem = findClientItem(iAlsaClient);
	if (pClientItem == NULL)
		return NULL;

	return pClientItem->findPortItem(iAlsaPort);
}


// Client:port refreshner.
int qtractorMidiClientListView::updateClientPorts (void)
{
	snd_seq_t *pAlsaSeq = alsaSeq();
	if (pAlsaSeq == NULL)
		return 0;

	int iDirtyCount = 0;

	markClientPorts(0);

	unsigned int uiAlsaFlags;
	if (isReadable())
		uiAlsaFlags = SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ;
	else
		uiAlsaFlags = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;

	snd_seq_client_info_t *pClientInfo;
	snd_seq_port_info_t   *pPortInfo;

	snd_seq_client_info_alloca(&pClientInfo);
	snd_seq_port_info_alloca(&pPortInfo);
	snd_seq_client_info_set_client(pClientInfo, -1);

	while (snd_seq_query_next_client(pAlsaSeq, pClientInfo) >= 0) {
		int iAlsaClient = snd_seq_client_info_get_client(pClientInfo);
		if (iAlsaClient > 0) {
			qtractorMidiClientItem *pClientItem = findClientItem(iAlsaClient);
			snd_seq_port_info_set_client(pPortInfo, iAlsaClient);
			snd_seq_port_info_set_port(pPortInfo, -1);
			while (snd_seq_query_next_port(pAlsaSeq, pPortInfo) >= 0) {
				unsigned int uiPortCapability
					= snd_seq_port_info_get_capability(pPortInfo);
				if (((uiPortCapability & uiAlsaFlags) == uiAlsaFlags) &&
					((uiPortCapability & SND_SEQ_PORT_CAP_NO_EXPORT) == 0)) {
					QString sClientName = QString::number(iAlsaClient);
					sClientName += ':';
					sClientName += QString::fromUtf8(
						snd_seq_client_info_get_name(pClientInfo));
					if (isClientName(sClientName)) {
						int iAlsaPort = snd_seq_port_info_get_port(pPortInfo);
						QString sPortName = QString::number(iAlsaPort);
						sPortName += ':';
						sPortName += QString::fromUtf8(
							snd_seq_port_info_get_name(pPortInfo));
						if (isPortName(sPortName)) {
							qtractorMidiPortItem *pPortItem = NULL;
							if (pClientItem == NULL) {
								pClientItem = new qtractorMidiClientItem(this,
									sClientName, iAlsaClient);
								++iDirtyCount;
							} else {
								pPortItem = pClientItem->findPortItem(iAlsaPort);
								if (sClientName != pClientItem->clientName()) {
									pClientItem->setClientName(sClientName);
									++iDirtyCount;
								}
							}
							if (pClientItem) {
								if (pPortItem == NULL) {
									pPortItem = new qtractorMidiPortItem(
										pClientItem, sPortName, iAlsaPort);
									++iDirtyCount;
								} else if (sPortName != pPortItem->portName()) {
									pPortItem->setPortName(sPortName);
									++iDirtyCount;
								}
							}
							if (pPortItem)
								pPortItem->markClientPort(1);
						}
					}
				}
			}
		}
	}

	cleanClientPorts(0);

	return iDirtyCount;
}


//----------------------------------------------------------------------------
// qtractorMidiConnect -- Alsa connections model integrated object.
//

// Constructor.
qtractorMidiConnect::qtractorMidiConnect (
	qtractorMidiClientListView *pOListView,
	qtractorMidiClientListView *pIListView,
	qtractorConnectorView *pConnectorView )
	: qtractorConnect(pOListView, pIListView, pConnectorView)
{
	createIcons();
}

// Default destructor.
qtractorMidiConnect::~qtractorMidiConnect (void)
{
	deleteIcons();
}


// Local icon-set janitor methods.
QIcon *qtractorMidiConnect::g_apIcons[qtractorMidiConnect::IconCount];
int    qtractorMidiConnect::g_iIconsRefCount = 0;

void qtractorMidiConnect::createIcons (void)
{
	if (++g_iIconsRefCount == 1) {
		g_apIcons[ClientIn]
			= new QIcon(":/images/itemMidiClientIn.png");
		g_apIcons[ClientOut]
			= new QIcon(":/images/itemMidiClientOut.png");
		g_apIcons[PortIn]
			= new QIcon(":/images/itemMidiPortIn.png");
		g_apIcons[PortOut]
			= new QIcon(":/images/itemMidiPortOut.png");
	}
}

void qtractorMidiConnect::deleteIcons (void)
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
const QIcon& qtractorMidiConnect::icon ( int iIcon )
{
	return *g_apIcons[iIcon];
}


// ALSA sequencer accessor.
snd_seq_t *qtractorMidiConnect::alsaSeq (void) const
{
	snd_seq_t *pAlsaSeq = NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession && pSession->midiEngine())
		pAlsaSeq = (pSession->midiEngine())->alsaSeq();

	return pAlsaSeq;
}


// Connection primitive.
bool qtractorMidiConnect::connectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorMidiPortItem *pOMidiPort
		= static_cast<qtractorMidiPortItem *> (pOPort);
	qtractorMidiPortItem *pIMidiPort
		= static_cast<qtractorMidiPortItem *> (pIPort);

	if (pOMidiPort == NULL || pIMidiPort == NULL)
		return false;

	snd_seq_t *pAlsaSeq = alsaSeq();
	if (pAlsaSeq == NULL)
		return false;

	disconnectPortsUpdate(pOPort, pIPort);

	snd_seq_port_subscribe_t *pAlsaSubs;
	snd_seq_addr_t seq_addr;

	snd_seq_port_subscribe_alloca(&pAlsaSubs);

	seq_addr.client = pOMidiPort->alsaClient();
	seq_addr.port   = pOMidiPort->alsaPort();
	snd_seq_port_subscribe_set_sender(pAlsaSubs, &seq_addr);

	seq_addr.client = pIMidiPort->alsaClient();
	seq_addr.port   = pIMidiPort->alsaPort();
	snd_seq_port_subscribe_set_dest(pAlsaSubs, &seq_addr);

	return (snd_seq_subscribe_port(pAlsaSeq, pAlsaSubs) >= 0);
}


// Disconnection primitive.
bool qtractorMidiConnect::disconnectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorMidiPortItem *pOMidiPort
		= static_cast<qtractorMidiPortItem *> (pOPort);
	qtractorMidiPortItem *pIMidiPort
		= static_cast<qtractorMidiPortItem *> (pIPort);

	if (pOMidiPort == NULL || pIMidiPort == NULL)
		return false;

	snd_seq_t *pAlsaSeq = alsaSeq();
	if (pAlsaSeq == NULL)
		return false;

	disconnectPortsUpdate(pOPort, pIPort);

	snd_seq_port_subscribe_t *pAlsaSubs;
	snd_seq_addr_t seq_addr;

	snd_seq_port_subscribe_alloca(&pAlsaSubs);

	seq_addr.client = pOMidiPort->alsaClient();
	seq_addr.port   = pOMidiPort->alsaPort();
	snd_seq_port_subscribe_set_sender(pAlsaSubs, &seq_addr);

	seq_addr.client = pIMidiPort->alsaClient();
	seq_addr.port   = pIMidiPort->alsaPort();
	snd_seq_port_subscribe_set_dest(pAlsaSubs, &seq_addr);

	return (snd_seq_unsubscribe_port(pAlsaSeq, pAlsaSubs) >= 0);
}


// Update (clear) MIDI-buses connect lists (non-virtual).
void qtractorMidiConnect::disconnectPortsUpdate (
	qtractorPortListItem *pOPort, qtractorPortListItem *pIPort )
{
	qtractorMidiEngine *pMidiEngine = NULL;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	QString sPortName;
	qtractorBus::BusMode busMode = qtractorBus::None;
	if (pOPort->clientName().section(':', 1, 1) == pMidiEngine->clientName()) {
		busMode = qtractorBus::Output;
		sPortName = pOPort->portName().section(':', 1, 1);
	}
	else 
	if (pIPort->clientName().section(':', 1, 1) == pMidiEngine->clientName()) {
		busMode = qtractorBus::Input;
		sPortName = pIPort->portName().section(':', 1, 1);
	}

	if (busMode == qtractorBus::None)
		return;

	for (qtractorBus *pBus = pMidiEngine->buses().first();
			pBus; pBus = pBus->next()) {
		if ((pBus->busMode() & busMode) == 0)
			continue;
		if (sPortName == pBus->busName()) {
			if (busMode & qtractorBus::Input) {
				pBus->inputs().clear();
			} else {
				pBus->outputs().clear();
				// Remember to resend all session/tracks control stuff...
				pMidiEngine->resetAllControllers(false); // Deferred++
			}
			break;
		}
	}
}


// Update port connection references.
void qtractorMidiConnect::updateConnections (void)
{
	snd_seq_t *pAlsaSeq = alsaSeq();
	if (pAlsaSeq == NULL)
		return;

	snd_seq_query_subscribe_t *pAlsaSubs;
	snd_seq_addr_t seq_addr;

	snd_seq_query_subscribe_alloca(&pAlsaSubs);

	// Proper type casts.
	qtractorMidiClientListView *pOListView
		= static_cast<qtractorMidiClientListView *> (OListView());
	qtractorMidiClientListView *pIListView
		= static_cast<qtractorMidiClientListView *> (IListView());

	// For each client item...
	int iItemCount = pOListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = pOListView->topLevelItem(iItem);
		if (pItem->type() != qtractorConnect::ClientItem)
			continue;
		qtractorMidiClientItem *pOClient
			= static_cast<qtractorMidiClientItem *> (pItem);
		if (pOClient == NULL)
			continue;
		// For each port item
		int iChildCount = pOClient->childCount();
		for (int iChild = 0; iChild < iChildCount; ++iChild) {
			QTreeWidgetItem *pChild = pOClient->child(iChild);
			if (pChild->type() != qtractorConnect::PortItem)
				continue;
			qtractorMidiPortItem *pOPort
				= static_cast<qtractorMidiPortItem *> (pChild);
			if (pOPort == NULL)
				continue;
			// Are there already any connections?
			if (pOPort->connects().count() > 0)
				continue;
			// Get port connections...
			snd_seq_query_subscribe_set_type(pAlsaSubs, SND_SEQ_QUERY_SUBS_READ);
			snd_seq_query_subscribe_set_index(pAlsaSubs, 0);
			seq_addr.client = pOPort->alsaClient();
			seq_addr.port   = pOPort->alsaPort();
			snd_seq_query_subscribe_set_root(pAlsaSubs, &seq_addr);
			while (snd_seq_query_port_subscribers(pAlsaSeq, pAlsaSubs) >= 0) {
				seq_addr = *snd_seq_query_subscribe_get_addr(pAlsaSubs);
				qtractorMidiPortItem *pIPort
					= pIListView->findClientPortItem(
						seq_addr.client, seq_addr.port);
				if (pIPort) {
					pOPort->addConnect(pIPort);
					pIPort->addConnect(pOPort);
				}
				snd_seq_query_subscribe_set_index(pAlsaSubs,
					snd_seq_query_subscribe_get_index(pAlsaSubs) + 1);
			}
		}
	}
}


// end of qtractorMidiConnect.cpp
