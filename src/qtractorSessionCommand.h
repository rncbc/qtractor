// qtractorSessionCommand.h
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorSessionCommand_h
#define __qtractorSessionCommand_h

#include "qtractorPropertyCommand.h"

#include "qtractorSession.h"


// Forward declarations.
class qtractorTimeScaleUpdateNodeCommand;


//----------------------------------------------------------------------
// class qtractorSessionCommand - declaration.
//

class qtractorSessionCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorSessionCommand(const QString& sName,
		qtractorSession *pSession);

	// Destructor.
	virtual ~qtractorSessionCommand();

	// Track accessor.
	qtractorSession *session() const { return m_pSession; }

private:

	// Instance variables.
	qtractorSession *m_pSession;
};


//----------------------------------------------------------------------
// class qtractorSessionLoopCommand - declaration.
//

class qtractorSessionLoopCommand : public qtractorSessionCommand
{
public:

	// Constructor.
	qtractorSessionLoopCommand(qtractorSession *pSession,
		unsigned long iLoopStart, unsigned long iLoopEnd);

	// Session-loop command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	unsigned long m_iLoopStart;
	unsigned long m_iLoopEnd;
};


//----------------------------------------------------------------------
// class qtractorSessionPunchCommand - declaration.
//

class qtractorSessionPunchCommand : public qtractorSessionCommand
{
public:

	// Constructor.
	qtractorSessionPunchCommand(qtractorSession *pSession,
		unsigned long iPunchIn, unsigned long iPunchOut);

	// Session-punch command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	unsigned long m_iPunchIn;
	unsigned long m_iPunchOut;
};


//----------------------------------------------------------------------
// class qtractorSessionEditCommand - declaration.
//

class qtractorSessionEditCommand : public qtractorSessionCommand
{
public:

	// Constructor.
	qtractorSessionEditCommand(qtractorSession *pSession,
		const qtractorSession::Properties& properties);

	// Destructor.
	~qtractorSessionEditCommand();

	// Session-tempo command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorPropertyCommand<qtractorSession::Properties> *m_pPropertiesCommand;
	qtractorTimeScaleUpdateNodeCommand *m_pTempoCommand;
	unsigned short m_iTicksPerBeat;
};


#endif	// __qtractorSessionCommand_h

// end of qtractorSessionCommand.h

