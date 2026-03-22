// qtractorMidiControlPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiControlPlugin.h"

#include "qtractorSession.h"
#include "qtractorMidiEngine.h"

#include "qtractorCurve.h"


// Config keys.
static const char *ControlTypeKey        = "ControlType";
static const char *ControlParamKey       = "ControlParam";
static const char *ControlChannelKey     = "ControlChannel";
static const char *ControlLogarithmicKey = "ControlLogarithmic";
static const char *ControlInvertKey      = "ControlInvert";
static const char *ControlBipolarKey     = "ControlBipolar";
static const char *ControlAutoConnectKey = "ControlAutoConnect";
static const char *ControlSendKey        = "ControlSend";


//----------------------------------------------------------------------------
// qtractorMidiControlPluginType -- Insert pseudo-plugin type impl.
//

// Factory method (static)
qtractorPlugin *qtractorMidiControlPluginType::createPlugin (
	qtractorPluginList *pList )
{
	qtractorPlugin *pPlugin = nullptr;
	qtractorMidiControlPluginType *pMidiControlType
		= new qtractorMidiControlPluginType();
	if (pMidiControlType->open())
		pPlugin = new qtractorMidiControlPlugin(pList, pMidiControlType);

	return pPlugin;
}


// Derived methods.
bool qtractorMidiControlPluginType::open (void)
{
	// Sanity check...
	const unsigned short iChannels = index();
	if (iChannels > 0)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControlPluginType[%p]::open() channels=%u",
		this, iChannels);
#endif

	// Pseudo-plugin type names.
	m_sName  = QObject::tr("Control (MIDI)");
	m_sLabel = "MidiControl";
//	m_sLabel.remove(' ');

	// Pseudo-plugin unique identifier.
	m_iUniqueID = qHash(m_sLabel);//^ qHash(iChannels);

	// Pseudo-plugin port counts...
	m_iControlIns  = 1;
	m_iControlOuts = 0;
	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = true;

	// Done.
	return true;
}


void qtractorMidiControlPluginType::close (void)
{
}


// Instance cached-deferred accessors.
const QString& qtractorMidiControlPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		m_sAboutText += QObject::tr("MIDI Controller Send pseudo-plugin");
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_WEBSITE;
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_COPYRIGHT;
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------------
// qtractorMidiControlPlugin -- MIDI Controller pseudo-plugin instance.
//

// Constructors.
qtractorMidiControlPlugin::qtractorMidiControlPlugin (
	qtractorPluginList *pList, qtractorMidiControlPluginType *pMidiControlType )
	: qtractorPlugin(pList, pMidiControlType), m_pMidiBus(nullptr),
		m_controlType(qtractorMidiEvent::CONTROLLER),
			m_iControlParam(0), m_iControlChannel(0),
			m_bControlLogarithmic(false),
			m_bControlInvert(false),
			m_bControlBipolar(false),
			m_bControlAutoConnect(false)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControlPlugin[%p] channels=%u",
		this, pMidiControlType->channels());
#endif

	// Create and attach the custom parameters...
	m_pControlValueParam = new Param(this, 0);
	m_pControlValueParam->setName(QObject::tr("Value"));
	m_pControlValueParam->setMinValue(0.0f);
	m_pControlValueParam->setMaxValue(1.0f);
	m_pControlValueParam->setDefaultValue(0.0f);
	m_pControlValueParam->setValue(0.0f, false);
	addParam(m_pControlValueParam);

	// Setup plugin instance...
	//setChannels(channels());
}


// Destructor.
qtractorMidiControlPlugin::~qtractorMidiControlPlugin (void)
{
	// Cleanup plugin instance...
	setChannels(0);
}


// Channel/instance number accessors.
void qtractorMidiControlPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorPluginType *pType = type();
	if (pType == nullptr)
		return;

	// We'll need this globals...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	// Estimate the (new) number of instances...
	const unsigned short iInstances
		= pType->instances(iChannels, list()->isMidi());
	// Now see if instance and channel count changed anyhow...
	if (iInstances == instances() && iChannels == channels())
		return;

	// Gotta go for a while...
	const bool bActivated = isActivated();
	setChannelsActivated(iChannels, false);

	// Cleanup bus...
	if (m_pMidiBus) {
		pMidiEngine->removeBusEx(m_pMidiBus);
		m_pMidiBus->close();
		delete m_pMidiBus;
		m_pMidiBus = nullptr;
	}

	// Set new instance number...
	setInstances(iInstances);
	if (iInstances < 1) {
	//	setChannelsActivated(iChannels, bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControlPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	// MIDI bus name -- it must be unique...
	int iBusName = 1;
	const QString sBusNamePrefix("Control_%1");
	QString sBusName = label(); // HACK: take previously saved label.
	if (sBusName.isEmpty() || sBusName == pType->label())
		sBusName = sBusNamePrefix.arg(iBusName);
	while (pMidiEngine->findBus(sBusName)
		|| pMidiEngine->findBusEx(sBusName))
		sBusName = sBusNamePrefix.arg(++iBusName);

	setLabel(sBusName); // HACK: remember this label.

	// Create the private audio bus...
	m_pMidiBus = new qtractorMidiBus(pMidiEngine, sBusName,
		qtractorBus::BusMode(qtractorBus::Output | qtractorBus::Ex),
		false);

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
	m_pMidiBus->open();

	// (Re)activate instance if necessary...
	setChannelsActivated(iChannels, bActivated);
}


// Do the actual activation.
void qtractorMidiControlPlugin::activate (void)
{
}


// Do the actual deactivation.
void qtractorMidiControlPlugin::deactivate (void)
{
}


// The main plugin processing procedure.
void qtractorMidiControlPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	qtractorPlugin::process(ppIBuffer, ppOBuffer, nframes);
}


// Pseudo-plugin configuration handlers.
void qtractorMidiControlPlugin::configure (
	const QString& sKey, const QString& sValue )
{
	if (m_pMidiBus == nullptr)
		return;

	if (sKey == ControlTypeKey) {
		setControlType(qtractorMidiControl::typeFromText(sValue));
		return;
	}

	if (sKey == ControlParamKey) {
		setControlParam(sValue.toUShort());
		return;
	}

	if (sKey == ControlChannelKey) {
		setControlChannel(sValue.toUShort());
		return;
	}

	if (sKey == ControlLogarithmicKey) {
		setControlLogarithmic(sValue.toInt() > 0);
		return;
	}

	if (sKey == ControlInvertKey) {
		setControlInvert(sValue.toInt() > 0);
		return;
	}

	if (sKey == ControlBipolarKey) {
		setControlBipolar(sValue.toInt() > 0);
		return;
	}

	if (sKey == ControlAutoConnectKey) {
		setControlAutoConnect(sValue.toInt() > 0);
		return;
	}

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
	if (sKeyPrefix == ControlSendKey)
		m_pMidiBus->outputs().append(pItem);
	else
		delete pItem;
}


// Pseudo-plugin configuration/state snapshot.
void qtractorMidiControlPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControlPlugin[%p]::freezeConfigs()", this);
#endif

	clearConfigs();

	setConfig(ControlTypeKey, qtractorMidiControl::textFromType(m_controlType));
	setConfig(ControlParamKey, QString::number(int(m_iControlParam)));
	setConfig(ControlChannelKey, QString::number(int(m_iControlChannel)));
	setConfig(ControlLogarithmicKey, QString::number(int(m_bControlLogarithmic)));
	setConfig(ControlInvertKey, QString::number(int(m_bControlInvert)));
	setConfig(ControlBipolarKey, QString::number(int(m_bControlBipolar)));
	setConfig(ControlAutoConnectKey, QString::number(int(m_bControlAutoConnect)));

	if (m_pMidiBus == nullptr)
		return;

	// Save connect items...
	const QString sKeyPrefix(ControlSendKey);
	int iKey = 0;

	qtractorBus::ConnectList connects;
	m_pMidiBus->updateConnects(qtractorBus::Output, connects);
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


void qtractorMidiControlPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControlPlugin[%p]::releaseConfigs()", this);
#endif

	qtractorPlugin::clearConfigs();
}


// MIDI specific accessors.
qtractorMidiBus *qtractorMidiControlPlugin::midiBus (void) const
{
	return m_pMidiBus;
}


// Accessors.
void qtractorMidiControlPlugin::setControlType (
	qtractorMidiControl::ControlType ctype )
{
	m_controlType = ctype;
}

qtractorMidiControl::ControlType qtractorMidiControlPlugin::controlType (void) const
{
	return m_controlType;
}


void qtractorMidiControlPlugin::setControlParam ( unsigned short iParam )
{
	m_iControlParam = iParam;
}

unsigned short qtractorMidiControlPlugin::controlParam (void) const
{
	return m_iControlParam;
}


void qtractorMidiControlPlugin::setControlChannel ( unsigned short iChannel )
{
	m_iControlChannel = iChannel;
}

unsigned short qtractorMidiControlPlugin::controlChannel (void) const
{
	return m_iControlChannel;
}


void qtractorMidiControlPlugin::setControlLogarithmic ( bool bLogarithmic )
{
	m_bControlLogarithmic = bLogarithmic;
}

bool qtractorMidiControlPlugin::isControlLogarithmic (void) const
{
	return m_bControlLogarithmic;
}


void qtractorMidiControlPlugin::setControlInvert ( bool bInvert )
{
	m_bControlInvert = bInvert;
}

bool qtractorMidiControlPlugin::isControlInvert (void) const
{
	return m_bControlInvert;
}


void qtractorMidiControlPlugin::setControlBipolar ( bool bBipolar )
{
	m_bControlBipolar = bBipolar;

	updateControlBipolar();
}

bool qtractorMidiControlPlugin::isControlBipolar (void) const
{
	return m_bControlBipolar;
}


void qtractorMidiControlPlugin::updateControlBipolar (void)
{
	const float fOldMinValue
		= m_pControlValueParam->minValue();
	const float fNewMinValue
		= (m_bControlBipolar ? -1.0f : 0.0f);

	if (qAbs(fOldMinValue - fNewMinValue) > 0.0f) {
		const float fValue = m_pControlValueParam->value();
		m_pControlValueParam->setMinValue(fNewMinValue);
		m_pControlValueParam->setValue(fValue, true);
		qtractorCurve *pCurve = m_pControlValueParam->subject()->curve();
		if (pCurve) {
			qtractorCurve::Node *pNode = pCurve->nodes().first();
			while (pNode) {
				const float fValue = pNode->value;
				if (fNewMinValue < 0.0f)
					pNode->value = 2.0f * (fValue - 0.5f);
				else
					pNode->value = 0.5f * (fValue + 1.0f);
				pNode = pNode->next();
			}
			pCurve->update();
		}
	}
}


void qtractorMidiControlPlugin::setControlAutoConnect ( bool bAutoConnect )
{
	m_bControlAutoConnect = bAutoConnect;

	updateControlAutoConnect();
}

bool qtractorMidiControlPlugin::isControlAutoConnect (void) const
{
	return m_bControlAutoConnect;
}


void qtractorMidiControlPlugin::updateControlAutoConnect (void)
{
	if (m_pMidiBus == nullptr)
		return;

	// We'll need this globals...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	qtractorMidiBus *pIControlBus = nullptr;
	if (pMidiEngine->isControlBus())
		pIControlBus = pMidiEngine->controlBus_in();
	if (pIControlBus == nullptr) {
		for (qtractorBus *pBus = pMidiEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Input) {
				pIControlBus = static_cast<qtractorMidiBus *> (pBus);
				break;
			}
		}
	}

	if (pIControlBus == nullptr)
		return;

	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

	snd_seq_port_subscribe_t *pAlsaSubs;
	snd_seq_addr_t seq_addr;
	snd_seq_port_subscribe_alloca(&pAlsaSubs);

	seq_addr.client = pMidiEngine->alsaClient();
	seq_addr.port   = m_pMidiBus->alsaPort();
	snd_seq_port_subscribe_set_sender(pAlsaSubs, &seq_addr);

	seq_addr.client = pMidiEngine->alsaClient();
	seq_addr.port   = pIControlBus->alsaPort();
	snd_seq_port_subscribe_set_dest(pAlsaSubs, &seq_addr);

	if (m_bControlAutoConnect)
		snd_seq_subscribe_port(pAlsaSeq, pAlsaSubs);
	else
		snd_seq_unsubscribe_port(pAlsaSeq, pAlsaSubs);
}


// Override title/name caption.
QString qtractorMidiControlPlugin::title (void) const
{
	QString sTitle;

	if (m_pMidiBus && qtractorPlugin::alias().isEmpty()) {
		sTitle = QObject::tr("%1 (MIDI)").arg(m_pMidiBus->busName());
		sTitle.replace('_', ' ');
	} else {
		sTitle = qtractorPlugin::title();
	}

	return sTitle;
}


//----------------------------------------------------------------------------
// qtractorMidiControlPlugin::Param -- MIDI Controller plugin control input port.
//

// Logarithmic range hint predicate method.
bool qtractorMidiControlPlugin::Param::isLogarithmic (void) const
{
	qtractorMidiControlPlugin *pMidiControlPlugin
		= static_cast<qtractorMidiControlPlugin *> (plugin());
	if (pMidiControlPlugin)
		return pMidiControlPlugin->isControlLogarithmic();
	else
		return false;
}


// Parameter update method (virtual).
void qtractorMidiControlPlugin::updateParam (
	qtractorPlugin::Param *pParam, float fValue, bool bUpdate )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlPlugin::Param::updateParam(%p, %g, %d)",
		pParam, fValue, int(bUpdate));
#endif

	qtractorPlugin::updateParam(pParam, fValue, bUpdate);

	qtractorMidiControlPlugin::Param *pControlValueParam
		= static_cast<qtractorMidiControlPlugin::Param *> (pParam);
	if (pControlValueParam == m_pControlValueParam) {
		if (m_pMidiBus && isActivated()) {
			const qtractorMidiControl::ControlType ctype
				= controlType();
			const unsigned short iChannel
				= controlChannel();
			const unsigned short iParam
				= controlParam();
			const bool bLogarithmic
				= isControlLogarithmic();
			const bool bInvert
				= isControlInvert();
			const bool bBipolar
				= isControlBipolar();
			unsigned short iScale = 0x7f;
			if (ctype == qtractorMidiEvent::PITCHBEND ||
				ctype == qtractorMidiEvent::CONTROL14 ||
				ctype == qtractorMidiEvent::REGPARAM  ||
				ctype == qtractorMidiEvent::NONREGPARAM)
				iScale = 0x3fff;
			float fScale = (bBipolar ? 0.5f * (fValue + 1.0f) : fValue);
			if (bLogarithmic) {
				if (bBipolar) {
					if (fScale > 0.5f) {
						fScale = 2.0f * (fScale - 0.5f);
						fScale = 0.5f + 0.5f * ::cbrtf(fScale);
					} else {
						fScale = 2.0f * (0.5f - fScale);
						fScale = 0.5f - 0.5f * ::cbrtf(fScale);
					}
				} else {
					fScale = ::cbrtf(fScale);
				}
			}
			unsigned short iValue = ::rintf(float(iScale) * fScale);
			if (iValue > iScale)
				iValue = iScale;
			if (bInvert)
				iValue = (iScale - iValue);
			m_pMidiBus->sendEvent(ctype, iChannel, iParam, iValue);
		}
	}
}


// end of qtractorMidiControlPlugin.cpp
