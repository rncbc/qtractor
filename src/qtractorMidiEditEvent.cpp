// qtractorMidiEditEvent.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEditEvent.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditView.h"

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

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define horizontalAdvance  width
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
	QWidget::setFont(QFont(font.family(), font.pointSize() - 3));
}

// Default destructor.
qtractorMidiEditEventScale::~qtractorMidiEditEventScale (void)
{
}


// Paint event handler.
void qtractorMidiEditEventScale::paintEvent ( QPaintEvent * )
{
	QPainter painter(this);

	painter.setPen(Qt::darkGray);

	// Draw scale line labels...
	qtractorMidiEditEvent *pEditEvent = m_pEditor->editEvent();
	const QFontMetrics& fm = painter.fontMetrics();
	int h  = (pEditEvent->viewport())->height();
	int w  = QWidget::width();

	int h2 = (fm.height() >> 1);
	int dy = (h >> 3);

	// Account for event view widget frame...
	h += 4;
	int y  = 2;
	int y0 = y + 1;

	int n, dn;

	const qtractorMidiEvent::EventType eventType
		= pEditEvent->eventType();
	if (eventType == qtractorMidiEvent::PITCHBEND) {
		n  = 8192;
		dn = (n >> 2);
	} else {
		if (eventType == qtractorMidiEvent::REGPARAM    ||
			eventType == qtractorMidiEvent::NONREGPARAM ||
			eventType == qtractorMidiEvent::CONTROL14)
			n = 16384;
		else
			n = 128;
		dn = (n >> 3);
	}

	while (y < h) {
		const QString& sLabel = QString::number(n);
		if (fm.horizontalAdvance(sLabel) < w - 6)
			painter.drawLine(w - 4, y, w - 1, y);
		if (y > y0 + (h2 << 1) && y < h - (h2 << 1)) {
			painter.drawText(2, y - h2, w - 8, fm.height(),
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
	m_eventParam = 0;

	// Zoom tool widgets
	m_pHzoomOut   = new QToolButton(this);
	m_pHzoomIn    = new QToolButton(this);
	m_pHzoomReset = new QToolButton(this);

	m_pHzoomOut->setIcon(QIcon::fromTheme("viewZoomOut"));
	m_pHzoomIn->setIcon(QIcon::fromTheme("viewZoomIn"));
	m_pHzoomReset->setIcon(QIcon::fromTheme("viewZoomReset"));

	const int iScrollBarExtent
		= qtractorScrollView::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	m_pHzoomReset->setFixedWidth(iScrollBarExtent);
	m_pHzoomIn->setFixedWidth(iScrollBarExtent);
	m_pHzoomOut->setFixedWidth(iScrollBarExtent);
	qtractorScrollView::addScrollBarWidget(m_pHzoomReset, Qt::AlignRight);
	qtractorScrollView::addScrollBarWidget(m_pHzoomIn,    Qt::AlignRight);
	qtractorScrollView::addScrollBarWidget(m_pHzoomOut,   Qt::AlignRight);

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
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 3));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));

	// Trap for help/tool-tips and leave events.
	qtractorScrollView::viewport()->installEventFilter(this);
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

	m_pEditor->selectAll(this, false);
	m_pEditor->editEventScale()->update();
//	m_pEditor->updateContents();
}

qtractorMidiEvent::EventType qtractorMidiEditEvent::eventType (void) const
{
	return m_eventType;
}


void qtractorMidiEditEvent::setEventParam ( unsigned short param )
{
	m_eventParam = param;

	m_pEditor->selectAll(this, false);
//	m_pEditor->updateContents();
}

unsigned short qtractorMidiEditEvent::eventParam (void) const
{
	return m_eventParam;
}


// Resize event handler.
void qtractorMidiEditEvent::resizeEvent ( QResizeEvent *pResizeEvent )
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
	const int w = pViewport->width();
	const int h = pViewport->height() & ~1; // always even.

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	const QColor& rgbBase  = pal.base().color();
	const QColor& rgbLine  = pal.mid().color();
	const QColor& rgbLight = pal.midlight().color();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(rgbBase);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	QPainter painter(&m_pixmap);
//	painter.initFrom(this);
	painter.setFont(qtractorScrollView::font());

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
	const int dy = (h >> 3);
	int  y = 0;
	while (y < h) {
		painter.drawLine(0, y, w, y);
		y += dy;
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
#if 0
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
			painter.setPen(rgbLine);
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
			const int q = iPixelsPerBeat / iSnapPerBeat;
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
#else
	unsigned short iBar = pNode->barFromPixel(dx);
	if (iBar > 0) pNode = cursor.seekBar(--iBar);
	int x = pNode->pixelFromBar(iBar) - dx;
	while (x < w) {
		// Next bar...
		pNode = cursor.seekPixel(x + dx);
		const int x2 = pNode->pixelFromBar(++iBar) - dx;
		// Zebra lines...
		if (m_pEditor->isSnapZebra() && (iBar & 1))
			painter.fillRect(QRect(x, 0, x2 - x + 1, h), zebra);
		// Beat lines...
		const unsigned short iBeatsPerBar2 = pNode->beatsPerBar2();
		const float q2 = float(x2 - x) / float(iBeatsPerBar2);
		if (q2 > 8.0f) {
			float p2 = float(x);
			for (int i = 0; i < iBeatsPerBar2; ++i) {
				if (iSnapPerBeat > 1) {
					const float q1 = q2 / float(iSnapPerBeat);
					if (q1 > 4.0f) {
						painter.setPen(rgbBase.value() < 0x7f
							? rgbLight.darker(105) : rgbLight.lighter(120));
						float p1 = p2;
						for (int j = 1; j < iSnapPerBeat; ++j) {
							const int x1 = int(p1 += q1);
							painter.drawLine(x1, 0, x1, h);
						}
					}
				}
				x = int(p2 += q2);
				if (x > w)
					break;
				if (i < iBeatsPerBar2 - 1) {
					painter.setPen(rgbLight);
					painter.drawLine(x, 0, x, h);
				}
			}
		}
		// Bar line...
		painter.setPen(rgbLine);
		painter.drawLine(x2 - 1, 0, x2 - 1, h);
		painter.setPen(rgbLight);
		painter.drawLine(x2, 0, x2, h);
		// Move forward...
		x = x2;
	}
#endif

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
	// Draw the clip(s) events...
	//

	qtractorMidiSequence *pSeq = m_pEditor->sequence();
	if (pSeq == nullptr)
		return;

	pNode = cursor.seekPixel(x = dx);
	const unsigned long iTickStart = pNode->tickFromPixel(x);
	pNode = cursor.seekPixel(x += w);
	const unsigned long iTickEnd = pNode->tickFromPixel(x);

	const unsigned long f1 = f0 + m_pEditor->length();
	pNode = cursor.seekFrame(f1);
	const unsigned long iTickEnd2 = pNode->tickFromFrame(f1);

	// This is the zero-line...
	const int y0 = (m_eventType == qtractorMidiEvent::PITCHBEND ? h >> 1 : h);

	painter.setPen(rgbLight);
	painter.drawLine(0, y0 - 1, w, y0 - 1);
	painter.setPen(rgbLine);
	painter.drawLine(0, y0, w, y0);

	painter.setRenderHint(QPainter::Antialiasing, true);

	// Draw ghost-track events in dimmed transparecncy (alpha=55)...
	qtractorTrack *pGhostTrack = m_pEditor->ghostTrack();
	if (pGhostTrack) {
		// Don't draw beyhond the right-most position (x = dx + w)...
		const unsigned long f2 = pTimeScale->frameFromPixel(x);
		const bool bDrumMode = pGhostTrack->isMidiDrums();
		qtractorClip *pClip = pGhostTrack->clips().first();
		while (pClip && pClip->clipStart() + pClip->clipLength() < f0)
			pClip = pClip->next();
		while (pClip && pClip->clipStart() < f2) {
			qtractorMidiSequence *pGhostSeq = nullptr;
			qtractorMidiClip *pGhostClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pGhostClip && pGhostClip != m_pEditor->midiClip())
				pGhostSeq = pGhostClip->sequence();
			if (pGhostSeq) {
				m_pEditor->reset(false); // FIXME: reset cached cursors...
				const unsigned long iClipStart
					= pGhostClip->clipStart();
				const unsigned long iClipEnd
					= iClipStart + pGhostClip->clipLength();
				pNode = cursor.seekFrame(iClipStart);
				const unsigned long t1 = pNode->tickFromFrame(iClipStart);
				pNode = cursor.seekFrame(iClipEnd);
				const unsigned long t2 = pNode->tickFromFrame(iClipEnd);
				drawEvents(painter, dx, y0, pGhostSeq,
					t1, iTickStart, iTickEnd, t2, bDrumMode,
					pGhostTrack->foreground(), pGhostTrack->background(), 60);
			}
			pClip = pClip->next();
		}
		 // FIXME: reset cached cursors...
		m_pEditor->reset(false);
	}

	// Draw actual events in full brightness (alpha=255)...
	drawEvents(painter, dx, y0, pSeq,
		t0, iTickStart, iTickEnd, iTickEnd2, m_pEditor->isDrumMode(),
		m_pEditor->foreground(), m_pEditor->background());

	painter.setRenderHint(QPainter::Antialiasing, false);

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
void qtractorMidiEditEvent::drawEvents ( QPainter& painter,
	int dx, int dy, qtractorMidiSequence *pSeq, unsigned long t0,
	unsigned long iTickStart, unsigned long iTickEnd,
	unsigned long iTickEnd2, bool bDrumMode,
	const QColor& fore, const QColor& back, int alpha )
{
	const int y0 = dy;	// former nomenclature.

	QColor rgbFore(fore);
	rgbFore.setAlpha(alpha);
	painter.setPen(rgbFore);

	QColor rgbValue(back);
	int hue, sat, val;
	rgbValue.getHsv(&hue, &sat, &val); sat = 86;
	rgbValue.setAlpha(alpha);

	int x, y;

	const QFontMetrics fm(qtractorScrollView::font());
	const int hs = fm.ascent(); // fm.height() - 2;

	qtractorTimeScale::Cursor cursor(m_pEditor->timeScale());
	qtractorTimeScale::Node *pNode;

	const qtractorMidiEvent::EventType eventType = m_eventType;
	const unsigned short eventParam = m_eventParam;
	const bool bEventParam
		= (eventType == qtractorMidiEvent::CONTROLLER
		|| eventType == qtractorMidiEvent::REGPARAM
		|| eventType == qtractorMidiEvent::NONREGPARAM
		|| eventType == qtractorMidiEvent::CONTROL14);
	const int wm = m_pEditor->minEventWidth();

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
		if (pEvent->type() == eventType && t2 >= iTickStart
			&& (!bEventParam || pEvent->param() == eventParam)) {
			if (eventType == qtractorMidiEvent::REGPARAM    ||
				eventType == qtractorMidiEvent::NONREGPARAM ||
				eventType == qtractorMidiEvent::CONTROL14)
				y = y0 - (y0 * pEvent->value()) / 16384;
			else
			if (eventType == qtractorMidiEvent::PITCHBEND)
				y = y0 - (y0 * pEvent->pitchBend()) / 8192;
			else
			if (eventType == qtractorMidiEvent::PGMCHANGE)
				y = y0 - (y0 * pEvent->param()) / 128;
			else
				y = y0 - (y0 * pEvent->value()) / 128;
			pNode = cursor.seekTick(t1);
			x = pNode->pixelFromTick(t1) - dx;
			pNode = cursor.seekTick(t2);
			int w1 = (t1 >= t2 && m_pEditor->isClipRecordEx()
				? m_pEditor->playHeadX()
				: pNode->pixelFromTick(t2)) - dx - x;
			if (w1 < wm || !m_pEditor->isNoteDuration() || bDrumMode)
				w1 = wm;
			if (eventType == qtractorMidiEvent::NOTEON ||
				eventType == qtractorMidiEvent::KEYPRESS) {
				if (m_pEditor->isNoteColor()) {
					hue = (128 - int(pEvent->note())) << 4;
					if (m_pEditor->isValueColor())
						sat = 64 + (int(pEvent->velocity()) >> 1);
					rgbValue.setHsv(hue, sat, val, alpha);
				} else if (m_pEditor->isValueColor()) {
					hue = (128 - int(pEvent->velocity())) << 1;
					rgbValue.setHsv(hue, sat, val, alpha);
				}
			}
			// Lollipop candy shadow...
			const QRect rect(x, y, wm, wm);
			const int ym = (y > y0 - 5 ? -5 : 0); // negative pitch-bend adjust.
			painter.setPen(rgbFore);
			painter.setBrush(rgbFore);
			painter.drawEllipse(rect.adjusted(-1, ym - 1, +1, ym + 1));
			// Lollipop stick...
			if (y < y0) {
				painter.fillRect(x, y, w1, y0 - y, rgbFore);
				painter.fillRect(x + 1, y + 1, w1 - 4, y0 - y - 2, rgbValue);
			} else if (y > y0) {
				painter.fillRect(x, y0, w1, y - y0, rgbFore);
				painter.fillRect(x + 1, y0 + 1, w1 - 4, y - y0 - 2, rgbValue);
			} else {
				painter.fillRect(x, y0 - 2, w1, 4, rgbFore);
				painter.fillRect(x + 1, y0 - 1, w1 - 4, 2, rgbValue);
			}
			// Lollipop candy hilite...
			painter.setPen(rgbValue);
			painter.setBrush(rgbValue);
			painter.drawEllipse(rect.adjusted(0, ym, -2, ym - 2));
			// Note name...
			if (m_pEditor->isNoteNames() &&
				m_pEditor->isNoteDuration() && hs < y0 - y && (
				eventType == qtractorMidiEvent::NOTEON ||
				eventType == qtractorMidiEvent::KEYPRESS)) {
				const QString& sNoteName
					= m_pEditor->noteName(pEvent->note());
				painter.setPen(rgbFore.darker(160));
				painter.drawText(
					QRect(x + 2, y + 1, w1 - 6, y0 - y),
					Qt::AlignTop | Qt::AlignLeft, sNoteName);
				painter.setPen(rgbFore);
			}
		}
		pEvent = pEvent->next();
	}
}


// Draw the time scale.
void qtractorMidiEditEvent::drawContents ( QPainter *pPainter, const QRect& rect )
{
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

	if (m_pEditor->isStepInputHead()) {
		x = m_pEditor->stepInputHeadX() - cx;
		if (x >= rect.left() && x <= rect.right()) {
			pPainter->setPen(QColor(255, 0, 0, 120));
			pPainter->drawLine(x, rect.top(), x, rect.bottom());
		}
	}
}


// To have event view in h-sync with main view.
void qtractorMidiEditEvent::contentsXMovingSlot ( int cx, int /*cy*/ )
{
	if (qtractorScrollView::contentsX() != cx)
		qtractorScrollView::setContentsPos(cx, qtractorScrollView::contentsY());
}


// Focus lost event.
void qtractorMidiEditEvent::focusOutEvent ( QFocusEvent *pFocusEvent )
{
	m_pEditor->focusOut(this);

	qtractorScrollView::focusOutEvent(pFocusEvent);
}


// Keyboard event handler.
void qtractorMidiEditEvent::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (!m_pEditor->keyPress(this, pKeyEvent->key(), pKeyEvent->modifiers()))
		qtractorScrollView::keyPressEvent(pKeyEvent);
}


// Handle item selection/dragging -- mouse button press.
void qtractorMidiEditEvent::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse press...
//	qtractorScrollView::mousePressEvent(pMouseEvent);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// We'll need options somehow...
	qtractorOptions *pOptions = qtractorOptions::getInstance();

	// Which mouse state?
	bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));

	// Maybe start the drag-move-selection dance?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	const unsigned long iFrame = m_pEditor->frameSnap(m_pEditor->offset()
		+ pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));

	switch (pMouseEvent->button()) {
	case Qt::LeftButton:
		// Only the left-mouse-button was meaningful...
		break;
	case Qt::MiddleButton:
		// Mid-button direct positioning...
		m_pEditor->selectAll(this, false);
		// Which mouse state?
		if (pOptions && pOptions->bMidButtonModifier)
			bModifier = !bModifier;	// Reverse mid-button role...
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
void qtractorMidiEditEvent::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse move...
//	qtractorScrollView::mouseMoveEvent(pMouseEvent);

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
}


// Handle zoom with mouse wheel.
void qtractorMidiEditEvent::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers() & Qt::ControlModifier) {
		const int delta = pWheelEvent->angleDelta().y();
		if (delta > 0)
			m_pEditor->zoomIn();
		else
			m_pEditor->zoomOut();
	}
	else qtractorScrollView::wheelEvent(pWheelEvent);
}


// Trap for help/tool-tip and leave events.
bool qtractorMidiEditEvent::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	if (m_pEditor->dragMoveFilter(this, pObject, pEvent))
		return true;

	// Not handled here.
	return qtractorScrollView::eventFilter(pObject, pEvent);
}


// end of qtractorMidiEditEvent.cpp
