// qtractorTrackTime.cpp
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorTrackTime.h"
#include "qtractorTrackView.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"

#include "qtractorSessionCommand.h"

#include "qtractorMainForm.h"

#include "qtractorTimeScaleForm.h"

#include <QApplication>
#include <QPainter>
#include <QCursor>
#include <QPixmap>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>

#include <QPolygon>


//----------------------------------------------------------------------------
// qtractorTrackTime -- Track time scale widget.

// Constructor.
qtractorTrackTime::qtractorTrackTime ( qtractorTracks *pTracks,
	QWidget *pParent ) : qtractorScrollView(pParent)
{
	m_pTracks = pTracks;

	m_dragState  = DragNone;
	m_dragCursor = DragNone;

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	qtractorScrollView::setFrameStyle(QFrame::Panel | QFrame::Raised);

	qtractorScrollView::setFocusPolicy(Qt::NoFocus);
//	qtractorScrollView::viewport()->setFocusPolicy(Qt::ClickFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
//	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);
	qtractorScrollView::setMouseTracking(true);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorTrackTime::~qtractorTrackTime (void)
{
}


// (Re)create the complete track view pixmap.
void qtractorTrackTime::updatePixmap ( int cx, int /* cy */)
{
	QWidget *pViewport = qtractorScrollView::viewport();
	int w = pViewport->width();
	int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.window().color());

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	// Draw the time scale...
	//
	const QFontMetrics& fm = painter.fontMetrics();
	int x, y1, y2 = h - 1;

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekPixel(cx);

	unsigned short iPixelsPerBeat = pNode->pixelsPerBeat();
	unsigned int iBeat = pNode->beatFromPixel(cx);
	int x1 = x = pNode->pixelFromBeat(iBeat) - cx;
	while (x < w) {
		bool bBeatIsBar = pNode->beatIsBar(iBeat) && (x >= x1);
		if (bBeatIsBar) {
			y1 = 0;
			painter.setPen(pal.windowText().color());
			painter.drawText(x + 2, y1 + fm.ascent(),
				QString::number(pNode->barFromBeat(iBeat) + 1));
			x1 = x + 16;
			if (iBeat == pNode->beat) {
				iPixelsPerBeat = pNode->pixelsPerBeat();
				painter.setPen(pal.base().color().value() < 0x7f
					? pal.light().color() : pal.dark().color()); 
				painter.drawText(x1, y1 + fm.ascent(),
					QString("%1 %2/%3")
					.arg(pNode->tempo, 0, 'g', 3)
					.arg(pNode->beatsPerBar)
					.arg(1 << pNode->beatDivisor));
			}
		} else {
			y1 = (y2 >> 1);
		}
		if (bBeatIsBar || iPixelsPerBeat > 16) {
			painter.setPen(pal.mid().color());
			painter.drawLine(x, y1, x, y2);
			painter.setPen(pal.light().color());
			painter.drawLine(x + 1, y1, x + 1, y2);
		}
		pNode = cursor.seekBeat(++iBeat);
		x = pNode->pixelFromBeat(iBeat) - cx;
	}

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		QPolygon polyg(3);
	//	h -= 4;
		int d = (h >> 2);
		painter.setPen(Qt::darkCyan);
		painter.setBrush(Qt::cyan);
		x = pSession->pixelFromFrame(pSession->loopStart()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			painter.drawPolygon(polyg);
		}
		x = pSession->pixelFromFrame(pSession->loopEnd()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x, h - d,
				x, h,
				x - d, h - d);
			painter.drawPolygon(polyg);
		}
	}

	// Draw punch in/out boundaries, if applicable...
	if (pSession->isPunching()) {
		QPolygon polyg(3);
	//	h -= 4;
		int d = (h >> 2);
		painter.setPen(Qt::darkMagenta);
		painter.setBrush(Qt::magenta);
		x = pSession->pixelFromFrame(pSession->punchIn()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			painter.drawPolygon(polyg);
		}
		x = pSession->pixelFromFrame(pSession->punchOut()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x, h - d,
				x, h,
				x - d, h - d);
			painter.drawPolygon(polyg);
		}
	}
}


// Rectangular contents update.
void qtractorTrackTime::updateContents ( const QRect& rect )
{
	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorTrackTime::updateContents (void)
{
	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();
}


// Resize event handler.
void qtractorTrackTime::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorScrollView::resizeEvent(pResizeEvent);
	updateContents();
}


// Draw the time scale.
void qtractorTrackTime::drawContents ( QPainter *pPainter, const QRect& rect )
{
	// Render the famous pixmap region...
	pPainter->drawPixmap(rect, m_pixmap, rect);

	// Headers a-head...
	int cx = qtractorScrollView::contentsX();
	int h = qtractorScrollView::height() - 4;
	int d = (h >> 2);
	int x;
	
	// Draw edit-head line...
	x = (m_pTracks->trackView())->editHeadX() - cx;
	if (x >= rect.left() - d && x <= rect.right() + d) {
		QPolygon polyg(3);
		polyg.putPoints(0, 3,
			x + d, h - d,
			x, h,
			x, h - d);
		pPainter->setPen(Qt::blue);
		pPainter->setBrush(Qt::blue);
		pPainter->drawPolygon(polyg);
	}

	// Draw edit-tail line...
	x = (m_pTracks->trackView())->editTailX() - cx;
	if (x >= rect.left() - d && x <= rect.right() + d) {
		QPolygon polyg(3);
		polyg.putPoints(0, 3,
			x, h - d,
			x, h,
			x - d, h - d);
		pPainter->setPen(Qt::blue);
		pPainter->setBrush(Qt::blue);
		pPainter->drawPolygon(polyg);
	}

	// Draw special play-head header...
	x = (m_pTracks->trackView())->playHeadX() - cx;
	if (x >= rect.left() - d && x <= rect.right() + d) {
		QPolygon polyg(3);
		polyg.putPoints(0, 3,
			x - d, h - d,
			x, h,
			x + d, h - d);
		pPainter->setPen(Qt::red);
		pPainter->setBrush(Qt::red);
		pPainter->drawPolygon(polyg);
	}
}


// To have timeline in h-sync with main track view.
void qtractorTrackTime::contentsXMovingSlot ( int cx, int /*cy*/ )
{
	if (qtractorScrollView::contentsX() != cx)
		qtractorScrollView::setContentsPos(cx, qtractorScrollView::contentsY());
}


// Check if some position header is to be dragged...
bool qtractorTrackTime::dragHeadStart ( const QPoint& pos )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Try to catch mouse clicks over the
	// play/edit-head/tail cursors...
	int h = qtractorScrollView::height(); // - 4;
	int d = (h >> 1);
	QRect rect(0, h - d, d << 1, d);

	// Check play-head header...
	rect.moveLeft(m_pTracks->trackView()->playHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragPlayHead;
		qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		return true;
	}

	// Check loop-point headers...
	if (pSession->isLooping()) {
		// Check loop-start header...
		rect.moveLeft(pSession->pixelFromFrame(pSession->loopStart()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragLoopStart;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			return true;
		}
		// Check loop-end header...
		rect.moveLeft(pSession->pixelFromFrame(pSession->loopEnd()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragLoopEnd;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			return true;
		}
	}

	// Check punch-point headers...
	if (pSession->isPunching()) {
		// Check punch-in header...
		rect.moveLeft(pSession->pixelFromFrame(pSession->punchIn()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragPunchIn;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			return true;
		}
		// Check punch-out header...
		rect.moveLeft(pSession->pixelFromFrame(pSession->punchOut()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragPunchOut;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			return true;
		}
	}

	// Check edit-head header...
	rect.moveLeft(m_pTracks->trackView()->editHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragEditHead;
		qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		return true;
	}

	// Check edit-tail header...
	rect.moveLeft(m_pTracks->trackView()->editTailX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragEditTail;
		qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		return true;
	}

	// Reset cursor if any persist around.
	if (m_dragCursor != DragNone) {
		qtractorScrollView::unsetCursor();
		m_dragCursor  = DragNone;
	}

	// Nothing.
	return false;
}


// Handle selection/dragging -- mouse button press.
void qtractorTrackTime::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	m_dragState = DragNone;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Direct snap positioning...
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		switch (pMouseEvent->button()) {
		case Qt::LeftButton:
			// Remember what and where we'll be dragging/selecting...
			m_dragState = DragStart;
			m_posDrag   = pos;
			// Try to catch mouse clicks over the cursor heads...
			if (dragHeadStart(m_posDrag))
				m_dragState = m_dragCursor;
			break;
		case Qt::MidButton:
			// Mid-button direct positioning...
			m_pTracks->trackView()->selectAll(false);
			// Edit-tail positioning...
			m_pTracks->trackView()->setEditHead(iFrame);
			m_pTracks->trackView()->setEditTail(iFrame);
			// Logical contents changed, just for visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case Qt::RightButton:
			// Right-button direct positioning...
			m_pTracks->trackView()->setEditTail(iFrame);
			// Logical contents changed, just for visual feedback...
			m_pTracks->selectionChangeNotify();
			// Fall thru...
		default:
			break;
		}
	}

	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse pointer move.
void qtractorTrackTime::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		// Are we already moving/dragging something?
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		int y = m_pTracks->trackView()->contentsY();
		switch (m_dragState) {
		case DragNone:
			// Try to catch mouse over the cursor heads...
			dragHeadStart(pos);
			break;
		case DragSelect:
			// Rubber-band selection...
			m_rectDrag.setRight(pos.x());
			m_pTracks->trackView()->ensureVisible(pos.x(), y, 16, 0);
			m_pTracks->trackView()->selectRect(m_rectDrag,
				qtractorTrackView::SelectRange,
				qtractorTrackView::EditBoth);
			break;
		case DragPlayHead:
			// Play-head positioning...
			m_pTracks->trackView()->ensureVisible(pos.x(), y, 16, 0);
			m_pTracks->trackView()->setPlayHead(iFrame);
			// Let the change get some immediate visual feedback...
			if (pMainForm)
				pMainForm->updateTransportTime(iFrame);
			break;
		case DragLoopStart:
		case DragPunchIn:
		case DragEditHead:
			// Edit-head positioning...
			m_pTracks->trackView()->ensureVisible(pos.x(), y, 16, 0);
			m_pTracks->trackView()->setEditHead(iFrame);
			break;
		case DragLoopEnd:
		case DragPunchOut:
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
				int h = qtractorScrollView::height();	// - 4;
				m_rectDrag.setTop(0);			// h - (h >> 2)
				m_rectDrag.setLeft(m_posDrag.x());
				m_rectDrag.setRight(pos.x());
				m_rectDrag.setBottom(h);
				m_dragState = m_dragCursor = DragSelect;
				qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			}
			// Fall thru...
		default:
			break;
		}
	}

	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse button release.
void qtractorTrackTime::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Which mouse state?
		const bool bModifier = (pMouseEvent->modifiers()
			& (Qt::ShiftModifier | Qt::ControlModifier));
		// Direct snap positioning...
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		switch (m_dragState) {
		case DragSelect:
			// Do the final range selection...
			m_pTracks->trackView()->selectRect(m_rectDrag,
				qtractorTrackView::SelectRange,
				qtractorTrackView::EditBoth);
			// For immediate visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragPlayHead:
			// Play-head positioning commit...
			m_pTracks->trackView()->setPlayHead(iFrame);
			pSession->setPlayHead(m_pTracks->trackView()->playHead());
			// Not quite a selection, rather just
			// for immediate visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragLoopStart:
			// New loop-start boundary...
			if (pSession->editHead() < pSession->loopEnd()) {
				// Yep, new loop-start point...
				pSession->execute(
					new qtractorSessionLoopCommand(pSession,
						pSession->editHead(), pSession->loopEnd()));
			}
			break;
		case DragPunchIn:
			// New punch-in boundary...
			if (pSession->editHead() < pSession->punchOut()) {
				// Yep, new punch-in point...
				pSession->setPunch(
					pSession->editHead(), pSession->punchOut());
				// For visual feedback...
				m_pTracks->contentsChangeNotify();
			}
			break;
		case DragEditHead:
			// Not quite a contents change, but for visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragLoopEnd:
			// New loop-end boundary...
			if (pSession->loopStart() < pSession->editTail()) {
				// Yep, new loop-end point...
				pSession->execute(
					new qtractorSessionLoopCommand(pSession,
						pSession->loopStart(), pSession->editTail()));
			}
			break;
		case DragPunchOut:
			// New punch-out boundary...
			if (pSession->punchIn() < pSession->editTail()) {
				// Yep, new punch-out point...
				pSession->setPunch(
					pSession->punchIn(), pSession->editTail());
				// For visual feedback...
				m_pTracks->contentsChangeNotify();
			}
			break;
		case DragEditTail:
			// Not quite a contents change, but for visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragStart:
			// Left-button indirect positioning...
			if (bModifier) {
				// Playhead positioning...
				m_pTracks->trackView()->setPlayHead(iFrame);
				// Immediately commited...
				pSession->setPlayHead(iFrame);
			} else {
				// Deferred left-button edit-head positioning...
				m_pTracks->trackView()->setEditHead(iFrame);
			}
			// Not quite a selection, rather just
			// for immediate visual feedback...
			m_pTracks->selectionChangeNotify();
			// Fall thru...
		case DragNone:
		default:
			break;
		}
	}

	// Clean up.
	resetDragState();
}


// Tempo-map dialog accessor.
void qtractorTrackTime::mouseDoubleClickEvent ( QMouseEvent *pMouseEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	unsigned long iFrame = pSession->frameSnap(
		pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));

	// Show tempo map dialog.
	qtractorTimeScaleForm form(pMainForm);
	form.setFrame(iFrame);
	form.exec();
}


// Keyboard event handler.
void qtractorTrackTime::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorTrackTime::keyPressEvent(key=%d)\n", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
	{
		// Restore uncommitted play-head position?...
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession && m_dragState == DragPlayHead)
			m_pTracks->trackView()->setPlayHead(pSession->playHead());
		resetDragState();
		break;
	}
	default:
		qtractorScrollView::keyPressEvent(pKeyEvent);
		break;
	}
}


// Reset drag/select state.
void qtractorTrackTime::resetDragState (void)
{
	// Cancel any dragging/cursor out there...
	if (m_dragState == DragSelect)
		qtractorScrollView::updateContents();

	if (m_dragCursor != DragNone)
		qtractorScrollView::unsetCursor();

	// Force null state.
	m_dragState  = DragNone;
	m_dragCursor = DragNone;
	
	// HACK: give focus to track-view... 
	m_pTracks->trackView()->setFocus();
}


// Context menu event handler (dummy).
void qtractorTrackTime::contextMenuEvent (
	QContextMenuEvent */*pContextMenuEvent*/ )
{
}


// end of qtractorTrackTime.cpp
