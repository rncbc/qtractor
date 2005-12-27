// qtractorTrackCommand.h
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

#ifndef __qtractorTrackCommand_h
#define __qtractorTrackCommand_h

#include "qtractorCommand.h"

// Forward declarations.
class qtractorTrack;


//----------------------------------------------------------------------
// class qtractorTrackCommand - declaration.
//

class qtractorTrackCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTrackCommand(qtractorMainForm *pMainForm,
		const QString& sName, qtractorTrack *pTrack);
	// Destructor.
	virtual ~qtractorTrackCommand();

	// Track accessor.
	qtractorTrack *track() const { return m_pTrack; }

protected:

	// Track command methods.
	bool addTrack();
	bool removeTrack();

private:

	// Instance variables.
	qtractorTrack *m_pTrack;
};


//----------------------------------------------------------------------
// class qtractorAddTrackCommand - declaration.
//

class qtractorAddTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorAddTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack);

	// Track insertion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorRemoveTrackCommand - declaration.
//

class qtractorRemoveTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorRemoveTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack);

	// Track-removal command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorMoveTrackCommand - declaration.
//

class qtractorMoveTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorMoveTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack, qtractorTrack *pPrevTrack);

	// Track-move command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack *m_pPrevTrack;
};


#endif	// __qtractorTrackCommand_h

// end of qtractorTrackCommand.h

