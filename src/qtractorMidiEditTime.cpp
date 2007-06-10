// qtractorMidiEditTime.cpp
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

#include "qtractorMidiEditTime.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditView.h"

#include "qtractorTimeScale.h"

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPalette>


//----------------------------------------------------------------------------
// qtractorMidiEditTime -- MIDI sequence time scale widget.

// Constructor.
qtractorMidiEditTime::qtractorMidiEditTime (
	qtractorMidiEditor *pEditor, QWidget *pParent )
	: qtractorScrollView(pParent)
{
	m_pEditor = pEditor;

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	qtractorScrollView::setFrameStyle(QFrame::Panel | QFrame::Raised);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::ClickFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
//	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorMidiEditTime::~qtractorMidiEditTime (void)
{
}


// (Re)create the complete view pixmap.
void qtractorMidiEditTime::updatePixmap ( int cx, int /*cy*/)
{
	QWidget *pViewport = qtractorScrollView::viewport();
	int w = pViewport->width();
	int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.window().color());

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	QPainter p(&m_pixmap);
	p.initFrom(this);

	// Draw the time scale...
	//
	const QFontMetrics& fm = p.fontMetrics();
	int x, y1, y2 = h - 1;

	unsigned short iBeat = pTimeScale->beatFromPixel(cx);
	unsigned long iFrameFromBeat = pTimeScale->frameFromBeat(iBeat);
	unsigned long iFramesPerBeat = pTimeScale->frameFromBeat(1);
	unsigned int  iPixelsPerBeat = pTimeScale->pixelFromBeat(1);
	x = pTimeScale->pixelFromFrame(iFrameFromBeat) - cx;
	while (x < w) {
		bool bBeatIsBar = pTimeScale->beatIsBar(iBeat);
		if (bBeatIsBar) {
			y1 = 0;
			p.setPen(pal.windowText().color());
			p.drawText(x + 2, y1 + fm.ascent(), QString::number(
				1 + (iBeat / pTimeScale->beatsPerBar())));
		} else {
			y1 = (y2 >> 1);
		}
		if (bBeatIsBar || iPixelsPerBeat > 16) {
			p.setPen(pal.mid().color());
			p.drawLine(x, y1, x, y2);
			p.setPen(pal.light().color());
			p.drawLine(x + 1, y1, x + 1, y2);
		}
		iFrameFromBeat += iFramesPerBeat;
		x = pTimeScale->pixelFromFrame(iFrameFromBeat) - cx;
		iBeat++;
	}
}


// Rectangular contents update.
void qtractorMidiEditTime::updateContents ( const QRect& rect )
{
	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorMidiEditTime::updateContents (void)
{
	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();
}


// Resize event handler.
void qtractorMidiEditTime::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorScrollView::resizeEvent(pResizeEvent);
	updateContents();
}


// Keyboard event handler.
void qtractorMidiEditTime::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (pKeyEvent->key() == Qt::Key_Escape)
		resetDragState();

	m_pEditor->editView()->keyPressEvent(pKeyEvent);
}


// Draw the time scale.
void qtractorMidiEditTime::drawContents ( QPainter *pPainter, const QRect& rect )
{
	pPainter->drawPixmap(rect, m_pixmap, rect);
}


// To have timeline in h-sync with main view.
void qtractorMidiEditTime::contentsXMovingSlot ( int cx, int /*cy*/ )
{
	if (qtractorScrollView::contentsX() != cx)
		qtractorScrollView::setContentsPos(cx, qtractorScrollView::contentsY());
}


// Handle item selection/dragging -- mouse button press.
void qtractorMidiEditTime::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	resetDragState();

	//
	//	TODO: Process mouse press...
	//
	qtractorScrollView::mousePressEvent(pMouseEvent);

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditTime::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	//
	//	TODO: Process mouse move...
	//
	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditTime::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	//
	//	TODO: Process mouse release...
	//
	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	// Force null state.
	resetDragState();

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Reset drag/select/move state.
void qtractorMidiEditTime::resetDragState (void)
{
	//
	//	TODO: Reset mouse drag-state...
	//
}


// end of qtractorMidiEditTime.cpp
