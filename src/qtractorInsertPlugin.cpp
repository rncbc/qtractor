// qtractorInsertPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorInsertPlugin.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"


//----------------------------------------------------------------------------
// qtractorInsertPluginType -- Insert pseudo-plugin type instance.
//

// Derived methods.
bool qtractorInsertPluginType::open (void)
{
	// Sanity check...
	if (m_iChannels < 1)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorInsertPluginType[%p]::open() channels=%u",
		this, m_iChannels);
#endif

	// Pseudo-plugin type names.
	m_sName  = "Insert";
	m_sLabel = m_sName;

	// Pseudo-plugin unique identifier.
	m_iUniqueID = m_iChannels;

	// Pseudo-plugin port counts...
	m_iControlIns  = 0;
	m_iControlOuts = 0;
	m_iAudioIns    = m_iChannels;
	m_iAudioOuts   = m_iChannels;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = true;

	// Done.
	return true;
}


void qtractorInsertPluginType::close (void)
{
}


// Factory method (static)
qtractorInsertPluginType *qtractorInsertPluginType::createType (
	unsigned short iChannels )
{
	// Sanity check...
	if (iChannels < 1)
		return NULL;

	// Yep, most probably its a valid pseu-plugin...
	return new qtractorInsertPluginType(iChannels);
}


//----------------------------------------------------------------------------
// qtractorInsertPlugin -- Insert pseudo-plugin instance.
//

// Constructors.
qtractorInsertPlugin::qtractorInsertPlugin ( qtractorPluginList *pList,
	qtractorInsertPluginType *pInsertType )
	: qtractorPlugin(pList, pInsertType)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorInsertPlugin[%p] channels=%u",
		this, pInsertType->channels());
#endif

	setChannels(pInsertType->channels());
}


// Destructor.
qtractorInsertPlugin::~qtractorInsertPlugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);
}


// Channel/instance number accessors.
void qtractorInsertPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorPluginType *pType = type();
	if (pType == NULL)
		return;

	// We'll need this globals...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;
	
	// Estimate the (new) number of instances...
	unsigned short iInstances
		= pType->instances(iChannels, pType->isMidi());
	// Now see if instance count changed anyhow...
	if (iInstances == instances())
		return;

	// Gotta go for a while...
	bool bActivated = isActivated();
	setActivated(false);

	// TODO: Cleanup bus...
	if (m_pAudioBus) {
		pAudioEngine->removeBusEx(m_pAudioBus);
		m_pAudioBus->close();
		delete m_pAudioBus;
		m_pAudioBus = NULL;
	}

	// Set new instance number...
	setInstances(iInstances);
	if (iInstances < 1) {
		setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorInsertPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	// Audio bus name -- it must be unique...
	const QString& sBusNamePrefix
		= qtractorSession::sanitize(list()->name() + '_' + pType->name());

	int iBusName = 1;
	QString sBusName = sBusNamePrefix;
	while (pAudioEngine->findBus(sBusName)
		|| pAudioEngine->findBusEx(sBusName))
		sBusName = sBusNamePrefix + '_' + QString::number(++iBusName);

	// Create the private audio bus...
	m_pAudioBus = new qtractorAudioBus(pAudioEngine,
		sBusName, qtractorBus::Duplex, false, iChannels, false);

	// Add this one to the engine's exo-bus list,
	// for conection persistence purposes...
	pAudioEngine->addBusEx(m_pAudioBus);

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();

	// Open-up private bus...
	m_pAudioBus->open();

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Do the actual activation.
void qtractorInsertPlugin::activate (void)
{
}


// Do the actual deactivation.
void qtractorInsertPlugin::deactivate (void)
{
}


// The main plugin processing procedure.
void qtractorInsertPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_pAudioBus == NULL)
		return;

	m_pAudioBus->process_prepare(nframes);
	
	float **ppOut = m_pAudioBus->out(); // Sends.
	float **ppIn  = m_pAudioBus->in();  // Returns.

	unsigned short iChannels = channels();
	
	for (unsigned short i = 0; i < iChannels; ++i) {
		::memcpy(ppOut[i], ppIBuffer[i], nframes);
		::memcpy(ppOBuffer[i], ppIn[i], nframes);
	}

//	m_pAudioBus->process_commit(nframes);
}


// Pseudo-plugin configuration handlers.
void qtractorInsertPlugin::configure ( const QString& sKey, const QString& sValue )
{
	if (m_pAudioBus == NULL)
		return;

	qtractorBus::ConnectItem *pItem = new qtractorBus::ConnectItem;
	
	pItem->index = sValue.section('|', 0, 0).toUShort();

	const QString& sClient = sValue.section('|', 1, 1);
	const QString& sClientName = sClient.section(':', 1);
	if (sClientName.isEmpty()) {
		pItem->clientName = sClient;
	} else {
	//	pItem->client = sClient.section(':', 0, 0).toInt();
		pItem->clientName = sClientName;
	}

	const QString& sPort = sValue.section('|', 2, 2);
	const QString& sPortName = sPort.section(':', 1);
	if (sPortName.isEmpty()) {
		pItem->portName = sPort;
	} else {
	//	pItem->port = sPort.section(':', 0, 0).toInt();
		pItem->portName = sPortName;
	}

	const QString& sKeyPrefix = sKey.section('_', 0, 0);
	if (sKeyPrefix == "in")
		m_pAudioBus->inputs().append(pItem);
	else
	if (sKeyPrefix == "out")
		m_pAudioBus->outputs().append(pItem);
	else
		delete pItem;
}


// Pseudo-plugin configuration/state snapshot.
void qtractorInsertPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorInsertPlugin[%p]::freezeConfigs()",	this);
#endif

	freezeConfigs(qtractorBus::Input);
	freezeConfigs(qtractorBus::Output);
}

void qtractorInsertPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::releaseConfigs()", this);
#endif

	clearConfigs();
}


void qtractorInsertPlugin::freezeConfigs ( qtractorBus::BusMode busMode )
{
	if (m_pAudioBus == NULL)
		return;

	// Save connect items...
	const QString sKeyPrefix(busMode & qtractorBus::Input ? "in" : "out");
	int iKey = 0;

	qtractorBus::ConnectList connects;
	m_pAudioBus->updateConnects(busMode, connects);
	QListIterator<qtractorBus::ConnectItem *> iter(connects);
	while (iter.hasNext()) {
		qtractorBus::ConnectItem *pItem = iter.next();
		QString sIndex = QString::number(pItem->index);
		QString sClient;
		if (pItem->client >= 0)
			sClient += QString::number(pItem->client) + ':';
		sClient += pItem->clientName;
		QString sPort;
		if (pItem->port >= 0)
			sPort += QString::number(pItem->port) + ':';
		sPort += pItem->portName;
		QString sKey = sKeyPrefix + '_' + QString::number(iKey++);
		setConfig(sKey, sIndex + '|' + sClient + '|' + sPort);
	}
}


// end of qtractorInsertPlugin.cpp
