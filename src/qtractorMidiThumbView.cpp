// qtractorMidiThumbView.cpp
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

	// Local contents length (in ticks).
	m_iContentsLength = 0;

	// Local play-head positioning.
	m_iPlayHeadX  = 0;

	m_dragState   = DragNone;
	m_pRubberBand = new qtractorRubberBand(QRubberBand::Rectangle, this, 2);
//	QPalette pal(m_pRubberBand->palette());
//	pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
//	m_pRubberBand->setPalette(pal);
//	m_pRubberBand->setBackgroundRole(QPalette::NoRole);
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
	m_pixmap.fill(pal.dark().color());

	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == NULL)
		return;

	qtractorTrack *pTrack = pMidiClip->track();
	if (pTrack == NULL)
		return;

	qtractorMidiSequence *pSeq = pMidiClip->sequence();
	if (pSeq == NULL)
		return;

	// Local contents length (in ticks).
	const int cw = m_pEditor->editView()->contentsWidth() + 1;
	const unsigned long iClipStart = pMidiClip->clipStart();
	const unsigned long iClipEnd = iClipStart + pMidiClip->clipLength();
	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iClipStart);
	const unsigned long t0 = pNode->tickFromFrame(iClipStart);
	const int x0 = pNode->pixelFromTick(t0);
	pNode = cursor.seekPixel(x0 + cw);
	m_iContentsLength = pNode->tickFromPixel(x0 + cw) - t0;

	const int f2 = 1 + (m_iContentsLength / w);
	int x2;

	// Check maximum note span...
	int iNoteSpan = (pSeq->noteMax() - pSeq->noteMin() + 1);
	if (iNoteSpan < 6)
		iNoteSpan = 6;

	const int h2 = 1 + (h / iNoteSpan);

	const QColor& fg = pTrack->foreground();
	painter.setPen(fg);
	painter.setBrush(fg.lighter());

	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			x2 = pEvent->time() / f2;
			const int y2 = h - h2
				- (h * (pEvent->note() - pSeq->noteMin())) / iNoteSpan;
			const int w2 = 1 + (pEvent->duration() / f2);
		//	painter.fillRect(x2, y2, w2, h2, fg);
			painter.drawRect(x2, y2, w2, h2);
		}
		pEvent = pEvent->next();
	}

	// Draw the location marker lines, if any...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekFrame(iClipStart);
	while (pMarker) {
		x2 = int(pTimeScale->tickFromFrame(pMarker->frame) - t0) / f2;
		if (x2 < 0 || x2 > w)
			break;
		painter.setPen(pMarker->color);
		painter.drawLine(x2, 0, x2, h);
		pMarker = pMarker->next();
	}

	// Shade the beyond-end-of-clip zone...
	const QBrush shade(QColor(0, 0, 0, 60));
	pNode = cursor.seekFrame(iClipEnd);
	x2 = int(pNode->tickFromFrame(iClipEnd) - t0) / f2;
	painter.fillRect(QRect(x2, 0, w - x2, h), shade);

	// Draw the loop-bound lines, if any...
	if (pSession->isLooping()) {
		painter.setPen(Qt::darkCyan);
		x2 = int(pTimeScale->tickFromFrame(pSession->loopStart()) - t0) / f2;
		if (x2 > 0 && x2 < w) {
			painter.fillRect(QRect(0, 0, x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
		x2 = int(pTimeScale->tickFromFrame(pSession->loopEnd()) - t0) / f2;
		if (x2 > 0 && x2 < w) {
			painter.fillRect(QRect(x2, 0, w - x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
	}

	// Don't forget the punch-in/out ones too...
	if (pSession->isPunching()) {
		painter.setPen(Qt::darkMagenta);
		x2 = int(pTimeScale->tickFromFrame(pSession->punchIn()) - t0) / f2;
		if (x2 > 0 && x2 < w) {
			painter.fillRect(QRect(0, 0, x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
		x2 = int(pTimeScale->tickFromFrame(pSession->punchOut()) - t0) / f2;
		if (x2 < w) {
			painter.fillRect(QRect(x2, 0, w - x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
	}

	// Update relative play-head position...
	const unsigned long iPlayHead = m_pEditor->playHead();
	m_iPlayHeadX = int(pTimeScale->tickFromFrame(iPlayHead) - t0) / f2;

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
	if (pTimeScale == NULL)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == NULL)
		return;

	const int f2 = 1 + (m_iContentsLength / w);

	// Extra: update current playhead position...
	const unsigned long iClipStart = pMidiClip->clipStart();
	const unsigned long t0 = pTimeScale->tickFromFrame(iClipStart);
	const int x2 = int(pTimeScale->tickFromFrame(iPlayHead) - t0) / f2;

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
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == NULL)
		return;

	const int f2 = 1 + (m_iContentsLength / w);

	const unsigned long t0 = pTimeScale->tickFromFrame(pMidiClip->clipStart());
	pSession->setPlayHead(pTimeScale->frameFromTick(t0 + f2 * iPlayHeadX));
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
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	qtractorMidiClip *pMidiClip = m_pEditor->midiClip();
	if (pMidiClip == NULL)
		return;

	const int f2 = 1 + (m_iContentsLength / w);
	int x2;

	const unsigned long iClipStart = pMidiClip->clipStart();
	const unsigned long t0 = pTimeScale->tickFromFrame(iClipStart);
	
	// Extra: update edit-bound positions...
	painter.setPen(Qt::blue);	
	x2 = int(pTimeScale->tickFromFrame(pSession->editHead()) - t0) / f2;
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);
	x2 = int(pTimeScale->tickFromFrame(pSession->editTail()) - t0) / f2;
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);

	// Draw current play-head as well...
	x2 = m_iPlayHeadX;
	if (x2 >= rect.left() && x2 <= rect.right()) {
		painter.setPen(Qt::red);
		painter.drawLine(x2, 0, x2, h);
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
		m_posDrag = pMouseEvent->pos();
		if (m_pRubberBand->geometry().contains(m_posDrag)) {
			m_dragState = DragStart;
			QFrame::setCursor(QCursor(Qt::SizeHorCursor));
		} else {
			m_dragState = DragClick;
			QFrame::setCursor(QCursor(Qt::PointingHandCursor));
		}
	}
	else
	if (pMouseEvent->button() == Qt::MidButton) {
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
		if (m_dragState == DragStart
			&& (pos - m_posDrag).manhattanLength()
				> QApplication::startDragDistance())
			m_dragState = DragMove;
		if (m_dragState == DragMove)
			updateThumb(pos.x() - m_posDrag.x());
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
			if (m_dragState == DragClick) {
				const QRect& rect = m_pRubberBand->geometry();
				updateView(pos.x() - ((rect.left() + rect.right()) >> 1));
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
