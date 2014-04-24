// qtractorTrackView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTrackView.h"
#include "qtractorTrackTime.h"
#include "qtractorTrackList.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorFiles.h"

#include "qtractorAudioClip.h"
#include "qtractorAudioFile.h"
#include "qtractorMidiClip.h"
#include "qtractorMidiFile.h"
#include "qtractorSessionCursor.h"
#include "qtractorFileListView.h"
#include "qtractorClipSelect.h"
#include "qtractorCurveSelect.h"

#include "qtractorOptions.h"

#include "qtractorClipCommand.h"
#include "qtractorCurveCommand.h"

#include "qtractorMainForm.h"
#include "qtractorThumbView.h"

#include <QToolButton>
#include <QScrollBar>
#include <QToolTip>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QKeyEvent>

#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QCursor>
#include <QTimer>
#include <QUrl>
#include <QFileInfo>

#if QT_VERSION >= 0x050000
#include <QMimeData>
#include <QDrag>
#endif

// Follow-playhead: maximum iterations on hold.
#define QTRACTOR_SYNC_VIEW_HOLD 66


//----------------------------------------------------------------------------
// qtractorTrackView::ClipBoard - Local clipaboard singleton.

// Singleton declaration.
qtractorTrackView::ClipBoard qtractorTrackView::g_clipboard;


//----------------------------------------------------------------------------
// qtractorTrackView -- Track view widget.

// Constructor.
qtractorTrackView::qtractorTrackView ( qtractorTracks *pTracks,
	QWidget *pParent ) : qtractorScrollView(pParent)
{
	m_pTracks = pTracks;

	m_pClipSelect    = new qtractorClipSelect();
	m_pCurveSelect   = new qtractorCurveSelect();

	m_pSessionCursor = NULL;
	m_pRubberBand    = NULL;

	m_selectMode = SelectClip;

	m_bDropSpan  = true;
	m_bSnapZebra = true;
	m_bSnapGrid  = true;
	m_bToolTips  = true;

	m_bCurveEdit = false;

	m_pCurveEditCommand = NULL;

	m_bSyncViewHold = false;

	clear();

	// Zoom tool widgets
	m_pHzoomIn    = new QToolButton(this);
	m_pHzoomOut   = new QToolButton(this);
	m_pVzoomIn    = new QToolButton(this);
	m_pVzoomOut   = new QToolButton(this);
	m_pXzoomReset = new QToolButton(this);

	const QIcon& iconZoomIn = QIcon(":/images/viewZoomIn.png");
	m_pHzoomIn->setIcon(iconZoomIn);
	m_pVzoomIn->setIcon(iconZoomIn);

	const QIcon& iconZoomOut = QIcon(":/images/viewZoomOut.png");
	m_pHzoomOut->setIcon(iconZoomOut);
	m_pVzoomOut->setIcon(iconZoomOut);

	m_pXzoomReset->setIcon(QIcon(":/images/viewZoomTool.png"));

	m_pHzoomIn->setAutoRepeat(true);
	m_pHzoomOut->setAutoRepeat(true);
	m_pVzoomIn->setAutoRepeat(true);
	m_pVzoomOut->setAutoRepeat(true);

	m_pHzoomIn->setToolTip(tr("Zoom in (horizontal)"));
	m_pHzoomOut->setToolTip(tr("Zoom out (horizontal)"));
	m_pVzoomIn->setToolTip(tr("Zoom in (vertical)"));
	m_pVzoomOut->setToolTip(tr("Zoom out (vertical)"));
	m_pXzoomReset->setToolTip(tr("Zoom reset"));

	int iScrollBarExtent
		= qtractorScrollView::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	m_pHzoomIn->setFixedWidth(iScrollBarExtent);
	m_pHzoomOut->setFixedWidth(iScrollBarExtent);
	qtractorScrollView::addScrollBarWidget(m_pHzoomIn,  Qt::AlignRight);
	qtractorScrollView::addScrollBarWidget(m_pHzoomOut, Qt::AlignRight);
	m_pVzoomOut->setFixedHeight(iScrollBarExtent);
	m_pVzoomIn->setFixedHeight(iScrollBarExtent);
	qtractorScrollView::addScrollBarWidget(m_pVzoomOut, Qt::AlignBottom);
	qtractorScrollView::addScrollBarWidget(m_pVzoomIn,  Qt::AlignBottom);

	QObject::connect(m_pHzoomIn, SIGNAL(clicked()),
		m_pTracks, SLOT(horizontalZoomInSlot()));
	QObject::connect(m_pHzoomOut, SIGNAL(clicked()),
		m_pTracks, SLOT(horizontalZoomOutSlot()));
	QObject::connect(m_pVzoomIn, SIGNAL(clicked()),
		m_pTracks, SLOT(verticalZoomInSlot()));
	QObject::connect(m_pVzoomOut, SIGNAL(clicked()),
		m_pTracks, SLOT(verticalZoomOutSlot()));
	QObject::connect(m_pXzoomReset, SIGNAL(clicked()),
		m_pTracks, SLOT(viewZoomResetSlot()));

	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::StrongFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);
	qtractorScrollView::setMouseTracking(true);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));

	// Trap for help/tool-tips events.
	qtractorScrollView::viewport()->installEventFilter(this);
}


// Destructor.
qtractorTrackView::~qtractorTrackView (void)
{
	clear();

	delete m_pCurveSelect;
	delete m_pClipSelect;
}


// Track view state reset.
void qtractorTrackView::clear (void)
{
	g_clipboard.clear();

	m_pClipSelect->clear();
	m_pCurveSelect->clear();

	m_dropType   = qtractorTrack::None;
	m_dragState  = DragNone;
	m_dragCursor = DragNone;
	m_iDragClipX = 0;
	m_pClipDrag  = NULL;
	m_bDragTimer = false;

	m_iPlayHead  = 0;

	m_iPlayHeadX = 0;
	m_iEditHeadX = 0;
	m_iEditTailX = 0;

	m_iLastRecordX = 0;
	
	m_iPasteCount  = 0;
	m_iPastePeriod = 0;

	m_iSyncViewHold = 0;

	if (m_pSessionCursor)
		delete m_pSessionCursor;
	m_pSessionCursor = NULL;

	if (m_pRubberBand)
		delete m_pRubberBand;
	m_pRubberBand = NULL;

	if (m_pCurveEditCommand)
		delete m_pCurveEditCommand;
	m_pCurveEditCommand = NULL;
	
	m_bDragSingleTrack = false;
	m_iDragSingleTrackY = 0;
	m_iDragSingleTrackHeight = 0;

	m_pDragCurveNode = NULL;
	m_iDragCurveX = 0;

	qtractorScrollView::setContentsPos(0, 0);
}


// Update track view content height.
void qtractorTrackView::updateContentsHeight (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Allways give some room to drop something at the bottom...
	int iContentsHeight = qtractorTrack::HeightBase << 1;
	// Compute total track height...
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		iContentsHeight += pTrack->zoomHeight();
		pTrack = pTrack->next();
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTrackView::updateContentsHeight(%d)", iContentsHeight);
#endif

	// Do the contents resize thing...
	qtractorScrollView::resizeContents(
		qtractorScrollView::contentsWidth(), iContentsHeight);

	// Keep selection (we'll update all contents anyway)...
	if (m_bCurveEdit)
		updateCurveSelect();
	else
		updateClipSelect();
}


// Update track view content width.
void qtractorTrackView::updateContentsWidth ( int iContentsWidth )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		const int iSessionWidth
			= pSession->pixelFromFrame(pSession->sessionEnd());
		if (iContentsWidth < iSessionWidth)
			iContentsWidth = iSessionWidth;
		qtractorTimeScale::Cursor cursor(pSession->timeScale());
		qtractorTimeScale::Node *pNode = cursor.seekPixel(iContentsWidth);
		iContentsWidth += pNode->pixelFromBeat(
			pNode->beat + 2 * pNode->beatsPerBar) - pNode->pixel;
		if (iContentsWidth < qtractorScrollView::width())
			iContentsWidth += qtractorScrollView::width();
		m_iPlayHeadX = pSession->pixelFromFrame(pSession->playHead());
		m_iEditHeadX = pSession->pixelFromFrame(pSession->editHead());
		m_iEditTailX = pSession->pixelFromFrame(pSession->editTail());
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTrackView::updateContentsWidth(%d)", iContentsWidth);
#endif

	// Do the contents resize thing...
	qtractorScrollView::resizeContents(
		iContentsWidth, qtractorScrollView::contentsHeight());

	// Keep selection (we'll update all contents anyway)...
	if (m_bCurveEdit)
		updateCurveSelect();
	else
		updateClipSelect();

	// Force an update on the track time line too...
	m_pTracks->trackTime()->resizeContents(
		iContentsWidth + 100, m_pTracks->trackTime()->viewport()->height());
	m_pTracks->trackTime()->updateContents();
}


// Local rectangular contents update.
void qtractorTrackView::updateContents ( const QRect& rect )
{
	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
}


// Overall contents update.
void qtractorTrackView::updateContents (void)
{
	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();
}


// Special recording visual feedback.
void qtractorTrackView::updateContentsRecord (void)
{
	QWidget *pViewport = qtractorScrollView::viewport();
	const int cx = qtractorScrollView::contentsX();
	int w = m_iPlayHeadX - cx;
	if (w > 0 && w < pViewport->width()) {
		int x = 0, dx = 8;
		if (m_iPlayHeadX > m_iLastRecordX) {
			dx += (m_iPlayHeadX - m_iLastRecordX);
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession && pSession->midiRecord() < 1) {
				w = dx;
				if (m_iLastRecordX > cx + dx)
					x = m_iLastRecordX - (cx + dx);
			}
		}
		else w = m_iLastRecordX - cx + dx;
		m_iLastRecordX = m_iPlayHeadX;
		pViewport->update(QRect(x, 0, w + dx, pViewport->height()));
	}
}

	
// Draw the track view.
void qtractorTrackView::drawContents ( QPainter *pPainter, const QRect& rect )
{
	// Draw viewport canvas...
	pPainter->drawPixmap(rect, m_pixmap, rect);

	// Lines a-head...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const int cx = qtractorScrollView::contentsX();
	const int cy = qtractorScrollView::contentsY();
	const int ch = qtractorScrollView::contentsHeight();
	int x, w;

	// Draw track clip selection...
	if (isClipSelected()) {
		const QRect& rectView = qtractorScrollView::viewport()->rect();
		const qtractorClipSelect::ItemList& items = m_pClipSelect->items();
		qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
		const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
		for ( ; iter != iter_end; ++iter) {
			qtractorClip *pClip = iter.key();
			// Make sure it's a legal selection...
			if (pClip->track() && pClip->isClipSelected()) {
				qtractorClipSelect::Item *pClipItem = iter.value();
				QRect rectClip(pClipItem->rectClip);
				rectClip.moveTopLeft(
					qtractorScrollView::contentsToViewport(rectClip.topLeft()));
				rectClip = rectClip.intersected(rectView);
				if (!rectClip.isEmpty())
					pPainter->fillRect(rectClip, QColor(0, 0, 255, 120));
			}
		}
	}

	// Common stuff for the job(s) ahead...
	unsigned long iTrackStart = 0;
	unsigned long iTrackEnd   = 0;

	int y1 = 0;
	int y2 = 0;

	// On-the-fly recording clip drawing...
	if (pSession->isRecording() && pSession->isPlaying()) {
		iTrackStart = pSession->frameFromPixel(cx + rect.x());
		iTrackEnd   = pSession->playHead();
		if (iTrackStart < iTrackEnd) {
			const unsigned long iFrameTime = pSession->frameTimeEx();
			const unsigned long iFrameOffset = iFrameTime - iTrackEnd;
			const unsigned long iLoopStart = pSession->loopStart();
			const unsigned long iLoopEnd = pSession->loopEnd();
			qtractorTrack *pTrack = pSession->tracks().first();
			while (pTrack && y2 < ch) {
				y1  = y2;
				y2 += pTrack->zoomHeight();
				// Dispatch to paint this track...
				qtractorClip *pClipRecord = pTrack->clipRecord();
				if (pClipRecord && y2 > cy) {
					const int h = y2 - y1 - 2;
					const QRect trackRect(
						rect.left() - 1, y1 - cy + 1, rect.width() + 2, h);
					// Track/clip background colors...
					QColor bg = pTrack->background();
					pPainter->setPen(bg.darker());
					bg.setAlpha(192); // translucency...
				#ifdef CONFIG_GRADIENT
					const int y = trackRect.y();
					QLinearGradient grad(0, y, 0, y + h);
					grad.setColorAt(0.4, bg);
					grad.setColorAt(1.0, bg.darker(130));
					pPainter->setBrush(grad);
				#else
					pPainter->setBrush(bg);
				#endif
					unsigned long iClipStart  = pClipRecord->clipStart();
					unsigned long iClipOffset = 0;
					if (iLoopStart < iLoopEnd && !pSession->isPunching()) {
						// Clip recording started within loop range:
						// -- adjust turn-around clip offset...
						if (iClipStart > iLoopStart && iClipStart < iLoopEnd) {
							unsigned long iHeadLength = (iLoopEnd - iClipStart);
							unsigned long iLoopLength = (iLoopEnd - iLoopStart);
							unsigned long iClipLength
								= (iFrameTime - pTrack->clipRecordStart());
							if (iClipLength > iHeadLength) {
								unsigned long iLoopCount
									= (iClipLength - iHeadLength) / iLoopLength;
								iClipOffset += iHeadLength + iLoopCount * iLoopLength;
								iClipStart = iLoopStart;
							}
						}
						else
						// Clip recording is within loop range:
						// -- redraw leading/head clip segment...
						if (iFrameTime > iLoopStart) {
							unsigned long iHeadOffset = 0;
							if (iClipStart < iTrackStart)
								iHeadOffset += iTrackStart - iClipStart;
							x = pSession->pixelFromFrame(iClipStart) - cx;
							w = 0;
							if (iClipStart < iLoopStart) {
								w += pSession->pixelFromFrame(iLoopStart - iClipStart);
								iClipOffset += iLoopStart - iClipStart;
							}
							const QRect& headRect
								= QRect(x, y1 - cy + 1, w, h).intersected(trackRect);
							if (!headRect.isEmpty()) {
								pClipRecord->drawClip(pPainter, headRect, iHeadOffset);
								pPainter->fillRect(headRect, QColor(255, 0, 0, 120));
							}
							iClipOffset += iFrameOffset;
							iClipStart = iLoopStart;
						}
					}
					// Clip recording rolling:
					// -- redraw current clip segment...
					if (iClipStart < iTrackStart)
						iClipOffset += iTrackStart - iClipStart;
					x = pSession->pixelFromFrame(iClipStart) - cx;
					w = 0;
					if (iClipStart < iTrackEnd)
						w += pSession->pixelFromFrame(iTrackEnd - iClipStart);
					const QRect& clipRect
						= QRect(x, y1 - cy + 1, w, h).intersected(trackRect);
					if (!clipRect.isEmpty()) {
						pClipRecord->drawClip(pPainter, clipRect, iClipOffset);
						pPainter->fillRect(clipRect, QColor(255, 0, 0, 120));
					}
				}
				pTrack = pTrack->next();
			}
		}
		// Make-up for next job...
		y1 = y2 = 0;
		iTrackEnd = 0;
	}
	
	// Automation curve drawing...
	pPainter->setRenderHint(QPainter::Antialiasing, true);

	x = rect.left();
	w = rect.width();

	if (w < 8) {
		x -= 4; if (x < 0) x = 0;
		w += 8;
	}

	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack && y2 < ch) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		qtractorCurve *pCurve = pTrack->currentCurve();
		if (pCurve && y2 > cy) {
			const int h = y2 - y1 - 2;
			const QRect trackRect(x, y1 - cy + 1, w, h);
			if (iTrackStart == 0)
				iTrackStart = pSession->frameFromPixel(cx + x);
			if (iTrackEnd == 0)
				iTrackEnd = pSession->frameFromPixel(cx + x + w);
			unsigned long frame = iTrackStart;
			qtractorCurve::Cursor cursor(pCurve);
			qtractorCurve::Node *pNode = cursor.seek(frame);
			qtractorCurve::Mode mode = pCurve->mode();
			const bool bLogarithmic = pCurve->isLogarithmic();
			const bool bLocked = pCurve->isLocked();
			int xc2, xc1 = trackRect.x();
			int yc2, yc1 = y2 - int(cursor.scale(pNode, frame) * float(h)) - cy;
			int yc3, xc3 = xc1 + 4;
			QColor rgbCurve(pCurve->color());
			QPen pen(rgbCurve);
			pPainter->setPen(pen);
			QPainterPath path;
			path.moveTo(xc1, yc1);
			while (pNode && pNode->frame < iTrackEnd) {
				xc2 = pSession->pixelFromFrame(pNode->frame) - cx;
				yc2 = y2 - int(cursor.scale(pNode) * float(h)) - cy;
				if (!bLocked)
					pPainter->drawRect(QRect(xc2 - 4, yc2 - 4, 8, 8));
				switch (mode) {
				case qtractorCurve::Hold:
					path.lineTo(xc2, yc1);
					yc1 = yc2;
					break;
				case qtractorCurve::Linear:
					if (!bLogarithmic)
						break;
					// Fall thru...
				case qtractorCurve::Spline:
				default:
					for ( ; xc3 < xc2 - 4; xc3 += 4) {
						frame = pSession->frameFromPixel(cx + xc3);
						yc3 = y2 - int(cursor.scale(frame) * float(h)) - cy;
						path.lineTo(xc3, yc3);
					}
					xc3 = xc2 + 4;
					break;
				}
				path.lineTo(xc2, yc2);
				pNode = pNode->next();
			}
			xc2 = rect.right();
			frame = pSession->frameFromPixel(cx + xc2);
			yc2 = y2 - int(cursor.scale(frame) * float(h)) - cy;
			switch (mode) {
			case qtractorCurve::Hold:
				path.lineTo(xc2, yc1);
				break;
			case qtractorCurve::Linear:
				if (!bLogarithmic)
					break;
				// Fall thru...
			case qtractorCurve::Spline:
			default:
				for ( ; xc3 < xc2 - 4; xc3 += 4) {
					frame = pSession->frameFromPixel(cx + xc3);
					yc3 = y2 - int(cursor.scale(frame) * float(h)) - cy;
					path.lineTo(xc3, yc3);
				}
				break;
			}
			path.lineTo(xc2, yc2);
			// Draw line...
			//pen.setWidth(2);
			pPainter->strokePath(path, pen);	
			// Fill semi-transparent area...
			rgbCurve.setAlpha(60);
			path.lineTo(xc2, y2 - cy);
			path.lineTo(xc1, y2 - cy);
			pPainter->fillPath(path, rgbCurve);
			if (m_bCurveEdit && m_pCurveSelect->isCurrentCurve(pCurve)) {
				const qtractorCurveSelect::ItemList& items
					= m_pCurveSelect->items();
				qtractorCurveSelect::ItemList::ConstIterator iter
					= items.constBegin();
				const qtractorCurveSelect::ItemList::ConstIterator iter_end
					= items.constEnd();
				for ( ; iter != iter_end; ++iter) {
					qtractorCurveSelect::Item *pItem = iter.value();
					if (pItem->flags & 1) {
						QRect rectNode(pItem->rectNode);
						rectNode.moveTopLeft(contentsToViewport(
							rectNode.topLeft() + QPoint(m_iDragCurveX, 0)));
						pPainter->fillRect(rectNode, QColor(0, 0, 255, 80));
					}
				}
			}
		}
		pTrack = pTrack->next();
	}

	pPainter->setRenderHint(QPainter::Antialiasing, false);

	// Draw edit-head line...
	//m_iEditHeadX = pSession->pixelFromFrame(pSession->editHead());
	x = m_iEditHeadX - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::blue);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	// Draw edit-tail line...
	//m_iEditTailX = pSession->pixelFromFrame(pSession->editTail());
	x = m_iEditTailX - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::blue);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	// Draw play-head line...
	//m_iPlayHeadX = pSession->pixelFromFrame(pSession->playHead());
	x = m_iPlayHeadX - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::red);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	// Show/hide a moving clip fade in/out slope lines...
	if (m_dragState == DragClipFadeIn || m_dragState == DragClipFadeOut) {
		QRect rectHandle(m_rectHandle);
		// Horizontal adjust...
		rectHandle.translate(m_iDragClipX, 0);
		// Convert rectangle into view coordinates...
		rectHandle.moveTopLeft(contentsToViewport(rectHandle.topLeft()));
		// Draw envelope line...
		QPoint vpos;
		QPen pen(Qt::DotLine);
		pen.setColor(Qt::blue);
		pPainter->setPen(pen);
		if (m_dragState == DragClipFadeIn) {
			vpos = contentsToViewport(m_rectDrag.bottomLeft());
			pPainter->drawLine(
				vpos.x(), vpos.y(), rectHandle.left(), rectHandle.top());
		} 
		else 
		if (m_dragState == DragClipFadeOut) {
			vpos = contentsToViewport(m_rectDrag.bottomRight());
			pPainter->drawLine(
				rectHandle.right(), rectHandle.top(), vpos.x(), vpos.y());
		}
	}
}


// Resize event handler.
void qtractorTrackView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorScrollView::resizeEvent(pResizeEvent);

	// Corner tool widget layout management...
	if (m_pXzoomReset) {
		const QSize& size = qtractorScrollView::size();
		int h = size.height();
		int w = qtractorScrollView::style()->pixelMetric(
			QStyle::PM_ScrollBarExtent);
		int x = size.width() - w - 2;
		m_pXzoomReset->setGeometry(x, h - w - 2, w, w);
	}

	updateContents();

	// HACK: let our (single) thumb view get notified...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->thumbView())
		pMainForm->thumbView()->updateThumb();
}


// (Re)create the complete track view pixmap.
void qtractorTrackView::updatePixmap ( int cx, int cy )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	const QPalette& pal = qtractorScrollView::palette();

	if (w < 1 || h < 1)
		return;

	const QColor& rgbMid = pal.mid().color();
	
	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(rgbMid);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return;

	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	// Update view session cursor location,
	// so that we'll start drawing clips from there...
	const unsigned long iTrackStart = pTimeScale->frameFromPixel(cx);
	const unsigned long iTrackEnd   = iTrackStart + pTimeScale->frameFromPixel(w);
	// Create cursor now if applicable...
	if (m_pSessionCursor == NULL) {
		m_pSessionCursor = pSession->createSessionCursor(iTrackStart);
	} else {
		m_pSessionCursor->seek(iTrackStart);
	}

	const QColor& rgbLight = pal.midlight().color();
	const QColor& rgbDark  = rgbMid.darker(120);

	// Draw vertical grid lines...
	if (m_bSnapGrid || m_bSnapZebra) {
		const QBrush zebra(QColor(0, 0, 0, 20));
		qtractorTimeScale::Cursor cursor(pTimeScale);
		qtractorTimeScale::Node *pNode = cursor.seekPixel(cx);
		unsigned short iPixelsPerBeat = pNode->pixelsPerBeat();
		unsigned int iBeat = pNode->beatFromPixel(cx);
		if (iBeat > 0) pNode = cursor.seekBeat(--iBeat);
		unsigned short iBar = pNode->barFromBeat(iBeat);
		int x = pNode->pixelFromBeat(iBeat) - cx;
		int x2 = x;
		while (x < w) {
			bool bBeatIsBar = pNode->beatIsBar(iBeat);
			if (bBeatIsBar) {
				if (m_bSnapGrid) {
					painter.setPen(rgbLight);
					painter.drawLine(x, 0, x, h);
				}
				if (m_bSnapZebra && (x > x2) && (++iBar & 1))
					painter.fillRect(QRect(x2, 0, x - x2 + 1, h), zebra);
				x2 = x;
				if (iBeat == pNode->beat)
					iPixelsPerBeat = pNode->pixelsPerBeat();
			}
			if (m_bSnapGrid && (bBeatIsBar || iPixelsPerBeat > 16)) {
				painter.setPen(rgbDark);
				painter.drawLine(x - 1, 0, x - 1, h);
			}
			pNode = cursor.seekBeat(++iBeat);
			x = pNode->pixelFromBeat(iBeat) - cx;
		}
		if (m_bSnapZebra && (x > x2) && (++iBar & 1))
			painter.fillRect(QRect(x2, 0, x - x2 + 1, h), zebra);
	}

	// Draw track and horizontal lines...
	int y1, y2;
	y1 = y2 = 0;
	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack && y2 < cy + h) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (y2 > cy) {
			// Dispatch to paint this track...
			if (y1 > cy) {
				painter.setPen(rgbLight);
				painter.drawLine(0, y1 - cy, w, y1 - cy);
			}
			const QRect trackRect(0, y1 - cy + 1, w, y2 - y1 - 2);
		//	painter.fillRect(trackRect, rgbMid);
			pTrack->drawTrack(&painter, trackRect, iTrackStart, iTrackEnd,
				m_pSessionCursor->clip(iTrack));
			painter.setPen(rgbDark);
			painter.drawLine(0, y2 - cy - 1, w, y2 - cy - 1);
		}
		pTrack = pTrack->next();
		++iTrack;
	}

	// Fill the empty area...
	if (y2 < cy + h) {
		painter.setPen(rgbMid);
		painter.drawLine(0, y2 - cy, w, y2 - cy);
		painter.fillRect(0, y2 - cy + 1, w, h, pal.dark().color());
	}

	// Draw location marker lines...
	qtractorTimeScale::Marker *pMarker
		= pTimeScale->markers().seekPixel(cx);
	while (pMarker) {
		int x = pTimeScale->pixelFromFrame(pMarker->frame) - cx;
		if (x > w) break;
		painter.setPen(pMarker->color);
		painter.drawLine(x, 0, x, h);
		pMarker = pMarker->next();
	}

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		const QBrush shade(QColor(0, 0, 0, 60));
		painter.setPen(Qt::darkCyan);
		int x = pTimeScale->pixelFromFrame(pSession->loopStart()) - cx;
		if (x >= w)
			painter.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x >= 0) {
			painter.fillRect(QRect(0, 0, x, h), shade);
			painter.drawLine(x, 0, x, h);
		}
		x = pTimeScale->pixelFromFrame(pSession->loopEnd()) - cx;
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
		int x = pTimeScale->pixelFromFrame(pSession->punchIn()) - cx;
		if (x >= w)
			painter.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x >= 0) {
			painter.fillRect(QRect(0, 0, x, h), shade);
			painter.drawLine(x, 0, x, h);
		}
		x = pTimeScale->pixelFromFrame(pSession->punchOut()) - cx;
		if (x < 0)
			painter.fillRect(QRect(0, 0, w, h), shade);
		else
		if (x < w) {
			painter.fillRect(QRect(x, 0, w - x, h), shade);
			painter.drawLine(x, 0, x, h);
		}
	}
}


// To have track view in v-sync with track list.
void qtractorTrackView::contentsYMovingSlot ( int /*cx*/, int cy )
{
	if (qtractorScrollView::contentsY() != cy)
		qtractorScrollView::setContentsPos(qtractorScrollView::contentsX(), cy);
}


// Get track from given contents vertical position.
qtractorTrack *qtractorTrackView::trackAt ( const QPoint& pos,
	bool bSelectTrack, qtractorTrackViewInfo *pTrackViewInfo ) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL || m_pSessionCursor == NULL)
		return NULL;

	int y1 = 0;
	int y2 = 0;
	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (y2 > pos.y())
			break;
		pTrack = pTrack->next();
		++iTrack;
	}

	if (bSelectTrack)
		m_pTracks->trackList()->setCurrentTrackRow(pTrack ? iTrack : -1);

	if (pTrackViewInfo) {
		const int x = qtractorScrollView::contentsX();
		const int w = qtractorScrollView::width();// View width, not contents.
		if (pTrack == NULL) {				// Below all tracks.
			y1 = y2;
			y2 = y1 + (qtractorTrack::HeightBase * pSession->verticalZoom()) / 100;
		}
		pTrackViewInfo->trackIndex = iTrack;
		pTrackViewInfo->trackStart = m_pSessionCursor->frame();
		pTrackViewInfo->trackEnd   = pTrackViewInfo->trackStart
			+ pSession->frameFromPixel(w);
		pTrackViewInfo->trackRect.setRect(x, y1 + 1, w, y2 - y1 - 2);
	}

	return pTrack;
}


// Get clip from given contents position.
qtractorClip *qtractorTrackView::clipAtTrack (
	const QPoint& pos, QRect *pClipRect,
	qtractorTrack *pTrack, qtractorTrackViewInfo *pTrackViewInfo ) const
{
	if (pTrack == NULL)
		return NULL;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL || m_pSessionCursor == NULL)
		return NULL;

	qtractorClip *pClip = m_pSessionCursor->clip(pTrackViewInfo->trackIndex);
	if (pClip == NULL)
		pClip = pTrack->clips().first();
	if (pClip == NULL)
		return NULL;

	qtractorClip *pClipAt = NULL;
	while (pClip && pClip->clipStart() < pTrackViewInfo->trackEnd) {
		const int x = pSession->pixelFromFrame(pClip->clipStart());
		const int w = pSession->pixelFromFrame(pClip->clipLength());
		if (pos.x() >= x && x + w >= pos.x()) {
			pClipAt = pClip;
			if (pClipRect) {
				pClipRect->setRect(
					x, pTrackViewInfo->trackRect.y(),
					w, pTrackViewInfo->trackRect.height());
			}
		}
		pClip = pClip->next();
	}

	return pClipAt;
}


qtractorClip *qtractorTrackView::clipAt (
	const QPoint& pos, bool bSelectTrack, QRect *pClipRect ) const
{
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, bSelectTrack, &tvi);
	return clipAtTrack(pos, pClipRect, pTrack, &tvi);
}


// Get automation curve node from given contents position.
qtractorCurve::Node *qtractorTrackView::nodeAtTrack ( const QPoint& pos,
	qtractorTrack *pTrack, qtractorTrackViewInfo *pTrackViewInfo ) const
{
	if (pTrack == NULL)
		return NULL;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL || m_pSessionCursor == NULL)
		return NULL;

	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList == NULL)
		return NULL;

	qtractorCurve *pCurve = pCurveList->currentCurve();
	if (pCurve == NULL)
		return NULL;
	if (pCurve->isLocked())
		return NULL;

	const int w  = pTrackViewInfo->trackRect.width();
	const int h  = pTrackViewInfo->trackRect.height();
	const int y2 = pTrackViewInfo->trackRect.bottom() + 1;
	const int cx = qtractorScrollView::contentsX();

	const unsigned long frame = pSession->frameFromPixel(cx);
	qtractorCurve::Cursor cursor(pCurve);
	qtractorCurve::Node *pNode = cursor.seek(frame);

	while (pNode) {
		const int x = pSession->pixelFromFrame(pNode->frame);
		if (x > cx + w) // No use....
			break;
		const int y = y2 - int(cursor.scale(pNode) * float(h));
		if (QRect(x - 4, y - 4, 8, 8).contains(pos))
			return pNode;
		// Test next...
		pNode = pNode->next();
	}

	// Not found.
	return NULL;
}


qtractorCurve::Node *qtractorTrackView::nodeAt ( const QPoint& pos ) const
{
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, false, &tvi);
	return nodeAtTrack(pos, pTrack, &tvi);
}


// Get contents visible rectangle from given track.
bool qtractorTrackView::trackInfo (
	qtractorTrack *pTrack, qtractorTrackViewInfo *pTrackViewInfo ) const
{
	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL || m_pSessionCursor == NULL)
		return false;

	int y1, y2 = 0;
	int iTrack = 0;
	qtractorTrack *pTrackEx = pSession->tracks().first();
	while (pTrackEx) {
		y1  = y2;
		y2 += pTrackEx->zoomHeight();
		if (pTrackEx == pTrack) {
			const int x = qtractorScrollView::contentsX();
			const int w = qtractorScrollView::width(); // View width, not contents.
			pTrackViewInfo->trackIndex = iTrack;
			pTrackViewInfo->trackStart = m_pSessionCursor->frame();
			pTrackViewInfo->trackEnd   = pTrackViewInfo->trackStart
				+ pSession->frameFromPixel(w);
			pTrackViewInfo->trackRect.setRect(x, y1 + 1, w, y2 - y1 - 2);
			return true;
		}
		pTrackEx = pTrackEx->next();
		++iTrack;
	}

	return false;
}


// Get contents rectangle from given clip.
bool qtractorTrackView::clipInfo (
	qtractorClip *pClip, QRect *pClipRect ) const
{
	qtractorTrack *pTrack = pClip->track();
	if (pTrack == NULL)
		return false;

	qtractorTrackViewInfo tvi;
	if (!trackInfo(pTrack, &tvi))
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession) {
		const int x = pSession->pixelFromFrame(pClip->clipStart());
		const int w = pSession->pixelFromFrame(pClip->clipLength());
		pClipRect->setRect(x, tvi.trackRect.y(), w, tvi.trackRect.height());
	}

	return true;
}


// Selection-dragging, following the current mouse position.
qtractorTrack *qtractorTrackView::dragClipMove (
	const QPoint& pos, bool bKeyStep )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	// Which track we're pointing at?
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, true, &tvi);

	// May change vertically, if we've only one track selected,
	// and only between same track type...
	qtractorTrack *pSingleTrack = m_pClipSelect->singleTrack();
	if (pSingleTrack &&
		(pTrack == NULL || pSingleTrack->trackType() == pTrack->trackType())) {
		m_bDragSingleTrack = true;
		m_iDragSingleTrackY = tvi.trackRect.y();
		m_iDragSingleTrackHeight = tvi.trackRect.height();
	} else {
		m_bDragSingleTrack = false;
		m_iDragSingleTrackY = 0;
		m_iDragSingleTrackHeight = 0;
	}

	// Special update on keyboard vertical drag-stepping...
	if (bKeyStep)
		m_posStep.setY(m_posStep.y() - pos.y() + tvi.trackRect.y()
			+ (pTrack ? (tvi.trackRect.height() >> 1) : 0));

	// Always change horizontally wise...
	const int x = m_pClipSelect->rect().x();
	int dx = (pos.x() - m_posDrag.x());
	if (x + dx < 0)
		dx = -(x);	// Force to origin (x=0).
	m_iDragClipX = (pSession->pixelSnap(x + dx) - x);
	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);

	showClipSelect();

	// OK, we've moved it...
	return pTrack;
}


qtractorTrack *qtractorTrackView::dragClipDrop (
	const QPoint& pos, bool bKeyStep, const QMimeData *pMimeData )
{
	// It must be a valid session...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	// Find the current pointer track...
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, true, &tvi);

	// Special update on keyboard vertical drag-stepping...
	if (bKeyStep)
		m_posStep.setY(m_posStep.y() - pos.y() + tvi.trackRect.y()
			+ (pTrack ? (tvi.trackRect.height() >> 1) : 0));

	// If we're already dragging something,
	if (!m_dropItems.isEmpty()) {
		// Adjust to target track...
		updateClipDropRects(tvi.trackRect.y() + 1, tvi.trackRect.height() - 2);
		// Always change horizontally wise...
		const int x = m_rectDrag.x();
		int dx = (pos.x() - m_posDrag.x());
		if (x + dx < 0)
			dx = -(x);	// Force to origin (x=0).
		m_iDragClipX = (pSession->pixelSnap(x + dx) - x);
	//	showDropRects();
		// OK, we've moved it...
		return pTrack;
	}

	// Let's start from scratch...
	qDeleteAll(m_dropItems);
	m_dropItems.clear();
	m_dropType = qtractorTrack::None;

	// Nothing more?
	if (pMimeData == NULL)
		return NULL;
	// Can it be single track channel (MIDI for sure)?
	if (qtractorFileChannelDrag::canDecode(pMimeData)) {
		// Let's see how many track-channels are there...
		const qtractorFileChannelDrag::List& items
			= qtractorFileChannelDrag::decode(pMimeData);
		QListIterator<qtractorFileChannelDrag::Item> iter(items);
		while (iter.hasNext()) {
			const qtractorFileChannelDrag::Item& item = iter.next();
			m_dropItems.append(new DropItem(item.path, item.channel));
		}
	}
	else
	// Can we decode it as Audio/MIDI files?
	if (pMimeData->hasUrls()) {
		// Let's see how many files there are...
		QList<QUrl> list = pMimeData->urls();
		QListIterator<QUrl> iter(list);
		while (iter.hasNext()) {
			const QString& sPath = iter.next().toLocalFile();
			// Close current session and try to load the new one...
			if (!sPath.isEmpty())
				m_dropItems.append(new DropItem(sPath));
		}
	}

	// Nice, now we'll try to set a preview selection rectangle set...
	m_posDrag.setX(pSession->pixelSnap(pos.x() > 8 ? pos.x() - 8 : 0));
	m_posDrag.setY(tvi.trackRect.y() + 1);
	m_rectDrag.setRect(
		m_posDrag.x(), m_posDrag.y(), 0, tvi.trackRect.height() - 2);

	// Now's time to add those rectangles...
	QMutableListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		DropItem *pDropItem = iter.next();
		// First test as a MIDI file...
		if (m_dropType == qtractorTrack::None
			|| m_dropType == qtractorTrack::Midi) {
			const int x0 = m_posDrag.x();
			qtractorTimeScale::Cursor cursor(pSession->timeScale());
			qtractorTimeScale::Node *pNode = cursor.seekPixel(x0);
			unsigned long t1, t0 = pNode->tickFromPixel(x0);
			qtractorMidiFile file;
			if (file.open(pDropItem->path)) {
				qtractorMidiSequence seq;
				seq.setTicksPerBeat(pSession->ticksPerBeat());
				if (pDropItem->channel < 0) {
					int iTracks = (file.format() == 1 ? file.tracks() : 16);
					for (int iTrackChannel = 0;
							iTrackChannel < iTracks; ++iTrackChannel) {
						if (file.readTrack(&seq, iTrackChannel)
							&& seq.duration() > 0) {
							t1 = t0 + seq.duration();
							pNode = cursor.seekTick(t1);
							m_rectDrag.setWidth(pNode->pixelFromTick(t1) - x0);
							pDropItem->rect = m_rectDrag;
							m_rectDrag.translate(0, m_rectDrag.height() + 4);
						}
					}
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Midi;
				} else if (file.readTrack(&seq, pDropItem->channel)
					&& seq.duration() > 0) {
					t1 = t0 + seq.duration();
					pNode = cursor.seekTick(t1);
					m_rectDrag.setWidth(pNode->pixelFromTick(t1) - x0);
					pDropItem->rect = m_rectDrag;
					m_rectDrag.translate(0, m_rectDrag.height() + 4);
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Midi;
				} else /*if (m_dropType == qtractorTrack::Midi)*/ {
					iter.remove();
					delete pDropItem;
				}
				file.close();
				continue;
			} else if (m_dropType == qtractorTrack::Midi) {
				iter.remove();
				delete pDropItem;
			}
		}
		// Then as an Audio file ?
		if (m_dropType == qtractorTrack::None
			|| m_dropType == qtractorTrack::Audio) {
			qtractorAudioFile *pFile
				= qtractorAudioFileFactory::createAudioFile(pDropItem->path);
			if (pFile) {
				if (pFile->open(pDropItem->path)) {
					unsigned long iFrames = pFile->frames();
					if (pFile->sampleRate() > 0
						&& pFile->sampleRate() != pSession->sampleRate()) {
						iFrames = (unsigned long) (iFrames
							* float(pSession->sampleRate())
							/ float(pFile->sampleRate()));
					}
					m_rectDrag.setWidth(pSession->pixelFromFrame(iFrames));
					pDropItem->rect = m_rectDrag;
					m_rectDrag.translate(0, m_rectDrag.height() + 4);
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Audio;
				} else if (m_dropType == qtractorTrack::Audio) {
					iter.remove();
					delete pDropItem;
				}
				delete pFile;
				continue;
			} else if (m_dropType == qtractorTrack::Audio) {
				iter.remove();
				delete pDropItem;
			}
		}
	}

	// Are we still here?
	if (m_dropItems.isEmpty() || m_dropType == qtractorTrack::None) {
		m_dropType = qtractorTrack::None;
		return NULL;
	}

	// Ok, sure we're into some drag&drop state...
	m_dragState  = DragClipDrop;
	m_iDragClipX = 0;

	// Finally, show it to the world...
	updateClipDropRects(tvi.trackRect.y() + 1, tvi.trackRect.height() - 2);
//	showClipDropRects();

	// Done.
	return pTrack;
}


qtractorTrack *qtractorTrackView::dragClipDropEvent ( QDropEvent *pDropEvent )
{
	return dragClipDrop(
		viewportToContents(pDropEvent->pos()),
		false, pDropEvent->mimeData());
}


bool qtractorTrackView::canClipDropEvent ( QDropEvent *pDropEvent )
{
	// have one existing track on target?
	qtractorTrack *pTrack = dragClipDropEvent(pDropEvent);

	// Can only drop if anything...
	if (m_dropItems.isEmpty())
		return false;

	// Can only drop on same type tracks...
	if (pTrack && pTrack->trackType() != m_dropType)
		return false;

	// Special MIDI track-channel cases...
	if (m_dropType == qtractorTrack::Midi) {
		if (m_dropItems.count() == 1 && m_dropItems.first()->channel >= 0)
			return true;
		else
			return (pTrack == NULL);
	}

	// Drop in the blank...
	return (pTrack == NULL || m_dropItems.count() == 1 || m_bDropSpan);
}


// Selection-dragging, following the current mouse position.
void qtractorTrackView::dragCurveMove ( const QPoint& pos, bool /*bKeyStep*/ )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QRect& rect = m_pCurveSelect->rect();
	QRect rectUpdate(rect.translated(m_iDragCurveX, 0));

	// Always change horizontally wise...
	const int x = rect.x();
	int dx = (pos.x() - m_posDrag.x());
	if (x + dx < 0)
		dx = -(x);	// Force to origin (x=0).
	m_iDragCurveX = (pSession->pixelSnap(x + dx) - x);
	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);

	updateRect(rectUpdate.united(rect.translated(m_iDragCurveX, 0)));
}


// Drag enter event handler.
void qtractorTrackView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
#if 0
	if (canDropEvent(pDragEnterEvent)) {
		showDropRects();
		if (!pDragEnterEvent->isAccepted()) {
			pDragEnterEvent->setDropAction(Qt::CopyAction);
			pDragEnterEvent->accept();
			m_bDragTimer = false;
		}
	} else {
		pDragEnterEvent->ignore();
		hideDropRects();
	}
#else
	// Always accept the drag-enter event,
	// so let we deal with it during move later...
	pDragEnterEvent->accept();
	m_bDragTimer = false;
#endif
}


// Drag move event handler.
void qtractorTrackView::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	if (canClipDropEvent(pDragMoveEvent)) {
		showClipDropRects();
		if (!pDragMoveEvent->isAccepted()) {
			pDragMoveEvent->setDropAction(Qt::CopyAction);
			pDragMoveEvent->accept();
			m_bDragTimer = false;
		}
	} else {
		pDragMoveEvent->ignore();
		hideClipDropRects();
	}

	// Kind of auto-scroll...
	const QPoint& pos = viewportToContents(pDragMoveEvent->pos());
	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);
}


// Drag leave event handler.
void qtractorTrackView::dragLeaveEvent ( QDragLeaveEvent *pDragLeaveEvent )
{
	// Maybe we have something currently going on?
	if (pDragLeaveEvent->isAccepted()) {
		if (!m_bDragTimer) {
			m_bDragTimer = true;
			QTimer::singleShot(100, this, SLOT(dragTimeout()));
		}
	} else {
		// Nothing's being accepted...
		m_bDragTimer = false;
		resetDragState();
	}
}


// Drag timeout slot.
void qtractorTrackView::dragTimeout (void)
{
	if (m_bDragTimer) {
		resetDragState();
		m_bDragTimer = false;
	}
}


// Drop event handler.
bool qtractorTrackView::dropClip (
	const QPoint& pos, const QMimeData *pMimeData )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Add new clips on proper and consecutive track locations...
	unsigned long iClipStart = pSession->frameSnap(
		pSession->frameFromPixel(m_rectDrag.x() + m_iDragClipX));

	// Now check whether the drop is intra-track...
	qtractorTrack *pTrack = dragClipDrop(pos, false, pMimeData);
	// And care if we're not spanning horizontally...
	if (pTrack == NULL
		&& (!m_bDropSpan || m_dropType == qtractorTrack::Midi)) {
		// Do we have something to drop anyway?
		// if yes, this is a extra-track drop...
		int iAddTrack = 0;
		if (!m_dropItems.isEmpty()) {
			// Prepare file list for import...
			QStringList files;
			QListIterator<DropItem *> iter(m_dropItems);
			while (iter.hasNext()) {
				DropItem *pDropItem = iter.next();
				if (m_dropType == qtractorTrack::Midi
					&& pDropItem->channel >= 0) {
					m_pTracks->addMidiTrackChannel(
						pDropItem->path, pDropItem->channel, iClipStart);
					++iAddTrack;
				} else  {
					files.append(pDropItem->path);
				}
			}
			// Depending on import type...
			if (!files.isEmpty()) {
				switch (m_dropType) {
				case qtractorTrack::Audio:
					m_pTracks->addAudioTracks(files, iClipStart);
					++iAddTrack;
					break;
				case qtractorTrack::Midi:
					m_pTracks->addMidiTracks(files, iClipStart);
					++iAddTrack;
					break;
				default:
					break;
				}
			}
		}
		resetDragState();
		return (iAddTrack > 0);
	}

	// Check whether we can really drop it.
	if (pTrack && pTrack->trackType() != m_dropType) {
		resetDragState();
		return false;
	}

	// We'll build a composite command...
	QList<qtractorClip *> clips;

	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("add clip"));

	// If dropping spanned we'll need a track, sure...
	int iTrackClip = 0;
	const bool bAddTrack = (pTrack == NULL);
	if (bAddTrack) {
		pTrack = new qtractorTrack(pSession, m_dropType);
		// Create a new track right away...
		int iTrack = pSession->tracks().count() + 1;
		const QColor& color = qtractorTrack::trackColor(iTrack);
		pTrack = new qtractorTrack(pSession, m_dropType);
		pTrack->setBackground(color);
		pTrack->setForeground(color.darker());
	//	pTrack->setTrackName(tr("Track %1").arg(iTrack));
		pClipCommand->addTrack(pTrack);
	}

	// Now's time to create the clip(s)...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	QListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		DropItem *pDropItem = iter.next();
		switch (pTrack->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
			if (pAudioClip) {
				pAudioClip->setFilename(pDropItem->path);
				pAudioClip->setClipStart(iClipStart);
				pClipCommand->addClip(pAudioClip, pTrack);
				clips.append(pAudioClip);
				// Don't forget to add this one to local repository.
				if (pMainForm)
					pMainForm->addAudioFile(pDropItem->path);
			}
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
			if (pMidiClip) {
				pMidiClip->setFilename(pDropItem->path);
				pMidiClip->setTrackChannel(pDropItem->channel);
				pMidiClip->setClipStart(iClipStart);
				pClipCommand->addClip(pMidiClip, pTrack);
				clips.append(pMidiClip);
				// Don't forget to add this one to local repository.
				if (pMainForm)
					pMainForm->addMidiFile(pDropItem->path);
			}
			break;
		}
		case qtractorTrack::None:
		default:
			break;
		}
		// If track's new it will need a name...
		if (bAddTrack && iTrackClip == 0)
			pTrack->setTrackName(QFileInfo(pDropItem->path).baseName());
		// If multiple items, just snap/concatenate them...
		iClipStart = pSession->frameSnap(iClipStart
			+ pSession->frameFromPixel(pDropItem->rect.width()));
		++iTrackClip;
	}

	// Clean up.
	resetDragState();

	// Put it in the form of an undoable command...
	const bool bResult = pSession->execute(pClipCommand);

	// Redo selection as new...
	if (!clips.isEmpty()) {
		QListIterator<qtractorClip *> clip_iter(clips);
		while (clip_iter.hasNext())
			clip_iter.next()->setClipSelected(true);
		updateClipSelect();
		m_pTracks->selectionChangeNotify();
	}

	return bResult;
}


void qtractorTrackView::dropEvent ( QDropEvent *pDropEvent )
{
	if (!dropClip(viewportToContents(pDropEvent->pos()), pDropEvent->mimeData())) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->dropEvent(pDropEvent);
	}
}


// Handle item selection/dragging -- mouse button press.
void qtractorTrackView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Which mouse state?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	const Qt::KeyboardModifiers& modifiers = pMouseEvent->modifiers();
	bool bModifier = (modifiers & (Qt::ShiftModifier | Qt::ControlModifier));

	// Are we already step-moving or pasting something?
	switch (m_dragState) {
	case DragClipStep:
		// One-click change from drag-step to drag-move...
		m_dragState = DragClipMove;
		m_posDrag   = m_rectDrag.center();
		m_posStep   = QPoint(0, 0);
		dragClipMove(pos + m_posStep);
		// Fall thru...
	case DragClipPaste:
	case DragClipPasteDrop:
	//	qtractorScrollView::mousePressEvent(pMouseEvent);
		return;
	case DragCurveStep:
		// One-click change from drag-step to drag-move...
		m_dragState = DragCurveMove;
		m_posDrag   = m_rectDrag.center();
		m_posStep   = QPoint(0, 0);
		dragCurveMove(pos + m_posStep);
		// Fall thru...
	case DragCurvePaste:
	//	qtractorScrollView::mousePressEvent(pMouseEvent);
		return;
	default:
		break;
	}

	// Automation curve editing modes...
	if (pMouseEvent->button() == Qt::LeftButton) {
		if (m_dragCursor == DragCurveNode
			|| (m_bCurveEdit && m_dragCursor == DragNone)) {
			qtractorTrackViewInfo tvi;
			qtractorTrack *pTrack = trackAt(pos, true, &tvi);
			if (pTrack && m_dragCursor == DragCurveNode) {
				qtractorCurve::Node *pNode = nodeAtTrack(pos, pTrack, &tvi);
				if (pNode) {
					m_pDragCurve = pTrack->currentCurve();
					m_pDragCurveNode = pNode;
				}
			}
		}
		if (m_bCurveEdit && !bModifier)
			clearSelect();
		if (m_dragCursor == DragCurveNode
			|| (m_bCurveEdit && m_dragCursor == DragNone)) {
			m_dragState = DragStart;//DragCurveNode;
			m_posDrag   = pos;
			m_pClipDrag = NULL;
		//	qtractorScrollView::mousePressEvent(pMouseEvent);
			return;
		}
	}

	// Force null state.
	m_pClipDrag = NULL;
	resetDragState();

	// We'll need options somehow...
	qtractorOptions *pOptions = qtractorOptions::getInstance();

	// We need a session and a location...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Direct snap positioning...
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
		// Which button is being pressed?
		switch (pMouseEvent->button()) {
		case Qt::LeftButton:
			// Remember what and where we'll be dragging/selecting...
			m_dragState = DragStart;
			m_posDrag   = pos;
			m_pClipDrag = clipAt(m_posDrag, true, &m_rectDrag);
			// Should it be selected(toggled)?
			if (m_pClipDrag && !dragMoveStart(pos)) {
				// Show that we're about to something...
				m_dragCursor = m_dragState;
				qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
				// Make it (un)selected, right on the file view too...
				if (!m_bCurveEdit && m_selectMode == SelectClip)
					selectClipFile(!bModifier);
			}
			// Something got it started?...
			if (m_pClipDrag == NULL
				|| (m_pClipDrag && !m_pClipDrag->isClipSelected())) {
				// Clear any selection out there?
				if (!m_bCurveEdit && !bModifier)
					clearSelect();
			}
			break;
		case Qt::MidButton:
			// Mid-button positioning...
			clearSelect();
			if (pOptions && pOptions->bMidButtonModifier)
				bModifier = !bModifier;	// Reverse mid-button role...
			if (bModifier) {
				// Play-head positioning...
				pSession->setPlayHead(iFrame);
				setPlayHead(iFrame);
			} else {
				// Edit cursor (merge) positioning...
				setEditHead(iFrame);
				setEditTail(iFrame);
			}
			// Not quite a selection, but some visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case Qt::RightButton:
			// Have sense if pointer falls over a clip...
			m_pClipDrag = clipAt(pos, true);
		#if 0
			if (m_pClipDrag == NULL) {
				// Right-button edit-tail positioning...
				setEditTail(iFrame);
				// Not quite a selection, but some visual feedback...
				m_pTracks->selectionChangeNotify();
			}
		#endif
			// Fall thru...
		default:
			break;
		}
	}

//	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorTrackView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Are we already moving/dragging something?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());
	const Qt::KeyboardModifiers& modifiers = pMouseEvent->modifiers();

	switch (m_dragState) {
	case DragNone:
		// Try to catch mouse over the fade or resize handles...
		dragMoveStart(pos);
		break;
	case DragClipMove:
	case DragClipPaste:
		dragClipMove(pos + m_posStep);
		break;
	case DragClipPasteDrop:
		dragClipDrop(pos + m_posStep);
		showClipDropRects();
		break;
	case DragClipFadeIn:
	case DragClipFadeOut:
		dragClipFadeMove(pos);
		break;
	case DragClipResizeLeft:
	case DragClipResizeRight:
		dragClipResizeMove(pos);
		break;
	case DragCurveMove:
	case DragCurvePaste:
		dragCurveMove(pos + m_posStep);
		break;
	case DragCurveNode:
		dragCurveNode(pos, modifiers & Qt::ControlModifier);
		break;
	case DragSelect:
		m_rectDrag.setBottomRight(pos);
		moveRubberBand(&m_pRubberBand, m_rectDrag);
		qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);
		if (m_bCurveEdit) {
			// Select all current track curve/automation
			// nodes that fall inside the rubber-band...
			selectCurveRect(m_rectDrag,
				SelectRect,
				selectFlags(modifiers));
		} else {
			// Here we're mainly supposed to select a few bunch
			// of clips that fall inside the rubber-band...
			selectClipRect(m_rectDrag,
				m_selectMode,
				selectFlags(modifiers));
		}
		showToolTip(m_rectDrag.normalized(), m_iDragClipX);
		break;
	case DragStart:
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// Check if we're pointing in some fade-in/out,
			// resize handle or a automation curve node...
			if (dragMoveStart(m_posDrag)) {
				m_dragState = m_dragCursor;
				if (m_dragState == DragCurveMove) {
					// DragCurveMove...
					m_iDragCurveX = (pos.x() - m_posDrag.x());
					qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
				}
				else
				if (m_dragState == DragClipFadeIn  ||
					m_dragState == DragClipFadeOut) {
					// DragClipFade...
					m_iDragClipX = (pos.x() - m_posDrag.x());
					qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
					moveRubberBand(&m_pRubberBand, m_rectHandle);					
				} else if (m_pClipDrag) {
					// DragClipResize...
					moveRubberBand(&m_pRubberBand, m_rectDrag, 3);					
				}
			}
			else
			if (!m_bCurveEdit
				|| (m_dragCursor != DragCurveNode
				 && m_dragCursor != DragCurveMove)) {
				// We'll start dragging clip/regions alright...
				qtractorSession *pSession = qtractorSession::getInstance();
				qtractorClipSelect::Item *pClipItem = NULL;
				if (m_pClipDrag && pSession)
					pClipItem = m_pClipSelect->findItem(m_pClipDrag);
				if (pClipItem && pClipItem->rectClip.contains(pos)) {
					int x = pSession->pixelSnap(m_rectDrag.x());
					m_iDragClipX = (x - m_rectDrag.x());
					m_dragState = m_dragCursor = DragClipMove;
					qtractorScrollView::setCursor(QCursor(Qt::SizeAllCursor));
					showClipSelect();
				} else {
					// We'll start rubber banding...
					m_rectDrag.setTopLeft(m_posDrag);
					m_rectDrag.setBottomRight(pos);
					m_dragState = m_dragCursor = DragSelect;
					// Set a proper cursor...
					qtractorScrollView::setCursor(QCursor(
						m_selectMode == SelectRange ?
							Qt::SizeHorCursor : Qt::CrossCursor));
					// Create the rubber-band if there's none...
					moveRubberBand(&m_pRubberBand, m_rectDrag);
				}
			}
		}
		// Fall thru...
	case DragClipStep:
	case DragClipDrop:
	default:
		break;
	}

//	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorTrackView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
//	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Direct snap positioning...
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		const unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(m_posDrag.x() > 0 ? m_posDrag.x() : 0));
		// Which mouse state?
		const Qt::KeyboardModifiers& modifiers
			= pMouseEvent->modifiers();
		const bool bModifier
			= (modifiers & (Qt::ShiftModifier | Qt::ControlModifier));
		switch (m_dragState) {
		case DragSelect:
			if (m_bCurveEdit) {
				// Select all current track curve/automation
				// nodes that fall inside the rubber-band...
				selectCurveRect(m_rectDrag,
					SelectRect,
					selectFlags(modifiers) | SelectCommit);
			} else {
				// Here we're mainly supposed to select a few bunch
				// of clips that fall inside the rubber-band...
				selectClipRect(m_rectDrag,
					m_selectMode,
					selectFlags(modifiers) | SelectCommit);
			}
			// For immediate visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragClipMove:
			// Let's move them...
			moveClipSelect(dragClipMove(pos + m_posStep));
			break;
		case DragClipPaste:
			// Let's paste them...
			pasteClipSelect(dragClipMove(pos + m_posStep));
			break;
		case DragClipPasteDrop:
			// Let's drop-paste them...
			dropClip(pos + m_posStep);
			break;
		case DragClipFadeIn:
		case DragClipFadeOut:
			dragClipFadeDrop(pos);
			break;
		case DragClipResizeLeft:
		case DragClipResizeRight:
			dragClipResizeDrop(pos, bModifier);
			break;
		case DragCurveMove:
			// Let's move them...
			moveCurveSelect(pos + m_posStep);
			break;
		case DragCurvePaste:
			pasteCurveSelect(pos + m_posStep);
			break;
		case DragStart:
			// Deferred left-button positioning...
			if (m_pClipDrag) {
				// Make it right on the file view now...
				if (!m_bCurveEdit && m_selectMode != SelectClip)
					selectClipFile(!bModifier);
				// Nothing more has been deferred...
			} else {
				// As long we're editing curve/automation...
				if (m_bCurveEdit)
					dragCurveNode(pos, modifiers & Qt::ControlModifier);
				// As long we're not editing anything...
				if (m_dragCursor == DragNone && bModifier) {
					// Direct play-head positioning:
					// first, set actual engine position...
					pSession->setPlayHead(iFrame);
					// Play-head positioning...
					setPlayHead(iFrame);
					// Done with (deferred) play-head positioning.
				}
			}
			// Fall thru...
		case DragCurveNode:
			// Specially when editing curve nodes,
			// for immediate visual feedback...
			if (m_pCurveEditCommand && !m_pCurveEditCommand->isEmpty()) {
				pSession->commands()->push(m_pCurveEditCommand);
				m_pCurveEditCommand = NULL;
				m_pTracks->dirtyChangeNotify();
			} else {
				m_pTracks->selectionChangeNotify();
			}
			// Fall thru...
		case DragCurveStep:
		case DragClipStep:
		case DragClipDrop:
		case DragNone:
		default:
			break;
		}
	}

	// Force null state.
	resetDragState();
}


// Handle item/clip editing from mouse.
void qtractorTrackView::mouseDoubleClickEvent ( QMouseEvent *pMouseEvent )
{
	qtractorScrollView::mouseDoubleClickEvent(pMouseEvent);

	// By this time we should have something under...
	qtractorClip *pClip = m_pClipDrag;
	if (pClip == NULL) {
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		pClip = clipAt(pos, true);
	}

	if (pClip)
		m_pTracks->editClip(pClip);
	else
		m_pTracks->selectCurrentTrack();
}


// Handle zoom with mouse wheel.
void qtractorTrackView::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers() & Qt::ControlModifier) {
		const int delta = pWheelEvent->delta();
		if (delta > 0)
			m_pTracks->zoomIn();
		else
			m_pTracks->zoomOut();
	}
	else qtractorScrollView::wheelEvent(pWheelEvent);
}


// Focus lost event.
void qtractorTrackView::focusOutEvent ( QFocusEvent *pFocusEvent )
{
	if (m_dragState == DragClipStep      ||
		m_dragState == DragClipPaste     ||
		m_dragState == DragClipPasteDrop ||
		m_dragState == DragCurveStep     ||
		m_dragState == DragCurvePaste)
		resetDragState();

	qtractorScrollView::focusOutEvent(pFocusEvent);
}


// Trap for help/tool-tip events.
bool qtractorTrackView::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	if (static_cast<QWidget *> (pObject) == pViewport) {
		if (pEvent->type() == QEvent::ToolTip && m_bToolTips) {
			QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
			if (pHelpEvent) {
				const QPoint& pos
					= qtractorScrollView::viewportToContents(pHelpEvent->pos());
				qtractorTrackViewInfo tvi;
				qtractorTrack *pTrack = trackAt(pos, false, &tvi);
				if (pTrack) {
					qtractorCurve::Node *pNode = nodeAtTrack(pos, pTrack, &tvi);
					if (pNode) {
						qtractorCurve *pCurve = pTrack->currentCurve();
						QToolTip::showText(pHelpEvent->globalPos(),
							nodeToolTip(pCurve, pNode), pViewport);
						return true;
					}
					qtractorClip *pClip = clipAtTrack(pos, NULL, pTrack, &tvi);
					if (pClip) {
						QToolTip::showText(pHelpEvent->globalPos(),
							pClip->toolTip(), pViewport);
						return true;
					}
				}
			}
		}
		else
		if (pEvent->type() == QEvent::Leave  &&
			m_dragState != DragCurvePaste    &&
			m_dragState != DragCurveStep     &&
			m_dragState != DragClipPasteDrop &&
			m_dragState != DragClipPaste     &&
			m_dragState != DragClipStep) {
			m_dragCursor = DragNone;
			qtractorScrollView::unsetCursor();
			return true;
		}
	}

	// Not handled here.
	return qtractorScrollView::eventFilter(pObject, pEvent);
}


// Clip file(item) selection convenience method.
void qtractorTrackView::selectClipFile ( bool bReset )
{
	if (m_pClipDrag == NULL)
		return;

	// Reset selection (conditional)...
	int iUpdate = 0;
	QRect rectUpdate = m_pClipSelect->rect();

	// Do the selection dance, first...
	qtractorClipSelect::Item *pClipItem = m_pClipSelect->findItem(m_pClipDrag);
	bool bSelect = !(pClipItem && pClipItem->rectClip.contains(m_posDrag));
	if (!bReset) {
		m_pClipSelect->selectItem(m_pClipDrag, m_rectDrag, bSelect);
		++iUpdate;
	} else if (bSelect || m_selectMode != SelectClip) {
		m_pClipSelect->clear();
		if (bSelect)
			m_pClipSelect->selectItem(m_pClipDrag, m_rectDrag, true);
		++iUpdate;
	}

	if (iUpdate > 0) {
		updateRect(rectUpdate.united(m_pClipSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Do the file view selection then...
	qtractorTrack *pTrack = m_pClipDrag->track();
	if (pTrack == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorFiles *pFiles = pMainForm->files();
	if (pFiles == NULL)
		return;

	switch (pTrack->trackType()) {
	case qtractorTrack::Audio: {
		qtractorAudioClip *pAudioClip
			= static_cast<qtractorAudioClip *> (m_pClipDrag);
		if (pAudioClip)
			pFiles->selectAudioFile(pAudioClip->filename());
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (m_pClipDrag);
		if (pMidiClip)
			pFiles->selectMidiFile(
				pMidiClip->filename(), pMidiClip->trackChannel());
		break;
	}
	default:
		break;
	}
}


// Convert selection flags from keyboard modifiers.
int qtractorTrackView::selectFlags (
	const Qt::KeyboardModifiers modifiers )
{
	int flags = SelectNone;

	if ((modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0)
		flags |= qtractorTrackView::SelectClear;
	if (modifiers & Qt::ControlModifier)
		flags |= qtractorTrackView::SelectToggle;

	return flags;
}


// Clip selection mode accessors.
void qtractorTrackView::setSelectMode (
	qtractorTrackView::SelectMode selectMode )
{
	m_selectMode = selectMode;
}

qtractorTrackView::SelectMode qtractorTrackView::selectMode (void) const
{
	return m_selectMode;
}


// Select everything (clips) under a given (rubber-band) rectangle.
void qtractorTrackView::selectClipRect ( const QRect& rectDrag,
	SelectMode selectMode, int flags, SelectEdit selectEdit )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	QRect rect(rectDrag.normalized());

	// The precise (snapped) selection frame points...
	const unsigned long iSelectStart
		= pSession->frameSnap(pSession->frameFromPixel(rect.left()));
	const unsigned long iSelectEnd
		= pSession->frameSnap(pSession->frameFromPixel(rect.right()));

	// Special whole-vertical range case...
	QRect rectRange(0, 0, 0, qtractorScrollView::contentsHeight());
	rectRange.setLeft(pSession->pixelFromFrame(iSelectStart));
	rectRange.setRight(pSession->pixelFromFrame(iSelectEnd));
	if (selectMode == SelectRange) {
		rect.setTop(rectRange.top());
		rect.setBottom(rectRange.height());
	}

	// Reset all selected clips...
	int iUpdate = 0;
	QRect rectUpdate = m_pClipSelect->rect();
	if ((flags & SelectClear) && (m_pClipSelect->items().count() > 0)) {
		m_pClipSelect->clear();
		++iUpdate;
	}

	// Now find all the clips/regions that fall
	// in the given rectangular region...
	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (rect.bottom() < y1)
			break;
		if (y2 >= rect.top()) {
			const int y = y1 + 1;
			const int h = y2 - y1 - 2;
			for (qtractorClip *pClip = pTrack->clips().first();
					pClip; pClip = pClip->next()) {
				const int x = pSession->pixelFromFrame(pClip->clipStart());
				if (x > rect.right())
					break;
				const int w = pSession->pixelFromFrame(pClip->clipLength());
				// Test whether the whole clip rectangle
				// intersects the rubber-band range one...
				QRect rectClip(x, y, w, h);
				if (rect.intersects(rectClip)) {
					if (selectMode != SelectClip)
						rectClip = rectRange.intersected(rectClip);
					m_pClipSelect->selectItem(pClip, rectClip, true);
					if (selectMode != SelectClip)
						pClip->setClipSelect(iSelectStart, iSelectEnd);
					++iUpdate;
				}
			}
		}
		pTrack = pTrack->next();
	}

	// Update the screen real estate...
	if (iUpdate > 0) {
		updateRect(rectUpdate.united(m_pClipSelect->rect()));
	//	m_pTracks->selectionChangeNotify();
	}

	// That's all very nice but we'll also set edit-range positioning...
	if (selectEdit & EditHead)
		setEditHead(iSelectStart);
	if (selectEdit & EditTail)
		setEditTail(iSelectEnd);
}


// Select every clip of a given track-range.
void qtractorTrackView::selectClipTrackRange (
	qtractorTrack *pTrackPtr, bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const unsigned long iSelectStart = pSession->editHead();
	const unsigned long iSelectEnd   = pSession->editTail();

	// Get and select the track's rectangular area
	// between the edit head and tail points...
	QRect rect(0, 0, 0, qtractorScrollView::contentsHeight());
	rect.setLeft(pSession->pixelFromFrame(iSelectStart));
	rect.setRight(pSession->pixelFromFrame(iSelectEnd));

	// Reset selection (unconditional)...
	int iUpdate = 0;
	QRect rectUpdate = m_pClipSelect->rect();
	if (bReset && m_pClipSelect->items().count() > 0) {
		m_pClipSelect->clear();
		++iUpdate;
	}

	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pTrack == pTrackPtr || pTrackPtr == NULL) {
			const int y = y1 + 1;
			const int h = y2 - y1 - 2;
			rect.setY(y);
			rect.setHeight(h);
			for (qtractorClip *pClip = pTrack->clips().first();
					pClip; pClip = pClip->next()) {
				const int x = pSession->pixelFromFrame(pClip->clipStart());
				if (x > rect.right())
					break;
				const int w = pSession->pixelFromFrame(pClip->clipLength());
				// Test whether the track clip rectangle
				// intersects the rubber-band range one...
				QRect rectClip(x, y, w, h);
				if (rect.intersects(rectClip)) {
					rectClip = rect.intersected(rectClip);
					const bool bSelect = !pClip->isClipSelected();
					m_pClipSelect->selectItem(pClip, rectClip, bSelect);
					if (bSelect)
						pClip->setClipSelect(iSelectStart, iSelectEnd);
					++iUpdate;
				}
			}
			if (pTrack == pTrackPtr)
				break;
		}
		pTrack = pTrack->next();
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		updateRect(rectUpdate.united(m_pClipSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Select every clip of a given track.
void qtractorTrackView::selectClipTrack (
	qtractorTrack *pTrackPtr, bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Reset selection (conditional)...
	int iUpdate = 0;
	QRect rectUpdate = m_pClipSelect->rect();
	if (bReset && m_pClipSelect->items().count() > 0) {
	//	&& m_pClipSelect->singleTrack() != pTrackPtr) {
		m_pClipSelect->clear();
		++iUpdate;
	}

	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pTrack == pTrackPtr) {
			const int y = y1 + 1;
			const int h = y2 - y1 - 2;
			for (qtractorClip *pClip = pTrack->clips().first();
					pClip; pClip = pClip->next()) {
				const int x = pSession->pixelFromFrame(pClip->clipStart());
				const int w = pSession->pixelFromFrame(pClip->clipLength());
				const bool bSelect = !pClip->isClipSelected();
				m_pClipSelect->selectItem(pClip, QRect(x, y, w, h), bSelect);
				++iUpdate;
			}
			break;
		}
		pTrack = pTrack->next();
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		updateRect(rectUpdate.united(m_pClipSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Select all clip contents.
void qtractorTrackView::selectClipAll (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Reset selection...
	int iUpdate = 0;
	QRect rectUpdate = m_pClipSelect->rect();
	if (m_pClipSelect->items().count() > 0) {
		m_pClipSelect->clear();
		++iUpdate;
	}

	// Select all clips on all tracks...
	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		const int y = y1 + 1;
		const int h = y2 - y1 - 2;
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			const int x = pSession->pixelFromFrame(pClip->clipStart());
			const int w = pSession->pixelFromFrame(pClip->clipLength());
			m_pClipSelect->selectItem(pClip, QRect(x, y, w, h));
			++iUpdate;
		}
		pTrack = pTrack->next();
	} 

	// This is most probably an overall update...
	if (iUpdate > 0) {
		updateRect(rectUpdate.united(m_pClipSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Invert selection on all tracks and clips.
void qtractorTrackView::selectClipInvert (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	int iUpdate = 0;
	QRect rectUpdate = m_pClipSelect->rect();

	// Invert selection...
	int y1, y2 = 0;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		const int y = y1 + 1;
		const int h = y2 - y1 - 2;
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			const int x = pSession->pixelFromFrame(pClip->clipStart());
			const int w = pSession->pixelFromFrame(pClip->clipLength());
			const bool bSelect = !pClip->isClipSelected();
			m_pClipSelect->selectItem(pClip, QRect(x, y, w, h), bSelect);
			++iUpdate;
		}
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		updateRect(rectUpdate.united(m_pClipSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Select all clips of given filename and track/channel.
void qtractorTrackView::selectClipFile ( qtractorTrack::TrackType trackType,
	const QString& sFilename, int iTrackChannel, bool bSelect )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Reset selection (conditional)...
	int iUpdate = 0;
	QRect rectUpdate = m_pClipSelect->rect();
	if (m_pClipSelect->items().count() > 0
		&& (QApplication::keyboardModifiers()
			& (Qt::ShiftModifier | Qt::ControlModifier)) == 0) {
		m_pClipSelect->clear();
		++iUpdate;
	}

	int x0 = qtractorScrollView::contentsWidth();
	int y0 = qtractorScrollView::contentsHeight();

	int y1, y2 = 0;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pTrack->trackType() != trackType)
			continue;
		const int y = y1 + 1;
		const int h = y2 - y1 - 2;
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			if (pClip->filename() != sFilename)
				continue;
			if (iTrackChannel >= 0 && trackType == qtractorTrack::Midi) {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (pClip);
				if (pMidiClip
					&& int(pMidiClip->trackChannel()) != iTrackChannel)
					continue;
			}
			const int x = pSession->pixelFromFrame(pClip->clipStart());
			const int w = pSession->pixelFromFrame(pClip->clipLength());
			m_pClipSelect->selectItem(pClip, QRect(x, y, w, h), bSelect);
			if (x0 > x) x0 = x;
			if (y0 > y) y0 = y;
			++iUpdate;
		}
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		updateRect(rectUpdate.united(m_pClipSelect->rect()));
		m_pTracks->selectionChangeNotify();
		// Make sure the earliest is barely visible...
		if (isClipSelected())
			qtractorScrollView::ensureVisible(x0, y0, 24, 24);
	}

	// Make sure we keep focus... (maybe not:)
//	qtractorScrollView::setFocus();
}


// Select curve nodes under a given (rubber-band) rectangle.
void qtractorTrackView::selectCurveRect ( const QRect& rectDrag,
	SelectMode selectMode, int flags, SelectEdit selectEdit )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	QRect rect(rectDrag.normalized());

	// The precise (snapped) selection frame points...
	const unsigned long iSelectStart
		= pSession->frameSnap(pSession->frameFromPixel(rect.left()));
	const unsigned long iSelectEnd
		= pSession->frameSnap(pSession->frameFromPixel(rect.right()));

	// Special whole-vertical range case...
	rect.setLeft(pSession->pixelFromFrame(iSelectStart));
	rect.setRight(pSession->pixelFromFrame(iSelectEnd));
	if (selectMode == SelectRange) {
		rect.setTop(0);
		rect.setBottom(qtractorScrollView::contentsHeight());
	}

	// Reset all selected nodes...
	int iUpdate = 0;
	QRect rectUpdate = m_pCurveSelect->rect();
	if ((flags & SelectClear) && (m_pCurveSelect->items().count() > 0)) {
		m_pCurveSelect->clear();
		++iUpdate;
	}

	// Now find all the curve/nodes that fall
	// in the given rectangular region...
	const bool bToggle = (flags & SelectToggle);

	int x1 = qtractorScrollView::contentsX();
	int x2 = x1 + (qtractorScrollView::viewport())->width();
	if (x1 > rect.left())
		x1 = rect.left();
	if (x2 < rect.right())
		x2 = rect.right();

	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (y1 > rect.bottom())
			break;
		if (y2 >= rect.top()) {
			qtractorCurve *pCurve = pTrack->currentCurve();
			if (pCurve && !pCurve->isLocked()) {
				const int h = y2 - y1 - 2;
				const unsigned long iFrameStart
					= pSession->frameFromPixel(x1);
				const unsigned long iFrameEnd
					= pSession->frameFromPixel(x2);
				qtractorCurve::Cursor cursor(pCurve);
				qtractorCurve::Node *pNode = cursor.seek(iFrameStart);
				while (pNode && pNode->frame < iFrameEnd) {
					const int x = pSession->pixelFromFrame(pNode->frame);
					const int y = y2 - int(cursor.scale(pNode) * float(h));
					const bool bSelect = rect.contains(x, y);
					m_pCurveSelect->selectItem(pCurve, pNode,
						QRect(x - 4, y - 4, 8, 8), bSelect, bToggle);
					++iUpdate;
					pNode = pNode->next();
				}
				if (m_pCurveSelect->isCurrentCurve(pCurve))
					break;
			}
		}
		pTrack = pTrack->next();
	}

	// Update the screen real estate...
	if (iUpdate > 0) {
		m_pCurveSelect->update(flags & SelectCommit);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
	//	m_pTracks->selectionChangeNotify();
	}

	// That's all very nice but we'll also set edit-range positioning...
	if (selectEdit & EditHead)
		setEditHead(iSelectStart);
	if (selectEdit & EditTail)
		setEditTail(iSelectEnd);
}


// Select every current curve/automation node of a given track-range.
void qtractorTrackView::selectCurveTrackRange (
	qtractorTrack *pTrackPtr, bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const unsigned long iSelectStart = pSession->editHead();
	const unsigned long iSelectEnd   = pSession->editTail();

	// Get and select the track's rectangular area
	// between the edit head and tail points...
	QRect rect(0, 0, 0, qtractorScrollView::contentsHeight());
	rect.setLeft(pSession->pixelFromFrame(iSelectStart));
	rect.setRight(pSession->pixelFromFrame(iSelectEnd));

	// Reset selection (unconditional)...
	if (pTrackPtr && !bReset)
		bReset = !m_pCurveSelect->isCurrentCurve(pTrackPtr->currentCurve());

	int iUpdate = 0;
	QRect rectUpdate = m_pCurveSelect->rect();
	if (bReset && m_pCurveSelect->items().count() > 0) {
		m_pCurveSelect->clear();
		++iUpdate;
	}

	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pTrack == pTrackPtr || pTrackPtr == NULL) {
			qtractorCurve *pCurve = pTrack->currentCurve();
			if (pCurve && !pCurve->isLocked()) {
				const int h = y2 - y1 - 2;
				rect.setY(y1 + 1);
				rect.setHeight(h);
				qtractorCurve::Node *pNode = pCurve->nodes().first();
				while (pNode) {
					const float s = pCurve->scaleFromValue(pNode->value);
					const int x = pSession->pixelFromFrame(pNode->frame);
					const int y	= y2 - int(s * float(h));
					const bool bSelect = rect.contains(x, y);
					m_pCurveSelect->selectItem(pCurve, pNode,
						QRect(x - 4, y - 4, 8, 8), bSelect, !bReset);
					++iUpdate;
					pNode = pNode->next();
				}
			}
			if (pTrackPtr || m_pCurveSelect->isCurrentCurve(pCurve))
				break;
		}
		pTrack = pTrack->next();
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Select every current curve/automation node of a given track.
void qtractorTrackView::selectCurveTrack (
	qtractorTrack *pTrackPtr, bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Reset selection (conditional)...
	if (pTrackPtr && !bReset)
		bReset = !m_pCurveSelect->isCurrentCurve(pTrackPtr->currentCurve());

	int iUpdate = 0;
	QRect rectUpdate = m_pCurveSelect->rect();
	if (bReset && m_pCurveSelect->items().count() > 0) {
		m_pCurveSelect->clear();
		++iUpdate;
	}

	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pTrack == pTrackPtr || pTrackPtr == NULL) {
			qtractorCurve *pCurve = pTrack->currentCurve();
			if (pCurve && !pCurve->isLocked()) {
				const int h = y2 - y1 - 2;
				qtractorCurve::Node *pNode = pCurve->nodes().first();
				while (pNode) {
					const float s = pCurve->scaleFromValue(pNode->value);
					const int x = pSession->pixelFromFrame(pNode->frame);
					const int y	= y2 - int(s * float(h));
					m_pCurveSelect->selectItem(pCurve, pNode,
						QRect(x - 4, y - 4, 8, 8), true, !bReset);
					++iUpdate;
					pNode = pNode->next();
				}
			}
			if (pTrackPtr || m_pCurveSelect->isCurrentCurve(pCurve))
				break;
		}
		pTrack = pTrack->next();
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Select all current track curve/automation nodes.
void qtractorTrackView::selectCurveAll (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTrack *pCurrentTrack = m_pTracks->currentTrack();
	if (pCurrentTrack == NULL)
		return;

	int iUpdate = 0;
	QRect rectUpdate = m_pCurveSelect->rect();
	if (m_pCurveSelect->items().count() > 0) {
		m_pCurveSelect->clear();
		++iUpdate;
	}

	// Select all current track/curve automation nodes...
	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pCurrentTrack == pTrack) {
			qtractorCurve *pCurve = pTrack->currentCurve();
			if (pCurve && !pCurve->isLocked()) {
				const int h = y2 - y1 - 2;
				qtractorCurve::Node *pNode = pCurve->nodes().first();
				while (pNode) {
					const float s = pCurve->scaleFromValue(pNode->value);
					const int x = pSession->pixelFromFrame(pNode->frame);
					const int y	= y2 - int(s * float(h));
					m_pCurveSelect->selectItem(pCurve, pNode,
						QRect(x - 4, y - 4, 8, 8));
					++iUpdate;
					pNode = pNode->next();
				}
			}
			break;
		}
		pTrack = pTrack->next();
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Invert selection on current track curve/automation nodes.
void qtractorTrackView::selectCurveInvert (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	int iUpdate = 0;
	QRect rectUpdate = m_pCurveSelect->rect();

	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		qtractorCurve *pCurve = pTrack->currentCurve();
		if (pCurve && !pCurve->isLocked()) {
			const int h = y2 - y1 - 2;
			qtractorCurve::Node *pNode = pCurve->nodes().first();
			while (pNode) {
				const float s = pCurve->scaleFromValue(pNode->value);
				const int x = pSession->pixelFromFrame(pNode->frame);
				const int y	= y2 - int(s * float(h));
				m_pCurveSelect->selectItem(pCurve, pNode,
					QRect(x - 4, y - 4, 8, 8), true, true);
				++iUpdate;
				pNode = pNode->next();
			}
			if (m_pCurveSelect->isCurrentCurve(pCurve))
				break;
		}
		pTrack = pTrack->next();
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}

	// Make sure we keep focus...
	qtractorScrollView::setFocus();
}


// Contents update overloaded methods.
void qtractorTrackView::updateRect ( const QRect& rect )
{
	qtractorScrollView::update(	// Add some slack margin...
		QRect(qtractorScrollView::contentsToViewport(
			rect.topLeft()), rect.size() + QSize(4, 4)));
}


// Whether there's any clip currently editable.
qtractorClip *qtractorTrackView::currentClip (void) const
{
	qtractorClip *pClip = m_pClipDrag;

	if (pClip == NULL && isClipSelected())
		pClip = m_pClipSelect->items().constBegin().key();

	return pClip;
}


// Clip selection accessor.
qtractorClipSelect *qtractorTrackView::clipSelect (void) const
{
	return m_pClipSelect;
}


// Update whole clip selection.
void qtractorTrackView::updateClipSelect (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Reset all selected clips, but don't
	// clear their own selection state...
	m_pClipSelect->reset();

	// Now find all the clips/regions
	// that were currently selected...
	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		const int y = y1 + 1;
		const int h = y2 - y1 - 2;
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			if (pClip->isClipSelected()) {
				const int x = pSession->pixelFromFrame(pClip->clipSelectStart());
				const int w = pSession->pixelFromFrame(pClip->clipSelectEnd()) - x;
				m_pClipSelect->addItem(pClip, QRect(x, y, w, h));
			}
		}
		pTrack = pTrack->next();
	}
}


// Draw/hide the whole current clip selection.
void qtractorTrackView::showClipSelect (void) const
{
	const qtractorClipSelect::ItemList& items = m_pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorClipSelect::Item *pClipItem = iter.value();
		QRect rectClip = pClipItem->rectClip;
		if (m_bDragSingleTrack) {
			rectClip.setY(m_iDragSingleTrackY);
			rectClip.setHeight(m_iDragSingleTrackHeight);
		}
		moveRubberBand(&(pClipItem->rubberBand), rectClip, 3);
	}

	showToolTip(m_pClipSelect->rect(), m_iDragClipX);
}

void qtractorTrackView::hideClipSelect (void) const
{
	const qtractorClipSelect::ItemList& items = m_pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorRubberBand *pRubberBand = iter.value()->rubberBand;
		if (pRubberBand && pRubberBand->isVisible())
			pRubberBand->hide();
	}
}


// Update whole automation/curve selection.
void qtractorTrackView::updateCurveSelect (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	int iUpdate = 0;
	QRect rectUpdate = m_pCurveSelect->rect();

	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		qtractorCurve *pCurve = pTrack->currentCurve();
		if (pCurve && m_pCurveSelect->isCurrentCurve(pCurve)) {
			const int h = y2 - y1 - 2;
			qtractorCurve::Node *pNode = pCurve->nodes().first();
			while (pNode) {
				qtractorCurveSelect::Item *pItem
					= m_pCurveSelect->findItem(pNode);
				if (pItem) {
					const float s = pCurve->scaleFromValue(pNode->value);
					const int x = pSession->pixelFromFrame(pNode->frame);
					const int y	= y2 - int(s * float(h));
					pItem->rectNode.setRect(x - 4, y - 4, 8, 8);
					++iUpdate;
				}
				pNode = pNode->next();
			}
			break;
		}
		pTrack = pTrack->next();
	}

	// This is most probably an overall update...
	if (iUpdate > 0) {
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
	//	m_pTracks->selectionChangeNotify();
	}
}


// Show selection tooltip...
void qtractorTrackView::showToolTip ( const QRect& rect, int dx ) const
{
	if (!m_bToolTips)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return;

	const unsigned long iFrameStart = pTimeScale->frameSnap(
		pTimeScale->frameFromPixel(rect.left() + dx));
	const unsigned long iFrameEnd = pTimeScale->frameSnap(
		iFrameStart + pTimeScale->frameFromPixel(rect.width()));

	QToolTip::showText(
		QCursor::pos(),
		tr("Start:\t%1\nEnd:\t%2\nLength:\t%3")
			.arg(pTimeScale->textFromFrame(iFrameStart))
			.arg(pTimeScale->textFromFrame(iFrameEnd))
			.arg(pTimeScale->textFromFrame(iFrameStart, true, iFrameEnd - iFrameStart)),
		qtractorScrollView::viewport());
}


// Draw/hide the whole drop rectagle list
void qtractorTrackView::updateClipDropRects ( int y, int h ) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	int x = 0;
	const bool bDropSpan
		= (m_bDropSpan && m_dropType != qtractorTrack::Midi);
	QListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		DropItem *pDropItem = iter.next();
		pDropItem->rect.setY(y);
		pDropItem->rect.setHeight(h);
		if (bDropSpan) {
			if (x > 0)
				pDropItem->rect.moveLeft(x);
			else
				x = pDropItem->rect.x();
			x = pSession->pixelSnap(x + pDropItem->rect.width());
		} else {
			y += h + 4;
		}
	}
}

void qtractorTrackView::showClipDropRects (void) const
{
	QRect rect;

	QListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		DropItem *pDropItem = iter.next();
		moveRubberBand(&(pDropItem->rubberBand), pDropItem->rect, 3);
		rect = rect.united(pDropItem->rect);
	}
	
	showToolTip(rect, m_iDragClipX);
}

void qtractorTrackView::hideClipDropRects (void) const
{
	QListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		qtractorRubberBand *pRubberBand = iter.next()->rubberBand;
		if (pRubberBand && pRubberBand->isVisible())
			pRubberBand->hide();
	}
}



// Show and move rubber-band item.
void qtractorTrackView::moveRubberBand ( qtractorRubberBand **ppRubberBand,
	const QRect& rectDrag, int thick ) const
{
	QRect rect(rectDrag.normalized());

	// Horizontal adjust...
	rect.translate(m_iDragClipX, 0);
	// Convert rectangle into view coordinates...
	rect.moveTopLeft(qtractorScrollView::contentsToViewport(rect.topLeft()));
	// Make sure the rectangle doesn't get too off view,
	// which it would make it sluggish :)
	if (rect.left() < 0)
		rect.setLeft(-8);
	if (rect.right() > qtractorScrollView::width())
		rect.setRight(qtractorScrollView::width() + 8);

	// Create the rubber-band if there's none...
	qtractorRubberBand *pRubberBand = *ppRubberBand;
	if (pRubberBand == NULL) {
		pRubberBand = new qtractorRubberBand(
			QRubberBand::Rectangle, qtractorScrollView::viewport(), thick);
	//	QPalette pal(pRubberBand->palette());
	//	pal.setColor(pRubberBand->foregroundRole(), Qt::blue);
	//	pRubberBand->setPalette(pal);
	//	pRubberBand->setBackgroundRole(QPalette::NoRole);
		// Do not ever forget to set it back...
		*ppRubberBand = pRubberBand;
	}
	
	// Just move it
	pRubberBand->setGeometry(rect);

	// Ah, and make it visible, of course...
	if (!pRubberBand->isVisible())
		pRubberBand->show();
}


// Check whether we're up to drag a clip fade-in/out or resize handle.
bool qtractorTrackView::dragMoveStart ( const QPoint& pos )
{
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, false, &tvi);
	if (pTrack == NULL)
		return false;

	qtractorCurve::Node *pNode = m_pDragCurveNode;
	if (pNode == NULL)
		pNode = nodeAtTrack(pos, pTrack, &tvi);
	if (pNode) {
		if (m_bCurveEdit && m_dragState == DragStart
			&& (m_pCurveSelect->items().count() > 1)
			&& (m_pCurveSelect->findItem(pNode)))
			m_dragCursor = DragCurveMove;
		else
			m_dragCursor = DragCurveNode;
		qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
		return true;
	}

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	QRect rectClip;
	qtractorClip *pClip = clipAtTrack(pos, &rectClip, pTrack, &tvi);
	if (pClip) {
		// Fade-in handle check...
		m_rectHandle.setRect(rectClip.left() + 1
			+ pSession->pixelFromFrame(pClip->fadeInLength()),
				rectClip.top() + 1, 8, 8);
		if (m_rectHandle.contains(pos)) {
			m_dragCursor = DragClipFadeIn;
			qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
			return true;
		}
		// Fade-out handle check...
		m_rectHandle.setRect(rectClip.right() - 8
			- pSession->pixelFromFrame(pClip->fadeOutLength()),
				rectClip.top() + 1, 8, 8);
		if (m_rectHandle.contains(pos)) {
			m_dragCursor = DragClipFadeOut;
			qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
			return true;
		}
		// Resize-right check...
		if (pos.x() >= rectClip.right() - 4) {
			m_dragCursor = DragClipResizeRight;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			return true;
		}
		// Resize-left check...
		if (pos.x() < rectClip.left() + 4) {
			m_dragCursor = DragClipResizeLeft;
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
			return true;
		}
	}

	// Reset cursor if any persist around.
	if (m_dragCursor != DragNone) {
		m_dragCursor  = DragNone;
		qtractorScrollView::unsetCursor();
	}

	return false;
}


// Clip fade-in/out handle drag-moving parts.
void qtractorTrackView::dragClipFadeMove ( const QPoint& pos )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Always change horizontally wise...
	const int x0 = pSession->pixelSnap(pos.x());
	int dx = (x0 - m_posDrag.x());
	if (m_rectHandle.left() + dx < m_rectDrag.left())
		dx = m_rectDrag.left() - m_rectHandle.left();
	else if (m_rectHandle.right() + dx > m_rectDrag.right())
		dx = m_rectDrag.right() - m_rectHandle.right();
	m_iDragClipX = dx;
	moveRubberBand(&m_pRubberBand, m_rectHandle);
	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);
	
	// Prepare to update the whole view area...
	updateRect(m_rectDrag);

	// Show fade-in/out tooltip..
	QRect rect(m_rectDrag);
	if (m_dragState == DragClipFadeIn)
		rect.setRight(m_rectHandle.left() + m_iDragClipX);
	else
	if (m_dragState == DragClipFadeOut)
		rect.setLeft(m_rectHandle.right() + m_iDragClipX);
	showToolTip(rect, 0);
}


// Clip fade-in/out handle settler.
void qtractorTrackView::dragClipFadeDrop ( const QPoint& pos )
{
	if (m_pClipDrag == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	dragClipFadeMove(pos);

	// We'll build a command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("clip %1").arg(
			m_dragState == DragClipFadeIn ? tr("fade-in") : tr("fade-out")));

	if (m_dragState == DragClipFadeIn) {
		pClipCommand->fadeInClip(m_pClipDrag,
			pSession->frameFromPixel(
				m_rectHandle.left() + m_iDragClipX - m_rectDrag.left()),
				m_pClipDrag->fadeInType());
	} 
	else
	if (m_dragState == DragClipFadeOut) {
		pClipCommand->fadeOutClip(m_pClipDrag,
			pSession->frameFromPixel(
				m_rectDrag.right() - m_iDragClipX - m_rectHandle.right()),
				m_pClipDrag->fadeOutType());
	}

	// Reset state for proper redrawing...
	m_dragState = DragNone;

	// Put it in the form of an undoable command...
	pSession->execute(pClipCommand);
}


// Clip resize handle drag-moving parts.
void qtractorTrackView::dragClipResizeMove ( const QPoint& pos )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Always change horizontally wise...
	int x = 0;
	const int dx = (pos.x() - m_posDrag.x());
	QRect rect(m_rectDrag);
	if (m_dragState == DragClipResizeLeft) {
		if (rect.left() > -(dx))
			x = pSession->pixelSnap(rect.left() + dx);
		if (x < 0)
			x = 0;
		else
		if (x > rect.right() - 8)
			x = rect.right() - 8;
		rect.setLeft(x);
	}
	else
	if (m_dragState == DragClipResizeRight) {
		if (rect.right() > -(dx))
			x = pSession->pixelSnap(rect.right() + dx);
		if (x < rect.left() + 8)
			x = rect.left() + 8;
		rect.setRight(x);
	}

	moveRubberBand(&m_pRubberBand, rect, 3);
	showToolTip(rect, 0);
	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);
}


// Clip resize handle settler.
void qtractorTrackView::dragClipResizeDrop (
	const QPoint& pos, bool bTimeStretch )
{
	if (m_pClipDrag == NULL)
		return;

	if (!m_pClipDrag->queryEditor())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// We'll build a command...
	qtractorClipCommand *pClipCommand = new qtractorClipCommand(
		bTimeStretch ? tr("clip stretch") : tr("clip resize"));

	unsigned long iClipStart  = m_pClipDrag->clipStart();
	unsigned long iClipOffset = m_pClipDrag->clipOffset();
	unsigned long iClipLength = m_pClipDrag->clipLength();

	// Always change horizontally wise...
	int x = 0;
	const int dx = (pos.x() - m_posDrag.x());
	if (m_dragState == DragClipResizeLeft) {
		unsigned long iClipDelta;
		x = m_rectDrag.left() + dx;
		if (x < 0)
			x = 0;
		else
		if (x > m_rectDrag.right() - 8)
			x = m_rectDrag.right() - 8;
		iClipStart = pSession->frameSnap(pSession->frameFromPixel(x));
		if (m_pClipDrag->clipStart() > iClipStart) {
			iClipDelta = (m_pClipDrag->clipStart() - iClipStart);
			if (!bTimeStretch) {
				if (iClipOffset  > iClipDelta)
					iClipOffset -= iClipDelta;
				else
					iClipOffset = 0;
			}
			iClipLength += iClipDelta;
		} else {
			iClipDelta = (iClipStart - m_pClipDrag->clipStart());
			if (!bTimeStretch)
				iClipOffset += iClipDelta;
			iClipLength -= iClipDelta;
		}
	}
	else
	if (m_dragState == DragClipResizeRight) {
		x = m_rectDrag.right() + dx;
		if (x < m_rectDrag.left() + 8)
			x = m_rectDrag.left() + 8;
		iClipLength = pSession->frameSnap(pSession->frameFromPixel(x))
			- iClipStart;
	}

	// Time stretching...
	float fTimeStretch = 0.0f;
	if (bTimeStretch && m_pClipDrag->track()) {
		float fOldTimeStretch = 1.0f;
		if ((m_pClipDrag->track())->trackType() == qtractorTrack::Audio) {
			qtractorAudioClip *pAudioClip
				= static_cast<qtractorAudioClip *> (m_pClipDrag);
			if (pAudioClip)
				fOldTimeStretch = pAudioClip->timeStretch();
		}
		fTimeStretch = (float(iClipLength) * fOldTimeStretch)
			/ float(m_pClipDrag->clipLength());
	}

	// Declare the clip resize parcel...
	pClipCommand->resizeClip(m_pClipDrag,
		iClipStart, iClipOffset, iClipLength, fTimeStretch);

	// Put it in the form of an undoable command...
	pSession->execute(pClipCommand);
}


// Automation curve node drag-move methods.
void qtractorTrackView::dragCurveNode ( const QPoint& pos, bool bToggle )
{
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, false, &tvi);
	if (pTrack == NULL)
		return;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return;

	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList == NULL)
		return;

	qtractorCurve *pCurve = pCurveList->currentCurve();
	if (pCurve == NULL)
		return;
	if (pCurve->isLocked())
		return;

	if (m_pDragCurve && m_pDragCurve != pCurve)
		return;

	if (m_pCurveEditCommand == NULL)
		m_pCurveEditCommand = new qtractorCurveEditCommand(pCurve);
	
	qtractorCurve::Node *pNode = m_pDragCurveNode;

	m_pDragCurveNode = NULL;

	if (pNode && m_dragState == DragCurveNode) {
		m_pCurveSelect->removeItem(pNode);
		m_pCurveEditCommand->removeNode(pNode);
		pCurve->unlinkNode(pNode);
		pNode = NULL;
	}

	if (m_pDragCurve == NULL) {
		m_pDragCurve = pCurve;
		m_dragCursor = DragCurveNode;
		qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
	}

	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);

	const int h  = tvi.trackRect.height();
	const int y2 = tvi.trackRect.bottom() + 1;

	if (pNode == NULL) {
		qtractorCurveEditList edits(pCurve);
		const unsigned long frame
			= pSession->frameSnap(pSession->frameFromPixel(pos.x()));
		const float value
			= pCurve->valueFromScale(float(y2 - pos.y()) / float(h));
		pNode = pCurve->addNode(frame, value, &edits);
		m_pCurveEditCommand->addEditList(&edits);
	}

	if (pNode) {
		const float s = pCurve->scaleFromValue(pNode->value);
		const int x = pSession->pixelFromFrame(pNode->frame);
		const int y = y2 - int(s * float(h));
		const QRect rectUpdate = m_pCurveSelect->rect();
		m_pCurveSelect->selectItem(pCurve, pNode,
			QRect(x - 4, y - 4, 8, 8), true, bToggle);
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
		m_pDragCurveNode = pNode;
		if (m_bToolTips) {
			QWidget *pViewport = qtractorScrollView::viewport();
			QToolTip::showText(pViewport->mapToGlobal(contentsToViewport(pos)),
				nodeToolTip(m_pDragCurve, m_pDragCurveNode), pViewport);
		}
	}
}


// Common tool-tip builder for automation nodes.
QString qtractorTrackView::nodeToolTip (
	qtractorCurve *pCurve, qtractorCurve::Node *pNode) const
{
	QString sToolTip;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorTimeScale *pTimeScale = pSession->timeScale();
		if (pTimeScale) {
			sToolTip = QString("(%2, %3)")
				.arg(pTimeScale->textFromFrame(pNode->frame))
				.arg(pNode->value);
			if (pCurve) {
				qtractorSubject *pSubject = pCurve->subject();
				if (pSubject) {
					sToolTip = QString("%1\n%2")
						.arg(pSubject->name())
						.arg(sToolTip);
				}
			}
		}
	}

	return sToolTip;
}


// Reset drag/select/move state.
void qtractorTrackView::resetDragState (void)
{
	// To remember what we were doing...
	const DragState dragState = m_dragState;

	// Should fallback mouse cursor...
	if (m_dragCursor != DragNone)
		qtractorScrollView::unsetCursor();
	if (m_dragState == DragClipStep)
		m_pClipSelect->clear();
	if (m_dragState == DragCurveStep)
		m_pCurveSelect->clear();
	if (m_dragState == DragClipResizeLeft  ||
		m_dragState == DragClipResizeRight ||
		m_dragState == DragClipPasteDrop   ||
		m_dragState == DragClipPaste       ||
		m_dragState == DragClipStep        ||
		m_dragState == DragClipMove        ||
		m_dragState == DragCurvePaste      ||
		m_dragState == DragCurveStep       ||
		m_dragState == DragCurveMove) {
		updateContents();
	}

	// Force null state, now.
	m_dragState  = DragNone;
	m_dragCursor = DragNone;
	m_iDragClipX = 0;
//	m_pClipDrag  = NULL;

	m_posStep = QPoint(0, 0);

	// No pasting nomore.
	m_iPasteCount  = 0;
	m_iPastePeriod = 0;

	// Single track dragging reset.
	m_bDragSingleTrack = false;
	m_iDragSingleTrackY = 0;
	m_iDragSingleTrackHeight = 0;

	// Automation curve stuff reset.
	m_pDragCurve = NULL;
	m_pDragCurveNode = NULL;
	m_iDragCurveX = 0;

	if (m_pCurveEditCommand)
		delete m_pCurveEditCommand;
	m_pCurveEditCommand = NULL;

	// If we were moving clips around,
	// just hide selection, of course.
	hideClipSelect();

	// Just hide the rubber-band...
	if (m_pRubberBand) {
		m_pRubberBand->hide();
		delete m_pRubberBand;
		m_pRubberBand = NULL;
	}

	// If we were dragging fade-slope lines, refresh...
	if (dragState == DragClipFadeIn || dragState == DragClipFadeOut)
		updateRect(m_rectDrag);

	// No dropping files, whatsoever.
	qDeleteAll(m_dropItems);
	m_dropItems.clear();
	m_dropType = qtractorTrack::None;
}


// Keyboard event handler.
void qtractorTrackView::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTrackView::keyPressEvent(%d)", pKeyEvent->key());
#endif
	const int iKey = pKeyEvent->key();
	switch (iKey) {
	case Qt::Key_Insert: // Aha, joking :)
	case Qt::Key_Return:
		if (m_dragState == DragClipStep)
			moveClipSelect(dragClipMove(m_posDrag + m_posStep));
		else
		if (m_dragState == DragCurveStep)
			moveCurveSelect(m_posDrag + m_posStep);
		else {
			const QPoint& pos = qtractorScrollView::viewportToContents(
				qtractorScrollView::viewport()->mapFromGlobal(QCursor::pos()));
			if (m_dragState == DragClipMove)
				moveClipSelect(dragClipMove(pos + m_posStep));
			else if (m_dragState == DragClipPaste)
				pasteClipSelect(dragClipMove(pos + m_posStep));
			else if (m_dragState == DragClipPasteDrop)
				dropClip(pos + m_posStep);
			else if (m_dragState == DragCurveMove)
				moveCurveSelect(pos + m_posStep);
			else if (m_dragState == DragCurvePaste)
				pasteCurveSelect(pos + m_posStep);
		}
		// Fall thru...
	case Qt::Key_Escape:
		// HACK: Force selection clearance!
		m_dragState = (m_bCurveEdit ? DragCurveStep : DragClipStep);
		resetDragState();
		break;
	case Qt::Key_Home:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(0, 0);
		} else {
			qtractorScrollView::setContentsPos(
				0, qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_End:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsWidth(),
				qtractorScrollView::contentsHeight());
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsWidth(),
				qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_Left:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() - qtractorScrollView::width(),
				qtractorScrollView::contentsY());
		} else if (!keyStep(iKey)) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() - 16,
				qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_Right:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() + qtractorScrollView::width(),
				qtractorScrollView::contentsY());
		} else if (!keyStep(iKey)) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() + 16,
				qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_Up:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY() - qtractorScrollView::height());
		} else if (!keyStep(iKey)) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY() - 16);
		}
		break;
	case Qt::Key_Down:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY() + qtractorScrollView::height());
		} else if (!keyStep(iKey)) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY() + 16);
		}
		break;
	case Qt::Key_PageUp:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(), 16);
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY() - qtractorScrollView::height());
		}
		break;
	case Qt::Key_PageDown:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsHeight());
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY() + qtractorScrollView::height());
		}
		break;
	default:
		qtractorScrollView::keyPressEvent(pKeyEvent);
		break;
	}
}


// Keyboard step handler.
bool qtractorTrackView::keyStep ( int iKey )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Set initial bound conditions...
	if (m_dragState == DragNone) {
		if (isClipSelected() || !m_dropItems.isEmpty()) {
			m_dragState = m_dragCursor = DragClipStep;
			m_rectDrag  = m_pClipSelect->rect();
			m_posDrag   = m_rectDrag.topLeft();
			m_posStep   = QPoint(0, 0);
			m_iDragClipX = (pSession->pixelSnap(m_rectDrag.x()) - m_rectDrag.x());
			qtractorScrollView::setCursor(QCursor(Qt::SizeAllCursor));
		}
		else
		if (isCurveSelected() && m_bCurveEdit
			&& iKey != Qt::Key_Up && iKey != Qt::Key_Down)  {
			m_dragState = m_dragCursor = DragCurveStep;
			m_rectDrag  = m_pCurveSelect->rect();
			m_posDrag   = m_rectDrag.topLeft();
			m_posStep   = QPoint(0, 0);
			m_iDragCurveX = (pSession->pixelSnap(m_rectDrag.x()) - m_rectDrag.x());
			qtractorScrollView::setCursor(QCursor(Qt::SizeHorCursor));
		}
	}

	// Now to say the truth...
	const bool bClipStep =
		m_dragState == DragClipMove  ||
		m_dragState == DragClipStep  ||
		m_dragState == DragClipPaste ||
		m_dragState == DragClipPasteDrop;

	const bool bCurveStep =
		m_dragState == DragCurveMove ||
		m_dragState == DragCurveStep ||
		m_dragState == DragCurvePaste;

	if (!bClipStep && !bCurveStep)
		return false;

	// Determine vertical step...
	if (iKey == Qt::Key_Up || iKey == Qt::Key_Down)  {
		if (bClipStep) {
			int iVerticalStep = qtractorTrack::HeightMin;
			qtractorTrack *pTrack = m_pTracks->currentTrack();
			if (pTrack)
				iVerticalStep += (pTrack->zoomHeight() >> 1);
			int y0 = m_posDrag.y();
			int y1 = y0 + m_posStep.y();
			if (iKey == Qt::Key_Up)
				y1 -= iVerticalStep;
			else
				y1 += iVerticalStep;
			m_posStep.setY((y1 < 0 ? 0 : y1) - y0);
		}
	}
	else
	// Determine horizontal step...
	if (iKey == Qt::Key_Left || iKey == Qt::Key_Right)  {
		int iHorizontalStep = 0;
		const int x0 = m_posDrag.x();
		int x1 = x0 + m_posStep.x();
		qtractorTimeScale::Cursor cursor(pSession->timeScale());
		qtractorTimeScale::Node *pNode = cursor.seekPixel(x1);
		const unsigned short iSnapPerBeat = pSession->snapPerBeat();
		if (iSnapPerBeat > 0)
			iHorizontalStep = pNode->pixelsPerBeat() / iSnapPerBeat;
		if (iHorizontalStep < 1)
			iHorizontalStep = 1;
		if (iKey == Qt::Key_Left)
			x1 -= iHorizontalStep;
		else
			x1 += iHorizontalStep;
		m_posStep.setX(pSession->pixelSnap(x1 < 0 ? 0 : x1) - x0);
	}

	// Early sanity check...
	QRect rect = m_rectDrag;
	if (bClipStep && m_dragState != DragClipPasteDrop)
		rect = m_pClipSelect->rect();
	else
	if (bCurveStep)
		rect = m_pCurveSelect->rect();

	QPoint pos = m_posDrag;
	if (m_dragState != DragClipStep &&
		m_dragState != DragCurveStep) {
		pos = qtractorScrollView::viewportToContents(
			qtractorScrollView::viewport()->mapFromGlobal(QCursor::pos()));
	}

	int x2 = - pos.x();
	int y2 = - pos.y();
	if (m_dragState != DragClipStep &&
		m_dragState != DragCurveStep) {
		x2 += (m_posDrag.x() - rect.x());
		y2 += (m_posDrag.y() - rect.y());
	}

	if (m_posStep.x() < x2) {
		m_posStep.setX (x2);
	} else {
		x2 += qtractorScrollView::contentsWidth() - (rect.width() >> 1);
		if (m_posStep.x() > x2)
			m_posStep.setX (x2);
	}

	if (bClipStep) {
		if (m_posStep.y() < y2) {
			m_posStep.setY (y2);
		} else {
			y2 += qtractorScrollView::contentsHeight() - (rect.height() >> 1);
			if (m_posStep.y() > y2)
				m_posStep.setY (y2);
		}
	}

	// Do our deeds (flag we're key-steppin')...
	if (m_dragState == DragClipPasteDrop) {
		dragClipDrop(pos + m_posStep, true);
		showClipDropRects();
	} else if (bClipStep) {
		dragClipMove(pos + m_posStep, true);
	} else if (bCurveStep) {
		dragCurveMove(pos + m_posStep, true);
	}

	return true;
}


// Make given frame position visible in view.
void qtractorTrackView::ensureVisibleFrame ( unsigned long iFrame )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		const int x0 = qtractorScrollView::contentsX();
		const int y  = qtractorScrollView::contentsY();
		const int w  = m_pixmap.width();
		const int w3 = w - (w >> 3);
		int x  = pSession->pixelFromFrame(iFrame);
		if (x < x0)
			x -= w3;
		else if (x > x0 + w3)
			x += w3;
		qtractorScrollView::ensureVisible(x, y, 0, 0);
	//	qtractorScrollView::setFocus();
	}
}


// Session cursor accessor.
qtractorSessionCursor *qtractorTrackView::sessionCursor (void) const
{
	return m_pSessionCursor;
}


// Vertical line positioning.
void qtractorTrackView::drawPositionX ( int& iPositionX, int x, bool bSyncView )
{
	// Update track-view position...
	const int x0 = qtractorScrollView::contentsX();
	const int w  = qtractorScrollView::width();
	const int h  = qtractorScrollView::height();
	const int wm = (w >> 3);

	// Time-line header extents...
	const int h0 = m_pTracks->trackTime()->height();
	const int d0 = (h0 >> 1);

	// Restore old position...
	int x1 = iPositionX - x0;
	if (iPositionX != x && x1 >= 0 && x1 < w + d0) {
		// Override old view line...
		qtractorScrollView::viewport()->update(QRect(x1, 0, 1, h));
		((m_pTracks->trackTime())->viewport())->update(
			QRect(x1 - d0, d0, h0, d0));
	}

	// New position is in...
	iPositionX = x;

	// Force position to be in view?
	if (bSyncView && (x < x0 || x > x0 + w - wm)
		&& m_dragState == DragNone && m_dragCursor == DragNone
	//	&& QApplication::mouseButtons() == Qt::NoButton
		&& --m_iSyncViewHold < 0) {
		// Maybe we'll need some head-room...
		if (x < qtractorScrollView::contentsWidth() - w) {
			qtractorScrollView::setContentsPos(
				x - wm, qtractorScrollView::contentsY());
		}
		else updateContentsWidth(x + w);
		m_iSyncViewHold = 0;
	} else {
		// Draw the line, by updating the new region...
		x1 = x - x0;
		if (x1 >= 0 && x1 < w + d0) {
			qtractorScrollView::viewport()->update(QRect(x1, 0, 1, h));
			((m_pTracks->trackTime())->viewport())->update(
				QRect(x1 - d0, d0, h0, d0));
		}
	}
}


// Playhead positioning.
void qtractorTrackView::setPlayHead ( unsigned long iPlayHead, bool bSyncView )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		m_iPlayHead = iPlayHead;
		const int iPlayHeadX = pSession->pixelFromFrame(iPlayHead);
		drawPositionX(m_iPlayHeadX, iPlayHeadX, bSyncView);
	}
}

unsigned long qtractorTrackView::playHead (void) const
{
	return m_iPlayHead;
}

int qtractorTrackView::playHeadX (void) const
{
	return m_iPlayHeadX;
}


// Edit-head positioning
void qtractorTrackView::setEditHead ( unsigned long iEditHead )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		if (iEditHead > pSession->editTail())
			setEditTail(iEditHead);
		else
			setSyncViewHoldOn(true);
		pSession->setEditHead(iEditHead);
		const int iEditHeadX = pSession->pixelFromFrame(iEditHead);
		drawPositionX(m_iEditHeadX, iEditHeadX);
	}
}

int qtractorTrackView::editHeadX (void) const
{
	return m_iEditHeadX;
}


// Edit-tail positioning
void qtractorTrackView::setEditTail ( unsigned long iEditTail )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		if (iEditTail < pSession->editHead())
			setEditHead(iEditTail);
		else
			setSyncViewHoldOn(true);
		pSession->setEditTail(iEditTail);
		const int iEditTailX = pSession->pixelFromFrame(iEditTail);
		drawPositionX(m_iEditTailX, iEditTailX);
	}
}

int qtractorTrackView::editTailX (void) const
{
	return m_iEditTailX;
}


// Clear current selection (no notify).
void qtractorTrackView::clearSelect (void)
{
//	g_clipboard.clear();

	int iUpdate = 0;

	QRect rectUpdate = m_pClipSelect->rect();
	if (m_pClipSelect->items().count() > 0) {
		m_pClipSelect->clear();
		++iUpdate;
	}
	if (m_pCurveSelect->items().count() > 0) {
		rectUpdate = rectUpdate.united(m_pCurveSelect->rect());
		m_pCurveSelect->clear();
		++iUpdate;
	}

	if (iUpdate > 0)
		updateRect(rectUpdate);
}


// Whether there's any clip currently selected.
bool qtractorTrackView::isClipSelected (void) const
{
	return (m_pClipSelect->items().count() > 0);
}


// Whether there's any curve/automation currently selected.
bool qtractorTrackView::isCurveSelected (void) const
{
	return (m_pCurveSelect->items().count() > 0);
}


// Whether there's a single track selection.
qtractorTrack *qtractorTrackView::singleTrackSelected (void)
{
	return m_pClipSelect->singleTrack();
}


// Whether there's any clip on clipboard. (static)
bool qtractorTrackView::isClipboard (void)
{
	return (g_clipboard.clips.count() > 0 || g_clipboard.nodes.count() > 0);
}


// Clipboard stuffer methods.
void qtractorTrackView::ClipBoard::addClip ( qtractorClip *pClip,
	const QRect& clipRect, unsigned long iClipStart,
	unsigned long iClipOffset, unsigned long iClipLength )
{
	ClipItem *pClipItem
		= new ClipItem(pClip, clipRect, iClipStart, iClipOffset, iClipLength);
	if (iClipOffset == pClip->clipOffset())
		pClipItem->fadeInLength = pClip->fadeInLength();
	if (iClipOffset + iClipLength == pClip->clipOffset() + pClip->clipLength())
		pClipItem->fadeOutLength = pClip->fadeOutLength();
	clips.append(pClipItem);
}


void qtractorTrackView::ClipBoard::addNode ( qtractorCurve::Node *pNode,
	const QRect& nodeRect, unsigned long iFrame, float fValue )
{
	nodes.append(new NodeItem(pNode, nodeRect, iFrame, fValue));
}


// Clipboard reset method.
void qtractorTrackView::ClipBoard::clear (void)
{
	qDeleteAll(clips);
	clips.clear();

	qDeleteAll(nodes);
	nodes.clear();

	singleTrack = NULL;
	frames = 0;
}


// Clip selection sanity check method.
bool qtractorTrackView::queryClipSelect ( qtractorClip *pClip )
{
	// Check if anything is really selected...
	if (m_pClipSelect->items().count() < 1)
		return (pClip && pClip->queryEditor());

	// Just ask whether any target clips have pending editors...
	QList<qtractorClip *> clips;

	const qtractorClipSelect::ItemList& items = m_pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorClip *pClip = iter.key();
		// Make sure it's a legal selection...
		if (pClip->track() && pClip->isClipSelected())
			clips.append(pClip);
	}

	QListIterator<qtractorClip *> clips_iter(clips);
	while (clips_iter.hasNext()) {
		qtractorClip *pClip = clips_iter.next();
		// Ask if it has any pending editor...
		if (!pClip->queryEditor())
			return false;
	}

	// If it reaches here, we can do what we will to...
	return true;
}


// Curve/automation selection executive method.
void qtractorTrackView::executeCurveSelect ( qtractorTrackView::Command cmd )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorCurve *pCurve = m_pCurveSelect->curve();
	if (pCurve == NULL)
		return;

	// Reset clipboard...
	const bool bClipboard = (cmd == Cut || cmd == Copy);
	if (bClipboard) {
		g_clipboard.clear();
		g_clipboard.frames = pSession->frameFromPixel(m_pCurveSelect->rect().width());
		QApplication::clipboard()->clear();
	}

	// We'll build a composite command...
	qtractorCurveEditCommand *pCurveEditCommand = NULL;
	const QString& sCmdMask = tr("%1 automation");
	QString sCmdName;
	switch (cmd) {
	case Cut:
		sCmdName = sCmdMask.arg(tr("cut"));
		break;
	case Delete:
		sCmdName = sCmdMask.arg(tr("delete"));
		break;
	default:
		break;
	}

	if (!sCmdName.isEmpty())
		pCurveEditCommand = new qtractorCurveEditCommand(sCmdName, pCurve);

	const qtractorCurveSelect::ItemList& items
		= m_pCurveSelect->items();
	qtractorCurveSelect::ItemList::ConstIterator iter
		= items.constBegin();
	const qtractorCurveSelect::ItemList::ConstIterator iter_end
		= items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorCurve::Node *pNode = iter.key();
		if (bClipboard) {
			g_clipboard.addNode(pNode,
				iter.value()->rectNode,
				pNode->frame,
				pNode->value);
		}
		if (pCurveEditCommand)
			pCurveEditCommand->removeNode(pNode);
	}

	// Hint from the whole selection rectangle...
	g_clipboard.frames = pSession->frameFromPixel(m_pCurveSelect->rect().width());

	// Put it in the form of an undoable command...
	if (pCurveEditCommand)
		pSession->execute(pCurveEditCommand);
}


// Clip selection executive method.
void qtractorTrackView::executeClipSelect (
	qtractorTrackView::Command cmd, qtractorClip *pClipEx )
{
	// Check if it's all about a single clip target...
	if (m_pClipSelect->items().count() < 1) {
		if (pClipEx == NULL)
			pClipEx = currentClip();
	}
	else pClipEx = NULL;	// Selection always takes precedence...

	// Check if anything is really selected and sane...
	if (!queryClipSelect(pClipEx))
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Reset clipboard...
	const bool bClipboard = (cmd == Cut || cmd == Copy);
	if (bClipboard) {
		g_clipboard.clear();
		g_clipboard.singleTrack = m_pClipSelect->singleTrack();
		g_clipboard.frames = pSession->frameFromPixel(m_pClipSelect->rect().width());
		QApplication::clipboard()->clear();
	}

	// We'll build a composite command...
	qtractorClipCommand *pClipCommand = NULL;
	const QString& sCmdMask = tr("%1 clip");
	QString sCmdName;
	switch (cmd) {
	case Cut:
		sCmdName = sCmdMask.arg(tr("cut"));
		break;
	case Delete:
		sCmdName = sCmdMask.arg(tr("delete"));
		break;
	case Split:
		sCmdName = sCmdMask.arg(tr("split"));
		break;
	default:
		break;
	}
	if (!sCmdName.isEmpty())
		pClipCommand = new qtractorClipCommand(sCmdName);

	if (pClipEx) {
		// -- Single clip...
		if (bClipboard) {
			QRect rectClip;
			clipInfo(pClipEx, &rectClip);
			g_clipboard.addClip(pClipEx,
				rectClip,
				pClipEx->clipStart(),
				pClipEx->clipOffset(),
				pClipEx->clipLength());
		}
		if (pClipCommand && cmd != Split)
			pClipCommand->removeClip(pClipEx);
		// Done, single clip.
	}

	const qtractorClipSelect::ItemList& items = m_pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorClip  *pClip  = iter.key();
		qtractorTrack *pTrack = pClip->track();
		// Make sure it's legal selection...
		if (pTrack && pClip->isClipSelected()) {
			// Clip parameters.
			const unsigned long iClipStart    = pClip->clipStart();
			const unsigned long iClipOffset   = pClip->clipOffset();
			const unsigned long iClipLength   = pClip->clipLength();
			const unsigned long iClipEnd      = iClipStart + iClipLength;
			// Clip selection points.
			const unsigned long iSelectStart  = pClip->clipSelectStart();
			const unsigned long iSelectEnd    = pClip->clipSelectEnd();
			const unsigned long iSelectOffset = iSelectStart - iClipStart;
			const unsigned long iSelectLength = iSelectEnd - iSelectStart;
			// Determine and dispatch selected clip regions...
			qtractorClipSelect::Item *pClipItem = iter.value();
			if (iSelectStart > iClipStart) {
				if (iSelectEnd < iClipEnd) {
					// -- Middle region...
					if (bClipboard) {
						g_clipboard.addClip(pClip,
							pClipItem->rectClip,
							iSelectStart,
							iClipOffset + iSelectOffset,
							iSelectLength);
					}
					if (pClipCommand) {
						// Left-clip...
						qtractorClip *pClipLeft = cloneClip(pClip);
						if (pClipLeft) {
							pClipLeft->setClipStart(iClipStart);
							pClipLeft->setClipOffset(iClipOffset);
							pClipLeft->setClipLength(iSelectOffset);
							pClipLeft->setFadeInLength(pClip->fadeInLength());
							pClipCommand->addClip(pClipLeft, pTrack);
						}
						// Split(middle)-clip...
						if (cmd == Split) {
							// Middle-clip...
							pClipCommand->resizeClip(pClip,
								iSelectStart,
								iClipOffset + iSelectOffset,
								iSelectLength);
							// Adjust middle-clip selection...
							pClip->setClipSelect(
								iSelectStart, iSelectStart + iSelectLength);
						}
						else pClipCommand->removeClip(pClip);
						// Right-clip...
						qtractorClip *pClipRight = cloneClip(pClip);
						if (pClipRight) {
							pClipRight->setClipStart(iSelectEnd);
							pClipRight->setClipOffset(iClipOffset
								+ iSelectOffset + iSelectLength);
							pClipRight->setClipLength(iClipEnd - iSelectEnd);
							pClipRight->setFadeOutLength(pClip->fadeOutLength());
							pClipCommand->addClip(pClipRight, pTrack);
						}
					}
					// Done, middle region.
				} else {
					// -- Right region...
					if (bClipboard) {
						g_clipboard.addClip(pClip,
							pClipItem->rectClip,
							iSelectStart,
							iClipOffset + iSelectOffset,
							iSelectLength);
					}
					if (pClipCommand) {
						// Left-clip...
						qtractorClip *pClipLeft = cloneClip(pClip);
						if (pClipLeft) {
							pClipLeft->setClipStart(iClipStart);
							pClipLeft->setClipOffset(iClipOffset);
							pClipLeft->setClipLength(iSelectOffset);
							pClipLeft->setFadeInLength(pClip->fadeInLength());
							pClipCommand->addClip(pClipLeft, pTrack);
						}
						// Split(right)-clip...
						if (cmd == Split) {
							// Right-clip...
							pClipCommand->resizeClip(pClip,
								iSelectStart,
								iClipOffset + iSelectOffset,
								iSelectLength);
							// Adjust middle-clip selection...
							pClip->setClipSelect(
								iSelectStart, iSelectStart + iSelectLength);
						}
						else pClipCommand->removeClip(pClip);
					}
					// Done, right region.
				}
			}
			else
			if (iSelectEnd < iClipEnd) {
				// -- Left region...
				if (bClipboard) {
					g_clipboard.addClip(pClip,
						pClipItem->rectClip,
						iClipStart,
						iClipOffset,
						iSelectLength);
				}
				if (pClipCommand) {
					// Split(left)-clip...
					if (cmd == Split) {
						// Left-clip...
						pClipCommand->resizeClip(pClip,
							iClipStart,
							iClipOffset,
							iSelectLength);
						// Adjust middle-clip selection...
						pClip->setClipSelect(
							iClipStart, iClipStart + iSelectLength);
					}
					else pClipCommand->removeClip(pClip);
					// Right-clip...
					qtractorClip *pClipRight = cloneClip(pClip);
					if (pClipRight) {
						pClipRight->setClipStart(iSelectEnd);
						pClipRight->setClipOffset(iClipOffset + iSelectLength);
						pClipRight->setClipLength(iClipLength - iSelectLength);
						pClipRight->setFadeOutLength(pClip->fadeOutLength());
						pClipCommand->addClip(pClipRight, pTrack);
					}
				}
				// Done, left region.
			} else {
				// -- Whole clip...
				if (bClipboard) {
					g_clipboard.addClip(pClip,
						pClipItem->rectClip,
						iClipStart,
						iClipOffset,
						iClipLength);
				}
				if (pClipCommand && cmd != Split)
					pClipCommand->removeClip(pClip);
				// Done, whole clip.
			}
		}
	}

	// Hint from the whole selection rectangle...
	g_clipboard.frames = (pClipEx ? pClipEx->clipLength()
		: pSession->frameFromPixel(m_pClipSelect->rect().width()));

	// Reset selection on cut or delete;
	// put it in the form of an undoable command...
	if (pClipCommand) {
		if (cmd == Split)
			pClipCommand->setClearSelect(false);
		pSession->execute(pClipCommand);
	}
}


// Retrieve current paste period.
// (as from current clipboard width)
unsigned long qtractorTrackView::pastePeriod (void) const
{
	if (g_clipboard.clips.count() < 1)
		return 0;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return 0;

	return pSession->frameSnap(g_clipboard.frames);
}


// Paste from clipboard (start).
void qtractorTrackView::pasteClipboard (
	unsigned short iPasteCount, unsigned long iPastePeriod )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTrackView::pasteClipboard(%u, %lu)",
		iPasteCount, iPastePeriod);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Make sure the mouse pointer is properly located...
	const QPoint& pos = qtractorScrollView::viewportToContents(
		qtractorScrollView::viewport()->mapFromGlobal(QCursor::pos()));

	// Check if anything's really on clipboard...
	if (g_clipboard.clips.isEmpty() &&
		g_clipboard.nodes.isEmpty()) {
		// System clipboard?
		QClipboard *pClipboard = QApplication::clipboard();
		if (pClipboard && (pClipboard->mimeData())->hasUrls()) {
			dragClipDrop(pos, false, pClipboard->mimeData());
			// Make a proper out of this (new) state?
			if (!m_dropItems.isEmpty()) {
				m_dragState = m_dragCursor = DragClipPasteDrop;
				// It doesn't matter which one, both pasteable views are due...
				qtractorScrollView::setFocus();
				qtractorScrollView::setCursor(
					QCursor(QPixmap(":/images/editPaste.png"), 12, 12));
				// Update the pasted stuff
				showClipDropRects();
			}
		}
		// Woot!
		return;
	}

	// FIXME: While pasting automation/curve nodes
	// maybe we can only do it over the original...
	qtractorTrackViewInfo tvi;
	qtractorCurve *pCurve = NULL;
	if (m_bCurveEdit) {
		if (g_clipboard.nodes.isEmpty())
			return;
		qtractorTrack *pTrack = m_pTracks->currentTrack();
		if (pTrack == NULL || !trackInfo(pTrack, &tvi))
			pTrack = trackAt(pos, true, &tvi);
		if (pTrack == NULL)
			return;
		pCurve = pTrack->currentCurve();
		if (pCurve == NULL)
			return;
		if (pCurve->isLocked())
			return;
	}
	else
	if (g_clipboard.clips.isEmpty())
		return;

	// Reset any current selection, whatsoever...
	m_pClipSelect->clear();
	m_pCurveSelect->clear();

	resetDragState();

	// Set paste parameters...
	m_iPasteCount  = iPasteCount;
	m_iPastePeriod = iPastePeriod;
	
	if (m_iPastePeriod < 1)
		m_iPastePeriod = pSession->frameSnap(g_clipboard.frames);

	unsigned long iPasteDelta = 0;

	// Copy clipboard items to floating selection;
	if (m_bCurveEdit) {
		// Flag this as target current curve,
		// otherwise nothing will be displayed...
		m_pCurveSelect->setCurve(pCurve);
		const int h = tvi.trackRect.height();
		const int y2 = tvi.trackRect.bottom() + 1;
		QListIterator<NodeItem *> iter(g_clipboard.nodes);
		for (unsigned short i = 0; i < m_iPasteCount; ++i) {
			iter.toFront();
			while (iter.hasNext()) {
				NodeItem *pNodeItem = iter.next();
				const int x
					= pSession->pixelFromFrame(pNodeItem->frame + iPasteDelta);
				const float s = pCurve->scaleFromValue(pNodeItem->value);
				const int y = y2 - int(s * float(h));
				m_pCurveSelect->addItem(pNodeItem->node,
					QRect(x - 4, y - 4, 8, 8));
			}
			iPasteDelta += m_iPastePeriod;
		}
		m_pCurveSelect->update(true);
		// We'll start a brand new floating state...
		m_dragState = m_dragCursor = DragCurvePaste;
		m_rectDrag  = m_pCurveSelect->rect();
		m_posDrag   = m_rectDrag.topLeft();
		m_posStep   = QPoint(0, 0);
	} else {
		// Copy clipboard items to floating selection;
		// adjust clip widths/lengths just in case time
		// scale (horizontal zoom) has been changed...
		QListIterator<ClipItem *> iter(g_clipboard.clips);
		for (unsigned short i = 0; i < m_iPasteCount; ++i) {
			iter.toFront();
			while (iter.hasNext()) {
				ClipItem *pClipItem = iter.next();
				QRect rect(pClipItem->rect);
				rect.setX(pSession->pixelFromFrame(pClipItem->clipStart + iPasteDelta));
				rect.setWidth(pSession->pixelFromFrame(pClipItem->clipLength));
				m_pClipSelect->addItem(pClipItem->clip, rect);
			}
			iPasteDelta += m_iPastePeriod;
		}
		// We'll start a brand new floating state...
		m_dragState = m_dragCursor = DragClipPaste;
		m_rectDrag  = m_pClipSelect->rect();
		m_posDrag   = m_rectDrag.topLeft();
		m_posStep   = QPoint(0, 0);
	}

	// It doesn't matter which one, both pasteable views are due...
	qtractorScrollView::setFocus();
	qtractorScrollView::setCursor(
		QCursor(QPixmap(":/images/editPaste.png"), 12, 12));

	// Let's-a go...
	qtractorScrollView::update();

	if (m_bCurveEdit)
		dragCurveMove(pos + m_posStep);
	else
		dragClipMove(pos + m_posStep);
}


// Intra-drag-n-drop clip move method.
void qtractorTrackView::moveClipSelect ( qtractorTrack *pTrack )
{
	// Check if anything is really selected and sane...
	if (!queryClipSelect())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// We'll need this...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("move clip"));

	// We can only move clips between tracks of the same type...
	int  iTrackClip = 0;
	long iClipDelta = 0;
	const bool bAddTrack = (pTrack == NULL);
	qtractorTrack *pSingleTrack = m_pClipSelect->singleTrack();
	if (pSingleTrack) {
		if (bAddTrack) {
			const int iTrack = pSession->tracks().count() + 1;
			const QColor& color = qtractorTrack::trackColor(iTrack);
			pTrack = new qtractorTrack(pSession, pSingleTrack->trackType());
		//	pTrack->setTrackName(tr("Track %1").arg(iTrack));
			pTrack->setBackground(color);
			pTrack->setForeground(color.darker());
			if (pSingleTrack->trackType() == qtractorTrack::Midi) {
				pTrack->setMidiChannel(pSingleTrack->midiChannel());
				pTrack->setMidiBank(pSingleTrack->midiBank());
				pTrack->setMidiProg(pSingleTrack->midiProg());
			}
			pClipCommand->addTrack(pTrack);
		}
		else
		if (pSingleTrack->trackType() != pTrack->trackType())
			return;
	}

	// We'll build a composite command...
	QList<qtractorClip *> clips;

	const qtractorClipSelect::ItemList& items = m_pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorClip *pClip = iter.key();
		if (pSingleTrack == NULL)
			pTrack = pClip->track();
		// Make sure it's legal selection...
		if (pTrack && pClip->isClipSelected()) {
			// Clip parameters.
			const unsigned long iClipStart    = pClip->clipStart();
			const unsigned long iClipOffset   = pClip->clipOffset();
			const unsigned long iClipLength   = pClip->clipLength();
			const unsigned long iClipEnd      = iClipStart + iClipLength;
			// Clip selection points.
			const unsigned long iSelectStart  = pClip->clipSelectStart();
			const unsigned long iSelectEnd    = pClip->clipSelectEnd();
			const unsigned long iSelectOffset = iSelectStart - iClipStart;
			const unsigned long iSelectLength = iSelectEnd - iSelectStart;
			// Determine and keep clip regions...
			qtractorClipSelect::Item *pClipItem = iter.value();
			if (iSelectStart > iClipStart) {
				// -- Left clip...
				qtractorClip *pClipLeft = cloneClip(pClip);
				pClipLeft->setClipStart(iClipStart);
				pClipLeft->setClipOffset(iClipOffset);
				pClipLeft->setClipLength(iSelectOffset);
				pClipLeft->setFadeInLength(pClip->fadeInLength());
				pClipCommand->addClip(pClipLeft, pClipLeft->track());
				// Done, left clip.
			}
			if (iSelectEnd < iClipEnd) {
				// -- Right clip...
				qtractorClip *pClipRight = cloneClip(pClip);
				pClipRight->setClipStart(iSelectEnd);
				pClipRight->setClipOffset(iClipOffset
					+ iSelectOffset + iSelectLength);
				pClipRight->setClipLength(iClipEnd - iSelectEnd);
				pClipRight->setFadeOutLength(pClip->fadeOutLength());
				pClipCommand->addClip(pClipRight, pClipRight->track());
				// Done, right clip.
			}
			// Convert to precise frame positioning,
			// but only the first clip gets snapped...
			unsigned long iClipStart2 = iSelectStart;
			if (iTrackClip == 0) {
				const int x = (pClipItem->rectClip.x() + m_iDragClipX);
				const unsigned long iFrameStart = pSession->frameSnap(
					pSession->frameFromPixel(x > 0 ? x : 0));
				iClipDelta  = long(iFrameStart) - long(iClipStart2);
				iClipStart2 = iFrameStart;
			} else if (long(iClipStart2) + iClipDelta > 0) {
				iClipStart2 += iClipDelta;
			} else {
				iClipStart2 = 0;
			}
			// -- Moved clip...
			pClipCommand->moveClip(pClip, pTrack,
				iClipStart2,
				iClipOffset + iSelectOffset,
				iSelectLength);
			clips.append(pClip);
			// If track's new it will need a name...
			if (bAddTrack && iTrackClip == 0)
				pTrack->setTrackName(pClip->clipName());
			++iTrackClip;
		}
	}

	// Put it in the form of an undoable command...
	pSession->execute(pClipCommand);

	// Redo selection as new...
	if (!clips.isEmpty()) {
		QListIterator<qtractorClip *> clip_iter(clips);
		while (clip_iter.hasNext())
			clip_iter.next()->setClipSelected(true);
		updateClipSelect();
		m_pTracks->selectionChangeNotify();
	}
}


// Paste from clipboard (execute).
void qtractorTrackView::pasteClipSelect ( qtractorTrack *pTrack )
{
	// Check if there's anything really on clipboard...
	if (g_clipboard.clips.count() < 1)
		return;

	// Check if anything is really selected and sane...
	if (!queryClipSelect())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// We'll need this...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("paste clip"));

	// We can only move clips between tracks of the same type...
	const bool bAddTrack = (pTrack == NULL);
	qtractorTrack *pSingleTrack = g_clipboard.singleTrack;
	if (pSingleTrack) {
		if (bAddTrack) {
			const int iTrack = pSession->tracks().count() + 1;
			const QColor& color = qtractorTrack::trackColor(iTrack);
			pTrack = new qtractorTrack(pSession, pSingleTrack->trackType());
		//	pTrack->setTrackName(tr("Track %1").arg(iTrack));
			pTrack->setBackground(color);
			pTrack->setForeground(color.darker());
			pClipCommand->addTrack(pTrack);
		}
		else
		if (pSingleTrack->trackType() != pTrack->trackType())
			return;
	}

	unsigned long iPastePeriod = m_iPastePeriod;
	if (iPastePeriod < 1)
		iPastePeriod = g_clipboard.frames;

	long iPasteDelta = 0;
	if (m_iDragClipX < 0)
		iPasteDelta = - pSession->frameFromPixel(- m_iDragClipX);
	else
		iPasteDelta = + pSession->frameFromPixel(+ m_iDragClipX);

	// We'll build a composite command...
	QList<qtractorClip *> clips;

	QListIterator<ClipItem *> iter(g_clipboard.clips);
	for (unsigned short i = 0; i < m_iPasteCount; ++i) {
		// Paste iteration...
		int iTrackClip = 0;
		iter.toFront();
		while (iter.hasNext()) {
			ClipItem *pClipItem = iter.next();
			qtractorClip *pClip = pClipItem->clip;
			if (pSingleTrack == NULL)
				pTrack = pClip->track();
			// Convert to precise frame positioning,
			// but only the first clip gets snapped...
			unsigned long iClipStart = pClipItem->clipStart;
			if (long(iClipStart) + iPasteDelta > 0)
				iClipStart += iPasteDelta;
			if (iTrackClip == 0) {
				unsigned long iFrameStart = iClipStart;
				iClipStart = pSession->frameSnap(iFrameStart);
				iPasteDelta += long(iClipStart) - long(iFrameStart);
			}
			// Now, its imperative to make a proper copy of those clips...
			qtractorClip *pNewClip = cloneClip(pClip);
			// Add the new pasted clip...
			if (pNewClip) {
				pNewClip->setClipStart(iClipStart);
				pNewClip->setClipOffset(pClipItem->clipOffset);
				pNewClip->setClipLength(pClipItem->clipLength);
				pNewClip->setFadeInLength(pClipItem->fadeInLength);
				pNewClip->setFadeOutLength(pClipItem->fadeOutLength);
				pClipCommand->addClip(pNewClip, pTrack);
				clips.append(pNewClip);
				// If track's new it will need a name...
				if (bAddTrack && iTrackClip == 0)
					pTrack->setTrackName(pClip->clipName());
				++iTrackClip;
			}
		}
		// Set to repeat...
		iPasteDelta += iPastePeriod;
	}

	// Put it in the form of an undoable command...
	pSession->execute(pClipCommand);

	// Redo selection as new...
	if (!clips.isEmpty()) {
		QListIterator<qtractorClip *> clip_iter(clips);
		while (clip_iter.hasNext())
			clip_iter.next()->setClipSelected(true);
		updateClipSelect();
		m_pTracks->selectionChangeNotify();
	}
}


// Intra-drag-n-drop curve/automation node move method.
void qtractorTrackView::moveCurveSelect ( const QPoint& pos )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	dragCurveMove(pos);

	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = m_pTracks->currentTrack();
	if (pTrack == NULL || !trackInfo(pTrack, &tvi))
		pTrack = trackAt(pos, true, &tvi);
	if (pTrack == NULL)
		return;

	qtractorCurve *pCurve = pTrack->currentCurve();
	if (pCurve == NULL)
		return;
	if (pCurve->isLocked())
		return;
	if (!m_pCurveSelect->isCurrentCurve(pCurve))
		return;

	const int x0 = m_pCurveSelect->rect().x();
	const int x1 = x0 + m_iDragCurveX;
	const long delta = long(pSession->frameFromPixel(x1))
		- long(pSession->frameFromPixel(x0));

	qtractorCurveEditCommand *pCurveEditCommand
		= new qtractorCurveEditCommand(tr("move automation"), pCurve);

	// We'll build a composite command...
	QList<qtractorCurve::Node *> nodes;
	qtractorCurveEditList edits(pCurve);

	const qtractorCurveSelect::ItemList& items = m_pCurveSelect->items();
	qtractorCurveSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorCurveSelect::ItemList::ConstIterator iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorCurveSelect::Item *pItem = iter.value();
		if (pItem->flags & 1) {
			qtractorCurve::Node *pNode = iter.key();
			const unsigned long frame = pNode->frame + delta;
			const float value = pNode->value;
			pCurveEditCommand->removeNode(pNode);
			pCurve->unlinkNode(pNode);
			pNode = pCurve->addNode(frame, value, &edits);
			if (pNode)
				nodes.append(pNode);
		}
	}

	pCurveEditCommand->addEditList(&edits);

	// Put it in the form of an undoable command...
	if (pCurveEditCommand->isEmpty()) {
		delete pCurveEditCommand;
	} else {
		pSession->commands()->push(pCurveEditCommand);
		m_pTracks->dirtyChangeNotify();
	}

	// Redo selection as new...
	if (!nodes.isEmpty()) {
		QRect rectUpdate = m_pCurveSelect->rect();
		m_pCurveSelect->clear();
		const int h  = tvi.trackRect.height();
		const int y2 = tvi.trackRect.bottom() + 1;
		QListIterator<qtractorCurve::Node *> node_iter(nodes);
		while (node_iter.hasNext()) {
			qtractorCurve::Node *pNode = node_iter.next();
			const float s = pCurve->scaleFromValue(pNode->value);
			const int x = pSession->pixelFromFrame(pNode->frame);
			const int y = y2 - int(s * float(h));
			m_pCurveSelect->selectItem(pCurve, pNode,
				QRect(x - 4, y - 4, 8, 8));
		}
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}
}


// Paste from clipboard (execute).
void qtractorTrackView::pasteCurveSelect ( const QPoint& pos )
{
	// Check if there's anything really on clipboard...
	if (g_clipboard.nodes.count() < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	dragCurveMove(pos);

	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = m_pTracks->currentTrack();
	if (pTrack == NULL || !trackInfo(pTrack, &tvi))
		return;

	qtractorCurve *pCurve = pTrack->currentCurve();
	if (pCurve == NULL)
		return;
	if (pCurve->isLocked())
		return;

	// We'll need this...
	qtractorCurveEditCommand *pCurveEditCommand
		= new qtractorCurveEditCommand(tr("paste automation"), pCurve);

	unsigned long iPastePeriod = m_iPastePeriod;
	if (iPastePeriod < 1)
		iPastePeriod = g_clipboard.frames;

	long iPasteDelta = 0;
	if (m_iDragCurveX < 0)
		iPasteDelta = - pSession->frameFromPixel(- m_iDragCurveX);
	else
		iPasteDelta = + pSession->frameFromPixel(+ m_iDragCurveX);
	
	// We'll build a composite command...
	QList<qtractorCurve::Node *> nodes;
	qtractorCurveEditList edits(pCurve);

	QListIterator<NodeItem *> iter(g_clipboard.nodes);
	for (unsigned short i = 0; i < m_iPasteCount; ++i) {
		// Paste iteration...
		iter.toFront();
		while (iter.hasNext()) {
			NodeItem *pNodeItem = iter.next();
			// Convert to precise frame positioning,
			// but only the first clip gets snapped...
			unsigned long iNodeFrame = pNodeItem->frame;
			if (long(iNodeFrame) + iPasteDelta > 0)
				iNodeFrame += iPasteDelta;
			if (nodes.isEmpty()) {
				unsigned long iFrameStart = iNodeFrame;
				iNodeFrame = pSession->frameSnap(iFrameStart);
				iPasteDelta += long(iNodeFrame) - long(iFrameStart);
			}
			// Now, its imperative to make a proper copy of those nodes...
			qtractorCurve::Node *pNode
				= pCurve->addNode(iNodeFrame, pNodeItem->value, &edits);
			if (pNode)
				nodes.append(pNode);
		}
		// Set to repeat...
		iPasteDelta += iPastePeriod;
	}

	pCurveEditCommand->addEditList(&edits);

	// Put it in the form of an undoable command...
	if (pCurveEditCommand->isEmpty()) {
		delete pCurveEditCommand;
	} else {
		pSession->commands()->push(pCurveEditCommand);
		m_pTracks->dirtyChangeNotify();
	}

	// Redo selection as new...
	if (!nodes.isEmpty()) {
		QRect rectUpdate = m_pCurveSelect->rect();
		m_pCurveSelect->clear();
		const int h  = tvi.trackRect.height();
		const int y2 = tvi.trackRect.bottom() + 1;
		QListIterator<qtractorCurve::Node *> node_iter(nodes);
		while (node_iter.hasNext()) {
			qtractorCurve::Node *pNode = node_iter.next();
			const float s = pCurve->scaleFromValue(pNode->value);
			const int x = pSession->pixelFromFrame(pNode->frame);
			const int y = y2 - int(s * float(h));
			m_pCurveSelect->selectItem(pCurve, pNode,
				QRect(x - 4, y - 4, 8, 8));
		}
		m_pCurveSelect->update(true);
		updateRect(rectUpdate.united(m_pCurveSelect->rect()));
		m_pTracks->selectionChangeNotify();
	}
}


// Clip cloner helper.
qtractorClip *qtractorTrackView::cloneClip ( qtractorClip *pClip )
{
	if (pClip == NULL)
		return NULL;

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == NULL)
		return NULL;

	qtractorClip *pNewClip = NULL;
	switch (pTrack->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip
				= static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip)
				pNewClip = new qtractorAudioClip(*pAudioClip);
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip)
				pNewClip = new qtractorMidiClip(*pMidiClip);
			break;
		}
		case qtractorTrack::None:
		default:
			break;
	}

	return pNewClip;
}


// Multi-item drop mode (whether to span clips horixontally).
void qtractorTrackView::setDropSpan ( bool bDropSpan )
{
	m_bDropSpan = bDropSpan;
}

bool qtractorTrackView::isDropSpan (void) const
{
	return m_bDropSpan;
}


// Snap-to-bar zebra mode.
void qtractorTrackView::setSnapZebra ( bool bSnapZebra )
{
	m_bSnapZebra = bSnapZebra;

	updateContents();
}

bool qtractorTrackView::isSnapZebra (void) const
{
	return m_bSnapZebra;
}


// Snap-to-beat grid mode.
void qtractorTrackView::setSnapGrid ( bool bSnapGrid )
{
	m_bSnapGrid = bSnapGrid;

	updateContents();
}

bool qtractorTrackView::isSnapGrid (void) const
{
	return m_bSnapGrid;
}


// Floating tool-tips mode.
void qtractorTrackView::setToolTips ( bool bToolTips )
{
	m_bToolTips = bToolTips;
}

bool qtractorTrackView::isToolTips (void) const
{
	return m_bToolTips;
}


// Automation curve node editing mode.
void qtractorTrackView::setCurveEdit ( bool bCurveEdit )
{
	if (( m_bCurveEdit &&  bCurveEdit) ||
		(!m_bCurveEdit && !bCurveEdit))
		return;

	m_bCurveEdit = bCurveEdit;

	g_clipboard.clear();

	clearSelect();

	m_pTracks->selectionChangeNotify();
}

bool qtractorTrackView::isCurveEdit (void) const
{
	return m_bCurveEdit;
}


// Temporary sync-view/follow-playhead hold state.
void qtractorTrackView::setSyncViewHoldOn ( bool bOn )
{
	m_iSyncViewHold = (m_bSyncViewHold && bOn ? QTRACTOR_SYNC_VIEW_HOLD : 0);
}


void qtractorTrackView::setSyncViewHold ( bool bSyncViewHold )
{
	m_bSyncViewHold = bSyncViewHold;
	setSyncViewHoldOn(bSyncViewHold);
}


bool qtractorTrackView::isSyncViewHold (void) const
{
	return (m_bSyncViewHold && m_iSyncViewHold > 0);
}


// end of qtractorTrackView.cpp
