// qtractorMidiEditTime.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifdef QTRACTOR_TEST
#include "qtractorTimeScale.h"
#else
#include "qtractorSession.h"
#include "qtractorMainForm.h"
#endif

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
	m_dragState = DragNone;

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	qtractorScrollView::setFrameStyle(QFrame::Panel | QFrame::Raised);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::ClickFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
//	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);

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

	unsigned short iBeat = pTimeScale->beatFromPixel(dx);
	unsigned long iFrameFromBeat = pTimeScale->frameFromBeat(iBeat);
	unsigned long iFramesPerBeat = pTimeScale->frameFromBeat(1);
	unsigned int  iPixelsPerBeat = pTimeScale->pixelFromBeat(1);
	x = pTimeScale->pixelFromFrame(iFrameFromBeat) - (dx);
	while (x < w) {
		bool bBeatIsBar = pTimeScale->beatIsBar(iBeat);
		if (bBeatIsBar) {
			y1 = 0;
			p.setPen(pal.windowText().color());
			p.drawText(x + 2, y1 + fm.ascent(), QString::number(
				1 + (iBeat / pTimeScale->beatsPerBar())));
		} else {
			y1 = (y2 >> 1);
		}
		if (bBeatIsBar || iPixelsPerBeat > 16) {
			p.setPen(pal.mid().color());
			p.drawLine(x, y1, x, y2);
			p.setPen(pal.light().color());
			p.drawLine(x + 1, y1, x + 1, y2);
		}
		iFrameFromBeat += iFramesPerBeat;
		x = pTimeScale->pixelFromFrame(iFrameFromBeat) - dx;
		iBeat++;
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

	// Draw special play-head header...
	int cx = qtractorScrollView::contentsX();
	int h = qtractorScrollView::height() - 4;
	int d = (h >> 2);
	int x = m_pEditor->playHeadX() - cx;
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
	int h = qtractorScrollView::height() - 4;
	int d = (h >> 1);
	QRect rect(0, h - d, d << 1, d);

	// Check play-head header...
	rect.moveLeft(m_pEditor->playHeadX() - d);
	if (rect.contains(pos)) {
		m_dragState = DragPlayHead;
		return true;
	}

	// Nothing.
	return false;
}


// Handle item selection/dragging -- mouse button press.
void qtractorMidiEditTime::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	m_dragState = DragNone;

	// Which mouse state?
	const bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));
	// Make sure we'll reset selection...
	if (!bModifier)
		m_pEditor->selectAll(false);

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
//	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
//	unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
//		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
	switch (pMouseEvent->button()) {
	case Qt::LeftButton:
		// Remember what and where we'll be dragging/selecting...
		m_dragState = DragStart;
		m_posDrag   = pos;
		// Try to catch mouse clicks over the cursor heads...
		if (dragHeadStart(m_posDrag))
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		break;
	default:
		break;
	}

	qtractorScrollView::mousePressEvent(pMouseEvent);

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditTime::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
#ifndef QTRACTOR_TEST
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
#endif

	// Are we already moving/dragging something?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
	int y = m_pEditor->editView()->contentsY();
	switch (m_dragState) {
	case DragSelect:
		// Rubber-band selection...
		m_rectDrag.setRight(pos.x());
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->selectRect(m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, false);
		break;
	case DragPlayHead:
		// Play-head positioning...
		m_pEditor->editView()->ensureVisible(pos.x(), y, 16, 0);
		m_pEditor->setPlayHead(iFrame);
#ifndef QTRACTOR_TEST
		// Let the change get some immediate visual feedback...
		pMainForm->updateTransportTime(iFrame);
#endif
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
			m_dragState = DragSelect;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		}
		// Fall thru...
	case DragNone:
	default:
		break;
	}

	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditTime::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

#ifndef QTRACTOR_TEST
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;
#endif

	// Which mouse state?
	const bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));

	// Direct snap positioning...
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(m_posDrag.x() > 0 ? m_posDrag.x() : 0));
	switch (m_dragState) {
	case DragSelect:
		// Do the final range selection...
		m_pEditor->selectRect(m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, true);
		break;
	case DragPlayHead:
#ifndef QTRACTOR_TEST
		// Play-head positioning commit...
		pSession->setPlayHead(m_pEditor->playHead());
#endif
		// Not quite a selection, rather just
		// for immediate visual feedback...
		m_pEditor->selectionChangeNotify();
		break;
	case DragStart:
		// Left-button indirect positioning...
		if (bModifier) {
			// Playhead positioning...
			m_pEditor->setPlayHead(iFrame);
#ifndef QTRACTOR_TEST
			// Immediately commited...
			pSession->setPlayHead(iFrame);
#endif
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


// Keyboard event handler.
void qtractorMidiEditTime::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorMidiEditTime::keyPressEvent(key=%d)\n", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
#ifndef QTRACTOR_TEST
		// Restore uncommitted play-head position?...
		if (m_dragState == DragPlayHead) {
			qtractorSession  *pSession  = NULL;
			qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
			if (pMainForm)
				pSession = pMainForm->session();
			if (pSession)
				m_pEditor->setPlayHead(pSession->playHead());
		}
#endif
		resetDragState();
		break;
	default:
		m_pEditor->editView()->keyPressEvent(pKeyEvent);
		break;
	}
}


// Reset drag/select state.
void qtractorMidiEditTime::resetDragState (void)
{
	// Cancel any dragging out there...
	switch (m_dragState) {
	case DragSelect:
		qtractorScrollView::updateContents();
		// Fall thru...
	case DragPlayHead:
		qtractorScrollView::unsetCursor();
		// Fall thru again...
	case DragNone:
	default:
		break;
	}

	// Also get rid of any meta-breadcrumbs...
	m_pEditor->resetDragState(this);

	// Force null state.
	m_dragState = DragNone;
	
	// HACK: give focus to track-view... 
	m_pEditor->editView()->setFocus();
}


// end of qtractorMidiEditTime.cpp
