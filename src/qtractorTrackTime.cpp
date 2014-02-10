// qtractorTrackTime.cpp
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

#include "qtractorAbout.h"
#include "qtractorTrackTime.h"
#include "qtractorTrackView.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"

#include "qtractorOptions.h"

#include "qtractorSessionCommand.h"
#include "qtractorTimeScaleCommand.h"

#include "qtractorMainForm.h"

#include "qtractorTimeScaleForm.h"

#include <QApplication>
#include <QPainter>
#include <QCursor>
#include <QPixmap>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>

#include <QToolTip>


//----------------------------------------------------------------------------
// qtractorTrackTime -- Track time scale widget.

// Constructor.
qtractorTrackTime::qtractorTrackTime ( qtractorTracks *pTracks,
	QWidget *pParent ) : qtractorScrollView(pParent)
{
	m_pTracks = pTracks;

	m_dragState  = DragNone;
	m_dragCursor = DragNone;

	m_pDragMarker = NULL;

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
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 2));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));

	// Trap for help/tool-tips events.
	qtractorScrollView::viewport()->installEventFilter(this);
}


// (Re)create the complete track view pixmap.
void qtractorTrackTime::updatePixmap ( int cx, int /* cy */)
{
	QWidget *pViewport = qtractorScrollView::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.window().color());

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return;
	
	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	// Draw the time scale...
	//
	const QFontMetrics& fm = painter.fontMetrics();
	int x, x1, y1, y2 = h - 1;

	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekPixel(cx);

	unsigned short iPixelsPerBeat = pNode->pixelsPerBeat();
	unsigned int iBeat = pNode->beatFromPixel(cx);
	if (iBeat > 0) pNode = cursor.seekBeat(--iBeat);
	x = x1 = pNode->pixelFromBeat(iBeat) - cx;

	while (x < w) {
		const bool bBeatIsBar = pNode->beatIsBar(iBeat);
		if (bBeatIsBar || iPixelsPerBeat > 16) {
			y1 = (bBeatIsBar && x >= x1 ? 0 : fm.ascent());
			painter.setPen(pal.mid().color());
			painter.drawLine(x, y1, x, y2);
			painter.setPen(pal.light().color());
			++x; painter.drawLine(x, y1, x, y2);
		}
		if (bBeatIsBar) {
			y1 = fm.ascent();
			if (x >= x1) {
				x1 = x + 2;
				const unsigned short iBar = pNode->barFromBeat(iBeat);
				const QString& sBeat = QString::number(iBar + 1);
				painter.setPen(pal.windowText().color());
				painter.drawText(x1, y1, sBeat);
				x1 += fm.width(sBeat) + 2;
			}
			x1 += 2;
			if (iBeat == pNode->beat) {
				iPixelsPerBeat = pNode->pixelsPerBeat();
				const QString& sTempo = QString("%1 %2/%3")
					.arg(pNode->tempo, 0, 'g', 3)
					.arg(pNode->beatsPerBar)
					.arg(1 << pNode->beatDivisor);
				painter.setPen(pal.base().color().value() < 0x7f
					? pal.light().color() : pal.dark().color());
				painter.drawText(x1, y1, sTempo);
				x1 += fm.width(sTempo) + 2;
			}
		}
		pNode = cursor.seekBeat(++iBeat);
		x = pNode->pixelFromBeat(iBeat) - cx;
	}

	// Draw location markers, if any...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekPixel(cx);
	while (pMarker) {
		x = pTimeScale->pixelFromFrame(pMarker->frame) - cx + 4;
		if (x > w) break;
		painter.setPen(pMarker->color);
		painter.drawText(x, y2, pMarker->text);
		pMarker = pMarker->next();
	}

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		QPolygon polyg(3);
	//	h -= 4;
		const int d = (h >> 2);
		painter.setPen(Qt::darkCyan);
		painter.setBrush(Qt::cyan);
		x = pTimeScale->pixelFromFrame(pSession->loopStart()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			painter.drawPolygon(polyg);
		}
		x = pTimeScale->pixelFromFrame(pSession->loopEnd()) - cx;
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
		const int d = (h >> 2);
		painter.setPen(Qt::darkMagenta);
		painter.setBrush(Qt::magenta);
		x = pTimeScale->pixelFromFrame(pSession->punchIn()) - cx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			painter.drawPolygon(polyg);
		}
		x = pTimeScale->pixelFromFrame(pSession->punchOut()) - cx;
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
	const int cx = qtractorScrollView::contentsX();
	const int h = qtractorScrollView::height() - 4;
	const int d = (h >> 2);

	qtractorTrackView *pTrackView = m_pTracks->trackView();

	// Draw edit-head line...
	int x = pTrackView->editHeadX() - cx;
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
	x = pTrackView->editTailX() - cx;
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
	x = pTrackView->playHeadX() - cx;
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

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return false;

	// Try to catch mouse clicks over the
	// play/edit-head/tail cursors...
	const int h = qtractorScrollView::height(); // - 4;
	const int d = (h >> 1);
	QRect rect(0, h - d, d << 1, d);

	qtractorTrackView *pTrackView = m_pTracks->trackView();

	// Check play-head header...
	rect.moveLeft(pTrackView->playHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragPlayHead;
		return true;
	}

	// Check loop-point headers...
	if (pSession->isLooping()) {
		// Check loop-start header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->loopStart()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragLoopStart;
			return true;
		}
		// Check loop-end header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->loopEnd()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragLoopEnd;
			return true;
		}
	}

	// Check punch-point headers...
	if (pSession->isPunching()) {
		// Check punch-in header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->punchIn()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragPunchIn;
			return true;
		}
		// Check punch-out header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->punchOut()) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragPunchOut;
			return true;
		}
	}

	// Check location marker headers...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekPixel(pos.x());
	if (pMarker) {
		rect.moveLeft(pTimeScale->pixelFromFrame(pMarker->frame) - d);
		if (rect.contains(pos)) {
			m_dragCursor = DragMarker;
			m_pDragMarker = pMarker;
			return true;
		}
	}

	// Check edit-head header...
	rect.moveLeft(pTrackView->editHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragEditHead;
		return true;
	}

	// Check edit-tail header...
	rect.moveLeft(pTrackView->editTailX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragEditTail;
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

	qtractorTrackView *pTrackView = m_pTracks->trackView();

	// We'll need options somehow...
	qtractorOptions *pOptions = qtractorOptions::getInstance();

	// We need a session and a location...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Direct snap positioning...
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		// Which mouse state?
		const Qt::KeyboardModifiers& modifiers = pMouseEvent->modifiers();
		bool bModifier = (modifiers & (Qt::ShiftModifier | Qt::ControlModifier));
		switch (pMouseEvent->button()) {
		case Qt::LeftButton:
			// Remember what and where we'll be dragging/selecting...
			m_dragState = DragStart;
			m_posDrag   = pos;
			// Try to catch mouse clicks over the cursor heads...
			if (dragHeadStart(m_posDrag)) {
				qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
				m_dragState = m_dragCursor;
			}
			break;
		case Qt::MidButton:
			// Mid-button direct positioning...
			m_pTracks->selectNone();
			if (pOptions && pOptions->bMidButtonModifier)
				bModifier = !bModifier;	// Reverse mid-button role...
			if (bModifier) {
				// Play-head positioning commit...
				pTrackView->setPlayHead(iFrame);
				pSession->setPlayHead(pTrackView->playHead());
			} else {
				// Edit cursor (merge) positioning...
				pTrackView->setEditHead(iFrame);
				pTrackView->setEditTail(iFrame);
			}
			// Logical contents changed, just for visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case Qt::RightButton:
			// Right-button direct positioning...
			pTrackView->setEditTail(iFrame);
			// Logical contents changed, just for visual feedback...
			m_pTracks->selectionChangeNotify();
			// Fall thru...
		default:
			break;
		}
	}

//	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse pointer move.
void qtractorTrackTime::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorTrackView *pTrackView = m_pTracks->trackView();
		// Which mouse state?
		const Qt::KeyboardModifiers& modifiers
			= pMouseEvent->modifiers();
		// Are we already moving/dragging something?
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		const unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		const int y = pTrackView->contentsY();
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		switch (m_dragState) {
		case DragNone:
			// Try to catch mouse over the cursor heads...
			if (dragHeadStart(pos))
				qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
			break;
		case DragSelect:
			// Rubber-band selection...
			m_rectDrag.setRight(pos.x());
			pTrackView->ensureVisible(pos.x(), y, 16, 0);
			if (pTrackView->isCurveEdit()) {
				// Select all current track curve/automation
				// nodes that fall inside range...
				pTrackView->selectCurveRect(m_rectDrag,
					qtractorTrackView::SelectRange,
					qtractorTrackView::selectFlags(modifiers),
					qtractorTrackView::EditBoth);
			} else {
				// Here we're mainly supposed to select a few
				// bunch of clips that fall inside range...
				pTrackView->selectClipRect(m_rectDrag,
					qtractorTrackView::SelectRange,
					qtractorTrackView::selectFlags(modifiers),
					qtractorTrackView::EditBoth);
			}
			showToolTip(m_rectDrag.normalized());
			break;
		case DragPlayHead:
			// Play-head positioning...
			pTrackView->ensureVisible(pos.x(), y, 16, 0);
			pTrackView->setPlayHead(iFrame);
			// Let the change get some immediate visual feedback...
			if (pMainForm)
				pMainForm->updateTransportTime(iFrame);
			showToolTip(iFrame);
			break;
		case DragLoopStart:
		case DragPunchIn:
		case DragEditHead:
			// Edit-head positioning...
			pTrackView->ensureVisible(pos.x(), y, 16, 0);
			pTrackView->setEditHead(iFrame);
			showToolTip(iFrame);
			break;
		case DragLoopEnd:
		case DragPunchOut:
		case DragEditTail:
			// Edit-tail positioning...
			pTrackView->ensureVisible(pos.x(), y, 16, 0);
			pTrackView->setEditTail(iFrame);
			showToolTip(iFrame);
			break;
		case DragMarker:
			// Marker positioning...
			pTrackView->ensureVisible(pos.x(), y, 16, 0);
			showToolTip(iFrame);
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

//	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle selection/dragging -- mouse button release.
void qtractorTrackTime::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
//	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorTrackView *pTrackView = m_pTracks->trackView();
		// Which mouse state?
		const Qt::KeyboardModifiers& modifiers
			= pMouseEvent->modifiers();
		const bool bModifier
			= (modifiers & (Qt::ShiftModifier | Qt::ControlModifier));
		// Direct snap positioning...
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		const unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		switch (m_dragState) {
		case DragSelect:
			// Do the final range selection...
			if (pTrackView->isCurveEdit()) {
				// Select all current track curve/automation
				// nodes that fall inside range...
				pTrackView->selectCurveRect(m_rectDrag,
					qtractorTrackView::SelectRange,
					qtractorTrackView::selectFlags(modifiers)
						| qtractorTrackView::SelectCommit,
					qtractorTrackView::EditBoth);
			} else {
				// Here we're mainly supposed to select a few
				// bunch of clips that fall inside range...
				pTrackView->selectClipRect(m_rectDrag,
					qtractorTrackView::SelectRange,
					qtractorTrackView::selectFlags(modifiers)
						| qtractorTrackView::SelectCommit,
					qtractorTrackView::EditBoth);
			}
			// For immediate visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragPlayHead:
			// Play-head positioning commit...
			pTrackView->setPlayHead(iFrame);
			pSession->setPlayHead(pTrackView->playHead());
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
		case DragMarker:
			// Marker positioning commit...
			if (m_pDragMarker) {
				// Yep, new marker location...
				pSession->execute(
					new qtractorTimeScaleMoveMarkerCommand(
						pSession->timeScale(), m_pDragMarker, iFrame));
			}
			break;
		case DragStart:
			// Left-button indirect positioning...
			if (bModifier) {
				// Playhead positioning...
				pTrackView->setPlayHead(iFrame);
				// Immediately commited...
				pSession->setPlayHead(iFrame);
			} else {
				// Deferred left-button edit-head positioning...
				pTrackView->setEditHead(iFrame);
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
	const unsigned long iFrame = pSession->frameSnap(
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


// Handle zoom with mouse wheel.
void qtractorTrackTime::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers() & Qt::ControlModifier) {
		const int delta = pWheelEvent->delta();
		if (delta > 0)
			m_pTracks->zoomIn();
		else
			m_pTracks->zoomOut();
	}
//	else qtractorScrollView::wheelEvent(pWheelEvent);
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

	m_pDragMarker = NULL;

	// HACK: give focus to track-view... 
	m_pTracks->trackView()->setFocus();
}


// Context menu event handler (dummy).
void qtractorTrackTime::contextMenuEvent (
	QContextMenuEvent */*pContextMenuEvent*/ )
{
}


// Trap for help/tool-tip events.
bool qtractorTrackTime::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	if (static_cast<QWidget *> (pObject) == pViewport) {
		if (pEvent->type() == QEvent::ToolTip
			&& m_dragCursor != DragNone
			&& (m_pTracks->trackView())->isToolTips()) {
			QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
			if (pHelpEvent) {
				qtractorSession *pSession = qtractorSession::getInstance();
				if (pSession) {
					unsigned long iFrame = 0;
					switch (m_dragCursor) {
					case DragMarker:
						if (m_pDragMarker) iFrame = m_pDragMarker->frame;
						break;
					case DragPlayHead:
						iFrame = pSession->playHead();
						break;
					case DragEditHead:
						iFrame = pSession->editHead();
						break;
					case DragEditTail:
						iFrame = pSession->editTail();
						break;
					case DragLoopStart:
						iFrame = pSession->loopStart();
						break;
					case DragLoopEnd:
						iFrame = pSession->loopEnd();
						break;
					case DragPunchIn:
						iFrame = pSession->punchIn();
						break;
					case DragPunchOut:
						iFrame = pSession->punchOut();
						break;
					default:
						break;
					}
					showToolTip(iFrame);
				}
			}
		}
		else
		if (pEvent->type() == QEvent::Leave
			&& m_dragState != DragNone) {
			qtractorScrollView::unsetCursor();
			return true;
		}
	}

	// Not handled here.
	return qtractorScrollView::eventFilter(pObject, pEvent);
}


// Show dragging tooltip...
void qtractorTrackTime::showToolTip ( unsigned long iFrame ) const
{
	if (!m_pTracks->trackView()->isToolTips())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return;

	QString sToolTip;

	switch (m_dragCursor) {
	case DragMarker:
		if (m_pDragMarker) sToolTip += m_pDragMarker->text;
		break;
	case DragPlayHead:
		sToolTip += tr("Play-head");
		break;
	case DragEditHead:
		sToolTip += tr("Edit-head");
		break;
	case DragEditTail:
		sToolTip += tr("Edit-tail");
		break;
	case DragLoopStart:
		sToolTip += tr("Loop-start");
		break;
	case DragLoopEnd:
		sToolTip += tr("Loop-end");
		break;
	case DragPunchIn:
		sToolTip += tr("Punch-in");
		break;
	case DragPunchOut:
		sToolTip += tr("Punch-out");
		break;
	default:
		break;
	}

	if (!sToolTip.isEmpty()) sToolTip += '\n';

	sToolTip += pTimeScale->textFromFrame(iFrame);

	QToolTip::showText(QCursor::pos(), sToolTip,
		qtractorScrollView::viewport());
}


void qtractorTrackTime::showToolTip ( const QRect& rect ) const
{
	if (!m_pTracks->trackView()->isToolTips())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return;

	const unsigned long iFrameStart = pTimeScale->frameSnap(
		pTimeScale->frameFromPixel(rect.left()));
	const unsigned long iFrameEnd = pTimeScale->frameSnap(
		iFrameStart + pTimeScale->frameFromPixel(rect.width()));

	QToolTip::showText(QCursor::pos(),
		tr("Start:\t%1\nEnd:\t%2\nLength:\t%3")
			.arg(pTimeScale->textFromFrame(iFrameStart))
			.arg(pTimeScale->textFromFrame(iFrameEnd))
			.arg(pTimeScale->textFromFrame(iFrameStart, true, iFrameEnd - iFrameStart)),
		qtractorScrollView::viewport());
}


// end of qtractorTrackTime.cpp
