// qtractorMidiControl.cpp
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
#include "qtractorMidiControl.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"

#include "qtractorTracks.h"
#include "qtractorTrackList.h"
#include "qtractorTrackCommand.h"


//----------------------------------------------------------------------
// qtractorMidiControl::MapKey -- MIDI control map key.
//

class qtractorMidiControl::MapKey
{
public:

	// Constructor.
	MapKey(unsigned short iChannel = 0, unsigned short iController = 0)
		: m_iChannel(iChannel), m_iController(iController) {}

	// Channel accessors.
	void setChannel(unsigned short iChannel)
		{ m_iChannel = iChannel; }
	unsigned short channel() const
		{ return (m_iChannel & MaskParam); }

	bool isChannel() const
		{ return ((m_iChannel & MaskParam) == m_iChannel); }
	bool isChannelParam() const
		{ return (m_iChannel & TrackParam); }

	// Controller accessors.
	void setController (unsigned short iController)
		{ m_iController = iController; }
	unsigned short controller() const
		{ return (m_iController & MaskParam); }

	bool isController() const
		{ return ((m_iController & MaskParam ) == m_iController); }
	bool isControllerParam() const
		{ return (m_iController & TrackParam); }

	// Generic key matcher.
	bool match (unsigned short iChannel, unsigned iController) const
	{
		return (isChannelParam() || channel() == iChannel)
			&& (isControllerParam() || controller() == iController);
	}

	// Raw hash key.
	unsigned short hash() const
		{ return (m_iChannel ^ m_iController); }

	// Hash key comparator.
	bool operator== ( const MapKey& key ) const
	{
		return (key.m_iChannel == m_iChannel)
			&& (key.m_iController == m_iController);
	}

private:

	// Instance (key) member variables.
	unsigned short m_iChannel;
	unsigned short m_iController;
};


// Hash key function
inline uint qHash ( const qtractorMidiControl::MapKey& key )
{
	return qHash(key.hash());
}


//----------------------------------------------------------------------
// qtractorMidiControl::MapVal -- MIDI control map data value.
//

class qtractorMidiControl::MapVal
{
public:

	// Constructor.
	MapVal(Command command = TrackNone, int iParam = 0, bool bFeedback = false)
		: m_command(command), m_iParam(iParam), m_bFeedback(bFeedback) {}

	// Command accessors
	void setCommand(Command command)
		{ m_command = command; }
	Command command() const
		{ return m_command; }

	// Parameter accessor (eg. track delta-index)
	void setParam(int iParam)
		{ m_iParam = iParam; }
	int param() const
		{ return m_iParam; }

	// Feedback flag accessor.
	void setFeedback(bool bFeedback)
		{ m_bFeedback = bFeedback; }
	int isFeedback() const
		{ return m_bFeedback; }

private:

	// Instance (value) member variables.
	Command m_command;
	int     m_iParam;
	bool    m_bFeedback;
};


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

	// TEST: (Re)generate the default controller map...
	//
	// JLCooper faders (as in US-224)...
	mapChannelControllerParam(15, TrackGain, 0x40); // No feedback.
#ifdef CONFIG_TEST_BCx2000
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


// Resend all the controllers
// (as output bus changed, or session initialized)
void qtractorMidiControl::sendAllControllers (void) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControl::sendAllControllers()");
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
				float(pEvent->value()) / 127.0f,
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
	Command command, int iParam, float fValue ) const
{
	switch (command) {
	case TrackGain:
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


// end of qtractorMidiControl.cpp
