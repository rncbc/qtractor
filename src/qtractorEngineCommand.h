// qtractorEngineCommand.h
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

#ifndef __qtractorEngineCommand_h
#define __qtractorEngineCommand_h

#include "qtractorCommand.h"
#include "qtractorEngine.h"


//----------------------------------------------------------------------
// class qtractorBusCommand - declaration.
//

class qtractorBusCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorBusCommand(qtractorMainForm *pMainForm,
		const QString& sName, qtractorBus *pBus,
		qtractorBus::BusMode busMode);

	// Bus accessor.
	qtractorBus *bus() const { return m_pBus; }
	qtractorBus::BusMode busMode() const { return m_busMode; }

private:

	// Instance variables.
	qtractorBus *m_pBus;
	qtractorBus::BusMode m_busMode;
};


//----------------------------------------------------------------------
// class qtractorBusGainCommand - declaration.
//

class qtractorBusGainCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorBusGainCommand(qtractorMainForm *pMainForm,
		qtractorBus *pBus, qtractorBus::BusMode busMode, float fGain);

	// Bus-gain command methods.
	bool redo();
	bool undo() { return redo(); }

	// Gain value retrieval.
	float gain() const { return m_fGain; }

private:

	// Instance variables.
	float m_fGain;
	float m_fPrevGain;
};


//----------------------------------------------------------------------
// class qtractorBusPanningCommand - declaration.
//

class qtractorBusPanningCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorBusPanningCommand(qtractorMainForm *pMainForm,
		qtractorBus *pBus, qtractorBus::BusMode busMode, float fPanning);

	// Bus-panning command methods.
	bool redo();
	bool undo() { return redo(); }

	// Panning value retrieval.
	float panning() const { return m_fPanning; }

private:

	// Instance variables.
	float m_fPanning;
	float m_fPrevPanning;
};


#endif	// __qtractorEngineCommand_h

// end of qtractorEngineCommand.h
