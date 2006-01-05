// qtractorTracks.cpp
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

#include <qpainter.h>


//----------------------------------------------------------------------------
// qtractorTrackTime -- Track time scale widget.

// Constructor.
qtractorTrackTime::qtractorTrackTime ( qtractorTracks *pTracks,
	QWidget *pParent, const char *pszName )
	: QScrollView(pParent, pszName, WStaticContents)
{
	m_pTracks = pTracks;
	m_pPixmap = new QPixmap();

	QScrollView::viewport()->setBackgroundMode(Qt::PaletteBackground);
	QScrollView::setHScrollBarMode(QScrollView::AlwaysOff);
	QScrollView::setVScrollBarMode(QScrollView::AlwaysOff);

	QScrollView::setFrameStyle(QFrame::ToolBarPanel | QFrame::Plain);

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


// Overall contents update.
void qtractorTrackTime::updateContents (void)
{
	updatePixmap(QScrollView::contentsX(), QScrollView::contentsY());
	QScrollView::updateContents();
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

	//
	// TODO: Draw the time scale...
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

	// Draw edithead line...
	int iEditheadX = m_pTracks->trackView()->editheadX();
	if (iEditheadX >= clipx && iEditheadX < clipx + clipw) {
		QPointArray polyg(3);
		int y = clipy + cliph - 1;
		int d = (cliph >> 2);
		polyg.putPoints(0, 3,
			iEditheadX, y - d,
			iEditheadX, y,
			iEditheadX + d, y - d);
		p->setPen(Qt::blue);
		p->setBrush(Qt::blue);
		p->drawPolygon(polyg);
	}

	// Draw playhead line...
	int iPlayheadX = m_pTracks->trackView()->playheadX();
	if (iPlayheadX >= clipx && iPlayheadX < clipx + clipw) {
		QPointArray polyg(3);
		int y = clipy + cliph - 1;
		int d = (cliph >> 2);
		polyg.putPoints(0, 3,
			iPlayheadX - d, y - d,
			iPlayheadX, y,
			iPlayheadX + d, y - d);
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
	if (pMouseEvent->button() == Qt::LeftButton) {
		// Indirect positioning...
		if (pMouseEvent->state() & (Qt::ShiftButton | Qt::ControlButton)) {
			// Playhead positioning...
			unsigned long iFrame = 0;
			qtractorSession *pSession = m_pTracks->session();
			if (pSession)
				iFrame = pSession->frameFromPixel(pMouseEvent->pos().x());
			m_pTracks->trackView()->setPlayhead(iFrame, true);
		} else {
			// Edithead positioning...
			m_pTracks->trackView()->setEditheadX(pMouseEvent->pos().x());
		}
		// Not quite a selection, but for
		// immediate visual feedback...
		m_pTracks->selectionChangeNotify();
	}

	QScrollView::contentsMousePressEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse pointer move.
void qtractorTrackTime::contentsMouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Just a stub for future tapping...
	QScrollView::contentsMouseMoveEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse button release.
void qtractorTrackTime::contentsMouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	// Just a stub for future tapping...
	QScrollView::contentsMouseReleaseEvent(pMouseEvent);
}


// end of qtractorTrackTime.cpp
