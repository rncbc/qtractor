// qtractorMidiControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.
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

#include "qtractorMainForm.h"

#include "qtractorTracks.h"
#include "qtractorTrackList.h"
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
void qtractorMidiControl::mapChannelController (
	unsigned short iChannel, unsigned short iController,
	Command command, int iParam, bool bFeedback )
{
	m_controlMap.insert(
		MapKey(iChannel, iController),
		MapVal(command, iParam, bFeedback));
}

void qtractorMidiControl::mapChannelParamController (
	unsigned short iController,
	Command command, int iParam, bool bFeedback )
{
	mapChannelController(
		TrackParam, iController, command, iParam, bFeedback);
}

void qtractorMidiControl::mapChannelControllerParam (
	unsigned short iChannel,
	Command command, int iParam, bool bFeedback )
{
	mapChannelController(
		iChannel, TrackParam, command, iParam, bFeedback);
}


// Remove existing controller mapping.
void qtractorMidiControl::unmapChannelController (
	unsigned short iChannel, unsigned short iController )
{
	m_controlMap.remove(MapKey(iChannel, iController));
}


// Check if given channel, controller pair is currently mapped.
bool qtractorMidiControl::isChannelControllerMapped (
	unsigned short iChannel, unsigned short iController ) const
{
	return m_controlMap.contains(MapKey(iChannel, iController));
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
			unsigned short iController = key.controller();
			int iParam = val.param();
			int iTrack = 0;
			for (qtractorTrack *pTrack = pSession->tracks().first();
					pTrack; pTrack = pTrack->next()) {
				if (iTrack >= iFirstTrack) {
					if (key.isChannelParam())
						sendTrackController(pTrack,
							val.command(), iParam + iTrack, iController);
					else if (key.isControllerParam())
						sendTrackController(pTrack,
							val.command(), iChannel, iParam + iTrack);
					else if (val.param() == iTrack) {
						sendTrackController(pTrack,
							val.command(), iChannel, iController);
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
qtractorMidiControl::findEvent (
	qtractorMidiControlEvent *pEvent ) const
{
	// Check if controller map includes this event...
	ControlMap::ConstIterator it = m_controlMap.constBegin();
	for ( ; it != m_controlMap.constEnd(); ++it) {
		const MapKey& key = it.key();
		if (key.match(pEvent->channel(), pEvent->controller()))
			break;
	}
	return it;
}


// Process incoming controller event.
bool qtractorMidiControl::processEvent (
	qtractorMidiControlEvent *pEvent ) const
{

	// Find incoming controller event map tuple.
	ControlMap::ConstIterator it = findEvent(pEvent);
	// Is there one mapped, indeed?
	if (it == m_controlMap.end())
		return false;

	// Find the track by number...
	const MapKey& key = it.key();
	const MapVal& val = it.value();

	int iTrack = -1;	
	if (key.isChannelParam()) {
		if (int(pEvent->channel()) >= val.param())
			iTrack = int(pEvent->channel()) - val.param();
	}
	else
	if (key.isControllerParam()) {
		if (int(pEvent->controller()) >= val.param())
			iTrack = int(pEvent->controller()) - val.param();
	}
	else iTrack = val.param();

	if (iTrack < 0) return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return false;

	qtractorTrackList *pTrackList = pTracks->trackList();
	if (pTrackList == NULL)
		return false;

	qtractorTrack *pTrack = pTrackList->track(iTrack);
	if (pTrack == NULL)
		return false;

	qtractorTrackItemWidget *pTrackWidget
		= pTrackList->trackWidget(iTrack);
	if (pTrackWidget == NULL)
		return false;

	switch (val.command()) {
	case TrackGain:
		pSession->execute(
			new qtractorTrackGainCommand(pTrack,
				(pTrack->trackType() == qtractorTrack::Audio
					? cubef2(float(pEvent->value()) / 127.0f)
					: float(pEvent->value()) / 127.0f),
				true));
		break;
	case TrackPanning:
		pSession->execute(
			new qtractorTrackPanningCommand(pTrack,
				(float(pEvent->value()) - 63.0f) / 64.0f,
				true));
		break;
	case TrackMonitor:
		pSession->execute(
			new qtractorTrackMonitorCommand(pTrack,
				bool(pEvent->value() > 0),
				true));
		break;
	case TrackRecord:
		pSession->execute(
			new qtractorTrackButtonCommand(
				pTrackWidget->recordButton(),
				bool(pEvent->value() > 0),
				true));
		break;
	case TrackMute:
		pSession->execute(
			new qtractorTrackButtonCommand(
				pTrackWidget->muteButton(),
				bool(pEvent->value() > 0),
				true));
		break;
	case TrackSolo:
		pSession->execute(
			new qtractorTrackButtonCommand(
				pTrackWidget->soloButton(),
				bool(pEvent->value() > 0),
				true));
		break;
	default:
		break;
	}

	return true;
}


// Process incoming command.
void qtractorMidiControl::processCommand (
	Command command, int iParam, float fValue, bool bCubic ) const
{
	switch (command) {
	case TrackGain:
		if (bCubic) fValue = cbrtf2(fValue);
		sendParamController(command, iParam, int(127.0f * fValue));
		break;
	case TrackPanning:
		sendParamController(command, iParam, int((64.0f * fValue) + 63.0f));
		break;
	default:
		break;
	}
}


void qtractorMidiControl::processCommand (
	Command command, int iParam, bool bValue ) const
{
	switch (command) {
	case TrackMonitor:
	case TrackRecord:
	case TrackMute:
	case TrackSolo:
		sendParamController(command, iParam, (bValue ? 127 : 0));
		break;
	default:
		break;
	}
}


// Further processing of outgoing midi controller messages
void qtractorMidiControl::sendParamController (
	Command command, int iParam, int iValue ) const
{
	// Search for the command and parameter in controller map...
	ControlMap::ConstIterator it = m_controlMap.constBegin();
	for ( ; it != m_controlMap.constEnd(); ++it) {
		const MapKey& key = it.key();
		const MapVal& val = it.value();
		if (val.command() == command && val.isFeedback()) {
			// Now send the message out...
			if (key.isChannelParam())
				sendController(val.param() + iParam, key.controller(), iValue);
			else
			if (key.isControllerParam())
				sendController(key.channel(), val.param() + iParam, iValue);
			else
			if (val.param() == iParam)
				sendController(key.channel(), key.controller(), iValue);
		}
	}
}


void qtractorMidiControl::sendTrackController ( qtractorTrack *pTrack,
	Command command, unsigned short iChannel, unsigned short iController ) const
{
	int iValue = 0;

	switch (command) {
	case TrackGain:
		if (pTrack->trackType() == qtractorTrack::Audio)
			iValue = int(127.0f * cbrtf2(pTrack->gain()));
		else
			iValue = int(127.0f * pTrack->gain());
		break;
	case TrackPanning:
		iValue = int((64.0f * pTrack->panning()) + 63.0f);
		break;
	case TrackMonitor:
		iValue = (pTrack->isMonitor() ? 127 : 0);
		break;
	case TrackRecord:
		iValue = (pTrack->isRecord() ? 127 : 0);
		break;
	case TrackMute:
		iValue = (pTrack->isMute() ? 127 : 0);
		break;
	case TrackSolo:
		iValue = (pTrack->isSolo() ? 127 : 0);
		break;
	default:
		break;
	}

	if (iValue < 0)
		iValue = 0;
	else if (iValue > 127)
		iValue = 127;

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
			unsigned short iChannel
				= keyFromText(eItem.attribute("channel"));
			unsigned short iController
				= keyFromText(eItem.attribute("controller"));
			Command command = TrackNone;
			int iParam = 0;
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
				if (eVal.tagName() == "param")
					iParam = eVal.text().toInt();
				else
				if (eVal.tagName() == "feedback")
					bFeedback = pDocument->boolFromText(eVal.text());
			}
			m_controlMap.insert(
				MapKey(iChannel, iController),
				MapVal(command, iParam, bFeedback));
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
		const QString sTrackParam("TrackParam");
		eItem.setAttribute("channel",
			textFromKey(key.channel()));
		eItem.setAttribute("controller",
			textFromKey(key.controller()));
		pDocument->saveTextElement("command",
			textFromCommand(val.command()), &eItem);
		pDocument->saveTextElement("param",
			QString::number(val.param()), &eItem);
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
	if (sText == "TrackGain")
		return TrackGain;
	else
	if (sText == "TrackPanning")
		return TrackPanning;
	else
	if (sText == "TrackMonitor")
		return TrackMonitor;
	else
	if (sText == "TrackRecord")
		return TrackRecord;
	else
	if (sText == "TrackMute")
		return TrackMute;
	else
	if (sText == "TrackSolo")
		return TrackSolo;
	else
		return TrackNone;
}


QString qtractorMidiControl::textFromCommand (
	qtractorMidiControl::Command command )
{
	QString sText;

	switch (command) {
	case TrackGain:
		sText = "TrackGain";
		break;
	case TrackPanning:
		sText = "TrackPanning";
		break;
	case TrackMonitor:
		sText = "TrackMonitor";
		break;
	case TrackRecord:
		sText = "TrackRecord";
		break;
	case TrackMute:
		sText = "TrackMute";
		break;
	case TrackSolo:
		sText = "TrackSolo";
		break;
	case TrackNone:
	default:
		sText = "TrackNone";
		break;
	}

	return sText;
}


// end of qtractorMidiControl.cpp
