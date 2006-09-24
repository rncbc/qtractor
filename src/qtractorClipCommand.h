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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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

	// Primitive command methods.
	void addClip(qtractorClip *pClip, qtractorTrack *pTrack);
	void removeClip(qtractorClip *pClip);
	void moveClip(qtractorClip *pClip, qtractorTrack *pTrack,
		unsigned long iClipStart, unsigned long iClipOffset,
		unsigned long iClipLength);
	void resizeClip(qtractorClip *pClip, unsigned long iClipStart,
		unsigned long iClipOffset, unsigned long iClipLength);
	void fadeInClip(qtractorClip *pClip, unsigned long iFadeInLength);
	void fadeOutClip(qtractorClip *pClip, unsigned long iFadeOutLength);

	// Special clip record method.
	bool addClipRecord(qtractorTrack *pTrack);

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Common executive method.
	bool execute(bool bRedo);

private:

	// Primitive command types.
	enum CommandType {
		AddClip, RemoveClip, MoveClip, ResizeClip, FadeInClip, FadeOutClip };
	
	// Clip item struct.
	struct Item
	{
		// Item constructor.
		Item(CommandType cmd, qtractorClip *pClip, qtractorTrack *pTrack)
			: command(cmd), clip(pClip), track(pTrack),
				clipStart(0), clipOffset(0), clipLength(0),
				fadeInLength(0), fadeOutLength(0), autoDelete(false) {}
		// Item members.
		CommandType    command;
		qtractorClip  *clip;
		qtractorTrack *track;
		unsigned long  clipStart;
		unsigned long  clipOffset;
		unsigned long  clipLength;
		unsigned long  fadeInLength;
		unsigned long  fadeOutLength;
		bool           autoDelete;
	};

	// Instance variables.
	QPtrList<Item> m_items;
};


#endif	// __qtractorClipCommand_h

// end of qtractorClipCommand.h

