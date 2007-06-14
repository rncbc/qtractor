// qtractorMidiEditEvent.cpp
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

#include "qtractorMidiEditEvent.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditView.h"

#include "qtractorMidiSequence.h"

#include "qtractorTimeScale.h"

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <QScrollBar>
#include <QToolButton>

#if QT_VERSION >= 0x040201
#include <QStyle>
#endif


//----------------------------------------------------------------------------
// qtractorMidiEditEventScale -- MIDI event scale widget.

// Constructor.
qtractorMidiEditEventScale::qtractorMidiEditEventScale (
	qtractorMidiEditor *pEditor, QWidget *pParent ) : QWidget(pParent)
{
	m_pEditor = pEditor;

	QWidget::setMinimumWidth(22);
//	QWidget::setBackgroundRole(QPalette::Mid);

	const QFont& font = QWidget::font();
	QWidget::setFont(QFont(font.family(), font.pointSize() - 2));
}

// Default destructor.
qtractorMidiEditEventScale::~qtractorMidiEditEventScale (void)
{
}


// Paint event handler.
void qtractorMidiEditEventScale::paintEvent ( QPaintEvent * )
{
	QPainter p(this);

	p.setPen(Qt::darkGray);

	// Draw scale line labels...
	qtractorMidiEditEvent *pEditEvent = m_pEditor->editEvent();
	const QFontMetrics& fm = p.fontMetrics();
	int h  = (pEditEvent->viewport())->height();
	int w  = QWidget::width();

	int h2 = (fm.height() >> 1);
	int dy = (h >> 3);

	// Account for event view widget frame...
	h += 4;
	int y  = 2;
	int y0 = y + 1;

	int n  = 128;
	int dn = (n >> 3);

	if (pEditEvent->eventType() == qtractorMidiEvent::PITCHBEND) {
		n  = 8192;
		dn = (n >> 2);
	}

	while (y < h) {
		const QString& sLabel = QString::number(n);
		if (fm.width(sLabel) < w - 6)
			p.drawLine(w - 4, y, w - 1, y);
		if (y > y0 + (h2 << 1) && y < h - (h2 << 1)) {
			p.drawText(2, y - h2, w - 8, fm.height(),
				Qt::AlignRight | Qt::AlignVCenter, sLabel);
			y0 = y + 1;
		}
		y += dy;
		n -= dn;
	}
}


//----------------------------------------------------------------------------
// qtractorMidiEditEvent -- MIDI sequence event view widget.

// Constructor.
qtractorMidiEditEvent::qtractorMidiEditEvent (
	qtractorMidiEditor *pEditor, QWidget *pParent )
	: qtractorScrollView(pParent)
{
	m_pEditor = pEditor;

	m_eventType = qtractorMidiEvent::NOTEON;
	m_controller = 0;

	// Zoom tool widgets
	m_pHzoomOut   = new QToolButton(this);
	m_pHzoomIn    = new QToolButton(this);
	m_pHzoomReset = new QToolButton(this);

	m_pHzoomOut->setIcon(QIcon(":/icons/viewZoomOut.png"));
	m_pHzoomIn->setIcon(QIcon(":/icons/viewZoomIn.png"));
	m_pHzoomReset->setIcon(QIcon(":/icons/viewZoomTool.png"));

#if QT_VERSION >= 0x040201
	int iScrollBarExtent
		= qtractorScrollView::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	m_pHzoomReset->setFixedWidth(iScrollBarExtent);
	m_pHzoomIn->setFixedWidth(iScrollBarExtent);
	m_pHzoomOut->setFixedWidth(iScrollBarExtent);
	qtractorScrollView::addScrollBarWidget(m_pHzoomReset, Qt::AlignRight);
	qtractorScrollView::addScrollBarWidget(m_pHzoomIn,    Qt::AlignRight);
	qtractorScrollView::addScrollBarWidget(m_pHzoomOut,   Qt::AlignRight);
#endif

	m_pHzoomIn->setAutoRepeat(true);
	m_pHzoomOut->setAutoRepeat(true);

	m_pHzoomIn->setToolTip(tr("Zoom in (horizontal)"));
	m_pHzoomOut->setToolTip(tr("Zoom out (horizontal)"));
	m_pHzoomReset->setToolTip(tr("Zoom reset (horizontal)"));

	QObject::connect(m_pHzoomIn, SIGNAL(clicked()),
		m_pEditor, SLOT(horizontalZoomInSlot()));
	QObject::connect(m_pHzoomOut, SIGNAL(clicked()),
		m_pEditor, SLOT(horizontalZoomOutSlot()));
	QObject::connect(m_pHzoomReset, SIGNAL(clicked()),
		m_pEditor, SLOT(horizontalZoomResetSlot()));

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::ClickFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
//	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);
	qtractorScrollView::setMouseTracking(true);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorMidiEditEvent::~qtractorMidiEditEvent (void)
{
}


// Rectangular contents update.
void qtractorMidiEditEvent::updateContents ( const QRect& rect )
{
	updatePixmap(
		qtractorScrollView::contentsX(),
		qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorMidiEditEvent::updateContents (void)
{
	updatePixmap(
		qtractorScrollView::contentsX(),
		qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();
}


// Current event selection accessors.
void qtractorMidiEditEvent::setEventType (
	qtractorMidiEvent::EventType eventType )
{
	m_eventType = eventType;

	m_pEditor->updateContents();
}

qtractorMidiEvent::EventType qtractorMidiEditEvent::eventType (void) const
{
	return m_eventType;
}


void qtractorMidiEditEvent::setController ( unsigned char controller )
{
	m_controller = controller;

	m_pEditor->updateContents();
}

unsigned char qtractorMidiEditEvent::controller (void) const
{
	return m_controller;
}


// Resize event handler.
void qtractorMidiEditEvent::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorScrollView::resizeEvent(pResizeEvent);

	// Scrollbar/tools layout management.
#if QT_VERSION < 0x040201
	const QSize& size = qtractorScrollView::size();
	QScrollBar *pHScrollBar = qtractorScrollView::horizontalScrollBar();
	if (pHScrollBar->isVisible()) {
		int w = size.width();
		int h = pHScrollBar->height(); 
		int y = size.height() - h - 1;
		pHScrollBar->setFixedWidth(w - h * 3 - 2);
		if (m_pHzoomIn)
			m_pHzoomIn->setGeometry(w - h * 3, y, h, h);
		if (m_pHzoomOut)
			m_pHzoomOut->setGeometry(w - h * 2, y, h, h);
		if (m_pHzoomReset)
			m_pHzoomReset->setGeometry(w - h - 1, y, h, h);
	}
#endif

	// FIXME: Prevent any overlay selection during resizing this,
	// as the overlay rectangles will certainly be wrong...
	if (m_pEditor->isSelected()) {
		m_pEditor->updateContents();
	} else {
		updateContents();
	}
}


// (Re)create the complete view pixmap.
void qtractorMidiEditEvent::updatePixmap ( int cx, int /*cy*/ )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	int w = pViewport->width();
	int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.base().color());

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	QPainter p(&m_pixmap);
	p.initFrom(this);

	const QColor& rgbFore  = m_pEditor->foreground();
	const QColor& rgbBack  = m_pEditor->background();
	const QColor& rgbDark  = pal.dark().color();
	const QColor& rgbLight = pal.mid().color();

	// Draw horizontal lines...
	p.setPen(rgbLight);
	int dy = (h >> 3);
	int  y = 0;
	while (y < h) {
		p.drawLine(0, y, w, y);
		y += dy;
	}

	// Account for the editing offset:
	int dx = cx + pTimeScale->pixelFromFrame(m_pEditor->offset());

	// Draw vertical grid lines...
	unsigned short iBeat = pTimeScale->beatFromPixel(dx);
	unsigned short iTicksPerBeat  = pTimeScale->ticksPerBeat();
	unsigned short iPixelsPerBeat = pTimeScale->pixelsPerBeat();
	unsigned long  iTickFromBeat  = iBeat * iTicksPerBeat;
	int x = pTimeScale->pixelFromTick(iTickFromBeat) - dx;
	while (x < w) {
		if (x >= 0) {
			bool bBeatIsBar = pTimeScale->beatIsBar(iBeat);
			if (bBeatIsBar) {
				p.setPen(rgbLight);
				p.drawLine(x, 0, x, h);
			}
			if (bBeatIsBar || iPixelsPerBeat > 16) {
				p.setPen(rgbDark);
				p.drawLine(x - 1, 0, x - 1, h);
			}
		}
		iTickFromBeat += iTicksPerBeat;
		x = pTimeScale->pixelFromTick(iTickFromBeat) - dx;
		iBeat++;
	}

	//
	// Draw the sequence events...
	//

	qtractorMidiSequence *pSeq = m_pEditor->sequence();
	if (pSeq == NULL)
		return;

	unsigned long iTickStart = pTimeScale->tickFromPixel(cx);
	unsigned long iTickEnd   = iTickStart + pTimeScale->tickFromPixel(w);

	// This is the zero-line...
	int y0 = h;
	if (m_eventType == qtractorMidiEvent::PITCHBEND)
		y0 = (dy << 2);

	p.setPen(rgbLight);
	p.drawLine(0, y0 - 1, w, y0 - 1);
	p.setPen(rgbDark);
	p.drawLine(0, y0, w, y0);

	p.setPen(rgbFore);
	p.setBrush(rgbBack);

	bool bController = (m_eventType == qtractorMidiEvent::CONTROLLER);
	qtractorMidiEvent *pEvent = m_pEditor->seekEvent(iTickStart);
	while (pEvent && pEvent->time() < iTickEnd) {
		if (pEvent->type() == m_eventType	// Filter event type!
			&& (!bController || pEvent->controller() == m_controller)
			&& pEvent->time() + pEvent->duration() >= iTickStart) {
			if (m_eventType == qtractorMidiEvent::PITCHBEND)
				y = y0 - (y0 * pEvent->pitchBend()) / 8192;
			else
				y = y0 - (y0 * pEvent->value()) / 128;
			x = pTimeScale->pixelFromTick(pEvent->time()) - cx;
			int w1 = pTimeScale->pixelFromTick(pEvent->duration()) + 1;
			if (w1 < 5)
				w1 = 5;
			if (y < y0) {
				p.fillRect(x, y, w1, y0 - y, rgbFore);
				p.fillRect(x + 1, y + 1, w1 - 4, y0 - y - 2, rgbBack);
			} else if (y > y0) {
				p.fillRect(x, y0, w1, y - y0, rgbFore);
				p.fillRect(x + 1, y0 + 1, w1 - 4, y - y0 - 2, rgbBack);
			} else {
				p.fillRect(x, y0 - 2, w1, 4, rgbFore);
				p.fillRect(x + 1, y0 - 1, w1 - 4, 2, rgbBack);
			}
		}
		pEvent = pEvent->next();
	}
}


// Draw the time scale.
void qtractorMidiEditEvent::drawContents ( QPainter *pPainter, const QRect& rect )
{
	pPainter->drawPixmap(rect, m_pixmap, rect);

	m_pEditor->paintDragState(this, pPainter);
}


// To have event view in h-sync with main view.
void qtractorMidiEditEvent::contentsXMovingSlot ( int cx, int /*cy*/ )
{
	if (qtractorScrollView::contentsX() != cx)
		qtractorScrollView::setContentsPos(cx, qtractorScrollView::contentsY());
}


// Keyboard event handler.
void qtractorMidiEditEvent::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (pKeyEvent->key() == Qt::Key_Escape)
		m_pEditor->resetDragState(this);

	m_pEditor->editView()->keyPressEvent(pKeyEvent);
}


// Handle item selection/dragging -- mouse button press.
void qtractorMidiEditEvent::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse press...
	qtractorScrollView::mousePressEvent(pMouseEvent);

	// Maybe start the drag-move-selection dance?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	// Remember what and where we'll be dragging/selecting...
	m_pEditor->dragMoveStart(this, pos, pMouseEvent->modifiers());

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditEvent::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse move...
	qtractorScrollView::mouseMoveEvent(pMouseEvent);

	// Are we already moving/dragging something?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	m_pEditor->dragMoveUpdate(this, pos, pMouseEvent->modifiers());
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditEvent::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	// Were we moving/dragging something?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	m_pEditor->dragMoveCommit(this, pos, pMouseEvent->modifiers());

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Mouse leave event.
void qtractorMidiEditEvent::leaveEvent ( QEvent */*pEvent */ )
{
	m_pEditor->dragMoveLeave(this);
}


// end of qtractorMidiEditEvent.cpp
