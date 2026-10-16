// qtractorAudioEditCommand.h
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAudioEditCommand_h
#define __qtractorAudioEditCommand_h

#include "qtractorCommand.h"

#include <QString>


// Forward declarations.
class qtractorAudioClip;
class qtractorAudioFile;


//----------------------------------------------------------------------
// class qtractorAudioEditCommand - declaration.
//
// Implements a single undoable/redoable edit on an audio clip file.
// Two operation types are supported:
//
//   DeleteRange  – removes [iStart, iStart+iLength) from the source file
//                  and writes the result to a new revision file.
//
//   InsertRange  – splices the frames stored in sFilename into the source
//                  file at position iStart, writing the result to a new
//                  revision file.
//
//   MoveRange    – removes [iStart, iStart+iLength) from the source file,
//                  splices the frames stored in sFilename into the source
//                  file at position iAfter and writes the result to a new
//                  revision file.
//
// Both redo() and undo() call the shared execute(bool) helper which
// streams the appropriate source → destination pair in 4096-frame chunks,
// then hot-swaps the clip to point at the new file via setFilenameEx().
//

class qtractorAudioEditCommand : public qtractorCommand
{
public:

	// Operation types.
	enum CommandType { DeleteRange, InsertRange, MoveRange };

	// Constructor.
	// iStart    – intra-file frame position of the selection start.
	// iLength   – length in frames of the selection / region to insert.
	// iAfter    – intra-file frame position of the target region to move.
	//             (only used for MoveRange; otherwise ignored).
	// sFilename – path to the audio file whose frames are to be inserted
	//             (only used for InsertRange and MoveRange; ignored for
	//             DeleteRange).
	qtractorAudioEditCommand(
		qtractorAudioClip *pAudioClip,
		const QString& sName,
		CommandType ctype,
		unsigned long iStart,
		unsigned long iLength,
		unsigned long iAfter = 0,
		const QString& sFilename = QString());

	// Destructor.
	~qtractorAudioEditCommand();

	// Clip accessor.
	qtractorAudioClip *audioClip() const
		{ return m_pAudioClip; }

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Shared executive: streams old→new (bRedo) or new→old (!bRedo).
	bool execute(bool bRedo);

	// Operational helpers.
	bool deleteFrames(float **ppFrames, unsigned int nframes,
		qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
		unsigned long iStart, unsigned long iLength) const;

	bool insertFrames(float **ppFrames, unsigned int nframes,
		qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
		unsigned long iStart, unsigned long iLength) const;

	bool moveFrames(float **ppFrames, unsigned int nframes,
		qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
		unsigned long iStart, unsigned long iLength,
		unsigned long iAfter) const;

	// Stream frames helper.
	bool streamFrames(float **ppFrames, unsigned int nframes,
		qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
		unsigned long iOffset, unsigned long iLength) const;

private:

	qtractorAudioClip *m_pAudioClip;
	CommandType        m_ctype;
	unsigned long      m_iStart;    // intra-file selection start (frames)
	unsigned long      m_iLength;   // selection / insertion length (frames)
	unsigned long      m_iAfter;    // intra-file move target (frames)
	QString            m_sFilename; // clipboard source for InsertRange

	// Revision file paths captured at construction / first redo.
	QString m_sPrevFilename;  // file path before this command (undo target)
	QString m_sNextFilename;  // file path after  this command (redo target)

	// Clip lengths matching the two revision paths.
	unsigned long m_iPrevLength;
	unsigned long m_iNextLength;

	int m_iRedoCount;
};


#endif	// __qtractorAudioEditCommand_h

// end of qtractorAudioEditCommand.h
