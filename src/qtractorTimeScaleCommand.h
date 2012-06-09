// qtractorTimeScaleCommand.h
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorTimeScaleCommand_h
#define __qtractorTimeScaleCommand_h

#include "qtractorCommand.h"

#include "qtractorTimeScale.h"


// Forward declarations.
class qtractorClipCommand;
class qtractorClip;


//----------------------------------------------------------------------
// class qtractorTimeScaleCommand - declaration.
//

class qtractorTimeScaleCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTimeScaleCommand(const QString& sName,
		qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Node *pNode = NULL, unsigned long iFrame = 0,
		float fTempo = 120.0f, unsigned short iBeatType = 2,
		unsigned short iBeatsPerBar = 4, unsigned short iBeatDivisor = 2);

	// Destructor.
	virtual ~qtractorTimeScaleCommand();
	
	// Time-scale accessor.
	qtractorTimeScale *timeScale() const
		{ return m_pTimeScale; }

protected:

	// Executive commands.
	bool addNode();
	bool updateNode();
	bool removeNode();

	// Make it automatic clip time-stretching command (static).
	qtractorClipCommand *createClipCommand(const QString& sName,
		qtractorTimeScale::Node *pNode, float fOldTempo, float fNewTempo);

private:

	// Instance variables.
	qtractorTimeScale *m_pTimeScale;

	qtractorTimeScale::Node *m_pNode;

	unsigned long  m_iFrame;
	float          m_fTempo;
	unsigned short m_iBeatType;
	unsigned short m_iBeatsPerBar;
	unsigned short m_iBeatDivisor;

	qtractorClipCommand *m_pClipCommand;

	bool m_bAutoTimeStretch;
};


//----------------------------------------------------------------------
// class qtractorTimeScaleAddNodeCommand - declaration.
//

class qtractorTimeScaleAddNodeCommand : public qtractorTimeScaleCommand
{
public:

	// Constructor.
	qtractorTimeScaleAddNodeCommand(
		qtractorTimeScale *pTimeScale, unsigned long iFrame,
		float fTempo = 120.0f, unsigned short iBeatType = 2,
		unsigned short iBeatsPerBar = 4, unsigned short iBeatDivisor = 2);
	
	// Time-scale command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorTimeScaleUpdateNodeCommand - declaration.
//

class qtractorTimeScaleUpdateNodeCommand : public qtractorTimeScaleCommand
{
public:

	// Constructor.
	qtractorTimeScaleUpdateNodeCommand(qtractorTimeScale *pTimeScale,
		unsigned long iFrame, float fTempo, unsigned short iBeatType,
		unsigned short iBeatsPerBar, unsigned short iBeatDivisor);
	
	// Time-scale command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorTimeScaleRemoveNodeCommand - declaration.
//

class qtractorTimeScaleRemoveNodeCommand : public qtractorTimeScaleCommand
{
public:

	// Constructor.
	qtractorTimeScaleRemoveNodeCommand(qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Node *pNode);
	
	// Time-scale command methods.
	bool redo();
	bool undo();
};


#endif	// __qtractorTimeScaleCommand_h

// end of qtractorTimeScaleCommand.h

