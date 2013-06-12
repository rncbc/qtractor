// qtractorMidiControlCommand.h
//
/****************************************************************************
   Copyright (C) 2010-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiControlCommand_h
#define __qtractorMidiControlCommand_h

#include "qtractorCommand.h"

#include "qtractorMidiControl.h"


// Forward declarations...
class qtractorMidiControlObserver;


//----------------------------------------------------------------------
// class qtractorMidiControlObserverCommand - declaration.
//

class qtractorMidiControlObserverCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorMidiControlObserverCommand(const QString& sName,
		qtractorMidiControlObserver *pMidiObserver = NULL);

protected:

	// Map/unmap observer methods.
	bool mapMidiObserver() const;
	bool unmapMidiObserver() const;

private:

	// Instance variables.
	qtractorMidiControlObserver *m_pMidiObserver;
};


//----------------------------------------------------------------------
// class qtractorMidiControlObserverMapCommand - declaration.
//

class qtractorMidiControlObserverMapCommand : public qtractorMidiControlObserverCommand
{
public:

	// Constructor.
	qtractorMidiControlObserverMapCommand(qtractorMidiControlObserver *pMidiObserver)
		: qtractorMidiControlObserverCommand(
			QObject::tr("set controller"), pMidiObserver) {}

	// MIDI control observer command methods.
	bool redo() { return mapMidiObserver(); }
	bool undo() { return unmapMidiObserver(); }
};


//----------------------------------------------------------------------
// class qtractorMidiControlObserverUnmapCommand - declaration.
//

class qtractorMidiControlObserverUnmapCommand : public qtractorMidiControlObserverCommand
{
public:

	// Constructor.
	qtractorMidiControlObserverUnmapCommand(qtractorMidiControlObserver *pMidiObserver)
		: qtractorMidiControlObserverCommand(
			QObject::tr("reset controller"), pMidiObserver) {}

	// MIDI control observer command methods.
	bool redo() { return unmapMidiObserver(); }
	bool undo() { return mapMidiObserver(); }
};


#endif	// __qtractorMidiControlCommand_h

// end of qtractorMidiControlCommand.h

