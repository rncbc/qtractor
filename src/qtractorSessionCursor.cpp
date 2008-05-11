// qtractorSessionCursor.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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
		// Optimize if seeking forward...
		if (iFrame > m_iFrame)
			pClip = m_ppClips[iTrack];
		// Locate first clip not past
		// the target frame position..
		pClip = seekClip(pTrack, pClip, iFrame);
		// Set fine position within target clip...
		if (pClip && bSync && pTrack->trackType() == m_syncType) {
			unsigned long iClipStart = pClip->clipStart();
			if (iFrame < iClipStart) {
				pClip->seek(0);
			} else if (iFrame < iClipStart + pClip->clipLength()) {
				pClip->seek(iFrame - iClipStart);
			} else {
				pClip->reset(m_pSession->isLooping());
			}
		}
		// Update cursor track clip...
		m_ppClips[iTrack] = pClip;
		pTrack = pTrack->next();
		iTrack++;
	}

	// Done.
	m_iFrame = iFrame;
}


// Current frame position accessor.
unsigned long qtractorSessionCursor::frame (void) const
{
	return m_iFrame;
}


// Absolute frame-time posiion accessor.
void qtractorSessionCursor::setFrameTime ( unsigned long iFrameTime )
{
	m_iFrameTime = iFrameTime;
}

unsigned long qtractorSessionCursor::frameTime (void) const
{
	return m_iFrameTime;
}


// Current track clip accessor.
qtractorClip *qtractorSessionCursor::clip ( unsigned int iTrack ) const
{
	if (iTrack >= m_iTracks)
		return NULL;
	return m_ppClips[iTrack];
}


// Clip locate method.
qtractorClip *qtractorSessionCursor::seekClip (
	qtractorTrack *pTrack, qtractorClip *pClip, unsigned long iFrame ) const
{
	if (pClip == NULL)
		pClip = pTrack->clips().first();

	while (pClip && iFrame > pClip->clipStart() + pClip->clipLength()) {
		if (pTrack->trackType() == m_syncType)
			pClip->reset(m_pSession->isLooping());
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

	unsigned int iTracks = m_iTracks;
	if (iTracks >= m_iSize) {
		m_iSize += iTracks;
		qtractorClip **ppOldClips = m_ppClips;
		qtractorClip **ppNewClips = new qtractorClip * [m_iSize];
		updateClips(ppNewClips, iTracks + 1);
		m_ppClips = ppNewClips;
		delete [] ppOldClips;
	} else {
		updateClips(m_ppClips, iTracks + 1);
	}

	m_iTracks++;
}


// Update track after adding/removing a clip from cursor.
void qtractorSessionCursor::updateTrack ( qtractorTrack *pTrack )
{
	int iTrack = m_pSession->tracks().find(pTrack);
	if (iTrack >= 0) {
		qtractorClip *pClip = seekClip(pTrack, NULL, m_iFrame);
		if (pClip && pTrack->trackType() == m_syncType
			&& m_iFrame >= pClip->clipStart()
			&& m_iFrame <  pClip->clipStart() - pClip->clipLength()) {
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
			&& m_iFrame <  pClip->clipStart() - pClip->clipLength()) {
			pClip->seek(m_iFrame - pClip->clipStart());
		}
		ppClips[iTrack] = pClip;
		pTrack = pTrack->next();
		iTrack++;
	}
}


// Reset cursor.
void qtractorSessionCursor::reset (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSessionCursor[%p,%d]::reset()", this, (int) m_syncType);
#endif

	m_iFrameTime = 0;

	// Free existing clip references.
	if (m_ppClips) {
		qtractorClip **ppOldClips = m_ppClips;
		m_ppClips = NULL;
		delete [] ppOldClips;
	}

	// Rebuild the whole bunch...
	m_iTracks = m_pSession->tracks().count();
	m_iSize   = m_iTracks;

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
