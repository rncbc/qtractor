// qtractorMidiEditTime.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTimeScale.h"

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

	// Rectangular contents update.
	void updateContents(const QRect& rect);
	// Overall contents update.
	void updateContents();

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the time scale.
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Check if some position header is to be dragged...
	bool dragHeadStart(const QPoint& pos);

	// Handle item selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Tempo-map dialog accessor.
	void mouseDoubleClickEvent(QMouseEvent *pMouseEvent);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Handle zoom with mouse wheel.
	void wheelEvent(QWheelEvent *pWheelEvent);

	// Reset drag/select state.
	void resetDragState();

	// Context menu request slot (dummy).
	void contextMenuEvent(QContextMenuEvent *);

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// Show dragging tooltip...
	void showToolTip(unsigned long iFrame) const;
	void showToolTip(const QRect& rect) const;

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

	// The current selecting/dragging head stuff.
	enum DragState {
		DragNone = 0, DragStart, DragSelect,
		DragPlayHead, DragMarker,
		DragEditHead, DragEditTail,
		DragLoopStart, DragLoopEnd,
		DragPunchIn, DragPunchOut
	} m_dragState, m_dragCursor;

	QRect  m_rectDrag;
	QPoint m_posDrag;

	qtractorTimeScale::Marker *m_pDragMarker;
};


#endif  // __qtractorMidiEditTime_h


// end of qtractorMidiEditTime.h
