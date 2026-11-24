// qtractorAudioThumbView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioThumbView.h"
#include "qtractorAudioEditView.h"
#include "qtractorAudioEditor.h"
#include "qtractorAudioClip.h"
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
// qtractorAudioThumbView -- Session track line thumb view.

// Constructor.
qtractorAudioThumbView::qtractorAudioThumbView (
	qtractorAudioEditor *pEditor, QWidget *pParent )
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

	QFrame::setToolTip(tr("Audio Thumb view"));
}


// (Re)create the complete view pixmap.
void qtractorAudioThumbView::updateContents (void)
{
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

	qtractorAudioClip *pAudioClip = m_pEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	const unsigned long iClipLength
		= qMin(pAudioClip->fileLength(), pAudioClip->clipLength());
	const unsigned long iClipStart = pAudioClip->clipStart();
	const unsigned long iClipEnd = iClipStart + iClipLength;
	const int x0 = pSession->pixelFromFrame(iClipStart);
	const int w0 = pSession->pixelFromFrame(iClipEnd) - x0;

	qtractorAudioEditView *pEditView = m_pEditor->editView();
	const int cw = pSession->pixelFromFrame(iClipStart
		+ pTimeScale->frameFromPixel(pEditView->contentsWidth() + 1)) - x0;

	m_pixmap = QPixmap(cw, h);
	m_pixmap.fill(QFrame::palette().base().color());

	QPainter painter(&m_pixmap);
	pAudioClip->draw(painter, QRect(0, 0, w0, h), 0);

	// Update relative play-head position...
	const unsigned long iPlayHead = pSession->playHead();
	m_iPlayHeadX = (w * (pSession->pixelFromFrame(iPlayHead) - x0)) / cw;

	// May trigger an update now...
	update();
	updateThumb();
}


// Update thumb-position.
void qtractorAudioThumbView::updateThumb ( int dx )
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorAudioEditView *pEditView = m_pEditor->editView();

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
void qtractorAudioThumbView::updatePlayHead ( unsigned long iPlayHead )
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorAudioClip *pAudioClip = m_pEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	// Extra: update current playhead position...
	const int cw = m_pixmap.width() + 1;
	const unsigned long iClipStart = pAudioClip->clipStart();
	const int x0 = pSession->pixelFromFrame(iClipStart);
	const int x2 = (w * (pSession->pixelFromFrame(iPlayHead) - x0)) / cw;

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
void qtractorAudioThumbView::updateView ( int dx )
{
	const int w = QFrame::width();
	if (w < 1)
		return;

	qtractorAudioEditView *pEditView = m_pEditor->editView();

	const int cw = pEditView->contentsWidth() + 1;
	const int cy = pEditView->contentsY();

	int cx = pEditView->contentsX() + (dx * cw) / w;
	if (cx < 0)
		cx = 0;

	m_pEditor->setSyncViewHoldOn(true);
	pEditView->setContentsPos(cx, cy);
}


// Set playhead-position (indirect).
void qtractorAudioThumbView::setPlayHeadX ( int iPlayHeadX )
{
	const int w = QFrame::width();
	if (w < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorAudioClip *pAudioClip = m_pEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	const int cw = m_pixmap.width() + 1;
	const int x0 = pSession->pixelFromFrame(pAudioClip->clipStart());
	pSession->setPlayHead(
		pSession->frameFromPixel(x0 + ((cw * iPlayHeadX) / w)));

	m_pEditor->setSyncViewHoldOn(false);
}


// Session track-line paint method.
void qtractorAudioThumbView::paintEvent ( QPaintEvent *pPaintEvent )
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	QPainter painter(this);

	// Render the scaled pixmap region...
	const qreal dw = qreal(m_pixmap.width()) / qreal(w);
	const qreal dh = qreal(m_pixmap.height()) / qreal(h);
	const QRectF& rect = QRectF(pPaintEvent->rect());
	painter.drawPixmap(rect, m_pixmap, QRectF(
		rect.x() * dw, rect.y() * dh,
		rect.width() * dw, rect.height() * dh));

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorAudioClip *pAudioClip = m_pEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	const int cw = m_pixmap.width() + 1;
	const unsigned long iClipLength = pAudioClip->clipLength();
	const unsigned long iClipStart = pAudioClip->clipStart();
	const unsigned long iClipEnd = iClipStart + iClipLength;
	const unsigned long iClipLength2 = pAudioClip->fileLength();
	const unsigned long iClipEnd2 = iClipStart + iClipLength2;
	const unsigned long x0 = pTimeScale->pixelFromFrame(iClipStart);

	int x1, x2;

	// Shade the beyond-end-of-clip zone...
	const QBrush shade(QColor(0, 0, 0, 60));
	x1 = (w * (pTimeScale->pixelFromFrame(iClipEnd) - x0)) / cw;
	if (x1 < 0)
		x1 = 0;
	if (x1 < w)
		painter.fillRect(x1, 0, w - x1, h, shade);

	if (iClipLength > iClipLength2) {
		x2 = (w * (pTimeScale->pixelFromFrame(iClipEnd2) - x0)) / cw;
		if (x2 < 0)
			x2 = 0;
		if (x2 < w) {
			const QPalette& pal = QFrame::palette();
			const QColor& rgbBase = pal.base().color();
			const QColor& rgbLight = pal.midlight().color();
			const bool bDark = (rgbBase.value() < 0x7f);
			painter.fillRect(x2, 0, x1 - x2, h, QBrush(
				bDark ? rgbLight.lighter() : rgbLight.darker(),
				Qt::BDiagPattern));
		}
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
void qtractorAudioThumbView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QFrame::resizeEvent(pResizeEvent);

	updateContents();
}


// Handle selection with mouse.
void qtractorAudioThumbView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	m_dragState = DragNone;

	const QPoint& pos = pMouseEvent->pos();
	const QRect& rect = m_pRubberBand->geometry();

	// Only expected behavior with left/middle-button pressed...
	if (pMouseEvent->button() == Qt::LeftButton) {
		QFrame::setCursor(QCursor(Qt::PointingHandCursor));
		m_posDrag = pos;
		if (rect.contains(pos))
			m_dragState = DragStart;
		else
			m_dragState = DragClick;
		updateView(pos.x() - rect.center().x());
	}
	else
	if (pMouseEvent->button() == Qt::MiddleButton) {
		updateView(pos.x() - rect.center().x());
		// Make it change playhead?...
		setPlayHeadX(pos.x());
	}

	QFrame::mousePressEvent(pMouseEvent);
}


void qtractorAudioThumbView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
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
		if (m_dragState == DragMove/*&& rect().contains(pos)*/) {
			updateView(pos.x() - m_posDrag.x());
			m_posDrag.setX(pos.x());
		}
	}

	QFrame::mouseMoveEvent(pMouseEvent);
}


void qtractorAudioThumbView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QFrame::mouseReleaseEvent(pMouseEvent);

	// Only expected behavior with left-button pressed...
	if (pMouseEvent->button() == Qt::LeftButton) {
		const QPoint& pos = pMouseEvent->pos();
		if (m_dragState == DragMove) {
			updateView(pos.x() - m_posDrag.x());
		} else {
			if (m_dragState == DragStart || m_dragState == DragClick) {
				const QRect& rect = m_pRubberBand->geometry();
				updateView(pos.x() - rect.center().x());
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
void qtractorAudioThumbView::resetDragState (void)
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
void qtractorAudioThumbView::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioThumbView::keyPressEvent(%d)", pKeyEvent->key());
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


// end of qtractorAudioThumbView.cpp
