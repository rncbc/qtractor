// qtractorEngineCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorEngineCommand.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorSession.h"

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
qtractorBusCommand::qtractorBusCommand ( const QString& sName,
	qtractorBus *pBus, qtractorBus::BusMode busMode )
	: qtractorCommand(sName), m_pBus(pBus), m_busMode(busMode),
		m_busType(qtractorTrack::None), m_bMonitor(false),
		m_iChannels(0), m_bAutoConnect(false)
{
	setRefresh(false);

	// Set initial bus properties if any...
	if (m_pBus && m_busMode == qtractorBus::None) {
		m_busMode  = m_pBus->busMode();
		m_busType  = m_pBus->busType();
		m_sBusName = m_pBus->busName();
		m_bMonitor = m_pBus->isMonitor();
		// Special case typed buses...
		switch (m_pBus->busType()) {
		case qtractorTrack::Audio: {
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (m_pBus);
			if (pAudioBus) {
				m_iChannels = pAudioBus->channels();
				m_bAutoConnect = pAudioBus->isAutoConnect();
			}
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (m_pBus);
			if (pMidiBus) {
				m_sInstrumentName = pMidiBus->instrumentName();
			}
			break;
		}
		case qtractorTrack::None:
		default:
			break;
		}
	}
}


// Create a new bus.
bool qtractorBusCommand::createBus (void)
{
	if (m_pBus || m_sBusName.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;


	// Create the bus of proper type...
	m_pBus = NULL;
	qtractorAudioBus *pAudioBus = NULL;
	qtractorMidiBus *pMidiBus = NULL;
	switch (m_busType) {
	case qtractorTrack::Audio: {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine) {
			pAudioBus = new qtractorAudioBus(pAudioEngine,
				m_sBusName, m_busMode, m_bMonitor, m_iChannels);
			pAudioBus->setAutoConnect(m_bAutoConnect);
			pAudioEngine->addBus(pAudioBus);
			pAudioEngine->resetPlayerBus();
			pAudioEngine->resetMetroBus();
			m_pBus = pAudioBus;
		}
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
		if (pMidiEngine) {
			pMidiBus = new qtractorMidiBus(pMidiEngine,
				m_sBusName, m_busMode, m_bMonitor);
			pMidiBus->setInstrumentName(m_sInstrumentName);
			pMidiEngine->addBus(pMidiBus);
			pMidiEngine->resetControlBus();
			pMidiEngine->resetMetroBus();
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

	// Yet special for audio buses...
	if (pAudioBus)
		pAudioBus->autoConnect();

	// Update mixer (look for new strips...)
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer)
			pMixer->updateBuses(true);
	}

	// Done.
	return true;
}


// Update bus properties.
bool qtractorBusCommand::updateBus (void)
{
	if (m_pBus == NULL || m_sBusName.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// We need to hold things for a while...
	bool bPlaying = pSession->isPlaying();

	pSession->lock();
	pSession->setPlaying(false);

	// Save current bus properties...
	qtractorBus::BusMode busMode = m_pBus->busMode();
	QString sBusName = m_pBus->busName();
	bool bMonitor = m_pBus->isMonitor();

	// Save current connections...
	qtractorBus::ConnectList inputs;
	qtractorBus::ConnectList outputs;

	if (busMode & qtractorBus::Input)
		m_pBus->updateConnects(qtractorBus::Input, inputs);
	if (busMode & qtractorBus::Output)
		m_pBus->updateConnects(qtractorBus::Output, outputs);

	// Special case typed buses...
	qtractorAudioBus *pAudioBus = NULL;
	qtractorMidiBus *pMidiBus = NULL;
	unsigned short iChannels = 0;
	bool bAutoConnect = false;
	QString sInstrumentName;
	switch (m_pBus->busType()) {
	case qtractorTrack::Audio:
		pAudioBus = static_cast<qtractorAudioBus *> (m_pBus);
		if (pAudioBus) {
			iChannels = pAudioBus->channels();
			bAutoConnect = pAudioBus->isAutoConnect();
		}
		break;
	case qtractorTrack::Midi:
		pMidiBus = static_cast<qtractorMidiBus *> (m_pBus);
		if (pMidiBus) {
			sInstrumentName = pMidiBus->instrumentName();
		}
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Update (reset) all applicable mixer strips...
	QList<qtractorMixerStrip *> strips;
	qtractorMixerStrip *pStrip;
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer) {
		if (m_pBus->busMode() & qtractorBus::Input) {
			pStrip = (pMixer->inputRack())->findStrip(m_pBus->monitor_in());
			if (pStrip) {
				pStrip->clear();
				if (m_busMode & qtractorBus::Input) {
					strips.append(pStrip);
				} else {
					(pMixer->inputRack())->removeStrip(pStrip);
				}
			}
		}
		if (m_pBus->busMode() & qtractorBus::Output) {
			pStrip = (pMixer->outputRack())->findStrip(m_pBus->monitor_out());
			if (pStrip) {
				pStrip->clear();
				if (m_busMode & qtractorBus::Output) {
					strips.append(pStrip);
				} else {
					(pMixer->outputRack())->removeStrip(pStrip);
				}
			}
		}
	}

	// Close all applicable tracks...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->inputBus() == m_pBus)
			pTrack->setInputBusName(m_sBusName);
		if (pTrack->outputBus() == m_pBus)
			pTrack->setOutputBusName(m_sBusName);
		if (pTrack->inputBus() == m_pBus || pTrack->outputBus() == m_pBus) {
			if (pMixer) {
				pStrip = (pMixer->trackRack())->findStrip(pTrack->monitor());
				if (pStrip) {
					pStrip->clear();
					strips.append(pStrip);
				}
			}
			pTrack->close();
		}
	}

	// May close now the bus...
	m_pBus->close();

	// Set new properties...
	m_pBus->setBusName(m_sBusName);
	m_pBus->setBusMode(m_busMode);
	m_pBus->setMonitor(m_bMonitor);
	// Special case for typed buses...
	if (pAudioBus) {
		pAudioBus->setChannels(m_iChannels);
		pAudioBus->setAutoConnect(m_bAutoConnect);
	}
	if (pMidiBus) {
		pMidiBus->setInstrumentName(m_sInstrumentName);
	}

	// May reopen up the bus...
	m_pBus->open();

	// Yet special for audio buses...
	if (pAudioBus)
		pAudioBus->autoConnect();

	// Restore previous connections...
	if (m_busMode & qtractorBus::Input)
		m_pBus->updateConnects(qtractorBus::Input, inputs, true);
	if (m_busMode & qtractorBus::Output)
		m_pBus->updateConnects(qtractorBus::Output, outputs, true);

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
			if (pTracks)
				(pTracks->trackList())->updateTrack(pTrack);
		}
	}

	// Update (reset) all applicable mixer strips...
	if (pMixer) {
		QListIterator<qtractorMixerStrip *> iter(strips);
		while (iter.hasNext()) {
			pStrip = iter.next();
			if (pStrip->track())
				pStrip->setTrack(pStrip->track());
			else
			if (pStrip->bus())
				pStrip->setBus(pStrip->bus());
		}
		pMixer->updateBuses();
	}

	// Swap saved bus properties...
	m_busMode   = busMode;
	m_sBusName  = sBusName;
	m_bMonitor  = bMonitor;
	m_iChannels = iChannels;
	m_bAutoConnect = bAutoConnect;
	m_sInstrumentName = sInstrumentName;

	// Carry on...
	pSession->setPlaying(bPlaying);
	pSession->unlock();

	// Done.
	return true;
}


// Delete bus.
bool qtractorBusCommand::deleteBus (void)
{
	if (m_pBus == NULL)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
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

	pSession->lock();
	pSession->setPlaying(false);

	// Close all applicable tracks (and mixer strips)...
	qtractorMixerStrip *pStrip;
	qtractorMixer *pMixer = pMainForm->mixer();
	QList<qtractorMixerStrip *> strips;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->inputBus() == m_pBus || pTrack->outputBus() == m_pBus) {
			pTrack->close();
			if (pMixer) {
				pStrip = (pMixer->trackRack())->findStrip(pTrack->monitor());
				if (pStrip) {
					pStrip->clear();
					strips.append(pStrip);
				}
			}
		}			
	}

	// May close now the bus...
	m_pBus->close();

	// And remove it...
	pEngine->removeBus(m_pBus);
	m_pBus = NULL;

	// Better update special buses anyway...
	switch (pEngine->syncType()) {
	case qtractorTrack::Audio: {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine) {
			pAudioEngine->resetPlayerBus();
			pAudioEngine->resetMetroBus();
		}
	}
	case qtractorTrack::Midi: {
		qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
		if (pMidiEngine) {
			pMidiEngine->resetControlBus();
			pMidiEngine->resetMetroBus();
		}
		break;
	}
	default:
		break;
	}

	// Update mixer (clean old strips...)
	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks && pMixer) {
		QListIterator<qtractorMixerStrip *> iter(strips);
		while (iter.hasNext()) {
			qtractorTrack *pTrack = iter.next()->track();
			if (pTrack) {
				pTrack->open();
				// Update track list item...
				(pTracks->trackList())->updateTrack(pTrack);
			}
		}
		pMixer->updateBuses();
	}

	// Carry on...
	pSession->setPlaying(bPlaying);
	pSession->unlock();

	// Done.
	return true;
}



// Monitor meter accessor.
qtractorMeter *qtractorBusCommand::meter (void) const
{
	qtractorBus *pBus = bus();
	if (pBus == NULL)
		return NULL;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return NULL;

	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer == NULL)
		return NULL;

	// Mixer strip determination...
	qtractorMixerStrip *pStrip = NULL;
	if ((busMode() & qtractorBus::Input) && pBus->monitor_in())
		pStrip = pMixer->inputRack()->findStrip(pBus->monitor_in());
	else
	if ((busMode() & qtractorBus::Output) && pBus->monitor_out())
		pStrip = pMixer->outputRack()->findStrip(pBus->monitor_out());

	return (pStrip ? pStrip->meter() : NULL);
}


//----------------------------------------------------------------------
// class qtractorCreateBusCommand - implementation.
//

// Constructor.
qtractorCreateBusCommand::qtractorCreateBusCommand (void)
	: qtractorBusCommand(QObject::tr("create bus"))
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
qtractorUpdateBusCommand::qtractorUpdateBusCommand ( qtractorBus *pBus )
	: qtractorBusCommand(QObject::tr("update bus"), pBus)
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
qtractorDeleteBusCommand::qtractorDeleteBusCommand ( qtractorBus *pBus )
	: qtractorBusCommand(QObject::tr("delete bus"), pBus)
{
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
// class qtractorBusMonitorCommand - implementation.
//

// Constructor.
qtractorBusMonitorCommand::qtractorBusMonitorCommand (
	qtractorBus *pBus, bool bMonitor )
	: qtractorBusCommand(QObject::tr("bus pass-through"), pBus, pBus->busMode())
{
	setMonitor(bMonitor);
}


// Bus-gain command method.
bool qtractorBusMonitorCommand::redo (void)
{
	qtractorBus *pBus = bus();
	if (pBus == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Set Bus gain (repective monitor gets set too...)
	float bMonitor = pBus->isMonitor();
	pBus->setMonitor(qtractorBusCommand::isMonitor());
	qtractorBusCommand::setMonitor(bMonitor);

	// Update (reset) all applicable mixer strips...
	qtractorMixer *pMixer = pMainForm->mixer();
	if (pMixer) {
		if (pBus->busMode() & qtractorBus::Input) {
			pMixer->updateBusStrip(pMixer->inputRack(),
				pBus, qtractorBus::Input, true);
		}
		if (pBus->busMode() & qtractorBus::Output) {
			pMixer->updateBusStrip(pMixer->outputRack(),
				pBus, qtractorBus::Output, true);
		}
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorBusGainCommand - implementation.
//

// Constructor.
qtractorBusGainCommand::qtractorBusGainCommand ( qtractorBus *pBus,
	qtractorBus::BusMode busMode, float fGain )
	: qtractorBusCommand(QObject::tr("bus gain"), pBus, busMode)
{
	m_fGain = fGain;
	m_fPrevGain = 1.0f;

	qtractorMeter *pMeter = meter();
	if (pMeter)
		m_fPrevGain = pMeter->prevGain();

	// Try replacing an previously equivalent command...
	static qtractorBusGainCommand *s_pPrevGainCommand = NULL;
	if (s_pPrevGainCommand) {
		qtractorSession *pSession = qtractorSession::getInstance();
		qtractorCommand *pLastCommand
			= (pSession->commands())->lastCommand();
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
				if (iPrevSign == iCurrSign || m_fGain == m_fPrevGain) {
					m_fPrevGain = fLastGain;
					(pSession->commands())->removeLastCommand();
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

	qtractorMeter *pMeter = meter();
	if (pMeter)
		pMeter->setGain(m_fGain);

	// MIDI buses are special...
	if (pBus->busType() == qtractorTrack::Midi) {
		// Now we gotta make sure of proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus)
			pMidiBus->setMasterVolume(m_fGain);
	}

	// Set undo value...
	m_fPrevGain = m_fGain;
	m_fGain     = fGain;

	return true;
}


//----------------------------------------------------------------------
// class qtractorBusPanningCommand - implementation.
//

// Constructor.
qtractorBusPanningCommand::qtractorBusPanningCommand ( qtractorBus *pBus,
	qtractorBus::BusMode busMode, float fPanning )
	: qtractorBusCommand(QObject::tr("bus pan"), pBus, busMode)
{
	m_fPanning = fPanning;
	m_fPrevPanning = 0.0f;
	
	qtractorMeter *pMeter = meter();
	if (pMeter)
		m_fPrevPanning = pMeter->prevPanning();

	// Try replacing an previously equivalent command...
	static qtractorBusPanningCommand *s_pPrevPanningCommand = NULL;
	if (s_pPrevPanningCommand) {
		qtractorSession *pSession = qtractorSession::getInstance();
		qtractorCommand *pLastCommand
			= (pSession->commands())->lastCommand();
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
				if (iPrevSign == iCurrSign || m_fPanning == m_fPrevPanning) {
					m_fPrevPanning = fLastPanning;
					(pSession->commands())->removeLastCommand();
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

	// Set Bus panning (repective monitor gets set too...)
	float fPanning = m_fPrevPanning;

	qtractorMeter *pMeter = meter();
	if (pMeter)
		pMeter->setPanning(m_fPanning);

	// MIDI buses are special...
	if (pBus->busType() == qtractorTrack::Midi) {
		// Now we gotta make sure of proper MIDI bus...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus)
			pMidiBus->setMasterPanning(m_fPanning);
	}

	// Set undo value...
	m_fPrevPanning = m_fPanning;
	m_fPanning     = fPanning;

	return true;
}


// end of qtractorEngineCommand.cpp
