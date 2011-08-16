// qtractorMidiClip.h
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

#ifndef __qtractorMidiClip_h
#define __qtractorMidiClip_h

#include "qtractorClip.h"
#include "qtractorMidiCursor.h"
#include "qtractorMidiFile.h"

// Forward declartiuons.
class qtractorMidiEditorForm;


//----------------------------------------------------------------------
// class qtractorMidiClip -- MIDI file/sequence clip.
//

class qtractorMidiClip : public qtractorClip
{
public:

	// Constructor.
	qtractorMidiClip(qtractorTrack *pTrack);
	// Copy constructor.
	qtractorMidiClip(const qtractorMidiClip& clip);

	// Destructor.
	~qtractorMidiClip();

	// Clip (re)open method.
	void open();

	// Brand new clip contents new method.
	bool createMidiFile(const QString& sFilename, int iTrackChannel = 0);

	// The main use method.
	bool openMidiFile(const QString& sFilename, int iTrackChannel = 0,
		int iMode = qtractorMidiFile::Read);

	// MIDI file properties accessors.
	void setTrackChannel(unsigned short iTrackChannel)
		{ m_iTrackChannel = iTrackChannel; }
	unsigned short trackChannel() const
		{ return m_iTrackChannel; }

	void setFormat(unsigned short iFormat)
		{ m_iFormat = iFormat; }
	unsigned short format() const
		{ return m_iFormat; }

	// (Meta)Session flag accessors.
	void setSessionFlag(bool bSessionFlag)
		{ m_bSessionFlag = bSessionFlag; }
	bool isSessionFlag() const
		{ return m_bSessionFlag; }

	// Revisionist accessors.
	void setRevision(unsigned short iRevision)
		{ m_iRevision = iRevision; }
	unsigned short revision() const
		{ return m_iRevision; }

	// Revisionist method.
	QString createFilePathRevision(bool bForce = false);

	// Sequence properties accessors.
	qtractorMidiSequence *sequence() const
		{ return m_pSeq; }

	unsigned short channel() const
		{ return (m_pSeq ? m_pSeq->channel() : 0); }
	int bank() const
		{ return (m_pSeq ? m_pSeq->bank() : -1); }
	int program() const
		{ return (m_pSeq ? m_pSeq->program() : -1); }

	// Statistical cached accessors.
	unsigned char noteMin() const
		{ return m_pSeq ? m_pSeq->noteMin() : m_noteMin; }
	unsigned char noteMax() const
		{ return m_pSeq ? m_pSeq->noteMax() : m_noteMax; }

	// Intra-clip frame positioning.
	void seek(unsigned long iFrame);

	// Reset clip state.
	void reset(bool bLooping);

	// Loop positioning.
	void set_loop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// Clip close-commit (record specific)
	void close();

	// MIDI clip special process cycle executive.
	void process(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Clip paint method.
	void draw(QPainter *pPainter, const QRect& clipRect,
		unsigned long iClipOffset);

	// Clip editor methods.
	bool startEditor(QWidget *pParent);
	void resetEditor(bool bSelectClear);
	void updateEditor(bool bSelectClear);
	bool queryEditor();

	// MIDI clip tool-tip.
	QString toolTip() const;

	// MIDI clip export method.
	typedef void (*ClipExport)(qtractorMidiSequence *, void *);

	bool clipExport(ClipExport pfnClipExport, void *pvArg,
		unsigned long iOffset = 0, unsigned long iLength = 0) const;

	// Default MIDI file format accessors
	// (specific to capture/recording)
	static void setDefaultFormat(unsigned short iFormat);
	static unsigned short defaultFormat();

protected:

	// Virtual document element methods.
	bool loadClipElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveClipElement(qtractorDocument *pDocument, QDomElement *pElement) const;

	// Private cleanup.
	void closeMidiFile();

private:

	// Instance variables.
	qtractorMidiFile *m_pFile;

	unsigned short m_iTrackChannel;
	unsigned short m_iFormat;
	bool           m_bSessionFlag;

	// Revisionist count.
	unsigned short m_iRevision;

	// Statistical cached variables.
	unsigned char  m_noteMin;
	unsigned char  m_noteMax;

	// Most interesting data...
	qtractorMidiSequence *m_pSeq;

	// To optimize and keep track of current playback
	// position, mostly like an sequence cursor/iterator.
	qtractorMidiCursor m_playCursor;
	qtractorMidiCursor m_drawCursor;

	// This clip editor form widget.
	qtractorMidiEditorForm *m_pMidiEditorForm;

	// Default MIDI file format (for capture/record)
	static unsigned short g_iDefaultFormat;
};


#endif  // __qtractorMidiClip_h


// end of qtractorMidiClip.h
