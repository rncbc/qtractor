// qtractorMidiClip.h
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

	// The main use method.
	bool openMidiFile(const QString& sFilename, int iTrackChannel = 0,
		int iMode = qtractorMidiFile::Read);
	// Overloaded open method; reuse an already open MIDI file.
	bool openMidiFile(qtractorMidiFile *pFile, int iTrackChannel = 0);

	// MIDI file properties accessors.
	void setTrackChannel(unsigned short iTrackChannel);
	unsigned short trackChannel() const;

	void setFormat(unsigned short iFormat);
	unsigned short format() const;

	// (Meta)Session flag accessors.
	void setSessionFlag(bool bSessionFlag);
	bool isSessionFlag() const;

	// Revisionist methods.
	void setRevision(unsigned short iRevision);
	unsigned short revision() const;

	QString createFilePathRevision(bool bForce = false);

	// Sequence properties accessors.
	qtractorMidiSequence *sequence() const;
	unsigned short channel() const;
	int bank() const;
	int program() const;

	// Intra-clip frame positioning.
	void seek(unsigned long iFrame);

	// Reset clip state.
	void reset(bool bLooping);

	// Loop positioning.
	void set_loop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// Clip close-commit (record specific)
	void close(bool bForce);

	// MIDI clip special process cycle executive.
	void process(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Clip paint method.
	void draw(QPainter *pPainter, const QRect& clipRect,
		unsigned long iClipOffset);

	// Clip editor methods.
	bool startEditor(QWidget *pParent);
	bool queryEditor();
	void resetEditor();
	void updateEditor();

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
	bool loadClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

private:

	// Instance variables.
	qtractorMidiFile     *m_pFile;
	qtractorMidiSequence *m_pSeq;

	unsigned short m_iTrackChannel;
	unsigned short m_iFormat;
	bool           m_bSessionFlag;

	// Revisionist count.
	unsigned short m_iRevision;

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
