// qtractorSessionCursor.h
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

#ifndef __qtractorSessionCursor_h
#define __qtractorSessionCursor_h

#include "qtractorTrack.h"

// Forward declarations.
class qtractorClip;


//----------------------------------------------------------------------
// class qtractorSessionCursor - declaration.
//

class qtractorSessionCursor : public qtractorList<qtractorSessionCursor>::Link
{
public:

	// Constructor.
	qtractorSessionCursor(qtractorSession *pSession, unsigned long iFrame = 0,
		qtractorTrack::TrackType syncType = qtractorTrack::None);
	// Destructor.
	~qtractorSessionCursor();

	// Session accessor.
	qtractorSession *session() const;

	// General bi-directional locate method.
	void seek(unsigned long iFrame, bool bSync = false);

	// Current frame position accessor.
	unsigned long frame() const;

	// Absolute frame-time posiion accessors.
	void setFrameTime(unsigned long iFrameTime);
	unsigned long frameTime() const;

	// Clip sync flag accessor.
	void setSyncType(qtractorTrack::TrackType syncType);
	qtractorTrack::TrackType syncType() const;

	// Current track clip accessor.
	qtractorClip *clip(unsigned int iTrack) const;

	// Add a track to cursor.
	void addTrack    (qtractorTrack *pTrack);
	// Update track after adding/removing a clip from cursor.
	void updateTrack (qtractorTrack *pTrack);
	// Remove a track from cursor.
	void removeTrack (qtractorTrack *pTrack);

	// Update (stabilize) cursor.
	void update();
	// Reset cursor.
	void reset();

	// Frame-time processor (increment only).
	void process(unsigned int nframes);

protected:

	// Frame locate method.
	void seekForward  (unsigned long iFrame);
	void seekBackward (unsigned long iFrame);
	
	// Clip locate methods.
	qtractorClip *seekClipForward  (qtractorTrack *pTrack,
		qtractorClip *pClip, unsigned long iFrame);
	qtractorClip *seekClipBackward (qtractorTrack *pTrack,
		qtractorClip *pClip, unsigned long iFrame);

	// Remove a track from cursor (by index).
	void removeTrack (unsigned int iTrack);

private:

	// Instance variables.
	qtractorSession         *m_pSession;
	unsigned long            m_iFrame;
	unsigned long            m_iFrameTime;
	qtractorTrack::TrackType m_syncType;

	unsigned int   m_iTracks;
	qtractorClip **m_ppClips;
	unsigned int   m_iSize;
};


#endif	// __qtractorSessionCursor_h

// end of qtractorSessionCursor.h

