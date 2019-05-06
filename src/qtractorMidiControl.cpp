// qtractorMidiControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.
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

#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>

#include <QFile>


// Translatable macro contextualizer.
#undef  _TR
#define _TR(x) QT_TR_NOOP(x)


#include <math.h>

// Ref. P.448. Approximate cube root of an IEEE float
// Hacker's Delight (2nd Edition), by Henry S. Warren
// http://www.hackersdelight.org/hdcodetxt/acbrt.c.txt
//
static inline float cbrtf2 ( float x )
{
#ifdef CONFIG_FLOAT32//_NOP
	// Avoid strict-aliasing optimization (gcc -O2).
	union { float f; int i; } u;
	u.f  = x;
	u.i  = (u.i >> 4) + (u.i >> 2);
	u.i += (u.i >> 4) + 0x2a6497f8;
//	return 0.33333333f * (2.0f * u.f + x / (u.f * u.f));
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


// Clear track (catch-up) map.
void qtractorMidiControl::clearControlMap (void)
{
	ControlMap::Iterator it = m_controlMap.begin();
	const ControlMap::Iterator& it_end = m_controlMap.end();
	for ( ; it != it_end; ++it)
		it.value().clear();
}


// Insert new controller mappings.
void qtractorMidiControl::mapChannelParam (
	ControlType ctype, unsigned short iChannel, unsigned short iParam,
	Command command, int iTrack, int iFlags )
{
	m_controlMap.insert(
		MapKey(ctype, iChannel, iParam),
		MapVal(command, iTrack, iFlags));
}

void qtractorMidiControl::mapChannelTrack (
	ControlType ctype, unsigned short iParam,
	Command command, int iTrack, int iFlags )
{
	mapChannelParam(
		ctype, TrackParam, iParam, command, iTrack, iFlags);
}

void qtractorMidiControl::mapChannelParamTrack (
	ControlType ctype, unsigned short iChannel, unsigned short iParam,
	Command command, int iTrack, int iFlags )
{
	mapChannelParam(
		ctype, iChannel, iParam | TrackParam, command, iTrack, iFlags);
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
			const unsigned short iChannel = key.channel();
			const unsigned short iParam = (key.param() & TrackParamMask);
			int iTrack = 0;
			int iLastTrack = iFirstTrack;
			if (key.isParamTrack())
				iLastTrack += val.trackLimit();
			for (qtractorTrack *pTrack = pSession->tracks().first();
					pTrack; pTrack = pTrack->next()) {
				if (iTrack >= iFirstTrack) {
					if (key.isChannelTrack())
						sendTrackController(key.type(),
							pTrack, val.command(), iTrack, iParam);
					else if (key.isParamTrack()) {
						if (iFirstTrack < iLastTrack && iTrack >= iLastTrack)
							break; // Bail out from inner track loop.
						const unsigned short iParamTrack
							= iParam + val.trackOffset() + iTrack;
						sendTrackController(key.type(),
							pTrack, val.command(), iChannel, iParamTrack);
					}
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
		// Generic key matcher.
		if (key.type() != ctle.type())
			continue;
		if (!key.isChannelTrack() && key.channel() != ctle.channel())
			continue;
		const unsigned short iParam = (key.param() & TrackParamMask);
		if (key.isParamTrack()) {
			const MapVal& val = it.value();
			const unsigned short iKeyParam
				= iParam + val.trackOffset();
			const unsigned short iKeyParamLimit
				= iKeyParam + val.trackLimit();
			const unsigned short iCtlParam = ctle.param();
			if (iCtlParam >= iKeyParam
				&& (iKeyParam >= iKeyParamLimit || iCtlParam < iKeyParamLimit))
				break;
		}
		else
		if (iParam == ctle.param())
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

	int iTrack = 0;
	if (key.isChannelTrack()) {
		iTrack += val.track();
		iTrack += int(ctle.channel());
	}
	else
	if (key.isParamTrack()) {
		const unsigned short iKeyParam
			= (key.param() & TrackParamMask) + val.trackOffset();
		const unsigned short iKeyParamLimit
			= iKeyParam + val.trackLimit();
		const unsigned short iCtlParam = ctle.param();
		if (iCtlParam >= iKeyParam
			&& (iKeyParam >= iKeyParamLimit || iCtlParam < iKeyParamLimit))
			iTrack += (iCtlParam - iKeyParam);
		else
			return bResult;
	}
	else iTrack = val.track();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return bResult;

	qtractorTrack *pTrack = pSession->tracks().at(iTrack);
	if (pTrack == NULL)
		return bResult;

	ControlScale scale(ctle.type());

	MapVal::Track& ctlv = val.track(iTrack);

	float fValue, fOldValue;
	switch (val.command()) {
	case TRACK_GAIN:
		fValue = scale.valueFromMidi(ctle.value());
		fOldValue = pTrack->gain();
		if (pTrack->trackType() == qtractorTrack::Audio && !val.isDelta())
			fValue = ::cubef2(fValue);
		if (ctlv.syncDecimal(fValue, fOldValue, val.isDelta())) {
			bResult = pSession->execute(
				new qtractorTrackGainCommand(pTrack, ctlv.value(), true));
		}
		break;
	case TRACK_PANNING:
		fValue = scale.valueSignedFromMidi(ctle.value());
		fOldValue = pTrack->panning();
		if (ctlv.syncDecimal(fValue, fOldValue, val.isDelta())) {
			bResult = pSession->execute(
				new qtractorTrackPanningCommand(pTrack, ctlv.value(), true));
		}
		break;
	case TRACK_MONITOR:
		fValue = scale.valueToggledFromMidi(ctle.value());
		fOldValue = (pTrack->isMonitor() ? 1.0f : 0.0f);
		if (ctlv.syncToggled(fValue, fOldValue, val.isDelta())) {
			bResult = pSession->execute(
				new qtractorTrackMonitorCommand(pTrack, ctlv.value(), true));
		}
		break;
	case TRACK_RECORD:
		fValue = scale.valueToggledFromMidi(ctle.value());
		fOldValue = (pTrack->isRecord() ? 1.0f : 0.0f);
		if (ctlv.syncToggled(fValue, fOldValue, val.isDelta())) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Record, ctlv.value(), true));
		}
		break;
	case TRACK_MUTE:
		fValue = scale.valueToggledFromMidi(ctle.value());
		fOldValue = (pTrack->isMute() ? 1.0f : 0.0f);
		if (ctlv.syncToggled(fValue, fOldValue, val.isDelta())) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Mute, ctlv.value(), true));
		}
		break;
	case TRACK_SOLO:
		fValue = scale.valueToggledFromMidi(ctle.value());
		fOldValue = (pTrack->isSolo() ? 1.0f : 0.0f);
		if (ctlv.syncToggled(fValue, fOldValue, val.isDelta())) {
			bResult = pSession->execute(
				new qtractorTrackStateCommand(pTrack,
					qtractorTrack::Solo, ctlv.value(), true));
		}
		break;
	default:
		break;
	}

	return bResult;
}


// Process incoming command.
void qtractorMidiControl::processTrackCommand (
	Command command, int iTrack, float fValue, bool bLogarithmic )
{
	sendTrackController(iTrack, command, fValue, bLogarithmic);
}


void qtractorMidiControl::processTrackCommand (
	Command command, int iTrack, bool bValue )
{
	sendTrackController(iTrack, command, (bValue ? 1.0f : 0.0f), false);
}


// Further processing of outgoing midi controller messages
void qtractorMidiControl::sendTrackController (
	int iTrack, Command command, float fValue, bool bLogarithmic )
{
	// Search for the command and parameter in controller map...
	ControlMap::Iterator it = m_controlMap.begin();
	const ControlMap::Iterator& it_end = m_controlMap.end();
	for ( ; it != it_end; ++it) {
		const MapKey& key = it.key();
		MapVal& val = it.value();
		if (val.command() == command) {
			val.syncReset(iTrack);
			if (!val.isFeedback())
				continue;
			// Convert/normalize value...
			const ControlType ctype = key.type();
			const ControlScale scale(ctype);
			unsigned short iValue = 0;
			switch (command) {
			case TRACK_GAIN:
				if (bLogarithmic) fValue = ::cbrtf2(fValue);
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
			if (key.isParamTrack()) {
				const unsigned short iParamTrack
					= iParam + val.trackOffset() + iTrack;
				sendController(key.type(), key.channel(), iParamTrack, iValue);
			}
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
	const ControlScale scale(ctype);
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

	// Property accessors.
	qtractorMidiControl *midiControl() const
		{ return m_pMidiControl; }

	// External storage simple methods.
	bool load(const QString& sFilename);
	bool save(const QString& sFilename);

protected:

	// Elemental loader/savers...
	bool loadElement(QDomElement *pElement)
		{ return m_pMidiControl->loadElement(this, pElement); }
	bool saveElement(QDomElement *pElement)
		{ return m_pMidiControl->saveElement(this, pElement); }

private:

	// Instance variables.
	qtractorMidiControl *m_pMidiControl;
};


// External storage simple load method.
bool qtractorMidiControl::Document::load ( const QString& sFilename )
{
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	// Parse it a-la-DOM :-)
	QDomDocument *pDocument = document();
	if (!pDocument->setContent(&file)) {
		file.close();
		return false;
	}

	file.close();

	QDomElement elem = pDocument->documentElement();
	// Get root element and check for proper taq name.
	if (elem.tagName() != "midi-control")
		return false;

	return loadElement(&elem);
}


// External storage simple save method.
bool qtractorMidiControl::Document::save ( const QString& sFilename )
{
	QFile file(sFilename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

	QDomDocument *pDocument = document();
	QDomElement elem = pDocument->createElement("midi-control");
	saveElement(&elem);
	pDocument->appendChild(elem);

	QTextStream ts(&file);
	ts << pDocument->toString() << endl;
	file.close();

	return true;
}


// Load controller rules.
bool qtractorMidiControl::loadElement (
	Document * /*pDocument*/, QDomElement *pElement )
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
			bool bDelta = false;
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
					iTrack |= (eVal.text().toInt() & 0x7f);
				else
				if (eVal.tagName() == "limit")
					iTrack |= (eVal.text().toInt() << 7) & 0x3fc0;
				else
				if (eVal.tagName() == "delta")
					bDelta = qtractorDocument::boolFromText(eVal.text());
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
			int iFlags = 0;
			if (bDelta)
				iFlags |= MapVal::Delta;
			if (bFeedback)
				iFlags |= MapVal::Feedback;
			m_controlMap.insert(
				MapKey(ctype, iChannel, iParam),
				MapVal(command, iTrack, iFlags));
		}
	}

	return true;
}


// Save controller rules.
bool qtractorMidiControl::saveElement (
	Document *pDocument, QDomElement *pElement )
{
	// Save this program version (informational)...
	pElement->setAttribute("version", PACKAGE_STRING);

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
			QString::number(val.trackOffset()), &eItem);
		pDocument->saveTextElement("limit",
			QString::number(val.trackLimit()), &eItem);
		pDocument->saveTextElement("delta",
			qtractorDocument::textFromBool(val.isDelta()), &eItem);
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
			pController->name = eController.attribute("name");
			pController->index = eController.attribute("index").toULong();
			pController->ctype = typeFromText(eController.attribute("type"));
			pController->channel = 0;
			pController->param = 0;
			pController->logarithmic = false;
			pController->feedback = false;
			pController->invert = false;
			pController->hook = false;
			pController->latch = false;
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
				else
				if (eProp.tagName() == "hook")
					pController->hook = qtractorDocument::boolFromText(eProp.text());
				else
				if (eProp.tagName() == "latch")
					pController->latch = qtractorDocument::boolFromText(eProp.text());
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
		pDocument->saveTextElement("latch",
			qtractorDocument::textFromBool(pController->latch), &eController);
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


// MIDI control non catch-up/hook global option.
bool qtractorMidiControl::g_bSync = false;

void qtractorMidiControl::setSync ( bool bSync )
{
	g_bSync = bSync;
}

bool qtractorMidiControl::isSync (void)
{
	return g_bSync;
}


// end of qtractorMidiControl.cpp
