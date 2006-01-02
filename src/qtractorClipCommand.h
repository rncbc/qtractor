// qtractorClipCommand.h
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

#ifndef __qtractorClipCommand_h
#define __qtractorClipCommand_h

#include "qtractorCommand.h"

#include "qtractorClip.h"

#include <qptrlist.h>


//----------------------------------------------------------------------
// class qtractorClipCommand - declaration.
//

class qtractorClipCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorClipCommand(qtractorMainForm *pMainForm,
		const QString& sName);
	// Destructor.
	virtual ~qtractorClipCommand();

	// Add clip to command list.
	void addClip(qtractorClip *pClip,
		qtractorTrack *pTrack, unsigned long iClipStart);

protected:

	// Clip command methods.
	bool addClipItems();
	bool removeClipItems();
	bool moveClipItems();

private:

	// Clip item struct.
	struct Item
	{
		// Item constructor.
		Item(qtractorClip *pClip,
			qtractorTrack *pTrack, unsigned long iClipStart)
			: clip(pClip), track(pTrack), clipStart(iClipStart) {}
		// Item members.
		qtractorClip  *clip;
		qtractorTrack *track;
		unsigned long  clipStart;
	};

	// Instance variables.
	QPtrList<Item> m_clips;
};


//----------------------------------------------------------------------
// class qtractorAddClipCommand - declaration.
//

class qtractorAddClipCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorAddClipCommand(qtractorMainForm *pMainForm);

	// Clip insertion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorRemoveClipCommand - declaration.
//

class qtractorRemoveClipCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorRemoveClipCommand(qtractorMainForm *pMainForm);

	// Clip-removal command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorMoveClipCommand - declaration.
//

class qtractorMoveClipCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorMoveClipCommand(qtractorMainForm *pMainForm);

	// Clip-move command methods.
	bool redo();
	bool undo();
};


#endif	// __qtractorClipCommand_h

// end of qtractorClipCommand.h

