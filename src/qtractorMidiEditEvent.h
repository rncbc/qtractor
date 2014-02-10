// qtractorMidiEditEvent.h
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

#ifndef __qtractorMidiEditEvent_h
#define __qtractorMidiEditEvent_h

#include "qtractorScrollView.h"

#include "qtractorMidiEvent.h"

#include <QPixmap>


// Forward declarations.
class qtractorMidiEditor;
class qtractorRubberBand;

class QResizeEvent;
class QMouseEvent;
class QKeyEvent;

class QToolButton;


//----------------------------------------------------------------------------
// qtractorMidiEditEventScale -- MIDI event scale widget.

class qtractorMidiEditEventScale : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEditEventScale(qtractorMidiEditor *pEditor, QWidget *pParent);

	// Default destructor.
	~qtractorMidiEditEventScale();

protected:
	
	// Specific event handlers.
	void paintEvent(QPaintEvent *);

	// Draw IEC scale line and label.
	void drawLineLabel(QPainter *p, int y, const QString& sLabel);

private:

	// Local instance variables.
	qtractorMidiEditor *m_pEditor;

	// Running variables.
	int m_iLastY;
};


//----------------------------------------------------------------------------
// qtractorMidiEditEvent -- MIDI sequence event view widget.

class qtractorMidiEditEvent : public qtractorScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEditEvent(qtractorMidiEditor *pEditor, QWidget *pParent);
	// Destructor.
	~qtractorMidiEditEvent();

	// Rectangular contents update.
	void updateContents(const QRect& rect);
	// Overall contents update.
	void updateContents();

	// Current event selection accessors.
	void setEventType(qtractorMidiEvent::EventType eventType);
	qtractorMidiEvent::EventType eventType() const;

	void setEventParam(unsigned short param);
	unsigned short eventParam() const;

protected:

	// Virtual size hint.
	QSize sizeHint() const { return QSize(480, 120); }

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the time scale.
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Focus lost event.
	void focusOutEvent(QFocusEvent *pFocusEvent);

	// Keyboard event handler.
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

	// To have timeline in h-sync with main track view.
	void contentsXMovingSlot(int cx, int cy);

	// (Re)create the time scale pixmap.
	void updatePixmap(int cx, int cy);

private:

	// The logical parent binding.
	qtractorMidiEditor *m_pEditor;

	// Local zoom control widgets.
	QToolButton *m_pHzoomOut;
	QToolButton *m_pHzoomIn;
	QToolButton *m_pHzoomReset;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// Current selection holders.
	qtractorMidiEvent::EventType m_eventType;
	unsigned short m_eventParam;
};


#endif  // __qtractorMidiEditEvent_h


// end of qtractorMidiEditEvent.h
