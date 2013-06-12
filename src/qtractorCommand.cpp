// qtractorCommand.cpp
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

#include "qtractorCommand.h"

#include <QAction>
#include <QRegExp>


//----------------------------------------------------------------------
// class qtractorCommandList - declaration.
//

// Constructor.
qtractorCommandList::qtractorCommandList (void)
{
	m_pLastCommand = NULL;

	m_commands.setAutoDelete(true);
}

// Destructor.
qtractorCommandList::~qtractorCommandList (void)
{
	clear();
}


// Command stack cleaner.
void qtractorCommandList::clear (void)
{
	m_commands.clear();

	m_pLastCommand = NULL;
}


// Command chain accessors.
qtractorCommand *qtractorCommandList::lastCommand (void) const
{
	return m_pLastCommand;
}

qtractorCommand *qtractorCommandList::nextCommand (void) const
{
	if (m_pLastCommand) {
		return m_pLastCommand->next();
	} else {
		return m_commands.first();
	}
}


// Remove last command from command chain.
void qtractorCommandList::removeLastCommand (void)
{
	if (m_pLastCommand) {
		qtractorCommand *pPrevCommand = m_pLastCommand->prev();
		m_commands.remove(m_pLastCommand);
		m_pLastCommand = pPrevCommand;
	}
}


// Special backout method (EXPERIMENTAL).
void qtractorCommandList::backout ( qtractorCommand *pCommand )
{
	int iUpdate = 0;

	unsigned int flags = qtractorCommand::None;
	while (m_pLastCommand && m_pLastCommand != pCommand) {
		flags |= m_pLastCommand->flags();
		m_pLastCommand->undo();
		removeLastCommand();
		++iUpdate;
	}

	if (iUpdate > 0)
		emit updateNotifySignal(flags);
}


// Cannonical command methods.
bool qtractorCommandList::push ( qtractorCommand *pCommand )
{
	// Trim the command list from current last command...
	qtractorCommand *pNextCommand = nextCommand();
	while (pNextCommand) {
		qtractorCommand *pLateCommand = pNextCommand->next();
		m_commands.remove(pNextCommand);
		pNextCommand = pLateCommand;
	}

	if (pCommand == NULL)
		return false;

	// It must be this last one...
	m_commands.append(pCommand);
	m_pLastCommand = m_commands.last();

	return (m_pLastCommand != NULL);
}

bool qtractorCommandList::exec ( qtractorCommand *pCommand )
{
	bool bResult = false;

	// Append command...
	if (push(pCommand)) {
		// Execute operation...
		bResult = m_pLastCommand->redo();
		// Notify commanders...
		emit updateNotifySignal(m_pLastCommand->flags());
	}

	return bResult;
}

bool qtractorCommandList::undo (void)
{
	bool bResult = false;

	if (m_pLastCommand) {
		// Undo operation...
		bResult = m_pLastCommand->undo();
		// Backward one command...
		unsigned int flags = m_pLastCommand->flags();
		m_pLastCommand = m_pLastCommand->prev();
		// Notify commanders...
		emit updateNotifySignal(flags);
	}

	return bResult;
}

bool qtractorCommandList::redo (void)
{
	bool bResult = false;

	// Forward one command...
	m_pLastCommand = nextCommand();
	if (m_pLastCommand) {
		// Redo operation...
		bResult = m_pLastCommand->redo();
		// Notify commanders...
		emit updateNotifySignal(m_pLastCommand->flags());
	}

	return bResult;
}


// Command action update helper.
void qtractorCommandList::updateAction ( QAction *pAction,
	qtractorCommand *pCommand ) const
{
	const QRegExp rxBrackets("[\\s]+\\([^\\)]+\\)$");
	pAction->setText(pAction->text().remove(rxBrackets));
	pAction->setStatusTip(pAction->statusTip().remove(rxBrackets));
	pAction->setToolTip(pAction->toolTip().remove(rxBrackets));
	if (pCommand) {
		const QString sCommand  = QString(pCommand->name()).remove(rxBrackets);
		const QString sBrackets = QString(" (%1)").arg(sCommand);
		pAction->setText(pAction->text() + sBrackets);
		pAction->setStatusTip(pAction->statusTip() + sBrackets);
		pAction->setToolTip(pAction->toolTip() + sBrackets);
	}
	pAction->setEnabled(pCommand != NULL);
}


// end of qtractorCommand.cpp
