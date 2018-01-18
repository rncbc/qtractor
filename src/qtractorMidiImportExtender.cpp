// qtractorMidiImportExtender.cpp
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiImportExtender.h"
#include "qtractorSession.h"
#include "qtractorPlugin.h"
#include "qtractorOptions.h"
#include "qtractorPluginListDocument.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiManager.h"
#include "qtractorTrackCommand.h"
#include "qtractorMidiClip.h"
#include <QDomDocument>
#include <QElapsedTimer>

//----------------------------------------------------------------------
// class qtractorMidiImportExtender -- MIDI import extender class.
//

//
// It is not a good idea to delete plugins: At least DSSI plugins supporting
// run_multiple_synths interface get confused and cause crashes when deleting.
//
// To avoid deletion of plugins the plugins are kept in static m_pPluginList
// which is created as soon as pluginList() is called (typically by settings
// dialog).
// At MIDI import process the plugins in plugin list are reused by moving to
// first track generated to avoid wasting resources.
//


// Pointer to singleton plugin list for display.
qtractorPluginList *qtractorMidiImportExtender::m_pPluginList = NULL;
// Keep info that plugin list was empied.
bool qtractorMidiImportExtender::m_bPluginListIsEmpty = true;

// Constructor.
qtractorMidiImportExtender::qtractorMidiImportExtender()
{
	// Get global session object
	qtractorSession *pSession = qtractorSession::getInstance();
	if (!pSession)
		return;
	// Get global options object
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (!pOptions)
		return;

	// Keep a shadow copy of settings.
	if (!pOptions->sMidiImportPlugins.isEmpty()) {
		m_extendedSettings.pPluginDomDocument = new QDomDocument("qtractorMidiImport");
		m_extendedSettings.pPluginDomDocument->setContent(pOptions->sMidiImportPlugins);
	} else
		m_extendedSettings.pPluginDomDocument = NULL;
	m_extendedSettings.sMidiImportInstInst = pOptions->sMidiImportInstInst;
	m_extendedSettings.sMidiImportDrumInst = pOptions->sMidiImportDrumInst;
	m_extendedSettings.iMidiImportInstBank = pOptions->iMidiImportInstBank;
	m_extendedSettings.iMidiImportDrumBank = pOptions->iMidiImportDrumBank;
	m_extendedSettings.eMidiImportTrackNameType = (TrackNameType) pOptions->iMidiImportTrackName;

	// Get reference of the last command before dialog.
	m_pLastUndoCommand = NULL;
	qtractorCommandList *pCommands = pSession->commands();
	if (pCommands)
		m_pLastUndoCommand = pCommands->lastCommand();

	// Setup track number here. When setting track name, imported tracks are
	// already created.
	m_iTrackNumber = pSession->tracks().count();

	m_bAutoDeactivateWasDisabled = false;
}


// Destructor.
qtractorMidiImportExtender::~qtractorMidiImportExtender()
{
	// cleanup document
	if (m_extendedSettings.pPluginDomDocument)
		delete m_extendedSettings.pPluginDomDocument;
}


// Save all settings to global options.
void qtractorMidiImportExtender::saveSettings()
{
	// Get global options object
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (!pOptions)
		return;

	// Xmlize plugin list and store in options
	pluginListToDocument();
	QString strPluginListXML;
	if (m_extendedSettings.pPluginDomDocument)
		// Store as XML string without whitespaces.
		strPluginListXML = m_extendedSettings.pPluginDomDocument->toString(-1);
	pOptions->sMidiImportPlugins = strPluginListXML;

	// Store other options
	pOptions->sMidiImportInstInst = m_extendedSettings.sMidiImportInstInst;
	pOptions->sMidiImportDrumInst = m_extendedSettings.sMidiImportDrumInst;
	pOptions->iMidiImportInstBank = m_extendedSettings.iMidiImportInstBank;
	pOptions->iMidiImportDrumBank = m_extendedSettings.iMidiImportDrumBank;
	pOptions->iMidiImportTrackName = (int)m_extendedSettings.eMidiImportTrackNameType;
}


// Undo commands done since creation (for CANCEL on dialog).
void qtractorMidiImportExtender::backoutCommandList()
{
	// Backout all commands made in this dialog-session.
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->commands()->backout(m_pLastUndoCommand);
}


// Restore undo command list to the point of creation without performing
// plugin list actions. Do this to avoid commands added to undo menu (for OK
// on dialogs).
void qtractorMidiImportExtender::restoreCommandList()
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorCommandList *pCommands = pSession->commands();
		// Remove all commands without execution - tested: command list keeps
		// them with proper auto-delete so they are cleaned up properly on
		// session close.
		for (qtractorCommand *pCurrLastCommand = pCommands->lastCommand();
			pCurrLastCommand != m_pLastUndoCommand;
			pCurrLastCommand = pCommands->lastCommand())
			pCommands->removeLastCommand();
	}
}


// Accessor/creator for plugin list.
qtractorPluginList *qtractorMidiImportExtender::pluginListForGui()
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (!pSession)
		return NULL;

	// Create plugin list.
	if (!m_pPluginList) {
		m_pPluginList = new qtractorPluginList(0, qtractorPluginList::Midi);
		// GUI plugin list will never be heard so do no processing.
		m_pPluginList->forceNoProcessing(true);
	}
	// Were plugins removed?
	if (m_bPluginListIsEmpty) {
		// Create plugins from document.
		if (m_extendedSettings.pPluginDomDocument) {
			qtractorPluginListDocument pluginListDocument(
						m_extendedSettings.pPluginDomDocument,
						m_pPluginList);
			// Get root element and check for proper taq name.
			QDomElement elem = m_extendedSettings.pPluginDomDocument->documentElement();
			if (elem.tagName() == "PluginList") {
				pluginListDocument.loadElement(&elem);
			}
		}
		// Plugin list needs channels set > 0. Otherwise all entries in
		// plugin selection dialog are disabled. Try same channel count as
		// master output audio bus. That is default connection for imported
		// tracks.
		qtractorAudioBus *pAudioBus = NULL;
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		for (qtractorBus *pBus = (pAudioEngine->buses()).first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				pAudioBus = static_cast<qtractorAudioBus *> (pBus);
				break;
			}
		}
		if (pAudioBus)
			m_pPluginList->setChannels(pAudioBus->channels(),
					qtractorPluginList::Midi);
		else
			// Use reasonable fallback channel count.
			m_pPluginList->setChannels(2, qtractorPluginList::Midi);
		// Plugin list is filled now.
		m_bPluginListIsEmpty = false;
	}

	return m_pPluginList;
}


// Clear singleton plugin list and delete plugins
void qtractorMidiImportExtender::clearPluginList()
{
	if (m_pPluginList) {
		delete m_pPluginList;
		m_pPluginList = NULL;
	}
	m_bPluginListIsEmpty = true;
}


// Add plugins to track - this must be done before track is added to session.
// There was no way found to wait for plugins e.g. loading soundfonts.
void qtractorMidiImportExtender::prepareTrackForExtension(qtractorTrack *pTrack)
{
	// Are there plugins to add ?
	if (m_extendedSettings.pPluginDomDocument) {

		// Bail out if something is wrong.
		if (!pTrack)
			return;
		qtractorSession *pSession = qtractorSession::getInstance();
		if (!pSession)
			return;
		qtractorPluginList *pPluginList = pTrack->pluginList();
		if (!pPluginList)
			return;

		// Auto deactivation must be disabled to make plugins process program/bank
		// changes.
		if (!m_bAutoDeactivateWasDisabled && pSession->isAutoDeactivate()) {
			pSession->setAutoDeactivate(false);
			m_bAutoDeactivateWasDisabled = true;
		}

		// Deactivate processing for plugin list - we have to choose the hard way
		// because during adding tracks to session activation state of plugins
		// flip. It seems activated plugins must report proper activation state
		// for next actions - otherwise output is scrambled.
		pPluginList->forceNoProcessing(true);

		// Add plugins early. Several options were tested to add plugins later
		// (when tracks are complete / added to session): it was not possible to
		// set patch reproducable.

		// Take plugins for first track from plugin list.
		if (m_pPluginList && !m_bPluginListIsEmpty)
		{
			// Move plugins to target.
			while (m_pPluginList->count())
				pPluginList->movePlugin(m_pPluginList->first(), NULL);
			m_bPluginListIsEmpty = true;
		}
		// All further are cloned by our document - if there is one.
		else {
			// Get root element and check for proper taq name.
			QDomElement elem =
					m_extendedSettings.pPluginDomDocument->documentElement();
			if (elem.tagName() == "PluginList") {
				qtractorPluginListDocument pluginListDocument(
							m_extendedSettings.pPluginDomDocument, pPluginList);
				pluginListDocument.loadElement(&elem);
			}
		}
	}
}


// Set instrument and track-name. Tracks must be 'complete' (have an output bus).
bool qtractorMidiImportExtender::finishTracksForExtension(
		QList<qtractorTrack *> *pImportedTracks)
{
	// Bail out if something is wrong.
	if (!pImportedTracks || pImportedTracks->count() == 0)
		return false;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (!pSession)
		return false;

	qtractorTrack *pTrack;

	// Were plugins added?
	if (m_extendedSettings.pPluginDomDocument) {
		// Loop all tracks to wake plugins.
		for (pTrack = pImportedTracks->first(); pTrack; pTrack = pTrack->next()) {
			qtractorPluginList *pPluginList = pTrack->pluginList();
			if (!pPluginList)
				continue;
			if (pPluginList->count() > 0)
				pPluginList->forceNoProcessing(false);
		}

		// Make sure plugins are able to process program/bank changes send below.
		pSession->stabilize(100);
	}

	// Return value notifying import process of changes done here.
	bool bOneOrMoreTracksChanged = false;

	// Loop all tracks to set patches.
	for (pTrack = pImportedTracks->first(); pTrack; pTrack = pTrack->next()) {

		qtractorPluginList *pPluginList = pTrack->pluginList();
		if (!pPluginList)
			continue;

		// Get current track properties.
		qtractorTrack::Properties &properties = pTrack->properties();
		bool bIsDrumTrack = properties.midiChannel == 9;

		// Midi bank.
		int iBankNew = properties.midiBank;
		if (!bIsDrumTrack) {
			// Instruments: apply bank settings only if clip does not suggest.
			if (iBankNew < 0)
				iBankNew = m_extendedSettings.iMidiImportInstBank;
		} else
			// Follow settings for drums whatever clip suggests.
			iBankNew = m_extendedSettings.iMidiImportDrumBank;
		// And bank action...
		if (iBankNew >= 0) {
			// By default midiBankSelMethod is -1. If not set it here, bank change
			// is not send to instrument.
			properties.midiBankSelMethod = 0;
			properties.midiBank = iBankNew;
			bOneOrMoreTracksChanged = true;
		}

		// Midi prog.
		// Special case for drum-tracks program: Some MIDI files out in the wild do
		// not set program for drum tracks. Give those a reasonable default.
		if (bIsDrumTrack && properties.midiProg < 0)
			// Set standard drums.
			properties.midiProg = 0;
		// Initial program change was missed likely (plugins did not process).
		if (properties.midiProg >= 0)
			bOneOrMoreTracksChanged = true;

		// Instrument.
		QString strInstrumentNew = bIsDrumTrack ?
				m_extendedSettings.sMidiImportDrumInst :
				m_extendedSettings.sMidiImportInstInst;
		if (!strInstrumentNew.isEmpty())
			bOneOrMoreTracksChanged = true;

		// Do set patch (instrument / midiprog / midibank).
		qtractorMidiBus *pMidiBus =
			static_cast<qtractorMidiBus *> (pTrack->outputBus());
		if (pMidiBus) {
			if (properties.midiProg >= 0) {
				pMidiBus->setPatch(properties.midiChannel,
					strInstrumentNew,
					properties.midiBankSelMethod,
					properties.midiBank,
					properties.midiProg,
					pTrack);
			}
		}

		// Create track name.  Set names only for tracks making noise after
		// import. This indicates from first glance that track is special or
		// broken or...
		QString sTrackName;
		if (properties.midiBank >= 0 && properties.midiProg >= 0) {
			// Set track name based upon type set up.
			switch (m_extendedSettings.eMidiImportTrackNameType) {
			case Midifile:
				// MIDI filename is default: no modification.
				break;
			case Track:
				sTrackName = pSession->uniqueTrackName(
						QString("Track %1").arg(++m_iTrackNumber));
				break;
			case PatchName:
				// Instrument track.
				if (!bIsDrumTrack) {
					// Collect what's necessary to get patch name.
					qtractorMidiManager *pMidiManager = pPluginList->midiManager();
					if (!pMidiManager)
						break;

					const qtractorMidiManager::Instruments& list
						= pMidiManager->instruments();
					QString sInstrumentName = m_extendedSettings.sMidiImportInstInst;
					if (!list.contains(sInstrumentName))
						break;

					const qtractorMidiManager::Banks& banks
						= list[sInstrumentName];
					if (!banks.contains(properties.midiBank))
						break;

					// A patch name was found!
					const qtractorMidiManager::Progs& progs = banks[properties.midiBank].progs;
					sTrackName = progs[properties.midiProg];
				}

				// Drum track's name is fixed - patch names are not really useful at drums.
				else
					sTrackName = QObject::tr("Drums");
				break;
			}
		}

		// Do set track name.
		if (!sTrackName.isEmpty()) {
			properties.trackName = sTrackName;
			pSession->releaseTrackName(pTrack);
			pTrack->setTrackName(sTrackName);
			pSession->acquireTrackName(pTrack);
			bOneOrMoreTracksChanged = true;
		}

		// Apply all to track also.
		pTrack->setProperties(properties);
	}

	// Restore plugin auto deactivation.
	if (m_bAutoDeactivateWasDisabled) {
		pSession->setAutoDeactivate(true);
		m_bAutoDeactivateWasDisabled = false;
	}
	return bOneOrMoreTracksChanged;
}


// Accessor to extended settings.
qtractorMidiImportExtender::exportExtensionsData *qtractorMidiImportExtender::exportExtensions()
{
	return &m_extendedSettings;
}


// Xmlize plugin list.
void qtractorMidiImportExtender::pluginListToDocument()
{
	// Remove old plugin list document.
	if (m_extendedSettings.pPluginDomDocument) {
		delete m_extendedSettings.pPluginDomDocument;
		m_extendedSettings.pPluginDomDocument = NULL;
	}

	if (m_pPluginList && m_pPluginList->count()) {
		// Save plugin settings to new document.
		m_extendedSettings.pPluginDomDocument = new QDomDocument("qtractorMidiImport");
		qtractorPluginListDocument pluginListDocument(m_extendedSettings.pPluginDomDocument, m_pPluginList);
		QDomElement elem = m_extendedSettings.pPluginDomDocument->createElement("PluginList");
		m_pPluginList->saveElement(&pluginListDocument, &elem);
		m_extendedSettings.pPluginDomDocument->appendChild(elem);
	}
}


// end of qtractorMidiImportExtender.cpp
