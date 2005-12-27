// qtractorCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorCommand.h"

#include "qtractorMainForm.h"


//----------------------------------------------------------------------
// class qtractorCommand - implementation.
//

// Constructor.
qtractorCommand::qtractorCommand ( qtractorMainForm *pMainForm,
	const QString& sName )
{
	m_pMainForm   = pMainForm;
	m_sName       = sName;
	m_bAutoDelete = false;
}


// Destructor.
qtractorCommand::~qtractorCommand (void)
{
}


//----------------------------------------------------------------------
// class qtractorCommandList - declaration.
//

// Constructor.
qtractorCommandList::qtractorCommandList (void)
{
	m_commands.setAutoDelete(true);

	m_pLastCommand = NULL;
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


// Cannonical command methods.
bool qtractorCommandList::exec ( qtractorCommand *pCommand )
{
	bool bResult = false;

	// Trim the command list from current last command...
	qtractorCommand *pNextCommand = nextCommand();
	while (pNextCommand) {
		qtractorCommand *pLateCommand = pNextCommand->next();
		m_commands.remove(pNextCommand);
		pNextCommand = pLateCommand;
	}

	if (pCommand) {
		m_commands.append(pCommand);
		m_pLastCommand = m_commands.last();
		if (m_pLastCommand)
			bResult = m_pLastCommand->redo();
	}

	return bResult;
}

bool qtractorCommandList::undo (void)
{
	bool bResult = false;

	if (m_pLastCommand) {
		bResult = m_pLastCommand->undo();
		m_pLastCommand = m_pLastCommand->prev();
	}

	return bResult;
}

bool qtractorCommandList::redo (void)
{
	bool bResult = false;

	m_pLastCommand = nextCommand();
	if (m_pLastCommand)
		bResult = m_pLastCommand->redo();

	return bResult;
}


// end of qtractorCommand.cpp
