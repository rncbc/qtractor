// qtractorMidiControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.
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

#include "qtractorTrackCommand.h"

#include "qtractorDocument.h"

#include <QFile>

#include <math.h>

// Possible cube roor optimization.
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
}


// Insert new controller mappings.
void qtractorMidiControl::mapChannelParam (
	ControlType ctype, unsigned short iChannel, unsigned short iParam,
	Command command, bool bFeedback )
{
	m_controlMap.insert(
		MapKey(ctype, iChannel, iParam),
		MapVal(command, bFeedback));
}

void qtractorMidiControl::mapChannelTrack (
	ControlType ctype, unsigned short iParam,
	Command command, bool bFeedback )
{
	mapChannelParam(
		ctype, TrackParam, iParam, command, bFeedback);
}

void qtractorMidiControl::mapChannelParamTrack (
	ControlType ctype, unsigned short iChannel, unsigned short iParam,
	Command command, bool bFeedback )
{
	mapChannelParam(
		ctype, iChannel, iParam | TrackParam, command, bFeedback);
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

	// Walk through midi controller map...
	// FIXME: should only be sent if command ask for feedback.
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
						sendTrackController(pTrack,
							val.command(), iTrack, iParam);
					else if (key.isParamTrack())
						sendTrackController(pTrack,
							val.command(), iChannel, iParam + iTrack);
					else /* if (iParam == iTrack) */ {
						sendTrackController(pTrack,
							val.command(), iChannel, iParam);
						break; // Bail out from inner track loop.
					}
				}
				++iTrack;
			}
		}
	}
}


// Find incoming controller event map.
qtractorMidiControl::ControlMap::ConstIterator
qtractorMidiControl::findEvent ( const qtractorCtlEvent& ctle ) const
{
	// Check if controller map includes this event...
	ControlMap::ConstIterator it = m_controlMap.constBegin();
	for ( ; it != m_controlMap.constEnd(); ++it) {
		const MapKey& key = it.key();
		if (key.match(CONTROLLER, ctle.channel(), ctle.controller()))
			break;
	}
	return it;
}


// Process incoming controller event.
bool qtractorMidiControl::processEvent ( const qtractorCtlEvent& ctle ) const
{

	// Find incoming controller event map tuple.
	ControlMap::ConstIterator it = findEvent(ctle);
	// Is there one mapped, indeed?
	if (it == m_controlMap.end())
		return false;

	// Find the track by number...
	const MapKey& key = it.key();
	const MapVal& val = it.value();

	int iTrack = -1;	
	if (key.isChannelTrack()) {
		iTrack = int(ctle.channel());
	}
	else
	if (key.isParamTrack()) {
		unsigned short iParam = key.param() & TrackParamMask;
		if (int(ctle.controller()) >= iParam)
			iTrack = int(ctle.controller()) - iParam;
	}
//	else iTrack = key.param();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorTrack *pTrack = pSession->tracks().at(iTrack);
	if (pTrack == NULL)
		return false;

	switch (val.command()) {
	case TRACK_GAIN:
		pSession->execute(
			new qtractorTrackGainCommand(pTrack,
				(pTrack->trackType() == qtractorTrack::Audio
					? cubef2(float(ctle.value()) / 127.0f)
					: float(ctle.value()) / 127.0f),
				true));
		break;
	case TRACK_PANNING:
		pSession->execute(
			new qtractorTrackPanningCommand(pTrack,
				(float(ctle.value()) - 64.0f) / 63.0f,
				true));
		break;
	case TRACK_MONITOR:
		pSession->execute(
			new qtractorTrackMonitorCommand(pTrack,
				bool(ctle.value() > 0),
				true));
		break;
	case TRACK_RECORD:
		pSession->execute(
			new qtractorTrackStateCommand(pTrack,
				qtractorTrack::Record,
				bool(ctle.value()> 0),
				true));
		break;
	case TRACK_MUTE:
		pSession->execute(
			new qtractorTrackStateCommand(pTrack,
				qtractorTrack::Mute,
				bool(ctle.value() > 0),
				true));
		break;
	case TRACK_SOLO:
		pSession->execute(
			new qtractorTrackStateCommand(pTrack,
				qtractorTrack::Solo,
				bool(ctle.value() > 0),
				true));
		break;
	default:
		break;
	}

	return true;
}


// Process incoming command.
void qtractorMidiControl::processTrackCommand (
	Command command, int iTrack, float fValue, bool bCubic ) const
{
	switch (command) {
	case TRACK_GAIN:
		if (bCubic) fValue = cbrtf2(fValue);
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
	Command command, int iTrack, bool bValue ) const
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
	int iTrack, Command command, int iValue ) const
{
	// Search for the command and parameter in controller map...
	ControlMap::ConstIterator it = m_controlMap.constBegin();
	for ( ; it != m_controlMap.constEnd(); ++it) {
		const MapKey& key = it.key();
		const MapVal& val = it.value();
		if (key.type() == CONTROLLER &&
			val.command() == command &&
			val.isFeedback()) {
			// Now send the message out...
			unsigned short iParam = (key.param() & TrackParamMask);
			if (key.isChannelTrack())
				sendController(iTrack, iParam, iValue);
			else
			if (key.isParamTrack())
				sendController(key.channel(), iParam + iTrack, iValue);
		//	else
		//	if (iParam == iTrack)
		//		sendController(key.channel(), iParam, iValue);
		}
	}
}


void qtractorMidiControl::sendTrackController ( qtractorTrack *pTrack,
	Command command, unsigned short iChannel, unsigned short iController ) const
{
	int iValue = 0;

	switch (command) {
	case TRACK_GAIN:
		if (pTrack->trackType() == qtractorTrack::Audio)
			iValue = int(127.0f * cbrtf2(pTrack->gain()));
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

	sendController(iChannel, iController, iValue);
}


// Send this value out to midi bus.
void qtractorMidiControl::sendController (
	unsigned short iChannel, unsigned short iController, int iValue ) const
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
	else if (iValue > 127)
		iValue = 127;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControl::sendController(%u, %u, %d)",
		iChannel, iController, iValue);
#endif

	pMidiBus->setController(iChannel, iController, iValue);
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
	qtractorDocument *pDocument, QDomElement *pElement )
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
			if (bOldMap) {
				ctype  = CONTROLLER;
				iParam = keyFromText(eItem.attribute("controller"));
			} else {
				iParam = eItem.attribute("param").toUShort();
				if (pDocument->boolFromText(eItem.attribute("track")))
					iParam |= TrackParam;
			}
			Command command = Command(0);
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
				if (eVal.tagName() == "param" && bOldMap)
					iParam |= eVal.text().toUShort();
				else
				if (eVal.tagName() == "feedback")
					bFeedback = pDocument->boolFromText(eVal.text());
			}
			m_controlMap.insert(
				MapKey(ctype, iChannel, iParam),
				MapVal(command, bFeedback));
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
			pDocument->textFromBool(key.isParamTrack()));
		pDocument->saveTextElement("command",
			textFromCommand(val.command()), &eItem);
		pDocument->saveTextElement("feedback",
			pDocument->textFromBool(val.isFeedback()), &eItem);
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


// Document textual helpers.
qtractorMidiControl::ControlType qtractorMidiControl::typeFromText (
	const QString& sText )
{
	if (sText == "MMC")
		return MMC;
	else
	if (sText == "NOTE_ON")
		return NOTE_ON;
	else
	if (sText == "NOTE_OFF")
		return NOTE_OFF;
	else
	if (sText == "KEY_PRESS")
		return KEY_PRESS;
	else
	if (sText == "CONTROLLER")
		return CONTROLLER;
	else
	if (sText == "PGM_CHANGE")
		return PGM_CHANGE;
	else
	if (sText == "CHAN_PRESS")
		return CHAN_PRESS;
	else
	if (sText == "PITCH_BEND")
		return PITCH_BEND;
	else
		return ControlType(0);
}

QString qtractorMidiControl::textFromType (
	qtractorMidiControl::ControlType ctype )
{
	QString sText;

	switch (ctype) {
	case MMC:
		sText = "MMC";
		break;
	case NOTE_ON:
		sText = "NOTE_ON";
		break;
	case NOTE_OFF:
		sText = "NOTE_OFF";
		break;
	case KEY_PRESS:
		sText = "KEY_PRESS";
		break;
	case CONTROLLER:
		sText = "CONTROLLER";
		break;
	case PGM_CHANGE:
		sText = "PGM_CHANGE";
		break;
	case CHAN_PRESS:
		sText = "CHAN_PRESS";
		break;
	case PITCH_BEND:
		sText = "PITCH_BEND";
		break;
	}

	return sText;
}


unsigned short qtractorMidiControl::keyFromText ( const QString& sText )
{
	if (sText == "TrackParam" || sText == "*" || sText.isEmpty())
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


qtractorMidiControl::Command qtractorMidiControl::commandFromText (
	const QString& sText )
{
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
}

QString qtractorMidiControl::textFromCommand (
	qtractorMidiControl::Command command )
{
	QString sText;

	switch (command) {
	case TRACK_GAIN:
		sText = "TRACK_GAIN";
		break;
	case TRACK_PANNING:
		sText = "TRACK_PANNING";
		break;
	case TRACK_MONITOR:
		sText = "TRACK_MONITOR";
		break;
	case TRACK_RECORD:
		sText = "TRACK_RECORD";
		break;
	case TRACK_MUTE:
		sText = "TRACK_MUTE";
		break;
	case TRACK_SOLO:
		sText = "TRACK_SOLO";
		break;
	default:
		break;
	}

	return sText;
}


// end of qtractorMidiControl.cpp
