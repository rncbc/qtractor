// qtractorAudioEditView.h
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAudioEditView_h
#define __qtractorAudioEditView_h

#include "qtractorScrollView.h"

#include <QPixmap>
#include <QBrush>


// Forward declarations.
class qtractorAudioEditor;
class qtractorAudioPeak;

class QResizeEvent;
class QMouseEvent;
class QKeyEvent;

class QToolButton;


//----------------------------------------------------------------------------
// qtractorAudioEditViewScale -- Audio clip time scale widget.

class qtractorAudioEditViewScale : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioEditViewScale(qtractorAudioEditor *pEditor, QWidget *pParent);

	// Default destructor.
	~qtractorAudioEditViewScale();

protected:
	
	// Specific event handlers.
	void paintEvent(QPaintEvent *);

private:

	// Local instance variables.
	qtractorAudioEditor *m_pEditor;

	// Running variables.
	int m_iLastY;
};


//----------------------------------------------------------------------------
// qtractorAudioEditView -- Audio clip view widget.

class qtractorAudioEditView : public qtractorScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioEditView(qtractorAudioEditor *pEditor, QWidget *pParent);
	// Destructor.
	~qtractorAudioEditView();

	// Update view content width.
	void updateContentsWidth(int iContentsWidth = 0);

	// Rectangular contents update.
	void updateContents(const QRect& rect);
	// Overall contents update.
	void updateContents();

protected:

	// Virtual size hint.
	QSize sizeHint() const { return QSize(480, 120); }

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the audio peaks (waveforms).
	void drawAudioPeak(QPainter& painter, int dx, const QRect& clipRect);

	// Draw the time scale.
	void drawContents(QPainter& painter, const QRect& rect);

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
	qtractorAudioEditor *m_pEditor;

	// Local zoom control widgets.
	QToolButton *m_pHzoomOut;
	QToolButton *m_pHzoomIn;
	QToolButton *m_pHzoomReset;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// Optional edge-shadow gradient brushes.
	QBrush m_gradLeft;
	QBrush m_gradRight;
};


#endif  // __qtractorAudioEditView_h


// end of qtractorAudioEditView.h
