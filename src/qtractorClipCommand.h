// qtractorClipCommand.h
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

#ifndef __qtractorClipCommand_h
#define __qtractorClipCommand_h

#include "qtractorCommand.h"

#include "qtractorClip.h"

#include <QList>

// Forward declarations.
class qtractorAddTrackCommand;


//----------------------------------------------------------------------
// class qtractorClipCommand - declaration.
//

class qtractorClipCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorClipCommand(const QString& sName);
	// Destructor.
	virtual ~qtractorClipCommand();

	// Primitive command methods.
	void addClip(qtractorClip *pClip, qtractorTrack *pTrack);
	void removeClip(qtractorClip *pClip);
	void renameClip(qtractorClip *pClip, const QString& sClipName);
	void moveClip(qtractorClip *pClip, qtractorTrack *pTrack,
		unsigned long iClipStart, unsigned long iClipOffset,
		unsigned long iClipLength);
	void resizeClip(qtractorClip *pClip, unsigned long iClipStart,
		unsigned long iClipOffset, unsigned long iClipLength);
	void fadeInClip(qtractorClip *pClip, unsigned long iFadeInLength,
		qtractorClip::FadeType fadeInType);
	void fadeOutClip(qtractorClip *pClip, unsigned long iFadeOutLength,
		qtractorClip::FadeType fadeOutType);
	void timeStretchClip(qtractorClip *pClip, float fTimeStetch);

	// Special clip record method.
	bool addClipRecord(qtractorTrack *pTrack);

	// When new tracks are needed.
	void addTrack(qtractorTrack *pTrack);

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Common executive method.
	bool execute(bool bRedo);

private:

	// Primitive command types.
	enum CommandType {
		AddClip, RemoveClip,
		RenameClip,	MoveClip, ResizeClip,
		FadeInClip, FadeOutClip, TimeStretchClip
	};

	// Clip item struct.
	struct Item
	{
		// Item constructor.
		Item(CommandType cmd, qtractorClip *pClip, qtractorTrack *pTrack)
			: command(cmd), clip(pClip), track(pTrack), autoDelete(false),
				clipStart(0), clipOffset(0), clipLength(0),
				fadeInLength(0), fadeInType(qtractorClip::Quadratic), 
				fadeOutLength(0), fadeOutType(qtractorClip::Quadratic),
				timeStretch(1.0f) {}
		// Item members.
		CommandType    command;
		qtractorClip  *clip;
		qtractorTrack *track;
		bool           autoDelete;
		QString        clipName;
		unsigned long  clipStart;
		unsigned long  clipOffset;
		unsigned long  clipLength;
		unsigned long  fadeInLength;
		qtractorClip::FadeType fadeInType;
		unsigned long  fadeOutLength;
		qtractorClip::FadeType fadeOutType;
		float          timeStretch;
	};

	// Instance variables.
	QList<Item *> m_items;

	// When new tracks are needed.
	QList<qtractorAddTrackCommand *> m_trackCommands;
};


#endif	// __qtractorClipCommand_h

// end of qtractorClipCommand.h

