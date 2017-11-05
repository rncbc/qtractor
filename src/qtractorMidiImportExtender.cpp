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
#include "QDomDocument"

// It is not a good idea to delete plugins within a session. To avoid
// deletion, the plugins are kept in static m_pPluginList. As soon
// as an object of qtractorMidiImportExtender is created, a plugin-list
// is created to. It is either deleted by:
// * closing session which calls qtractorMidiImportExtender::clearPluginList
// * on import / first track: After moving plugins to target track, plugin
//   list is empty and can be deleted
qtractorPluginList *qtractorMidiImportExtender::m_pPluginList = NULL;

// Constructor.
qtractorMidiImportExtender::qtractorMidiImportExtender() :
	m_pPluginDomDocument(NULL), m_pPluginListDocument(NULL),
	m_iTrackNumber(0)
{
	// Get global session object
	qtractorSession *pSession = qtractorSession::getInstance();
	if (!pSession)
		return;
	// Get global options object
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (!pOptions)
		return;

	// Create and fill plugin-lists if when called first time in session
	if (!m_pPluginList) {
		m_pPluginList = new qtractorPluginList(0, 0);

		// Restore plugin-list from persistant options.
		if (!pOptions->sMidiImportPlugins.isEmpty()) {
			QDomDocument domDocument("qtractorMidiImport");
			if (domDocument.setContent(pOptions->sMidiImportPlugins)) {
				qtractorPluginListDocument pluginListDocument(&domDocument, m_pPluginList);
				// Get root element and check for proper taq name.
				QDomElement elem = domDocument.documentElement();
				if (elem.tagName() == "PluginList")
					m_pPluginList->loadElement(&pluginListDocument, &elem);
			}
		}
		// Output bus gets to be the first available output bus (master).
		qtractorAudioBus *pAudioBus = NULL;
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		for (qtractorBus *pBus = (pAudioEngine->buses()).first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				pAudioBus = static_cast<qtractorAudioBus *> (pBus);
				break;
			}
		}

		// Plugin-List needs one or more channels - just to allow adding plugins.
		if (pAudioBus)
			m_pPluginList->setChannels(pAudioBus->channels(),
				qtractorPluginList::Midi);
		else
			m_pPluginList->setChannels(1, qtractorPluginList::Midi);
	}

	// Keep a shadow copy of settings.
	m_tExtendedSettings.sMidiImportInstInst = pOptions->sMidiImportInstInst;
	m_tExtendedSettings.sMidiImportDrumInst = pOptions->sMidiImportDrumInst;
	m_tExtendedSettings.iMidiImportInstBank = pOptions->iMidiImportInstBank;
	m_tExtendedSettings.iMidiImportDrumBank = pOptions->iMidiImportDrumBank;
	m_tExtendedSettings.eMidiImportTrackNameType = (TrackNameType) pOptions->iMidiImportTrackName;

	// Get reference of the last command before dialog.
	m_pLastCommand = NULL;
	qtractorCommandList *pCommands = pSession->commands();
	m_pLastCommand = pCommands->lastCommand();
}


// Destructor.
qtractorMidiImportExtender::~qtractorMidiImportExtender()
{
}


// Save all settings to global options.
void qtractorMidiImportExtender::saveSettings()
{
	// Get global options object
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (!pOptions)
		return;

	// Xmlize plugin list and staore in options
	pluginListToDocument();
	QString strPluginListXML;
	if (m_pPluginDomDocument)
		// Store as XML string without whitespaces.
		strPluginListXML = m_pPluginDomDocument->toString(-1);
	pOptions->sMidiImportPlugins = strPluginListXML;

	// Store other options
	pOptions->sMidiImportInstInst = m_tExtendedSettings.sMidiImportInstInst;
	pOptions->sMidiImportDrumInst = m_tExtendedSettings.sMidiImportDrumInst;
	pOptions->iMidiImportInstBank = m_tExtendedSettings.iMidiImportInstBank;
	pOptions->iMidiImportDrumBank = m_tExtendedSettings.iMidiImportDrumBank;
	pOptions->iMidiImportTrackName = (int)m_tExtendedSettings.eMidiImportTrackNameType;
}


// Undo commands done since creation.
void qtractorMidiImportExtender::backoutCommandList()
{
	// Backout all commands made in this dialog-session.
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->commands()->backout(m_pLastCommand);
}


// Restore command list to the point of creation.
void qtractorMidiImportExtender::restoreCommandList()
{
	// Avoid inconsistent execution of undo menu entries for actions done
	// within dialog.
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorCommandList *pCommands = pSession->commands();
		// Remove all commands without execution - tested: command-list keeps
		// them with proper auto-delete so they are cleaned up properly on
		// session close.
		for (qtractorCommand *pCurrLastCommand = pCommands->lastCommand();
			pCurrLastCommand != m_pLastCommand;
			pCurrLastCommand = pCommands->lastCommand())
			pCommands->removeLastCommand();
	}
}


// Accessor to plugin list.
qtractorPluginList *qtractorMidiImportExtender::pluginList()
{
	return m_pPluginList;
}


// Clear singleton plugin-list and delete plugins
void qtractorMidiImportExtender::clearPluginList()
{
	if (m_pPluginList) {
		delete m_pPluginList;
		m_pPluginList = NULL;
	}
}

// Add plugins to track and return a track-edit-command for setting bank
qtractorEditTrackCommand *qtractorMidiImportExtender::modifyBareTrackAndGetEditCommand(
		qtractorTrack *pTrack)
{
	// Bail out if something is seriously wrong.
	if (!pTrack)
		return NULL;
	qtractorPluginList *pTargetList = pTrack->pluginList();
	if (!pTargetList)
		return NULL;

	// Add plugins

	// Take plugins for first track from plugin-list.
	if (m_pPluginList) {
		// First ensure we have a valid document for further tracks
		pluginListToDocument();

		// Move plugins to target.
		while (m_pPluginList->count())
			pTargetList->movePlugin(m_pPluginList->first(), NULL);

		// Remove list: It is empty and no more required
		clearPluginList();
	}

	// all further are cloned by our document - if there is one.
	else if (m_pPluginListDocument) {
		// Get root element and check for proper taq name.
		QDomElement elem = m_pPluginDomDocument->documentElement();
		if (elem.tagName() == "PluginList")
			pTargetList->loadElement(m_pPluginListDocument, &elem);
	}


	// Set bank.
	bool bIsDrumTrack = pTrack->midiChannel() == 9;
	int iBankNew = bIsDrumTrack ?
		m_tExtendedSettings.iMidiImportDrumBank :
				m_tExtendedSettings.iMidiImportInstBank;

	bool bTrackChanged = false;
	qtractorTrack::Properties &properties = pTrack->properties();
	if (iBankNew >= 0) {
		properties.midiBank = iBankNew;
		// By default midiBankSelMethod is -1. If we don't set it here bank
		// change is not sent to instrument.
		properties.midiBankSelMethod = 0;
		bTrackChanged = true;
		// Special case for drum-tracks: Some MIDI files out in the wild do not
		// set program for drum-tracks. Give those a reasonable default.
		if (bIsDrumTrack && properties.midiProg < 0)
			// Set standard drums.
			properties.midiProg = 0;
	}

	qtractorEditTrackCommand *pEditCommand = NULL;
	if (bTrackChanged)
		pEditCommand = new qtractorEditTrackCommand(pTrack, properties);
	return pEditCommand;
}


bool qtractorMidiImportExtender::modifyFinishedTrack(qtractorTrack *pTrack)
{
	// Bail out if something seriously is wrong.
	if (!pTrack)
		return false;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (!pSession)
		return false;

	bool bTrackChanged = false;
	int iMidiChannel = pTrack->midiChannel();
	bool bIsDrumTrack = pTrack->midiChannel() == 9;

	// Set instrument.
	qtractorMidiBus *pMidiBus =
		static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus) {
		// Get current patch
		const qtractorMidiBus::Patch& patch = pMidiBus->patch(iMidiChannel);
		// set instrument / bank for tracks with program set only.
		if (patch.prog >= 0) {
			// Set instrument.
			QString strInstrumentNew = bIsDrumTrack ?
				m_tExtendedSettings.sMidiImportDrumInst :
						m_tExtendedSettings.sMidiImportInstInst;

			// Update patch if necessary.
			if (!strInstrumentNew.isEmpty()) {
				pMidiBus->setPatch(iMidiChannel,
					strInstrumentNew,
					patch.bankSelMethod,
					patch.bank,
					patch.prog,
					pTrack);
				bTrackChanged = true;
			}
		}
	}

	// Set track name.
	QString sTrackName;
	switch (m_tExtendedSettings.eMidiImportTrackNameType) {
	case Midifile:
		// Default: no modification
		break;
	case Track: {
		// Set names only for tracks making music after import
		if (pTrack->midiBank() < 0 || pTrack->midiProg() < 0)
			break;
		sTrackName = pSession->uniqueTrackName(
						QString("Track %1").arg(++m_iTrackNumber));
		break;
	}
	case PatchName:
		// Instrument track.
		if (!bIsDrumTrack) {
			// Other bail out checks...
			if (pTrack->midiBank() < 0 || pTrack->midiProg() < 0)
				break;
			qtractorPluginList *pPluginList = pTrack->pluginList();
			if (!pPluginList)
				break;
			qtractorMidiManager *pMidiManager = pPluginList->midiManager();
			if (!pMidiManager)
				break;

			const qtractorMidiManager::Instruments& list
				= pMidiManager->instruments();
			QString sInstrumentName = m_tExtendedSettings.sMidiImportInstInst;
			if (!list.contains(sInstrumentName))
				break;

			// Banks reference...
			const qtractorMidiManager::Banks& banks
				= list[sInstrumentName];
			if (banks.contains(pTrack->midiBank())) {
				const qtractorMidiManager::Progs& progs = banks[pTrack->midiBank()].progs;
				sTrackName = progs[pTrack->midiProg()];
			}
		}
		// Drum track.
		else
			sTrackName = QObject::tr("Drums");
		break;
	}
	if (!sTrackName.isEmpty()) {
		pSession->releaseTrackName(pTrack);
		pTrack->setTrackName(sTrackName);
		pSession->acquireTrackName(pTrack);
		bTrackChanged = true;
	}

	return bTrackChanged;
}


// Accessor to exteneded settings.
qtractorMidiImportExtender::exportExtensionsData *qtractorMidiImportExtender::exportExtensions()
{
	return &m_tExtendedSettings;
}

// Xmlize plugin List
void qtractorMidiImportExtender::pluginListToDocument()
{
	// remove old plugin-list settings
	if (m_pPluginDomDocument) {
		delete m_pPluginDomDocument;
		m_pPluginDomDocument = NULL;
	}
	if (m_pPluginListDocument) {
		delete m_pPluginListDocument;
		m_pPluginListDocument = NULL;
	}

	// Xmlize plugin-list for options and cloning.
	if (m_pPluginList->count()) {
		m_pPluginDomDocument = new QDomDocument("qtractorMidiImport");
		m_pPluginListDocument = new qtractorPluginListDocument(m_pPluginDomDocument, m_pPluginList);

		// Save plugin settings to document.
		QDomElement elem = m_pPluginDomDocument->createElement("PluginList");
		m_pPluginList->saveElement(m_pPluginListDocument, &elem);
		m_pPluginDomDocument->appendChild(elem);
	}
}



// end of qtractorMidiImportExtender.cpp
