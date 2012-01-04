// qtractorSessionCursor.h
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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
	unsigned long frameTimeEx() const;

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

	// Update current track clip under cursor.
	void updateTrackClip(qtractorTrack *pTrack);

	// Reset cursor.
	void reset();

	// Reset track/clips cache.
	void resetClips();

	// Frame-time processor (increment only).
	void process(unsigned int nframes);

protected:

	// Clip locate method.
	qtractorClip *seekClip(qtractorTrack *pTrack,
		qtractorClip *pClip, unsigned long iFrame) const;

	// Update (stabilize) cursor.
	void updateClips(qtractorClip **ppClips, unsigned int iTracks);

	// Remove a track from cursor (by index).
	void removeTrack (unsigned int iTrack);

private:

	// Instance variables.
	qtractorSession         *m_pSession;
	unsigned long            m_iFrame;
	unsigned long            m_iFrameTime;
	unsigned long            m_iFrameDelta;
	qtractorTrack::TrackType m_syncType;

	unsigned int   m_iTracks;
	qtractorClip **m_ppClips;
	unsigned int   m_iSize;
};


#endif	// __qtractorSessionCursor_h

// end of qtractorSessionCursor.h

