// qtractorMidiThumbView.h
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

#ifndef __qtractorMidiThumbView_h
#define __qtractorMidiThumbView_h

#include <QFrame>


// Forward declarations.
class qtractorMidiEditor;
class qtractorRubberBand;

class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class QKeyEvent;


//-------------------------------------------------------------------------
// qtractorMidiThumbView -- Session track line thumb view.

class qtractorMidiThumbView : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiThumbView(qtractorMidiEditor *pEditor, QWidget *pParent = 0);

	// Update playhead-position.
	void updatePlayHead(unsigned long iPlayHead);

	// (Re)create the complete view pixmap.
	void updateContents();

public slots:

	// Update thumb-position.
	void updateThumb(int dx = 0);

protected:

	// Update view-position.
	void updateView(int dx);

	// Set playhead-position (indirect).
	void setPlayHeadX(int iPlayHeadX);

	// MIDI clip-line paint method.
	void paintEvent(QPaintEvent *pPaintEvent);

	// MIDI clip-line paint method.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Handle selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Reset drag state.
	void resetDragState();

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

private:

	// LOcal MIDI editor instance.
	qtractorMidiEditor *m_pEditor;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// Local contents length (in ticks).
	unsigned long m_iContentsLength;

	// Local playhead positioning.
	int m_iPlayHeadX;

	// The thumb rubber-band widget.
	qtractorRubberBand *m_pRubberBand;

	// Thumb drag-states.
	enum { DragNone = 0, DragStart, DragMove, DragClick } m_dragState;
	QPoint m_posDrag;
};


#endif  // __qtractorMidiThumbView_h


// end of qtractorMidiThumbView.h
