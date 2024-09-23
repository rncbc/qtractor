// qtractorClipCommand.h
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorClipCommand_h
#define __qtractorClipCommand_h

#include "qtractorCommand.h"

#include "qtractorClip.h"

#include <QList>

// Forward declarations.
class qtractorTrackCommand;
class qtractorSessionCommand;
class qtractorCurveEditCommand;
class qtractorTimeScaleNodeCommand;
class qtractorTimeScaleMarkerCommand;
class qtractorMidiEditCommand;
class qtractorMidiClip;


//----------------------------------------------------------------------
// class qtractorClipCommand - declaration.
//

class qtractorClipCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorClipCommand(const QString& sName);
	// Destructor.
	virtual ~qtractorClipCommand();

	// Primitive command methods.
	void addClip(qtractorClip *pClip, qtractorTrack *pTrack);
	void removeClip(qtractorClip *pClip);

	// Edit clip command methods.
	void fileClip(qtractorClip *pClip, const QString& sFilename,
		unsigned short iTrackChannel = 0);
	void renameClip(qtractorClip *pClip, const QString& sClipName);
	void moveClip(qtractorClip *pClip, qtractorTrack *pTrack,
		unsigned long iClipStart, unsigned long iClipOffset,
		unsigned long iClipLength);
	void resizeClip(qtractorClip *pClip, unsigned long iClipStart,
		unsigned long iClipOffset, unsigned long iClipLength,
		float fTimeStretch = 0.0f, float fPitchShift = 0.0f);
	void gainClip(qtractorClip *pClip, float fGain);
	void panningClip(qtractorClip *pClip, float fPanning);
	void fadeInClip(qtractorClip *pClip, unsigned long iFadeInLength,
		qtractorClip::FadeType fadeInType);
	void fadeOutClip(qtractorClip *pClip, unsigned long iFadeOutLength,
		qtractorClip::FadeType fadeOutType);
	void timeStretchClip(qtractorClip *pClip, float fTimeStretch);
	void pitchShiftClip(qtractorClip *pClip, float fPitchShift);
	void takeInfoClip(qtractorClip *pClip, qtractorClip::TakeInfo *pTakeInfo);
	void resetClip(qtractorClip *pClip);
	void wsolaClip(qtractorClip *pClip,
		bool bWsolaTimeStretch, bool bWsolaQuickSeek);

	// Special clip record methods.
	bool addClipRecord(qtractorTrack *pTrack, unsigned long iFrameTime);
	bool addClipRecordTake(qtractorTrack *pTrack, qtractorClip *pClip,
		unsigned long iClipStart, unsigned long iClipOffset,
		unsigned long iClipLength, qtractorClip::TakePart *pTakePart = nullptr);

	// When new tracks are needed.
	void addTrack(qtractorTrack *pTrack);

	// When MIDI clips are stretched.
	qtractorMidiEditCommand *createMidiEditCommand(
		qtractorMidiClip *pMidiClip, float fTimeStretch);

	// Composite predicate.
	bool isEmpty() const;

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Conveniency helper: whether a clip is certain type.
	bool isAudioClip(qtractorClip *pClip) const;

	// When clips need to get reopenned.
	void reopenClip(qtractorClip *pClip, bool bClose = false);

	// Common executive method.
	virtual bool execute(bool bRedo);

private:

	// Primitive command types.
	enum CommandType {
		AddClip, RemoveClip, FileClip,
		RenameClip, MoveClip, ResizeClip,
		GainClip, PanningClip, FadeInClip, FadeOutClip,
		TimeStretchClip, PitchShiftClip,
		TakeInfoClip, ResetClip, WsolaClip
	};

	// Clip item struct.
	struct Item
	{
		// Item constructor.
		Item(CommandType cmd, qtractorClip *pClip, qtractorTrack *pTrack)
			: command(cmd), clip(pClip), track(pTrack),
				autoDelete(false), trackChannel(0),
				clipStart(0), clipOffset(0), clipLength(0),
				clipGain(0.0f), clipPanning(0.0f),
				fadeInLength(0), fadeInType(qtractorClip::InQuad), 
				fadeOutLength(0), fadeOutType(qtractorClip::OutQuad),
				timeStretch(0.0f), pitchShift(0.0f),
				wsolaTimeStretch(false), wsolaQuickSeek(false),
				editCommand(nullptr), takeInfo(nullptr) {}
		// Item members.
		CommandType    command;
		qtractorClip  *clip;
		qtractorTrack *track;
		bool           autoDelete;
		QString        filename;
		unsigned short trackChannel;
		QString        clipName;
		unsigned long  clipStart;
		unsigned long  clipOffset;
		unsigned long  clipLength;
		float          clipGain;
		float          clipPanning;
		unsigned long  fadeInLength;
		qtractorClip::FadeType fadeInType;
		unsigned long  fadeOutLength;
		qtractorClip::FadeType fadeOutType;
		float          timeStretch;
		float          pitchShift;
		bool           wsolaTimeStretch;
		bool           wsolaQuickSeek;
		// When MIDI clips are time-stretched...
		qtractorMidiEditCommand *editCommand;
		// When clips have take(record) descriptors...
		qtractorClip::TakeInfo *takeInfo;
	};

	// Instance variables.
	QList<Item *> m_items;

	// When new tracks are needed.
	QList<qtractorTrackCommand *> m_trackCommands;

	// When clips need to reopem.
	QHash<qtractorClip *, bool> m_clips;
};


//----------------------------------------------------------------------
// class qtractorClipTakeCommand - declaration.
//

class qtractorClipTakeCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorClipTakeCommand(qtractorClip::TakeInfo *pTakeInfo,
		qtractorTrack *pTrack = nullptr, int iCurrentTake = -1);

protected:

	// Executive override.
	bool execute(bool bRedo);

private:

	// Instance variables.
	qtractorClip::TakeInfo *m_pTakeInfo;

	int m_iCurrentTake;
};


//----------------------------------------------------------------------
// class qtractorClipRangeCommand - declaration.
//

class qtractorClipRangeCommand : public qtractorClipCommand
{
public:

	// Constructor.
	qtractorClipRangeCommand(const QString& sName);

	// Destructor.
	~qtractorClipRangeCommand();

	// When Loop/Punch changes are needed.
	void addSessionCommand(
		qtractorSessionCommand *pSessionCommand);

	// When automation curves are needed.
	void addCurveEditCommand(
		qtractorCurveEditCommand *pCurveEditCommand);

	// When location markers are needed.
	void addTimeScaleMarkerCommand(
		qtractorTimeScaleMarkerCommand *pTimeScaleMarkerCommand);

	// When tempo-map/time-sig nodes are needed.
	void addTimeScaleNodeCommand(
		qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand);

protected:

	// Executive override.
	bool execute(bool bRedo);

private:

	// Instance variables.
	QList<qtractorSessionCommand *>         m_sessionCommands;
	QList<qtractorCurveEditCommand *>       m_curveEditCommands;
	QList<qtractorTimeScaleMarkerCommand *> m_timeScaleMarkerCommands;
	QList<qtractorTimeScaleNodeCommand *>   m_timeScaleNodeCommands;
};


//----------------------------------------------------------------------
// class qtractorClipContextCommand - declaration. (virtual class)
//

class qtractorClipContextCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorClipContextCommand(const QString& sName);
	// Destructor.
	virtual ~qtractorClipContextCommand();

	// Composite command methods.
	void addMidiClipContext(qtractorMidiClip *pMidiClip);

	// Composite predicate.
	bool isEmpty() const;

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Main executive method.
	bool execute(bool bRedo);

	// Virtual executive method.
	struct MidiClipCtx {
		QString filename;
		unsigned long offset;
		unsigned long length;
	};

	virtual bool executeMidiClipContext(
		qtractorMidiClip *pMidiClip, const MidiClipCtx& mctx, bool bRedo) = 0;

private:

	int m_iRedoCount;

	typedef QHash<qtractorMidiClip *, MidiClipCtx> MidiClipCtxs;

	MidiClipCtxs m_midiClipCtxs;
};


//----------------------------------------------------------------------
// class qtractorClipSaveFileCommand - declaration.
//

class qtractorClipSaveFileCommand : public qtractorClipContextCommand
{
public:

	// Constructor.
	qtractorClipSaveFileCommand();

protected:

	// Context (visitor) executive method.
	bool executeMidiClipContext(
		qtractorMidiClip *pMidiClip, const MidiClipCtx& mctx, bool bRedo);
};


//----------------------------------------------------------------------
// class qtractorClipUnlinkCommand - declaration.
//

class qtractorClipUnlinkCommand : public qtractorClipContextCommand
{
public:

	// Constructor.
	qtractorClipUnlinkCommand();

protected:

	// Context (visitor) executive method.
	bool executeMidiClipContext(
		qtractorMidiClip *pMidiClip, const MidiClipCtx& mctx, bool bRedo);
};


//----------------------------------------------------------------------
// class qtractorClipToolCommand - declaration.
//

class qtractorClipToolCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorClipToolCommand(const QString& sName);
	// Destructor.
	virtual ~qtractorClipToolCommand();

	// Check if a clip is already part of the editing set.
	bool isLinkedMidiClip(qtractorMidiClip *pMidiClip) const;

	// Composite command methods.
	void addMidiEditCommand(qtractorMidiEditCommand *pMidiEditCommand);

	// When additional tempo-map/time-sig nodes are needed.
	void addTimeScaleNodeCommand(qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand);

	// Composite predicate.
	bool isEmpty() const;

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Execute tempo-map/time-sig commands.
	void executeTimeScaleNodeCommands(bool bRedo);

private:

	// Instance variables.
	int m_iRedoCount;

	// Multi-clip command list.
	QList<qtractorMidiEditCommand *> m_midiEditCommands;

	// When tempo-map node commands.
	QList<qtractorTimeScaleNodeCommand *> m_timeScaleNodeCommands;

	qtractorClipSaveFileCommand *m_pClipSaveFileCommand;
};


//----------------------------------------------------------------------
// class qtractorClipRecordExCommand - declaration.
//

class qtractorClipRecordExCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorClipRecordExCommand(qtractorClip *pClipRecordEx, bool bClipRecordEx);

	// Virtual command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	qtractorTrack *m_pTrack;

	bool           m_bClipRecordEx;
	qtractorClip  *m_pClipRecordEx;
};


#endif	// __qtractorClipCommand_h

// end of qtractorClipCommand.h

