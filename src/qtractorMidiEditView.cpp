// qtractorMidiEditView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEditView.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditList.h"
#include "qtractorMidiEditTime.h"
#include "qtractorMidiEditEvent.h"

#include "qtractorMidiSequence.h"

#include "qtractorSession.h"

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <QScrollBar>
#include <QToolButton>

#if QT_VERSION >= 0x040201
#include <QStyle>
#endif

#if QT_VERSION < 0x040300
#define lighter(x)	light(x)
#define darker(x)	dark(x)
#endif


//----------------------------------------------------------------------------
// qtractorMidiEditView -- MIDI sequence main view widget.

// Constructor.
qtractorMidiEditView::qtractorMidiEditView (
	qtractorMidiEditor *pEditor, QWidget *pParent )
	: qtractorScrollView(pParent)
{
	m_pEditor = pEditor;

	m_eventType = qtractorMidiEvent::NOTEON;

	// Zoom tool widgets
	m_pVzoomIn    = new QToolButton(this);
	m_pVzoomOut   = new QToolButton(this);
	m_pVzoomReset = new QToolButton(this);

	m_pVzoomIn->setIcon(QIcon(":/images/viewZoomIn.png"));
	m_pVzoomOut->setIcon(QIcon(":/images/viewZoomOut.png"));
	m_pVzoomReset->setIcon(QIcon(":/images/viewZoomTool.png"));

	m_pVzoomIn->setAutoRepeat(true);
	m_pVzoomOut->setAutoRepeat(true);

	m_pVzoomIn->setToolTip(tr("Zoom in (vertical)"));
	m_pVzoomOut->setToolTip(tr("Zoom out (vertical)"));
	m_pVzoomReset->setToolTip(tr("Zoom reset (vertical)"));

#if QT_VERSION >= 0x040201
	int iScrollBarExtent
		= qtractorScrollView::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	m_pVzoomReset->setFixedHeight(iScrollBarExtent);
	m_pVzoomOut->setFixedHeight(iScrollBarExtent);
	m_pVzoomIn->setFixedHeight(iScrollBarExtent);
	qtractorScrollView::addScrollBarWidget(m_pVzoomReset, Qt::AlignBottom);
	qtractorScrollView::addScrollBarWidget(m_pVzoomOut,   Qt::AlignBottom);
	qtractorScrollView::addScrollBarWidget(m_pVzoomIn,    Qt::AlignBottom);
#endif

	QObject::connect(m_pVzoomIn, SIGNAL(clicked()),
		m_pEditor, SLOT(verticalZoomInSlot()));
	QObject::connect(m_pVzoomOut, SIGNAL(clicked()),
		m_pEditor, SLOT(verticalZoomOutSlot()));
	QObject::connect(m_pVzoomReset, SIGNAL(clicked()),
		m_pEditor, SLOT(verticalZoomResetSlot()));

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::StrongFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
//	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);
	qtractorScrollView::setMouseTracking(true);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));

	// Trap for help/tool-tips and leave events.
	qtractorScrollView::viewport()->installEventFilter(this);
}


// Destructor.
qtractorMidiEditView::~qtractorMidiEditView (void)
{
}


// Scrollbar/tools layout management.
void qtractorMidiEditView::setVBarGeometry ( QScrollBar& vbar,
	int x, int y, int w, int h )
{
	vbar.setGeometry(x, y, w, h - w * 2);
	if (m_pVzoomIn)
		m_pVzoomIn->setGeometry(x, y + h - w * 2, w, w);
	if (m_pVzoomOut)
		m_pVzoomOut->setGeometry(x, y + h - w, w, w);
}


// Update track view content height.
void qtractorMidiEditView::updateContentsHeight (void)
{
	// Do the contents resize thing...
	qtractorScrollView::resizeContents(
		qtractorScrollView::contentsWidth(),
		m_pEditor->editList()->contentsHeight());
	
//	updateContents();
}


// Update track view content width.
void qtractorMidiEditView::updateContentsWidth ( int iContentsWidth )
{
	// Do the contents resize thing...
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale) {
		qtractorMidiSequence *pSeq = m_pEditor->sequence();
		if (pSeq) {
			unsigned long t0 = pTimeScale->tickFromFrame(m_pEditor->offset());
			int x0 = pTimeScale->pixelFromFrame(m_pEditor->offset());
			int w0 = pTimeScale->pixelFromTick(t0 + pSeq->duration()) - x0;
			if (iContentsWidth < w0)
				iContentsWidth = w0;
		}
		iContentsWidth += pTimeScale->pixelFromBeat(
			2 * pTimeScale->beatsPerBar()) + qtractorScrollView::width();
	}

	qtractorScrollView::resizeContents(
		iContentsWidth, qtractorScrollView::contentsHeight());

	// Force an update on other views too...
	m_pEditor->editTime()->resizeContents(
		iContentsWidth + 100, m_pEditor->editTime()->contentsHeight());
//	m_pEditor->editTime()->updateContents();

	m_pEditor->editEvent()->resizeContents(
		iContentsWidth, m_pEditor->editEvent()->contentsHeight());
//	m_pEditor->editEvent()->updateContents();
}


// Local rectangular contents update.
void qtractorMidiEditView::updateContents ( const QRect& rect )
{
	updatePixmap(
		qtractorScrollView::contentsX(),
		qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorMidiEditView::updateContents (void)
{
	updatePixmap(
		qtractorScrollView::contentsX(),
		qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();
}


// Current event selection accessors.
void qtractorMidiEditView::setEventType (
	qtractorMidiEvent::EventType eventType )
{
	m_eventType = eventType;

	m_pEditor->updateContents();
}

qtractorMidiEvent::EventType qtractorMidiEditView::eventType (void) const
{
	return m_eventType;
}


// Resize event handler.
void qtractorMidiEditView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorScrollView::resizeEvent(pResizeEvent);

	// Scrollbar/tools layout management.
	const QSize& size = qtractorScrollView::size();
	QScrollBar *pVScrollBar = qtractorScrollView::verticalScrollBar();
	int w = pVScrollBar->width(); 

#if QT_VERSION < 0x040201
	if (pVScrollBar->isVisible()) {
		int h = size.height();
		int x = size.width() - w - 1;
		pVScrollBar->setFixedHeight(h - w * 3 - 2);
		if (m_pVzoomIn)
			m_pVzoomIn->setGeometry(x, h - w * 3, w, w);
		if (m_pVzoomOut)
			m_pVzoomOut->setGeometry(x, h - w * 2, w, w);
		if (m_pVzoomReset)
			m_pVzoomReset->setGeometry(x, h - w - 1, w, w);
	}
#endif

	updateContents();

	m_pEditor->editEventScale()->setFixedWidth(
		m_pEditor->width() - size.width());
	m_pEditor->editEventFrame()->setFixedWidth(w);
}


// (Re)create the complete track view pixmap.
void qtractorMidiEditView::updatePixmap ( int cx, int cy )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	int w = pViewport->width();
	int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	const QColor& rgbBase  = pal.base().color();
	const QColor& rgbFore  = m_pEditor->foreground();
	const QColor& rgbBack  = m_pEditor->background();
	const QColor& rgbDark  = pal.dark().color();
	const QColor& rgbLight = pal.mid().color();
	const QColor& rgbSharp = rgbBase.darker(110);

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(rgbBase);

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	QPainter p(&m_pixmap);
	p.initFrom(this);

	// Show that we may have clip limits...
	if (m_pEditor->length() > 0) {
		int x1 = pTimeScale->pixelFromFrame(m_pEditor->length()) - cx;
		if (x1 < 0)
			x1 = 0;
		if (x1 < w)
			p.fillRect(x1, 0, w - x1, h, rgbBase.darker(105));
	}

	// Draw horizontal lines...
	p.setPen(rgbLight);
//	p.setBrush(rgbSharp);
	int h1 = m_pEditor->editList()->itemHeight();
	int ch = qtractorScrollView::contentsHeight() - cy;
	int q = (cy / h1);
	int n = 127 - q;
	int y = q * h1 - cy;
	while (y < h && y < ch) {
		int k = (n % 12);
		if (k == 1 || k == 3 || k == 6 || k == 8 || k == 10)
			p.fillRect(0, y + 1, w, h1, rgbSharp); 
		p.drawLine(0, y, w, y);
		y += h1;
		--n;
	}

	// Account for the editing offset:
	qtractorTimeScale::Cursor cursor(pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_pEditor->offset());
	unsigned long t0 = pNode->tickFromFrame(m_pEditor->offset());
	int x0 = pTimeScale->pixelFromFrame(m_pEditor->offset());
	int dx = x0 + cx;

	// Draw vertical grid lines...
	const QBrush zebra(QColor(0, 0, 0, 20));
	pNode = cursor.seekPixel(dx);
	unsigned short iSnapPerBeat
		= (m_pEditor->isSnapGrid() ? pTimeScale->snapPerBeat() : 0);
	unsigned short iPixelsPerBeat = pNode->pixelsPerBeat();
	unsigned int iBeat = pNode->beatFromPixel(dx);
	unsigned short iBar
		= (m_pEditor->isSnapZebra() ? pNode->barFromBeat(iBeat) : 0);
	int x = pNode->pixelFromBeat(iBeat) - dx;
	int x2 = x;
	while (x < w) {
		bool bBeatIsBar = pNode->beatIsBar(iBeat);
		if (bBeatIsBar) {
			p.setPen(rgbLight);
			p.drawLine(x, 0, x, h);
			if (m_pEditor->isSnapZebra() && (x > x2) && (++iBar & 1))
				p.fillRect(QRect(x2, 0, x - x2 + 1, h), zebra);
			x2 = x;
			if (iBeat == pNode->beat)
				iPixelsPerBeat = pNode->pixelsPerBeat();
		}
		if (bBeatIsBar || iPixelsPerBeat > 8) {
			p.setPen(rgbDark);
			p.drawLine(x - 1, 0, x - 1, h);
		}
		if (iSnapPerBeat > 1) {
			int q = iPixelsPerBeat / iSnapPerBeat;
			if (q > 4) {  
				p.setPen(rgbBase.value() < 0x7f
					? rgbDark.darker(105) : rgbLight.lighter(120));
				for (int i = 1; i < iSnapPerBeat; ++i) {
					x = pTimeScale->pixelSnap(x + dx + q) - dx - 1;
					p.drawLine(x, 0, x, h);
				}
			}
		}
		pNode = cursor.seekBeat(++iBeat);
		x = pNode->pixelFromBeat(iBeat) - dx;
	}
	if (m_pEditor->isSnapZebra() && (x > x2) && (++iBar & 1))
		p.fillRect(QRect(x2, 0, x - x2 + 1, h), zebra);

	if (y > ch)
		p.fillRect(0, ch, w, h - ch, rgbDark);

	//
	// Draw the sequence events...
	//

	qtractorMidiSequence *pSeq = m_pEditor->sequence();
	if (pSeq == NULL)
		return;

	pNode = cursor.seekPixel(x = dx);
	unsigned long iTickStart = pNode->tickFromPixel(x);
	pNode = cursor.seekPixel(x += w);
	unsigned long iTickEnd = pNode->tickFromPixel(x);

//	p.setPen(rgbFore);
//	p.setBrush(rgbBack);

	QColor rgbNote(rgbBack);
	int hue, sat, val;
	rgbNote.getHsv(&hue, &sat, &val); sat = 86;

	qtractorMidiEvent *pEvent
		= m_pEditor->seekEvent(iTickStart > t0 ? iTickStart - t0 : 0);
	while (pEvent) {
		unsigned long t1 = t0 + pEvent->time();
		if (t1 >= iTickEnd)
			break;
		unsigned long t2 = t1 + pEvent->duration();
		// Filter event type!...
		if (pEvent->type() == m_eventType && t2 >= iTickStart) {
			y = ch - h1 * (pEvent->note() + 1);
			if (y + h1 >= 0 && y < h) {
				pNode = cursor.seekTick(t1);
				x = pNode->pixelFromTick(t1) - x0 - cx;
				int w1 = pNode->pixelFromTick(t2) - x0 - cx - x;
				if (w1 < 5)
					w1 = 5;
				if (m_pEditor->isNoteColor()) {
					hue = (128 - int(pEvent->note())) << 4;
					if (m_pEditor->isValueColor())
						sat = 64 + (int(pEvent->value()) >> 1);
					rgbNote.setHsv(hue, sat, val);
				} else if (m_pEditor->isValueColor()) {
					hue = (128 - int(pEvent->value())) << 1;
					rgbNote.setHsv(hue, sat, val);
				}
				p.fillRect(x, y, w1, h1, rgbFore);
				if (h1 > 3)
					p.fillRect(x + 1, y + 1, w1 - 4, h1 - 3, rgbNote);
			}
		}
		pEvent = pEvent->next();
	}

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		const QBrush shade(QColor(0, 0, 0, 60));
		p.setPen(Qt::darkCyan);
		x = pTimeScale->pixelFromFrame(pSession->loopStart()) - dx;
		if (x >= w)
			p.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x >= 0) {
			p.fillRect(QRect(0, 0, x, h), shade);
			p.drawLine(x, 0, x, h);
		}
		x = pTimeScale->pixelFromFrame(pSession->loopEnd()) - dx;
		if (x < 0)
			p.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x < w) {
			p.fillRect(QRect(x, 0, w - x, h), shade);
			p.drawLine(x, 0, x, h);
		}
	}

	// Draw punch boundaries, if applicable...
	if (pSession->isPunching()) {
		const QBrush shade(QColor(0, 0, 0, 60));
		p.setPen(Qt::darkMagenta);
		x = pTimeScale->pixelFromFrame(pSession->punchIn()) - dx;
		if (x >= w)
			p.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x >= 0) {
			p.fillRect(QRect(0, 0, x, h), shade);
			p.drawLine(x, 0, x, h);
		}
		x = pTimeScale->pixelFromFrame(pSession->punchOut()) - dx;
		if (x < 0)
			p.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x < w) {
			p.fillRect(QRect(x, 0, w - x, h), shade);
			p.drawLine(x, 0, x, h);
		}
	}
}


// Draw the track view.
void qtractorMidiEditView::drawContents ( QPainter *pPainter, const QRect& rect )
{
	// Draw viewport canvas...
	pPainter->drawPixmap(rect, m_pixmap, rect);

	m_pEditor->paintDragState(this, pPainter);

	// Draw special play-head header...
	int cx = qtractorScrollView::contentsX();
	int x  = m_pEditor->playHeadX() - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::red);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}
}


// To have main view in h-sync with edit list.
void qtractorMidiEditView::contentsXMovingSlot ( int cx, int /*cy*/ )
{
	if (qtractorScrollView::contentsX() != cx)
		qtractorScrollView::setContentsPos(cx, qtractorScrollView::contentsY());
}


// To have main view in v-sync with edit list.
void qtractorMidiEditView::contentsYMovingSlot ( int /*cx*/, int cy )
{
	if (qtractorScrollView::contentsY() != cy)
		qtractorScrollView::setContentsPos(qtractorScrollView::contentsX(), cy);
}


// Focus lost event.
void qtractorMidiEditView::focusOutEvent ( QFocusEvent *pFocusEvent )
{
	m_pEditor->focusOut(this);

	qtractorScrollView::focusOutEvent(pFocusEvent);
}


// Keyboard event handler.
void qtractorMidiEditView::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (!m_pEditor->keyPress(this, pKeyEvent->key(), pKeyEvent->modifiers()))
		qtractorScrollView::keyPressEvent(pKeyEvent);
}


// Handle item selection/dragging -- mouse button press.
void qtractorMidiEditView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse press...
	qtractorScrollView::mousePressEvent(pMouseEvent);

	// Only the left-mouse-button is meaningful atm...
	if (pMouseEvent->button() != Qt::LeftButton)
		return;

	// Maybe start the drag-move-selection dance?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	// Remember what and where we'll be dragging/selecting...
	m_pEditor->dragMoveStart(this, pos, pMouseEvent->modifiers());
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse move...
	qtractorScrollView::mouseMoveEvent(pMouseEvent);

	// Are we already moving/dragging something?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	m_pEditor->dragMoveUpdate(this, pos, pMouseEvent->modifiers());
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse release...
	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	// Were we moving/dragging something?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	m_pEditor->dragMoveCommit(this, pos, pMouseEvent->modifiers());
}


// Handle zoom with mouse wheel.
void qtractorMidiEditView::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers() & Qt::ControlModifier) {
		int delta = pWheelEvent->delta();
		if (delta > 0)
			m_pEditor->zoomIn();
		else
			m_pEditor->zoomOut();
	}
	else qtractorScrollView::wheelEvent(pWheelEvent);
}


// Trap for help/tool-tip and leave events.
bool qtractorMidiEditView::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	if (m_pEditor->dragMoveFilter(this, pObject, pEvent))
		return true;

	return qtractorScrollView::eventFilter(pObject, pEvent);
}


// end of qtractorMidiEditView.cpp

