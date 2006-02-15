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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorMidiClip_h
#define __qtractorMidiClip_h

#include "qtractorClip.h"

// Forward declarations.
class qtractorMidiFile;
class qtractorMidiSequence;
class qtractorMidiEvent;


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

	// The main use method.
	bool open(const QString& sFilename, int iTrackChannel = 0);
	// Overloaded open method; reuse an already open MIDI file.
	bool open(qtractorMidiFile *pMidiFile, int iTrackChannel = 0,
		bool bSetTempo = false);

	// MIDI file properties accessors.
	const QString& filename() const;
	unsigned short trackChannel() const;

	// Sequence properties accessors.
	unsigned short channel() const;
	int bank() const;
	int program() const;

	// Intra-clip frame positioning.
	void seek(unsigned long iOffset);

	// Reset clip state.
	void reset();

	// Loop positioning.
	void loop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// MIDI clip special process cycle executive.
	void process(float fGain,
		unsigned long iFrameStart, unsigned long iFrameEnd);

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
	qtractorMidiSequence *m_pSeq;

	QString        m_sFilename;
	unsigned short m_iTrackChannel;

	// To optimize and keep track of current playback
	// position, mostly like an sequence cursor/iterator.
	struct ClipCursor
	{
		// Constructor.
		ClipCursor() : event(NULL), time(0) {}

		// Specific methods.
		void seek(qtractorMidiSequence *pSeq, unsigned long tick);
		void reset(qtractorMidiSequence *pSeq);
		
		// Instance variables.
		qtractorMidiEvent *event;
		unsigned long time;

	} m_clipCursor;
};


#endif  // __qtractorMidiClip_h


// end of qtractorMidiClip.h
