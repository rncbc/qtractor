// qtractorThumbView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

	// Local play-head positioning.
	m_iPlayHeadX  = 0;
	m_dragState   = DragNone;
	m_pRubberBand = new qtractorRubberBand(QRubberBand::Rectangle, this, 2);
//	QPalette pal(m_pRubberBand->palette());
//	pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
//	m_pRubberBand->setPalette(pal);
//	m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	m_pRubberBand->show();

	QFrame::setToolTip(tr("Thumb view"));
}


// (Re)create the complete view pixmap.
void qtractorThumbView::updateContents (void)
{
	int w = QFrame::width();
	int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	const QPalette& pal = QFrame::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.dark().color());

	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int cw = pTracks->trackView()->contentsWidth() + 1;
	int ch = pTracks->trackView()->contentsHeight();
	int x2, w2;

	if (ch > 0) {
		int y2 = 1;
		qtractorTrack *pTrack = pSession->tracks().first();
		while (pTrack && y2 < h) {
			int h2 = ((h * pTrack->zoomHeight()) / ch);
			if (h2 < 3)
				h2 = 3;
			qtractorClip *pClip = pTrack->clips().first();
			while (pClip) {
				x2 = (w * pSession->pixelFromFrame(pClip->clipStart()))  / cw;
				w2 = (w * pSession->pixelFromFrame(pClip->clipLength())) / cw;
				painter.fillRect(x2, y2, w2 - 1, h2 - 1, pTrack->background());
				pClip = pClip->next();
			}
			y2 += h2;
			pTrack = pTrack->next();
		}
	}

	// Draw the loop-bound lines, if any...
	if (pSession->isLooping()) {
		painter.setPen(Qt::darkCyan);
		x2 = (w * pSession->pixelFromFrame(pSession->loopStart())) / cw;
		if (x2 < w)
			painter.drawLine(x2, 0, x2, h);
		x2 = (w * pSession->pixelFromFrame(pSession->loopEnd())) / cw;
		if (x2 < w)
			painter.drawLine(x2, 0, x2, h);
	}

	// May trigger an update now.
	update();
}


// Update thumb-position.
void qtractorThumbView::updateThumb ( int dx )
{
	int w = QFrame::width();
	int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int cw = pTracks->trackView()->contentsWidth() + 1;
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

	// Extra: update current playhead position...
	updatePlayHead();
}


// Update playhead-position.
void qtractorThumbView::updatePlayHead (void)
{
	int w = QFrame::width();
	int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int cw = pTracks->trackView()->contentsWidth() + 1;
	int x2;

	// Extra: update current playhead position...
	x2 = (w * pSession->pixelFromFrame(pSession->playHead())) / cw;
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
	int w = QFrame::width();
	int h = QFrame::height();
	if (w < 1 || h < 1)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int cw = pTracks->trackView()->contentsWidth() + 1;
	int cx = pTracks->trackView()->contentsX() + (dx * cw) / w;
	int cy = pTracks->trackView()->contentsY();

	if (cx < 0)
		cx = 0;

	pTracks->trackView()->setContentsPos(cx, cy);
}


// Session track-line paint method.
void qtractorThumbView::paintEvent ( QPaintEvent *pPaintEvent )
{
	QPainter painter(this);
	
	// Render the famous pixmap region...
	const QRect& rect = pPaintEvent->rect();
	painter.drawPixmap(rect, m_pixmap, rect);

	int w = QFrame::width();
	int h = QFrame::height();
	if (w < 1 || h < 1)
		return;
	
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int cw = pTracks->trackView()->contentsWidth() + 1;
	int x2;

	// Draw current edit-bound lines...
	painter.setPen(Qt::blue);
	x2 = (w * pSession->pixelFromFrame(pSession->editHead())) / cw;
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);
	x2 = (w * pSession->pixelFromFrame(pSession->editTail())) / cw;
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);

	// Draw current play-head as well...
	painter.setPen(Qt::red);
	x2 = (w * pSession->pixelFromFrame(pSession->playHead())) / cw;
	if (x2 >= rect.left() && x2 <= rect.right())
		painter.drawLine(x2, 0, x2, h);
}


// Session track-line paint method.
void qtractorThumbView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QFrame::resizeEvent(pResizeEvent);

	updateThumb();
}


// Handle selection with mouse.
void qtractorThumbView::mousePressEvent ( QMouseEvent *pMouseEvent )
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

	QFrame::mousePressEvent(pMouseEvent);
}


void qtractorThumbView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
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


void qtractorThumbView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QFrame::mouseReleaseEvent(pMouseEvent);

	// Only expected behavior with left-button pressed...
	if (pMouseEvent->button() == Qt::LeftButton) {
		const QPoint& pos = pMouseEvent->pos();
		if (m_dragState == DragMove)
			updateView(pos.x() - m_posDrag.x());
		else
		if (m_dragState == DragClick) {
			const QRect& rect = m_pRubberBand->geometry();
			updateView(pos.x() - ((rect.left() + rect.right()) >> 1));
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
