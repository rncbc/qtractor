// qtractorSessionCursor.cpp
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

#include "qtractorAbout.h"
#include "qtractorSessionCursor.h"
#include "qtractorSession.h"
#include "qtractorClip.h"


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

	resetClips();
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
	if (iFrame == m_iFrame)
		return;

	unsigned int iTrack = 0; 
	qtractorTrack *pTrack = m_pSession->tracks().first();
	while (pTrack && iTrack < m_iTracks) {
		qtractorClip *pClip = NULL;
		qtractorClip *pClipLast = m_ppClips[iTrack];
		// Optimize if seeking forward...
		if (iFrame > m_iFrame)
			pClip = pClipLast;
		// Locate first clip not past the target frame position..
		pClip = seekClip(pTrack, pClip, iFrame);
		// Update cursor track clip...
		m_ppClips[iTrack] = pClip;
		// Now something fulcral for clips around...
		if (pTrack->trackType() == m_syncType) {
			// Tell whether play-head is after loop-start position...
			bool bLooping = (iFrame >= m_pSession->loopStart());
			// Care for old/previous clip...
			if (pClipLast && pClipLast != pClip)
				pClipLast->reset(bLooping);
			// Set final position within target clip...
			if (pClip && bSync) {
				// Take care of overlapping clips...
				unsigned long iClipEnd = pClip->clipStart() + pClip->clipLength();
				while (pClip) {
					unsigned long iClipStart = pClip->clipStart();
					if (iClipStart > iClipEnd)
						break;
					if (iFrame >= iClipStart &&
						iFrame <  iClipStart + pClip->clipLength()) {
						pClip->seek(iFrame - iClipStart);
					} else {
						pClip->reset(bLooping);
					}
					pClip = pClip->next();
				}
			}
		}
		// Next track...
		pTrack = pTrack->next();
		++iTrack;
	}

	// Done.
	m_iFrame = iFrame;
}


// Current frame position accessor.
unsigned long qtractorSessionCursor::frame (void) const
{
	return m_iFrame;
}


// Absolute frame-time position accessors.
void qtractorSessionCursor::setFrameTime ( unsigned long iFrameTime )
{
	m_iFrameTime = iFrameTime;
	m_iFrameDelta = m_iFrame - iFrameTime;
}

unsigned long qtractorSessionCursor::frameTime (void) const
{
	return m_iFrameTime;
}

unsigned long qtractorSessionCursor::frameTimeEx (void) const
{
	return m_iFrameTime + m_iFrameDelta;
}


// Current track clip accessor.
qtractorClip *qtractorSessionCursor::clip ( unsigned int iTrack ) const
{
	return (iTrack < m_iTracks ? m_ppClips[iTrack] : NULL);
}


// Clip locate method.
qtractorClip *qtractorSessionCursor::seekClip (
	qtractorTrack *pTrack, qtractorClip *pClip, unsigned long iFrame ) const
{
	if (pClip == NULL)
		pClip = pTrack->clips().first();

	while (pClip && iFrame > pClip->clipStart() + pClip->clipLength()) {
	//	if (pTrack->trackType() == m_syncType)
	//		pClip->reset(m_pSession->isLooping());
		pClip = pClip->next();
	}

	if (pClip == NULL)
		pClip = pTrack->clips().last();

	return pClip;
}


// Add a track to cursor.
void qtractorSessionCursor::addTrack ( qtractorTrack *pTrack )
{
	if (pTrack == NULL)
		pTrack = m_pSession->tracks().last();

	unsigned int iTracks = m_iTracks + 1;
	if (iTracks < m_iSize) {
		updateClips(m_ppClips, iTracks);
	} else {
		m_iSize += iTracks;
		qtractorClip **ppOldClips = m_ppClips;
		qtractorClip **ppNewClips = new qtractorClip * [m_iSize];
		updateClips(ppNewClips, iTracks);
		m_ppClips = ppNewClips;
		if (ppOldClips)
			delete [] ppOldClips;
	}
	m_iTracks = iTracks;
}


// Update track after adding/removing a clip from cursor.
void qtractorSessionCursor::updateTrack ( qtractorTrack *pTrack )
{
	int iTrack = m_pSession->tracks().find(pTrack);
	if (iTrack >= 0) {
		qtractorClip *pClip = seekClip(pTrack, NULL, m_iFrame);
		if (pClip && pTrack->trackType() == m_syncType
			&& m_iFrame >= pClip->clipStart()
			&& m_iFrame <  pClip->clipStart() + pClip->clipLength()) {
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
	for ( ; iTrack < m_iTracks; ++iTrack)
		m_ppClips[iTrack] = m_ppClips[iTrack + 1];
	m_ppClips[iTrack] = NULL;
}


// Update current track clip under cursor.
void qtractorSessionCursor::updateTrackClip ( qtractorTrack *pTrack )
{
	int iTrack = m_pSession->tracks().find(pTrack);
	if (iTrack >= 0) {
		qtractorClip *pClip = m_ppClips[iTrack];
		if (pClip && pTrack->trackType() == m_syncType) {
			if (m_iFrame >= pClip->clipStart() &&
				m_iFrame <  pClip->clipStart() + pClip->clipLength()) {
				pClip->seek(m_iFrame - pClip->clipStart());
			} else {
				pClip->reset(m_iFrame >= m_pSession->loopStart());
			}
		}
	}
}


// Update (stabilize) cursor.
void qtractorSessionCursor::updateClips ( qtractorClip **ppClips,
	unsigned int iTracks )
{
	// Reset clip positions...
	unsigned int iTrack = 0; 
	qtractorTrack *pTrack = m_pSession->tracks().first();
	while (pTrack && iTrack < iTracks) {
		qtractorClip *pClip = seekClip(pTrack, NULL, m_iFrame);
		if (pClip && pTrack->trackType() == m_syncType
			&& m_iFrame >= pClip->clipStart()
			&& m_iFrame <  pClip->clipStart() + pClip->clipLength()) {
			pClip->seek(m_iFrame - pClip->clipStart());
		}
		ppClips[iTrack] = pClip;
		pTrack = pTrack->next();
		++iTrack;
	}
}


// Reset cursor.
void qtractorSessionCursor::reset (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSessionCursor[%p,%d]::reset()", this, (int) m_syncType);
#endif

	m_iFrameTime = 0;
	m_iFrameDelta = m_iFrame;
}


// Reset track/clips cache.
void qtractorSessionCursor::resetClips (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSessionCursor[%p,%d]::resetClips()", this, (int) m_syncType);
#endif

	// Free existing clip references.
	if (m_ppClips) {
		qtractorClip **ppOldClips = m_ppClips;
		m_ppClips = NULL;
		delete [] ppOldClips;
	}

	// Rebuild the whole bunch...
	m_iTracks = m_pSession->tracks().count();
	m_iSize   = (m_iTracks << 1);

	if (m_iSize > 0) {
		qtractorClip **ppNewClips = new qtractorClip * [m_iSize];
		updateClips(ppNewClips, m_iTracks);
		m_ppClips = ppNewClips;
	}
}


// Frame-time processor (increment only).
void qtractorSessionCursor::process ( unsigned int nframes )
{
	m_iFrameTime += nframes;
}



// end of qtractorSessionCursor.cpp
