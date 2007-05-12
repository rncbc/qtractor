// qtractorPluginCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMainForm.h"
#include "qtractorSession.h"


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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	pSession->lock();

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

	pSession->unlock();

	return true;
}


// Remove existing plugin(s) command methods.
bool qtractorPluginCommand::removePlugins (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	pSession->lock();

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

	pSession->unlock();

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
// class qtractorMovePluginCommand - implementation
//

// Constructor.
qtractorMovePluginCommand::qtractorMovePluginCommand (
	qtractorPlugin *pPlugin, qtractorPlugin *pPrevPlugin )
	: qtractorPluginCommand(QObject::tr("move plugin"), pPlugin)
{
	m_pPrevPlugin = pPrevPlugin;
}


// Plugin-move command methods.
bool qtractorMovePluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == NULL)
		return false;

	qtractorPluginList *pPluginList = pPlugin->list();
	if (pPluginList == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	pSession->lock();

	// Save the previous track alright...
	qtractorPlugin *pPrevPlugin = pPlugin->prev();
	// Move it...
	pPluginList->movePlugin(pPlugin, m_pPrevPlugin);
	// Swap it nice, finally.
	m_pPrevPlugin = pPrevPlugin;

	pSession->unlock();

	return true;
}

bool qtractorMovePluginCommand::undo (void)
{
	// As we swap the prev/plugin this is non-idempotent.
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
	bool bActivated = !m_bActivated;

	QListIterator<qtractorPlugin *> iter(plugins());
	while (iter.hasNext())
		iter.next()->setActivated(m_bActivated);

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
	qtractorPlugin *pPlugin, const QStringList& vlist )
	: qtractorPluginCommand(QObject::tr("preset plugin"), pPlugin)
{
	m_vlist = vlist;
}


// Plugin-preset command methods.
bool qtractorPresetPluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == NULL)
		return false;

	// Save the currtoggled state alright...
	QStringList vlist = pPlugin->values();
	pPlugin->setValues(m_vlist);
	// Swap it nice, finally.
	m_vlist = vlist;

	// Update the form, showing it up as necessary...
	pPlugin->form()->refresh();

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
	m_bReset = false;
}


// Plugin-reset command methods.
bool qtractorResetPluginCommand::redo (void)
{
	qtractorPlugin *pPlugin = plugins().first();
	if (pPlugin == NULL)
		return false;

	// Toggle/swap it nice...
	m_bReset = !m_bReset;
	if (m_bReset) {
		m_vlist = pPlugin->values();
		pPlugin->reset();
	} else {
		pPlugin->setValues(m_vlist);
	}

	// Update the form, showing it up as necessary...
	pPlugin->form()->refresh();

	return true;
}


bool qtractorResetPluginCommand::undo (void)
{
	// As we swap the prev/state this is non-idempotent.
	return redo();
}


//----------------------------------------------------------------------
// class qtractorResetPluginCommand - implementation
//

// Constructor.
qtractorPluginPortCommand::qtractorPluginPortCommand (
	qtractorPluginPort *pPort, float fValue )
	: qtractorCommand(QString(pPort->name()).toLower()), m_pPort(pPort)
{
	m_fValue = fValue;
	m_fPrevValue = 0.0f;
	m_bPrevValue = false;

	setRefresh(false);

	// Try replacing an previously equivalent command...
	static qtractorPluginPortCommand *s_pPrevPortCommand = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (s_pPrevPortCommand && pMainForm) {
		qtractorCommand *pLastCommand
			= pMainForm->commands()->lastCommand();
		qtractorCommand *pPrevCommand
			= static_cast<qtractorCommand *> (s_pPrevPortCommand);
		if (pPrevCommand == pLastCommand
			&& s_pPrevPortCommand->port() == pPort) {
			qtractorPluginPortCommand *pLastPortCommand
				= static_cast<qtractorPluginPortCommand *> (pLastCommand);
			if (pLastPortCommand) {
				// Equivalence means same (sign) direction too...
				float fPrevValue = pLastPortCommand->prevValue();
				float fLastValue = pLastPortCommand->value();
				int   iPrevSign  = (fPrevValue > fLastValue ? +1 : -1);
				int   iCurrSign  = (fPrevValue < m_fValue   ? +1 : -1); 
				if (iPrevSign == iCurrSign) {
					m_fPrevValue = fLastValue;
					m_bPrevValue = true;
					pMainForm->commands()->removeLastCommand();
				}
			}
		}
	}
	s_pPrevPortCommand = this;
}


// Plugin-reset command methods.
bool qtractorPluginPortCommand::redo (void)
{
	qtractorPlugin *pPlugin = m_pPort->plugin();
	if (pPlugin == NULL)
		return false;

	// Set plugin port value...
	float fValue = (m_bPrevValue ? m_fPrevValue : m_pPort->value());
	m_pPort->setValue(m_fValue);

	// Set undo value...
	m_bPrevValue = false;
	m_fPrevValue = m_fValue;
	m_fValue     = fValue;

	// Update the form, showing it up as necessary...
	pPlugin->form()->updatePort(m_pPort);

	return true;
}


bool qtractorPluginPortCommand::undo (void)
{
	// As we swap the prev/value this is non-idempotent.
	return redo();
}


// end of qtractorPluginCommand.cpp
