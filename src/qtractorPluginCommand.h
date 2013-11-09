// qtractorPluginCommand.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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
		qtractorPlugin *pPlugin = NULL);

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
	qtractorAddPluginCommand(qtractorPlugin *pPlugin = NULL);

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
	qtractorAddInsertPluginCommand(qtractorPlugin *pPlugin = NULL);

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
	qtractorAddAuxSendPluginCommand(qtractorPlugin *pPlugin = NULL);

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
	qtractorAuxSendPluginCommand(qtractorAuxSendPlugin *pAuxSendPlugin,
		const QString& sAudioBusName);

	// Plugin insertion command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QString m_sAudioBusName;
};


//----------------------------------------------------------------------
// class qtractorRemovePluginCommand - declaration.
//

class qtractorRemovePluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorRemovePluginCommand(qtractorPlugin *pPlugin = NULL);

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
// class qtractorPluginParamCommand - declaration.
//

class qtractorPluginParamCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorPluginParamCommand(
		qtractorPluginParam *pParam, float fValue, bool bUpdate);

	// Plugin-port command methods.
	bool redo();
	bool undo();

	// Plugin-port accessor.
	qtractorPluginParam *param() const { return m_pParam; }

	// Plugin-port value retrieval.
	float value() const { return m_fValue; }

	// Last known panning predicate.
	float prevValue() const { return m_fPrevValue; }

private:

	// Instance variables.
	qtractorPluginParam *m_pParam;
	float m_fValue;
	bool  m_bUpdate;
	float m_fPrevValue;
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


#endif	// __qtractorPluginCommand_h

// end of qtractorPluginCommand.h

