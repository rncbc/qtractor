// qtractorThumbView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorThumbView.h"
#include "qtractorTrackView.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorClip.h"

#include "qtractorRubberBand.h"

#include "qtractorMainForm.h"

#include <QApplication>
#include <QPainter>

#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>


//-------------------------------------------------------------------------
// qtractorThumbView -- Session track line thumb view.

// Constructor.
qtractorThumbView::qtractorThumbView( QWidget *pParent )
	: QFrame(pParent)
{
	// Avoid intensively annoying repaints...
	QFrame::setAttribute(Qt::WA_StaticContents);
	QFrame::setAttribute(Qt::WA_OpaquePaintEvent);

	QFrame::setFrameShape(QFrame::Panel);
	QFrame::setFrameShadow(QFrame::Sunken);
	QFrame::setMinimumSize(QSize(120, 32));
	QFrame::setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	QFrame::setFocusPolicy(Qt::ClickFocus);

	// Local contents length (in frames).
	m_iContentsLength = 0;

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

	QFrame::setToolTip(tr("Thumb view"));
}


// (Re)create the complete view pixmap.
void qtractorThumbView::updateContents (void)
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	const QPalette& pal = QFrame::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.mid().color());

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	const int n1 = pSession->tracks().count();
	if (n1 < 1)
		return;

	QPainter painter(&m_pixmap);
//	painter.initFrom(this);
//	painter.setFont(QFrame::font());
	const QBrush shade(QColor(0, 0, 0, 60));

	// Local contents length (in frames).
	m_iContentsLength = pSession->sessionEnd();
	if (m_iContentsLength > 0) {
		qtractorTimeScale::Cursor cursor(pTimeScale);
		qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iContentsLength);
		m_iContentsLength += pNode->frameFromBeat(
			pNode->beat + 2 * pSession->beatsPerBar()) - pNode->frame;
	} else {
		m_iContentsLength += pTimeScale->frameFromPixel(
			pTracks->trackView()->width());
	}

	const int ch = pTracks->trackView()->contentsHeight();
	const int f2 = 1 + (m_iContentsLength / w);
	const int h1 = (h / n1) - 1;

	int x2, w2;

	if (ch > 0) {
		int y2 = 0;
		qtractorTrack *pTrack = pSession->tracks().first();
		while (pTrack && y2 < h) {
			const int h2 = 1 + ((h * pTrack->zoomHeight()) / ch);
			QColor bg(pTrack->background());
			if (pTrack->isMute() || (!pTrack->isSolo() && pSession->soloTracks()))
				bg = bg.darker();
			qtractorClip *pClip = pTrack->clips().first();
			while (pClip) {
				x2 = int(pClip->clipStart()  / f2);
				w2 = int(pClip->clipLength() / f2);
				painter.fillRect(x2, y2, w2, h2, bg);
				if (pClip->isClipMute())
					painter.fillRect(x2, y2, w2, h2, shade);
				pClip = pClip->next();
			}
			y2 += h2;
			if (h1 > 1)
				++y2;
			pTrack = pTrack->next();
		}
	}

	// Draw the location marker lines, if any...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().first();
	while (pMarker) {
		x2 = int(pMarker->frame / f2);
		painter.setPen(pMarker->color);
		painter.drawLine(x2, 0, x2, h);
		pMarker = pMarker->next();
	}

	// Draw the loop-bound lines, if any...
	if (pSession->isLooping()) {
		const QBrush shade(QColor(0, 0, 0, 60));
		painter.setPen(Qt::darkCyan);
		x2 = int(pSession->loopStart() / f2);
		if (x2 < w) {
			painter.fillRect(QRect(0, 0, x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
		x2 = int(pSession->loopEnd() / f2);
		if (x2 < w) {
			painter.fillRect(QRect(x2, 0, w - x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
	}

	// Don't forget the punch-in/out ones too...
	if (pSession->isPunching()) {
		painter.setPen(Qt::darkMagenta);
		x2 = int(pSession->punchIn() / f2);
		if (x2 < w) {
			painter.fillRect(QRect(0, 0, x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
		x2 = int(pSession->punchOut() / f2);
		if (x2 < w) {
			painter.fillRect(QRect(x2, 0, w - x2, h), shade);
			painter.drawLine(x2, 0, x2, h);
		}
	}

	// May trigger an update now.
	update();
}


// Update thumb-position.
void qtractorThumbView::updateThumb ( int dx )
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	const int cw = pSession->pixelFromFrame(m_iContentsLength) + 1;

	int x2 = dx + (w * pTracks->trackView()->contentsX()) / cw;
	int w2 = (w * pTracks->trackView()->viewport()->width()) / cw;

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
void qtractorThumbView::updatePlayHead ( unsigned long iPlayHead )
{
	const int w = QFrame::width();
	const int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	const int f2 = 1 + (m_iContentsLength / w);

	// Extra: update current playhead position...
	const int x2 = int(iPlayHead / f2);
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
void qtractorThumbView::updateView ( int dx )
{
	const int w = QFrame::width();
	if (w < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	qtractorTrackView *pTrackView = pTracks->trackView();
	if (pTrackView == nullptr)
		return;

	const int cw = pSession->pixelFromFrame(m_iContentsLength) + 1;
	const int cy = pTrackView->contentsY();

	int cx = pTrackView->contentsX() + (dx * cw) / w;
	if (cx < 0)
		cx = 0;

	pTrackView->setSyncViewHoldOn(true);
	pTrackView->setContentsPos(cx, cy);
}


// Set playhead-position (indirect).
void qtractorThumbView::setPlayHeadX ( int iPlayHeadX )
{
	const int w = QFrame::width();
	if (w < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	qtractorTrackView *pTrackView = pTracks->trackView();
	if (pTrackView == nullptr)
		return;

	const int f2 = 1 + (m_iContentsLength / w);
	const unsigned long iPlayHead = f2 * iPlayHeadX;
	pSession->setPlayHead(iPlayHead);

	pTrackView->setPlayHeadAutoBackward(iPlayHead);
	pTrackView->setSyncViewHoldOn(false);
}


// Session track-line paint method.
void qtractorThumbView::paintEvent ( QPaintEvent *pPaintEvent )
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

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	const int f2 = 1 + (m_iContentsLength / w);
	int x2;

	// Draw current edit-bound lines...
	painter.setPen(Qt::blue);
	x2 = int(pSession->editHead() / f2);
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);
	x2 = int(pSession->editTail() / f2);
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);

	x2 = int(pSession->playHeadAutoBackward() / f2);
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
void qtractorThumbView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QFrame::resizeEvent(pResizeEvent);

	updateContents();
	updateThumb();
}


// Handle selection with mouse.
void qtractorThumbView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	m_dragState = DragNone;

	// Only expected behavior with left-button pressed...
	if (pMouseEvent->button() == Qt::LeftButton) {
		const QRect& rect = m_pRubberBand->geometry();
		m_posDrag = pMouseEvent->pos();
		QFrame::setCursor(QCursor(Qt::PointingHandCursor));
		if (rect.contains(m_posDrag)) {
			m_dragState = DragStart;
		} else {
			m_dragState = DragClick;
			m_posDrag.setX(((rect.left() + rect.right()) >> 1));
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


void qtractorThumbView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
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
		if (m_dragState == DragMove) {
			updateView(pos.x() - m_posDrag.x());
			m_posDrag.setX(pos.x());
		}
	}

	QFrame::mouseMoveEvent(pMouseEvent);
}


void qtractorThumbView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QFrame::mouseReleaseEvent(pMouseEvent);

	// Only expected behavior with left-button pressed...
	if (pMouseEvent->button() == Qt::LeftButton) {
		const QPoint& pos = pMouseEvent->pos();
		if (m_dragState == DragMove)
			updateView(pos.x() - m_posDrag.x());
		else {
			if (m_dragState == DragClick)
				updateView(pos.x() - m_posDrag.x());
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
void qtractorThumbView::resetDragState (void)
{
	// Restore uncommitted thumb position?...
	if (m_dragState == DragMove)
		updateThumb();

	// Cancel any dragging out there...
	if (m_dragState != DragNone)
		QFrame::unsetCursor();

	// Force null state.
	m_dragState = DragNone;

	// HACK: give focus to track-view...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->tracks())
		pMainForm->tracks()->trackView()->setFocus();
}


// Keyboard event handler.
void qtractorThumbView::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorThumbView::keyPressEvent(%d)", pKeyEvent->key());
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


// end of qtractorThumbView.cpp
