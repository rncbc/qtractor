// qtractorPluginCommand.h
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

#ifndef __qtractorPluginCommand_h
#define __qtractorPluginCommand_h

#include "qtractorCommand.h"

#include "qtractorPlugin.h"

#include <QVariant>
#include <QList>

// Forward declarations...
class qtractorAuxSendPlugin;
class qtractorPluginPortWidget;
class qtractorMidiManager;


//----------------------------------------------------------------------
// class qtractorPluginCommand - declaration.
//

class qtractorPluginCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorPluginCommand(const QString& sName,
		qtractorPlugin *pPlugin = nullptr);

	// Destructor.
	virtual ~qtractorPluginCommand();

	// Plugin list accessors.
	const QList<qtractorPlugin *>& plugins() const { return m_plugins; }

	void addPlugin(qtractorPlugin *pPlugin)
		{ m_plugins.append(pPlugin); }

protected:

	// Add new plugin(s) command method.
	bool addPlugins();

	// Remove existing plugin(s) command method.
	bool removePlugins();

private:

	// Instance variables.
	QList<qtractorPlugin *> m_plugins;
};


//----------------------------------------------------------------------
// class qtractorAddPluginCommand - declaration.
//

class qtractorAddPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorAddPluginCommand(qtractorPlugin *pPlugin = nullptr);

	// Plugin insertion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorAddInsertPluginCommand - declaration.
//

class qtractorAddInsertPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorAddInsertPluginCommand(qtractorPlugin *pPlugin = nullptr);

	// Plugin insertion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorAddAuxSendPluginCommand - declaration.
//

class qtractorAddAuxSendPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorAddAuxSendPluginCommand(qtractorPlugin *pPlugin = nullptr);

	// Plugin insertion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorAuxSendPluginCommand - declaration.
//

class qtractorAuxSendPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorAuxSendPluginCommand(
		qtractorPlugin *pPlugin, const QString& sAuxSendBusName);

	// Plugin insertion command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QString m_sAuxSendBusName;
};


//----------------------------------------------------------------------
// class qtractorRemovePluginCommand - declaration.
//

class qtractorRemovePluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorRemovePluginCommand(qtractorPlugin *pPlugin = nullptr);

	// Plugin-removal command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorInsertPluginCommand - declaration.
//

class qtractorInsertPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorInsertPluginCommand(const QString& sName,
		qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin);

	// Plugin-move command methods.
	bool redo();
	bool undo();

protected:

	// The anchor plugin reference.
	void setNextPlugin(qtractorPlugin *pNextPlugin)
		{ m_pNextPlugin = pNextPlugin; }
	qtractorPlugin *nextPlugin() const
		{ return m_pNextPlugin; }

private:

	// Instance variables.
	qtractorPlugin *m_pNextPlugin;
};


//----------------------------------------------------------------------
// class qtractorMovePluginCommand - declaration.
//

class qtractorMovePluginCommand : public qtractorInsertPluginCommand
{
public:

	// Constructor.
	qtractorMovePluginCommand(qtractorPlugin *pPlugin,
		qtractorPlugin *pNextPlugin, qtractorPluginList *pPluginList);

	// Plugin-move command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorPluginList *m_pPluginList;

	// Special case for aux-sends moved into output buses...
	qtractorAuxSendPlugin *m_pAuxSendPlugin;
	QString                m_sAuxSendBusName;
};


//----------------------------------------------------------------------
// class qtractorActivatePluginCommand - declaration.
//

class qtractorActivatePluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorActivatePluginCommand(qtractorPlugin *pPlugin, bool bActivated);

	// Plugin-activate command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	bool m_bActivated;
};


//----------------------------------------------------------------------
// class qtractorPresetPluginCommand - declaration.
//

class qtractorPresetPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorPresetPluginCommand(qtractorPlugin *pPlugin,
		const QString& sPreset, const QStringList& vlist);

	// Plugin-preset command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QString     m_sPreset;
	QStringList m_vlist;
};


//----------------------------------------------------------------------
// class qtractorResetPluginCommand - declaration.
//

class qtractorResetPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorResetPluginCommand(qtractorPlugin *pPlugin);

	// Plugin-reset command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QString m_sPreset;
	QStringList m_vlist;
};


//----------------------------------------------------------------------
// class qtractorPluginProgramCommand - declaration.
//

class qtractorPluginProgramCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorPluginProgramCommand(qtractorPlugin *pPlugin,
		int iBank, int iProg);

	// Plugin-change command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	int m_iBank;
	int m_iProg;
};


//----------------------------------------------------------------------
// class qtractorAliasPluginCommand - declaration.
//

class qtractorPluginAliasCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorPluginAliasCommand(qtractorPlugin *pPlugin, const QString& sAlias);

	// Plugin-alias command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QString m_sAlias;
};


//----------------------------------------------------------------------
// class qtractorPluginParamCommand - declaration.
//

class qtractorPluginParamCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorPluginParamCommand(
		qtractorPlugin::Param *pParam, float fValue, bool bUpdate);

	// Plugin-port command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorPlugin::Param *m_pParam;

	float m_fValue;
	bool  m_bUpdate;

	qtractorPlugin::Param *m_pLastUpdatedParam;
};


//----------------------------------------------------------------------
// class qtractorPluginParamValuesCommand - declaration.
//

class qtractorPluginParamValuesCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorPluginParamValuesCommand(const QString& sName);

	// Destructor.
	~qtractorPluginParamValuesCommand();

	// Plugin-value list accessor.
	void updateParamValue(qtractorPlugin::Param *pParam, float fValue, bool bUpdate);

	// Composite predicate.
	bool isEmpty() const;

	// Plugin-values command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QList<qtractorPluginParamCommand *> m_paramCommands;
};


//----------------------------------------------------------------------
// class qtractorPluginPropertyCommand - declaration.
//

class qtractorPluginPropertyCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorPluginPropertyCommand(
		qtractorPlugin::Property *pProp, const QVariant& value);

	// Plugin-port command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorPlugin::Property *m_pProp;

	QVariant m_value;

	qtractorPlugin::Property *m_pLastUpdatedProperty;
};


//----------------------------------------------------------------------
// class qtractorAudioOutputBusCommand - declaration.
//

class qtractorAudioOutputBusCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorAudioOutputBusCommand(qtractorMidiManager *pMidiManager,
		bool bAudioOutputBus, bool bAudioOutputAutoConnect,
		const QString& sAudioOutputBusName);

	// Plugin audio ouput bus command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorMidiManager *m_pMidiManager;

	bool m_bAudioOutputBus;
	bool m_bAudioOutputAutoConnect;
	QString m_sAudioOutputBusName;
};


//----------------------------------------------------------------------
// class qtractorDirectAccessParamCommand - declaration.
//

class qtractorDirectAccessParamCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorDirectAccessParamCommand(qtractorPlugin *pPlugin,
		long iDirectAccessParamIndex);

	// Plugin-change command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	long m_iDirectAccessParamIndex;
};


//----------------------------------------------------------------------
// class qtractorImportPluginsCommand - declaration.
//

class qtractorImportPluginsCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorImportPluginsCommand();

	// Destructor.
	~qtractorImportPluginsCommand();

	// Plugin lists accessors.
	void addPlugin(qtractorPlugin *pPlugin)
		{ m_pAddCommand->addPlugin(pPlugin); }
	void removePlugin(qtractorPlugin *pPlugin)
		{ m_pRemoveCommand->addPlugin(pPlugin); }

	// Import plugins command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorAddPluginCommand    *m_pAddCommand;
	qtractorRemovePluginCommand *m_pRemoveCommand;
};


#endif	// __qtractorPluginCommand_h

// end of qtractorPluginCommand.h

