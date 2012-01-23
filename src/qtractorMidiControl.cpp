// qtractorMidiControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.
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
	return cbrtf(x);
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
qtractorMidiControl* qtractorMidiControl::g_pMidiControl = NULL;

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
	for ( ; it != m_controlMap.constEnd(); ++it) {
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
		for ( ; iter != m_observerMap.constEnd(); ++iter) {
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
	for ( ; it != m_controlMap.end(); ++it) {
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
		unsigned short iParam = key.param() & TrackParamMask;
		if (int(ctle.param()) >= iParam)
			iTrack += int(ctle.param()) - iParam;
	}

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return bResult;

	qtractorTrack *pTrack = pSession->tracks().at(iTrack);
	if (pTrack == NULL)
		return bResult;

	float fValue;
	switch (val.command()) {
	case TRACK_GAIN:
		fValue = float(ctle.value()) / 127.0f;
		if (pTrack->trackType() == qtractorTrack::Audio)
			fValue = ::cubef2(fValue);
		if (val.sync(fValue, pTrack->gain())) {
			bResult = pSession->execute(
				new qtractorTrackGainCommand(pTrack, val.value(), true));
		}
		break;
	case TRACK_PANNING:
		fValue = (float(ctle.value()) - 64.0f) / 63.0f;
		if (val.sync(fValue, pTrack->panning())) {
			bResult = pSession->execute(
				new qtractorTrackPanningCommand(pTrack, val.value(), true));
		}
		break;
	case TRACK_MONITOR:
		fValue = (ctle.value() > 0 ? 1.0f : 0.0f);
		if (val.sync(fValue, (pTrack->isMonitor() ? 1.0f : 0.0f))) {
			bResult = pSession->execute(
				new qtractorTrackMonitorCommand(pTrack, val.value(), true));
		}
		break;
	case TRACK_RECORD:
		fValue = (ctle.value() > 0 ? 1.0f : 0.0f);
		if (val.sync(fValue, (pTrack->isRecord() ? 1.0f : 0.0f))) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Record, val.value(), true));
		}
		break;
	case TRACK_MUTE:
		fValue = (ctle.value() > 0 ? 1.0f : 0.0f);
		if (val.sync(fValue, (pTrack->isMute() ? 1.0f : 0.0f))) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Mute, val.value(), true));
		}
		break;
	case TRACK_SOLO:
		fValue = (ctle.value() > 0 ? 1.0f : 0.0f);
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
	switch (command) {
	case TRACK_GAIN:
		if (bCubic) fValue = ::cbrtf2(fValue);
		sendTrackController(iTrack, command, int(127.0f * fValue));
		break;
	case TRACK_PANNING:
		sendTrackController(iTrack, command, 0x40 + int(63.0f * fValue));
		break;
	default:
		break;
	}
}


void qtractorMidiControl::processTrackCommand (
	Command command, int iTrack, bool bValue )
{
	switch (command) {
	case TRACK_MONITOR:
	case TRACK_RECORD:
	case TRACK_MUTE:
	case TRACK_SOLO:
		sendTrackController(iTrack, command, (bValue ? 127 : 0));
		break;
	default:
		break;
	}
}


// Further processing of outgoing midi controller messages
void qtractorMidiControl::sendTrackController (
	int iTrack, Command command, int iValue )
{
	// Search for the command and parameter in controller map...
	ControlMap::Iterator it = m_controlMap.begin();
	for ( ; it != m_controlMap.end(); ++it) {
		const MapKey& key = it.key();
		MapVal& val = it.value();
		if (val.command() == command) {
			val.syncReset();
			if (!val.isFeedback())
				continue;
			// Now send the message out...
			unsigned short iParam = (key.param() & TrackParamMask);
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
	int iValue = 0;

	switch (command) {
	case TRACK_GAIN:
		if (pTrack->trackType() == qtractorTrack::Audio)
			iValue = int(127.0f * ::cbrtf2(pTrack->gain()));
		else
			iValue = int(127.0f * pTrack->gain());
		break;
	case TRACK_PANNING:
		iValue = 0x40 + int(63.0f * pTrack->panning());
		break;
	case TRACK_MONITOR:
		iValue = (pTrack->isMonitor() ? 127 : 0);
		break;
	case TRACK_RECORD:
		iValue = (pTrack->isRecord() ? 127 : 0);
		break;
	case TRACK_MUTE:
		iValue = (pTrack->isMute() ? 127 : 0);
		break;
	case TRACK_SOLO:
		iValue = (pTrack->isSolo() ? 127 : 0);
		break;
	default:
		break;
	}

	sendController(ctype, iChannel, iParam, iValue);
}


// Send this value out to midi bus.
void qtractorMidiControl::sendController ( ControlType ctype,
	unsigned short iChannel, unsigned short iParam, int iValue ) const
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

	if (iValue < 0)
		iValue = 0;
	else
	if (ctype == qtractorMidiEvent::PITCHBEND) {
		if (iValue > 0x3fff)
			iValue = 0x3fff;
	}
	else
	if (iValue > 0x7f)
		iValue = 0x7f;

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
	MapKey key(
		pMidiObserver->type(),
		pMidiObserver->channel(),
		pMidiObserver->param());

	m_observerMap.insert(key, pMidiObserver);
}

void qtractorMidiControl::unmapMidiObserver (
	qtractorMidiControlObserver *pMidiObserver )
{
	MapKey key(
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
			unsigned short iChannel
				= keyFromText(eItem.attribute("channel"));
			unsigned short iParam = 0;
			bool bOldMap = (ctype == ControlType(0));
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
	for ( ; it != m_controlMap.constEnd(); ++it) {
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


// end of qtractorMidiControl.cpp
