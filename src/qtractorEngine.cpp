// qtractorEngine.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMonitor.h"

#include "qtractorDocument.h"
#include "qtractorCurveFile.h"

#include <QDomDocument>
#include <QFileInfo>


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


// Move bus on a device engine.
void qtractorEngine::moveBus (
	qtractorBus *pBus, qtractorBus *pNextBus )
{
	m_buses.unlink(pBus);

	if (pNextBus)
		m_buses.insertBefore(pBus, pNextBus);
	else
		m_buses.append(pBus);
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
	for (qtractorBus *pBusEx = m_busesEx.first();
			pBusEx; pBusEx = pBusEx->next()) {
		if (pBusEx == pBus) {
			m_busesEx.remove(pBus);
			break;
		}
	}
}


// Find a exo-device bus by name
qtractorBus *qtractorEngine::findBusEx ( const QString& sBusName ) const
{
	for (qtractorBus *pBusEx = m_busesEx.first();
			pBusEx; pBusEx = pBusEx->next()) {
		if (pBusEx->busName() == sBusName)
			return pBusEx;
	}

	return NULL;
}


// Device engine activation method.
bool qtractorEngine::open (void)
{
	// Update the session cursor tracks...
	m_pSessionCursor->resetClips();
	m_pSessionCursor->reset();

	// Open all buses (allocated and register ports...)
	qtractorBus *pBus = m_buses.first();
	while (pBus) {
		if (!pBus->open())
			return false;
		pBus = pBus->next();
	}

	// Nows time to activate...
	if (!activate())
		return false;

	// We're now ready and running...
	m_bActivated = true;

	return true;
}


// Device engine deactivation method.
void qtractorEngine::close (void)
{
	// Save current activation state...
	const bool bActivated = m_bActivated;
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
		m_pSessionCursor->reset();
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


// Clear/reset all pending connections.
void qtractorEngine::clearConnects (void)
{
	qtractorBus *pBus;
	
	pBus = m_buses.first();
	for (; pBus; pBus = pBus->next()) {
		pBus->outputs().clear();
		pBus->inputs().clear();
	}

	pBus = m_busesEx.first();
	for (; pBus; pBus = pBus->next()) {
		pBus->outputs().clear();
		pBus->inputs().clear();
	}
}
	
	
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

	if (m_busMode & Ex) {
		m_pMonitorSubject = NULL;
	} else {
		m_pMonitorSubject = new qtractorSubject();
		m_pMonitorSubject->setToggled(true);
	}

	if (m_busMode & Ex) {
		m_pMonitorObserver = NULL;
	} else {
		m_pMonitorObserver = new qtractorMidiControlObserver(m_pMonitorSubject);
		m_pMonitorObserver->setValue(bMonitor ? 1.0f : 0.0f);
	}
}

// Destructor.
qtractorBus::~qtractorBus (void)
{
	if (m_pMonitorObserver)
		delete m_pMonitorObserver;
	if (m_pMonitorSubject)
		delete m_pMonitorSubject;

	qDeleteAll(m_controllers_out);
	m_controllers_out.clear();

	qDeleteAll(m_controllers_in);
	m_controllers_in.clear();
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

	updateBusName();
}

const QString& qtractorBus::busName (void) const
{
	return m_sBusName;
}


// Bus name change event. [virtual]
void qtractorBus::updateBusName (void)
{
	if (m_pMonitorSubject)
		m_pMonitorSubject->setName(QObject::tr("%1 Monitor").arg(m_sBusName));
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
	if (m_pMonitorSubject)
		m_pMonitorSubject->setValue(bMonitor ? 1.0f : 0.0f);
}

bool qtractorBus::isMonitor (void) const
{
	return (m_pMonitorSubject ? (m_pMonitorSubject->value() > 0.0f) : false);
}


// Bus state (monitor) button setup.
qtractorSubject *qtractorBus::monitorSubject (void) const
{
	return m_pMonitorSubject;
}

qtractorMidiControlObserver *qtractorBus::monitorObserver (void) const
{
	return m_pMonitorObserver;
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


// Load bus (monitor, gain, pan) controllers (MIDI).
void qtractorBus::loadControllers ( QDomElement *pElement, BusMode busMode )
{
	if (busMode & Input)
		qtractorMidiControl::loadControllers(pElement, m_controllers_in);
	else
		qtractorMidiControl::loadControllers(pElement, m_controllers_out);
}


// Save bus (monitor, gain, pan) controllers (MIDI).
void qtractorBus::saveControllers (
	qtractorDocument *pDocument, QDomElement *pElement, BusMode busMode ) const
{
	if (m_pMonitorObserver == NULL)
		return;

	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorMonitor *pMonitor = NULL;
	if (busMode & Input)
		pMonitor = monitor_in();
	else
		pMonitor = monitor_out();
	if (pMonitor == NULL)
		return;

	qtractorMidiControl::Controllers controllers;

	if (busMode & Input) // It suffices for Duplex...
	if (pMidiControl->isMidiObserverMapped(m_pMonitorObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = m_pMonitorObserver->subject()->name();
		pController->index = 0; // 0=MonitorObserver
		pController->ctype = m_pMonitorObserver->type();
		pController->channel = m_pMonitorObserver->channel();
		pController->param = m_pMonitorObserver->param();
		pController->logarithmic = m_pMonitorObserver->isLogarithmic();
		pController->feedback = m_pMonitorObserver->isFeedback();
		pController->invert = m_pMonitorObserver->isInvert();
		pController->hook = m_pMonitorObserver->isHook();
		pController->latch = m_pMonitorObserver->isLatch();
		controllers.append(pController);
	}

	qtractorMidiControlObserver *pPanObserver = pMonitor->panningObserver();
	if (pMidiControl->isMidiObserverMapped(pPanObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = pPanObserver->subject()->name();
		pController->index = 1; // 1=PanObserver
		pController->ctype = pPanObserver->type();
		pController->channel = pPanObserver->channel();
		pController->param = pPanObserver->param();
		pController->logarithmic = pPanObserver->isLogarithmic();
		pController->feedback = pPanObserver->isFeedback();
		pController->invert = pPanObserver->isInvert();
		pController->hook = pPanObserver->isHook();
		pController->latch = pPanObserver->isLatch();
		controllers.append(pController);
	}

	qtractorMidiControlObserver *pGainObserver = pMonitor->gainObserver();
	if (pMidiControl->isMidiObserverMapped(pGainObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = pGainObserver->subject()->name();
		pController->index = 2; // 2=GainObserver
		pController->ctype = pGainObserver->type();
		pController->channel = pGainObserver->channel();
		pController->param = pGainObserver->param();
		pController->logarithmic = pGainObserver->isLogarithmic();
		pController->feedback = pGainObserver->isFeedback();
		pController->invert = pGainObserver->isInvert();
		pController->hook = pGainObserver->isHook();
		pController->latch = pGainObserver->isLatch();
		controllers.append(pController);
	}

	qtractorMidiControl::saveControllers(pDocument, pElement, controllers);

	qDeleteAll(controllers);
	controllers.clear();
}


// Map bus (monitor, gain, pan) controllers (MIDI).
void qtractorBus::mapControllers ( BusMode busMode )
{
	if (m_pMonitorObserver == NULL)
		return;

	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorMonitor *pMonitor = NULL;
	if (busMode & Input)
		pMonitor = monitor_in();
	else
		pMonitor = monitor_out();
	if (pMonitor == NULL)
		return;

	qtractorMidiControl::Controllers& controllers
		= (busMode & Input ? m_controllers_in : m_controllers_out);
	QListIterator<qtractorMidiControl::Controller *> iter(controllers);
	while (iter.hasNext()) {
		qtractorMidiControl::Controller *pController = iter.next();
		qtractorMidiControlObserver *pObserver = NULL;
		switch (pController->index) {
		case 0: // 0=MonitorObserver
			pObserver = monitorObserver();
			break;
		case 1: // 1=PanObserver
			pObserver = pMonitor->panningObserver();
			break;
		case 2: // 2=GainObserver
			pObserver = pMonitor->gainObserver();
			break;
		}
		if (pObserver) {
			pObserver->setType(pController->ctype);
			pObserver->setChannel(pController->channel);
			pObserver->setParam(pController->param);
			pObserver->setLogarithmic(pController->logarithmic);
			pObserver->setFeedback(pController->feedback);
			pObserver->setInvert(pController->invert);
			pObserver->setHook(pController->hook);
			pObserver->setLatch(pController->latch);
			pMidiControl->mapMidiObserver(pObserver);
		}
	}
	
	qDeleteAll(controllers);
	controllers.clear();
}


// Load bus automation curves (monitor, gain, pan).
void qtractorBus::loadCurveFile (
	QDomElement *pElement, BusMode /*busMode*/, qtractorCurveFile *pCurveFile )
{
	if (pCurveFile) pCurveFile->load(pElement);
}


// Save bus automation curves (monitor, gain, pan).
void qtractorBus::saveCurveFile ( qtractorDocument *pDocument,
	QDomElement *pElement, BusMode busMode, qtractorCurveFile *pCurveFile ) const
{
	if (m_pMonitorSubject == NULL)
		return;

	if (pCurveFile == NULL)
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == NULL)
		return;

	qtractorSession *pSession = m_pEngine->session();
	if (pSession == NULL)
		return;

	QString sBusName(busName());
	qtractorMonitor *pMonitor = NULL;
	if (busMode & Input) {
		pMonitor  = monitor_in();
		sBusName += "_in";
	} else {
		pMonitor  = monitor_out();
		sBusName += "_out";
	}

	if (pMonitor == NULL)
		return;

	pCurveFile->clear();
	pCurveFile->setBaseDir(pSession->sessionDir());

	qtractorCurve *pCurve;

	if (busMode & Input) { // It suffices for Duplex...
	pCurve = monitorSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 0;	// 0=MonitorSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 80;	// 80=General Purpose Button 1 (on/off)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}}

	pCurve = pMonitor->panningSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 1;	// 1=PanningSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 10;	// 10=Pan Position (coarse)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	pCurve = pMonitor->gainSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 2; // 2=GainSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 7;	// 7=Volume (coarse)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	if (pCurveFile->isEmpty())
		return;

	const QString sBaseName(sBusName + "_curve");
	pCurveFile->setFilename(pSession->createFilePath(sBaseName, "mid", true));

	pCurveFile->save(pDocument, pElement, pSession->timeScale());
}


// Apply bus automation curves (monitor, gain, pan).
void qtractorBus::applyCurveFile ( BusMode busMode, qtractorCurveFile *pCurveFile ) const
{
	if (m_pMonitorSubject == NULL)
		return;

	if (pCurveFile == NULL)
		return;
	if (pCurveFile->items().isEmpty())
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == NULL)
		return;

	qtractorSession *pSession = m_pEngine->session();
	if (pSession == NULL)
		return;

	qtractorMonitor *pMonitor = NULL;
	if (busMode & Input)
		pMonitor = monitor_in();
	else
		pMonitor = monitor_out();

	if (pMonitor == NULL)
		return;

	pCurveFile->setBaseDir(pSession->sessionDir());

	QListIterator<qtractorCurveFile::Item *> iter(pCurveFile->items());
	while (iter.hasNext()) {
		qtractorCurveFile::Item *pCurveItem = iter.next();
		switch (pCurveItem->index) {
		case 0: // 0=MonitorSubject
			pCurveItem->subject = monitorSubject();
			break;
		case 1: // 1=PanSubject
			pCurveItem->subject = pMonitor->panningSubject();
			break;
		case 2: // 2=GainSubject
			pCurveItem->subject = pMonitor->gainSubject();
			break;
		}
	}

	pCurveFile->apply(pSession->timeScale());
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


// Bus connections snapshot executive mthods.
int qtractorBus::Connects::save ( qtractorBus *pBus )
{
	m_sBusName = pBus->busName();
	m_busMode  = pBus->busMode();

	if (m_busMode & Input)
		pBus->updateConnects(Input, m_inputs);
	if (m_busMode & Output)
		pBus->updateConnects(Output, m_outputs);

	return m_inputs.count() + m_outputs.count();
}


// Bus connections snapshot executive mthods.
int qtractorBus::Connects::load( qtractorBus *pBus )
{
	if (pBus->busName() != m_sBusName ||
		pBus->busMode() != m_busMode)
		return 0;

	if (m_busMode & Input)
		pBus->inputs().copy(m_inputs);
	if (m_busMode & Output)
		pBus->outputs().copy(m_outputs);

	return pBus->inputs().count() + pBus->outputs().count();
}


// Bus connections snapshot cleaner.
void qtractorBus::Connects::clear (void)
{
	m_sBusName.clear();
	m_busMode = None;

	m_inputs.clear();
	m_outputs.clear();
}


// Engine connections snapshot executive mthods.
bool qtractorBus::Connections::load(qtractorEngine *pEngine)
{
	int iUpdate = 0;

	QListIterator<Connects *> iter(m_list);
	while (iter.hasNext()) {
		Connects *pConnect = iter.next();
		const QString& sBusName = pConnect->busName();
		const BusMode busMode = pConnect->busMode();
		qtractorBus *pBus = NULL;
		if (busMode & Ex)
			pBus = pEngine->findBusEx(sBusName);
		else
			pBus = pEngine->findBus(sBusName);
		if (pBus)
			iUpdate += pConnect->load(pBus);
	}

	return (iUpdate > 0);
}


bool qtractorBus::Connections::save(qtractorEngine *pEngine)
{
	int iUpdate = 0;

	iUpdate += save(pEngine->buses().first());
	iUpdate += save(pEngine->busesEx().first());

	return (iUpdate > 0);
}


int qtractorBus::Connections::save(qtractorBus *pBus)
{
	int iUpdate = 0;

	for (; pBus; pBus = pBus->next()) {
		Connects *pConnects = new Connects();
		if (pConnects->save(pBus) > 0) {
			m_list.append(pConnects);
			++iUpdate;
		}
		else delete pConnects;
	}

	return iUpdate;
}


// Generic connections snapshot cleaner.
void qtractorBus::Connections::clear (void)
{
	qDeleteAll(m_list);
	m_list.clear();
}


// end of qtractorEngine.cpp
