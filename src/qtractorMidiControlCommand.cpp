// qtractorMidiControlCommand.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiControlCommand.h"
#include "qtractorMidiControlObserver.h"


//----------------------------------------------------------------------
// class qtractorMidiControlObserverCommand - declaration.
//

// Constructor.
qtractorMidiControlObserverCommand::qtractorMidiControlObserverCommand (
	const QString& sName, qtractorMidiControlObserver *pMidiObserver )
	: qtractorCommand(sName), m_pMidiObserver(pMidiObserver)
{
	setRefresh(false);
}


// Map/unmap observer methods.
bool qtractorMidiControlObserverCommand::mapMidiObserver (void) const
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return false;

	if (pMidiControl->isMidiObserverMapped(m_pMidiObserver))
		return false;

	pMidiControl->mapMidiObserver(m_pMidiObserver);
	return true;
}


// Unmap observer methods.
bool qtractorMidiControlObserverCommand::unmapMidiObserver (void) const
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return false;

	if (!pMidiControl->isMidiObserverMapped(m_pMidiObserver))
		return false;

	pMidiControl->unmapMidiObserver(m_pMidiObserver);
	return true;
}


// end of qtractorMidiControlCommand.cpp

