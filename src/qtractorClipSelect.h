// qtractorClipSelect.h
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

#ifndef __qtractorClipSelect_h
#define __qtractorClipSelect_h

#include "qtractorClip.h"

#include <qptrlist.h>
#include <qrect.h>


//-------------------------------------------------------------------------
// qtractorClipSelect -- Track clip selection capsule.

class qtractorClipSelect
{
public:

	// Constructor.
	qtractorClipSelect();
	// Default constructor.
	~qtractorClipSelect();

	// Selection item struct.
	struct Item
	{
		// Item constructor.
		Item(qtractorClip *p, const QRect& rect)
			: pClip(p), rectClip(rect) {}
		// Item members.
		qtractorClip *pClip;
		QRect rectClip;
	};

	// Clip selection method.
	void selectClip(qtractorClip *pClip,
		const QRect& rectClip, bool bSelect = true);

	// Dynamic helper:
	// Do all selected clips belong to the same track?
	qtractorTrack *singleTrack();

	// Selection list accessor.
	QPtrList<qtractorClipSelect::Item>& clips();

	// Reset clip selection.
	void clear();

protected:

	// Clip selection item lookup.
	qtractorClipSelect::Item *findClip(qtractorClip *pClip);

private:

	// The clip selection list.
	QPtrList<qtractorClipSelect::Item> m_clips;

	// To cache single track selection.
	bool m_bTrackSingle;
	qtractorTrack *m_pTrackSingle;
	
};


#endif  // __qtractorClipSelect_h


// end of qtractorClipSelect.h
