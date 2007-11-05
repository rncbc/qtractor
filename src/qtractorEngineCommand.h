// qtractorEngineCommand.h
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
	qtractorBusCommand(const QString& sName, qtractorBus *pBus = NULL,
		qtractorBus::BusMode busMode = qtractorBus::None);

	// Bus accessors.
	void setBus(qtractorBus *pBus){ m_pBus = pBus; }
	qtractorBus *bus() const { return m_pBus; }

	// Bus properties accessors.
	void setBusMode(qtractorBus::BusMode busMode) { m_busMode = busMode; }
	qtractorBus::BusMode busMode() const { return m_busMode; }

	void setBusType(qtractorTrack::TrackType busType) { m_busType = busType; }
	qtractorTrack::TrackType busType() const { return m_busType; }

	void setBusName(const QString& sBusName) { m_sBusName = sBusName; }
	const QString& busName() const { return m_sBusName; }

	void setPassthru(bool bPassthru) { m_bPassthru = bPassthru; }
	bool isPassthru() const { return m_bPassthru; }

	// Special Audio bus properties accessors.
	void setChannels(unsigned short iChannels) { m_iChannels = iChannels; }
	unsigned short channels() const { return m_iChannels; }

	void setAutoConnect(bool bAutoConnect) { m_bAutoConnect = bAutoConnect; }
	bool isAutoConnect() const { return m_bAutoConnect; }

protected:

	// Bus command methods.
	bool createBus();
	bool updateBus();
	bool deleteBus();

private:

	// Instance variables.
	qtractorBus             *m_pBus;
	qtractorBus::BusMode     m_busMode;
	qtractorTrack::TrackType m_busType;
	QString                  m_sBusName;
	bool                     m_bPassthru;
	unsigned short           m_iChannels;
	bool                     m_bAutoConnect;
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
// class qtractorBusPassthruCommand - declaration.
//

class qtractorBusPassthruCommand : public qtractorBusCommand
{
public:

	// Constructor.
	qtractorBusPassthruCommand(qtractorBus *pBus, bool bPassthru);

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
	bool  m_bPrevGain;
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
	bool  m_bPrevPanning;
};


#endif	// __qtractorEngineCommand_h

// end of qtractorEngineCommand.h
