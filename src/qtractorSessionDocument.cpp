// qtractorSessionDocument.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorSessionDocument.h"

#include "qtractorAudioClip.h"


//-------------------------------------------------------------------------
// qtractorSessionDocument -- Session file import/export helper class.
//

// Constructor.
qtractorSessionDocument::qtractorSessionDocument ( QDomDocument *pDocument,
	qtractorSession *pSession, qtractorFiles *pFiles )
	: qtractorDocument(pDocument, "session")
{
	m_pSession = pSession;
	m_pFiles   = pFiles;
}


// Default destructor.
qtractorSessionDocument::~qtractorSessionDocument (void)
{
}


// Session accessor.
qtractorSession *qtractorSessionDocument::session (void)
{
	return m_pSession;
}


// File list accessor.
qtractorFiles *qtractorSessionDocument::files (void)
{
	return m_pFiles;
}


// Track type helpers.
qtractorTrack::TrackType qtractorSessionDocument::loadTrackType (
	const QString& sTrackType )
{
	qtractorTrack::TrackType trackType = qtractorTrack::None;
	if (sTrackType == "audio")
		trackType = qtractorTrack::Audio;
	else if (sTrackType == "midi")
		trackType = qtractorTrack::Midi;
	return trackType;
}


QString qtractorSessionDocument::saveTrackType (
	qtractorTrack::TrackType trackType )
{
	QString sTrackType;
	switch (trackType) {
	case qtractorTrack::Audio:
		sTrackType = "audio";
		break;
	case qtractorTrack::Midi:
		sTrackType = "midi";
		break;
	case qtractorTrack::None:
	default:
		sTrackType = "none";
		break;
	}
	return sTrackType;
}


// Device bus mode helpers.
qtractorBus::BusMode qtractorSessionDocument::loadBusMode (
	const QString& sBusMode )
{
	qtractorBus::BusMode busMode = qtractorBus::None;
	if (sBusMode == "input")
		busMode = qtractorBus::Input;
	else if (sBusMode == "output")
		busMode = qtractorBus::Output;
	else if (sBusMode == "duplex")
		busMode = qtractorBus::Duplex;
	return busMode;
}


QString qtractorSessionDocument::saveBusMode (
	qtractorBus::BusMode busMode )
{
	QString sBusMode;
	switch (busMode) {
	case qtractorBus::Input:
		sBusMode = "input";
		break;
	case qtractorBus::Output:
		sBusMode = "output";
		break;
	case qtractorBus::Duplex:
		sBusMode = "duplex";
		break;
	case qtractorBus::None:
	default:
		sBusMode = "none";
		break;
	}
	return sBusMode;
}


// The elemental loader implementation.
bool qtractorSessionDocument::loadElement ( QDomElement *pElement )
{
	return m_pSession->loadElement(this, pElement);
}


// The elemental saver implementation.
bool qtractorSessionDocument::saveElement ( QDomElement *pElement )
{
	return m_pSession->saveElement(this, pElement);
}


// end of qtractorSessionDocument.cpp
