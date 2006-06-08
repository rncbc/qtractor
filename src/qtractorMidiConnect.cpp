// qtractorMidiConnect.cpp
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

#include "qtractorMidiConnect.h"


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
		QListViewItem::setPixmap(0,
			qtractorMidiConnect::pixmap(QTRACTOR_MIDI_PORT_OUT));
	} else {
		QListViewItem::setPixmap(0,
			qtractorMidiConnect::pixmap(QTRACTOR_MIDI_PORT_IN));
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
		QListViewItem::setPixmap(0,
			qtractorMidiConnect::pixmap(QTRACTOR_MIDI_CLIENT_OUT));
	} else {
		QListViewItem::setPixmap(0,
			qtractorMidiConnect::pixmap(QTRACTOR_MIDI_CLIENT_IN));
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
	QListViewItem *pListItem = QListViewItem::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_PORT_ITEM) {
		qtractorMidiPortItem *pPortItem
			= static_cast<qtractorMidiPortItem *> (pListItem);
		if (pPortItem && pPortItem->alsaPort() == iAlsaPort)
			return pPortItem;
		pListItem = pListItem->nextSibling();
	}

	return 0;
}


//----------------------------------------------------------------------
// qtractorMidiClientListView -- Alsa client list view.
//

// Constructor.
qtractorMidiClientListView::qtractorMidiClientListView(
	QWidget *pParent, const char *pszName )
	: qtractorClientListView(pParent, pszName)
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
	return (pMidiConnect ? pMidiConnect->alsaSeq() : 0);
}


// Client finder by id.
qtractorMidiClientItem *qtractorMidiClientListView::findClientItem (
	int iAlsaClient )
{
	QListViewItem *pListItem = QListView::firstChild();
	while (pListItem && pListItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		qtractorMidiClientItem *pClientItem
			= static_cast<qtractorMidiClientItem *> (pListItem);
		if (pClientItem && pClientItem->alsaClient() == iAlsaClient)
			return pClientItem;
		pListItem = pListItem->nextSibling();
	}

	return 0;
}


// Client port finder by id.
qtractorMidiPortItem *qtractorMidiClientListView::findClientPortItem (
	int iAlsaClient, int iAlsaPort )
{
	qtractorMidiClientItem *pClientItem = findClientItem(iAlsaClient);
	if (pClientItem == 0)
		return 0;

	return pClientItem->findPortItem(iAlsaPort);
}


// Client:port refreshner.
int qtractorMidiClientListView::updateClientPorts (void)
{
	snd_seq_t *pAlsaSeq = alsaSeq();
	if (pAlsaSeq == 0)
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
					sClientName += snd_seq_client_info_get_name(pClientInfo);
					if (isClientName(sClientName)) {
						qtractorMidiPortItem *pPortItem = 0;
						int iAlsaPort = snd_seq_port_info_get_port(pPortInfo);
						if (pClientItem == 0) {
							pClientItem = new qtractorMidiClientItem(this,
								sClientName, iAlsaClient);
							iDirtyCount++;
						} else {
							pPortItem = pClientItem->findPortItem(iAlsaPort);
							if (sClientName != pClientItem->clientName()) {
								pClientItem->setClientName(sClientName);
								iDirtyCount++;
							}
						}
						if (pClientItem) {
							QString sPortName = QString::number(iAlsaPort);
							sPortName += ':';
							sPortName += snd_seq_port_info_get_name(pPortInfo);
							if (pPortItem == 0) {
								pPortItem = new qtractorMidiPortItem(
									pClientItem, sPortName, iAlsaPort);
								iDirtyCount++;
							} else if (sPortName != pPortItem->portName()) {
								pPortItem->setPortName(sPortName);
								iDirtyCount++;
							}
						}
						if (pPortItem)
							pPortItem->markClientPort(1);
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
	m_pAlsaSeq = 0;

	createIconPixmaps();
}

// Default destructor.
qtractorMidiConnect::~qtractorMidiConnect (void)
{
	deleteIconPixmaps();
}


// Local pixmap-set janitor methods.
QPixmap *qtractorMidiConnect::g_apPixmaps[QTRACTOR_MIDI_PIXMAPS];
int      qtractorMidiConnect::g_iPixmapsRefCount = 0;

void qtractorMidiConnect::createIconPixmaps (void)
{
	if (++g_iPixmapsRefCount == 1) {
		g_apPixmaps[QTRACTOR_MIDI_CLIENT_IN]
			= new QPixmap(QPixmap::fromMimeSource("itemMidiClientIn.png"));
		g_apPixmaps[QTRACTOR_MIDI_CLIENT_OUT]
			= new QPixmap(QPixmap::fromMimeSource("itemMidiClientOut.png"));
		g_apPixmaps[QTRACTOR_MIDI_PORT_IN]
			= new QPixmap(QPixmap::fromMimeSource("itemMidiPortIn.png"));
		g_apPixmaps[QTRACTOR_MIDI_PORT_OUT]
			= new QPixmap(QPixmap::fromMimeSource("itemMidiPortOut.png"));
	}
}

void qtractorMidiConnect::deleteIconPixmaps (void)
{
	if (--g_iPixmapsRefCount == 0) {
		for (int i = 0; i < QTRACTOR_MIDI_PIXMAPS; i++) {
			if (g_apPixmaps[i])
				delete g_apPixmaps[i];
			g_apPixmaps[i] = 0;
		}
	}
}


// Common pixmap accessor (static).
const QPixmap& qtractorMidiConnect::pixmap ( int iPixmap )
{
    return *g_apPixmaps[iPixmap];
}


// Alsa sequencer accessors.
void qtractorMidiConnect::setAlsaSeq ( snd_seq_t *pAlsaSeq )
{
	m_pAlsaSeq = pAlsaSeq;
}

snd_seq_t *qtractorMidiConnect::alsaSeq (void) const
{
	return m_pAlsaSeq;
}


// Connection primitive.
bool qtractorMidiConnect::connectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorMidiPortItem *pOMidiPort
		= static_cast<qtractorMidiPortItem *> (pOPort);
	qtractorMidiPortItem *pIMidiPort
		= static_cast<qtractorMidiPortItem *> (pIPort);

	if (pOMidiPort == 0 || pIMidiPort == 0)
		return false;

    snd_seq_port_subscribe_t *pAlsaSubs;
    snd_seq_addr_t seq_addr;

    snd_seq_port_subscribe_alloca(&pAlsaSubs);

    seq_addr.client = pOMidiPort->alsaClient();
    seq_addr.port   = pOMidiPort->alsaPort();
    snd_seq_port_subscribe_set_sender(pAlsaSubs, &seq_addr);

    seq_addr.client = pIMidiPort->alsaClient();
    seq_addr.port   = pIMidiPort->alsaPort();
    snd_seq_port_subscribe_set_dest(pAlsaSubs, &seq_addr);

    return (snd_seq_subscribe_port(m_pAlsaSeq, pAlsaSubs) >= 0);
}


// Disconnection primitive.
bool qtractorMidiConnect::disconnectPorts ( qtractorPortListItem *pOPort,
	qtractorPortListItem *pIPort )
{
	qtractorMidiPortItem *pOMidiPort
		= static_cast<qtractorMidiPortItem *> (pOPort);
	qtractorMidiPortItem *pIMidiPort
		= static_cast<qtractorMidiPortItem *> (pIPort);

	if (pOMidiPort == 0 || pIMidiPort == 0)
		return false;

    snd_seq_port_subscribe_t *pAlsaSubs;
    snd_seq_addr_t seq_addr;

    snd_seq_port_subscribe_alloca(&pAlsaSubs);

    seq_addr.client = pOMidiPort->alsaClient();
    seq_addr.port   = pOMidiPort->alsaPort();
    snd_seq_port_subscribe_set_sender(pAlsaSubs, &seq_addr);

    seq_addr.client = pIMidiPort->alsaClient();
    seq_addr.port   = pIMidiPort->alsaPort();
    snd_seq_port_subscribe_set_dest(pAlsaSubs, &seq_addr);

    return (snd_seq_unsubscribe_port(m_pAlsaSeq, pAlsaSubs) >= 0);
}


// Update port connection references.
void qtractorMidiConnect::updateConnections (void)
{
	snd_seq_query_subscribe_t *pAlsaSubs;
	snd_seq_addr_t seq_addr;

	snd_seq_query_subscribe_alloca(&pAlsaSubs);

	// Proper type casts.
	qtractorMidiClientListView *pOListView
		= static_cast<qtractorMidiClientListView *> (OListView());
	qtractorMidiClientListView *pIListView
		= static_cast<qtractorMidiClientListView *> (IListView());

	// For each output client item...
	QListViewItem *pOClientItem = pOListView->firstChild();
	while (pOClientItem && pOClientItem->rtti() == QTRACTOR_CLIENT_ITEM) {
		// For each output port item...
		QListViewItem *pOPortItem = pOClientItem->firstChild();
		while (pOPortItem && pOPortItem->rtti() == QTRACTOR_PORT_ITEM) {
			// Hava a proper type cast.
			qtractorPortListItem *pOPort
				= static_cast<qtractorPortListItem *> (pOPortItem);
			qtractorMidiPortItem *pOMidiPort
				= static_cast<qtractorMidiPortItem *> (pOPort);
			if (pOMidiPort == 0)
				continue;
			// Are there already any connections?
			if (pOMidiPort->connects().count() > 0)
				continue;
			// Get port connections...
			snd_seq_query_subscribe_set_type(pAlsaSubs, SND_SEQ_QUERY_SUBS_READ);
			snd_seq_query_subscribe_set_index(pAlsaSubs, 0);
			seq_addr.client = pOMidiPort->alsaClient();
			seq_addr.port   = pOMidiPort->alsaPort();
			snd_seq_query_subscribe_set_root(pAlsaSubs, &seq_addr);
			while (snd_seq_query_port_subscribers(m_pAlsaSeq, pAlsaSubs) >= 0) {
				seq_addr = *snd_seq_query_subscribe_get_addr(pAlsaSubs);
				qtractorMidiPortItem *pIMidiPort
					= pIListView->findClientPortItem(
						seq_addr.client, seq_addr.port);
				qtractorPortListItem *pIPort
					= static_cast<qtractorPortListItem *> (pIMidiPort);
				if (pIPort) {
					pOPort->addConnect(pIPort);
					pIPort->addConnect(pOPort);
				}
				snd_seq_query_subscribe_set_index(pAlsaSubs,
					snd_seq_query_subscribe_get_index(pAlsaSubs) + 1);
			}
			pOPortItem = pOPortItem->nextSibling();
		}
		pOClientItem = pOClientItem->nextSibling();
	}
}


// end of qtractorMidiConnect.cpp
