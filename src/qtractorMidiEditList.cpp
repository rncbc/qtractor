// qtractorMidiEditList.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEditList.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditView.h"

#include <QApplication>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <QToolTip>

#ifdef CONFIG_GRADIENT
#include <QLinearGradient>
#endif


//----------------------------------------------------------------------------
// qtractorMidiEditList -- MIDI sequence key scale widget.

// Constructor.
qtractorMidiEditList::qtractorMidiEditList (
	qtractorMidiEditor *pEditor, QWidget *pParent )
	: qtractorScrollView(pParent)
{
	m_pEditor = pEditor;

	m_iItemHeight = ItemHeightBase;
	m_dragState = DragNone;
	m_iNoteOn  = -1;
	m_iNoteVel = -1;

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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

	// Trap for help/tool-tips and leave events.
	qtractorScrollView::viewport()->installEventFilter(this);
}


// Destructor.
qtractorMidiEditList::~qtractorMidiEditList (void)
{
}


// Item height methods.
void qtractorMidiEditList::setItemHeight ( unsigned short iItemHeight )
{
	if (iItemHeight > ItemHeightMax)
		iItemHeight = ItemHeightMax;
	else
	if (iItemHeight < ItemHeightMin)
		iItemHeight = ItemHeightMin;
	if (iItemHeight == m_iItemHeight)
		return;

	m_iItemHeight = iItemHeight;

	updateContentsHeight();
}

unsigned short qtractorMidiEditList::itemHeight (void) const
{
	return m_iItemHeight;
}


// Update key-list content height.
void qtractorMidiEditList::updateContentsHeight (void)
{
	const int iContentsHeight = 128 * m_iItemHeight;

	qtractorScrollView::resizeContents(
		qtractorScrollView::contentsWidth(), iContentsHeight);

	// Force an update on other views too...
	m_pEditor->editView()->resizeContents(
		m_pEditor->editView()->contentsWidth(), iContentsHeight);
//	m_pEditor->editView()->updateContents();
}


// Rectangular contents update.
void qtractorMidiEditList::updateContents ( const QRect& rect )
{
	updatePixmap(
		qtractorScrollView::contentsX(),
		qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorMidiEditList::updateContents (void)
{
	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();
}


// Resize event handler.
void qtractorMidiEditList::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorScrollView::resizeEvent(pResizeEvent);
	updateContents();
}


// (Re)create the complete view pixmap.
void qtractorMidiEditList::updatePixmap ( int /*cx*/, int cy )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();
	const bool bDark = (pal.base().color().value() < 128);
	const QColor& rgbLine  = pal.mid().color();
	const QColor& rgbLight = QColor(Qt::white).darker(bDark ? 240 : 160);
	const QColor& rgbDark  = QColor(Qt::black).lighter(bDark ? 120 : 180);

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.window().color());
//	m_pixmap.fill(m_pEditor->background().darker(140));

	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	const int ch = qtractorScrollView::contentsHeight() - cy;

	const float hk = (12.0f * m_iItemHeight) / 7.0f;	// Key height.
	const int wk = (w << 1) / 3;						// Key width.
	const int w3 = (wk * 6) / 10;

	const int q0 = (cy / m_iItemHeight);
	const int n0 = 127 - q0;
	const int y0 = q0 * m_iItemHeight - cy;

	int n, k, y, x = w - wk;

	// Draw horizontal key-lines...
	painter.setPen(rgbLine);
	painter.setBrush(rgbDark);

#ifdef CONFIG_GRADIENT
	QLinearGradient gradLight(x, 0, w, 0);
	gradLight.setColorAt(0.0f, rgbLight);
	gradLight.setColorAt(0.1f, rgbLight.lighter());
	painter.fillRect(x, 0, wk, h, gradLight);
//	painter.setBrush(gradLight);
#else
//	painter.setBrush(rgbLight.lighter());
	painter.fillRect(x, 0, wk, h, rgbLight.lighter());
#endif

	y = y0;
	n = n0;
	while (y < h && y < ch) {
		k = (n % 12);
		if (k >= 5) ++k;
		if ((k & 1) == 0) {
			int y1 = ch - int(hk * ((n / 12) * 7 + (k >> 1)));
			painter.drawLine(x, y1, w, y1);
			if (k == 0) {
				painter.setPen(Qt::darkGray);
				y1 = y + m_iItemHeight;
				painter.drawText(2, y1 - 2, tr("C%1").arg((n / 12) - 1));
				painter.setPen(rgbLine);
			//	p.drawLine(0, y1, x, y1);
			}
		}
		y += m_iItemHeight;
		--n;
	}

#ifdef CONFIG_GRADIENT
	QLinearGradient gradDark(x, 0, x + w3, 0);
	gradDark.setColorAt(0.0, rgbLight);
	gradDark.setColorAt(0.4, rgbDark);
	gradDark.setColorAt(0.96, rgbDark);
	gradDark.setColorAt(0.98, rgbLight);
	gradDark.setColorAt(1.0, rgbDark);
	painter.setBrush(gradDark);
#else
	painter.setBrush(rgbDark);
#endif

	y = y0;
	n = n0;
	while (y < h && y < ch) {
		k = (n % 12);
		if (k >= 5) ++k;
		if (k & 1)
			painter.drawRect(x, y, w3, m_iItemHeight);
		y += m_iItemHeight;
		--n;
	}

	painter.drawLine(x, 0, x, h);

	if (y > ch)
		painter.fillRect(0, ch, w, h - ch, pal.dark().color());
}


// Draw the time scale.
void qtractorMidiEditList::drawContents ( QPainter *pPainter, const QRect& rect )
{
	pPainter->drawPixmap(rect, m_pixmap, rect);

	// Are we sticking in some note?
	if (m_iNoteOn >= 0) {
		pPainter->fillRect(QRect(
			contentsToViewport(m_rectNote.topLeft()),
			m_rectNote.size()),	m_iNoteVel > 0
				? QColor(255,   0, 120, 120)
				: QColor(120, 120, 255, 120));
	}
}


// To have keyline in v-sync with main view.
void qtractorMidiEditList::contentsYMovingSlot ( int /*cx*/, int cy )
{
	if (qtractorScrollView::contentsY() != cy)
		qtractorScrollView::setContentsPos(qtractorScrollView::contentsX(), cy);
}


// Piano keyboard note-on handler.
void qtractorMidiEditList::dragNoteOn ( int iNote, int iVelocity )
{
	// If it ain't changed we won't change it ;)
	if (iNote == m_iNoteOn && m_iNoteVel >= iVelocity)
		return;

	// Were we pending on some sounding note?
	dragNoteOff();

	// Now for the sounding new one...
	if (iNote >= 0) {
		// This stands for the keyboard area...
		QWidget *pViewport = qtractorScrollView::viewport();
		const int w = pViewport->width();
		int wk = (w << 1) / 3;
		int xk = w - wk;
	#if 0
		float yk, hk;
		int k  = (iNote % 12);
		if (k >= 5) ++k;
		if ((k % 2) == 0) {
			hk = (12.0f * m_iItemHeight) / 7.0f;
			yk = (128 * m_iItemHeight) - ((iNote / 12) * 7 + (k / 2) + 1) * hk + 2;
		} else {
			hk = m_iItemHeight;
			yk = ((127 - iNote) * hk) + 1;
			wk = (wk * 6) / 10;
		}
	#else
		const int hk = m_iItemHeight;
		int k = (iNote % 12);
		if (k >= 5) ++k;
		if (k % 2)
			wk = (wk * 6) / 10;
		const int yk = ((127 - iNote) * hk) + 1;
	#endif
		// This is the new note on...
		m_iNoteOn = iNote;
		m_iNoteVel = iVelocity;
		m_rectNote.setRect(xk, yk, wk, hk);
		if (m_iNoteVel > 0)
			m_pEditor->sendNote(m_iNoteOn, m_iNoteVel);
		// Otherwise, reset any pending note...
		qtractorScrollView::viewport()->update(
			QRect(contentsToViewport(m_rectNote.topLeft()),
			m_rectNote.size()));
	}
}


// Piano keyboard note-on position handler.
void qtractorMidiEditList::dragNoteOn ( const QPoint& pos, int iVelocity )
{
	// Compute new key cordinates...
	const int ch = qtractorScrollView::contentsHeight();
	dragNoteOn((ch - pos.y()) / m_iItemHeight, iVelocity);
}


// Piano keyboard note-on handler.
void qtractorMidiEditList::dragNoteOff (void)
{
	if (m_iNoteOn < 0)
		return;

	// Turn off old note...
	if (m_iNoteVel > 0)
		m_pEditor->sendNote(m_iNoteOn, 0);

	m_iNoteOn = m_iNoteVel = -1;

	qtractorScrollView::viewport()->update(
		QRect(contentsToViewport(m_rectNote.topLeft()),
		m_rectNote.size()));
}


// Handle item selection/dragging -- mouse button press.
void qtractorMidiEditList::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	m_dragState = DragNone;

	// Which mouse state?
	const bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));
	// Make sure we'll reset selection...
	if (!bModifier)
		m_pEditor->selectAll(m_pEditor->editView(), false);

	// Direct snap positioning...
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	switch (pMouseEvent->button()) {
	case Qt::LeftButton:
		// Remember what and where we'll be dragging/selecting...
		m_dragState = DragStart;
		m_posDrag   = pos;
		// Are we keying in some keyboard?
		dragNoteOn(pos);
		break;
	default:
		break;
	}

//	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditList::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Are we already moving/dragging something?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	const int x = m_pEditor->editView()->contentsX();
	switch (m_dragState) {
	case DragSelect:
		// Rubber-band selection...
		m_rectDrag.setBottom(pos.y());
		m_pEditor->editView()->ensureVisible(x, pos.y(), 0, 16);
		m_pEditor->selectRect(m_pEditor->editView(), m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, false);
		// Are we keying in some keyboard?
		dragNoteOn(pos);
		break;
	case DragStart:
		// Rubber-band starting...
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// We'll start dragging alright...
			const int w = m_pEditor->editView()->contentsWidth();
			m_rectDrag.setTop(m_posDrag.y());
			m_rectDrag.setLeft(0);
			m_rectDrag.setRight(w);
			m_rectDrag.setBottom(pos.y());
			m_dragState = DragSelect;
			qtractorScrollView::setCursor(QCursor(Qt::SizeVerCursor));
		}
		// Fall thru...
	case DragNone:
	default:
		// Are we hovering in some keyboard?
		dragNoteOn(pos, -1);
		break;
	}

//	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditList::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
//	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	// Direct snap positioning...
	switch (m_dragState) {
	case DragSelect:
		// Do the final range selection...
		m_pEditor->selectRect(m_pEditor->editView(), m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, true);
		// Keyboard notes are reset later anyway...
		break;
	case DragStart:
	case DragNone:
	default:
		break;
	}

	// Clean up.
	resetDragState();
}


// Handle zoom with mouse wheel.
void qtractorMidiEditList::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers() & Qt::ControlModifier) {
		const int delta = pWheelEvent->delta();
		if (delta > 0)
			m_pEditor->zoomIn();
		else
			m_pEditor->zoomOut();
	}
	else qtractorScrollView::wheelEvent(pWheelEvent);
}


// Keyboard event handler.
void qtractorMidiEditList::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (pKeyEvent->key() == Qt::Key_Escape)
		resetDragState();

	if (!m_pEditor->keyPress(this, pKeyEvent->key(), pKeyEvent->modifiers()))
		qtractorScrollView::keyPressEvent(pKeyEvent);
}


// Reset drag/select/move state.
void qtractorMidiEditList::resetDragState (void)
{
	// Were we stuck on some keyboard note?
	dragNoteOff();

	// Cancel any dragging out there...
	switch (m_dragState) {
	case DragSelect:
		qtractorScrollView::updateContents();
		// Fall thru...
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


// Trap for help/tool-tip events.
bool qtractorMidiEditList::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	if (static_cast<QWidget *> (pObject) == qtractorScrollView::viewport()) {
		if (pEvent->type() == QEvent::ToolTip) {
			QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
			if (pHelpEvent) {
				const QPoint& pos
					= qtractorScrollView::viewportToContents(pHelpEvent->pos());
				const QString sToolTip("%1 (%2)");
				const int ch = qtractorScrollView::contentsHeight();
				const int note = (ch - pos.y()) / m_iItemHeight;
				QToolTip::showText(pHelpEvent->globalPos(),
					sToolTip.arg(m_pEditor->noteName(note)).arg(note));
				return true;
			}
		}
		else
		if (pEvent->type() == QEvent::Leave) {
			dragNoteOff();
			return true;
		}
	}

	// Not handled here.
	return qtractorScrollView::eventFilter(pObject, pEvent);
}


// end of qtractorMidiEditList.cpp
