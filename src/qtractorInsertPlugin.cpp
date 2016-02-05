// qtractorInsertPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2016, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2011, Holger Dehnhardt.

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

#include "qtractorPluginListView.h"


#if defined(__SSE__)

#include <xmmintrin.h>

// SSE detection.
static inline bool sse_enabled (void)
{
#if defined(__GNUC__)
	unsigned int eax, ebx, ecx, edx;
#if defined(__x86_64__) || (!defined(PIC) && !defined(__PIC__))
	__asm__ __volatile__ (
		"cpuid\n\t" \
		: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) \
		: "a" (1) : "cc");
#else
	__asm__ __volatile__ (
		"push %%ebx\n\t" \
		"cpuid\n\t" \
		"movl %%ebx,%1\n\t" \
		"pop %%ebx\n\t" \
		: "=a" (eax), "=r" (ebx), "=c" (ecx), "=d" (edx) \
		: "a" (1) : "cc");
#endif
	return (edx & (1 << 25));
#else
	return false;
#endif
}

// SSE enabled processor versions.
static inline void sse_process_send_gain (
	float **ppFrames, unsigned int iFrames,
	unsigned short iChannels, float fGain )
{
	__m128 v0 = _mm_load_ps1(&fGain);

	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pFrames = ppFrames[i];
		unsigned int nframes = iFrames;
		for (; (long(pFrames) & 15) && (nframes > 0); --nframes)
			*pFrames++ *= fGain;	
		for (; nframes >= 4; nframes -= 4) {
			_mm_store_ps(pFrames,
				_mm_mul_ps(
					_mm_loadu_ps(pFrames), v0
				)
			);
			pFrames += 4;
		}
		for (; nframes > 0; --nframes)
			*pFrames++ *= fGain;
	}
}

static inline void sse_process_dry_wet (
	float **ppBuffer, float **ppFrames, unsigned int iFrames,
	unsigned short iChannels, float fGain )
{
	__m128 v0 = _mm_load_ps1(&fGain);

	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pBuffer = ppBuffer[i];
		float *pFrames = ppFrames[i];
		unsigned int nframes = iFrames;
		for (; (long(pBuffer) & 15) && (nframes > 0); --nframes)
			*pBuffer++ += fGain * *pFrames++;
		for (; nframes >= 4; nframes -= 4) {
			_mm_store_ps(pBuffer,
				_mm_add_ps(
					_mm_loadu_ps(pBuffer),
					_mm_mul_ps(
						_mm_loadu_ps(pFrames), v0)
					)
			);
			pFrames += 4;
			pBuffer += 4;
		}
		for (; nframes > 0; --nframes)
			*pBuffer++ += fGain * *pFrames++;
	}
}

#endif


// Standard processor versions.
static inline void std_process_send_gain (
	float **ppFrames, unsigned int iFrames,
	unsigned short iChannels, float fGain )
{
	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pFrames = ppFrames[i];
		for (unsigned int n = 0; n < iFrames; ++n)
			*pFrames++ *= fGain;
	}
}

static inline void std_process_dry_wet (
	float **ppBuffer, float **ppFrames, unsigned int iFrames,
	unsigned short iChannels, float fGain )
{
	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pBuffer = ppBuffer[i];
		float *pFrames = ppFrames[i];
		for (unsigned int n = 0; n < iFrames; ++n)
			*pBuffer++ += fGain * *pFrames++;
	}
}


//----------------------------------------------------------------------------
// qtractorInsertPluginType -- Insert pseudo-plugin type impl.
//

// Factory method (static)
qtractorPlugin *qtractorInsertPluginType::createPlugin (
	qtractorPluginList *pList, unsigned short iChannels )
{
	// Sanity check...
	if (iChannels < 1)
		return NULL;

	// Check whether it's a valid insert pseudo-plugin...
	qtractorInsertPluginType *pInsertType
		= new qtractorAudioInsertPluginType(iChannels);

	if (pInsertType->open())
		return new qtractorAudioInsertPlugin(pList, pInsertType);

	delete pInsertType;
	return NULL;
}


//----------------------------------------------------------------------------
// qtractorAudioInsertPluginType -- Audio-insert pseudo-plugin type instance.
//

// Derived methods.
bool qtractorAudioInsertPluginType::open (void)
{
	// Sanity check...
	const unsigned short iChannels = index();
	if (iChannels < 1)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioInsertPluginType[%p]::open() channels=%u",
		this, iChannels);
#endif

	// Pseudo-plugin type names.
	m_sName  = "Insert";
	m_sLabel = m_sName;
//	m_sLabel.remove(' ');

	// Pseudo-plugin unique identifier.
	m_iUniqueID = qHash(m_sLabel) ^ qHash(iChannels);

	// Pseudo-plugin port counts...
	m_iControlIns  = 2;
	m_iControlOuts = 0;
	m_iAudioIns    = iChannels;
	m_iAudioOuts   = iChannels;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = true;

	// Done.
	return true;
}


void qtractorAudioInsertPluginType::close (void)
{
}


// Instance cached-deferred accessors.
const QString& qtractorAudioInsertPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		m_sAboutText += QObject::tr("Audio Insert Send/Return pseudo-plugin.");
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_WEBSITE;
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_COPYRIGHT;
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------------
// qtractorAudioInsertPlugin -- Audio-insert pseudo-plugin instance.
//

// Constructors.
qtractorAudioInsertPlugin::qtractorAudioInsertPlugin (
	qtractorPluginList *pList, qtractorInsertPluginType *pInsertType )
	: qtractorPlugin(pList, pInsertType), m_pAudioBus(NULL)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioInsertPlugin[%p] channels=%u",
		this, pInsertType->channels());
#endif

	// Custom optimized processors.
#if defined(__SSE__)
	if (sse_enabled()) {
		m_pfnProcessSendGain = sse_process_send_gain;
		m_pfnProcessDryWet = sse_process_dry_wet;
	} else {
#endif
	m_pfnProcessSendGain = std_process_send_gain;
	m_pfnProcessDryWet = std_process_dry_wet;
#if defined(__SSE__)
	}
#endif

	// Create and attach the custom parameters...
	m_pSendGainParam = new qtractorInsertPluginParam(this, 0);
	m_pSendGainParam->setName(QObject::tr("Send Gain"));
	m_pSendGainParam->setMinValue(0.0f);
	m_pSendGainParam->setMaxValue(2.0f);
	m_pSendGainParam->setDefaultValue(1.0f);
	m_pSendGainParam->setValue(1.0f, false);
	addParam(m_pSendGainParam);

	m_pDryWetParam = new qtractorInsertPluginParam(this, 1);
	m_pDryWetParam->setName(QObject::tr("Dry / Wet"));
	m_pDryWetParam->setMinValue(0.0f);
	m_pDryWetParam->setMaxValue(2.0f);
	m_pDryWetParam->setDefaultValue(0.0f);
	m_pDryWetParam->setValue(0.0f, false);
	addParam(m_pDryWetParam);

	// Setup plugin instance...
	setChannels(channels());
}


// Destructor.
qtractorAudioInsertPlugin::~qtractorAudioInsertPlugin (void)
{
	// Cleanup plugin instance...
	setChannels(0);
}


// Channel/instance number accessors.
void qtractorAudioInsertPlugin::setChannels ( unsigned short iChannels )
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
	const unsigned short iInstances
		= pType->instances(iChannels, list()->isMidi());
	// Now see if instance count changed anyhow...
	if (iInstances == instances())
		return;

	// Gotta go for a while...
	const bool bActivated = isActivated();
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
	//	setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioInsertPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	// Audio bus name -- it must be unique...
	const QString& sBusNamePrefix
		= qtractorSession::sanitize(list()->name() + ' ' + pType->name());

	int iBusName = 1;
	QString sBusName = sBusNamePrefix;
	while (pAudioEngine->findBus(sBusName)
		|| pAudioEngine->findBusEx(sBusName))
		sBusName = sBusNamePrefix + '_' + QString::number(++iBusName);

	// Create the private audio bus...
	m_pAudioBus = new qtractorAudioBus(pAudioEngine, sBusName,
		qtractorBus::BusMode(qtractorBus::Duplex | qtractorBus::Ex),
		false, iChannels);

	// Add this one to the engine's exo-bus list,
	// for conection persistence purposes...
	pAudioEngine->addBusEx(m_pAudioBus);

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// Open-up private bus...
	m_pAudioBus->open();

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Do the actual activation.
void qtractorAudioInsertPlugin::activate (void)
{
}


// Do the actual deactivation.
void qtractorAudioInsertPlugin::deactivate (void)
{
}


// The main plugin processing procedure.
void qtractorAudioInsertPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_pAudioBus == NULL)
		return;

//	m_pAudioBus->process_prepare(nframes);

	float **ppOut = m_pAudioBus->out(); // Sends.
	float **ppIn  = m_pAudioBus->in();  // Returns.

	const unsigned short iChannels = channels();

	for (unsigned short i = 0; i < iChannels; ++i) {
		::memcpy(ppOut[i], ppIBuffer[i], nframes * sizeof(float));
		::memcpy(ppOBuffer[i], ppIn[i], nframes * sizeof(float));
	}

	const float fSendGain = m_pSendGainParam->value();
	(*m_pfnProcessSendGain)(ppOut, nframes, iChannels, fSendGain);

	const float fDryWet = m_pDryWetParam->value();
	if (fDryWet > 0.001f)
		(*m_pfnProcessDryWet)(ppOBuffer, ppIBuffer, nframes, iChannels, fDryWet);

//	m_pAudioBus->process_commit(nframes);
}


// Pseudo-plugin configuration handlers.
void qtractorAudioInsertPlugin::configure (
	const QString& sKey, const QString& sValue )
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
void qtractorAudioInsertPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioInsertPlugin[%p]::freezeConfigs()",	this);
#endif

	clearConfigs();

	freezeConfigs(qtractorBus::Input);
	freezeConfigs(qtractorBus::Output);
}


void qtractorAudioInsertPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioInsertPlugin[%p]::releaseConfigs()", this);
#endif

	clearConfigs();
}


void qtractorAudioInsertPlugin::freezeConfigs ( int iBusMode )
{
	if (m_pAudioBus == NULL)
		return;

	// Save connect items...
	qtractorBus::BusMode busMode = qtractorBus::BusMode(iBusMode);
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


// Audio specific accessor.
qtractorAudioBus *qtractorAudioInsertPlugin::audioBus (void) const
{
	return m_pAudioBus;
}


//----------------------------------------------------------------------------
// qtractorAuxSendPluginType -- Aux-send pseudo-plugin impl.
//

// Factory method (static)
qtractorPlugin *qtractorAuxSendPluginType::createPlugin (
	qtractorPluginList *pList, unsigned short iChannels )
{
	// Sanity check...
	if (iChannels < 1)
		return NULL;

	// Check whether it's a valid aux-send pseudo-plugin...
	qtractorAuxSendPluginType *pAuxSendType
		= new qtractorAudioAuxSendPluginType(iChannels);

	if (pAuxSendType->open())
		return new qtractorAudioAuxSendPlugin(pList, pAuxSendType);

	delete pAuxSendType;
	return NULL;
}


//----------------------------------------------------------------------------
// qtractorAudioAuxSendPluginType -- Audio aux-send pseudo-plugin type.
//

// Derived methods.
bool qtractorAudioAuxSendPluginType::open (void)
{
	// Sanity check...
	const unsigned short iChannels = index();
	if (iChannels < 1)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioAuxSendPluginType[%p]::open() channels=%u",
		this, iChannels);
#endif

	// Pseudo-plugin type names.
	m_sName  = "Aux Send";
	m_sLabel = m_sName;
	m_sLabel.remove(' ');

	// Pseudo-plugin unique identifier.
	m_iUniqueID = qHash(m_sLabel) ^ qHash(iChannels);

	// Pseudo-plugin port counts...
	m_iControlIns  = 1;
	m_iControlOuts = 0;
	m_iAudioIns    = iChannels;
	m_iAudioOuts   = iChannels;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = true;

	// Done.
	return true;
}


void qtractorAudioAuxSendPluginType::close (void)
{
}


// Instance cached-deferred accesors.
const QString& qtractorAudioAuxSendPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		m_sAboutText += QObject::tr("Audio Aux Send pseudo-plugin.");
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_WEBSITE;
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_COPYRIGHT;
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------------
// qtractorAudioAuxSendPlugin -- Audio aux-send pseudo-plugin instance.
//

// Constructors.
qtractorAudioAuxSendPlugin::qtractorAudioAuxSendPlugin (
	qtractorPluginList *pList, qtractorAuxSendPluginType *pAuxSendType )
	: qtractorPlugin(pList, pAuxSendType), m_pAudioBus(NULL)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioAuxSendPlugin[%p] channels=%u",
		this, pAuxSendType->channels());
#endif

	// Custom optimized processors.
#if defined(__SSE__)
	if (sse_enabled()) {
		m_pfnProcessDryWet = sse_process_dry_wet;
	} else {
#endif
		m_pfnProcessDryWet = std_process_dry_wet;
#if defined(__SSE__)
	}
#endif

	// Create and attach the custom parameters...
	m_pSendGainParam = new qtractorInsertPluginParam(this, 0);
	m_pSendGainParam->setName(QObject::tr("Send Gain"));
	m_pSendGainParam->setMinValue(0.0f);
	m_pSendGainParam->setMaxValue(2.0f);
	m_pSendGainParam->setDefaultValue(0.0f);
	m_pSendGainParam->setValue(0.0f, false);
	addParam(m_pSendGainParam);
}


// Destructor.
qtractorAudioAuxSendPlugin::~qtractorAudioAuxSendPlugin (void)
{
	// Cleanup plugin instance...
	setChannels(0);
}


// Channel/instance number accessors.
void qtractorAudioAuxSendPlugin::setChannels ( unsigned short iChannels )
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
	const unsigned short iInstances
		= pType->instances(iChannels, list()->isMidi());
	// Now see if instance count changed anyhow...
	if (iInstances == instances())
		return;

	// Gotta go for a while...
	const bool bActivated = isActivated();
	setActivated(false);

	// TODO: Cleanup bus...
	if (m_pAudioBus)
		m_pAudioBus = NULL;

	// Set new instance number...
	setInstances(iInstances);
	if (iInstances < 1) {
	//	setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioAuxSendPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// Try to find a nice default...
	if (m_sAudioBusName.isEmpty()) {
		for (qtractorBus *pBus = pAudioEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				qtractorAudioBus *pAudioBus
					= static_cast<qtractorAudioBus *> (pBus);
				if (pAudioBus && pAudioBus->channels() == iChannels) {
					m_sAudioBusName = pAudioBus->busName();
					break;
				}
			}
		}
	}

	// Setup aux-send bus...
	setAudioBusName(m_sAudioBusName);

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Audio bus specific accessors.
void qtractorAudioAuxSendPlugin::setAudioBusName ( const QString& sAudioBusName )
{
	if (sAudioBusName.isEmpty())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	qtractorAudioBus *pAudioBus = static_cast<qtractorAudioBus *> (
		pAudioEngine->findOutputBus(sAudioBusName));
	if (pAudioBus && pAudioBus->channels() == channels()) {
		m_pAudioBus = pAudioBus;
		m_sAudioBusName = sAudioBusName;
	//	setConfig("audioBusName", m_sAudioBusName);
	} else {
		m_pAudioBus = NULL;
		m_sAudioBusName.clear();
	//	clearConfigs();
	}

	updateAudioBusName();
}

const QString& qtractorAudioAuxSendPlugin::audioBusName (void) const
{
	return m_sAudioBusName;
}


// Audio bus to appear on plugin lists.
void qtractorAudioAuxSendPlugin::updateAudioBusName (void) const
{
	const QString& sText = (m_pAudioBus ? m_sAudioBusName : type()->name());
	QListIterator<qtractorPluginListItem *> iter(items());
	while (iter.hasNext())
		iter.next()->setText(sText);
}


// The main plugin processing procedure.
void qtractorAudioAuxSendPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_pAudioBus == NULL)
		return;

//	m_pAudioBus->process_prepare(nframes);

	float **ppOut = m_pAudioBus->out();

	const unsigned short iChannels = channels();

	for (unsigned short i = 0; i < iChannels; ++i)
		::memcpy(ppOBuffer[i], ppIBuffer[i], nframes * sizeof(float));

	const float fSendGain = m_pSendGainParam->value();
	(*m_pfnProcessDryWet)(ppOut, ppOBuffer, nframes, iChannels, fSendGain);

//	m_pAudioBus->process_commit(nframes);
}


// Do the actual activation.
void qtractorAudioAuxSendPlugin::activate (void)
{
}


// Do the actual deactivation.
void qtractorAudioAuxSendPlugin::deactivate (void)
{
}


// Pseudo-plugin configuration handlers.
void qtractorAudioAuxSendPlugin::configure ( const QString& sKey, const QString& sValue )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioAuxSendPlugin[%p]::configure()", this);
#endif

	if (sKey == "audioBusName")
		setAudioBusName(sValue);
}


// Pseudo-plugin configuration/state snapshot.
void qtractorAudioAuxSendPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioAuxSendPlugin[%p]::freezeConfigs()", this);
#endif

	clearConfigs();

	setConfig("audioBusName", m_sAudioBusName);
}


void qtractorAudioAuxSendPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorAudioAuxSendPlugin[%p]::releaseConfigs()", this);
#endif

	clearConfigs();
}


// end of qtractorInsertPlugin.cpp
