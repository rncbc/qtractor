// qtractorPluginCommand.cpp
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
#include "qtractorPluginCommand.h"
#include "qtractorPluginForm.h"

#include "qtractorMainForm.h"

#include "qtractorPluginListView.h"


//----------------------------------------------------------------------
// class qtractorPluginCommand - implementation
//

// Constructor.
qtractorPluginCommand::qtractorPluginCommand ( qtractorMainForm *pMainForm,
	const QString& sName, qtractorPlugin *pPlugin )
	: qtractorCommand(pMainForm, sName)
{
	m_plugins.setAutoDelete(false);

	if (pPlugin)
		m_plugins.append(pPlugin);	

	setRefresh(false);
}


// Destructor.
qtractorPluginCommand::~qtractorPluginCommand (void)
{
	m_plugins.setAutoDelete(isAutoDelete());
	m_plugins.clear();
}


// Add new plugin(s) command methods.
bool qtractorPluginCommand::addPlugin ( qtractorPlugin *pPlugin )
{
	if (pPlugin == NULL)
		return false;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorPluginCommand::addPlugin(%p)\n", pPlugin);
#endif

	qtractorPluginList *pPluginList = pPlugin->list();
	if (pPluginList == NULL)
		return false;

	// Guess which item we're adding after...
	qtractorPlugin *pPrevPlugin = pPlugin->prev();
	if (pPrevPlugin == NULL && pPlugin->next() == NULL)
		pPrevPlugin = pPluginList->last();
	// Link the track into session...
	pPluginList->insertAfter(pPlugin, pPrevPlugin);
	pPlugin->setChannels(pPluginList->channels());

	// Now update each observer list-view...
	QPtrList<qtractorPluginListView>& views = pPluginList->views();
	for (qtractorPluginListView *pListView = views.first();
			pListView; pListView = views.next()) {
		// Get the previous one, if any...
		qtractorPluginListItem *pPrevItem = pListView->pluginItem(pPrevPlugin);
		// Add the list-view item...
		qtractorPluginListItem *pItem
			= new qtractorPluginListItem(pListView, pPlugin, pPrevItem);
		pListView->setSelected(pItem, true);
	}

	// Show the plugin form right away...
	qtractorPluginForm *pPluginForm = pPlugin->form();
	pPluginForm->show();
	pPluginForm->raise();
	pPluginForm->setActiveWindow();

	return true;
}


bool qtractorPluginCommand::addPlugins (void)
{
	// Add all listed plugins, in order...
	for (qtractorPlugin *pPlugin = m_plugins.first();
			pPlugin; pPlugin = m_plugins.next()) {
		if (!addPlugin(pPlugin))
			return false;
	}

	// Avoid the disposal of the plugin reference(s).
	setAutoDelete(false);

	return true;
}


// Remove existing plugin(s) command methods.
bool qtractorPluginCommand::removePlugin ( qtractorPlugin *pPlugin )
{
	if (pPlugin == NULL)
		return false;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorPluginCommand::removePlugin(%p)\n", pPlugin);
#endif

	qtractorPluginList *pPluginList = pPlugin->list();
	if (pPluginList == NULL)
		return false;

	// Just add the plugin to the list...
	pPlugin->setChannels(0);
	pPluginList->unlink(pPlugin);

	// Now update each observer list-view...
	QPtrList<qtractorPluginListItem>& items = pPlugin->items();
	for (qtractorPluginListItem *pItem = items.first();
			pItem; pItem = items.next()) {
		delete pItem;
	}

	return true;
}


bool qtractorPluginCommand::removePlugins (void)
{
	// Add all listed plugins, in order...
	for (qtractorPlugin *pPlugin = m_plugins.last();
			pPlugin; pPlugin = m_plugins.prev()) {
		if (!removePlugin(pPlugin))
			return false;
	}

	// Allow the disposal of the plugin reference(s).
	setAutoDelete(true);

	return true;
}



//----------------------------------------------------------------------
// class qtractorAddPluginCommand - implementation
//

// Constructor.
qtractorAddPluginCommand::qtractorAddPluginCommand (
	qtractorMainForm *pMainForm, qtractorPlugin *pPlugin )
	: qtractorPluginCommand(pMainForm, QObject::tr("add plugin"), pPlugin)
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
	qtractorMainForm *pMainForm, qtractorPlugin *pPlugin )
	: qtractorPluginCommand(pMainForm, QObject::tr("remove plugin"), pPlugin)
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
	qtractorMainForm *pMainForm, qtractorPlugin *pPlugin,
		qtractorPlugin *pPrevPlugin )
	: qtractorPluginCommand(pMainForm, QObject::tr("move plugin"), pPlugin)
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

	// Save the previous track alright...
	qtractorPlugin *pPrevPlugin = pPlugin->prev();

	// Remove and insert back again...
	pPluginList->unlink(pPlugin);
	if (m_pPrevPlugin)
		pPluginList->insertAfter(pPlugin, m_pPrevPlugin);
	else
		pPluginList->prepend(pPlugin);

	// Now update each observer list-view...
	QPtrList<qtractorPluginListItem>& items = pPlugin->items();
	for (qtractorPluginListItem *pItem = items.first();
			pItem; pItem = items.next()) {
		qtractorPluginListView *pListView
			= static_cast<qtractorPluginListView *> (pItem->listView());
		if (pListView) {
			qtractorPluginListItem *pPrevItem
				= pListView->pluginItem(m_pPrevPlugin);
			// Remove the old item...
			delete pItem;
			// Just insert under the track list position...
			pItem = new qtractorPluginListItem(pListView, pPlugin, pPrevItem);
			pListView->setSelected(pItem, true);
		}
	}

	// Swap it nice, finally.
	m_pPrevPlugin = pPrevPlugin;

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
	qtractorMainForm *pMainForm, qtractorPlugin *pPlugin, bool bActivated )
	: qtractorPluginCommand(pMainForm, QObject::tr("activate plugin"), pPlugin)
{
	m_bActivated = bActivated;
}


// Plugin-activate command methods.
bool qtractorActivatePluginCommand::redo (void)
{
	// Save the toggled state alright...
	bool bActivated = !m_bActivated;

	for (qtractorPlugin *pPlugin = plugins().first();
			pPlugin; pPlugin = plugins().next()) {
		pPlugin->setActivated(m_bActivated);
	}

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
	qtractorMainForm *pMainForm, qtractorPlugin *pPlugin,
	const QStringList& vlist )
	: qtractorPluginCommand(pMainForm, QObject::tr("preset plugin"), pPlugin)
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
	qtractorMainForm *pMainForm, qtractorPlugin *pPlugin )
	: qtractorPluginCommand(pMainForm, QObject::tr("reset plugin"), pPlugin)
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
	qtractorMainForm *pMainForm, qtractorPluginPort *pPort, float fValue )
	: qtractorCommand(pMainForm, QString(pPort->name()).lower()),
		m_pPort(pPort)
{
	m_fValue = fValue;
	m_fPrevValue = 0.0f;
	m_bPrevValue = false;

	setRefresh(false);

	// Try replacing an previously equivalent command...
	static qtractorPluginPortCommand *s_pPrevPortCommand = NULL;
	if (s_pPrevPortCommand) {
		qtractorCommand *pLastCommand
			= mainForm()->commands()->lastCommand();
		qtractorCommand *pPrevCommand
			= static_cast<qtractorCommand *> (s_pPrevPortCommand);
		if (pPrevCommand == pLastCommand
			&& s_pPrevPortCommand->port() == pPort) {
			qtractorPluginPortCommand *pLastPortCommand
				= static_cast<qtractorPluginPortCommand *> (pLastCommand);
			if (pLastPortCommand) {
				m_fPrevValue = pLastPortCommand->value();
				m_bPrevValue = true;
				mainForm()->commands()->removeLastCommand();
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
	m_fValue = fValue;
	m_bPrevValue = false;

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
