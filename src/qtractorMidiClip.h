// qtractorMidiClip.h
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __qtractorMidiClip_h
#define __qtractorMidiClip_h

#include "qtractorClip.h"
#include "qtractorMidiFile.h"


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

	// (Meta)Session flag accessors.
	void setSessionFlag(bool bSessionFlag);
	bool isSessionFlag() const;

	// Sequence properties accessors.
	qtractorMidiSequence *sequence() const;
	unsigned short channel() const;
	int bank() const;
	int program() const;

	// Clip time reference settler method.
	void updateClipTime();

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
	void drawClip(QPainter *pPainter, const QRect& rect,
		unsigned long iClipOffset);

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
	bool           m_bSessionFlag;

	// To optimize and keep track of current playback
	// position, mostly like an sequence cursor/iterator.
	struct ClipCursor
	{
		// Constructor.
		ClipCursor() : event(NULL), time(0) {}

		// Specific methods.
		void seek(qtractorMidiSequence *pSeq, unsigned long tick);
		void reset(qtractorMidiSequence *pSeq, unsigned long tick = 0);
		
		// Instance variables.
		qtractorMidiEvent *event;
		unsigned long time;
	};

	ClipCursor m_playCursor;
	ClipCursor m_drawCursor;
};


#endif  // __qtractorMidiClip_h


// end of qtractorMidiClip.h
