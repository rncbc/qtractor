// qtractorPluginCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorPluginCommand.h"
#include "qtractorPluginForm.h"

#include "qtractorPluginListView.h"

#include "qtractorInsertPlugin.h"

#include "qtractorSession.h"
#include "qtractorMidiManager.h"

#include "qtractorMainForm.h"
#include "qtractorTracks.h"
#include "qtractorMixer.h"


//----------------------------------------------------------------------
// class qtractorPluginCommand - implementation
//

// Constructor.
qtractorPluginCommand::qtractorPluginCommand ( const QString& sName,
	qtractorPlugin *pPlugin ) : qtractorCommand(sName)
{
	if (pPlugin)
		m_plugins.append(pPlugin);	

	setRefresh(false);
}


// Destructor.
qtractorPluginCommand::~qtractorPluginCommand (void)
{
	if (isAutoDelete())
		qDeleteAll(m_plugins);
	m_plugins.clear();
}


// Add new plugin(s) command methods.
bool qtractorPluginCommand::addPlugins (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

//	pSession->lock();

	// Add all listed plugins, in order...
	QListIterator<qtractorPlugin *> iter(m_plugins);
	while (iter.hasNext()) {
		qtractorPlugin *pPlugin = iter.next();
		qtractorPluginList *pPluginList = pPlugin->list();
		if (pPluginList)
			pPluginList->addPlugin(pPlugin);
	}
	// Avoid the disposal of the plugin reference(s).
	setAutoDelete(false);

//	pSession->unlock();

	return true;
}


// Remove existing plugin(s) command methods.
bool qtractorPluginCommand::removePlugins (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

//	pSession->lock();

	// Unlink all listed plugins, in order...
	QListIterator<qtractorPlugin *> iter(m_plugins);
	iter.toBack();
	while (iter.hasPrevious()) {
		qtractorPlugin *pPlugin = iter.previous();
		qtractorPluginList *pPluginList = pPlugin->list();
		if (pPluginList)
			pPluginList->removePlugin(pPlugin);
	}
	// Allow the disposal of the plugin reference(s).
	setAutoDelete(true);

//	pSession->unlock();

	return true;
}


//----------------------------------------------------------------------
// class qtractorAddPluginCommand - implementation
//

// Constructor.
qtractorAddPluginCommand::qtractorAddPluginCommand ( qtractorPlugin *pPlugin )
	: qtractorPluginCommand(QObject::tr("add plugin"), pPlugin)
{
}


// Plugin insertion command methods.
bool qtractorAddPluginCommand::redo (void)
{
	return addPlugins();
}

bool qtractorAddPluginCommand::undo (void)
{
	return removePlugins();
}


//----------------------------------------------------------------------
// class qtractorAddInsertPluginCommand - implementation
//

// Constructor.
qtractorAddInsertPluginCommand::qtractorAddInsertPluginCommand (
	qtractorPlugin *pPlugin ) : qtractorPluginCommand(
		QObject::tr("add insert"), pPlugin)
{
}


// Plugin insertion command methods.
bool qtractorAddInsertPluginCommand::redo (void)
{
	return addPlugins();
}

bool qtractorAddInsertPluginCommand::undo (void)
{
	return removePlugins();
}


//----------------------------------------------------------------------
// class qtractorAddAuxSendPluginCommand - implementation
//

// Constructor.
qtractorAddAuxSendPluginCommand::qtractorAddAuxSendPluginCommand (
	qtractorPlugin *pPlugin ) : qtractorPluginCommand(
		QObject::tr("add aux-send"), pPlugin)
{
}


// Plugin insertion command methods.
bool qtractorAddAuxSendPluginCommand::redo (void)
{
	return addPlugins();
}

bool qtractorAddAuxSendPluginCommand::undo (void)
{
	return removePlugins();
}


//----------------------------------------------------------------------
// class qtractorAuxSendPluginCommand - implementation
//

// Constructor.
qtractorAuxSendPluginCommand::qtractorAuxSendPluginCommand (
	qtractorPlugin *pPlugin, const QString& sAuxSendBusName )
	: qtractorPluginCommand(QObject::tr("aux-send bus"), pPlugin),
		m_sAuxSendBusName(sAuxSendBusName)
{
}


// Plugin insertion command methods.
bool qtractorAuxSendPluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	if ((pPlugin->type())->index() > 0) {
		qtractorAudioAuxSendPlugin *pAudioAuxSendPlugin
			= static_cast<qtractorAudioAuxSendPlugin *> (pPlugin);
		if (pAudioAuxSendPlugin == nullptr)
			return false;
		const QString sAudioBusName = pAudioAuxSendPlugin->audioBusName();
		pAudioAuxSendPlugin->setAudioBusName(m_sAuxSendBusName, true);
		m_sAuxSendBusName = sAudioBusName;
		pAudioAuxSendPlugin->updateFormAuxSendBusName();
	} else {
		qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
			= static_cast<qtractorMidiAuxSendPlugin *> (pPlugin);
		if (pMidiAuxSendPlugin == nullptr)
			return false;
		const QString sMidiBusName = pMidiAuxSendPlugin->midiBusName();
		pMidiAuxSendPlugin->setMidiBusName(m_sAuxSendBusName);
		m_sAuxSendBusName = sMidiBusName;
		pMidiAuxSendPlugin->updateFormAuxSendBusName();
	}

	return true;
}

bool qtractorAuxSendPluginCommand::undo (void)
{
	return redo();
}


//----------------------------------------------------------------------
// class qtractorRemovePluginCommand - implementation
//

// Constructor.
qtractorRemovePluginCommand::qtractorRemovePluginCommand (
	qtractorPlugin *pPlugin )
	: qtractorPluginCommand(QObject::tr("remove plugin"), pPlugin)
{
}


// Plugin-removal command methods.
bool qtractorRemovePluginCommand::redo (void)
{
	return removePlugins();
}

bool qtractorRemovePluginCommand::undo (void)
{
	return addPlugins();
}


//----------------------------------------------------------------------
// class qtractorInsertPluginCommand - implementation
//

// Constructor.
qtractorInsertPluginCommand::qtractorInsertPluginCommand (
	const QString& sName, qtractorPlugin *pPlugin,
	qtractorPlugin *pNextPlugin ) : qtractorPluginCommand(sName, pPlugin)
{
	m_pNextPlugin = pNextPlugin;
}


// Plugin-insert command methods.
bool qtractorInsertPluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	// Save the previous plugin alright...
	qtractorPluginList *pPluginList = pPlugin->list();
	if (pPluginList == nullptr)
		return false;

//	pSession->lock();

	qtractorPlugin *pNextPlugin = pPlugin->next();

	// Insert it...
	pPluginList->insertPlugin(pPlugin, m_pNextPlugin);

	// Swap it nice, finally.
	m_pNextPlugin = pNextPlugin;

	// Whether to allow the disposal of the plugin reference.
	setAutoDelete(false);

//	pSession->unlock();

	return true;
}

bool qtractorInsertPluginCommand::undo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	// Save the previous track alright...
	qtractorPluginList *pPluginList = pPlugin->list();
	if (pPluginList == nullptr)
		return false;

//	pSession->lock();

	qtractorPlugin *pNextPlugin = pPlugin->next();

	// Insert it...
	pPluginList->removePlugin(pPlugin);

	// Swap it nice, finally.
	m_pNextPlugin = pNextPlugin;

	// Whether to allow the disposal of the plugin reference.
	setAutoDelete(true);

//	pSession->unlock();

	return true;
}


//----------------------------------------------------------------------
// class qtractorMovePluginCommand - implementation
//

// Constructor.
qtractorMovePluginCommand::qtractorMovePluginCommand (
	qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin,
		qtractorPluginList *pPluginList )
	: qtractorInsertPluginCommand(QObject::tr("move plugin"),
		pPlugin, pNextPlugin)
{
	m_pPluginList = pPluginList;

	// Special case for aux-sends moved into output buses
	// and not of same plugin chain-list...
	m_pAuxSendPlugin = nullptr;

	qtractorPluginType *pType = pPlugin->type();
	if (pType && (pType->typeHint() == qtractorPluginType::AuxSend)
		&& (pPluginList != pPlugin->list())) {
		if ((pPluginList->flags() & qtractorPluginList::AudioOutBus) &&
			(pType->index() > 0)) { // index == channels > 0 => Audio aux-send.
			qtractorAudioAuxSendPlugin *pAudioAuxSendPlugin
				= static_cast<qtractorAudioAuxSendPlugin *> (pPlugin);
			if (pAudioAuxSendPlugin) {
				m_pAuxSendPlugin  = pAudioAuxSendPlugin;
				m_sAuxSendBusName = pAudioAuxSendPlugin->audioBusName();
			}
		}
		else // index == 0 => MIDI aux-send.
		if (pPluginList->flags() & qtractorPluginList::MidiOutBus) {
			qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
				= static_cast<qtractorMidiAuxSendPlugin *> (pPlugin);
			if (pMidiAuxSendPlugin) {
				m_pAuxSendPlugin  = pMidiAuxSendPlugin;
				m_sAuxSendBusName = pMidiAuxSendPlugin->midiBusName();
			}
		}
	}
}


// Plugin-move command methods.
bool qtractorMovePluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	qtractorPluginType *pType = pPlugin->type();
	if (pType == nullptr)
		return false;

	if (m_pPluginList == nullptr)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

//	pSession->lock();

	// Save the previous track alright...
	qtractorPlugin *pNextPlugin = pPlugin->next();
	qtractorPluginList *pPluginList = pPlugin->list();

	// Move it...
	m_pPluginList->movePlugin(pPlugin, nextPlugin());

	// Special case for audio Aux-sends moved into output buses...
	if (m_pAuxSendPlugin) {
		if (pType->index() > 0) { // index == channels > 0 => Audio aux-send.
			qtractorAudioAuxSendPlugin *pAudioAuxSendPlugin
				= static_cast<qtractorAudioAuxSendPlugin *> (m_pAuxSendPlugin);
			if (pAudioAuxSendPlugin) {
				const QString sAuxSendBusName
					= pAudioAuxSendPlugin->audioBusName();
				if (sAuxSendBusName.isEmpty())
					pAudioAuxSendPlugin->setAudioBusName(m_sAuxSendBusName);
				else
					pAudioAuxSendPlugin->setAudioBusName(QString());
			}
		} else { // index == 0 => MIDI aux-send.
			qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
				= static_cast<qtractorMidiAuxSendPlugin *> (m_pAuxSendPlugin);
			if (pMidiAuxSendPlugin) {
				const QString sAuxSendBusName
					= pMidiAuxSendPlugin->midiBusName();
				pMidiAuxSendPlugin->setMidiBusName(m_sAuxSendBusName);
				m_sAuxSendBusName = sAuxSendBusName;
			}
		}
	}

	// Swap it nice, finally.
	m_pPluginList = pPluginList;
	setNextPlugin(pNextPlugin);

//	pSession->unlock();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks) {
			pTracks->clearSelect();
			pTracks->updateTrackList();
			pTracks->updateTrackView();
		}
	}

	return true;
}

bool qtractorMovePluginCommand::undo (void)
{
	// As we swap the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorActivatePluginCommand - implementation
//

// Constructor.
qtractorActivatePluginCommand::qtractorActivatePluginCommand (
	qtractorPlugin *pPlugin, bool bActivated )
	: qtractorPluginCommand(QObject::tr("activate plugin"), pPlugin)
{
	m_bActivated = bActivated;
}


// Plugin-activate command methods.
bool qtractorActivatePluginCommand::redo (void)
{
	// Save the toggled state alright...
	const bool bActivated = !m_bActivated;

	QListIterator<qtractorPlugin *> iter(plugins());
	while (iter.hasNext())
		iter.next()->setActivatedEx(m_bActivated);

	// Swap it nice, finally.
	m_bActivated = bActivated;

	return true;
}

bool qtractorActivatePluginCommand::undo (void)
{
	// As we toggle the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorPresetPluginCommand - implementation
//

// Constructor.
qtractorPresetPluginCommand::qtractorPresetPluginCommand (
	qtractorPlugin *pPlugin, const QString& sPreset, const QStringList& vlist )
	: qtractorPluginCommand(QObject::tr("preset plugin"), pPlugin)
{
	m_sPreset = sPreset;
	m_vlist = vlist;
}


// Plugin-preset command methods.
bool qtractorPresetPluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	// Save the current toggled state alright...
	const QString sPreset = pPlugin->preset();
	const QStringList vlist = pPlugin->valueList();

	pPlugin->setPreset(m_sPreset);
	pPlugin->setValueList(m_vlist);
	pPlugin->realizeValues();
	pPlugin->releaseValues();

	// Swap it nice, finally.
	m_sPreset = sPreset;
	m_vlist = vlist;

	// Update the form, showing it up as necessary...
	pPlugin->refreshForm();

	return true;
}

bool qtractorPresetPluginCommand::undo (void)
{
	// As we swap the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorResetPluginCommand - implementation
//

// Constructor.
qtractorResetPluginCommand::qtractorResetPluginCommand (
	qtractorPlugin *pPlugin )
	: qtractorPluginCommand(QObject::tr("reset plugin"), pPlugin)
{
}


// Plugin-reset command methods.
bool qtractorResetPluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	// Toggle/swap it nice...
	const QString sPreset = pPlugin->preset();
	const QStringList vlist = pPlugin->valueList();

	pPlugin->setPreset(m_sPreset);

	if (m_sPreset.isEmpty() || m_vlist.isEmpty()) {
		pPlugin->reset();
	} else {
		pPlugin->setValueList(m_vlist);
		pPlugin->releaseValues();
	}

	// Swap it nice.
	m_sPreset = sPreset;
	m_vlist = vlist;

	// Update the form, showing it up as necessary...
	pPlugin->refreshForm();

	return true;
}


bool qtractorResetPluginCommand::undo (void)
{
	// As we swap the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorProgramPluginCommand - implementation
//

// Constructor.
qtractorPluginProgramCommand::qtractorPluginProgramCommand (
	qtractorPlugin *pPlugin, int iBank, int iProg )
	: qtractorPluginCommand(QObject::tr("plugin program"), pPlugin)
{
	m_iBank = iBank;
	m_iProg = iProg;
}


// Plugin-preset command methods.
bool qtractorPluginProgramCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	// Save the current toggled state alright...
	qtractorPluginList::MidiProgramSubject *pMidiProgramSubject
		= (pPlugin->list())->midiProgramSubject();
	if (pMidiProgramSubject == nullptr)
		return false;


	const int iBank = pMidiProgramSubject->bank();
	const int iProg = pMidiProgramSubject->prog();

	pMidiProgramSubject->setProgram(m_iBank, m_iProg);

	m_iBank = iBank;
	m_iProg = iProg;

	return true;
}

bool qtractorPluginProgramCommand::undo (void)
{
	// As we swap the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorPluginAliasCommand - implementation
//

// Constructor.
qtractorPluginAliasCommand::qtractorPluginAliasCommand (
	qtractorPlugin *pPlugin, const QString& sAlias )
	: qtractorPluginCommand(QObject::tr("plugin alias"), pPlugin)
{
	m_sAlias = sAlias;
}


// Plugin-alias command methods.
bool qtractorPluginAliasCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	// Save the previous alias alright...
	const QString sAlias = pPlugin->alias();

	pPlugin->setAlias(m_sAlias);

	pPlugin->updateEditorTitle();
	pPlugin->updateListViews(true);
	pPlugin->refreshForm();

	// Swap it nice, finally.
	m_sAlias = sAlias;

	return true;
}

bool qtractorPluginAliasCommand::undo (void)
{
	// As we toggle the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorPluginParamCommand - implementation
//

// Constructor.
qtractorPluginParamCommand::qtractorPluginParamCommand (
	qtractorPlugin::Param *pParam, float fValue, bool bUpdate )
	: qtractorCommand(pParam->name()),
		m_pParam(pParam), m_fValue(fValue), m_bUpdate(bUpdate),
		m_pLastUpdatedParam(pParam)
{
	setRefresh(false);
}


// Plugin-reset command methods.
bool qtractorPluginParamCommand::redo (void)
{
	qtractorPlugin *pPlugin = m_pParam->plugin();
	if (pPlugin == nullptr)
		return false;

	// Set plugin parameter value...
	const float fValue = m_pParam->value();
	m_pParam->setValue(m_fValue, m_bUpdate);

	// Set undo value...
	m_fValue  = fValue;
	m_bUpdate = true;

	qtractorPlugin::Param *pLastUpdatedParam = pPlugin->lastUpdatedParam();
	pPlugin->setLastUpdatedParam(m_pLastUpdatedParam);
	m_pLastUpdatedParam = pLastUpdatedParam;

	// Update the form, showing it up as necessary...
	pPlugin->updateFormDirtyCount();

	// Update any GUI editor...
	// pPlugin->idleEditor();

	return true;
}


bool qtractorPluginParamCommand::undo (void)
{
	// As we swap the prev/value this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorPluginParamValuesCommand - impl.
//

// Constructor.
qtractorPluginParamValuesCommand::qtractorPluginParamValuesCommand (
	const QString& sName ) : qtractorCommand(sName)
{
	setRefresh(false);
}


// Destructor.
qtractorPluginParamValuesCommand::~qtractorPluginParamValuesCommand (void)
{
	qDeleteAll(m_paramCommands);
	m_paramCommands.clear();
}


// Param-values list builder.
void qtractorPluginParamValuesCommand::updateParamValue (
	qtractorPlugin::Param *pParam, float fValue, bool bUpdate )
{
	qtractorPlugin *pPlugin = pParam->plugin();
	if (pPlugin == nullptr)
		return;

	if (pPlugin->isLastUpdatedParam(pParam)) {
		pParam->setValue(fValue, bUpdate);
		pPlugin->updateFormDirtyCount();
	} else {
		m_paramCommands.append(
			new qtractorPluginParamCommand(pParam, fValue, bUpdate));
	//	pPlugin->setLastUpdatedParam(pParam);
	}
}


// Composite predicate.
bool qtractorPluginParamValuesCommand::isEmpty (void) const
{
	return m_paramCommands.isEmpty();
}


// Plugin-values command methods.
bool qtractorPluginParamValuesCommand::redo (void)
{
	bool bRedo = true;

	QListIterator<qtractorPluginParamCommand *> iter(m_paramCommands);
	while (bRedo && iter.hasNext())
		bRedo = iter.next()->redo();

	return bRedo;
}


bool qtractorPluginParamValuesCommand::undo (void)
{
	bool bUndo = true;

	QListIterator<qtractorPluginParamCommand *> iter(m_paramCommands);
	iter.toBack();
	while (bUndo && iter.hasPrevious())
		bUndo = iter.previous()->undo();

	return bUndo;
}


//----------------------------------------------------------------------
// class qtractorPluginPropertyCommand - implementation
//

// Constructor.
qtractorPluginPropertyCommand::qtractorPluginPropertyCommand (
	qtractorPlugin::Property *pProp, const QVariant& value )
	: qtractorCommand(pProp->name()),
		m_pProp(pProp), m_value(value),
		m_pLastUpdatedProperty(pProp)
{
	setRefresh(false);
}


// Plugin-property command methods.
bool qtractorPluginPropertyCommand::redo (void)
{
	qtractorPlugin *pPlugin = m_pProp->plugin();
	if (pPlugin == nullptr)
		return false;

	// Save the current toggled state alright...
	const QVariant value = m_pProp->variant();

	m_pProp->setVariant(m_value, true);

	// Set undo value.
	m_value = value;

	qtractorPlugin::Property *pLastUpdatedProperty = pPlugin->lastUpdatedProperty();
	pPlugin->setLastUpdatedProperty(m_pLastUpdatedProperty);
	m_pLastUpdatedProperty = pLastUpdatedProperty;

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_PATCH
	if (!m_pProp->isAutomatable()) {
		qtractorPluginType *pType = pPlugin->type();
		if (pType->typeHint() == qtractorPluginType::Lv2) {
			qtractorLv2Plugin *pLv2Plugin
				= static_cast<qtractorLv2Plugin *> (pPlugin);
			if (pLv2Plugin)
				pLv2Plugin->lv2_property_update(m_pProp->index());
		}
	}
#endif
#endif

	// Update the form, showing it up as necessary...
	pPlugin->updateFormDirtyCount();

	// Update any GUI editor...
	// pPlugin->idleEditor();

	// FIXME: Might no work the first time...
	pPlugin->refreshForm();

	return true;
}

bool qtractorPluginPropertyCommand::undo (void)
{
	// As we swap the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorAudioOutputBusCommand - declaration.
//

// Constructor.
qtractorAudioOutputBusCommand::qtractorAudioOutputBusCommand (
	qtractorMidiManager *pMidiManager, bool bAudioOutputBus,
	bool bAudioOutputAutoConnect, const QString& sAudioOutputBusName )
	: qtractorCommand(QObject::tr("dedicated audio outputs")),
		m_pMidiManager(pMidiManager), m_bAudioOutputBus(bAudioOutputBus),
		m_bAudioOutputAutoConnect(bAudioOutputAutoConnect),
		m_sAudioOutputBusName(sAudioOutputBusName)
{
}


// Plugin audio ouput bus command methods.
bool qtractorAudioOutputBusCommand::redo (void)
{
	if (m_pMidiManager == nullptr)
		return false;

	const bool bAudioOutputBus
		= m_pMidiManager->isAudioOutputBus();
	const bool bAudioOutputAutoConnect
		= m_pMidiManager->isAudioOutputAutoConnect();
	const QString sAudioOutputBusName
		= m_pMidiManager->audioOutputBusName();

	m_pMidiManager->setAudioOutputBusName(m_sAudioOutputBusName);
	m_pMidiManager->setAudioOutputAutoConnect(m_bAudioOutputAutoConnect);
	m_pMidiManager->setAudioOutputBus(m_bAudioOutputBus);

	m_sAudioOutputBusName = sAudioOutputBusName;
	m_bAudioOutputAutoConnect = bAudioOutputAutoConnect;
	m_bAudioOutputBus = bAudioOutputBus;

	return true;
}

bool qtractorAudioOutputBusCommand::undo (void)
{
	return redo();
}


//----------------------------------------------------------------------
// class qtractorDirectAccessParamCommand - implementation
//

// Constructor.
qtractorDirectAccessParamCommand::qtractorDirectAccessParamCommand (
	qtractorPlugin *pPlugin, long iDirectAccessParamIndex )
	: qtractorPluginCommand(QObject::tr("direct access param"), pPlugin)
{
	m_iDirectAccessParamIndex = iDirectAccessParamIndex;
}


// Plugin-change command methods.
bool qtractorDirectAccessParamCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == nullptr)
		return false;

	const long iDirectAccessParamIndex = pPlugin->directAccessParamIndex();
	pPlugin->setDirectAccessParamIndex(m_iDirectAccessParamIndex);
	m_iDirectAccessParamIndex = iDirectAccessParamIndex;

	return true;
}

bool qtractorDirectAccessParamCommand::undo (void)
{
	return redo();
}


//----------------------------------------------------------------------
// class qtractorImportPluginsCommand - declaration.
//

// Constructor.
qtractorImportPluginsCommand::qtractorImportPluginsCommand (void)
	: qtractorCommand(QObject::tr("import plugins"))
{
	m_pAddCommand    = new qtractorAddPluginCommand();
	m_pRemoveCommand = new qtractorRemovePluginCommand();
}


// Destructor.
qtractorImportPluginsCommand::~qtractorImportPluginsCommand (void)
{
	delete m_pRemoveCommand;
	delete m_pAddCommand;
}


// Import plugins command methods.
bool qtractorImportPluginsCommand::redo (void)
{
	return (m_pRemoveCommand->redo() && m_pAddCommand->redo());
}


bool qtractorImportPluginsCommand::undo (void)
{
	return (m_pAddCommand->undo() && m_pRemoveCommand->undo());
}


// end of qtractorPluginCommand.cpp
