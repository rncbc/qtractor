// qtractorTrackCommand.h
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

#ifndef __qtractorTrackCommand_h
#define __qtractorTrackCommand_h

#include "qtractorPropertyCommand.h"

#include "qtractorSession.h"

#include <QList>

// Forward declarations.
class qtractorClipCommand;


//----------------------------------------------------------------------
// class qtractorTrackCommand - declaration.
//

class qtractorTrackCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTrackCommand(const QString& sName,
		qtractorTrack *pTrack);

	// Destructor.
	virtual ~qtractorTrackCommand();

	// Track accessor.
	qtractorTrack *track() const { return m_pTrack; }

protected:

	// Extra track list item.
	struct TrackItem
	{	// Constructor.
		TrackItem(qtractorTrack *pTrack, bool bOn)
			: track(pTrack), on(bOn) {}
		// Item members.
		qtractorTrack *track;
		bool on;
	};

	// Track command methods.
	bool addTrack(qtractorTrack *pAfterTrack = NULL);
	bool removeTrack();

private:

	// Instance variables.
	qtractorTrack *m_pTrack;
};


//----------------------------------------------------------------------
// class qtractorAddTrackCommand - declaration.
//

class qtractorAddTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorAddTrackCommand(qtractorTrack *pTrack,
		qtractorTrack *pAfterTrack = NULL);

	// Track insertion command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack *m_pAfterTrack;
};


//----------------------------------------------------------------------
// class qtractorRemoveTrackCommand - declaration.
//

class qtractorRemoveTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorRemoveTrackCommand(qtractorTrack *pTrack);

	// Track-removal command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorMoveTrackCommand - declaration.
//

class qtractorMoveTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorMoveTrackCommand(qtractorTrack *pTrack,
		qtractorTrack *pNextTrack);

	// Track-move command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack *m_pNextTrack;
};


//----------------------------------------------------------------------
// class qtractorResizeTrackCommand - declaration.
//

class qtractorResizeTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorResizeTrackCommand(qtractorTrack *pTrack, int iZoomHeight);

	// Track-move command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	int m_iZoomHeight;
};


//----------------------------------------------------------------------
// class qtractorInportTracksCommand - declaration.
//

class qtractorImportTrackCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorImportTrackCommand(qtractorTrack *pAfterTrack);

	// Destructor.
	~qtractorImportTrackCommand();

	// Add track to command list.
	void addTrack(qtractorTrack *pTrack);

	// Track-import command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack *m_pAfterTrack;

	QList<qtractorAddTrackCommand *> m_trackCommands;

	// Session properties backup stuff.
    qtractorSession::Properties m_sessionProps;
	qtractorPropertyCommand<qtractorSession::Properties> *m_pSaveCommand;
	int m_iSaveCount;
};


//----------------------------------------------------------------------
// class qtractorEditTrackCommand - declaration.
//

class qtractorEditTrackCommand
	: public qtractorPropertyCommand<qtractorTrack::Properties>
{
public:

	// Constructor.
	qtractorEditTrackCommand(qtractorTrack *pTrack,
		const qtractorTrack::Properties& props);

	// Overridden track-edit command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack *m_pTrack;
};


//----------------------------------------------------------------------
// class qtractorTrackControlCommand - declaration.
//

class qtractorTrackControlCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorTrackControlCommand(const QString& sName,
		qtractorTrack *pTrack, bool bMidiControl = false);

protected:

	// Primitive control predicates.
	bool midiControlFeedback();

private:

	// Instance variables.
	bool m_bMidiControl;
	int  m_iMidiControlFeedback; 
};


//----------------------------------------------------------------------
// class qtractorTrackStateCommand - declaration.
//

class qtractorTrackStateCommand : public qtractorTrackControlCommand
{
public:

	// Constructor.
	qtractorTrackStateCommand(qtractorTrack *pTrack,
		qtractorTrack::ToolType toolType, bool bOn,
		bool bMidiControl = false);

	// Destructor.
	~qtractorTrackStateCommand();

	// Track-button command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack::ToolType m_toolType;
	bool m_bOn;

	// Extra track list.
	QList<TrackItem *> m_tracks;

	// Special sub-command needed to track recording clips.
	qtractorClipCommand *m_pClipCommand;
	int m_iRecordCount;
};


//----------------------------------------------------------------------
// class qtractorTrackMonitorCommand - declaration.
//

class qtractorTrackMonitorCommand : public qtractorTrackControlCommand
{
public:

	// Constructor.
	qtractorTrackMonitorCommand(qtractorTrack *pTrack,
		bool bMonitor, bool bMidiControl = false);

	// Destructor.
	~qtractorTrackMonitorCommand();

	// Track-monitoring command methods.
	bool redo();
	bool undo() { return redo(); }

private:

	// Instance variables.
	bool m_bMonitor;

	// Extra track list.
	QList<TrackItem *> m_tracks;
};


//----------------------------------------------------------------------
// class qtractorTrackGainCommand - declaration.
//

class qtractorTrackGainCommand : public qtractorTrackControlCommand
{
public:

	// Constructor.
	qtractorTrackGainCommand(qtractorTrack *pTrack,
		float fGain, bool bMidiControl = false);

	// Track-gain command methods.
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
// class qtractorTrackPanningCommand - declaration.
//

class qtractorTrackPanningCommand : public qtractorTrackControlCommand
{
public:

	// Constructor.
	qtractorTrackPanningCommand(qtractorTrack *pTrack,
		float fPanning, bool bMidiControl = false);

	// Track-panning command methods.
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


#endif	// __qtractorTrackCommand_h

// end of qtractorTrackCommand.h
