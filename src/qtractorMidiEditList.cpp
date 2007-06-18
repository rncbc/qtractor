// qtractorMidiEditList.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiEditList.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditView.h"

#include <QApplication>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>


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
	m_iNoteOn = -1;

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::ClickFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
//	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 2));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));
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
	int iContentsHeight = 128 * m_iItemHeight;

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
	int w = pViewport->width();
	int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.base().color());

	QPainter p(&m_pixmap);
	p.initFrom(this);

	// Draw horizontal key-lines...
	p.setPen(pal.mid().color());
	p.setBrush(pal.shadow().color());

	int ch = qtractorScrollView::contentsHeight() - cy;

	int x = (w << 1) / 3;
	p.drawLine(x, 0, x, ch);

	int q = (cy / m_iItemHeight);
	int n = 127 - q;
	int y = q * m_iItemHeight - cy;
	while (y < h && y < ch) {
		int k = (n % 12);
		if (k == 1 || k == 3 || k == 6 || k == 8 || k == 10) {
			p.drawRect(x, y + 1, w, m_iItemHeight); 
		} else {
			p.drawLine(x, y, w, y);
			if (k == 0) {
				int y1 = y + m_iItemHeight;
				p.drawText(2, y1 - 2, tr("C%1").arg((n / 12) - 2));
				p.drawLine(0, y1, x, y1);
			}
		}
		y += m_iItemHeight;
		n--;
	}

	if (y > ch)
		p.fillRect(0, ch, w, h - ch, pal.dark().color());
}


// Draw the time scale.
void qtractorMidiEditList::drawContents ( QPainter *pPainter, const QRect& rect )
{
	pPainter->drawPixmap(rect, m_pixmap, rect);

	// Are we sticking in some note?
	if (m_iNoteOn >= 0) {
		pPainter->fillRect(QRect(
			contentsToViewport(m_rectNote.topLeft()),
			m_rectNote.size()), QColor(255, 0, 120, 120));
	}
}


// To have keyline in v-sync with main view.
void qtractorMidiEditList::contentsYMovingSlot ( int /*cx*/, int cy )
{
	if (qtractorScrollView::contentsY() != cy)
		qtractorScrollView::setContentsPos(qtractorScrollView::contentsX(), cy);
}


// Piano keyboard handlers.
void qtractorMidiEditList::dragNoteOn ( int iNote, int iVelocity )
{
	// If it ain't changed we won't change it ;)
	if (iNote == m_iNoteOn)
		return;

	// Were we pending on some sounding note?
	if (m_iNoteOn >= 0) {
		// Turn off old note...
		m_pEditor->sendNote(m_iNoteOn, 0);
		m_iNoteOn = -1;
		qtractorScrollView::viewport()->update(
			QRect(contentsToViewport(m_rectNote.topLeft()),
			m_rectNote.size()));
	}

	// Now for the sounding new one...
	if (iNote >= 0) {
		// This stands for the keyboard area...
		QWidget *pViewport = qtractorScrollView::viewport();
		int w = pViewport->width();
		int x = ((w << 1) / 3);
		int y = ((127 - iNote) * m_iItemHeight) + 1;
		// This is the new note on...
		m_iNoteOn = iNote;
		m_rectNote.setRect(x, y, w - x, m_iItemHeight);
		m_pEditor->sendNote(m_iNoteOn, iVelocity);
		qtractorScrollView::viewport()->update(
			QRect(contentsToViewport(m_rectNote.topLeft()),
			m_rectNote.size()));
		// Otherwise, reset any pending note...
	}
}

void qtractorMidiEditList::dragNoteOn ( const QPoint& pos, int iVelocity )
{
	// This stands for the keyboard area...
	QWidget *pViewport = qtractorScrollView::viewport();
	int w = pViewport->width();
	int x = ((w << 1) / 3);

	// Are we on it?
	if (pos.x() < x || pos.x() > w)
		return;

	// Compute new key cordinates...
	dragNoteOn(127 - ((pos.y() - 1) / m_iItemHeight), iVelocity);
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
		m_pEditor->selectAll(false);

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

	qtractorScrollView::mousePressEvent(pMouseEvent);

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditList::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Are we already moving/dragging something?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	int x = m_pEditor->editView()->contentsX();
	switch (m_dragState) {
	case DragSelect:
		// Rubber-band selection...
		m_rectDrag.setBottom(pos.y());
		m_pEditor->editView()->ensureVisible(x, pos.y(), 0, 16);
		m_pEditor->selectRect(m_rectDrag,
			pMouseEvent->modifiers() & Qt::ControlModifier, false);
		// Are we keying in some keyboard?
		dragNoteOn(pos);
		break;
	case DragStart:
		// Rubber-band starting...
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// We'll start dragging alright...
			int w = m_pEditor->editView()->contentsWidth();
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
		break;
	}

	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditList::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	// Direct snap positioning...
	switch (m_dragState) {
	case DragSelect:
		// Do the final range selection...
		m_pEditor->selectRect(m_rectDrag,
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


// Keyboard event handler.
void qtractorMidiEditList::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (pKeyEvent->key() == Qt::Key_Escape)
		resetDragState();

	m_pEditor->editView()->keyPressEvent(pKeyEvent);
}


// Reset drag/select/move state.
void qtractorMidiEditList::resetDragState (void)
{
	// Were we stuck on some keyboard note?
	dragNoteOn(-1);

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


// end of qtractorMidiEditList.cpp
