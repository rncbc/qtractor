// qtractorSessionCursor.cpp
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

#include "qtractorAbout.h"
#include "qtractorSessionCursor.h"
#include "qtractorSession.h"
#include "qtractorClip.h"

#ifdef DEBUG_0
#include <stdio.h>
#endif


//----------------------------------------------------------------------
// class qtractorSessionCursor - implementation.
//

// Constructor.
qtractorSessionCursor::qtractorSessionCursor ( qtractorSession *pSession,
	unsigned long iFrame, qtractorTrack::TrackType syncType )
{
	m_pSession = pSession;
	m_iFrame   = iFrame;
	m_syncType = syncType;
	m_iTracks  = 0;
	m_ppClips  = NULL;
	m_iSize    = 0;

	reset();
}


// Destructor.
qtractorSessionCursor::~qtractorSessionCursor (void)
{
	m_pSession->unlinkSessionCursor(this);

	if (m_ppClips)
		delete [] m_ppClips;
}


// Session accessor.
qtractorSession *qtractorSessionCursor::session (void) const
{
	return m_pSession;
}

// Clip sync flag accessor.
void qtractorSessionCursor::setSyncType ( qtractorTrack::TrackType syncType )
{
	m_syncType = syncType;
}

qtractorTrack::TrackType qtractorSessionCursor::syncType (void) const
{
	return m_syncType;
}


// General bi-directional locate method.
void qtractorSessionCursor::seek ( unsigned long iFrame, bool bSync )
{
	if (iFrame > m_iFrame)
		seekForward(iFrame);
	else 
	if (iFrame < m_iFrame)
		seekBackward(iFrame);

	if (bSync && m_syncType != qtractorTrack::None) {
		unsigned int iTrack = 0;
		qtractorTrack *pTrack = m_pSession->tracks().first();
		while (pTrack && iTrack < m_iTracks) {
			if (pTrack->trackType() == m_syncType) {
				qtractorClip *pClip = m_ppClips[iTrack];
				if (pClip && iFrame >= pClip->clipStart())
					pClip->seek(iFrame - pClip->clipStart());
			}
			pTrack = pTrack->next();
			iTrack++;
		}
	}
}


// Current frame position accessor.
unsigned long qtractorSessionCursor::frame (void) const
{
	return m_iFrame;
}


// Current track clip accessor.
qtractorClip *qtractorSessionCursor::clip ( unsigned int iTrack ) const
{
	if (iTrack >= m_iTracks)
		return NULL;
	return m_ppClips[iTrack];
}


// Forward locate method.
void qtractorSessionCursor::seekForward ( unsigned long iFrame )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorSessionCursor::seekForward(iFrame=%ld)\n", iFrame);
#endif

	unsigned int iTrack = 0; 
	qtractorTrack *pTrack = m_pSession->tracks().first();
	while (pTrack && iTrack < m_iTracks) {
		m_ppClips[iTrack] = seekClipForward(pTrack, m_ppClips[iTrack], iFrame);
		pTrack = pTrack->next();
		iTrack++;
	}
	m_iFrame = iFrame;
}


// Backward locate method.
void qtractorSessionCursor::seekBackward ( unsigned long iFrame )
{
#ifdef DEBUG_0
	fprintf(stderr, "qtractorSessionCursor::seekBackward(iFrame=%ld)\n", iFrame);
#endif

	unsigned int iTrack = 0; 
	qtractorTrack *pTrack = m_pSession->tracks().first();
	while (pTrack && iTrack < m_iTracks) {
		m_ppClips[iTrack] = seekClipBackward(pTrack, m_ppClips[iTrack], iFrame);
		pTrack = pTrack->next();
		iTrack++;
	}
	m_iFrame = iFrame;
}


// Forward clip locate method.
qtractorClip *qtractorSessionCursor::seekClipForward ( qtractorTrack *pTrack,
	qtractorClip *pClip, unsigned long iFrame )
{
	while (pClip && iFrame > pClip->clipStart() + pClip->clipLength()) {
		if (pTrack->trackType() == m_syncType)
			pClip->reset();
		pClip = pClip->next();
	}

	return pClip;
}


// Backward clip locate method.
qtractorClip *qtractorSessionCursor::seekClipBackward ( qtractorTrack *pTrack,
	qtractorClip *pClip, unsigned long iFrame )
{
	if (pClip == NULL)
		pClip = pTrack->clips().last();

	while (pClip && iFrame < pClip->clipStart()) {
		if (pTrack->trackType() == m_syncType)
			pClip->reset();
		pClip = pClip->prev();
	}

	if (pClip == NULL)
		pClip = pTrack->clips().first();

	return pClip;
}


// Add a track to cursor.
void qtractorSessionCursor::addTrack ( qtractorTrack *pTrack )
{
	if (pTrack == NULL)
		pTrack = m_pSession->tracks().last();

	unsigned int iTrack = m_iTracks;
	if (iTrack >= m_iSize) {
		m_iSize += iTrack;
		qtractorClip **ppClips = new qtractorClip * [m_iSize];
		delete [] m_ppClips;
		m_ppClips = ppClips;
	}
	++m_iTracks;

	update();
}


// Update track after adding/removing a clip from cursor.
void qtractorSessionCursor::updateTrack ( qtractorTrack *pTrack )
{
	int iTrack = m_pSession->tracks().find(pTrack);
	if (iTrack >= 0) {
		qtractorClip *pClip = seekClipForward(pTrack,
			pTrack->clips().first(), m_iFrame);
		if (pClip && m_iFrame >= pClip->clipStart()
			&& pTrack->trackType() == m_syncType) {
			pClip->seek(m_iFrame - pClip->clipStart());
		}
		m_ppClips[iTrack] = pClip;
	}
}


// Remove a track from cursor.
void qtractorSessionCursor::removeTrack ( qtractorTrack *pTrack )
{
	int iTrack = m_pSession->tracks().find(pTrack);
	if (iTrack >= 0)
		removeTrack((unsigned int) iTrack);
}

void qtractorSessionCursor::removeTrack ( unsigned int iTrack )
{
	--m_iTracks;	
	for ( ; iTrack < m_iTracks; iTrack++)
		m_ppClips[iTrack] = m_ppClips[iTrack + 1];
	m_ppClips[iTrack] = NULL;
}


// Update (stabilize) cursor.
void qtractorSessionCursor::update (void)
{
	// Reset clip positions...
	unsigned int iTrack = 0; 
	qtractorTrack *pTrack = m_pSession->tracks().first();
	while (pTrack && iTrack < m_iTracks) {
		qtractorClip *pClip = seekClipForward(pTrack,
			pTrack->clips().first(), m_iFrame);
		if (pClip && pClip->clipStart() < m_iFrame
			&& pTrack->trackType() == m_syncType) {
			pClip->seek(m_iFrame - pClip->clipStart());
		}
		m_ppClips[iTrack] = pClip;
		pTrack = pTrack->next();
		iTrack++;
	}
}


// Reset cursor.
void qtractorSessionCursor::reset (void)
{
	// Free existing clip references.
	if (m_ppClips)
		delete m_ppClips;

	// Rebuild the whole bunch...
	m_iTracks = m_pSession->tracks().count();
	m_iSize   = m_iTracks;
	m_ppClips = new qtractorClip * [m_iSize];

	update();
}


// end of qtractorSessionCursor.cpp
