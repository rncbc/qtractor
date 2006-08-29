// qtractorPluginCommand.h
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

#ifndef __qtractorPluginCommand_h
#define __qtractorPluginCommand_h

#include "qtractorCommand.h"

#include "qtractorPlugin.h"

// Forward declarations...
class qtractorPluginPortWidget;


//----------------------------------------------------------------------
// class qtractorPluginCommand - declaration.
//

class qtractorPluginCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorPluginCommand(qtractorMainForm *pMainForm,
		const QString& sName, qtractorPlugin *pPlugin = NULL);
	// Destructor.
	virtual ~qtractorPluginCommand();

	// Plugin list accessor.
	QPtrList<qtractorPlugin>& plugins() { return m_plugins; }

protected:

	// Add new plugin(s) command method.
	bool addPlugins();

	// Remove existing plugin(s) command method.
	bool removePlugins();

private:

	// Instance variables.
	QPtrList<qtractorPlugin> m_plugins;
};


//----------------------------------------------------------------------
// class qtractorAddPluginCommand - declaration.
//

class qtractorAddPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorAddPluginCommand(qtractorMainForm *pMainForm,
		qtractorPlugin *pPlugin = NULL);

	// Plugin insertion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorRemovePluginCommand - declaration.
//

class qtractorRemovePluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorRemovePluginCommand(qtractorMainForm *pMainForm,
		qtractorPlugin *pPlugin = NULL);

	// Plugin-removal command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorMovePluginCommand - declaration.
//

class qtractorMovePluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorMovePluginCommand(qtractorMainForm *pMainForm,
		qtractorPlugin *pPlugin, qtractorPlugin *pPrevPlugin);

	// Plugin-move command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorPlugin *m_pPrevPlugin;
};


//----------------------------------------------------------------------
// class qtractorActivatePluginCommand - declaration.
//

class qtractorActivatePluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorActivatePluginCommand(qtractorMainForm *pMainForm,
		qtractorPlugin *pPlugin, bool bActivated);

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
	qtractorResetPluginCommand(qtractorMainForm *pMainForm,
		qtractorPlugin *pPlugin);

	// Plugin-reset command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	bool m_bReset;
	QStringList m_vlist;
};


//----------------------------------------------------------------------
// class qtractorPresetPluginCommand - declaration.
//

class qtractorPresetPluginCommand : public qtractorPluginCommand
{
public:

	// Constructor.
	qtractorPresetPluginCommand(qtractorMainForm *pMainForm,
		qtractorPlugin *pPlugin, const QStringList& vlist);

	// Plugin-preset command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QStringList m_vlist;
};


//----------------------------------------------------------------------
// class qtractorPluginPortCommand - declaration.
//

class qtractorPluginPortCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorPluginPortCommand(qtractorMainForm *pMainForm,
		qtractorPluginPort *pPort, float fValue);

	// Plugin-port command methods.
	bool redo();
	bool undo();

	// Plugin-port accessor.
	qtractorPluginPort *port() const { return m_pPort; }

	// Plugin-port value retrieval.
	float value() const { return m_fValue; }

	// Last known panning predicate.
	float prevValue() const { return m_fPrevValue; }

private:

	// Instance variables.
	qtractorPluginPort *m_pPort;
	float m_fValue;
	float m_fPrevValue;
	bool  m_bPrevValue;
};


#endif	// __qtractorPluginCommand_h

// end of qtractorPluginCommand.h

