// qtractorMidiEditView.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiEditView_h
#define __qtractorMidiEditView_h

#include "qtractorScrollView.h"

#include "qtractorMidiEvent.h"

#include <QPixmap>


// Forward declarations.
class qtractorMidiEditor;

class QResizeEvent;
class QMouseEvent;
class QKeyEvent;

class QToolButton;


//----------------------------------------------------------------------------
// qtractorMidiEditView -- MIDI sequence main view widget.

class qtractorMidiEditView : public qtractorScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEditView(qtractorMidiEditor *pEditor, QWidget *pParent);
	// Destructor.
	~qtractorMidiEditView();

	// Update sequence view content height.
	void updateContentsHeight();
	// Update sequence view content width.
	void updateContentsWidth(int iContentsWidth = 0);

	// Contents update overloaded methods.
	void updateContents(const QRect& rect);
	void updateContents();

	// Current event selection accessors.
	void setEventType(qtractorMidiEvent::EventType eventType);
	qtractorMidiEvent::EventType eventType() const;

protected:

	// Virtual size hint.
	QSize sizeHint() const { return QSize(480, 240); }

	// Scrollbar/tools layout management.
	void setVBarGeometry(QScrollBar& vbar,
		int x, int y, int w, int h);

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the track view
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Focus lost event.
	void focusOutEvent(QFocusEvent *pFocusEvent);

	// Keyboard event handler (made public explicitly).
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Handle item selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Handle zoom with mouse wheel.
	void wheelEvent(QWheelEvent *pWheelEvent);

	// Trap for help/tool-tip and leave events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

protected slots:

	// To have track view in sync with track list.
	void contentsXMovingSlot(int cx, int cy);
	void contentsYMovingSlot(int cx, int cy);

	// (Re)create the complete track view pixmap.
	void updatePixmap(int cx, int cy);

private:

	// The logical parent binding.
	qtractorMidiEditor *m_pEditor;

	// Local zoom control widgets.
	QToolButton *m_pVzoomIn;
	QToolButton *m_pVzoomOut;
	QToolButton *m_pVzoomReset;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// Current selection holder.
	qtractorMidiEvent::EventType m_eventType;
};


#endif  // __qtractorMidiEditView_h


// end of qtractorMidiEditView.h
