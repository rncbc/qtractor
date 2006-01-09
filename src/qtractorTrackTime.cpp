// qtractorTrackTime.cpp
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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorTrackTime.h"
#include "qtractorTrackView.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"

#include <qapplication.h>
#include <qpainter.h>
#include <qcursor.h>

//----------------------------------------------------------------------------
// qtractorTrackTime -- Track time scale widget.

// Constructor.
qtractorTrackTime::qtractorTrackTime ( qtractorTracks *pTracks,
	QWidget *pParent, const char *pszName )
	: QScrollView(pParent, pszName, WStaticContents)
{
	m_pTracks = pTracks;
	m_pPixmap = new QPixmap();

	m_dragState = DragNone;

	QScrollView::viewport()->setBackgroundMode(Qt::PaletteBackground);
	QScrollView::setHScrollBarMode(QScrollView::AlwaysOff);
	QScrollView::setVScrollBarMode(QScrollView::AlwaysOff);

	QScrollView::setFrameStyle(QFrame::ToolBarPanel | QFrame::Plain);

	QScrollView::viewport()->setFocusPolicy(QWidget::ClickFocus);

	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorTrackTime::~qtractorTrackTime (void)
{
	if (m_pPixmap)
		delete m_pPixmap;
	m_pPixmap = NULL;
}


// (Re)create the complete track view pixmap.
void qtractorTrackTime::updatePixmap ( int cx, int /* cy */)
{
	const QColorGroup& cg = QScrollView::colorGroup();

#if 0
	QWidget *pViewport = QScrollView::viewport();
	int w = pViewport->width()  + 2;
	int h = pViewport->height() + 2;
#else
	int w = QScrollView::width();
	int h = QScrollView::height();
#endif

	m_pPixmap->resize(w, h);
	m_pPixmap->fill(cg.color(QColorGroup::Background));

	QPainter p(m_pPixmap);
	p.setViewport(0, 0, w, h);
	p.setWindow(0, 0, w, h);

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Draw the time scale...
	//
	const QFontMetrics& fm = p.fontMetrics();

	int iBeat = pSession->beatFromPixel(cx);
	int x = pSession->pixelFromBeat(iBeat) - cx;
	int y1, y2 = h - 1;
	int iBeatWidth = pSession->pixelFromBeat(1);

	while (x < w) {
		if (pSession->beatIsBar(iBeat)) {
			y1 = 0;
			p.setPen(cg.color(QColorGroup::Text));
			p.drawText(x + 2, y1 + fm.ascent(),
				QString::number(1 + (iBeat / pSession->beatsPerBar())));
		} else {
			y1 = (y2 >> 1);
		}
		p.setPen(cg.color(QColorGroup::Mid));
		p.drawLine(x, y1, x, y2);
		p.setPen(cg.color(QColorGroup::Midlight));
		p.drawLine(x + 1, y1, x + 1, y2);
		x += iBeatWidth;
		iBeat++;
	}
}


// Rectangular contents update.
void qtractorTrackTime::updateContents ( const QRect& rect )
{
	QScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorTrackTime::updateContents (void)
{
	updatePixmap(QScrollView::contentsX(), QScrollView::contentsY());
	QScrollView::updateContents();
}


// Resize event handler.
void qtractorTrackTime::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QScrollView::resizeEvent(pResizeEvent);
	updateContents();
}


// Draw the time scale.
void qtractorTrackTime::drawContents ( QPainter *p,
	int clipx, int clipy, int clipw, int cliph )
{
	p->drawPixmap(clipx, clipy, *m_pPixmap,
		clipx - QScrollView::contentsX(),
		clipy - QScrollView::contentsY(),
		clipw, cliph);

	// Headers a-head...
	int h = QScrollView::height() - 4;
	int d = (h >> 2);
	int x;
	
	// Draw edit-head line...
	x = m_pTracks->trackView()->editHeadX();
	if (x >= clipx - d && x < clipx + clipw + d) {
		QPointArray polyg(3);
		polyg.putPoints(0, 3,
			x + d, h - d,
			x, h,
			x, h - d);
		p->setPen(Qt::blue);
		p->setBrush(Qt::blue);
		p->drawPolygon(polyg);
	}

	// Draw edit-tail line...
	x = m_pTracks->trackView()->editTailX();
	if (x >= clipx - d && x < clipx + clipw + d) {
		QPointArray polyg(3);
		polyg.putPoints(0, 3,
			x, h - d,
			x, h,
			x - d, h - d);
		p->setPen(Qt::blue);
		p->setBrush(Qt::blue);
		p->drawPolygon(polyg);
	}

	// Draw play-head header...
	x = m_pTracks->trackView()->playHeadX();
	if (x >= clipx - d && x < clipx + clipw + d) {
		QPointArray polyg(3);
		polyg.putPoints(0, 3,
			x - d, h - d,
			x, h,
			x + d, h - d);
		p->setPen(Qt::red);
		p->setBrush(Qt::red);
		p->drawPolygon(polyg);
	}
}


// To have timeline in h-sync with main track view.
void qtractorTrackTime::contentsMovingSlot ( int cx, int /*cy*/ )
{
	if (QScrollView::contentsX() != cx)
		m_pTracks->setContentsPos(this, cx, QScrollView::contentsY());
}


// Handle selection/dragging -- mouse button press.
void qtractorTrackTime::contentsMousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	resetDragState();

	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		// Direct snap positioning...
		const bool bModifier = (pMouseEvent->state()
			& (Qt::ShiftButton | Qt::ControlButton));
		int x = pSession->pixelSnap(pMouseEvent->pos().x());
		switch (pMouseEvent->button()) {
		case Qt::LeftButton:
			// Remember what and where we'll be dragging/selecting...
			m_dragState = DragStart;
			m_posDrag   = pMouseEvent->pos();
			// Left-butoon indirect positioning...
			if (bModifier) {
				pSession->setPlayHead(pSession->frameFromPixel(x));
				// Playhead positioning...
				m_pTracks->trackView()->setPlayHeadX(x);
				// Not quite a selection, but for
				// immediate visual feedback...
				m_pTracks->selectionChangeNotify();
			}
			break;
		case Qt::RightButton:
			// Right-butoon indirect positioning...
			if (!bModifier) {
				// Edit-tail positioning...
				m_pTracks->trackView()->setEditTailX(x);
			}
			// Fall thru...
		default:
			break;
		}
	}

	QScrollView::contentsMousePressEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse pointer move.
void qtractorTrackTime::contentsMouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		// Are we already moving/dragging something?
		const QPoint& pos = pMouseEvent->pos();
		switch (m_dragState) {
		case DragSelect:
			drawDragSelect(m_rectDrag);	// Hide.
			m_rectDrag.setRight(pSession->pixelSnap(pos.x()));
			m_pTracks->trackView()->ensureVisible(pos.x(), pos.y(), 8, 8);
			drawDragSelect(m_rectDrag);	// Show.
			break;
		case DragStart:
			if ((m_posDrag - pos).manhattanLength()
				> QApplication::startDragDistance()) {
				// We'll start dragging alright...
				int h = QScrollView::height();	// - 4;
				m_rectDrag.setTop(0);			// h - (h >> 2)
				m_rectDrag.setLeft(pSession->pixelSnap(m_posDrag.x()));
				m_rectDrag.setRight(pSession->pixelSnap(pos.x()));
				m_rectDrag.setBottom(h);
				m_dragState = DragSelect;
				QScrollView::setCursor(QCursor(Qt::CrossCursor));
				drawDragSelect(m_rectDrag);	// Show.
			}
			// Fall thru...
		case DragNone:
		default:
			break;
		}
	}

	QScrollView::contentsMouseMoveEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse button release.
void qtractorTrackTime::contentsMouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QScrollView::contentsMouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		// Which mouse state?
		const bool bModifier = (pMouseEvent->state()
			& (Qt::ShiftButton | Qt::ControlButton));
		switch (m_dragState) {
		case DragSelect:
			drawDragSelect(m_rectDrag);	// Hide.
			if (!bModifier) {
				const QRect rect(m_rectDrag.normalize());
				m_pTracks->trackView()->setEditHeadX(
					pSession->pixelSnap(rect.left()));
				m_pTracks->trackView()->setEditTailX(
					pSession->pixelSnap(rect.right()));
			}
			break;
		case DragStart:
			// Deferred left-button edit-head positioning...
			if (!bModifier) {
				m_pTracks->trackView()->setEditHeadX(
					pSession->pixelSnap(m_posDrag.x()));
			}
			// Fall thru...
		case DragNone:
		default:
			break;
		}
	}

	// Clean up.
	resetDragState();
}


// Draw/hide the current drag selection.
void qtractorTrackTime::drawDragSelect ( const QRect& rectDrag ) const
{
	QPainter p(QScrollView::viewport());
	QRect rect(rectDrag.normalize());

	// Convert rectangle into view coordinates...
	rect.moveTopLeft(QScrollView::contentsToViewport(rect.topLeft()));

	p.setRasterOp(Qt::NotROP);
	p.fillRect(rect, Qt::gray);
}


// Reset drag/select state.
void qtractorTrackTime::resetDragState (void)
{
	// Cancel any dragging out there...
	if (m_dragState == DragSelect) {
		QScrollView::updateContents();
		QScrollView::unsetCursor();
	}

	// Force null state.
	m_dragState = DragNone;
}


// Keyboard event handler.
void qtractorTrackTime::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackTime::keyPressEvent(key=%d)\n", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
		if (m_dragState == DragSelect)
			drawDragSelect(m_rectDrag); // Hide.
		resetDragState();
		break;
	default:
		QScrollView::keyPressEvent(pKeyEvent);
		break;
	}
}


// end of qtractorTrackTime.cpp
