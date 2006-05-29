// qtractorTrackCommand.h
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

#ifndef __qtractorTrackCommand_h
#define __qtractorTrackCommand_h

#include "qtractorPropertyCommand.h"

#include "qtractorSession.h"
#include "qtractorTrack.h"

#include <qptrlist.h>

// Forward declarations.
class qtractorTrackButton;


//----------------------------------------------------------------------
// class qtractorTrackCommand - declaration.
//

class qtractorTrackCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTrackCommand(qtractorMainForm *pMainForm,
		const QString& sName, qtractorTrack *pTrack);
	// Destructor.
	virtual ~qtractorTrackCommand();

	// Track accessor.
	qtractorTrack *track() const { return m_pTrack; }

protected:

	// Track command methods.
	bool addTrack();
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
	qtractorAddTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack);

	// Track insertion command methods.
	bool redo();
	bool undo();
};


//----------------------------------------------------------------------
// class qtractorRemoveTrackCommand - declaration.
//

class qtractorRemoveTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorRemoveTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack);

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
	qtractorMoveTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack, qtractorTrack *pPrevTrack);

	// Track-move command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack *m_pPrevTrack;
};


//----------------------------------------------------------------------
// class qtractorResizeTrackCommand - declaration.
//

class qtractorResizeTrackCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorResizeTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack, int iItemHeight);

	// Track-move command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	int m_iItemHeight;
};


//----------------------------------------------------------------------
// class qtractorInportTracksCommand - declaration.
//

class qtractorImportTrackCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorImportTrackCommand(qtractorMainForm *pMainForm);
	// Destructor.
	~qtractorImportTrackCommand();

	// Add track to command list.
	void addTrack(qtractorTrack *pTrack);

	// Track-import command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QPtrList<qtractorAddTrackCommand> m_trackCommands;

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
	qtractorEditTrackCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack, const qtractorTrack::Properties& props);

	// Overridden track-edit command methods.
	bool redo();

private:

	// Instance variables.
	qtractorTrack *m_pTrack;
};


//----------------------------------------------------------------------
// class qtractorTrackButtonCommand - declaration.
//

class qtractorTrackButtonCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorTrackButtonCommand(qtractorMainForm *pMainForm,
		qtractorTrackButton *pTrackButton, bool bOn);

	// Track-button command methods.
	bool redo();
	bool undo() { return redo(); }

private:

	// Instance variables.
	qtractorTrack::ToolType m_toolType;
	bool m_bOn;
};


//----------------------------------------------------------------------
// class qtractorTrackGainCommand - declaration.
//

class qtractorTrackGainCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorTrackGainCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack, float fGain);

	// Track-gain command methods.
	bool redo();
	bool undo() { return redo(); }

	// Gain value retrieval.
	float gain() const { return m_fGain; }

private:

	// Instance variables.
	float m_fGain;
	float m_fPrevGain;
	bool  m_bPrevGain;
};


//----------------------------------------------------------------------
// class qtractorTrackPanningCommand - declaration.
//

class qtractorTrackPanningCommand : public qtractorTrackCommand
{
public:

	// Constructor.
	qtractorTrackPanningCommand(qtractorMainForm *pMainForm,
		qtractorTrack *pTrack, float fPanning);

	// Track-panning command methods.
	bool redo();
	bool undo() { return redo(); }

	// Panning value retrieval.
	float panning() const { return m_fPanning; }

private:

	// Instance variables.
	float m_fPanning;
	float m_fPrevPanning;
	bool  m_bPrevPanning;
};


#endif	// __qtractorTrackCommand_h

// end of qtractorTrackCommand.h

