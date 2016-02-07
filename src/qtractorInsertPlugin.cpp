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
#include "qtractorMidiEngine.h"
#include "qtractorMidiBuffer.h"

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
	// Check whether it's a valid insert pseudo-plugin...
	qtractorPlugin *pPlugin = NULL;
	qtractorInsertPluginType *pInsertType = NULL;

	if (iChannels > 0) {
		pInsertType = new qtractorAudioInsertPluginType(iChannels);
		if (pInsertType->open())
			pPlugin = new qtractorAudioInsertPlugin(pList, pInsertType);
	} else {
		pInsertType = new qtractorMidiInsertPluginType();
		if (pInsertType->open())
			pPlugin = new qtractorMidiInsertPlugin(pList, pInsertType);
	}

	if (pPlugin == NULL && pInsertType)
		delete pInsertType;

	return pPlugin;
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
	m_sLabel = "Audio" + m_sName;
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
// qtractorMidiInsertPluginType -- MIDI-insert pseudo-plugin type instance.
//

// Derived methods.
bool qtractorMidiInsertPluginType::open (void)
{
	// Sanity check...
	const unsigned short iChannels = index();
	if (iChannels > 0)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiInsertPluginType[%p]::open() channels=%u",
		this, iChannels);
#endif

	// Pseudo-plugin type names.
	m_sName  = "Insert";
	m_sLabel = "Midi" + m_sName;
//	m_sLabel.remove(' ');

	// Pseudo-plugin unique identifier.
	m_iUniqueID = qHash(m_sLabel);//^ qHash(iChannels);

	// Pseudo-plugin port counts...
	m_iControlIns  = 1;
	m_iControlOuts = 0;
	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
	m_iMidiIns     = 1;
	m_iMidiOuts    = 1;

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = true;

	// Done.
	return true;
}


void qtractorMidiInsertPluginType::close (void)
{
}


// Instance cached-deferred accessors.
const QString& qtractorMidiInsertPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		m_sAboutText += QObject::tr("MIDI Insert Send/Return pseudo-plugin.");
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
	//setChannels(channels());
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
	int iBusName = 1;
	const QString& sBusNamePrefix = pType->name() + "_%1";
	QString sBusName = sBusNamePrefix.arg(iBusName);
	while (pAudioEngine->findBus(sBusName)
		|| pAudioEngine->findBusEx(sBusName))
		sBusName = sBusNamePrefix.arg(++iBusName);

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
// qtractorMidiInsertPlugin -- MIDI-insert pseudo-plugin instance.
//

// Constructors.
qtractorMidiInsertPlugin::qtractorMidiInsertPlugin (
	qtractorPluginList *pList, qtractorInsertPluginType *pInsertType )
	: qtractorPlugin(pList, pInsertType), m_pMidiBus(NULL), m_pMidiBuffer(NULL)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiInsertPlugin[%p] channels=%u",
		this, pInsertType->channels());
#endif

	// Create and attach the custom parameters...
	m_pSendVolumeParam = new qtractorInsertPluginParam(this, 0);
	m_pSendVolumeParam->setName(QObject::tr("Send Volume"));
	m_pSendVolumeParam->setMinValue(0.0f);
	m_pSendVolumeParam->setMaxValue(2.0f);
	m_pSendVolumeParam->setDefaultValue(1.0f);
	m_pSendVolumeParam->setValue(1.0f, false);
	addParam(m_pSendVolumeParam);

	// Setup plugin instance...
	//setChannels(channels());
}


// Destructor.
qtractorMidiInsertPlugin::~qtractorMidiInsertPlugin (void)
{
	// Cleanup plugin instance...
	setChannels(0);
}


// Channel/instance number accessors.
void qtractorMidiInsertPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorPluginType *pType = type();
	if (pType == NULL)
		return;

	// We'll need this globals...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
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

	// Cleanup bus and buffers...
	if (m_pMidiBus) {
		pMidiEngine->removeInputBuffer(m_pMidiBus->alsaPort());
		pMidiEngine->removeBusEx(m_pMidiBus);
		m_pMidiBus->close();
		delete m_pMidiBus;
		m_pMidiBus = NULL;
	}

	if (m_pMidiBuffer) {
		delete m_pMidiBuffer;
		m_pMidiBuffer = NULL;
	}

	// Set new instance number...
	setInstances(iInstances);
	if (iInstances < 1) {
	//	setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiInsertPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	// MIDI bus name -- it must be unique...
	int iBusName = 1;
	const QString& sBusNamePrefix = pType->name() + "_%1";
	QString sBusName = sBusNamePrefix.arg(iBusName);
	while (pMidiEngine->findBus(sBusName)
		|| pMidiEngine->findBusEx(sBusName))
		sBusName = sBusNamePrefix.arg(++iBusName);

	// Create the private audio bus...
	m_pMidiBus = new qtractorMidiBus(pMidiEngine, sBusName,
		qtractorBus::BusMode(qtractorBus::Duplex | qtractorBus::Ex),
		false);

	// Create the private audio bus...
	m_pMidiBuffer = new qtractorMidiBuffer();

	// Add this one to the engine's exo-bus list,
	// for conection persistence purposes...
	pMidiEngine->addBusEx(m_pMidiBus);

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// Open-up private bus...
	if (m_pMidiBus->open())
		pMidiEngine->addInputBuffer(m_pMidiBus->alsaPort(), m_pMidiBuffer);

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Do the actual activation.
void qtractorMidiInsertPlugin::activate (void)
{
}


// Do the actual deactivation.
void qtractorMidiInsertPlugin::deactivate (void)
{
	if (m_pMidiBuffer)
		m_pMidiBuffer->reset();
}


// The main plugin processing procedure.
void qtractorMidiInsertPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
#ifdef CONFIG_DEBUG
	// TODO: Merge buffered input events
	// with list()->midiManager()->events()...
	if (m_pMidiBuffer) {
		snd_seq_event_t *pEv = m_pMidiBuffer->peek();
		while (pEv) {
			// - show event for debug purposes...
			const unsigned long iTime = pEv->time.tick;
			fprintf(stderr, "MIDI Ins %06lu 0x%02x", iTime, pEv->type);
			if (pEv->type == SND_SEQ_EVENT_SYSEX) {
				fprintf(stderr, " sysex {");
				unsigned char *data = (unsigned char *) pEv->data.ext.ptr;
				for (unsigned int i = 0; i < pEv->data.ext.len; ++i)
					fprintf(stderr, " %02x", data[i]);
				fprintf(stderr, " }\n");
			} else {
				for (unsigned int i = 0; i < sizeof(pEv->data.raw8.d); ++i)
					fprintf(stderr, " %3d", pEv->data.raw8.d[i]);
				fprintf(stderr, "\n");
			}
			pEv = m_pMidiBuffer->next();
		}
	}
#endif

	const unsigned short iChannels = channels();
	for (unsigned short i = 0; i < iChannels; ++i)
		::memcpy(ppOBuffer[i], ppIBuffer[i], nframes * sizeof(float));
}


// Pseudo-plugin configuration handlers.
void qtractorMidiInsertPlugin::configure (
	const QString& sKey, const QString& sValue )
{
	if (m_pMidiBus == NULL)
		return;

	qtractorBus::ConnectItem *pItem = new qtractorBus::ConnectItem;

	pItem->index = sValue.section('|', 0, 0).toUShort();

	const QString& sClient = sValue.section('|', 1, 1);
	const QString& sClientName = sClient.section(':', 1);
	if (sClientName.isEmpty()) {
		pItem->clientName = sClient;
	} else {
		pItem->client = sClient.section(':', 0, 0).toInt();
		pItem->clientName = sClientName;
	}

	const QString& sPort = sValue.section('|', 2, 2);
	const QString& sPortName = sPort.section(':', 1);
	if (sPortName.isEmpty()) {
		pItem->portName = sPort;
	} else {
		pItem->port = sPort.section(':', 0, 0).toInt();
		pItem->portName = sPortName;
	}

	const QString& sKeyPrefix = sKey.section('_', 0, 0);
	if (sKeyPrefix == "in")
		m_pMidiBus->inputs().append(pItem);
	else
	if (sKeyPrefix == "out")
		m_pMidiBus->outputs().append(pItem);
	else
		delete pItem;
}


// Pseudo-plugin configuration/state snapshot.
void qtractorMidiInsertPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiInsertPlugin[%p]::freezeConfigs()",	this);
#endif

	clearConfigs();

	freezeConfigs(qtractorBus::Input);
	freezeConfigs(qtractorBus::Output);
}


void qtractorMidiInsertPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiInsertPlugin[%p]::releaseConfigs()", this);
#endif

	clearConfigs();
}


void qtractorMidiInsertPlugin::freezeConfigs ( int iBusMode )
{
	if (m_pMidiBus == NULL)
		return;

	// Save connect items...
	qtractorBus::BusMode busMode = qtractorBus::BusMode(iBusMode);
	const QString sKeyPrefix(busMode & qtractorBus::Input ? "in" : "out");
	int iKey = 0;

	qtractorBus::ConnectList connects;
	m_pMidiBus->updateConnects(busMode, connects);
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


// MIDI specific accessors.
qtractorMidiBus *qtractorMidiInsertPlugin::midiBus (void) const
{
	return m_pMidiBus;
}

qtractorMidiBuffer *qtractorMidiInsertPlugin::midiBuffer (void) const
{
	return m_pMidiBuffer;
}


//----------------------------------------------------------------------------
// qtractorAuxSendPluginType -- Aux-send pseudo-plugin impl.
//

// Factory method (static)
qtractorPlugin *qtractorAuxSendPluginType::createPlugin (
	qtractorPluginList *pList, unsigned short iChannels )
{
	// Check whether it's a valid insert pseudo-plugin...
	qtractorPlugin *pPlugin = NULL;
	qtractorAuxSendPluginType *pAuxSendType = NULL;

	if (iChannels > 0) {
		pAuxSendType = new qtractorAudioAuxSendPluginType(iChannels);
		if (pAuxSendType->open())
			pPlugin = new qtractorAudioAuxSendPlugin(pList, pAuxSendType);
	} else {
		pAuxSendType = new qtractorMidiAuxSendPluginType();
		if (pAuxSendType->open())
			pPlugin = new qtractorMidiAuxSendPlugin(pList, pAuxSendType);
	}

	if (pPlugin == NULL && pAuxSendType)
		delete pAuxSendType;

	return pPlugin;
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
	m_sLabel = "Audio" + m_sName;
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
// qtractorMidiAuxSendPluginType -- MIDI Aux-send pseudo-plugin type.
//

// Derived methods.
bool qtractorMidiAuxSendPluginType::open (void)
{
	// Sanity check...
	const unsigned short iChannels = index();
	if (iChannels > 0)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiAuxSendPluginType[%p]::open() channels=%u",
		this, iChannels);
#endif

	// Pseudo-plugin type names.
	m_sName  = "Aux Send";
	m_sLabel = "Midi" + m_sName;
	m_sLabel.remove(' ');

	// Pseudo-plugin unique identifier.
	m_iUniqueID = qHash(m_sLabel);//^ qHash(iChannels);

	// Pseudo-plugin port counts...
	m_iControlIns  = 1;
	m_iControlOuts = 0;
	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
	m_iMidiIns     = 1;
	m_iMidiOuts    = 1;

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = true;

	// Done.
	return true;
}


void qtractorMidiAuxSendPluginType::close (void)
{
}


// Instance cached-deferred accesors.
const QString& qtractorMidiAuxSendPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		m_sAboutText += QObject::tr("MIDI Aux Send pseudo-plugin.");
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
void qtractorAudioAuxSendPlugin::configure (
	const QString& sKey, const QString& sValue )
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


//----------------------------------------------------------------------------
// qtractorMidiAuxSendPlugin -- MIDI aux-send pseudo-plugin instance.
//

// Constructors.
qtractorMidiAuxSendPlugin::qtractorMidiAuxSendPlugin (
	qtractorPluginList *pList, qtractorAuxSendPluginType *pAuxSendType )
	: qtractorPlugin(pList, pAuxSendType), m_pMidiBus(NULL)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiAuxSendPlugin[%p] channels=%u",
		this, pAuxSendType->channels());
#endif

	// Create and attach the custom parameters...
	m_pSendVolumeParam = new qtractorInsertPluginParam(this, 0);
	m_pSendVolumeParam->setName(QObject::tr("Send Volume"));
	m_pSendVolumeParam->setMinValue(0.0f);
	m_pSendVolumeParam->setMaxValue(2.0f);
	m_pSendVolumeParam->setDefaultValue(0.0f);
	m_pSendVolumeParam->setValue(0.0f, false);
	addParam(m_pSendVolumeParam);
}


// Destructor.
qtractorMidiAuxSendPlugin::~qtractorMidiAuxSendPlugin (void)
{
	// Cleanup plugin instance...
	setChannels(0);
}


// Channel/instance number accessors.
void qtractorMidiAuxSendPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorPluginType *pType = type();
	if (pType == NULL)
		return;

	// We'll need this globals...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
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
	if (m_pMidiBus)
		m_pMidiBus = NULL;

	// Set new instance number...
	setInstances(iInstances);
	if (iInstances < 1) {
	//	setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiAuxSendPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// Try to find a nice default...
	if (m_sMidiBusName.isEmpty()) {
		for (qtractorBus *pBus = pMidiEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				qtractorMidiBus *pMidiBus
					= static_cast<qtractorMidiBus *> (pBus);
				if (pMidiBus) {
					m_sMidiBusName = pMidiBus->busName();
					break;
				}
			}
		}
	}

	// Setup aux-send bus...
	setMidiBusName(m_sMidiBusName);

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// MIDI bus specific accessors.
void qtractorMidiAuxSendPlugin::setMidiBusName ( const QString& sMidiBusName )
{
	if (sMidiBusName.isEmpty())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	qtractorMidiBus *pMidiBus = static_cast<qtractorMidiBus *> (
		pMidiEngine->findOutputBus(sMidiBusName));
	if (pMidiBus) {
		m_pMidiBus = pMidiBus;
		m_sMidiBusName = sMidiBusName;
	//	setConfig("midiBusName", m_sMidiBusName);
	} else {
		m_pMidiBus = NULL;
		m_sMidiBusName.clear();
	//	clearConfigs();
	}

	updateMidiBusName();
}

const QString& qtractorMidiAuxSendPlugin::midiBusName (void) const
{
	return m_sMidiBusName;
}


// Audio bus to appear on plugin lists.
void qtractorMidiAuxSendPlugin::updateMidiBusName (void) const
{
	const QString& sText = (m_pMidiBus ? m_sMidiBusName : type()->name());
	QListIterator<qtractorPluginListItem *> iter(items());
	while (iter.hasNext())
		iter.next()->setText(sText);
}


// The main plugin processing procedure.
void qtractorMidiAuxSendPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	const unsigned short iChannels = channels();
	for (unsigned short i = 0; i < iChannels; ++i)
		::memcpy(ppOBuffer[i], ppIBuffer[i], nframes * sizeof(float));
}


// Do the actual activation.
void qtractorMidiAuxSendPlugin::activate (void)
{
}


// Do the actual deactivation.
void qtractorMidiAuxSendPlugin::deactivate (void)
{
}


// Pseudo-plugin configuration handlers.
void qtractorMidiAuxSendPlugin::configure (
	const QString& sKey, const QString& sValue )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiAuxSendPlugin[%p]::configure()", this);
#endif

	if (sKey == "midiBusName")
		setMidiBusName(sValue);
}


// Pseudo-plugin configuration/state snapshot.
void qtractorMidiAuxSendPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiAuxSendPlugin[%p]::freezeConfigs()", this);
#endif

	clearConfigs();

	setConfig("midiBusName", m_sMidiBusName);
}


void qtractorMidiAuxSendPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiAuxSendPlugin[%p]::releaseConfigs()", this);
#endif

	clearConfigs();
}


// end of qtractorInsertPlugin.cpp
