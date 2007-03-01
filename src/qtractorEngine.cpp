// qtractorEngine.cpp
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

#include "qtractorEngine.h"

#include "qtractorSessionCursor.h"
#include "qtractorSessionDocument.h"


//----------------------------------------------------------------------
// class qtractorEngine -- Abstract device engine instance (singleton).
//

// Constructor.
qtractorEngine::qtractorEngine ( qtractorSession *pSession,
	qtractorTrack::TrackType syncType )
{
	m_pSession       = pSession;
	m_pSessionCursor = m_pSession->createSessionCursor(0, syncType);
	m_bActivated     = false;
	m_bPlaying       = false;

	m_buses.setAutoDelete(true);
}

// Destructor.
qtractorEngine::~qtractorEngine (void)
{
	clear();

	if (m_pSessionCursor)
		delete m_pSessionCursor;
}


// Buses list clear.
void qtractorEngine::clear (void)
{
	m_buses.clear();
}


// Session accessor.
qtractorSession *qtractorEngine::session (void) const
{
	return m_pSession;
}


// Session cursor accessor.
qtractorSessionCursor *qtractorEngine::sessionCursor (void) const
{
	return m_pSessionCursor;
}


// Engine type accessor.
qtractorTrack::TrackType qtractorEngine::syncType (void) const
{
	return m_pSessionCursor->syncType();
}


// Client name accessor.
const QString& qtractorEngine::clientName (void) const
{
	return m_sClientName;
}


// Activation status accessor.
bool qtractorEngine::isActivated(void) const
{
	return m_bActivated;
}


// Buses list managament methods.
const qtractorList<qtractorBus>& qtractorEngine::buses (void) const
{
	return m_buses;
}


// Add a bus to a device engine.
void qtractorEngine::addBus ( qtractorBus *pBus )
{
	m_buses.append(pBus);
}


// Remove a bus from a device.
void qtractorEngine::removeBus ( qtractorBus *pBus )
{
	m_buses.remove(pBus);
}


// Find a device bus by name
qtractorBus *qtractorEngine::findBus ( const QString& sBusName )
{
	for (qtractorBus *pBus = m_buses.first();
			pBus; pBus = pBus->next()) {
		if (pBus->busName() == sBusName)
			return pBus;
	}

	return NULL;
}


// Device engine activation method.
bool qtractorEngine::open ( const QString& sClientName )
{
//	close();

	// Damn it.
	if (m_pSession == NULL)
		return false;

	// Call derived initialization...
	if (!init(sClientName))
		return false;

	// Set actual client name...
	m_sClientName = sClientName;

	// Update the session cursor tracks...
	m_pSessionCursor->reset();

	// Open all buses (allocated and register ports...)
	qtractorBus *pBus = m_buses.first();
	while (pBus) {
		if (!pBus->open()) {
			close();
			return false;
		}
		pBus = pBus->next();
	}

	// Nows time to activate...
	if (!activate()) {
		close();
		return false;
	}

	// We're now ready and running...
	m_bActivated = true;

	return true;
}


// Device engine deactivation method.
void qtractorEngine::close (void)
{
	// Save current activation state...
	bool bActivated = m_bActivated;
	// We're stopping now...
	m_bActivated = false;

	if (bActivated) {
		// Deactivate the derived engine first.
		deactivate();
		// Close all dependant buses...
		for (qtractorBus *pBus = m_buses.first();
				pBus; pBus = pBus->next()) {
			pBus->close();
		}
	}

	// Clean-up everything, finally
	clean();
}


// Engine state methods.
void qtractorEngine::setPlaying ( bool bPlaying )
{
	m_pSessionCursor->setFrameTime(0);

	if (bPlaying && !m_bPlaying) {
		m_bPlaying = start();
	}
	else
	if (!bPlaying && m_bPlaying) {
		m_bPlaying = false;
		stop();
	}
}

bool qtractorEngine::isPlaying(void) const
{
	return m_bPlaying;
}


// Retrieve/restore all connections, on all buses.
// return the total number of effective (re)connection attempts...
int qtractorEngine::updateConnects (void)
{
	// It mus be activated, sure...
	if (!isActivated())
		return 0;

	// On all dependable buses...
	int iUpdate = 0;
	for (qtractorBus *pBus = m_buses.first(); pBus; pBus = pBus->next()) {
		// Input connections...
		if (pBus->busMode() & qtractorBus::Input) {
			iUpdate += pBus->updateConnects(
				qtractorBus::Input, pBus->inputs(), true);
		}
		// Output connections...
		if (pBus->busMode() & qtractorBus::Output) {
			iUpdate += pBus->updateConnects(
				qtractorBus::Output, pBus->outputs(), true);
		}
	}

	// Done.
	return iUpdate;
}


//----------------------------------------------------------------------
// class qtractorBus -- Managed ALSA sequencer port set
//

// Constructor.
qtractorBus::qtractorBus ( qtractorEngine *pEngine,
	const QString& sBusName, BusMode busMode )
{
	m_pEngine   = pEngine;
	m_sBusName  = sBusName;
	m_busMode   = busMode;
}

// Destructor.
qtractorBus::~qtractorBus (void)
{
}


// Engine accessor.
qtractorEngine *qtractorBus::engine (void) const
{
	return m_pEngine;
}


// Bus type accessor.
qtractorTrack::TrackType qtractorBus::busType (void) const
{
	return (m_pEngine ? m_pEngine->syncType() : qtractorTrack::None);
}


// Bus name accessors.
void qtractorBus::setBusName ( const QString& sBusName )
{
	m_sBusName = sBusName;
}

const QString& qtractorBus::busName (void) const
{
	return m_sBusName;
}


// Bus mode property accessor.
void qtractorBus::setBusMode ( qtractorBus::BusMode busMode )
{
	m_busMode = busMode;

	updateBusMode();
}

qtractorBus::BusMode qtractorBus::busMode (void) const
{
	return m_busMode;
}


// Document element methods.
bool qtractorBus::loadConnects ( ConnectList& connects,
	qtractorSessionDocument * /*pDocument*/, QDomElement *pElement )
{
	connects.clear();

	// Load map items...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		// Load (other) track properties..
		if (eChild.tagName() == "connect") {
			ConnectItem *pItem = new ConnectItem;
			pItem->index = eChild.attribute("index").toUShort();
			for (QDomNode nConnect = eChild.firstChild();
					!nConnect.isNull();
						nConnect = nConnect.nextSibling()) {
				// Convert connect node to element...
				QDomElement eConnect = nConnect.toElement();
				if (eConnect.isNull())
					continue;
				// Add this one to map...
				if (eConnect.tagName() == "client")
					pItem->clientName = eConnect.text();
				else
				if (eConnect.tagName() == "port")
					pItem->portName = eConnect.text();
			}
			connects.append(pItem);
		}
	}

	return true;
}

bool qtractorBus::saveConnects ( ConnectList& connects,
	qtractorSessionDocument *pDocument,	QDomElement *pElement )
{
	// Save connect items...
	QListIterator<ConnectItem *> iter(connects);
	while (iter.hasNext()) {
		ConnectItem *pItem = iter.next();
		QDomElement eItem = pDocument->document()->createElement("connect");
		eItem.setAttribute("index", QString::number(pItem->index));
		pDocument->saveTextElement("client", pItem->clientName, &eItem);
		pDocument->saveTextElement("port", pItem->portName, &eItem);
		pElement->appendChild(eItem);
	}

	return true;
}


// end of qtractorEngine.cpp
