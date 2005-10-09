// qtractorTrackTime.h
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

#ifndef __qtractorTrackTime_h
#define __qtractorTrackTime_h

#include <qscrollview.h>

// Forward declarations.
class qtractorTracks;


//----------------------------------------------------------------------------
// qtractorTrackTime -- Track time scale widget.

class qtractorTrackTime : public QScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackTime(qtractorTracks *pTracks,
		QWidget *pParent, const char *pszName = 0);
	// Destructor.
	~qtractorTrackTime();

	// Overall contents update.
	void updateContents();

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the time scale.
	void drawContents(QPainter *p,
		int clipx, int clipy, int clipw, int cliph);

protected slots:

	// To have timeline in h-sync with main track view.
	void contentsMovingSlot(int cx, int cy);

	// (Re)create the time scale pixmap.
	void updatePixmap(int cx, int cy);

private:

	// The logical parent binding.
	qtractorTracks *m_pTracks;

	// Local double-buffering pixmap.
	QPixmap *m_pPixmap;
};


#endif  // __qtractorTrackTime_h


// end of qtractorTrackTime.h
