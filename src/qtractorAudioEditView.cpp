// qtractorAudioEditView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioEditView.h"
#include "qtractorAudioEditTime.h"

#include "qtractorAudioEditor.h"

#include "qtractorAudioClip.h"
#include "qtractorAudioPeak.h"

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
// qtractorAudioEditViewScale -- Audio clip time scale widget.

// Constructor.
qtractorAudioEditViewScale::qtractorAudioEditViewScale (
	qtractorAudioEditor *pEditor, QWidget *pParent ) : QWidget(pParent)
{
	m_pEditor = pEditor;

	QWidget::setMinimumWidth(32);

	QWidget::setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

//	QWidget::setBackgroundRole(QPalette::Mid);

	const QFont& font = QWidget::font();
	QWidget::setFont(QFont(font.family(), font.pointSize() - 3));
}

// Default destructor.
qtractorAudioEditViewScale::~qtractorAudioEditViewScale (void)
{
}


// Paint event handler.
void qtractorAudioEditViewScale::paintEvent ( QPaintEvent * )
{
	QPainter painter(this);

	painter.setPen(Qt::darkGray);

	const unsigned short iChannels = m_pEditor->channels();
	if (iChannels < 1)
		return;

	// Draw scale line labels...
	qtractorAudioEditView *pEditView= m_pEditor->editView();
	const QFontMetrics& fm = painter.fontMetrics();
	const int h  = (pEditView->viewport())->height() & ~1; // always even.
	const int w  = QWidget::width();

	const int h2 = (fm.height() >> 1);
	const int hn = (h / iChannels);
	const int dy = (hn >> 3);

	int y2 = hn;
	int y = 4;
	for (unsigned short i = 0; i < iChannels; ++i) {
		int n  = 1000;
		int dn = (n >> 2);
		int y0 = y + 1;
		while (y < y2) {
			const QString& sLabel = QString::number(0.001f * float(n), 'f', 1);
			if (fm.horizontalAdvance(sLabel) < w - 6)
				painter.drawLine(w - 4, y, w - 1, y);
			if (y > y0 + (h2 << 1) && y < y2 - (h2 << 1)) {
				painter.drawText(2, y - h2, w - 8, fm.height(),
					Qt::AlignRight | Qt::AlignVCenter, sLabel);
				y0 = y + 1;
			}
			y += dy;
			n -= dn;
		}
		y = y2;
		y2 += hn;
	}
}


//----------------------------------------------------------------------------
// qtractorAudioEditView -- Audio clip view widget.

// Constructor.
qtractorAudioEditView::qtractorAudioEditView (
	qtractorAudioEditor *pEditor, QWidget *pParent )
	: qtractorScrollView(pParent)
{
	m_pEditor = pEditor;

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
qtractorAudioEditView::~qtractorAudioEditView (void)
{
}


// Update track view content width.
void qtractorAudioEditView::updateContentsWidth ( int iContentsWidth )
{
	// Do the contents resize thing...
	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale) {
		const unsigned long f0
			= m_pEditor->offset();
		const unsigned long f1
			= m_pEditor->length();
		const int x0
			= pTimeScale->pixelFromFrame(f0);
		const int w1
			= pTimeScale->pixelFromFrame(f0 + f1) - x0;
		if (iContentsWidth < w1)
			iContentsWidth = w1;
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
}


// Rectangular contents update.
void qtractorAudioEditView::updateContents ( const QRect& rect )
{
	updatePixmap(
		qtractorScrollView::contentsX(),
		qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorAudioEditView::updateContents (void)
{
	updatePixmap(
		qtractorScrollView::contentsX(),
		qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();
}


// Resize event handler.
void qtractorAudioEditView::resizeEvent ( QResizeEvent *pResizeEvent )
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
void qtractorAudioEditView::updatePixmap ( int cx, int /*cy*/ )
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

	const unsigned short iChannels = m_pEditor->channels();
	if (iChannels < 1)
		return;

	QPainter painter(&m_pixmap);
//	painter.initFrom(this);
	painter.setFont(qtractorScrollView::font());

	// Draw horizontal lines...
	painter.setPen(rgbLight);
	const int hn = (h / iChannels);
	const int dy = (hn >> 3);
	int y2 = hn;
	int y = 4;
	for (unsigned short i = 0; i < iChannels; ++i) {
		while (y < y2) {
			painter.drawLine(0, y, w, y);
			y += dy;
		}
		y = y2;
		y2 += hn;
	}

	// Account for the editing offset:
	qtractorTimeScale::Cursor cursor(pTimeScale);
	const unsigned long f0 = m_pEditor->offset();
	const int x0 = pTimeScale->pixelFromFrame(f0);
	const int dx = x0 + cx;

	// Draw vertical grid lines...
	const QBrush zebra(QColor(0, 0, 0, 20));
	qtractorTimeScale::Node *pNode = cursor.seekPixel(dx);
	const unsigned short iSnapPerBeat
		= (m_pEditor->isSnapGrid() ? pTimeScale->snapPerBeat() : 0);

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

	// Draw the zero-lines...
	int y0 = (hn >> 1) + 2;
	for (unsigned short i = 0; i < iChannels; ++i) {
		painter.setPen(rgbLight);
		painter.drawLine(0, y0 - 1, w, y0 - 1);
		painter.setPen(rgbLine);
		painter.drawLine(0, y0, w, y0);
		y0 += hn - 4;
	}

	//
	// Draw the clip(s) contents...
	//

	const int w1 = pTimeScale->pixelFromFrame(f0 + m_pEditor->length()) - dx;
	const int w2 = qMin(w, w1);
	if (w2 > 0) {
		const QRect clipRect(0, 0, w2, h);
		painter.setRenderHint(QPainter::Antialiasing, true);
		drawAudioPeak(painter, dx, clipRect);
		m_pEditor->drawFadeInOut(painter, dx, clipRect);
		painter.setRenderHint(QPainter::Antialiasing, false);
	}

	// Show that we may have clip limits...
	if (m_pEditor->length() > 0) {
		int x1 = pTimeScale->pixelFromFrame(m_pEditor->length()) - cx;
		if (x1 < 0)
			x1 = 0;
		if (x1 < w)
			painter.fillRect(x1, 0, w - x1, h, QColor(0, 0, 0, 80));
	}

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


// Draw the audio peaks (waveforms).
void qtractorAudioEditView::drawAudioPeak (
	QPainter& painter, int dx, const QRect& clipRect )
{
	qtractorAudioClip *pAudioClip = m_pEditor->audioClip();
	if (pAudioClip == nullptr)
		return;

	qtractorTimeScale *pTimeScale = m_pEditor->timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorAudioClip::FractGain *pFractGains = pAudioClip->fractGains();
	if (pFractGains == nullptr)
		return;

	qtractorAudioPeak *pPeak = m_pEditor->audioPeak();
	if (pPeak == nullptr)
		return;

	const int w = qtractorScrollView::viewport()->width();
	const int h = qtractorScrollView::viewport()->height();

	const unsigned long iFrameStart = pTimeScale->frameFromPixel(dx);
	const unsigned long iFrameEnd = pTimeScale->frameFromPixel(dx + w);

	const unsigned long f0 = m_pEditor->offset();
	unsigned long iFrameOffset = iFrameStart;
	if (iFrameOffset < f0)
		iFrameOffset = f0;
	iFrameOffset += pAudioClip->clipOffset();
	iFrameOffset -= f0;

	const unsigned long iFrameLength
		= iFrameEnd - iFrameStart;

	qtractorAudioPeakFile::Frame *pPeakFrames
		= pPeak->peakFrames(iFrameOffset, iFrameLength, w);
	if (pPeakFrames == nullptr)
		return;

	const int w2 = clipRect.width();
	const unsigned int iPeakLength = (pPeak->peakLength() * w2) / w;
	if (iPeakLength < 1)
		return;

	// Polygon init...
	unsigned short k;
	const unsigned short iChannels = pPeak->channels();
	const unsigned int iPolyPoints = (iPeakLength << 1);
	QPolygon **pPolyMax = new QPolygon* [iChannels];
	QPolygon **pPolyRms = new QPolygon* [iChannels];
	for (k = 0; k < iChannels; ++k) {
		pPolyMax[k] = new QPolygon(iPolyPoints);
		pPolyRms[k] = new QPolygon(iPolyPoints);
	}

	// Draw peak chart...
	const int h1 = (h / iChannels);
	const int h2 = (h1 >> 1);

	int x, y, ymax, ymin, yrms;

	// Build polygonal vertexes...
	const int n2 = int(iPeakLength);
	for (int n = 0; n < n2; ++n) {
		x = (n * w2) / n2;
		y = h2;
		for (k = 0; k < iChannels; ++k) {
			const qtractorAudioClip::FractGain& fractGain = pFractGains[k];
			const int h2gain = (h2 * fractGain.num);
			ymax = (h2gain * pPeakFrames->max) >> fractGain.den;
			ymin = (h2gain * pPeakFrames->min) >> fractGain.den;
			yrms = (h2gain * pPeakFrames->rms) >> fractGain.den;
			pPolyMax[k]->setPoint(n, x, y - ymax);
			pPolyMax[k]->setPoint(iPolyPoints - n - 1, x, y + ymin);
			pPolyRms[k]->setPoint(n, x, y - yrms);
			pPolyRms[k]->setPoint(iPolyPoints - n - 1, x, y + yrms);
			y += h1; ++pPeakFrames;
		}
	}

	// Close, draw and free the polygons...
	QColor fg(m_pEditor->foreground().lighter(120));
	fg.setAlpha(200);
	painter.setPen(fg.lighter(120));
	painter.setBrush(fg);
	for (k = 0; k < iChannels; ++k) {
		painter.drawPolygon(*pPolyMax[k]);
		painter.drawPolygon(*pPolyRms[k]);
		delete pPolyRms[k];
		delete pPolyMax[k];
	}

	// Done on polygons.
	delete [] pPolyRms;
	delete [] pPolyMax;
}


// Draw the time scale.
void qtractorAudioEditView::drawContents ( QPainter& painter, const QRect& rect )
{
	painter.drawPixmap(rect, m_pixmap, rect);

#ifdef CONFIG_GRADIENT
	// Draw canvas edge-border shadows...
	const int ws = 22;
	const int xs = qtractorScrollView::viewport()->width() - ws;
	if (rect.left() < ws)
		painter.fillRect(0, rect.top(), ws, rect.bottom(), m_gradLeft);
	if (rect.right() > xs)
		painter.fillRect(xs, rect.top(), xs + ws, rect.bottom(), m_gradRight);
#endif
	m_pEditor->paintDragState(this, painter);

	// Draw special play/edit-head/tail headers...
	const int cx = qtractorScrollView::contentsX();

	int x = m_pEditor->editHeadX() - cx;
	if (x >= rect.left() && x <= rect.right()) {
		painter.setPen(Qt::blue);
		painter.drawLine(x, rect.top(), x, rect.bottom());
	}

	x = m_pEditor->editTailX() - cx;
	if (x >= rect.left() && x <= rect.right()) {
		painter.setPen(Qt::blue);
		painter.drawLine(x, rect.top(), x, rect.bottom());
	}

	x = m_pEditor->playHeadX() - cx;
	if (x >= rect.left() && x <= rect.right()) {
		painter.setPen(Qt::red);
		painter.drawLine(x, rect.top(), x, rect.bottom());
	}
}


// To have event view in h-sync with main view.
void qtractorAudioEditView::contentsXMovingSlot ( int cx, int /*cy*/ )
{
	if (qtractorScrollView::contentsX() != cx)
		qtractorScrollView::setContentsPos(cx, qtractorScrollView::contentsY());
}


// Keyboard event handler.
void qtractorAudioEditView::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	if (!m_pEditor->keyPress(this, pKeyEvent->key(), pKeyEvent->modifiers()))
		qtractorScrollView::keyPressEvent(pKeyEvent);
}


// Handle item selection/dragging -- mouse button press.
void qtractorAudioEditView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse press...
	qtractorScrollView::mousePressEvent(pMouseEvent);

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
void qtractorAudioEditView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse move...
//	qtractorScrollView::mouseMoveEvent(pMouseEvent);

	// Are we already moving/dragging something?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	m_pEditor->dragMoveUpdate(this, pos, pMouseEvent->modifiers());
}


// Handle item selection/dragging -- mouse button release.
void qtractorAudioEditView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	// Process mouse release...
//	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	// Were we moving/dragging something?
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	m_pEditor->dragMoveCommit(this, pos, pMouseEvent->modifiers());
}


// Handle zoom with mouse wheel.
void qtractorAudioEditView::wheelEvent ( QWheelEvent *pWheelEvent )
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
bool qtractorAudioEditView::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	// Not handled here.
	return qtractorScrollView::eventFilter(pObject, pEvent);
}


// end of qtractorAudioEditView.cpp
