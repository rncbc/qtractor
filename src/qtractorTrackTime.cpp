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

#include "qtractorMainForm.h"

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

	const QFont& font = QScrollView::font();
	QScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

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

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	QPainter p(m_pPixmap);
	p.setViewport(0, 0, w, h);
	p.setWindow(0, 0, w, h);
	p.setFont(QScrollView::font());

	// Draw the time scale...
	//
	const QFontMetrics& fm = p.fontMetrics();
	int x, y1, y2 = h - 1;

	unsigned short iBeat = pSession->beatFromPixel(cx);
	unsigned long iFrameFromBeat = pSession->frameFromBeat(iBeat);
	unsigned long iFramesPerBeat = pSession->frameFromBeat(1);
	unsigned int  iPixelsPerBeat = pSession->pixelFromBeat(1);
	x = pSession->pixelFromFrame(iFrameFromBeat) - cx;
	while (x < w) {
		bool bBeatIsBar = pSession->beatIsBar(iBeat);
		if (bBeatIsBar) {
			y1 = 0;
			p.setPen(cg.color(QColorGroup::Text));
			p.drawText(x + 2, y1 + fm.ascent(), QString::number(
				1 + (iBeat / pSession->beatsPerBar())));
		} else {
			y1 = (y2 >> 1);
		}
		if (bBeatIsBar || iPixelsPerBeat > 16) {
			p.setPen(cg.color(QColorGroup::Mid));
			p.drawLine(x, y1, x, y2);
			p.setPen(cg.color(QColorGroup::Midlight));
			p.drawLine(x + 1, y1, x + 1, y2);
		}
		iFrameFromBeat += iFramesPerBeat;
		x = pSession->pixelFromFrame(iFrameFromBeat) - cx;
		iBeat++;
	}

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		QPointArray polyg(3);
		h -= 4;
		int d = (h >> 2);
		p.setPen(Qt::darkCyan);
		p.setBrush(Qt::cyan);
		x = pSession->pixelFromFrame(pSession->loopStart()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			p.drawPolygon(polyg);
		}
		x = pSession->pixelFromFrame(pSession->loopEnd()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x, h - d,
				x, h,
				x - d, h - d);
			p.drawPolygon(polyg);
		}
	}
}


// Rectangular contents update.
void qtractorTrackTime::updateContents ( const QRect& rect, bool bRefresh )
{
	if (bRefresh)
		updatePixmap(QScrollView::contentsX(), QScrollView::contentsY());
	QScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorTrackTime::updateContents ( bool bRefresh )
{
	if (bRefresh)
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
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int h = QScrollView::height() - 4;
	int d = (h >> 2);
	int x;
	
	// Draw edit-head line...
	x = pSession->pixelFromFrame(pSession->editHead());
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
	x = pSession->pixelFromFrame(pSession->editTail());
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

	// Draw special play-head header...
	x = pSession->pixelFromFrame(m_pTracks->trackView()->playHead());
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


// Check if some position header is to be dragged...
bool qtractorTrackTime::dragHeadStart ( const QPoint& pos )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return false;

	// Try to catch mouse clicks over the
	// play/edit-head/tail cursors...
	int h = QScrollView::height() - 4;
	int d = (h >> 1);
	QRect rect(0, h - d, d << 1, d);

	// Check play-head header...
	rect.moveLeft(pSession->pixelFromFrame(
		m_pTracks->trackView()->playHead()) - d);
	if (rect.contains(pos)) {
		m_dragState = DragPlayHead;
		return true;
	}

	// Check loop-points, translating to edit-head/tail headers...
	if (pSession->isLooping()) {
		// Check loop-start header...
		rect.moveLeft(pSession->pixelFromFrame(pSession->loopStart()) - d);
		if (rect.contains(pos)) {
			m_dragState = DragLoopStart;
			return true;
		}
		// Check loop-end header...
		rect.moveLeft(pSession->pixelFromFrame(pSession->loopEnd()) - d);
		if (rect.contains(pos)) {
			m_dragState = DragLoopEnd;
			return true;
		}
	}

	// Check edit-head header...
	rect.moveLeft(pSession->pixelFromFrame(pSession->editHead()) - d);
	if (rect.contains(pos)) {
		m_dragState = DragEditHead;
		return true;
	}

	// Check edit-tail header...
	rect.moveLeft(pSession->pixelFromFrame(pSession->editTail()) - d);
	if (rect.contains(pos)) {
		m_dragState = DragEditTail;
		return true;
	}

	// Nothing.
	return false;
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
		const QPoint& pos = pMouseEvent->pos();
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		switch (pMouseEvent->button()) {
		case Qt::LeftButton:
			// Remember what and where we'll be dragging/selecting...
			m_dragState = DragStart;
			m_posDrag   = pos;
			// Left-butoon indirect positioning...
			if (bModifier) {
				// Playhead positioning...
				m_pTracks->trackView()->setPlayHead(iFrame);
				// Immediately commited...
				pSession->setPlayHead(iFrame);
				// Not quite a selection, rather just
				// for immediate visual feedback...
				m_pTracks->selectionChangeNotify();
			}	// Try to catch mouse clicks over the cursor heads...
			else if (dragHeadStart(m_posDrag)) {
				// We're starting something...
				QScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			}
			break;
		case Qt::MidButton:
			// Mid-butoon indirect positioning...
			if (!bModifier) {
				// Edit-tail positioning...
				m_pTracks->trackView()->setEditHead(iFrame);
				m_pTracks->trackView()->setEditTail(iFrame);
				// Logical contents changed, just for visual feedback...
				m_pTracks->contentsChangeNotify();
			}
			// Fall thru...
		case Qt::RightButton:
			// Right-butoon indirect positioning...
			if (!bModifier) {
				// Edit-tail positioning...
				m_pTracks->trackView()->setEditTail(iFrame);
				// Logical contents changed, just for visual feedback...
				m_pTracks->contentsChangeNotify();
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	// Are we already moving/dragging something?
	const QPoint& pos = pMouseEvent->pos();
	unsigned long iFrame = pSession->frameSnap(
		pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
	int y = m_pTracks->trackView()->contentsY();
	switch (m_dragState) {
	case DragSelect:
		// Rubber-band selection...
		m_rectDrag.setRight(pSession->pixelSnap(pos.x()));
		m_pTracks->trackView()->ensureVisible(pos.x(), y, 16, 0);
		m_pTracks->trackView()->selectRect(m_rectDrag,
			qtractorTrackView::SelectRange, true);
		break;
	case DragPlayHead:
		// Play-head positioning...
		m_pTracks->trackView()->ensureVisible(pos.x(), y, 16, 0);
		m_pTracks->trackView()->setPlayHead(iFrame);
		// Let the change get some immediate visual feedback...
		pMainForm->updateTransportTime(iFrame);
		break;
	case DragLoopStart:
	case DragEditHead:
		// Edit-head positioning...
		m_pTracks->trackView()->ensureVisible(pos.x(), y, 16, 0);
		m_pTracks->trackView()->setEditHead(iFrame);
		break;
	case DragLoopEnd:
	case DragEditTail:
		// Edit-tail positioning...
		m_pTracks->trackView()->ensureVisible(pos.x(), y, 16, 0);
		m_pTracks->trackView()->setEditTail(iFrame);
		break;
	case DragStart:
		// Rubber-band starting...
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// We'll start dragging alright...
			int h = QScrollView::height();	// - 4;
			m_rectDrag.setTop(0);			// h - (h >> 2)
			m_rectDrag.setLeft(pSession->pixelSnap(m_posDrag.x()));
			m_rectDrag.setRight(pSession->pixelSnap(pos.x()));
			m_rectDrag.setBottom(h);
			m_dragState = DragSelect;
			QScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		}
		// Fall thru...
	case DragNone:
	default:
		break;
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
			// Do the final range selection...
			m_pTracks->trackView()->selectRect(m_rectDrag,
				qtractorTrackView::SelectRange, true);
			// Logical contents changed, just for visual feedback...
			m_pTracks->contentsChangeNotify();
			break;
		case DragPlayHead:
			// Play-head positioning commit...
			pSession->setPlayHead(m_pTracks->trackView()->playHead());
			// Not quite a selection, rather just
			// for immediate visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragLoopStart:
			// New loop-start boundary...
			if (pSession->editHead() < pSession->loopEnd()) {
				// Yep, new loop-start point...
				pSession->setLoop(pSession->editHead(), pSession->loopEnd());
				m_pTracks->trackView()->updateContents();
				updateContents();
			}
			// Fall thru here...
		case DragEditHead:
			// Not quite a contents change, but for visual feedback...
			m_pTracks->contentsChangeNotify();
			break;
		case DragLoopEnd:
			// New loop-end boundary...
			if (pSession->loopStart() < pSession->editTail()) {
				// Yep, new loop-end point...
				pSession->setLoop(pSession->loopStart(), pSession->editTail());
				m_pTracks->trackView()->updateContents();
				updateContents();
			}
			// Fall thru here...
		case DragEditTail:
			// Not quite a contents change, but for visual feedback...
			m_pTracks->contentsChangeNotify();
			break;
		case DragStart:
			// Deferred left-button edit-head positioning...
			if (!bModifier) {
				m_pTracks->trackView()->setEditHead(
					pSession->frameSnap(pSession->frameFromPixel(
						m_posDrag.x() > 0 ? m_posDrag.x() : 0)));
				// Not a selection, rather just for visual feedback...
				m_pTracks->contentsChangeNotify();
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


// Reset drag/select state.
void qtractorTrackTime::resetDragState (void)
{
	// Cancel any dragging out there...
	switch (m_dragState) {
	case DragSelect:
		QScrollView::updateContents();
		// Fall thru...
	case DragPlayHead:
	case DragEditHead:
	case DragEditTail:
	case DragLoopStart:
	case DragLoopEnd:
		QScrollView::unsetCursor();
		// Fall thru again...
	case DragNone:
	default:
		break;
	}

	// Force null state.
	m_dragState = DragNone;
	
	// HACK: give focus to track-view... 
	m_pTracks->trackView()->setFocus();
}


// Keyboard event handler.
void qtractorTrackTime::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackTime::keyPressEvent(key=%d)\n", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
		// Restore uncommitted play-head position?...
		if (m_dragState == DragPlayHead && m_pTracks->session()) {
			m_pTracks->trackView()->setPlayHead(
				m_pTracks->session()->playHead());
		}
		resetDragState();
		break;
	default:
		QScrollView::keyPressEvent(pKeyEvent);
		break;
	}
}


// end of qtractorTrackTime.cpp
