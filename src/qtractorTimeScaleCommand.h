// qtractorTimeScaleCommand.h
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

#ifndef __qtractorTimeScaleCommand_h
#define __qtractorTimeScaleCommand_h

#include "qtractorCommand.h"

#include "qtractorTimeScale.h"


// Forward declarations.
class qtractorClipCommand;
class qtractorClip;


//----------------------------------------------------------------------
// class qtractorTimeScaleNodeCommand - declaration.
//

class qtractorTimeScaleNodeCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTimeScaleNodeCommand(const QString& sName,
		qtractorTimeScale *pTimeScale, unsigned long iFrame = 0,
		float fTempo = 120.0f, unsigned short iBeatType = 2,
		unsigned short iBeatsPerBar = 4, unsigned short iBeatDivisor = 2);

	// Destructor.
	virtual ~qtractorTimeScaleNodeCommand();
	
	// Time-scale accessor.
	qtractorTimeScale *timeScale() const
		{ return m_pTimeScale; }

	// Node properties accessors.
	unsigned long frame() const
		{ return m_iFrame; }
	float tempo() const
		{ return m_fTempo; }
	unsigned short beatType() const
		{ return m_iBeatType; }
	unsigned short beatsPerBar() const
		{ return m_iBeatsPerBar; }
	unsigned short beatDivisor() const
		{ return m_iBeatDivisor; }

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

class qtractorTimeScaleAddNodeCommand : public qtractorTimeScaleNodeCommand
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

class qtractorTimeScaleUpdateNodeCommand : public qtractorTimeScaleNodeCommand
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

class qtractorTimeScaleRemoveNodeCommand : public qtractorTimeScaleNodeCommand
{
public:

	// Constructor.
	qtractorTimeScaleRemoveNodeCommand(qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Node *pNode);

	// Time-scale command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorTimeScaleMoveNodeCommand - declaration.
//

class qtractorTimeScaleMoveNodeCommand : public qtractorTimeScaleNodeCommand
{
public:

	// Constructor.
	qtractorTimeScaleMoveNodeCommand(qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Node *pNode, unsigned long iFrame);

	// Time-scale command methods.
	bool redo();
	bool undo();

private:

	// The new location argument.
	unsigned long  m_iNewFrame;

	// Replaced node salvage.
	bool           m_bOldNode;
	unsigned long  m_iOldFrame;
	float          m_fOldTempo;
	unsigned short m_iOldBeatType;
	unsigned short m_iOldBeatsPerBar;
	unsigned short m_iOldBeatDivisor;
};


//----------------------------------------------------------------------
// class qtractorTimeScaleMarkerCommand - declaration.
//

class qtractorTimeScaleMarkerCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTimeScaleMarkerCommand(const QString& sName,
		qtractorTimeScale *pTimeScale, unsigned long iFrame = 0,
		const QString& sText = QString(),
		const QColor& rgbColor = Qt::darkGray);

	// Time-scale accessor.
	qtractorTimeScale *timeScale() const
		{ return m_pTimeScale; }

	// Marker properties accessors.
	unsigned long frame() const
		{ return m_iFrame; }
	const QString& text() const
		{ return m_sText; }
	const QColor& color() const
		{ return m_rgbColor; }

protected:

	// Executive commands.
	bool addMarker();
	bool updateMarker();
	bool removeMarker();

private:

	// Instance variables.
	qtractorTimeScale *m_pTimeScale;

	unsigned long m_iFrame;
	QString       m_sText;
	QColor        m_rgbColor;
};


//----------------------------------------------------------------------
// class qtractorTimeScaleAddMarkerCommand - declaration.
//

class qtractorTimeScaleAddMarkerCommand : public qtractorTimeScaleMarkerCommand
{
public:

	// Constructor.
	qtractorTimeScaleAddMarkerCommand(
		qtractorTimeScale *pTimeScale, unsigned long iFrame,
		const QString& sText, const QColor& rgbColor = Qt::darkGray);

	// Time-scale command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorTimeScaleUpdateNodeCommand - declaration.
//

class qtractorTimeScaleUpdateMarkerCommand : public qtractorTimeScaleMarkerCommand
{
public:

	// Constructor.
	qtractorTimeScaleUpdateMarkerCommand(
		qtractorTimeScale *pTimeScale, unsigned long iFrame,
		const QString& sText, const QColor& rgbColor = Qt::darkGray);

	// Time-scale command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorTimeScaleRemoveMarkerCommand - declaration.
//

class qtractorTimeScaleRemoveMarkerCommand : public qtractorTimeScaleMarkerCommand
{
public:

	// Constructor.
	qtractorTimeScaleRemoveMarkerCommand(
		qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Marker *pMarker);

	// Time-scale command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorTimeScaleMoveMarkerCommand - declaration.
//

class qtractorTimeScaleMoveMarkerCommand : public qtractorTimeScaleMarkerCommand
{
public:

	// Constructor.
	qtractorTimeScaleMoveMarkerCommand(qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Marker *pMarker, unsigned long iFrame);

	// Time-scale command methods.
	bool redo();
	bool undo();

private:

	// The new location argument.
	unsigned long m_iNewFrame;

	// Replaced marker salvage.
	bool          m_bOldMarker;
	unsigned long m_iOldFrame;
	QString       m_sOldText;
	QColor        m_rgbOldColor;
};


//----------------------------------------------------------------------
// class qtractorTimeScaleCommand - declaration.
//

class qtractorTimeScaleCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTimeScaleCommand(const QString& sName);

	// Destructor.
	~qtractorTimeScaleCommand();

	// Node commands.
	void addNodeCommand(qtractorTimeScaleNodeCommand *pNodeCommand);

	// Time-scale command methods.
	bool redo();
	bool undo();

private:

	// Node commands.
	QList<qtractorTimeScaleNodeCommand *> m_nodeCommands;
};


#endif	// __qtractorTimeScaleCommand_h

// end of qtractorTimeScaleCommand.h
