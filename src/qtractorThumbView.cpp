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

	m_dragState   = DragNone;
	m_pRubberBand = new qtractorRubberBand(QRubberBand::Rectangle, this, 2);
//	QPalette pal(m_pRubberBand->palette());
//	pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
//	m_pRubberBand->setPalette(pal);
//	m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	m_pRubberBand->show();

	QFrame::setToolTip(tr("Thumb view"));
}


// Update thumb-position.
void qtractorThumbView::updateThumb ( int dx )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int w = QFrame::width()  - 2;
	int h = QFrame::height() - 2;

	int w2 = 0;
	int x2 = dx;
	int f2 = pSession->pixelFromFrame(pSession->sessionLength());
	if (f2 > 0) {
		f2 += pSession->pixelFromBeat(2 * pSession->beatsPerBar());
		w2 += (w * pTracks->trackView()->viewport()->width()) / f2;
		x2 += (w * pTracks->trackView()->contentsX()) / f2;
	} else {
		w2 += w;
	}

	if (w2 < 8)
		w2 = 8;
	else
	if (w2 > w)
		w2 = w;

	if (x2 < 1)
		x2 = 1;
	else
	if (x2 > w - w2)
		x2 = w - w2;

	m_pRubberBand->setGeometry(x2, 1, w2, h);
}


// Update view-position.
void qtractorThumbView::updateView ( int dx )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int w = QFrame::width() - 2;

	int f2 = pSession->pixelFromFrame(pSession->sessionLength())
		+ pSession->pixelFromBeat(2 * pSession->beatsPerBar());

	int cx = pTracks->trackView()->contentsX() + (dx * f2) / w;
	int cy = pTracks->trackView()->contentsY();

	if (cx < 0)
		cx = 0;

	pTracks->trackView()->setContentsPos(cx, cy);
}


// Session track-line paint method.
void qtractorThumbView::paintEvent ( QPaintEvent *pPaintEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	int w = QFrame::width()  - 2;
	int h = QFrame::height() - 2;
	if (w < 2 || h < 2) {
		QFrame::paintEvent(pPaintEvent);
		return;
	}

	QPainter painter(this);

	const QPalette& pal = QFrame::palette();
	painter.fillRect(1, 1, w, h, pal.dark().color());

	int x2, w2;
	int f2 = pSession->sessionLength();
	if (f2 > 0)
		f2 += pSession->frameFromBeat(2 * pSession->beatsPerBar());
	else
		f2 += pSession->frameFromPixel(pTracks->trackView()->viewport()->width());
	f2 /= w;
	f2++;

	int c2 = pTracks->trackView()->contentsHeight();

	if (c2 > 0) {
		int y2 = 1;
		qtractorTrack *pTrack = pSession->tracks().first();
		while (pTrack && y2 < h) {
			int h2 = ((h * pTrack->zoomHeight()) / c2);
			if (h2 < 3)
				h2 = 3;
			qtractorClip *pClip = pTrack->clips().first();
			while (pClip) {
				x2 = (pClip->clipStart()  / f2);
				w2 = (pClip->clipLength() / f2);
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
		x2 = int(pSession->loopStart()) / f2;
		if (x2 < w)
			painter.drawLine(x2, 1, x2, h);
		x2 = int(pSession->loopEnd()) / f2;
		if (x2 < w)
			painter.drawLine(x2, 1, x2, h);
	}

	// Finally, daw current edit-bound lines...
	painter.setPen(Qt::blue);
	x2 = int(pSession->editHead()) / f2;
	if (x2 < w)
		painter.drawLine(x2, 1, x2, h);
	x2 = int(pSession->editTail()) / f2;
	if (x2 < w)
		painter.drawLine(x2, 1, x2, h);

	// done for now, QFrame will need right away...
	painter.end();

	// Do it the QFrame way...
	QFrame::paintEvent(pPaintEvent);
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
