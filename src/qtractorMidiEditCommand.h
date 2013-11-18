// qtractorMidiEditCommand.h
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

#ifndef __qtractorMidiEditCommand_h
#define __qtractorMidiEditCommand_h

#include "qtractorCommand.h"

#include "qtractorMidiEvent.h"

#include <QList>


// Forward declarations.
class qtractorMidiClip;


//----------------------------------------------------------------------
// class qtractorMidiEditCommand - declaration.
//

class qtractorMidiEditCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorMidiEditCommand(qtractorMidiClip *pMidiClip, const QString& sName);

	// Destructor.
	~qtractorMidiEditCommand();

	// Sequence accessor.
	qtractorMidiClip *midiClip() const { return m_pMidiClip; }

	// Primitive command types.
	enum CommandType {
		InsertEvent,
		MoveEvent,
		ResizeEventTime,
		ResizeEventValue,
		RemoveEvent
	};
	
	// Primitive command methods.
	void insertEvent(qtractorMidiEvent *pEvent);
	void moveEvent(qtractorMidiEvent *pEvent,
		int iNote, unsigned long iTime);
	void resizeEventTime(qtractorMidiEvent *pEvent,
		unsigned long iTime, unsigned long iDuration);
	void resizeEventValue(qtractorMidiEvent *pEvent, int iValue);
	void removeEvent(qtractorMidiEvent *pEvent);

	// Check whether the event is already in chain.
	bool findEvent(qtractorMidiEvent *pEvent, CommandType cmd) const;

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Common executive method.
	bool execute(bool bRedo);

	// Adjust edit-command result to prevent event overlapping.
	bool adjust();

private:

	// Event item struct.
	struct Item
	{
		// Item constructor.
		Item(CommandType cmd, qtractorMidiEvent *pEvent, int iNote = 0,
			unsigned long iTime = 0, unsigned long iDuration = 0,
			unsigned int iValue = 0)
			: command(cmd), event(pEvent), note(iNote),
				time(iTime), duration(iDuration), value(iValue),
				autoDelete(false) {}
		// Item members.
		CommandType        command;
		qtractorMidiEvent *event;
		int                note;
		unsigned long      time;
		unsigned long      duration;
		int                value;
		bool               autoDelete;
	};

	// Instance variables.
	qtractorMidiClip *m_pMidiClip;

	QList<Item *> m_items;

	bool m_bAdjusted;

	unsigned long m_iDuration;
};


#endif	// __qtractorMidiEditCommand_h

// end of qtractorMidiEditCommand.h

