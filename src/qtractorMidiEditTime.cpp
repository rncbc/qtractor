// qtractorMidiEditTime.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSessionCommand.h"

#include "qtractorMainForm.h"

#include "qtractorTimeScaleForm.h"

#include <QApplication>

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

	//
	// Draw the time scale...
	//

	const QFontMetrics& fm = p.fontMetrics();
	int x, y1, y2 = h - 1;

	// Account for the editing offset:
	int dx = cx + pTimeScale->pixelFromFrame(m_pEditor->offset());

	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekPixel(dx);

	unsigned short iPixelsPerBeat = pNode->pixelsPerBeat();
	unsigned int iBeat = pNode->beatFromPixel(dx);
	int x0 = x = pNode->pixelFromBeat(iBeat) - dx;
	while (x < w) {
		bool bBeatIsBar = pNode->beatIsBar(iBeat) && (x >= x0);
		if (bBeatIsBar) {
			y1 = 0;
			p.setPen(pal.windowText().color());
			p.drawText(x + 2, y1 + fm.ascent(),
				QString::number(pNode->barFromBeat(iBeat) + 1));
			x0 = x + 16;
			if (iBeat == pNode->beat) {
				iPixelsPerBeat = pNode->pixelsPerBeat();
				p.setPen(pal.base().color().value() < 0x7f
					? pal.light().color() : pal.dark().color()); 
				p.drawText(x0, y1 + fm.ascent(),
					QString("%1 %2/%3")
					.arg(pNode->tempo, 0, 'g', 3)
					.arg(pNode->beatsPerBar)
					.arg(1 << pNode->beatDivisor));
			}
		} else {
			y1 = (y2 >> 1);
		}
		if (bBeatIsBar || iPixelsPerBeat > 8) {
			p.setPen(pal.mid().color());
			p.drawLine(x, y1, x, y2);
			p.setPen(pal.light().color());
			p.drawLine(x + 1, y1, x + 1, y2);
		}
		pNode = cursor.seekBeat(++iBeat);
		x = pNode->pixelFromBeat(iBeat) - dx;
	}

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
	
	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		QPolygon polyg(3);
	//	h -= 4;
		int d = (h >> 2) + 1;
		p.setPen(Qt::darkCyan);
		p.setBrush(Qt::cyan);
		x = pTimeScale->pixelFromFrame(pSession->loopStart()) - dx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			p.drawPolygon(polyg);
		}
		x = pTimeScale->pixelFromFrame(pSession->loopEnd()) - dx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x, h - d,
				x, h,
				x - d, h - d);
			p.drawPolygon(polyg);
		}
	}

	// Draw punch in/out boundaries, if applicable...
	if (pSession->isPunching()) {
		QPolygon polyg(3);
	//	h -= 4;
		int d = (h >> 2) + 1;
		p.setPen(Qt::darkMagenta);
		p.setBrush(Qt::magenta);
		x = pTimeScale->pixelFromFrame(pSession->punchIn()) - dx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			p.drawPolygon(polyg);
		}
		x = pTimeScale->pixelFromFrame(pSession->punchOut()) - dx;
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


// Draw the time scale.
void qtractorMidiEditTime::drawContents ( QPainter *pPainter, const QRect& rect )
{
	pPainter->drawPixmap(rect, m_pixmap, rect);

	// Draw special play/edit-head/tail headers...
	int cx = qtractorScrollView::contentsX();
	int h = qtractorScrollView::height() - 4;
	int d = (h >> 2);
	int x = m_pEditor->editHeadX() - cx;
	if (x >= rect.left() - d && x <= rect.right() + d) {
		QPolygon polyg(3);
		polyg.putPoints(0, 3,
			x, h - d,
			x, h,
			x + d, h - d);
		pPainter->setPen(Qt::blue);
		pPainter->setBrush(Qt::blue);
		pPainter->drawPolygon(polyg);
	}

	x = m_pEditor->editTailX() - cx;
	if (x >= rect.left() - d && x <= rect.right() + d) {
		QPolygon polyg(3);
		polyg.putPoints(0, 3,
			x - d, h - d,
			x, h,
			x, h - d);
		pPainter->setPen(Qt::blue);
		pPainter->setBrush(Qt::blue);
		pPainter->drawPolygon(polyg);
	}

	x = m_pEditor->playHeadX() - cx;
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


// To have timeline in h-sync with main view.
void qtractorMidiEditTime::contentsXMovingSlot ( int cx, int /*cy*/ )
{
	if (qtractorScrollView::contentsX() != cx)
		qtractorScrollView::setContentsPos(cx, qtractorScrollView::contentsY());
}


// Check if some position header is to be dragged...
bool qtractorMidiEditTime::dragHeadStart ( const QPoint& pos )
{
	// Try to catch mouse clicks over the
	// play/edit-head/tail cursors...
	int h = qtractorScrollView::height(); // - 4;
	int d = (h >> 1);
	QRect rect(0, h - d, d << 1, d);

	// Check play-head header...
	rect.moveLeft(m_pEditor->playHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragPlayHead;
		qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		return true;
	}

	// Check loop and punch points...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Loop points...
		if (pSession->isLooping()) {
			qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
			int dx = d + pTimeScale->pixelFromFrame(m_pEditor->offset());
			// Check loop-start header...
			rect.moveLeft(pTimeScale->pixelFromFrame(pSession->loopStart()) - dx);
			if (rect.contains(pos)) {
				m_dragCursor = DragLoopStart;
				qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
				return true;
			}
			// Check loop-end header...
			rect.moveLeft(pTimeScale->pixelFromFrame(pSession->loopEnd()) - dx);
			if (rect.contains(pos)) {
				m_dragCursor = DragLoopEnd;
				qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
				return true;
			}
		}
		// Punch in/out points...
		if (pSession->isPunching()) {
			qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
			int dx = d + pTimeScale->pixelFromFrame(m_pEditor->offset());
			// Check punch-in header...
			rect.moveLeft(pTimeScale->pixelFromFrame(pSession->punchIn()) - dx);
			if (rect.contains(pos)) {
				m_dragCursor = DragPunchIn;
				qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
				return true;
			}
			// Check punch-out header...
			rect.moveLeft(pTimeScale->pixelFromFrame(pSession->punchOut()) - dx);
			if (rect.contains(pos)) {
				m_dragCursor = DragPunchOut;
				qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
				return true;
			}
		}
	}

	// Check edit-head header...
	rect.moveLeft(m_pEditor->editHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragEditHead;
		qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		return true;
	}

	// Check edit-tail header...
	rect.moveLeft(m_pEditor->editTailX() - d);
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


// Handle item selection/dragging -- mouse button press.
void qtractorMidiEditTime::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	m_dragState = DragNone;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Which mouse state?
	const bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));
	// Make sure we'll reset selection...
	if (!bModifier)
		m_pEditor->selectAll(false);

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
	switch (pMouseEvent->button()) {
	case Qt::LeftButton:
		// Remember what and where we'll be dragging/selecting...
		m_dragState = DragStart;
		m_posDrag   = pos;
		// Try to catch mouse clicks over the cursor heads...
		if (dragHeadStart(m_posDrag)) {
			m_dragState = m_dragCursor;
		} else if (!bModifier) {
			// Edit-head positioning...
			m_pEditor->setEditHead(iFrame);
			// Logical contents changed, just for visual feedback...
			m_pEditor->selectionChangeNotify();
		}
		break;
	case Qt::MidButton:
		// Edit-head/tail positioning...
		m_pEditor->setEditHead(iFrame);
		m_pEditor->setEditTail(iFrame);
		// Logical contents changed, just for visual feedback...
		m_pEditor->selectionChangeNotify();
		break;
	case Qt::RightButton:
		// Edit-tail positioning...
		m_pEditor->setEditTail(iFrame);
		// Logical contents changed, just for visual feedback...
		m_pEditor->selectionChangeNotify();
		// Fall thru...
	default:
		break;
	}

	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditTime::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	// Are we already moving/dragging something?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
	int y = m_pEditor->editView()->contentsY();
	switch (m_dragState) {
	case DragNone:
		// Try to catch mouse over the cursor heads...
		dragHeadStart(pos);
		break;
	case DragSelect:
		// Rubber-band selection...
		m_rectDrag.setRight(pos.x());
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->selectRect(m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, false);
		// Edit-tail positioning...
		m_pEditor->setEditTail(iFrame);
		break;
	case DragPlayHead:
		// Play-head positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->setPlayHead(iFrame);
		// Let the change get some immediate visual feedback...
		pMainForm->updateTransportTime(iFrame);
		break;
	case DragEditHead:
	case DragPunchIn:
	case DragLoopStart:
		// Edit-head positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->setEditHead(iFrame);
		break;
	case DragEditTail:
	case DragPunchOut:
	case DragLoopEnd:
		// Edit-head positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->setEditTail(iFrame);
		break;
	case DragStart:
		// Rubber-band starting...
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// We'll start dragging alright...
			int h = m_pEditor->editView()->contentsHeight();
			m_rectDrag.setTop(0);
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

	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditTime::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Which mouse state?
	const bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
	switch (m_dragState) {
	case DragSelect:
		// Do the final range selection...
		m_pEditor->selectRect(m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, true);
		// Edit-tail positioning...
		m_pEditor->setEditTail(iFrame);
		// Not quite a selection change,
		// but for visual feedback...
		m_pEditor->selectionChangeNotify();
		break;
	case DragPlayHead:
		// Play-head positioning commit...
		m_pEditor->setPlayHead(iFrame);
		pSession->setPlayHead(m_pEditor->playHead());
		// Fall thru...
	case DragEditHead:
	case DragEditTail:
		// Not quite a selection change,
		// but for visual feedback...
		m_pEditor->selectionChangeNotify();
		break;
	case DragLoopStart:
		// New loop-start boundary...
		if (pSession->editHead() < pSession->loopEnd()) {
			// Yep, new loop-start point...
			m_pEditor->commands()->exec(
				new qtractorSessionLoopCommand(pSession,
					pSession->editHead(), pSession->loopEnd()));
		}
		break;
	case DragLoopEnd:
		// New loop-end boundary...
		if (pSession->loopStart() < pSession->editTail()) {
			// Yep, new loop-end point...
			m_pEditor->commands()->exec(
				new qtractorSessionLoopCommand(pSession,
					pSession->loopStart(), pSession->editTail()));
		}
		break;
	case DragPunchIn:
		// New punch-in boundary...
		if (pSession->editHead() < pSession->punchOut()) {
			// Yep, new punch-in point...
			m_pEditor->commands()->exec(
				new qtractorSessionPunchCommand(pSession,
					pSession->editHead(), pSession->punchOut()));
		}
		break;
	case DragPunchOut:
		// New punch-out boundary...
		if (pSession->punchIn() < pSession->editTail()) {
			// Yep, new punch-out point...
			m_pEditor->commands()->exec(
				new qtractorSessionPunchCommand(pSession,
					pSession->punchIn(), pSession->editTail()));
		}
		break;
	case DragStart:
		// Left-button indirect positioning...
		if (bModifier) {
			// Play-head positioning...
			m_pEditor->setPlayHead(iFrame);
			// Immediately commited...
			pSession->setPlayHead(iFrame);
			// Not quite a selection, rather just
			// for immediate visual feedback...
			m_pEditor->selectionChangeNotify();
		}
		// Fall thru...
	case DragNone:
	default:
		break;
	}

	// Clean up.
	resetDragState();
}


// Tempo-map dialog accessor.
void qtractorMidiEditTime::mouseDoubleClickEvent ( QMouseEvent *pMouseEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));

	// Show tempo map dialog.
	qtractorTimeScaleForm form(pMainForm);
	form.setFrame(iFrame);
	form.exec();
}


// Keyboard event handler.
void qtractorMidiEditTime::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (pKeyEvent->key() == Qt::Key_Escape) {
		// Restore uncommitted play-head position?...
		if (m_dragState == DragPlayHead) {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession)
				m_pEditor->setPlayHead(pSession->playHead());
		}
		resetDragState();
	}

	if (!m_pEditor->keyPress(this, pKeyEvent->key(), pKeyEvent->modifiers()))
		qtractorScrollView::keyPressEvent(pKeyEvent);
}


// Reset drag/select state.
void qtractorMidiEditTime::resetDragState (void)
{
	// Cancel any dragging/cursor out there...
	if (m_dragState == DragSelect)
		qtractorScrollView::updateContents();

	if (m_dragCursor != DragNone)
		qtractorScrollView::unsetCursor();

	// Also get rid of any meta-breadcrumbs...
	m_pEditor->resetDragState(this);

	// Force null state.
	m_dragState  = DragNone;
	m_dragCursor = DragNone;

	// HACK: give focus to track-view... 
	m_pEditor->editView()->setFocus();
}


// Context menu event handler (dummy).
void qtractorMidiEditTime::contextMenuEvent (
	QContextMenuEvent */*pContextMenuEvent*/ )
{
}


// end of qtractorMidiEditTime.cpp
