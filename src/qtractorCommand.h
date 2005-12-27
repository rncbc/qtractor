// qtractorCommand.h
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

#ifndef __qtractorCommand_h
#define __qtractorCommand_h

#include "qtractorList.h"

#include <qstring.h>

// Forward declarations.
class qtractorMainForm;


//----------------------------------------------------------------------
// class qtractorCommand - declaration.
//

class qtractorCommand : public qtractorList<qtractorCommand>::Link
{
public:

	// Constructor.
	qtractorCommand(qtractorMainForm *pMainForm, const QString& sName);
	// Destructor.
	virtual ~qtractorCommand();

	// Main form accessor.
	qtractorMainForm *mainForm() const { return m_pMainForm; }

	// Descriptive command name accessor.
	const QString& name() const { return m_sName; }

	// Cannonical command methods.
	virtual bool redo() = 0;
	virtual bool undo() = 0;

protected:

	// Auto-removal/deletion flag accessor.
	void setAutoDelete(bool bAutoDelete) { m_bAutoDelete = bAutoDelete; }
	bool isAutoDelete() const { return m_bAutoDelete; }

private:

	// Instance variables.
	qtractorMainForm *m_pMainForm;
	QString           m_sName;
	bool              m_bAutoDelete;
};


//----------------------------------------------------------------------
// class qtractorCommandList - declaration.
//

class qtractorCommandList
{
public:

	// Constructor.
	qtractorCommandList();
	// Destructor.
	~qtractorCommandList();

	// Command stack cleaner.
	void clear();
	
	// Command cursor accessors.
	qtractorCommand *lastCommand() const;
	qtractorCommand *nextCommand() const;

	// Cannonical command methods.
	bool exec(qtractorCommand *pCommand);

	bool undo();
	bool redo();

private:

	// Instance variables.
	qtractorList<qtractorCommand> m_commands;
	qtractorCommand *m_pLastCommand;
};


#endif	// __qtractorCommand_h

// end of qtractorCommand.h

