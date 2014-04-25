// qtractorMidiControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2009, gizzmo aka Mathias Krause. 

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
#include "qtractorMidiControl.h"
#include "qtractorMidiEngine.h"

#include "qtractorMidiControlObserver.h"
#include "qtractorMidiControlObserverForm.h"

#include "qtractorTrackCommand.h"

#include "qtractorDocument.h"

#include <QFile>

#include <QDomDocument>

// Translatable macro contextualizer.
#undef  _TR
#define _TR(x) QT_TR_NOOP(x)


#include <math.h>

// Possible cube root optimization.
// (borrowed from metamerist.com)
static inline float cbrtf2 ( float x )
{
#ifdef CONFIG_FLOAT32
	// Avoid strict-aliasing optimization (gcc -O2).
	union { float f; int i; } u;
	u.f = x;
	u.i = (u.i / 3) + 710235478;
	return u.f;
#else
	return ::cbrtf(x);
#endif
}

static inline float cubef2 ( float x )
{
	return x * x * x;
}


//----------------------------------------------------------------------
// qtractorMidiControl -- MIDI control map (singleton).
//

// Kind of singleton reference.
qtractorMidiControl *qtractorMidiControl::g_pMidiControl = NULL;

// Constructor.
qtractorMidiControl::qtractorMidiControl (void)
{
	// Pseudo-singleton reference setup.
	g_pMidiControl = this;

	// Default controller mapping...
	clear();
}


// Destructor.
qtractorMidiControl::~qtractorMidiControl (void)
{
	// Pseudo-singleton reference shut-down.
	g_pMidiControl = NULL;
}


// Kind of singleton reference.
qtractorMidiControl *qtractorMidiControl::getInstance (void)
{
	return g_pMidiControl;
}

// Clear control map (reset to default).
void qtractorMidiControl::clear (void)
{
	m_controlMap.clear();

#ifdef TEST_USx2y
	// JLCooper faders (as in US-224)...
	mapChannelControllerParam(15, TrackGain, 0x40); // No feedback.
#endif
#ifdef TEST_BCx2000
	// Generic track feedback controllers (eg. Behringer BCx2000)...
	mapChannelParamController(7, TrackGain, 0, true);
	mapChannelParamController(10, TrackPanning, 0, true);
	mapChannelParamController(20, TrackMute, 0, true);
#endif

	m_observerMap.clear();
}


// Insert new controller mappings.
void qtractorMidiControl::mapChannelParam (
	ControlType ctype, unsigned short iChannel, unsigned short iParam,
	Command command, int iTrack, bool bFeedback )
{
	m_controlMap.insert(
		MapKey(ctype, iChannel, iParam),
		MapVal(command, iTrack, bFeedback));
}

void qtractorMidiControl::mapChannelTrack (
	ControlType ctype, unsigned short iParam,
	Command command, int iTrack, bool bFeedback )
{
	mapChannelParam(
		ctype, TrackParam, iParam, command, iTrack, bFeedback);
}

void qtractorMidiControl::mapChannelParamTrack (
	ControlType ctype, unsigned short iChannel, unsigned short iParam,
	Command command, int iTrack, bool bFeedback )
{
	mapChannelParam(
		ctype, iChannel, iParam | TrackParam, command, iTrack, bFeedback);
}


// Remove existing controller mapping.
void qtractorMidiControl::unmapChannelParam (
	ControlType ctype, unsigned short iChannel, unsigned short iParam )
{
	m_controlMap.remove(MapKey(ctype, iChannel, iParam));
}


// Check if given channel, param triplet is currently mapped.
bool qtractorMidiControl::isChannelParamMapped (
	ControlType ctype, unsigned short iChannel, unsigned short iParam ) const
{
	return m_controlMap.contains(MapKey(ctype, iChannel, iParam));
}


// Resend all (tracks) controllers
// (eg. session initialized, track added/removed)
void qtractorMidiControl::sendAllControllers ( int iFirstTrack ) const
{
	if (iFirstTrack < 0)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControl::sendAllControllers(%d)", iFirstTrack);
#endif

	// 1. Walk through midi controller map...
	ControlMap::ConstIterator it = m_controlMap.constBegin();
	const ControlMap::ConstIterator& it_end = m_controlMap.constEnd();
	for ( ; it != it_end; ++it) {
		const MapVal& val = it.value();
		if (val.isFeedback()) {
			const MapKey& key = it.key();
			unsigned short iChannel = key.channel();
			unsigned short iParam = key.param();
			if (key.isParamTrack())
				iParam &= TrackParamMask;
			int iTrack = 0;
			for (qtractorTrack *pTrack = pSession->tracks().first();
					pTrack; pTrack = pTrack->next()) {
				if (iTrack >= iFirstTrack) {
					if (key.isChannelTrack())
						sendTrackController(key.type(),
							pTrack, val.command(), iTrack, iParam);
					else if (key.isParamTrack())
						sendTrackController(key.type(),
							pTrack, val.command(), iChannel, iParam + iTrack);
					else if (val.track() == iTrack) {
						sendTrackController(key.type(),
							pTrack, val.command(), iChannel, iParam);
						break; // Bail out from inner track loop.
					}
				}
				++iTrack;
			}
		}
	}

	// 2. Walk through midi observer map...
	if (iFirstTrack == 0) {
		ObserverMap::ConstIterator iter = m_observerMap.constBegin();
		const ObserverMap::ConstIterator& iter_end = m_observerMap.constEnd();
		for ( ; iter != iter_end; ++iter) {
			qtractorMidiControlObserver *pMidiObserver = iter.value();
			if (pMidiObserver->isFeedback()) {
				sendController(
					pMidiObserver->type(),
					pMidiObserver->channel(),
					pMidiObserver->param(),
					pMidiObserver->midiValue());
			}
		}
	}
}


// Find incoming controller event map.
qtractorMidiControl::ControlMap::Iterator
qtractorMidiControl::findEvent ( const qtractorCtlEvent& ctle )
{
	// Check if controller map includes this event...
	ControlMap::Iterator it = m_controlMap.begin();
	const ControlMap::Iterator& it_end = m_controlMap.end();
	for ( ; it != it_end; ++it) {
		const MapKey& key = it.key();
		if (key.match(ctle.type(), ctle.channel(), ctle.param()))
			break;
	}
	return it;
}


// Process incoming controller event.
bool qtractorMidiControl::processEvent ( const qtractorCtlEvent& ctle )
{
	bool bResult = false;

	// Find whether there's any observer assigned...
	qtractorMidiControlObserver *pMidiObserver
		= findMidiObserver(ctle.type(), ctle.channel(), ctle.param());
	if (pMidiObserver) {
		pMidiObserver->setMidiValue(ctle.value());
		bResult = true;
	} else {
		qtractorMidiControlObserverForm *pMidiObserverForm
			= qtractorMidiControlObserverForm::getInstance();
		if (pMidiObserverForm)
			pMidiObserverForm->processEvent(ctle);
	}

	// Find incoming controller event map tuple.
	ControlMap::Iterator it = findEvent(ctle);
	// Is there one mapped, indeed?
	if (it == m_controlMap.end())
		return bResult;

	// Find the track by number...
	const MapKey& key = it.key();
	MapVal& val = it.value();

	int iTrack = val.track();
	if (key.isChannelTrack()) {
		iTrack += int(ctle.channel());
	}
	else
	if (key.isParamTrack()) {
		const unsigned short iKeyParam = key.param() & TrackParamMask;
		const unsigned short iCtlParam = ctle.param();
		if (iCtlParam >= iKeyParam)
			iTrack += iCtlParam - iKeyParam;
		else
			return bResult;
	}

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return bResult;

	qtractorTrack *pTrack = pSession->tracks().at(iTrack);
	if (pTrack == NULL)
		return bResult;

	Scale scale(ctle.type());

	float fValue;
	switch (val.command()) {
	case TRACK_GAIN:
		fValue = scale.valueFromMidi(ctle.value());
		if (pTrack->trackType() == qtractorTrack::Audio)
			fValue = ::cubef2(fValue);
		if (val.sync(fValue, pTrack->gain())) {
			bResult = pSession->execute(
				new qtractorTrackGainCommand(pTrack, val.value(), true));
		}
		break;
	case TRACK_PANNING:
		fValue = scale.valueSignedFromMidi(ctle.value());
		if (val.sync(fValue, pTrack->panning())) {
			bResult = pSession->execute(
				new qtractorTrackPanningCommand(pTrack, val.value(), true));
		}
		break;
	case TRACK_MONITOR:
		fValue = scale.valueToggledFromMidi(ctle.value());
		if (val.sync(fValue, (pTrack->isMonitor() ? 1.0f : 0.0f))) {
			bResult = pSession->execute(
				new qtractorTrackMonitorCommand(pTrack, val.value(), true));
		}
		break;
	case TRACK_RECORD:
		fValue = scale.valueToggledFromMidi(ctle.value());
		if (val.sync(fValue, (pTrack->isRecord() ? 1.0f : 0.0f))) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Record, val.value(), true));
		}
		break;
	case TRACK_MUTE:
		fValue = scale.valueToggledFromMidi(ctle.value());
		if (val.sync(fValue, (pTrack->isMute() ? 1.0f : 0.0f))) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Mute, val.value(), true));
		}
		break;
	case TRACK_SOLO:
		fValue = scale.valueToggledFromMidi(ctle.value());
		if (val.sync(fValue, (pTrack->isSolo() ? 1.0f : 0.0f))) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Solo, val.value(), true));
		}
		break;
	default:
		break;
	}

	return bResult;
}


// Process incoming command.
void qtractorMidiControl::processTrackCommand (
	Command command, int iTrack, float fValue, bool bCubic )
{
	sendTrackController(iTrack, command, fValue, bCubic);
}


void qtractorMidiControl::processTrackCommand (
	Command command, int iTrack, bool bValue )
{
	sendTrackController(iTrack, command, (bValue ? 1.0f : 0.0f), false);
}


// Further processing of outgoing midi controller messages
void qtractorMidiControl::sendTrackController (
	int iTrack, Command command, float fValue, bool bCubic )
{
	// Search for the command and parameter in controller map...
	ControlMap::Iterator it = m_controlMap.begin();
	const ControlMap::Iterator& it_end = m_controlMap.end();
	for ( ; it != it_end; ++it) {
		const MapKey& key = it.key();
		MapVal& val = it.value();
		if (val.command() == command) {
			val.syncReset();
			if (!val.isFeedback())
				continue;
			// Convert/normalize value...
			const ControlType ctype = key.type();
			const Scale scale(ctype);
			unsigned short iValue = 0;
			switch (command) {
			case TRACK_GAIN:
				if (bCubic) fValue = ::cbrtf2(fValue);
				iValue = scale.midiFromValue(fValue);
				break;
			case TRACK_PANNING:
				iValue = scale.midiFromValueSigned(fValue);
				break;
			case TRACK_MONITOR:
			case TRACK_RECORD:
			case TRACK_MUTE:
			case TRACK_SOLO:
				iValue = scale.midiFromValueToggled(fValue);
				// Fall thru...
			default:
				break;
			}
			// Now send the message out...
			const unsigned short iParam = (key.param() & TrackParamMask);
			if (key.isChannelTrack())
				sendController(key.type(), iTrack, iParam, iValue);
			else
			if (key.isParamTrack())
				sendController(key.type(), key.channel(), iParam + iTrack, iValue);
			else
			if (val.track() == iTrack)
				sendController(key.type(), key.channel(), iParam, iValue);
		}
	}
}


void qtractorMidiControl::sendTrackController (
	ControlType ctype, qtractorTrack *pTrack,
	Command command, unsigned short iChannel, unsigned short iParam ) const
{
	const Scale scale(ctype);
	unsigned short iValue = 0;

	switch (command) {
	case TRACK_GAIN:
		if (pTrack->trackType() == qtractorTrack::Audio)
			iValue = scale.midiFromValue(::cbrtf2(pTrack->gain()));
		else
			iValue = scale.midiFromValue(pTrack->gain());
		break;
	case TRACK_PANNING:
		iValue = scale.midiFromValueSigned(pTrack->panning());
		break;
	case TRACK_MONITOR:
		iValue = scale.midiFromValueToggled(pTrack->isMonitor());
		break;
	case TRACK_RECORD:
		iValue = scale.midiFromValueToggled(pTrack->isRecord());
		break;
	case TRACK_MUTE:
		iValue = scale.midiFromValueToggled(pTrack->isMute());
		break;
	case TRACK_SOLO:
		iValue = scale.midiFromValueToggled(pTrack->isSolo());
		break;
	default:
		break;
	}

	sendController(ctype, iChannel, iParam, iValue);
}


// Send this value out to midi bus.
void qtractorMidiControl::sendController (
	ControlType ctype, unsigned short iChannel,
	unsigned short iParam, unsigned short iValue ) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	qtractorMidiBus *pMidiBus = pMidiEngine->controlBus_out();
	if (pMidiBus == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControl::sendController(0x%02x, %u, %u, %d)",
		int(ctype), iChannel, iParam, iValue);
#endif

	pMidiBus->sendEvent(ctype, iChannel, iParam, iValue);
}


// Insert/remove observer mappings.
void qtractorMidiControl::mapMidiObserver (
	qtractorMidiControlObserver *pMidiObserver )
{
	const MapKey key(
		pMidiObserver->type(),
		pMidiObserver->channel(),
		pMidiObserver->param());

	m_observerMap.insert(key, pMidiObserver);
}

void qtractorMidiControl::unmapMidiObserver (
	qtractorMidiControlObserver *pMidiObserver )
{
	const MapKey key(
		pMidiObserver->type(),
		pMidiObserver->channel(),
		pMidiObserver->param());

	m_observerMap.remove(key);
}

// Observer map predicate.
bool qtractorMidiControl::isMidiObserverMapped (
	qtractorMidiControlObserver *pMidiObserver ) const
{
	return (findMidiObserver(
		pMidiObserver->type(),
		pMidiObserver->channel(),
		pMidiObserver->param()) == pMidiObserver);
}


// Observer finder.
qtractorMidiControlObserver *qtractorMidiControl::findMidiObserver (
	ControlType ctype, unsigned short iChannel, unsigned short iParam) const
{
	return m_observerMap.value(MapKey(ctype, iChannel, iParam), NULL);
}


//----------------------------------------------------------------------
// qtractorMidiControl::Document -- MIDI control document.
//

class qtractorMidiControl::Document : public qtractorDocument
{
public:

	// Constructor.
	Document(QDomDocument *pDocument, qtractorMidiControl *pMidiControl)
		: qtractorDocument(pDocument, "midi-control"),
			m_pMidiControl(pMidiControl) {}

	// Elemental loader/savers...
	bool loadElement(QDomElement *pElement)
		{ return m_pMidiControl->loadElement(this, pElement); }
	bool saveElement(QDomElement *pElement)
		{ return m_pMidiControl->saveElement(this, pElement); }

private:

	// Instance variables.
	qtractorMidiControl *m_pMidiControl;
};


// Load controller rules.
bool qtractorMidiControl::loadElement (
	qtractorDocument * /*pDocument*/, QDomElement *pElement )
{
	for (QDomNode nItem = pElement->firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		// Convert item node to element...
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		if (eItem.tagName() == "map") {
			ControlType ctype
				= typeFromText(eItem.attribute("type"));
			const unsigned short iChannel
				= keyFromText(eItem.attribute("channel"));
			unsigned short iParam = 0;
			const bool bOldMap = (ctype == ControlType(0));
			bool bOldTrackParam = false;
			if (bOldMap) {
				ctype  = qtractorMidiEvent::CONTROLLER;
				iParam = keyFromText(eItem.attribute("controller"));
				bOldTrackParam = bool(iParam & TrackParam);
			} else {
				iParam = eItem.attribute("param").toUShort();
				if (qtractorDocument::boolFromText(eItem.attribute("track")))
					iParam |= TrackParam;
			}
			Command command = Command(0);
			int iTrack = 0;
			bool bFeedback = false;
			for (QDomNode nVal = eItem.firstChild();
					!nVal.isNull();
						nVal = nVal.nextSibling()) {
				// Convert value node to element...
				QDomElement eVal = nVal.toElement();
				if (eVal.isNull())
					continue;
				if (eVal.tagName() == "command")
					command = commandFromText(eVal.text());
				else
				if (eVal.tagName() == "track")
					iTrack = eVal.text().toInt();
				else
				if (eVal.tagName() == "feedback")
					bFeedback = qtractorDocument::boolFromText(eVal.text());
				else
				if (eVal.tagName() == "param" && bOldMap) {
					iTrack = eVal.text().toInt();
					if (bOldTrackParam) {
						iParam += iTrack;
						iTrack  = 0;
					}
				}
			}
			m_controlMap.insert(
				MapKey(ctype, iChannel, iParam),
				MapVal(command, iTrack, bFeedback));
		}
	}

	return true;
}


// Save controller rules.
bool qtractorMidiControl::saveElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	ControlMap::ConstIterator it = m_controlMap.constBegin();
	const ControlMap::ConstIterator& it_end = m_controlMap.constEnd();
	for ( ; it != it_end; ++it) {
		const MapKey& key = it.key();
		const MapVal& val = it.value();
		QDomElement eItem = pDocument->document()->createElement("map");
		eItem.setAttribute("type",
			textFromType(key.type()));
		eItem.setAttribute("channel",
			textFromKey(key.channel()));
		eItem.setAttribute("param",
			QString::number(key.param() & TrackParamMask));
		eItem.setAttribute("track",
			qtractorDocument::textFromBool(key.isParamTrack()));
		pDocument->saveTextElement("command",
			textFromCommand(val.command()), &eItem);
		pDocument->saveTextElement("track",
			QString::number(val.track()), &eItem);
		pDocument->saveTextElement("feedback",
			qtractorDocument::textFromBool(val.isFeedback()), &eItem);
		pElement->appendChild(eItem);
	}

	return true;
}


// Document file methods.
bool qtractorMidiControl::loadDocument ( const QString& sFilename )
{
	QDomDocument doc("qtractorMidiControl");
	return qtractorMidiControl::Document(&doc, this).load(sFilename);
}

bool qtractorMidiControl::saveDocument ( const QString& sFilename )
{
	QDomDocument doc("qtractorMidiControl");
	return qtractorMidiControl::Document(&doc, this).save(sFilename);
}


unsigned short qtractorMidiControl::keyFromText ( const QString& sText )
{
	if (sText == "*" || sText == "TrackParam" || sText.isEmpty())
		return TrackParam;
	else
		return sText.toUShort();
}

QString qtractorMidiControl::textFromKey ( unsigned short iKey )
{
	if (iKey & TrackParam)
		return "*"; // "TrackParam";
	else
		return QString::number(iKey);
}


// Load meter controllers (MIDI).
void qtractorMidiControl::loadControllers (
	QDomElement *pElement, Controllers& controllers )
{
	qDeleteAll(controllers);
	controllers.clear();

	for (QDomNode nController = pElement->firstChild();
			!nController.isNull(); nController = nController.nextSibling()) {
		// Convert node to element, if any.
		QDomElement eController = nController.toElement();
		if (eController.isNull())
			continue;
		// Check for controller item...
		if (eController.tagName() == "controller") {
			Controller *pController = new Controller;
			pController->name  = eController.attribute("name");
			pController->index = eController.attribute("index").toULong();
			pController->ctype = typeFromText(eController.attribute("type"));
			for (QDomNode nProp = eController.firstChild();
					!nProp.isNull(); nProp = nProp.nextSibling()) {
				// Convert node to element, if any.
				QDomElement eProp = nProp.toElement();
				if (eProp.isNull())
					continue;
				// Check for property item...
				if (eProp.tagName() == "channel")
					pController->channel = eProp.text().toUShort();
				else
				if (eProp.tagName() == "param")
					pController->param = eProp.text().toUShort();
				else
				if (eProp.tagName() == "logarithmic")
					pController->logarithmic = qtractorDocument::boolFromText(eProp.text());
				else
				if (eProp.tagName() == "feedback")
					pController->feedback = qtractorDocument::boolFromText(eProp.text());
				else
				if (eProp.tagName() == "invert")
					pController->invert = qtractorDocument::boolFromText(eProp.text());
				if (eProp.tagName() == "hook")
					pController->hook = qtractorDocument::boolFromText(eProp.text());
			}
			controllers.append(pController);
		}
	}
}


// Save meter controllers (MIDI).
void qtractorMidiControl::saveControllers ( qtractorDocument *pDocument,
	QDomElement *pElement, const Controllers& controllers )
{
	QListIterator<Controller *> iter(controllers);
	while (iter.hasNext()) {
		Controller *pController = iter.next();
		QDomElement eController = pDocument->document()->createElement("controller");
		eController.setAttribute("name", pController->name);
		eController.setAttribute("index", QString::number(pController->index));
		eController.setAttribute("type", textFromType(pController->ctype));
		pDocument->saveTextElement("channel",
			QString::number(pController->channel), &eController);
		pDocument->saveTextElement("param",
			QString::number(pController->param), &eController);
		pDocument->saveTextElement("logarithmic",
			qtractorDocument::textFromBool(pController->logarithmic), &eController);
		pDocument->saveTextElement("feedback",
			qtractorDocument::textFromBool(pController->feedback), &eController);
		pDocument->saveTextElement("invert",
			qtractorDocument::textFromBool(pController->invert), &eController);
		pDocument->saveTextElement("hook",
			qtractorDocument::textFromBool(pController->hook), &eController);
		pElement->appendChild(eController);
	}
}


//----------------------------------------------------------------------------
// MIDI Controller Type Text/Names - Default control types hash maps.

static struct
{
	qtractorMidiControl::ControlType ctype;
	const char *text;
	const char *name;

} g_aControlTypes[] = {

	{ qtractorMidiEvent::NOTEON,     "NOTEON",     _TR("Note On")    },
	{ qtractorMidiEvent::NOTEOFF,    "NOTEOFF",    _TR("Note Off")   },
	{ qtractorMidiEvent::KEYPRESS,   "KEYPRESS",   _TR("Key Press")  },
	{ qtractorMidiEvent::CONTROLLER, "CONTROLLER", _TR("Controller") },
	{ qtractorMidiEvent::PGMCHANGE,  "PGMCHANGE",  _TR("Pgm Change") },
	{ qtractorMidiEvent::CHANPRESS,  "CHANPRESS",  _TR("Chan Press") },
	{ qtractorMidiEvent::PITCHBEND,  "PITCHBEND",  _TR("Pitch Bend") },
	{ qtractorMidiEvent::REGPARAM,   "REGPARAM",   _TR("RPN")        },
	{ qtractorMidiEvent::NONREGPARAM,"NONREGPARAM",_TR("NRPN")       },
	{ qtractorMidiEvent::CONTROL14,  "CONTROL14",  _TR("Control 14") },

	{ qtractorMidiControl::ControlType(0), NULL, NULL }
};

static QHash<qtractorMidiControl::ControlType, QString> g_controlTypeTexts;
static QHash<QString, qtractorMidiControl::ControlType> g_textControlTypes;

static void initControlTypeTexts (void)
{
	if (g_controlTypeTexts.isEmpty()) {
		// Pre-load ontrol-types hash table...
		for (int i = 0; g_aControlTypes[i].text; ++i) {
			qtractorMidiControl::ControlType ctype = g_aControlTypes[i].ctype;
			const QString& sText = QString(g_aControlTypes[i].text);
			g_controlTypeTexts.insert(ctype, sText);
			g_textControlTypes.insert(sText, ctype);
		}
	}
}


static QHash<qtractorMidiControl::ControlType, QString> g_controlTypeNames;
static QHash<QString, qtractorMidiControl::ControlType> g_nameControlTypes;

static void initControlTypeNames (void)
{
	if (g_controlTypeNames.isEmpty()) {
		// Pre-load ontrol-types hash table...
		for (int i = 0; g_aControlTypes[i].name; ++i) {
			qtractorMidiControl::ControlType ctype = g_aControlTypes[i].ctype;
			const QString& sName = QObject::tr(g_aControlTypes[i].name, "controlType");
			g_controlTypeNames.insert(ctype, sName);
			g_nameControlTypes.insert(sName, ctype);
		}
	}
}


// Control type text (translatable) conversion helpers.
qtractorMidiControl::ControlType qtractorMidiControl::typeFromText (
	const QString& sText )
{
	initControlTypeTexts();

	return g_textControlTypes[sText];
}

const QString& qtractorMidiControl::textFromType (
	qtractorMidiControl::ControlType ctype )
{
	initControlTypeTexts();

	return g_controlTypeTexts[ctype];
}


// Control type name (label) conversion helpers.
qtractorMidiControl::ControlType qtractorMidiControl::typeFromName (
	const QString& sName )
{
	initControlTypeNames();

	return g_nameControlTypes[sName];
}

const QString& qtractorMidiControl::nameFromType (
	qtractorMidiControl::ControlType ctype )
{
	initControlTypeNames();

	return g_controlTypeNames[ctype];
}


//----------------------------------------------------------------------------
// MIDI Controller Command Text/Names - Default command names hash map.

static struct
{
	qtractorMidiControl::Command command;
	const char *text;
	const char *name;

} g_aCommandNames[] = {

	{ qtractorMidiControl::TRACK_GAIN,    "TRACK_GAIN",    _TR("Track Gain")    },
	{ qtractorMidiControl::TRACK_PANNING, "TRACK_PANNING", _TR("Track Panning") },
	{ qtractorMidiControl::TRACK_MONITOR, "TRACK_MONITOR", _TR("Track Monitor") },
	{ qtractorMidiControl::TRACK_RECORD,  "TRACK_RECORD",  _TR("Track Record")  },
	{ qtractorMidiControl::TRACK_MUTE,    "TRACK_MUTE",    _TR("Track Mute")    },
	{ qtractorMidiControl::TRACK_SOLO,    "TRACK_SOLO",    _TR("Track Solo")    },

	{ qtractorMidiControl::Command(0), NULL, NULL }
};

static QHash<qtractorMidiControl::Command, QString> g_commandTexts;
static QHash<QString, qtractorMidiControl::Command> g_textCommands;

static void initCommandTexts (void)
{
	if (g_commandTexts.isEmpty()) {
		// Pre-load command-names hash table...
		for (int i = 0; g_aCommandNames[i].text; ++i) {
			qtractorMidiControl::Command command = g_aCommandNames[i].command;
			const QString& sText = QString(g_aCommandNames[i].text);
			g_commandTexts.insert(command, sText);
			g_textCommands.insert(sText, command);
		}
	}
}

static QHash<qtractorMidiControl::Command, QString> g_commandNames;
static QHash<QString, qtractorMidiControl::Command> g_nameCommands;

static void initCommandNames (void)
{
	if (g_commandNames.isEmpty()) {
		// Pre-load command-names hash table...
		for (int i = 0; g_aCommandNames[i].name; ++i) {
			qtractorMidiControl::Command command = g_aCommandNames[i].command;
			const QString& sName = QObject::tr(g_aCommandNames[i].name, "commandName");
			g_commandNames.insert(command, sName);
			g_nameCommands.insert(sName, command);
		}
	}
}


qtractorMidiControl::Command qtractorMidiControl::commandFromText (
	const QString& sText )
{
#if 0
	initCommandTexts();

	return g_textCommands[sText];
#else
	if (sText == "TRACK_GAIN" || sText == "TrackGain")
		return TRACK_GAIN;
	else
	if (sText == "TRACK_PANNING" || sText == "TrackPanning")
		return TRACK_PANNING;
	else
	if (sText == "TRACK_MONITOR" || sText == "TrackMonitor")
		return TRACK_MONITOR;
	else
	if (sText == "TRACK_RECORD" || sText == "TrackRecord")
		return TRACK_RECORD;
	else
	if (sText == "TRACK_MUTE" || sText == "TrackMute")
		return TRACK_MUTE;
	else
	if (sText == "TRACK_SOLO" || sText == "TrackSolo")
		return TRACK_SOLO;
	else
		return Command(0);
#endif
}

const QString& qtractorMidiControl::textFromCommand ( Command command )
{
	initCommandTexts();

	return g_commandTexts[command];
}


qtractorMidiControl::Command qtractorMidiControl::commandFromName (
	const QString& sName )
{
	initCommandNames();

	return g_nameCommands[sName];
}

const QString& qtractorMidiControl::nameFromCommand ( Command command )
{
	initCommandNames();

	return g_commandNames[command];
}


//----------------------------------------------------------------------------
// qtractorMidiControlTypeGroup - MIDI control type/param widget group.

#include <QComboBox>
#include <QLineEdit>
#include <QLabel>

// Constructor.
qtractorMidiControlTypeGroup::qtractorMidiControlTypeGroup (
	QComboBox *pControlTypeComboBox, QComboBox *pControlParamComboBox,
	QLabel *pControlParamTextLabel ) : QObject(),
		m_pControlTypeComboBox(pControlTypeComboBox),
		m_pControlParamComboBox(pControlParamComboBox),
		m_pControlParamTextLabel(pControlParamTextLabel),
		m_iControlParamUpdate(0)
{
	const QIcon iconControlType(":/images/itemProperty.png");
	m_pControlTypeComboBox->clear();
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NOTEON),
		int(qtractorMidiEvent::NOTEON));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NOTEOFF),
		int(qtractorMidiEvent::NOTEOFF));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::KEYPRESS),
		int(qtractorMidiEvent::KEYPRESS));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CONTROLLER),
		int(qtractorMidiEvent::CONTROLLER));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::PGMCHANGE),
		int(qtractorMidiEvent::PGMCHANGE));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CHANPRESS),
		int(qtractorMidiEvent::CHANPRESS));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::PITCHBEND),
		int(qtractorMidiEvent::PITCHBEND));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::REGPARAM),
		int(qtractorMidiEvent::REGPARAM));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NONREGPARAM),
		int(qtractorMidiEvent::NONREGPARAM));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CONTROL14),
		int(qtractorMidiEvent::CONTROL14));

	m_pControlParamComboBox->setInsertPolicy(QComboBox::NoInsert);

	QObject::connect(m_pControlTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(activateControlType(int)));
	QObject::connect(m_pControlParamComboBox,
		SIGNAL(activated(int)),
		SLOT(activateControlParam(int)));
}


// Accessors.
void qtractorMidiControlTypeGroup::setControlType (
	qtractorMidiControl::ControlType ctype )
{
	const int iControlType = indexFromControlType(ctype);
	m_pControlTypeComboBox->setCurrentIndex(iControlType);
	activateControlType(iControlType);
}

qtractorMidiControl::ControlType qtractorMidiControlTypeGroup::controlType (void) const
{
	const int iControlType = m_pControlTypeComboBox->currentIndex();
	return qtractorMidiControl::ControlType(
		m_pControlTypeComboBox->itemData(iControlType).toInt());
}


void qtractorMidiControlTypeGroup::setControlParam (
	unsigned short iParam )
{
	const int iControlParam = indexFromControlParam(iParam);
	if (iControlParam >= 0) {
		m_pControlParamComboBox->setCurrentIndex(iControlParam);
		activateControlParam(iControlParam);
	} else {
		const QString& sControlParam = QString::number(iParam);
		m_pControlParamComboBox->setEditText(sControlParam);
		editControlParamFinished();
	}
}

unsigned short qtractorMidiControlTypeGroup::controlParam (void) const
{
	const int iControlParam = m_pControlParamComboBox->currentIndex();
	if (iControlParam >= 0)
		return m_pControlParamComboBox->itemData(iControlParam).toInt();
	else
		return m_pControlParamComboBox->currentText().toInt();
}


// Stabilizers.
void qtractorMidiControlTypeGroup::updateControlType ( int iControlType )
{
	if (iControlType < 0)
		iControlType = m_pControlTypeComboBox->currentIndex();

	const qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::ControlType(
			m_pControlTypeComboBox->itemData(iControlType).toInt());

	const int iOldParam
		= m_pControlParamComboBox->currentIndex();

	m_pControlParamComboBox->clear();

	const QString sTextMask("%1 - %2");

	switch (ctype) {
	case qtractorMidiEvent::NOTEON:
	case qtractorMidiEvent::NOTEOFF:
	case qtractorMidiEvent::KEYPRESS: {
		const QIcon iconNotes(":/images/itemNotes.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		for (unsigned short iParam = 0; iParam < 128; ++iParam) {
			m_pControlParamComboBox->addItem(iconNotes,
				sTextMask.arg(iParam).arg(
					qtractorMidiEditor::defaultNoteName(iParam)),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::CONTROLLER: {
		const QIcon iconControllers(":/images/itemControllers.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		for (unsigned short iParam = 0; iParam < 128; ++iParam) {
			m_pControlParamComboBox->addItem(iconControllers,
				sTextMask.arg(iParam).arg(
					qtractorMidiEditor::defaultControllerName(iParam)),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::PGMCHANGE: {
		const QIcon iconPatches(":/images/itemPatches.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		for (unsigned short iParam = 0; iParam < 128; ++iParam) {
			m_pControlParamComboBox->addItem(iconPatches,
				sTextMask.arg(iParam).arg('-'), int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::REGPARAM: {
		const QIcon iconRpns(":/images/itemRpns.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(true);
		const QMap<unsigned short, QString>& rpns
			= qtractorMidiEditor::defaultRpnNames();
		QMap<unsigned short, QString>::ConstIterator rpns_iter
			= rpns.constBegin();
		const QMap<unsigned short, QString>::ConstIterator& rpns_end
			= rpns.constEnd();
		for ( ; rpns_iter != rpns_end; ++rpns_iter) {
			const unsigned short iParam = rpns_iter.key();
			m_pControlParamComboBox->addItem(iconRpns,
				sTextMask.arg(iParam).arg(rpns_iter.value()),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::NONREGPARAM: {
		const QIcon iconNrpns(":/images/itemNrpns.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(true);
		const QMap<unsigned short, QString>& nrpns
			= qtractorMidiEditor::defaultNrpnNames();
		QMap<unsigned short, QString>::ConstIterator nrpns_iter
			= nrpns.constBegin();
		const QMap<unsigned short, QString>::ConstIterator& nrpns_end
			= nrpns.constEnd();
		for ( ; nrpns_iter != nrpns_end; ++nrpns_iter) {
			const unsigned short iParam = nrpns_iter.key();
			m_pControlParamComboBox->addItem(iconNrpns,
				sTextMask.arg(iParam).arg(nrpns_iter.value()),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::CONTROL14: {
		const QIcon iconControllers(":/images/itemControllers.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		for (unsigned short iParam = 1; iParam < 32; ++iParam) {
			m_pControlParamComboBox->addItem(iconControllers,
				sTextMask.arg(iParam).arg(
					qtractorMidiEditor::defaultControl14Name(iParam)),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::CHANPRESS:
	case qtractorMidiEvent::PITCHBEND:
	default:
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(false);
		m_pControlParamComboBox->setEnabled(false);
		m_pControlParamComboBox->setEditable(false);
		break;
	}

	if (m_pControlParamComboBox->isEditable()) {
		QObject::connect(m_pControlParamComboBox->lineEdit(),
			SIGNAL(editingFinished()),
			SLOT(editControlParamFinished()));
	}

	if (iOldParam >= 0 && iOldParam < m_pControlParamComboBox->count())
		m_pControlParamComboBox->setCurrentIndex(iOldParam);
}


// Private slots.
void qtractorMidiControlTypeGroup::activateControlType ( int iControlType )
{
	updateControlType(iControlType);

	const qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::ControlType(
			m_pControlTypeComboBox->itemData(iControlType).toInt());

	emit controlTypeChanged(int(ctype));

	activateControlParam(m_pControlParamComboBox->currentIndex());
}

void qtractorMidiControlTypeGroup::activateControlParam ( int iControlParam )
{
	const unsigned short iParam
		= m_pControlParamComboBox->itemData(iControlParam).toInt();

	emit controlParamChanged(int(iParam));
}


void qtractorMidiControlTypeGroup::editControlParamFinished (void)
{
	if (m_iControlParamUpdate > 0)
		return;

	++m_iControlParamUpdate;

	const QString& sControlParam
		= m_pControlParamComboBox->currentText();

	bool bOk = false;
	const unsigned short iParam = sControlParam.toInt(&bOk);
	if (bOk) emit controlParamChanged(int(iParam));

	--m_iControlParamUpdate;
}


// Find combo-box index from control type.
int qtractorMidiControlTypeGroup::indexFromControlType (
	qtractorMidiControl::ControlType ctype ) const
{
	const int iItemCount = m_pControlTypeComboBox->count();
	for (int iIndex = 0; iIndex < iItemCount; ++iIndex) {
		if (qtractorMidiControl::ControlType(
			m_pControlTypeComboBox->itemData(iIndex).toInt()) == ctype)
			return iIndex;
	}
	return (-1);
}


// Find combo-box index from control parameter number.
int qtractorMidiControlTypeGroup::indexFromControlParam (
	unsigned short iParam ) const
{
	const int iItemCount = m_pControlParamComboBox->count();
	for (int iIndex = 0; iIndex < iItemCount; ++iIndex) {
		if (m_pControlParamComboBox->itemData(iIndex).toInt() == int(iParam))
			return iIndex;
	}
	return (-1);
}


// end of qtractorMidiControl.cpp
