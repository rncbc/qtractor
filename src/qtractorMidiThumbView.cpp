// qtractorMidiThumbView.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiThumbView.h"
#include "qtractorMidiEditView.h"
#include "qtractorMidiEditor.h"
#include "qtractorMidiClip.h"
#include "qtractorSession.h"
#include "qtractorTrack.h"

#include "qtractorRubberBand.h"

#include <QApplication>
#include <QPainter>

#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>


//-------------------------------------------------------------------------
// qtractorMidiThumbView -- Session track line thumb view.

// Constructor.
qtractorMidiThumbView::qtractorMidiThumbView(
	qtractorMidiEditor *pEditor, QWidget *pParent )
	: QFrame(pParent)
{
	m_pEditor = pEditor;

	// Avoid intensively annoying repaints...
	QFrame::setAttribute(Qt::WA_StaticContents);
	QFrame::setAttribute(Qt::WA_OpaquePaintEvent);

	QFrame::setFrameShape(QFrame::Panel);
	QFrame::setFrameShadow(QFrame::Sunken);
	QFrame::setMinimumSize(QSize(120, 32));
	QFrame::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	QFrame::setFocusPolicy(Qt::ClickFocus);

	// Local play-head positioning.
	m_iPlayHeadX  = 0;

	m_dragState   = DragNone;
	m_pRubberBand = new qtractorRubberBand(QRubberBand::Rectangle, this, 2);
#if 0
	QPalette pal(m_pRubberBand->palette());
	pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
	m_pRubberBand->setPalette(pal);
	m_pRubberBand->setBackgroundRole(QPalette::NoRole);
#endif
	m_pRubberBand->show();

	QFrame::setToolTip(tr("MIDI Thumb view"));
}


// (Re)create the complete view pixmap.
void qtractorMidiThumbView::updateContents (void)
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	const QPalette& pal = QFrame::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.base().color());

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	qtractorTrack *pTrack = pMidiClip->track();
	if (pTrack == nullptr)
		return;

	qtractorMidiSequence *pSeq = pMidiClip->sequence();
	if (pSeq == nullptr)
		return;

	QPainter painter(&m_pixmap);
//	painter.initFrom(this);
//	painter.setFont(QFrame::font());

	// Local contents length (in ticks).
	const int cw = m_pEditor->editView()->contentsWidth() + 1;
	const unsigned long iClipStart = pMidiClip->clipStart();
	const unsigned long iClipEnd = iClipStart + pMidiClip->clipLength();
	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iClipStart);
	const unsigned long t0 = pNode->tickFromFrame(iClipStart);
	const int x0 = pNode->pixelFromTick(t0);

	int x2;

	// Check maximum note span...
	int iNoteSpan = (pSeq->noteMax() - pSeq->noteMin() + 1);
	if (iNoteSpan < 6)
		iNoteSpan = 6;

	const int h2 = 1 + (h / iNoteSpan);

	const bool bDrumMode = m_pEditor->isDrumMode();
	QVector<QPoint> diamond;
	if (bDrumMode) {
		const int h4 = (h2 >> 1);
		diamond.append(QPoint(  0, -h4));
		diamond.append(QPoint(-h2,  h4));
		diamond.append(QPoint(  0,  h4 + h2));
		diamond.append(QPoint( h2,  h4));
	}

	const QColor& fg = pTrack->foreground();
	painter.setPen(fg);
	painter.setBrush(fg.lighter());

	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			const unsigned long t1 = t0 + pEvent->time();
			pNode = cursor.seekTick(t1);
			x2 = (w * (pNode->pixelFromTick(t1) - x0)) / cw;
			const int y2 = h - h2
				- (h * (pEvent->note() - pSeq->noteMin())) / iNoteSpan;
			if (bDrumMode) {
				const QPolygon& polyg
					= QPolygon(diamond).translated(x2, y2);
				painter.drawPolygon(polyg); // diamond
			} else {
				const unsigned long t2 = t1 + pEvent->duration();
				const int w2 = (w * (pNode->pixelFromTick(t2) - x0)) / cw - x2;
			//	painter.fillRect(x2, y2, w2, h2, fg);
				painter.drawRect(x2, y2, w2, h2);
			}
		}
		pEvent = pEvent->next();
	}

	// Draw the location marker lines, if any...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekFrame(iClipStart);
	while (pMarker) {
		x2 = (w * (pTimeScale->pixelFromFrame(pMarker->frame) - x0)) / cw;
		if (x2 < 0 || x2 > w)
			break;
		painter.setPen(pMarker->color);
		painter.drawLine(x2, 0, x2, h);
		pMarker = pMarker->next();
	}

	// Shade the beyond-end-of-clip zone...
	const QBrush shade(QColor(0, 0, 0, 60));
	pNode = cursor.seekFrame(iClipEnd);
	x2 = (w * (pTimeScale->pixelFromFrame(iClipEnd) - x0)) / cw;
	painter.fillRect(QRect(x2, 0, w - x2, h), shade);

	// Draw the loop-bound lines, if any...
	if (pSession->isLooping()) {
		painter.setPen(Qt::darkCyan);
		x2 = (w * (pTimeScale->pixelFromFrame(pSession->loopStart()) - x0)) / cw;
		if (x2 > 0 && x2 < w) {
			painter.fillRect(QRect(0, 0, x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
		x2 = (w * (pTimeScale->pixelFromFrame(pSession->loopEnd()) - x0)) / cw;
		if (x2 > 0 && x2 < w) {
			painter.fillRect(QRect(x2, 0, w - x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
	}

	// Don't forget the punch-in/out ones too...
	if (pSession->isPunching()) {
		painter.setPen(Qt::darkMagenta);
		x2 = (w * (pTimeScale->pixelFromFrame(pSession->punchIn()) - x0)) / cw;
		if (x2 > 0 && x2 < w) {
			painter.fillRect(QRect(0, 0, x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
		x2 = (w * (pTimeScale->pixelFromFrame(pSession->punchOut()) - x0)) / cw;
		if (x2 < w) {
			painter.fillRect(QRect(x2, 0, w - x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
	}

	// Update relative play-head position...
	const unsigned long iPlayHead = pSession->playHead();
	m_iPlayHeadX = (w * (pTimeScale->pixelFromFrame(iPlayHead) - x0)) / cw;

	// May trigger an update now...
	update();
	updateThumb();
}


// Update thumb-position.
void qtractorMidiThumbView::updateThumb ( int dx )
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorMidiEditView *pEditView = m_pEditor->editView();

	const int cw = pEditView->contentsWidth() + 1;

	int x2 = dx + (w * pEditView->contentsX()) / cw;
	int w2 = (w * pEditView->viewport()->width()) / cw;

	if (w2 < 8)
		w2 = 8;
	else
	if (w2 > w)
		w2 = w;

	if (x2 < 0)
		x2 = 0;
	else
	if (x2 > w - w2)
		x2 = w - w2;

	m_pRubberBand->setGeometry(x2, 0, w2, h);
}


// Update playhead-position.
void qtractorMidiThumbView::updatePlayHead ( unsigned long iPlayHead )
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	// Extra: update current playhead position...
	const int cw = m_pEditor->editView()->contentsWidth() + 1;
	const unsigned long iClipStart = pMidiClip->clipStart();
	const int x0 = pTimeScale->pixelFromFrame(iClipStart);
	const int x2 = (w * (pTimeScale->pixelFromFrame(iPlayHead) - x0)) / cw;

	if (m_iPlayHeadX != x2) {
		// Override old playhead line...
		update(QRect(m_iPlayHeadX, 0, 1, h));
		// New position is in...
		m_iPlayHeadX = x2;
		// And draw it...
		update(QRect(m_iPlayHeadX, 0, 1, h));
	}
}


// Update view-position.
void qtractorMidiThumbView::updateView ( int dx )
{
	const int w = QFrame::width();
	if (w < 1)
		return;

	qtractorMidiEditView *pEditView = m_pEditor->editView();

	const int cw = pEditView->contentsWidth() + 1;
	const int cy = pEditView->contentsY();

	int cx = pEditView->contentsX() + (dx * cw) / w;
	if (cx < 0)
		cx = 0;

	m_pEditor->setSyncViewHoldOn(true);
	pEditView->setContentsPos(cx, cy);
}


// Set playhead-position (indirect).
void qtractorMidiThumbView::setPlayHeadX ( int iPlayHeadX )
{
	const int w = QFrame::width();
	if (w < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	const int cw = m_pEditor->editView()->contentsWidth() + 1;
	const int x0 = pTimeScale->pixelFromFrame(pMidiClip->clipStart());
	pSession->setPlayHead(pTimeScale->frameFromPixel((cw * iPlayHeadX) / w) - x0);

	m_pEditor->setSyncViewHoldOn(false);
}


// Session track-line paint method.
void qtractorMidiThumbView::paintEvent ( QPaintEvent *pPaintEvent )
{
	QPainter painter(this);
	
	// Render the famous pixmap region...
	const QRect& rect = pPaintEvent->rect();
	painter.drawPixmap(rect, m_pixmap, rect);

	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;
	
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == nullptr)
		return;

	int x2;

	const int cw = m_pEditor->editView()->contentsWidth() + 1;
	const unsigned long iClipStart = pMidiClip->clipStart();
	const unsigned long x0 = pTimeScale->pixelFromFrame(iClipStart);
	
	// Extra: update edit-bound positions...
	painter.setPen(Qt::blue);
	x2 = (w * (pTimeScale->pixelFromFrame(pSession->editHead()) - x0)) / cw;
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);
	x2 = (w * (pTimeScale->pixelFromFrame(pSession->editTail()) - x0)) / cw;
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);

	x2 = (w * (pTimeScale->pixelFromFrame(pSession->playHeadAutoBackward()) - x0)) / cw;
	if (x2 >= rect.left() && x2 <= rect.right()) {
		painter.setPen(QColor(240, 0, 0, 60));
		painter.drawLine(x2, 0, x2, h);
	}

	// Draw current play-head as well...
	x2 = m_iPlayHeadX;
	if (x2 >= rect.left() && x2 <= rect.right()) {
		painter.setPen(Qt::red);
		painter.drawLine(x2, 0, x2, h);
	}

	// Shade-out what's not in view...
	if (m_pRubberBand) {
		const QColor rgba(0, 0, 0, 96);
		const QRect& rect2 = m_pRubberBand->geometry();
		if (rect2.left() > rect.left())
			painter.fillRect(
				rect.left(), rect.top(),
				rect2.left() - rect.left(), rect.height(), rgba);
		if (rect2.right() < rect.right() + 1)
			painter.fillRect(
				rect2.right() + 1, rect.top(),
				rect.right() - rect2.right(), rect.height(), rgba);
	}
}


// Session track-line paint method.
void qtractorMidiThumbView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QFrame::resizeEvent(pResizeEvent);

	updateContents();
}


// Handle selection with mouse.
void qtractorMidiThumbView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	m_dragState = DragNone;

	// Only expected behavior with left-button pressed...
	if (pMouseEvent->button() == Qt::LeftButton) {
		QFrame::setCursor(QCursor(Qt::PointingHandCursor));
		m_posDrag = pMouseEvent->pos();
		const QRect& rect = m_pRubberBand->geometry();
		if (rect.contains(pMouseEvent->pos())) {
			m_dragState = DragStart;
		} else {
			m_dragState = DragClick;
		}
	}
	else
	if (pMouseEvent->button() == Qt::MiddleButton) {
		// Make it change playhead?...
		if (pMouseEvent->modifiers()
			& (Qt::ShiftModifier | Qt::ControlModifier))
			setPlayHeadX(pMouseEvent->pos().x());
	}

	QFrame::mousePressEvent(pMouseEvent);
}


void qtractorMidiThumbView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Only expected behavior with left-button pressed...
	if (pMouseEvent->buttons() & Qt::LeftButton) {
		const QPoint& pos = pMouseEvent->pos();
		if ((m_dragState == DragStart || m_dragState == DragClick)
			&& (pos - m_posDrag).manhattanLength()
				> QApplication::startDragDistance()) {
			m_dragState = DragMove;
			QFrame::setCursor(QCursor(Qt::SizeHorCursor));
		}
		if (m_dragState == DragMove && rect().contains(pos)) {
			updateView(pos.x() - m_posDrag.x());
			m_posDrag.setX(pos.x());
		}
	}

	QFrame::mouseMoveEvent(pMouseEvent);
}


void qtractorMidiThumbView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QFrame::mouseReleaseEvent(pMouseEvent);

	// Only expected behavior with left-button pressed...
	if (pMouseEvent->button() == Qt::LeftButton) {
		const QPoint& pos = pMouseEvent->pos();
		if (m_dragState == DragMove)
			updateView(pos.x() - m_posDrag.x());
		else {
			if (m_dragState == DragStart || m_dragState == DragClick) {
				const QRect& rect = m_pRubberBand->geometry();
				m_posDrag.setX(((rect.left() + rect.right()) >> 1));
				updateView(pos.x() - m_posDrag.x());
			}
			// Make it change playhead?...
			if (pMouseEvent->modifiers()
				& (Qt::ShiftModifier | Qt::ControlModifier))
				setPlayHeadX(pos.x());
		}
	}

	// Clean up.
	resetDragState();
}


// Reset drag/select state.
void qtractorMidiThumbView::resetDragState (void)
{
	// Restore uncommitted thumb position?...
	if (m_dragState == DragMove)
		updateThumb();

	// Cancel any dragging out there...
	if (m_dragState != DragNone)
		QFrame::unsetCursor();

	// Force null state.
	m_dragState = DragNone;

	// HACK: give focus to edit-view...
	m_pEditor->editView()->setFocus();
}


// Keyboard event handler.
void qtractorMidiThumbView::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiThumbView::keyPressEvent(%d)", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
		resetDragState();
		break;
	default:
		QFrame::keyPressEvent(pKeyEvent);
		break;
	}
}


// end of qtractorMidiThumbView.cpp
