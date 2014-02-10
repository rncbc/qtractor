// qtractorMidiEditList.h
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

#ifndef __qtractorMidiEditList_h
#define __qtractorMidiEditList_h

#include "qtractorScrollView.h"

#include <QPixmap>


// Forward declarations.
class qtractorMidiEditor;

class QResizeEvent;
class QMouseEvent;
class QKeyEvent;


//----------------------------------------------------------------------------
// qtractorMidiEditList -- MIDI sequence key scale widget.

class qtractorMidiEditList : public qtractorScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEditList(qtractorMidiEditor *pEditor, QWidget *pParent);
	// Destructor.
	~qtractorMidiEditList();

	// Item height constants (in pixels).
	enum { ItemHeightMin = 4,  ItemHeightBase = 8, ItemHeightMax = 32 };

	// Item height methods.
	void setItemHeight(unsigned short iItemHeight);
	unsigned short itemHeight() const;

	// Update key-list content height.
	void updateContentsHeight();

	// Rectangular contents update.
	void updateContents(const QRect& rect);
	// Overall contents update.
	void updateContents();

	// Piano keyboard note handler.
	void dragNoteOn(int iNote, int iVelocity = 1);

	// Piano keyboard position handler.
	void dragNoteOn(const QPoint& pos, int iVelocity = 1);

protected:

	// Virtual size hint.
	QSize sizeHint() const { return QSize(60, 240); }

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the time scale.
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Reset drag/select/move state.
	void resetDragState();

	// Handle item selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Handle zoom with mouse wheel.
	void wheelEvent(QWheelEvent *pWheelEvent);

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

protected slots:

	// To have timeline in h-sync with main track view.
	void contentsYMovingSlot(int cx, int cy);

	// (Re)create the time scale pixmap.
	void updatePixmap(int cx, int cy);

private:

	// The logical parent binding.
	qtractorMidiEditor *m_pEditor;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// Current item height.
	unsigned short m_iItemHeight;

	// The current selecting/dragging list stuff.
	enum DragState {
		DragNone = 0, DragStart, DragSelect
	} m_dragState;

	QRect  m_rectDrag;
	QPoint m_posDrag;

	// The current note being keyed on.
	int    m_iNoteOn;
	int    m_iNoteVel;
	QRect  m_rectNote;
};


#endif  // __qtractorMidiEditList_h


// end of qtractorMidiEditList.h
