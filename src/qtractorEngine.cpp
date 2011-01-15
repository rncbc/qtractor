// qtractorEngine.cpp
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

#include "qtractorAbout.h"
#include "qtractorEngine.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"

#include "qtractorMidiControlObserver.h"

#include "qtractorEngineCommand.h"

#include "qtractorMainForm.h"
#include "qtractorMonitor.h"
#include "qtractorMeter.h"
#include "qtractorMixer.h"

#include "qtractorDocument.h"

#include <QDomDocument>


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
	m_busesEx.setAutoDelete(false);
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
	m_busesEx.clear();
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
	return m_pSession->clientName();
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
qtractorBus *qtractorEngine::findBus ( const QString& sBusName ) const
{
	for (qtractorBus *pBus = m_buses.first();
			pBus; pBus = pBus->next()) {
		if (pBus->busName() == sBusName)
			return pBus;
	}

	return NULL;
}


// Find an input bus by name
qtractorBus *qtractorEngine::findInputBus (
	const QString& sInputBusName ) const
{
	for (qtractorBus *pBus = m_buses.first();
			pBus; pBus = pBus->next()) {
		if ((pBus->busMode() & qtractorBus::Input)
			&& pBus->busName() == sInputBusName)
			return pBus;
	}

	return NULL;
}


// Find an output bus by name
qtractorBus *qtractorEngine::findOutputBus (
	const QString& sOutputBusName ) const
{
	for (qtractorBus *pBus = m_buses.first();
			pBus; pBus = pBus->next()) {
		if ((pBus->busMode() & qtractorBus::Output)
			&& pBus->busName() == sOutputBusName)
			return pBus;
	}

	return NULL;
}


// Exo-buses list managament methods.
const qtractorList<qtractorBus>& qtractorEngine::busesEx (void) const
{
	return m_busesEx;
}


// Add an exo-bus to a device engine.
void qtractorEngine::addBusEx ( qtractorBus *pBus )
{
	m_busesEx.append(pBus);
}


// Remove an exo-bus from a device.
void qtractorEngine::removeBusEx ( qtractorBus *pBus )
{
	m_busesEx.remove(pBus);
}


// Find a exo-device bus by name
qtractorBus *qtractorEngine::findBusEx ( const QString& sBusName ) const
{
	for (qtractorBus *pBus = m_busesEx.first();
			pBus; pBus = pBus->next()) {
		if (pBus->busName() == sBusName)
			return pBus;
	}

	return NULL;
}


// Device engine activation method.
bool qtractorEngine::open (void)
{
//	close();

	// Damn it.
	if (m_pSession == NULL)
		return false;

	// Call derived initialization...
	if (!init())
		return false;

	// Update the session cursor tracks...
	m_pSessionCursor->resetClips();
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
	if (bPlaying && !m_bPlaying) {
		m_pSessionCursor->setFrameTime(0);
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
	// It must be activated, sure...
	if (!isActivated())
		return 0;

	// On all dependable buses...
	int iUpdate = 0;

	iUpdate += updateConnects(m_buses.first());
	iUpdate += updateConnects(m_busesEx.first());

	// Done.
	return iUpdate;
}

int qtractorEngine::updateConnects ( qtractorBus *pBus )
{
	int iUpdate = 0;
	for (; pBus; pBus = pBus->next()) {
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
	return iUpdate;
}


//------------------------------------------------------------------------
// qtractorBus::MonitorObserver -- Local bus state observer.

class qtractorBus::MonitorObserver : public qtractorMidiControlObserver
{
public:

	// Constructor.
	MonitorObserver(qtractorBus *pBus, qtractorSubject *pSubject)
		: qtractorMidiControlObserver(pSubject), m_pBus(pBus) {}

protected:

	// Update feedback.
	void update()
	{
		m_pBus->monitorChangeNotify(value() > 0.0f);
		qtractorMidiControlObserver::update();
	}

private:

	// Members.
	qtractorBus *m_pBus;
};


//----------------------------------------------------------------------
// class qtractorBus -- Managed ALSA sequencer port set
//

// Constructor.
qtractorBus::qtractorBus ( qtractorEngine *pEngine,
	const QString& sBusName, BusMode busMode, bool bMonitor )
{
	m_pEngine  = pEngine;
	m_sBusName = sBusName;
	m_busMode  = busMode;

	m_pMonitorSubject = new qtractorSubject(bMonitor ? 1.0f : 0.0f);
	m_pMonitorObserver = new MonitorObserver(this, m_pMonitorSubject);
}

// Destructor.
qtractorBus::~qtractorBus (void)
{
	if (m_pMonitorObserver)
		delete m_pMonitorObserver;
	if (m_pMonitorSubject)
		delete m_pMonitorSubject;

	qDeleteAll(m_controllers);
	m_controllers.clear();
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

	m_pMonitorSubject->setName(QObject::tr("%1 Monitor").arg(sBusName));
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


// Pass-thru mode accessor.
void qtractorBus::setMonitor ( bool bMonitor )
{
	m_pMonitorObserver->setValue(bMonitor ? 1.0f : 0.0f);
}

bool qtractorBus::isMonitor (void) const
{
	return (m_pMonitorSubject->value() > 0.0f);
}


// Bus state (monitor) button setup.
qtractorSubject *qtractorBus::monitorSubject (void) const
{
	return m_pMonitorSubject;
}

qtractorMidiControlObserver *qtractorBus::monitorObserver (void) const
{
	return static_cast<qtractorMidiControlObserver *> (m_pMonitorObserver);
}


// Bus state (monitor) notifier (proto-slot).
void qtractorBus::monitorChangeNotify ( bool bOn )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorBus[%p]::monitorChangeNotify(%d)", this, int(bOn));
#endif

	// Put it in the form of an undoable command...
	qtractorSession *pSession = m_pEngine->session();
	if (pSession)
		pSession->execute(
			new qtractorBusMonitorCommand(this, bOn));
}


// Load track state (record, mute, solo) controllers (MIDI).
void qtractorBus::loadControllers ( QDomElement *pElement )
{
	qtractorMidiControl::loadControllers(pElement, m_controllers);
}


// Save track state (record, mute, solo) controllers (MIDI).
void qtractorBus::saveControllers (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer == NULL)
		return;

	qtractorMonitor *pMonitor = NULL;
	qtractorMixerRack *pMixerRack = NULL;
	if (m_busMode & Input) {
		pMonitor = monitor_in();
		pMixerRack = pMixer->inputRack();
	} else {
		pMonitor = monitor_out();
		pMixerRack = pMixer->outputRack();
	}
	
	qtractorMixerStrip *pMixerStrip	= pMixerRack->findStrip(pMonitor);
	if (pMixerStrip == NULL)
		return;

	qtractorMidiControl::Controllers controllers;

	if (pMidiControl->isMidiObserverMapped(m_pMonitorObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->index = 0; // 0=MonitorObserver
		pController->ctype = m_pMonitorObserver->type();
		pController->channel = m_pMonitorObserver->channel();
		pController->param = m_pMonitorObserver->param();
		pController->logarithmic = m_pMonitorObserver->isLogarithmic();
		pController->feedback = m_pMonitorObserver->isFeedback();
		controllers.append(pController);
	}

	qtractorMidiControlObserver *pPanObserver
		= pMixerStrip->meter()->panningObserver();
	if (pMidiControl->isMidiObserverMapped(pPanObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->index = 1; // 1=PanObserver
		pController->ctype = pPanObserver->type();
		pController->channel = pPanObserver->channel();
		pController->param = pPanObserver->param();
		pController->logarithmic = pPanObserver->isLogarithmic();
		pController->feedback = pPanObserver->isFeedback();
		controllers.append(pController);
	}

	qtractorMidiControlObserver *pGainObserver
		= pMixerStrip->meter()->gainObserver();
	if (pMidiControl->isMidiObserverMapped(pGainObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->index = 2; // 2=GainObserver
		pController->ctype = pGainObserver->type();
		pController->channel = pGainObserver->channel();
		pController->param = pGainObserver->param();
		pController->logarithmic = pGainObserver->isLogarithmic();
		pController->feedback = pGainObserver->isFeedback();
		controllers.append(pController);
	}

	qtractorMidiControl::saveControllers(pDocument, pElement, controllers);

	qDeleteAll(controllers);
	controllers.clear();
}


// Map track state (record, mute, solo) controllers (MIDI).
void qtractorBus::mapControllers (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer == NULL)
		return;

	qtractorMonitor *pMonitor = NULL;
	qtractorMixerRack *pMixerRack = NULL;
	if (m_busMode & Input) {
		pMonitor = monitor_in();
		pMixerRack = pMixer->inputRack();
	} else {
		pMonitor = monitor_out();
		pMixerRack = pMixer->outputRack();
	}
	
	qtractorMixerStrip *pMixerStrip	= pMixerRack->findStrip(pMonitor);
	if (pMixerStrip == NULL)
		return;

	QListIterator<qtractorMidiControl::Controller *> iter(m_controllers);
	while (iter.hasNext()) {
		qtractorMidiControl::Controller *pController = iter.next();
		qtractorMidiControlObserver *pObserver = NULL;
		switch (pController->index) {
		case 0: // 0=MonitorObserver
			pObserver = monitorObserver();
			break;
		case 1: // 1=PanObserver
			pObserver = pMixerStrip->meter()->panningObserver();
			break;
		case 2: // 2=GainObserver
			pObserver = pMixerStrip->meter()->gainObserver();
			break;
		}
		if (pObserver) {
			pObserver->setType(pController->ctype);
			pObserver->setChannel(pController->channel);
			pObserver->setParam(pController->param);
			pObserver->setLogarithmic(pController->logarithmic);
			pObserver->setFeedback(pController->feedback);
			pMidiControl->mapMidiObserver(pObserver);
		}
	}
	
	qDeleteAll(m_controllers);
	m_controllers.clear();
}


// Document element methods.
bool qtractorBus::loadConnects ( ConnectList& connects,
	qtractorDocument * /*pDocument*/, QDomElement *pElement )
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
				if (eConnect.tagName() == "client") {
					const QString& sClient = eConnect.text();
					const QString& sClientName = sClient.section(':', 1);
					if (sClientName.isEmpty()) {
						pItem->clientName = sClient;
					} else {
					//	pItem->client = sClient.section(':', 0, 0).toInt();
						pItem->clientName = sClientName;
					}
				}
				else
				if (eConnect.tagName() == "port") {
					const QString& sPort = eConnect.text();
					const QString& sPortName = sPort.section(':', 1);
					if (sPortName.isEmpty()) {
						pItem->portName = sPort;
					} else {
					//	pItem->port = sPort.section(':', 0, 0).toInt();
						pItem->portName = sPortName;
					}
				}
			}
			connects.append(pItem);
		}
	}

	return true;
}

bool qtractorBus::saveConnects ( ConnectList& connects,
	qtractorDocument *pDocument,	QDomElement *pElement )
{
	// Save connect items...
	QListIterator<ConnectItem *> iter(connects);
	while (iter.hasNext()) {
		ConnectItem *pItem = iter.next();
		QDomElement eItem = pDocument->document()->createElement("connect");
		eItem.setAttribute("index", QString::number(pItem->index));
		QString sClient;
		if (pItem->client >= 0)
			sClient += QString::number(pItem->client) + ':';
		sClient += pItem->clientName;
		pDocument->saveTextElement("client", sClient, &eItem);
		QString sPort;
		if (pItem->port >= 0)
			sPort += QString::number(pItem->port) + ':';
		sPort += pItem->portName;
		pDocument->saveTextElement("port", sPort, &eItem);
		pElement->appendChild(eItem);
	}

	return true;
}


// Bus mode textual helper methods.
qtractorBus::BusMode qtractorBus::busModeFromText ( const QString& sText )
{
	BusMode busMode = None;
	if (sText == "input")
		busMode = Input;
	else if (sText == "output")
		busMode = Output;
	else if (sText == "duplex")
		busMode = Duplex;
	return busMode;
}

QString qtractorBus::textFromBusMode ( BusMode busMode )
{
	QString sText;
	switch (busMode) {
	case Input:
		sText = "input";
		break;
	case Output:
		sText = "output";
		break;
	case Duplex:
		sText = "duplex";
		break;
	case None:
	default:
		sText = "none";
		break;
	}
	return sText;
}


// end of qtractorEngine.cpp
