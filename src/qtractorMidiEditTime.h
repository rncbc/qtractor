// qtractorMidiEditTime.h
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __qtractorMidiEditTime_h
#define __qtractorMidiEditTime_h

#include "qtractorScrollView.h"

#include <QPixmap>


// Forward declarations.
class qtractorMidiEditor;

class QResizeEvent;
class QMouseEvent;
class QKeyEvent;


//----------------------------------------------------------------------------
// qtractorMidiEditTime -- MIDI sequence time scale widget.

class qtractorMidiEditTime : public qtractorScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEditTime(qtractorMidiEditor *pEditor, QWidget *pParent);
	// Destructor.
	~qtractorMidiEditTime();

	// Rectangular contents update.
	void updateContents(const QRect& rect);
	// Overall contents update.
	void updateContents();

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Draw the time scale.
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Handle item selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Reset drag/select/move state.
	void resetDragState();

protected slots:

	// To have timeline in h-sync with main track view.
	void contentsXMovingSlot(int cx, int cy);

	// (Re)create the time scale pixmap.
	void updatePixmap(int cx, int cy);

private:

	// The logical parent binding.
	qtractorMidiEditor *m_pEditor;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;
};


#endif  // __qtractorMidiEditTime_h


// end of qtractorMidiEditTime.h
