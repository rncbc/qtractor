// qtractorScrollView.cpp
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

#include "qtractorScrollView.h"

#include <QScrollBar>
#include <QPainter>

#include <QPaintEvent>


//----------------------------------------------------------------------------
// qtractorScrollView -- abstract scroll view widget.

// Constructor.
qtractorScrollView::qtractorScrollView ( QWidget *pParent )
	: QAbstractScrollArea(pParent)
{
	// Make it non-sense to the viewport background attribute...
	QWidget *pViewport = QAbstractScrollArea::viewport();
	pViewport->setAttribute(Qt::WA_StaticContents);
	pViewport->setAttribute(Qt::WA_NoSystemBackground);
	pViewport->setBackgroundRole(QPalette::NoRole);
	pViewport->setAutoFillBackground(false);
}


// Destructor.
qtractorScrollView::~qtractorScrollView (void)
{
}


// Virtual contents methods.
void qtractorScrollView::setContentsPos ( int cx, int cy )
{
	if (cx < 0)
		cx = 0;
	if (cx != m_rectContents.x())
		QAbstractScrollArea::horizontalScrollBar()->setSliderPosition(cx);

	if (cy < 0)
		cy = 0;
	if (cy != m_rectContents.y())
		QAbstractScrollArea::verticalScrollBar()->setSliderPosition(cy);
}


void qtractorScrollView::resizeContents ( int cw, int ch )
{
	if (cw >= 0 && m_rectContents.width() != cw)
		m_rectContents.setWidth(cw);

	if (ch >= 0 && m_rectContents.height() != ch)
		m_rectContents.setHeight(ch);

	updateScrollBars();
}


// Scrolls contents so that given point is visible.
void qtractorScrollView::ensureVisible ( int cx, int cy, int mx, int my )
{
	QWidget *pViewport = QAbstractScrollArea::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	int dx = m_rectContents.x();
	int dy = m_rectContents.y();
	int cw = m_rectContents.width();
	int ch = m_rectContents.height();

	if (w < (mx << 1))
		mx = (w >> 1);

	if (h < (my << 1))
		my = (h >> 1);

	if (cw <= w) {
		mx = 0;
		dx = 0;
	}

	if (ch <= h) {
		my = 0;
		dy = 0;
	}

	if (cx < mx + dx)
		dx = cx - mx;
	else
	if (cx >= w - mx + dx)
		dx = cx + mx - w;

	if (cy < my + dy)
		dy = cy - my;
	else
	if (cy >= h - my + dy)
		dy = cy + my - h;

	if (dx < 0)
		dx = 0;
	else
	if (dx >= cw - w && w >= cw)
		dx  = cw - w;

	if (dy < 0)
		dy = 0;
	else
	if (dy >= ch - h && h >= ch)
		dy  = ch - h;

	setContentsPos(dx, dy);
}


// Scrollbar stabilization.
void qtractorScrollView::updateScrollBars (void)
{
	QWidget *pViewport = QAbstractScrollArea::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	const int cw
		= (m_rectContents.width() > w ? m_rectContents.width() - w : 0);

	QScrollBar *pHScrollBar = QAbstractScrollArea::horizontalScrollBar();
	if (pHScrollBar->sliderPosition() > cw)
		pHScrollBar->setSliderPosition(cw);
	pHScrollBar->setRange(0, cw);
	pHScrollBar->setSingleStep((w >> 4) + 1);
	pHScrollBar->setPageStep(w);

	const int ch
		= (m_rectContents.height() > h ? m_rectContents.height() - h : 0);

	QScrollBar *pVScrollBar = QAbstractScrollArea::verticalScrollBar();
	if (pVScrollBar->sliderPosition() > ch)
		pVScrollBar->setSliderPosition(ch);
	pVScrollBar->setRange(0, ch);
	pVScrollBar->setSingleStep((h >> 4) + 1);
	pVScrollBar->setPageStep(h);
}


// Specialized event handlers.
void qtractorScrollView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QAbstractScrollArea::resizeEvent(pResizeEvent);
	updateScrollBars();
}


void qtractorScrollView::paintEvent ( QPaintEvent *pPaintEvent )
{
	QPainter painter(QAbstractScrollArea::viewport());
	drawContents(&painter, pPaintEvent->rect().adjusted(0, 0, 1, 1));
}


void qtractorScrollView::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier)) {
		setContentsPos(
			m_rectContents.x() - pWheelEvent->delta(),
			m_rectContents.y());
	}
	else QAbstractScrollArea::wheelEvent(pWheelEvent);
}


// Scroll area updater.
void qtractorScrollView::scrollContentsBy ( int dx, int dy )
{
	int iUpdate = 0;

	if (dx) {
		m_rectContents.moveLeft(m_rectContents.x() - dx);
		++iUpdate;
	}

	if (dy) {
		m_rectContents.moveTop(m_rectContents.y() - dy);
		++iUpdate;
	}

	if (iUpdate > 0) {
		updateContents();
		emit contentsMoving(m_rectContents.x(), m_rectContents.y());
	}
}


// Rectangular contents update.
void qtractorScrollView::updateContents ( const QRect& rect )
{
	QAbstractScrollArea::viewport()->update(
		QRect(contentsToViewport(rect.topLeft()), rect.size()));
}


// Overall contents update.
void qtractorScrollView::updateContents (void)
{
	QAbstractScrollArea::viewport()->update();
}


// Viewport/contents position converters.
QPoint qtractorScrollView::viewportToContents ( const QPoint& pos ) const
{
	return QPoint(pos.x() + m_rectContents.x(), pos.y() + m_rectContents.y()); 
}


QPoint qtractorScrollView::contentsToViewport ( const QPoint& pos ) const
{
	return QPoint(pos.x() - m_rectContents.x(), pos.y() - m_rectContents.y()); 
}


// end of qtractorScrollView.cpp
