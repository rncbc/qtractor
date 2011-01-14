// qtractorEngineCommand.h
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

#ifndef __qtractorEngineCommand_h
#define __qtractorEngineCommand_h

#include "qtractorCommand.h"
#include "qtractorEngine.h"

// Forward declarations.
class qtractorMeter;


//----------------------------------------------------------------------
// class qtractorBusCommand - declaration.
//

class qtractorBusCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorBusCommand(const QString& sName, qtractorBus *pBus = NULL,
		qtractorBus::BusMode busMode = qtractorBus::None);

	// Bus accessors.
	void setBus(qtractorBus *pBus)
		{ m_pBus = pBus; }
	qtractorBus *bus() const
		{ return m_pBus; }

	// Bus properties accessors.
	void setBusMode(qtractorBus::BusMode busMode)
		{ m_busMode = busMode; }
	qtractorBus::BusMode busMode() const
		{ return m_busMode; }

	void setBusType(qtractorTrack::TrackType busType)
		{ m_busType = busType; }
	qtractorTrack::TrackType busType() const
		{ return m_busType; }

	void setBusName(const QString& sBusName)
		{ m_sBusName = sBusName; }
	const QString& busName() const
		{ return m_sBusName; }

	void setMonitor(bool bMonitor)
		{ m_bMonitor = bMonitor; }
	bool isMonitor() const
		{ return m_bMonitor; }

	// Special Audio bus properties accessors.
	void setChannels(unsigned short iChannels)
		{ m_iChannels = iChannels; }
	unsigned short channels() const
		{ return m_iChannels; }

	void setAutoConnect(bool bAutoConnect)
		{ m_bAutoConnect = bAutoConnect; }
	bool isAutoConnect() const
		{ return m_bAutoConnect; }

	// Special MIDI bus properties accessors.
	void setInstrumentName(const QString& sInstrumentName)
		{ m_sInstrumentName = sInstrumentName; }
	const QString& instrumentName() const
		{ return m_sInstrumentName; }

protected:

	// Bus command methods.
	bool createBus();
	bool updateBus();
	bool deleteBus();

	// Monitor meter accessor.
	qtractorMeter *meter() const;

private:

	// Instance variables.
	qtractorBus             *m_pBus;
	qtractorBus::BusMode     m_busMode;
	qtractorTrack::TrackType m_busType;
	QString                  m_sBusName;
	bool                     m_bMonitor;
	unsigned short           m_iChannels;
	bool                     m_bAutoConnect;
	QString                  m_sInstrumentName;
};


//----------------------------------------------------------------------
// class qtractorCreateBusCommand - declaration.
//

class qtractorCreateBusCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorCreateBusCommand();

	// Bus creation command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorUpdateBusCommand - declaration.
//

class qtractorUpdateBusCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorUpdateBusCommand(qtractorBus *pBus);

	// Bus update command methods.
	bool redo();
	bool undo() { return redo(); }
};


//----------------------------------------------------------------------
// class qtractorDeleteBusCommand - declaration.
//

class qtractorDeleteBusCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorDeleteBusCommand(qtractorBus *pBus);

	// Bus deletion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorBusMonitorCommand - declaration.
//

class qtractorBusMonitorCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorBusMonitorCommand(qtractorBus *pBus, bool bMonitor);

	// Bus-gain command methods.
	bool redo();
	bool undo() { return redo(); }
};


//----------------------------------------------------------------------
// class qtractorBusGainCommand - declaration.
//

class qtractorBusGainCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorBusGainCommand(qtractorBus *pBus,
		qtractorBus::BusMode busMode, float fGain);

	// Bus-gain command methods.
	bool redo();
	bool undo() { return redo(); }

	// Gain value retrieval.
	float gain() const { return m_fGain; }

	// Last known gain predicate.
	float prevGain() const { return m_fPrevGain; }

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
	qtractorBusPanningCommand(qtractorBus *pBus,
		qtractorBus::BusMode busMode, float fPanning);

	// Bus-panning command methods.
	bool redo();
	bool undo() { return redo(); }

	// Panning value retrieval.
	float panning() const { return m_fPanning; }

	// Last known panning predicate.
	float prevPanning() const { return m_fPrevPanning; }

private:

	// Instance variables.
	float m_fPanning;
	float m_fPrevPanning;
};


#endif	// __qtractorEngineCommand_h

// end of qtractorEngineCommand.h
