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

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Primitive command types.
	enum CommandType { AddClip, RemoveClip, MoveClip, ChangeClip };
	
	// Add primitive clip item to command list.
	void addItem(CommandType cmd, qtractorClip *pClip,
		qtractorTrack *pTrack, unsigned long iClipStart = 0,
		unsigned long iClipOffset = 0, unsigned long iClipLength = 0);

	// Common executive method.
	bool execute(bool bRedo);

private:

	// Clip item struct.
	struct Item
	{
		// Item constructor.
		Item(CommandType cmd, qtractorClip *pClip,
			qtractorTrack *pTrack, unsigned long iClipStart,
			unsigned long iClipOffset, unsigned long iClipLength)
			: command(cmd), clip(pClip), track(pTrack),
				clipStart(iClipStart),
				clipOffset(iClipOffset),
				clipLength(iClipLength),
				autoDelete(false) {}
		// Item members.
		CommandType    command;
		qtractorClip  *clip;
		qtractorTrack *track;
		unsigned long  clipStart;
		unsigned long  clipOffset;
		unsigned long  clipLength;
		bool           autoDelete;
	};

	// Instance variables.
	QPtrList<Item> m_items;
};


//----------------------------------------------------------------------
// class qtractorAddClipCommand - declaration.
//

class qtractorAddClipCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorAddClipCommand(qtractorMainForm *pMainForm);

	// Special clip record nethod.
	bool addClipRecord(qtractorTrack *pTrack);

	// Add clip item to command list.
	void addItem(qtractorClip *pClip, qtractorTrack *pTrack);
};


//----------------------------------------------------------------------
// class qtractorRemoveClipCommand - declaration.
//

class qtractorRemoveClipCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorRemoveClipCommand(qtractorMainForm *pMainForm);

	// Add clip item to command list.
	void addItem(qtractorClip *pClip);
};


//----------------------------------------------------------------------
// class qtractorMoveClipCommand - declaration.
//

class qtractorMoveClipCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorMoveClipCommand(qtractorMainForm *pMainForm);

	// Add clip item to command list.
	void addItem(qtractorClip *pClip,
		qtractorTrack *pTrack, unsigned long iClipStart);
};


#endif	// __qtractorClipCommand_h

// end of qtractorClipCommand.h

