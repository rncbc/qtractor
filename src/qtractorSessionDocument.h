// qtractorSessionDocument.h
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorSessionDocument_h
#define __qtractorSessionDocument_h

#include "qtractorDocument.h"
#include "qtractorEngine.h"
#include "qtractorTrack.h"
#include "qtractorFiles.h"
#include "qtractorClip.h"


//-------------------------------------------------------------------------
// qtractorSessionDocument -- Session file import/export helper class.
//

class qtractorSessionDocument : public qtractorDocument
{
public:

	// Constructor.
	qtractorSessionDocument(QDomDocument *pDocument,
		qtractorSession *pSession, qtractorFiles *pFiles);
	// Default destructor.
	~qtractorSessionDocument();

	// Property accessors.
	qtractorSession *session();
	qtractorFiles   *files();

	// Track type helper methods.
	qtractorTrack::TrackType loadTrackType(const QString& sTrackType) const; 
	QString saveTrackType(qtractorTrack::TrackType trackType) const;

	// Audio bus mode helper methods.
	qtractorBus::BusMode loadBusMode(const QString& sBusMode) const;
	QString saveBusMode(qtractorBus::BusMode busMode) const;

	// Clip fade type helper methods.
	qtractorClip::FadeType loadFadeType(const QString& sFadeType) const;
	QString saveFadeType(qtractorClip::FadeType fadeType) const;

	// Elemental loader/savers...
	bool loadElement(QDomElement *pElement);
	bool saveElement(QDomElement *pElement);

private:

	// Instance variables.
	qtractorSession *m_pSession;
	qtractorFiles   *m_pFiles;
};


#endif  // __qtractorSessionDocument_h

// end of qtractorSessionDocument.h
