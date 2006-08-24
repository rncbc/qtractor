// qtractorEngineCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorEngineCommand.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"

#include "qtractorTracks.h"
#include "qtractorTrackList.h"
#include "qtractorMonitor.h"
#include "qtractorMixer.h"
#include "qtractorMeter.h"


//----------------------------------------------------------------------
// class qtractorBusCommand - implementation
//

// Constructor.
qtractorBusCommand::qtractorBusCommand ( qtractorMainForm *pMainForm,
	const QString& sName, qtractorBus *pBus, qtractorBus::BusMode busMode )
	: qtractorCommand(pMainForm, sName), m_pBus(pBus), m_busMode(busMode),
		m_busType(qtractorTrack::None), m_iChannels(0), m_bAutoConnect(false)
{
	setRefresh(false);
}


// Create a new bus.
bool qtractorBusCommand::createBus (void)
{
	if (m_pBus || m_sBusName.isEmpty())
		return false;
		
	qtractorMainForm *pMainForm = mainForm();
	if (pMainForm == NULL)
		return false;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// Create the bus of proper type...
	m_pBus = NULL;
	switch (m_busType) {
	case qtractorTrack::Audio: {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine) {
			qtractorAudioBus *pAudioBus
				= new qtractorAudioBus(pAudioEngine, m_sBusName, m_busMode,
					m_iChannels, m_bAutoConnect);
			pAudioEngine->addBus(pAudioBus);
			m_pBus = pAudioBus;
		}
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
		if (pMidiEngine) {
			qtractorMidiBus *pMidiBus
				= new qtractorMidiBus(pMidiEngine, m_sBusName, m_busMode);
			pMidiEngine->addBus(pMidiBus);
			m_pBus = pMidiBus;
		}
		break;
	}
	default:
		break;
	}

	// Check if we really have a new bus...
	if (m_pBus == NULL)
		return false;

	// Open up the new bus...
	m_pBus->open();

	// Update mixer (look for new strips...)
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateBusses();

	// Done.
	return true;
}


// Update bus properties.
bool qtractorBusCommand::updateBus (void)
{
	if (m_pBus == NULL || m_sBusName.isEmpty())
		return false;

	qtractorMainForm *pMainForm = mainForm();
	if (pMainForm == NULL)
		return false;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// We need to hold things for a while...
	bool bPlaying = pSession->isPlaying();
	pSession->setPlaying(false);

	// Save current bus properties...
	qtractorBus::BusMode busMode  = m_pBus->busMode();
	QString              sBusName = m_pBus->busName();

	// Special case for audio busses...
	qtractorAudioBus *pAudioBus = NULL;
	unsigned short iChannels = 0;
	bool bAutoConnect = false;
	if (m_pBus->busType() == qtractorTrack::Audio) {
		pAudioBus = static_cast<qtractorAudioBus *> (m_pBus);
		if (pAudioBus) {
			iChannels = pAudioBus->channels();
			bAutoConnect = pAudioBus->isAutoConnect();
		}
	}
	
	// Close all applicable tracks...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->inputBus() == m_pBus)
			pTrack->setInputBusName(m_sBusName);
		if (pTrack->outputBus() == m_pBus)
			pTrack->setOutputBusName(m_sBusName);
		if (pTrack->inputBus() == m_pBus || pTrack->outputBus() == m_pBus)
			pTrack->close();
	}

	// May close now the bus...
	m_pBus->close();

	// Set new properties...
	m_pBus->setBusName(m_sBusName);
	m_pBus->setBusMode(m_busMode);
	// Special case for Audio busses...
	if (pAudioBus) {
		pAudioBus->setChannels(m_iChannels);
		pAudioBus->setAutoConnect(m_bAutoConnect);
	}

	// May reopen up the bus...
	m_pBus->open();

	// Update (reset) all applicable mixer strips...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer) {
		if (m_pBus->busMode() & qtractorBus::Input) {
			pMixer->updateBusStrip(pMixer->inputRack(),
				m_pBus, qtractorBus::Input, true);
		}
		if (m_pBus->busMode() & qtractorBus::Output) {
			pMixer->updateBusStrip(pMixer->outputRack(),
				m_pBus, qtractorBus::Output, true);
		}
	}

	// (Re)open all applicable tracks
	// and (reset) respective mixer strips too ...
	qtractorTracks *pTracks = pMainForm->tracks();
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->inputBusName()  == m_sBusName ||
			pTrack->outputBusName() == m_sBusName) {
			// Reopen track back...
			pTrack->open();
			// Update track list item...
			if (pTracks) {
				qtractorTrackListItem *pTrackItem
					= pTracks->trackList()->trackItem(pTrack);
				if (pTrackItem)
					pTrackItem->setText(qtractorTrackList::Bus, m_sBusName);
			}
			// Update mixer strip...
			if (pMixer)
				pMixer->updateTrackStrip(pTrack, true);
		}
	}

	// Swap saved bus properties...
	m_busMode      = busMode;
	m_sBusName     = sBusName;
	m_iChannels    = iChannels;
	m_bAutoConnect = bAutoConnect;
	
	// Carry on...
	pSession->setPlaying(bPlaying);

	// Done.
	return true;
}


// Delete bus.
bool qtractorBusCommand::deleteBus (void)
{
	if (m_pBus == NULL)
		return false;

	qtractorMainForm *pMainForm = mainForm();
	if (pMainForm == NULL)
		return false;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// Get the device view root item...
	qtractorEngine *pEngine = NULL;
	switch (m_pBus->busType()) {
	case qtractorTrack::Audio:
		pEngine = pSession->audioEngine();
		break;
	case qtractorTrack::Midi:
		pEngine = pSession->midiEngine();
		break;
	default:
		break;
	}
	// Still valid?
	if (pEngine == NULL)
		return false;

	// We need to hold things for a while...
	bool bPlaying = pSession->isPlaying();
	pSession->setPlaying(false);

	// Close all applicable tracks...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->inputBus() == m_pBus || pTrack->outputBus() == m_pBus)
			pTrack->close();
	}

	// May close now the bus...
	m_pBus->close();

	// And remove it...
	pEngine->removeBus(m_pBus);
	m_pBus = NULL;

	// Update mixer (clean old strips...)
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer)
		pMixer->updateBusses();

	// Carry on...
	pSession->setPlaying(bPlaying);
	
	// Done.
	return true;
}



//----------------------------------------------------------------------
// class qtractorCreateBusCommand - implementation.
//

// Constructor.
qtractorCreateBusCommand::qtractorCreateBusCommand (
	qtractorMainForm *pMainForm )
	: qtractorBusCommand(pMainForm, QObject::tr("create bus"))
{
}

// Bus creation command methods.
bool qtractorCreateBusCommand::redo (void)
{
	return createBus();
}

bool qtractorCreateBusCommand::undo (void)
{
	return deleteBus();
}


//----------------------------------------------------------------------
// class qtractorUpdateBusCommand - implementation.
//

// Constructor.
qtractorUpdateBusCommand::qtractorUpdateBusCommand (
	qtractorMainForm *pMainForm, qtractorBus *pBus )
	: qtractorBusCommand(pMainForm, QObject::tr("update bus"), pBus)
{
}

// Bus update command methods.
bool qtractorUpdateBusCommand::redo (void)
{
	return updateBus();
}


//----------------------------------------------------------------------
// class qtractorDeleteBusCommand - implementation.
//

// Constructor.
qtractorDeleteBusCommand::qtractorDeleteBusCommand (
	qtractorMainForm *pMainForm, qtractorBus *pBus )
	: qtractorBusCommand(pMainForm, QObject::tr("delete bus"), pBus)
{
	// Save bus properties for creation (undo)...
	setBusType(pBus->busType());
	setBusName(pBus->busName());
	setBusMode(pBus->busMode());
	// Special case for Audio busses...
	if (pBus->busType() == qtractorTrack::Audio) {
		qtractorAudioBus *pAudioBus
			= static_cast <qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			setChannels(pAudioBus->channels());
			setAutoConnect(pAudioBus->isAutoConnect());
		}
	}
}

// Bus deletion command methods.
bool qtractorDeleteBusCommand::redo (void)
{
	return deleteBus();
}

bool qtractorDeleteBusCommand::undo (void)
{
	return createBus();
}


//----------------------------------------------------------------------
// class qtractorBusGainCommand - implementation.
//

// Constructor.
qtractorBusGainCommand::qtractorBusGainCommand (
	qtractorMainForm *pMainForm, qtractorBus *pBus,
	qtractorBus::BusMode busMode, float fGain )
	: qtractorBusCommand(pMainForm, "bus gain", pBus, busMode)
{
	m_fGain     = fGain;
	m_fPrevGain = 1.0f;
	m_bPrevGain = false;

	// Try replacing an previously equivalent command...
	static qtractorBusGainCommand *s_pPrevGainCommand = NULL;
	if (s_pPrevGainCommand) {
		qtractorCommand *pLastCommand
			= mainForm()->commands()->lastCommand();
		qtractorCommand *pPrevCommand
			= static_cast<qtractorCommand *> (s_pPrevGainCommand);
		if (pPrevCommand == pLastCommand
			&& s_pPrevGainCommand->bus() == pBus
			&& s_pPrevGainCommand->busMode() == busMode) {
			qtractorBusGainCommand *pLastGainCommand
				= static_cast<qtractorBusGainCommand *> (pLastCommand);
			if (pLastGainCommand) {
				// Equivalence means same (sign) direction too...
				float fPrevGain = pLastGainCommand->prevGain();
				float fLastGain = pLastGainCommand->gain();
				int   iPrevSign = (fPrevGain > fLastGain ? +1 : -1);
				int   iCurrSign = (fPrevGain < m_fGain   ? +1 : -1); 
				if (iPrevSign == iCurrSign) {
					m_fPrevGain = fLastGain;
					m_bPrevGain = true;
					mainForm()->commands()->removeLastCommand();
				}
			}
		}
	}
	s_pPrevGainCommand = this;
}


// Bus-gain command method.
bool qtractorBusGainCommand::redo (void)
{
	qtractorBus *pBus = bus();
	if (pBus == NULL)
		return false;

	// Set Bus gain (repective monitor gets set too...)
	float fGain = m_fPrevGain;
	if ((busMode() & qtractorBus::Input) && pBus->monitor_in()) {
		if (!m_bPrevGain)
			fGain = pBus->monitor_in()->gain();	
		pBus->monitor_in()->setGain(m_fGain);
	}
	if ((busMode() & qtractorBus::Output) && pBus->monitor_out()) {
		if (!m_bPrevGain)
			fGain = pBus->monitor_out()->gain();	
		pBus->monitor_out()->setGain(m_fGain);
	}
	// MIDI busses are special...
	if (pBus->busType() == qtractorTrack::Midi) {
		// Now we gotta make sure of proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus)
			pMidiBus->setMasterVolume(m_fGain);
	}

	// Set undo value...
	m_bPrevGain = false;
	m_fPrevGain = m_fGain;
	m_fGain     = fGain;

	// Mixer/Meter turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer) {
		qtractorMixerStrip *pStrip;
		if ((busMode() & qtractorBus::Input) && pBus->monitor_in()) {
			pStrip = pMixer->inputRack()->findStrip(pBus->monitor_in());
			if (pStrip && pStrip->meter())
				pStrip->meter()->updateGain();
		}
		if ((busMode() & qtractorBus::Output) && pBus->monitor_out()) {
			pStrip = pMixer->outputRack()->findStrip(pBus->monitor_out());
			if (pStrip && pStrip->meter())
				pStrip->meter()->updateGain();
		}
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorBusPanningCommand - implementation.
//

// Constructor.
qtractorBusPanningCommand::qtractorBusPanningCommand (
	qtractorMainForm *pMainForm, qtractorBus *pBus,
	qtractorBus::BusMode busMode, float fPanning )
	: qtractorBusCommand(pMainForm, "bus pan", pBus, busMode)
{
	m_fPanning = fPanning;
	m_fPrevPanning = 0.0f;
	m_bPrevPanning = false;

	// Try replacing an previously equivalent command...
	static qtractorBusPanningCommand *s_pPrevPanningCommand = NULL;
	if (s_pPrevPanningCommand) {
		qtractorCommand *pLastCommand
			= mainForm()->commands()->lastCommand();
		qtractorCommand *pPrevCommand
			= static_cast<qtractorCommand *> (s_pPrevPanningCommand);
		if (pPrevCommand == pLastCommand
			&& s_pPrevPanningCommand->bus() == pBus
			&& s_pPrevPanningCommand->busMode() == busMode) {
			qtractorBusPanningCommand *pLastPanningCommand
				= static_cast<qtractorBusPanningCommand *> (pLastCommand);
			if (pLastPanningCommand) {
				// Equivalence means same (sign) direction too...
				float fPrevPanning = pLastPanningCommand->prevPanning();
				float fLastPanning = pLastPanningCommand->panning();
				int   iPrevSign    = (fPrevPanning > fLastPanning ? +1 : -1);
				int   iCurrSign    = (fPrevPanning < m_fPanning   ? +1 : -1); 
				if (iPrevSign == iCurrSign) {
					m_fPrevPanning = fLastPanning;
					m_bPrevPanning = true;
					mainForm()->commands()->removeLastCommand();
				}
			}
		}
	}
	s_pPrevPanningCommand = this;
}


// Bus-panning command method.
bool qtractorBusPanningCommand::redo (void)
{
	qtractorBus *pBus = bus();
	if (pBus == NULL)
		return false;

	// Set bus panning (repective monitor gets set too...)
	float fPanning = m_fPrevPanning;	
	if ((busMode() & qtractorBus::Input) && pBus->monitor_in()) {
		if (!m_bPrevPanning)
			fPanning = pBus->monitor_in()->panning();	
		pBus->monitor_in()->setPanning(m_fPanning);
	}
	if ((busMode() & qtractorBus::Output) && pBus->monitor_out()) {
		if (!m_bPrevPanning)
			fPanning = pBus->monitor_out()->panning();	
		pBus->monitor_out()->setPanning(m_fPanning);
	}

	// Set undo value...
	m_bPrevPanning = false;
	m_fPrevPanning = m_fPanning;
	m_fPanning     = fPanning;

	// Mixer/Meter turn...
	qtractorMixer *pMixer = mainForm()->mixer();
	if (pMixer) {
		qtractorMixerStrip *pStrip;
		if ((busMode() & qtractorBus::Input) && pBus->monitor_in()) {
			pStrip = pMixer->inputRack()->findStrip(pBus->monitor_in());
			if (pStrip && pStrip->meter())
				pStrip->meter()->updatePanning();
		}
		if ((busMode() & qtractorBus::Output) && pBus->monitor_out()) {
			pStrip = pMixer->outputRack()->findStrip(pBus->monitor_out());
			if (pStrip && pStrip->meter())
				pStrip->meter()->updatePanning();
		}
	}

	return true;
}


// end of qtractorEngineCommand.cpp
