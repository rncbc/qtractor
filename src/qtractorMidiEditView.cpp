// qtractorMidiEditView.cpp
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
#include "qtractorMidiEditView.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditList.h"
#include "qtractorMidiEditTime.h"
#include "qtractorMidiEditEvent.h"

#include "qtractorMidiSequence.h"
#include "qtractorMidiClip.h"

#include "qtractorSession.h"
#include "qtractorOptions.h"

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <QScrollBar>
#include <QToolButton>

#include <QStyle>

#ifdef CONFIG_GRADIENT
#include <QLinearGradient>
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
	m_pVzoomReset->setIcon(QIcon(":/images/viewZoomReset.png"));

	m_pVzoomIn->setAutoRepeat(true);
	m_pVzoomOut->setAutoRepeat(true);

	m_pVzoomIn->setToolTip(tr("Zoom in (vertical)"));
	m_pVzoomOut->setToolTip(tr("Zoom out (vertical)"));
	m_pVzoomReset->setToolTip(tr("Zoom reset (vertical)"));

	const int iScrollBarExtent
		= qtractorScrollView::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	m_pVzoomReset->setFixedHeight(iScrollBarExtent);
	m_pVzoomOut->setFixedHeight(iScrollBarExtent);
	m_pVzoomIn->setFixedHeight(iScrollBarExtent);
	qtractorScrollView::addScrollBarWidget(m_pVzoomReset, Qt::AlignBottom);
	qtractorScrollView::addScrollBarWidget(m_pVzoomOut,   Qt::AlignBottom);
	qtractorScrollView::addScrollBarWidget(m_pVzoomIn,    Qt::AlignBottom);

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
			const unsigned long t0
				= pTimeScale->tickFromFrame(m_pEditor->offset());
			const int x0
				= pTimeScale->pixelFromFrame(m_pEditor->offset());
			const int w0
				= pTimeScale->pixelFromTick(t0 + pSeq->duration()) - x0;
			if (iContentsWidth < w0)
				iContentsWidth = w0;
		}
		iContentsWidth += pTimeScale->pixelFromBeat(pTimeScale->beatsPerBar());
		if (iContentsWidth  < qtractorScrollView::width())
			iContentsWidth += qtractorScrollView::width();
	}

	qtractorScrollView::resizeContents(
		iContentsWidth, qtractorScrollView::contentsHeight());

	// Force an update on other views too...
	m_pEditor->editTime()->resizeContents(
		iContentsWidth + 100, m_pEditor->editTime()->viewport()->height());
//	m_pEditor->editTime()->updateContents();

	m_pEditor->editEvent()->resizeContents(
		iContentsWidth, m_pEditor->editEvent()->viewport()->height());
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

#ifdef CONFIG_GRADIENT
	// Update canvas edge-border shadow gradients...
	const int ws = 22;
	const QColor rgba0(0, 0, 0, 0);
	const QColor rgba1(0, 0, 0, 20);
	const QColor rgba2(0, 0, 0, 80);
	QLinearGradient gradLeft(0, 0, ws, 0);
	gradLeft.setColorAt(0.0f, rgba2);
	gradLeft.setColorAt(0.5f, rgba1);
	gradLeft.setColorAt(1.0f, rgba0);
	m_gradLeft = gradLeft;
	const int xs = qtractorScrollView::viewport()->width() - ws;
	QLinearGradient gradRight(xs, 0, xs + ws, 0);
	gradRight.setColorAt(0.0f, rgba0);
	gradRight.setColorAt(0.5f, rgba1);
	gradRight.setColorAt(1.0f, rgba2);
	m_gradRight = gradRight;
#endif

	// Scrollbar/tools layout management.
	const QSize& size = qtractorScrollView::size();
	QScrollBar *pVScrollBar = qtractorScrollView::verticalScrollBar();
	const int w = pVScrollBar->width();

	updateContents();

	m_pEditor->editEventScale()->setFixedWidth(
		m_pEditor->width() - size.width());
	m_pEditor->editEventFrame()->setFixedWidth(w);
}


// (Re)create the complete track view pixmap.
void qtractorMidiEditView::updatePixmap ( int cx, int cy )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	const QColor& rgbBase  = pal.base().color();
	const QColor& rgbDark  = pal.mid().color();
	const QColor& rgbLight = pal.midlight().color();
	const QColor& rgbSharp = rgbBase.darker(110);

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(rgbBase);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == NULL)
		return;

	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	// Show that we may have clip limits...
	if (m_pEditor->length() > 0) {
		int x1 = pTimeScale->pixelFromFrame(m_pEditor->length()) - cx;
		if (x1 < 0)
			x1 = 0;
		if (x1 < w)
			painter.fillRect(x1, 0, w - x1, h, rgbBase.darker(105));
	}

	// Draw horizontal lines...
	painter.setPen(rgbLight);
//	p.setBrush(rgbSharp);
	const int h1 = m_pEditor->editList()->itemHeight();
	const int ch = qtractorScrollView::contentsHeight() - cy;
	const int q = (cy / h1);

	int n = 127 - q;
	int y = q * h1 - cy;
	while (y < h && y < ch) {
		int k = (n % 12);
		if (k == 1 || k == 3 || k == 6 || k == 8 || k == 10)
			painter.fillRect(0, y + 1, w, h1, rgbSharp);
		painter.drawLine(0, y, w, y);
		y += h1;
		--n;
	}

	// Account for the editing offset:
	qtractorTimeScale::Cursor cursor(pTimeScale);
	const unsigned long f0 = m_pEditor->offset();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(f0);
	const unsigned long t0 = pNode->tickFromFrame(f0);
	const int x0 = pTimeScale->pixelFromFrame(f0);
	const int dx = x0 + cx;

	// Draw vertical grid lines...
	const QBrush zebra(QColor(0, 0, 0, 20));
	pNode = cursor.seekPixel(dx);
	const unsigned short iSnapPerBeat
		= (m_pEditor->isSnapGrid() ? pTimeScale->snapPerBeat() : 0);
	unsigned short iPixelsPerBeat = pNode->pixelsPerBeat();
	unsigned int iBeat = pNode->beatFromPixel(dx);
	if (iBeat > 0) pNode = cursor.seekBeat(--iBeat);
	unsigned short iBar
		= (m_pEditor->isSnapZebra() ? pNode->barFromBeat(iBeat) : 0);
	int x = pNode->pixelFromBeat(iBeat) - dx;
	int x2 = x;
	while (x < w) {
		const bool bBeatIsBar = pNode->beatIsBar(iBeat);
		if (bBeatIsBar) {
			painter.setPen(rgbDark);
			painter.drawLine(x - 1, 0, x - 1, h);
			if (m_pEditor->isSnapZebra() && (x > x2) && (++iBar & 1))
				painter.fillRect(QRect(x2, 0, x - x2 + 1, h), zebra);
			x2 = x;
			if (iBeat == pNode->beat)
				iPixelsPerBeat = pNode->pixelsPerBeat();
		}
		if (bBeatIsBar || iPixelsPerBeat > 8) {
			painter.setPen(rgbLight);
			painter.drawLine(x, 0, x, h);
		}
		if (iSnapPerBeat > 1) {
			int q = iPixelsPerBeat / iSnapPerBeat;
			if (q > 4) {  
				painter.setPen(rgbBase.value() < 0x7f
					? rgbLight.darker(105) : rgbLight.lighter(120));
				for (int i = 1; i < iSnapPerBeat; ++i) {
					x = pTimeScale->pixelSnap(x + dx + q) - dx - 1;
					painter.drawLine(x, 0, x, h);
				}
			}
		}
		pNode = cursor.seekBeat(++iBeat);
		x = pNode->pixelFromBeat(iBeat) - dx;
	}
	if (m_pEditor->isSnapZebra() && (x > x2) && (++iBar & 1))
		painter.fillRect(QRect(x2, 0, x - x2 + 1, h), zebra);

	if (y > ch)
		painter.fillRect(0, ch, w, h - ch, rgbDark);

	// Draw location marker lines...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekPixel(dx);
	while (pMarker) {
		x = pTimeScale->pixelFromFrame(pMarker->frame) - dx;
		if (x > w) break;
		painter.setPen(pMarker->color);
		painter.drawLine(x, 0, x, h);
		pMarker = pMarker->next();
	}

	//
	// Draw the clip(s) sequenece events...
	//

	qtractorMidiSequence *pSeq = m_pEditor->sequence();
	if (pSeq == NULL)
		return;

	pNode = cursor.seekPixel(x = dx);
	const unsigned long iTickStart = pNode->tickFromPixel(x);
	pNode = cursor.seekPixel(x += w);
	const unsigned long iTickEnd = pNode->tickFromPixel(x);

	const unsigned long f1 = f0 + m_pEditor->length();
	pNode = cursor.seekFrame(f1);
	const unsigned long iTickEnd2 = pNode->tickFromFrame(f1);

	// Draw ghost-track events in dimmed transparecncy (alpha=55)...
	qtractorTrack *pTrack = m_pEditor->ghostTrack();
	if (pTrack) {
		// Don't draw beyhond the right-most position (x = dx + w)...
		const unsigned long f2 = pTimeScale->frameFromPixel(x);
		const bool bDrumMode = pTrack->isMidiDrums();
		qtractorClip *pClip = pTrack->clips().first();
		while (pClip && pClip->clipStart() + pClip->clipLength() < f0)
			pClip = pClip->next();
		while (pClip && pClip->clipStart() < f2) {
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip && pMidiClip != m_pEditor->midiClip()) {
				m_pEditor->reset(false); // FIXME: reset cached cursor...
				const unsigned long iClipStart
					= pMidiClip->clipStart();
				const unsigned long iClipEnd
					= iClipStart + pMidiClip->clipLength();
				pNode = cursor.seekFrame(iClipStart);
				const unsigned long t1 = pNode->tickFromFrame(iClipStart);
				pNode = cursor.seekFrame(iClipEnd);
				const unsigned long t2 = pNode->tickFromFrame(iClipEnd);
				drawEvents(painter, dx, cy, pMidiClip->sequence(),
					t1, iTickStart, iTickEnd, t2, bDrumMode,
					pTrack->foreground(), pTrack->background(), 55);
			}
			pClip = pClip->next();
		}
		 // FIXME: reset cached cursor...
		m_pEditor->reset(false);
	}

	// Draw actual events in full brightness (alpha=255)...
	drawEvents(painter, dx, cy, pSeq,
		t0, iTickStart, iTickEnd, iTickEnd2, m_pEditor->isDrumMode(),
		m_pEditor->foreground(), m_pEditor->background());

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		const QBrush shade(QColor(0, 0, 0, 60));
		painter.setPen(Qt::darkCyan);
		x = pTimeScale->pixelFromFrame(pSession->loopStart()) - dx;
		if (x >= w)
			painter.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x >= 0) {
			painter.fillRect(QRect(0, 0, x, h), shade);
			painter.drawLine(x, 0, x, h);
		}
		x = pTimeScale->pixelFromFrame(pSession->loopEnd()) - dx;
		if (x < 0)
			painter.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x < w) {
			painter.fillRect(QRect(x, 0, w - x, h), shade);
			painter.drawLine(x, 0, x, h);
		}
	}

	// Draw punch boundaries, if applicable...
	if (pSession->isPunching()) {
		const QBrush shade(QColor(0, 0, 0, 60));
		painter.setPen(Qt::darkMagenta);
		x = pTimeScale->pixelFromFrame(pSession->punchIn()) - dx;
		if (x >= w)
			painter.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x >= 0) {
			painter.fillRect(QRect(0, 0, x, h), shade);
			painter.drawLine(x, 0, x, h);
		}
		x = pTimeScale->pixelFromFrame(pSession->punchOut()) - dx;
		if (x < 0)
			painter.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x < w) {
			painter.fillRect(QRect(x, 0, w - x, h), shade);
			painter.drawLine(x, 0, x, h);
		}
	}
}


// Draw the track view events.
void qtractorMidiEditView::drawEvents ( QPainter& painter,
	int dx, int dy, qtractorMidiSequence *pSeq, unsigned long t0,
	unsigned long iTickStart, unsigned long iTickEnd,
	unsigned long iTickEnd2, bool bDrumMode,
	const QColor& fore, const QColor& back, int alpha )
{
	const int h  = qtractorScrollView::viewport()->height();
	const int h1 = m_pEditor->editList()->itemHeight();
	const int ch = qtractorScrollView::contentsHeight() - dy;

	QVector<QPoint> diamond;
	if (bDrumMode) {
		const int h2 = (h1 >> 1) + 1;
		diamond.append(QPoint(  0, -1));
		diamond.append(QPoint(-h2, h2));
		diamond.append(QPoint(  0, h1 + 2));
		diamond.append(QPoint( h2, h2));
		painter.setRenderHint(QPainter::Antialiasing, true);
	}

	QColor rgbFore(fore);
	rgbFore.setAlpha(alpha);
	painter.setPen(rgbFore);

	QColor rgbNote(back);
	int hue, sat, val;
	rgbNote.getHsv(&hue, &sat, &val); sat = 86;
	rgbNote.setAlpha(alpha);

	int x, y;

	qtractorTimeScale::Cursor cursor(m_pEditor->timeScale());
	qtractorTimeScale::Node *pNode;

	const qtractorMidiEvent::EventType eventType = m_eventType;

	qtractorMidiEvent *pEvent
		= m_pEditor->seekEvent(pSeq, iTickStart > t0 ? iTickStart - t0 : 0);
	while (pEvent) {
		const unsigned long t1 = t0 + pEvent->time();
		if (t1 >= iTickEnd)
			break;
		unsigned long t2 = t1 + pEvent->duration();
		if (t2 > iTickEnd2)
			t2 = iTickEnd2;
		// Filter event type!...
		if (pEvent->type() == eventType && t2 >= iTickStart) {
			y = ch - h1 * (pEvent->note() + 1);
			if (y + h1 >= 0 && y < h) {
				pNode = cursor.seekTick(t1);
				x = pNode->pixelFromTick(t1) - dx;
				pNode = cursor.seekTick(t2);
				int w1 = (t1 >= t2 && m_pEditor->isClipRecord()
					? m_pEditor->playHeadX()
					: pNode->pixelFromTick(t2) - dx) - x;
				if (w1 < 5) w1 = 5;
				if (m_pEditor->isNoteColor()) {
					hue = (128 - int(pEvent->note())) << 4;
					if (m_pEditor->isValueColor())
						sat = 64 + (int(pEvent->value()) >> 1);
					rgbNote.setHsv(hue, sat, val, alpha);
				} else if (m_pEditor->isValueColor()) {
					hue = (128 - int(pEvent->value())) << 1;
					rgbNote.setHsv(hue, sat, val, alpha);
				}
				if (bDrumMode) {
					painter.setBrush(rgbNote);
					const QPolygon& polyg
						= QPolygon(diamond).translated(x, y);
					if (h1 > 3)
						painter.drawPolygon(polyg.translated(1, 0)); // shadow
					painter.drawPolygon(polyg); // diamond
				} else {
					painter.fillRect(x, y, w1, h1, rgbFore);
					if (h1 > 3)
						painter.fillRect(x + 1, y + 1, w1 - 4, h1 - 3, rgbNote);
				}
			}
		}
		pEvent = pEvent->next();
	}

	if (bDrumMode)
		painter.setRenderHint(QPainter::Antialiasing, false);
}


// Draw the track view.
void qtractorMidiEditView::drawContents ( QPainter *pPainter, const QRect& rect )
{
	// Draw viewport canvas...
	pPainter->drawPixmap(rect, m_pixmap, rect);

#ifdef CONFIG_GRADIENT
	// Draw canvas edge-border shadows...
	const int ws = 22;
	const int xs = qtractorScrollView::viewport()->width() - ws;
	if (rect.left() < ws)
		pPainter->fillRect(0, rect.top(), ws, rect.bottom(), m_gradLeft);
	if (rect.right() > xs)
		pPainter->fillRect(xs, rect.top(), xs + ws, rect.bottom(), m_gradRight);
#endif
	m_pEditor->paintDragState(this, pPainter);

	// Draw special play/edit-head/tail headers...
	const int cx = qtractorScrollView::contentsX();

	int x = m_pEditor->editHeadX() - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::blue);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	x = m_pEditor->editTailX() - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::blue);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	x = m_pEditor->playHeadX() - cx;
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
//	qtractorScrollView::mousePressEvent(pMouseEvent);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Which mouse state?
	bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));

	// Maybe start the drag-move-selection dance?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	const unsigned long iFrame = pTimeScale->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));

	// We'll need options somehow...
	qtractorOptions *pOptions = qtractorOptions::getInstance();

	switch (pMouseEvent->button()) {
	case Qt::LeftButton:
		// Only the left-mouse-button was meaningful...
		break;
	case Qt::MidButton:
		// Mid-button direct positioning...
		m_pEditor->selectAll(this, false);
		if (pOptions && pOptions->bMidButtonModifier)
			bModifier = !bModifier;	// Reverse mid-button role...
		// Which mouse state?
		if (bModifier) {
			// Play-head positioning commit...
			m_pEditor->setPlayHead(iFrame);
			pSession->setPlayHead(iFrame);
		} else {
			// Edit cursor (merge) positioning...
			m_pEditor->setEditHead(iFrame);
			m_pEditor->setEditTail(iFrame);
		}
		// Logical contents changed, just for visual feedback...
		m_pEditor->selectionChangeNotify();
		// Fall thru...
	default:
		return;
	}

	// Remember what and where we'll be dragging/selecting...
	m_pEditor->dragMoveStart(this, pos, pMouseEvent->modifiers());
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorMidiEditView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse move...
//	qtractorScrollView::mouseMoveEvent(pMouseEvent);

	// Are we already moving/dragging something?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	m_pEditor->dragMoveUpdate(this, pos, pMouseEvent->modifiers());
}


// Handle item selection/dragging -- mouse button release.
void qtractorMidiEditView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse release...
//	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

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
