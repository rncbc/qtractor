// qtractorMidiEditTime.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorOptions.h"

#include "qtractorSessionCommand.h"
#include "qtractorTimeScaleCommand.h"

#include "qtractorMainForm.h"

#include "qtractorTimeScaleForm.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>

#include <QToolTip>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define horizontalAdvance  width
#endif


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

	m_pDragMarker = nullptr;

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


// (Re)create the complete view pixmap.
void qtractorMidiEditTime::updatePixmap ( int cx, int /*cy*/)
{
	QWidget *pViewport = qtractorScrollView::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	const QColor& rgbText  = pal.windowText().color();
	const QColor& rgbLine  = pal.mid().color();
	const QColor& rgbLight = pal.light().color();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.window().color());

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	QPainter painter(&m_pixmap);
//	painter.initFrom(this);
	painter.setFont(qtractorScrollView::font());

	//
	// Draw the time scale...
	//

	const QFontMetrics& fm = painter.fontMetrics();
	int x, x1, y1, y2 = h - 1;

	// Account for the editing offset:
	const int dx = cx + pTimeScale->pixelFromFrame(m_pEditor->offset());

	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekPixel(dx);
#if 0
	unsigned short iPixelsPerBeat = pNode->pixelsPerBeat();
	unsigned int iBeat = pNode->beatFromPixel(dx);
	if (iBeat > 0) pNode = cursor.seekBeat(--iBeat);
	x = x1 = pNode->pixelFromBeat(iBeat) - dx;

	while (x < w) {
		const bool bBeatIsBar = pNode->beatIsBar(iBeat);
		if (bBeatIsBar || iPixelsPerBeat > 8) {
			y1 = (bBeatIsBar && x >= x1 ? 0 : fm.ascent());
			painter.setPen(rgbLine);
			painter.drawLine(x, y1, x, y2);
			painter.setPen(rgbLight);
			++x; painter.drawLine(x, y1, x, y2);
		}
		if (bBeatIsBar) {
			y1 = fm.ascent();
			if (x >= x1) {
				x1 = x + 2;
				const unsigned short iBar = pNode->barFromBeat(iBeat);
				const QString& sBeat = QString::number(iBar + 1);
				painter.setPen(rgbText);
				painter.drawText(x1, y1, sBeat);
				x1 += fm.horizontalAdvance(sBeat) + 2;
			}
			x1 += 2;
			if (iBeat == pNode->beat) {
				iPixelsPerBeat = pNode->pixelsPerBeat();
				const QString& sTempo = QString("%1 %2/%3")
					.arg(pNode->tempo)
					.arg(pNode->beatsPerBar)
					.arg(1 << pNode->beatDivisor);
				painter.setPen(Qt::darkGray);
				painter.drawText(x1, y1, sTempo);
				x1 += fm.horizontalAdvance(sTempo) + 2;
			}
		}
		pNode = cursor.seekBeat(++iBeat);
		x = pNode->pixelFromBeat(iBeat) - dx;
	}
#else
	unsigned short iBar = pNode->barFromPixel(dx);
	if (iBar > 0) pNode = cursor.seekBar(--iBar);
	x = x1 = pNode->pixelFromBar(iBar) - dx;
	while (x < w) {
		// Next bar...
		pNode = cursor.seekPixel(x + dx);
		const int x2 = pNode->pixelFromBar(++iBar) - dx;
		// Bar label...
		if (x >= x1) {
			const QString& sBar	= QString::number(iBar);
			x1 = x;
			y1 = fm.ascent();
			painter.setPen(rgbText);
			painter.drawText(x1 + 2, y1, sBar);
			x1 += fm.horizontalAdvance(sBar) + 2;
		}
		x1 += 2;
		// Tempo/time-sig. label...
		if (iBar == pNode->bar + 1) {
			const QString& sTempo = QString("%1 %2/%3")
				.arg(pNode->tempo)
				.arg(pNode->beatsPerBar)
				.arg(1 << pNode->beatDivisor);
			y1 = fm.ascent();
			painter.setPen(Qt::darkGray);
			painter.drawText(x1 + 2, y1, sTempo);
			x1 += fm.horizontalAdvance(sTempo) + 2;
		}
		// Beat lines...
		const unsigned short iBeatsPerBar2 = pNode->beatsPerBar2();
		const float q2 = float(x2 - x) / float(iBeatsPerBar2);
		if (q2 > 8.0f) {
			float p2 = float(x - 1);
			for (int i = 1; i < iBeatsPerBar2; ++i) {
				x = int(p2 += q2);
				if (x > w)
					break;
				if (x > x1) {
					y1 = fm.ascent();
					painter.setPen(rgbLine);
					painter.drawLine(x, y1, x, y2);
					painter.setPen(rgbLight);
					++x; painter.drawLine(x, y1, x, y2);
				}
			}
		}
		// Bar line...
		if (x2 > x1) {
			y1 = 0;
			painter.setPen(rgbLine);
			painter.drawLine(x2 - 1, y1, x2 - 1, y2);
			painter.setPen(rgbLight);
			painter.drawLine(x2, y1, x2, y2);
		}
		// Move forward...
		x = x2;
	}
#endif

	// Draw location markers, if any...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekPixel(dx);
	while (pMarker) {
		x = pTimeScale->pixelFromFrame(pMarker->frame) - dx + 4;
		if (x > w) break;
		if (qtractorTimeScale::isKeySignature(
				pMarker->accidentals, pMarker->mode)) {
			const QString& sKeySignature
				= qtractorTimeScale::keySignatureName(
					pMarker->accidentals, pMarker->mode);
			painter.setPen(Qt::darkGray);
			painter.drawText(x, y2, sKeySignature);
			x += fm.horizontalAdvance(sKeySignature) + 4;
		}
		if (!pMarker->text.isEmpty()) {
			painter.setPen(pMarker->color);
			painter.drawText(x, y2, pMarker->text);
		}
		pMarker = pMarker->next();
	}

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		QPolygon polyg(3);
	//	h -= 4;
		const int d = (h >> 2);
		QRect rect(0, h - d, w, h - d);
		QColor color = Qt::cyan;
		painter.setPen(color.darker());
		painter.setBrush(color);
		x = pTimeScale->pixelFromFrame(pSession->loopStart()) - dx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			painter.drawPolygon(polyg);
		}
		rect.setLeft(x);
		x = pTimeScale->pixelFromFrame(pSession->loopEnd()) - dx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x, h - d,
				x, h,
				x - d, h - d);
			painter.drawPolygon(polyg);
		}
		if (rect.x() < w && x >= 0) {
			rect.setRight(x);
			color.setAlpha(120);
			painter.fillRect(rect, color);
		}
	}

	// Draw punch in/out boundaries, if applicable...
	if (pSession->isPunching()) {
		QPolygon polyg(3);
	//	h -= 4;
		const int d = (h >> 2);
		QRect rect(0, h - d, w, h - d);
		QColor color = Qt::magenta;
		painter.setPen(color.darker());
		painter.setBrush(color);
		x = pTimeScale->pixelFromFrame(pSession->punchIn()) - dx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x + d, h - d,
				x, h,
				x, h - d);
			painter.drawPolygon(polyg);
		}
		rect.setLeft(x);
		x = pTimeScale->pixelFromFrame(pSession->punchOut()) - dx;
		if (x >= 0 && x < w) {
			polyg.putPoints(0, 3,
				x, h - d,
				x, h,
				x - d, h - d);
			painter.drawPolygon(polyg);
		}
		if (rect.x() < w && x >= 0) {
			rect.setRight(x);
			color.setAlpha(120);
			painter.fillRect(rect, color);
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
	const int cx = qtractorScrollView::contentsX();
	const int h = qtractorScrollView::height() - 4;
	const int d = (h >> 2);

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
	const int h = qtractorScrollView::height(); // - 4;
	const int d = (h >> 1);

	QRect rect(0, h - d, d << 1, d);

	// Check play-head header...
	rect.moveLeft(m_pEditor->playHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragPlayHead;
		return true;
	}

	// Check loop and punch points...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return false;

	const int dx = pTimeScale->pixelFromFrame(m_pEditor->offset()) + d;

	// Loop points...
	if (pSession->isLooping()) {
		// Check loop-start header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->loopStart()) - dx);
		if (rect.contains(pos)) {
			m_dragCursor = DragLoopStart;
			return true;
		}
		// Check loop-end header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->loopEnd()) - dx);
		if (rect.contains(pos)) {
			m_dragCursor = DragLoopEnd;
			return true;
		}
	}

	// Punch in/out points...
	if (pSession->isPunching()) {
		// Check punch-in header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->punchIn()) - dx);
		if (rect.contains(pos)) {
			m_dragCursor = DragPunchIn;
			return true;
		}
		// Check punch-out header...
		rect.moveLeft(pTimeScale->pixelFromFrame(pSession->punchOut()) - dx);
		if (rect.contains(pos)) {
			m_dragCursor = DragPunchOut;
			return true;
		}
	}

	// Check edit-head header...
	rect.moveLeft(m_pEditor->editHeadX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragEditHead;
		return true;
	}

	// Check edit-tail header...
	rect.moveLeft(m_pEditor->editTailX() - d);
	if (rect.contains(pos)) {
		m_dragCursor = DragEditTail;
		return true;
	}

	// Check location marker headers...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekPixel(pos.x() + dx);
	if (pMarker) {
		unsigned long iFrame = pMarker->frame;
		rect.moveLeft(pTimeScale->pixelFromFrame(iFrame) - dx);
		if (rect.contains(pos)) {
			m_dragCursor = DragMarker;
			m_pDragMarker = pSession->timeScale()->markers().seekFrame(iFrame);
			return true;
		}
	}

	// Reset cursor if any persist around.
	if (m_dragCursor != DragNone) {
		qtractorScrollView::unsetCursor();
		m_dragCursor = DragNone;
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
	if (pSession == nullptr)
		return;

	// We'll need options somehow...
	qtractorOptions *pOptions = qtractorOptions::getInstance();

	// Which mouse state?
	bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));

	// Make sure we'll reset selection...
	if (!bModifier)
		m_pEditor->selectAll(m_pEditor->editView(), false);

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	unsigned long iFrame = m_pEditor->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));

	switch (pMouseEvent->button()) {
	case Qt::LeftButton:
		// Remember what and where we'll be dragging/selecting...
		m_dragState = DragStart;
		m_posDrag   = pos;
		// Try to catch mouse clicks over the cursor heads...
		if (dragHeadStart(m_posDrag)) {
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		//	m_dragState = m_dragCursor;
		}/* else if (!bModifier) {
			// Edit-head positioning...
			m_pEditor->setEditHead(iFrame);
			// Logical contents changed, just for visual feedback...
			m_pEditor->selectionChangeNotify();
		}*/
		break;
	case Qt::MiddleButton:
		if (pOptions && pOptions->bMidButtonModifier)
			bModifier = !bModifier;	// Reverse mid-button role...
		if (bModifier) {
			// Play-head positioning...
			m_pEditor->setPlayHead(iFrame);
			pSession->setPlayHead(iFrame);
		} else {
			// Edit-head/tail (merged) positioning...
			m_pEditor->setEditHead(iFrame);
			m_pEditor->setEditTail(iFrame);
		}
		// Logical contents changed, just for visual feedback...
		m_pEditor->selectionChangeNotify();
		break;
	case Qt::RightButton:
		if (pOptions && pOptions->bShiftKeyModifier)
			bModifier = !bModifier;
		if (!bModifier) {
			// Edit-tail positioning...
			m_pEditor->setEditTail(iFrame);
			// Logical contents changed, just for visual feedback...
			m_pEditor->selectionChangeNotify();
		}
		// Fall thru...
	default:
		break;
	}

//	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditTime::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	// Are we already moving/dragging something?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	const unsigned long iFrame = m_pEditor->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
	const int y = m_pEditor->editView()->contentsY();
	switch (m_dragState) {
	case DragNone:
		// Try to catch mouse over the cursor heads...
		if (dragHeadStart(pos))
			qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
		break;
	case DragSelect:
		// Rubber-band selection...
		m_rectDrag.setRight(
			m_pEditor->pixelSnap(pos.x() > 0 ? pos.x() : 0));
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->selectRect(m_pEditor->editView(), m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, false);
		// Edit-head/tail positioning...
		selectEdit(iFrame);
		showToolTip(m_rectDrag.normalized());
		break;
	case DragPlayHead:
		// Play-head positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->setPlayHead(iFrame);
		// Let the change get some immediate visual feedback...
		pMainForm->updateTransportTime(iFrame);
		showToolTip(iFrame);
		break;
	case DragEditHead:
	case DragPunchIn:
	case DragLoopStart:
		// Edit-head positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->setEditHead(iFrame);
		showToolTip(iFrame);
		break;
	case DragEditTail:
	case DragPunchOut:
	case DragLoopEnd:
		// Edit-head positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->setEditTail(iFrame);
		showToolTip(iFrame);
		break;
	case DragMarker:
		// Marker positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		showToolTip(iFrame);
		break;
	case DragStart:
		// Rubber-band starting...
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// We'll start dragging alright...
			const int h = m_pEditor->editView()->contentsHeight();
			m_rectDrag.setTop(0);
			m_rectDrag.setLeft(
				m_pEditor->pixelSnap(m_posDrag.x() > 0 ? m_posDrag.x() : 0));
			m_rectDrag.setRight(
				m_pEditor->pixelSnap(pos.x() > 0 ? pos.x() : 0));
			m_rectDrag.setBottom(h);
			if (!dragHeadStart(m_posDrag))
				m_dragCursor = DragSelect;
			m_dragState = m_dragCursor;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		}
		// Fall thru...
	default:
		break;
	}

//	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditTime::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
//	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Which mouse state?
	bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));

	// We'll need options somehow...
	qtractorOptions *pOptions = qtractorOptions::getInstance();

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	const unsigned long iFrame = m_pEditor->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));

	switch (m_dragState) {
	case DragSelect: {
		// Do the final range selection...
		m_pEditor->selectRect(m_pEditor->editView(), m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, true);
		// Edit-head/tail positioning...
		selectEdit(iFrame);
		// Not quite a selection change,
		// but for visual feedback...
		m_pEditor->selectionChangeNotify();
		break;
	}
	case DragPlayHead:
		// Play-head positioning commit...
		m_pEditor->setPlayHead(iFrame);
		pSession->setPlayHead(iFrame);
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
			m_pEditor->execute(
				new qtractorSessionLoopCommand(pSession,
					pSession->editHead(), pSession->loopEnd()));
		}
		break;
	case DragLoopEnd:
		// New loop-end boundary...
		if (pSession->loopStart() < pSession->editTail()) {
			// Yep, new loop-end point...
			m_pEditor->execute(
				new qtractorSessionLoopCommand(pSession,
					pSession->loopStart(), pSession->editTail()));
		}
		break;
	case DragPunchIn:
		// New punch-in boundary...
		if (pSession->editHead() < pSession->punchOut()) {
			// Yep, new punch-in point...
			m_pEditor->execute(
				new qtractorSessionPunchCommand(pSession,
					pSession->editHead(), pSession->punchOut()));
		}
		break;
	case DragPunchOut:
		// New punch-out boundary...
		if (pSession->punchIn() < pSession->editTail()) {
			// Yep, new punch-out point...
			m_pEditor->execute(
				new qtractorSessionPunchCommand(pSession,
					pSession->punchIn(), pSession->editTail()));
		}
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
		if (pOptions && pOptions->bShiftKeyModifier)
			bModifier = !bModifier;
		if (bModifier) {
			// Play-head positioning...
			m_pEditor->setPlayHead(iFrame);
			// Immediately commited...
			pSession->setPlayHead(iFrame);
		} else {
			// Deferred left-button edit-head positioning...
			m_pEditor->setEditHead(iFrame);
		}
		// Not quite a selection, rather just
		// for immediate visual feedback...
		m_pEditor->selectionChangeNotify();
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
	if (pMainForm == nullptr)
		return;

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	const unsigned long iFrame = m_pEditor->frameSnap(m_pEditor->offset()
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


// Handle zoom with mouse wheel.
void qtractorMidiEditTime::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers() & Qt::ControlModifier) {
		const int delta = pWheelEvent->angleDelta().y();
		if (delta > 0)
			m_pEditor->zoomIn();
		else
			m_pEditor->zoomOut();
	}
//	else qtractorScrollView::wheelEvent(pWheelEvent);
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

	m_pDragMarker = nullptr;

	// HACK: give focus to track-view... 
	m_pEditor->editView()->setFocus();
}


// Context menu event handler (dummy).
void qtractorMidiEditTime::contextMenuEvent (
	QContextMenuEvent */*pContextMenuEvent*/ )
{
}


// Trap for help/tool-tip events.
bool qtractorMidiEditTime::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	if (static_cast<QWidget *> (pObject) == pViewport) {
		if (pEvent->type() == QEvent::ToolTip
			&& m_dragCursor != DragNone && m_pEditor->isToolTips()) {
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
void qtractorMidiEditTime::showToolTip ( unsigned long iFrame ) const
{
	if (!m_pEditor->isToolTips())
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
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


void qtractorMidiEditTime::showToolTip ( const QRect& rect ) const
{
	if (!m_pEditor->isToolTips())
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	const unsigned long iFrameStart = m_pEditor->frameSnap(
		pTimeScale->frameFromPixel(qMax(0, rect.left())));
	const unsigned long iFrameEnd = m_pEditor->frameSnap(
		pTimeScale->frameFromPixel(qMax(0, rect.right())));

	QToolTip::showText(QCursor::pos(),
		tr("Start:\t%1\nEnd:\t%2\nLength:\t%3")
			.arg(pTimeScale->textFromFrame(iFrameStart))
			.arg(pTimeScale->textFromFrame(iFrameEnd))
			.arg(pTimeScale->textFromFrame(iFrameStart, true, iFrameEnd - iFrameStart)),
		qtractorScrollView::viewport());
}


// Edit-head/tail positioning...
void qtractorMidiEditTime::selectEdit ( unsigned long iFrame )
{
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	const unsigned long iFrame2
		= m_pEditor->frameSnap(m_pEditor->offset()
			+ pTimeScale->frameFromPixel(m_rectDrag.left()));

	if (iFrame < iFrame2) {
		m_pEditor->setEditHead(iFrame);
		m_pEditor->setEditTail(iFrame2);
	} else {
		m_pEditor->setEditHead(iFrame2);
		m_pEditor->setEditTail(iFrame);
	}
}


// end of qtractorMidiEditTime.cpp
