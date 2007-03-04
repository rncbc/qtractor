// qtractorTrackView.cpp
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
#include "qtractorClipCommand.h"

#include "qtractorMainForm.h"
#include "qtractorThumbView.h"

#include <QToolButton>
#include <QScrollBar>

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QKeyEvent>

#include <QApplication>
#include <QPainter>
#include <QCursor>
#include <QTimer>
#include <QUrl>


//----------------------------------------------------------------------------
// qtractorTrackView -- Track view widget.

// Constructor.
qtractorTrackView::qtractorTrackView ( qtractorTracks *pTracks,
	QWidget *pParent ) : qtractorScrollView(pParent)
{
	m_pTracks = pTracks;

	m_pClipSelect    = new qtractorClipSelect();
	m_pSessionCursor = NULL;
	m_pRubberBand    = NULL;

	m_selectMode = SelectClip;
	
	clear();

	// Zoom tool widgets
	m_pHzoomIn    = new QToolButton(this);
	m_pHzoomOut   = new QToolButton(this);
	m_pVzoomIn    = new QToolButton(this);
	m_pVzoomOut   = new QToolButton(this);
	m_pXzoomReset = new QToolButton(this);

	const QIcon& iconZoomIn = QIcon(":/icons/viewZoomIn.png");
	m_pHzoomIn->setIcon(iconZoomIn);
	m_pVzoomIn->setIcon(iconZoomIn);

	const QIcon& iconZoomOut = QIcon(":/icons/viewZoomOut.png");
	m_pHzoomOut->setIcon(iconZoomOut);
	m_pVzoomOut->setIcon(iconZoomOut);

	m_pXzoomReset->setIcon(QIcon(":/icons/viewZoomTool.png"));

	m_pHzoomIn->setAutoRepeat(true);
	m_pHzoomOut->setAutoRepeat(true);
	m_pVzoomIn->setAutoRepeat(true);
	m_pVzoomOut->setAutoRepeat(true);

	m_pHzoomIn->setToolTip(tr("Zoom in (horizontal)"));
	m_pHzoomOut->setToolTip(tr("Zoom out (horizontal)"));
	m_pVzoomIn->setToolTip(tr("Zoom in (vertical)"));
	m_pVzoomOut->setToolTip(tr("Zoom out (vertical)"));
	m_pXzoomReset->setToolTip(tr("Zoom reset"));

#if QT_VERSION >= 0x040201
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
#endif

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

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorTrackView::~qtractorTrackView (void)
{
	clear();
	clearClipboard();

	delete m_pClipSelect;
}


// Track view state reset.
void qtractorTrackView::clear (void)
{
	m_bDragTimer = false;

	m_pClipSelect->clear();

	m_dropType   = qtractorTrack::None;
	m_dragState  = DragNone;
	m_iDraggingX = 0;
	m_pClipDrag  = NULL;
	m_bDragTimer = false;

	m_iPlayHead  = 0;

	m_iPlayHeadX = 0;
	m_iEditHeadX = 0;
	m_iEditTailX = 0;

	m_iLastRecordX = 0;

	if (m_pSessionCursor)
		delete m_pSessionCursor;
	m_pSessionCursor = NULL;

	if (m_pRubberBand)
		delete m_pRubberBand;
	m_pRubberBand = NULL;
}


// Update track view content height.
void qtractorTrackView::updateContentsHeight (void)
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Allways give some room to drop something...
	int iContentsHeight = qtractorTrackList::ItemHeightBase;
	// Compute total track height...
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		iContentsHeight += pTrack->zoomHeight();
		pTrack = pTrack->next();
	}

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorTrackView::updateContentsHeight()");
		" => iContentsHeight=%d\n", iContentsHeight);
#endif

	// No selection anymore (we'll update all contents anyway)...
	m_pClipSelect->clear();

	// Do the contents resize thing...
	qtractorScrollView::resizeContents(
		qtractorScrollView::contentsWidth(), iContentsHeight);
}


// Update track view content width.
void qtractorTrackView::updateContentsWidth ( int iContentsWidth )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		if (iContentsWidth < 1) 
			iContentsWidth = pSession->pixelFromFrame(pSession->sessionLength());
		iContentsWidth += pSession->pixelFromBeat(2 * pSession->beatsPerBar());
		m_iEditHeadX = pSession->pixelFromFrame(pSession->editHead());
		m_iEditTailX = pSession->pixelFromFrame(pSession->editTail());
	}

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorTrackView::updateContentsWidth()");
		" => iContentsWidth=%d\n", iContentsWidth);
#endif

	// No selection anymore (we'll update all contents anyway)...
	m_pClipSelect->clear();

	// Do the contents resize thing...
	qtractorScrollView::resizeContents(
		iContentsWidth, qtractorScrollView::contentsHeight());

	// Force an update on the track time line too...
	m_pTracks->trackTime()->resizeContents(
		iContentsWidth + 100, m_pTracks->trackTime()->contentsHeight());
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
	int cx = qtractorScrollView::contentsX();
	int x  = cx;
	int w  = qtractorScrollView::width();

	int iCurrRecordX = m_iPlayHeadX;
	if (iCurrRecordX > x + w)
		iCurrRecordX = x + w;

	if (m_iLastRecordX < iCurrRecordX) {
		if (x < m_iLastRecordX && m_iLastRecordX < x + w)
			x = m_iLastRecordX - 2;
		w = iCurrRecordX - x + 2;
		qtractorScrollView::viewport()->update(
			QRect(x - cx, 0, w, qtractorScrollView::height()));
	}
	else if (m_iLastRecordX > iCurrRecordX)
		qtractorScrollView::viewport()->update();

	m_iLastRecordX = iCurrRecordX;
}


// Draw the track view.
void qtractorTrackView::drawContents ( QPainter *pPainter, const QRect& rect )
{
	// Draw viewport canvas...
	pPainter->drawPixmap(rect, m_pixmap, rect);

	// Lines a-head...
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int cx = qtractorScrollView::contentsX();
	int cy = qtractorScrollView::contentsY();
	int ch = qtractorScrollView::contentsHeight();
	int x;

	// On-the-fly recording clip drawing...
	if (pSession->isRecording() && pSession->isPlaying()) {
		unsigned long iTrackStart = pSession->frameFromPixel(cx + rect.x());
		unsigned long iTrackEnd   = pSession->playHead();
		if (iTrackStart < iTrackEnd) {
			int y1 = 0;
			int y2 = 0;
			qtractorTrack *pTrack = pSession->tracks().first();
			while (pTrack && y2 < ch) {
				y1  = y2;
				y2 += pTrack->zoomHeight();
				// Dispatch to paint this track...
				if (y2 > cy && pTrack->clipRecord()) {
					int h = y2 - y1 - 2;
					const QRect trackRect(
						rect.left() - 1, y1 - cy + 1, rect.width() + 2, h);
					qtractorClip *pClipRecord = pTrack->clipRecord();
					unsigned long iClipStart  = pClipRecord->clipStart();
					unsigned long iClipOffset = 0;
					if (iClipStart < iTrackStart)
						iClipOffset += iTrackStart - iClipStart;
					x = pSession->pixelFromFrame(iClipStart) - cx;
					int w = 0;
					if (iClipStart < iTrackEnd)
						w += pSession->pixelFromFrame(iTrackEnd - iClipStart);
					const QRect& clipRect
						= QRect(x, y1 - cy + 1, w, h).intersect(trackRect);
					if (!clipRect.isEmpty()) {
						// Just draw a semi-transparent rectangle...
						QColor rgbPen   = pTrack->background().dark();
						QColor rgbBrush = pTrack->background();
						rgbPen.setAlpha(120);
						rgbBrush.setAlpha(120);
						pPainter->setPen(rgbPen);
						pPainter->setBrush(rgbBrush);
						pPainter->drawRect(clipRect);
					}
				}
				pTrack = pTrack->next();
			}
		}
	}

	// Draw edit-head line...
	m_iEditHeadX = pSession->pixelFromFrame(pSession->editHead());
	x = m_iEditHeadX - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::blue);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	// Draw edit-tail line...
	m_iEditTailX = pSession->pixelFromFrame(pSession->editTail());
	x = m_iEditTailX - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::blue);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	// Draw play-head line...
	m_iPlayHeadX = pSession->pixelFromFrame(playHead());
	x = m_iPlayHeadX - cx;
	if (x >= rect.left() && x <= rect.right()) {
		pPainter->setPen(Qt::red);
		pPainter->drawLine(x, rect.top(), x, rect.bottom());
	}

	// Show/hide a moving clip fade in/out slope lines...
	if (m_dragState == DragFadeIn || m_dragState == DragFadeOut) {
		QRect rectHandle(m_rectHandle);
		// Horizontal adjust...
		rectHandle.translate(m_iDraggingX, 0);
		// Convert rectangle into view coordinates...
		rectHandle.moveTopLeft(contentsToViewport(rectHandle.topLeft()));
		// Draw envelope line...
		QPoint vpos;
		pPainter->setPen(QColor(0, 0, 255, 120));
		if (m_dragState == DragFadeIn) {
			vpos = contentsToViewport(m_rectDrag.bottomLeft());
			pPainter->drawLine(
				vpos.x(), vpos.y(), rectHandle.left(), rectHandle.top());
		} 
		else 
		if (m_dragState == DragFadeOut) {
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

#if QT_VERSION >= 0x040201
	// Corner tool widget layout management...
	if (m_pXzoomReset) {
		const QSize& size = qtractorScrollView::size();
		int h = size.height();
		int w = qtractorScrollView::style()->pixelMetric(
			QStyle::PM_ScrollBarExtent);
		int x = size.width() - w - 2;
		m_pXzoomReset->setGeometry(x, h - w - 2, w, w);
	}
#else
	// Scrollbar/tools layout management.
	const QSize& size = qtractorScrollView::size();
	QScrollBar *pVScrollBar = qtractorScrollView::verticalScrollBar();
	if (pVScrollBar->isVisible()) {
		int h = size.height();
		int w = pVScrollBar->width(); 
		int x = size.width() - w - 1;
		pVScrollBar->setFixedHeight(h - w * 3 - 2);
		if (m_pVzoomIn)
			m_pVzoomIn->setGeometry(x, h - w * 3, w, w);
		if (m_pVzoomOut)
			m_pVzoomOut->setGeometry(x, h - w * 2, w, w);
		if (m_pXzoomReset)
			m_pXzoomReset->setGeometry(x, h - w - 1, w, w);
	}
	QScrollBar *pHScrollBar = qtractorScrollView::horizontalScrollBar();
	if (pHScrollBar->isVisible()) {
		int w = size.width();
		int h = pHScrollBar->height(); 
		int y = size.height() - h - 1;
		pHScrollBar->setFixedWidth(w - h * 3 - 2);
		if (m_pHzoomOut)
			m_pHzoomOut->setGeometry(w - h * 3, y, h, h);
		if (m_pHzoomIn)
			m_pHzoomIn->setGeometry(w - h * 2, y, h, h);
	}
#endif

	updateContents();

	// HACK: let our (single) thumb view get notified...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->thumbView())
		pMainForm->thumbView()->updateThumb();
}


// (Re)create the complete track view pixmap.
void qtractorTrackView::updatePixmap ( int cx, int cy )
{
	const QPalette& pal = qtractorScrollView::palette();

	int w = qtractorScrollView::width();
	int h = qtractorScrollView::height();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.dark().color());

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	QPainter painter(&m_pixmap);
	painter.initFrom(this);

	// Update view session cursor location,
	// so that we'll start drawing clips from there...
	unsigned long iTrackStart = pSession->frameFromPixel(cx);
	unsigned long iTrackEnd   = iTrackStart + pSession->frameFromPixel(w);
	// Create cursor now if applicable...
	if (m_pSessionCursor == NULL) {
		m_pSessionCursor = pSession->createSessionCursor(iTrackStart);
	} else {
		m_pSessionCursor->seek(iTrackStart);
	}

	const QColor& rgbLight = pal.midlight().color().dark(120);
	const QColor& rgbMid   = pal.mid().color();
	const QColor& rgbDark  = rgbMid.dark(120);
	
	// Draw track and horizontal lines...
	int x, y1, y2;
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
			QRect trackRect(0, y1 - cy + 1, w, y2 - y1 - 2);
			painter.fillRect(trackRect, rgbMid);
			pTrack->drawTrack(&painter, trackRect, iTrackStart, iTrackEnd,
				m_pSessionCursor->clip(iTrack));
			painter.setPen(rgbDark);
			painter.drawLine(0, y2 - cy - 1, w, y2 - cy - 1);
		}
		pTrack = pTrack->next();
		iTrack++;
	}

	// Draw vertical grid lines...
	if (cy < y2) {
		unsigned short iBeat = pSession->beatFromPixel(cx);
		unsigned long iFrameFromBeat = pSession->frameFromBeat(iBeat);
		unsigned long iFramesPerBeat = pSession->frameFromBeat(1);
		unsigned int  iPixelsPerBeat = pSession->pixelFromBeat(1);
		x = pSession->pixelFromFrame(iFrameFromBeat) - cx;
		while (x < w) {
			if (x >= 0) {
				bool bBeatIsBar = pSession->beatIsBar(iBeat);
				if (bBeatIsBar) {
					painter.setPen(rgbLight);
					painter.drawLine(x, 0, x, y2 - cy - 2);
				}
				if (bBeatIsBar || iPixelsPerBeat > 16) {
					painter.setPen(rgbDark);
					painter.drawLine(x - 1, 0, x - 1, y2 - cy - 2);
				}
			}
			iFrameFromBeat += iFramesPerBeat;
			x = pSession->pixelFromFrame(iFrameFromBeat) - cx;
			iBeat++;
		}
	}

	// Fill the empty area...
	if (y2 < cy + h) {
		painter.setPen(rgbMid);
		painter.drawLine(0, y2 - cy, w, y2 - cy);
	//	painter.fillRect(0, y2 - cy + 1, w, h, pal.dark().color());
	}

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		painter.setPen(Qt::darkCyan);
		x = pSession->pixelFromFrame(pSession->loopStart()) - cx;
		if (x >= 0 && x < w)
			painter.drawLine(x, 0, x, h);
		x = pSession->pixelFromFrame(pSession->loopEnd()) - cx;
		if (x >= 0 && x < w)
			painter.drawLine(x, 0, x, h);
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
	qtractorTrackViewInfo *pTrackViewInfo ) const
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL || m_pSessionCursor == NULL)
		return NULL;

	int y1 = 0;
	int y2 = 0;
	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (y2 > pos.y()) {
			m_pTracks->trackList()->selectTrack(iTrack);
			break;
		}
		pTrack = pTrack->next();
		iTrack++;
	}

	if (pTrackViewInfo) {
		int x = qtractorScrollView::contentsX();
		int w = qtractorScrollView::width();// View width, not contents.
		if (pTrack == NULL) {				// Below all tracks.
			y1 = y2;
			y2 = y1 + (48 * pSession->verticalZoom()) / 100;
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
qtractorClip *qtractorTrackView::clipAt ( const QPoint& pos,
	QRect *pClipRect ) const
{
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, &tvi);
	if (pTrack == NULL)
		return NULL;

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return NULL;

	if (m_pSessionCursor == NULL)
		return NULL;

	qtractorClip *pClip = m_pSessionCursor->clip(tvi.trackIndex);
	if (pClip == NULL)
		pClip = pTrack->clips().first();
	if (pClip == NULL)
		return NULL;

	qtractorClip *pClipAt = NULL;
	while (pClip && pClip->clipStart() < tvi.trackEnd) {
		int x = pSession->pixelFromFrame(pClip->clipStart());
		int w = pSession->pixelFromFrame(pClip->clipLength());
		if (pos.x() > x && pos.x() < x + w) {
			pClipAt = pClip;
			if (pClipRect)
				pClipRect->setRect(
					x, tvi.trackRect.y(), w, tvi.trackRect.height());
		}
		pClip = pClip->next();
	}
	return pClipAt;
}


// Get contents visible rectangle from given track.
bool qtractorTrackView::trackInfo ( qtractorTrack *pTrackPtr,
	qtractorTrackViewInfo *pTrackViewInfo ) const
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL || m_pSessionCursor == NULL)
		return false;

	int y1, y2 = 0;
	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pTrack == pTrackPtr) {
			int x = qtractorScrollView::contentsX();
			int w = qtractorScrollView::width();   	// View width, not contents.
			pTrackViewInfo->trackIndex = iTrack;
			pTrackViewInfo->trackStart = m_pSessionCursor->frame();
			pTrackViewInfo->trackEnd   = pTrackViewInfo->trackStart
				+ m_pTracks->session()->frameFromPixel(w);
			pTrackViewInfo->trackRect.setRect(x, y1 + 1, w, y2 - y1 - 2);
			return true;
		}
		pTrack = pTrack->next();
		iTrack++;
	}

	return false;
}


// Get contents rectangle from given clip.
bool qtractorTrackView::clipInfo ( qtractorClip *pClip,
	QRect *pClipRect ) const
{
	qtractorTrackViewInfo tvi;
	if (!trackInfo(pClip->track(), &tvi))
		return false;

	int x = m_pTracks->session()->pixelFromFrame(pClip->clipStart());
	int w = m_pTracks->session()->pixelFromFrame(pClip->clipLength());
	pClipRect->setRect(x, tvi.trackRect.y(), w, tvi.trackRect.height());

	return true;
}


// Selection-dragging, following the current mouse position.
qtractorTrack *qtractorTrackView::dragMoveTrack ( const QPoint& pos )
{
	// Which track we're pointing at?
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, &tvi);
	if (pTrack == NULL)
	    return NULL;

	// May change vertically, if we've only one track selected,
	// and only between same track type...
	qtractorTrack *pSingleTrack = m_pClipSelect->singleTrack();
	if (pSingleTrack && pSingleTrack->trackType() == pTrack->trackType())
		updateClipSelect(tvi.trackRect.y() + 1, tvi.trackRect.height() - 2);

	// Always change horizontally wise...
	int  x = m_pClipSelect->rect().x();
	int dx = (pos.x() - m_posDrag.x());
	if (x + dx < 0)
		dx = -(x);	// Force to origin (x=0).
	m_iDraggingX = (m_pTracks->session()->pixelSnap(x + dx) - x);
	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);

	showClipSelect();

	// OK, we've moved it...
	return pTrack;
}


qtractorTrack *qtractorTrackView::dragDropTrack ( QDropEvent *pDropEvent )
{
	// It must be a valid session...
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return NULL;
	
	// If we're already dragging something,
	// find the current pointer track...
	const QPoint& pos = viewportToContents(pDropEvent->pos());
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, &tvi);
	if (!m_dropItems.isEmpty()) {
		// Adjust vertically to target track...
		updateDropRects(tvi.trackRect.y() + 1, tvi.trackRect.height() - 2);
		// Always change horizontally wise...
		int  x = m_rectDrag.x();
		int dx = (pos.x() - m_posDrag.x());
		if (x + dx < 0)
			dx = -(x);	// Force to origin (x=0).
		m_iDraggingX = (pSession->pixelSnap(x + dx) - x);
	//	showDropRects();
		// OK, we've moved it...
		return pTrack;
	}

	// Let's start from scratch...
	qDeleteAll(m_dropItems);
	m_dropItems.clear();
	m_dropType = qtractorTrack::None;

	// Can it be single track channel (MIDI for sure)?
	if (qtractorFileChannelDrag::canDecode(pDropEvent->mimeData())) {
		// In the meantime, it can only be only one...
		QString sPath;
		unsigned short iChannel = 0;
		if (qtractorFileChannelDrag::decode(
				pDropEvent->mimeData(), sPath, &iChannel)) {
			m_dropItems.append(new DropItem(sPath, iChannel));
			m_dropType = qtractorTrack::Midi;
		}
	}
	else
	// Can we decode it as Audio/MIDI files?
	if (pDropEvent->mimeData()->hasUrls()) {
		// Let's see how many files there are...
		QList<QUrl> list = pDropEvent->mimeData()->urls();
		QListIterator<QUrl> iter(list);
		while (iter.hasNext()) {
			const QString& sPath = iter.next().toLocalFile();
			// Close current session and try to load the new one...
			if (!sPath.isEmpty())
				m_dropItems.append(new DropItem(sPath));
		}
	}

	// Nice, now we'll try to set a preview selection rectangle set...
	m_posDrag.setX(pSession->pixelSnap(pos.x() - 8));
	m_posDrag.setY(tvi.trackRect.y() + 1);
	m_rectDrag.setRect(
		m_posDrag.x(), m_posDrag.y(), 0, tvi.trackRect.height() - 2);

	// Nows time to add those rectangles...
	QMutableListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		DropItem *pDropItem = iter.next();
		// First test as a MIDI file...
		if (m_dropType == qtractorTrack::None
			|| m_dropType == qtractorTrack::Midi) {
			qtractorMidiFile file;
			if (file.open(pDropItem->path)) {
				qtractorMidiSequence seq;
				seq.setTicksPerBeat(pSession->ticksPerBeat());
				if (pDropItem->channel < 0) {
					int iTracks = (file.format() == 1 ? file.tracks() : 16);
					for (int iTrackChannel = 0;
						iTrackChannel < iTracks; iTrackChannel++) {
						if (file.readTrack(&seq, iTrackChannel)
							&& seq.duration() > 0) {
							m_rectDrag.setWidth(
								pSession->pixelFromTick(seq.duration()));
							pDropItem->rect = m_rectDrag;
							m_rectDrag.translate(0, m_rectDrag.height() + 4);
						}
					}
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Midi;
				} else if (file.readTrack(&seq, pDropItem->channel)
					&& seq.duration() > 0) {
					m_rectDrag.setWidth(
						pSession->pixelFromTick(seq.duration()));
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

	// Ok, sure we're into some drag state...
	m_dragState = DragDrop;
	m_iDraggingX = 0;	

	// Finally, show it to the world...
//	showDropRects();

	// Done.
	return pTrack;
}


bool qtractorTrackView::canDropTrack ( QDropEvent *pDropEvent )
{
	qtractorTrack *pTrack = dragDropTrack(pDropEvent);
	return ((pTrack	&& pTrack->trackType() == m_dropType
		&& m_dropItems.count() == 1)
			|| (pTrack == NULL && !m_dropItems.isEmpty()));
}


// Drag enter event handler.
void qtractorTrackView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
#if 0
	if (canDropTrack(pDragEnterEvent)) {
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
void qtractorTrackView::dragMoveEvent (	QDragMoveEvent *pDragMoveEvent )
{
	if (canDropTrack(pDragMoveEvent)) {
		showDropRects();
		if (!pDragMoveEvent->isAccepted()) {
			pDragMoveEvent->setDropAction(Qt::CopyAction);
			pDragMoveEvent->accept();
			m_bDragTimer = false;
		}
	} else {
		pDragMoveEvent->ignore();
		hideDropRects();
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
void qtractorTrackView::dropEvent (	QDropEvent *pDropEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Add new clips on proper and consecutive track locations...
	unsigned long iClipStart = pSession->frameSnap(
		pSession->frameFromPixel(m_rectDrag.x() + m_iDraggingX));

	// Now check whether the drop is intra-track...
	qtractorTrack *pTrack = dragDropTrack(pDropEvent);
	if (pTrack == NULL) {
		// Do we have something to drop anyway?
		// if yes, this is a extra-track drop...
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
				} else  {
					files.append(pDropItem->path);
				}
			}
			// Depending on import type...
			if (!files.isEmpty()) {
				switch (m_dropType) {
				case qtractorTrack::Audio:
					m_pTracks->addAudioTracks(files, iClipStart);
					break;
				case qtractorTrack::Midi:
					m_pTracks->addMidiTracks(files, iClipStart);
					break;
				default:
					break;
				}
			}
		}
		resetDragState();
		return;
	}

	// Check whether we can really drop it.
	if (pTrack->trackType() != m_dropType) {
		resetDragState();
		return;
	}

	// We'll build a composite command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(pMainForm, tr("add clip"));

	// Nows time to create the clip(s)...
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
				// Don't forget to add this one to local repository.
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
				// Don't forget to add this one to local repository.
				pMainForm->addMidiFile(pDropItem->path);
			}
			break;
		}
		case qtractorTrack::None:
		default:
			break;
		}
		// If multiple items, just concatenate them...
		iClipStart += pSession->frameFromPixel(pDropItem->rect.width());
	}

	// Clean up.
	resetDragState();

	// Put it in the form of an undoable command...
	pMainForm->commands()->exec(pClipCommand);
}


// Handle item selection/dragging -- mouse button press.
void qtractorTrackView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	resetDragState();

	// Which mouse state?
	const bool bModifier = (pMouseEvent->modifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier));
	const QPoint& pos = viewportToContents(pMouseEvent->pos());

	// We need a session and a location...
	qtractorSession *pSession = m_pTracks->session();
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
			m_pClipDrag = clipAt(m_posDrag, &m_rectDrag);
			// Should it be selected(toggled)?
			if (m_pClipDrag) {
				// Show that we're about to something...
				qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
				// Make it (un)selected, right on the file view too...
				if (m_selectMode == SelectClip)
					selectClipFile(!bModifier);
			}
			// Something got it started?...
			if (m_pClipDrag == NULL
				|| (m_pClipDrag && !m_pClipDrag->isClipSelected())) {
				// Clear any selection out there?
				if (!bModifier /* || m_selectMode != SelectClip */)
					selectAll(false);
			}
			break;
		case Qt::MidButton:
			// Mid-button positioning...
			selectAll(false);
			// Edit cursor positioning...
			setEditHead(iFrame);
			setEditTail(iFrame);
			// Not quite a selection, but some visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case Qt::RightButton:
			// Have no sense if pointer falls over a clip...
			if (!clipAt(pos)) {
				// Right-button edit-tail positioning...
				setEditTail(iFrame);
				// Not quite a selection, but some visual feedback...
				m_pTracks->selectionChangeNotify();
			}
			// Fall thru...
		default:
			break;
		}
	}

	qtractorScrollView::mousePressEvent(pMouseEvent);

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorTrackView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Are we already moving/dragging something?
	const QPoint& pos = viewportToContents(pMouseEvent->pos());

	switch (m_dragState) {
	case DragMove:
		dragMoveTrack(pos);
		break;
	case DragFadeIn:
	case DragFadeOut:
		dragFadeMove(pos);
		break;
	case DragSelect:
		m_rectDrag.setBottomRight(pos);
		moveRubberBand(&m_pRubberBand, m_rectDrag);
		qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);
		selectRect(m_rectDrag, m_selectMode);
		break;
	case DragStart:
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// Check if we're pointing in some fade-in/out handle...
			if (dragFadeStart(m_posDrag)) {
				m_iDraggingX = (pos.x() - m_posDrag.x());
				moveRubberBand(&m_pRubberBand, m_rectHandle);
			} else {
				// We'll start dragging clip/regions alright...
				qtractorClipSelect::Item *pClipItem = NULL;
				if (m_pClipDrag)
					pClipItem = m_pClipSelect->findClipItem(m_pClipDrag);
				if (pClipItem && pClipItem->rectClip.contains(pos)) {
					int x = m_pTracks->session()->pixelSnap(m_rectDrag.x());
					m_iDraggingX = (x - m_rectDrag.x());
					m_dragState = DragMove;
					qtractorScrollView::setCursor(QCursor(Qt::SizeAllCursor));
					showClipSelect();
				} else {
					// We'll start rubber banding...
					m_rectDrag.setTopLeft(m_posDrag);
					m_rectDrag.setBottomRight(pos);
					m_dragState = DragSelect;
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
	case DragDrop:
	case DragNone:
	default:
		break;
	}

	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorTrackView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		// Direct snap positioning...
		const QPoint& pos = viewportToContents(pMouseEvent->pos());
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(m_posDrag.x() > 0 ? m_posDrag.x() : 0));
		// Which mouse state?
		const bool bModifier = (pMouseEvent->modifiers()
			& (Qt::ShiftModifier | Qt::ControlModifier));
		switch (m_dragState) {
		case DragSelect:
			// Here we're mainly supposed to select a few bunch
			// of clips (all that fall inside the rubber-band...
			selectRect(m_rectDrag, m_selectMode);
			// For immediate visual feedback...
			m_pTracks->selectionChangeNotify();
			break;
		case DragMove:
			// Let's move them...
			moveClipSelect(dragMoveTrack(pos), m_iDraggingX);
			break;
		case DragFadeIn:
		case DragFadeOut:
			dragFadeDrop(pos);
			break;
		case DragStart:
			// Deferred left-button positioning...
			if (m_pClipDrag) {
				// Make it right on the file view now...
				if (m_selectMode != SelectClip)
					selectClipFile(!bModifier);
				// Nothing more has been deferred...
			} else {
				// Direct play-head positioning...
				if (bModifier) {
					// First, set actual engine position...
					pSession->setPlayHead(iFrame);
					// Play-head positioning...
					setPlayHead(iFrame);
					// Done with (deferred) play-head positioning.
				} else {
					// Deferred left-button edit-head positioning...
					setEditHead(iFrame);
				}
				// Not quite a selection, but for
				// immediate visual feedback...
				m_pTracks->selectionChangeNotify();
			}
			// Fall thru...
		case DragDrop:
		case DragNone:
		default:
			break;
		}
	}

	// Force null state.
	resetDragState();

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Clip file(item) selection convenience method.
void qtractorTrackView::selectClipFile ( bool bReset )
{
	if (m_pClipDrag == NULL)
		return;

	// Do the selection dance, first...
	qtractorClipSelect::Item *pClipItem = m_pClipSelect->findClipItem(m_pClipDrag);
	bool bSelect = !(pClipItem && pClipItem->rectClip.contains(m_posDrag));
	if (!bReset) {
		m_pClipSelect->selectClip(m_pClipDrag, m_rectDrag, bSelect);
		updateContents(m_rectDrag);
		m_pTracks->selectionChangeNotify();
	} else if (bSelect || m_selectMode != SelectClip) {
		m_pClipSelect->clear();
		if (bSelect)
			m_pClipSelect->selectClip(m_pClipDrag, m_rectDrag, true);
		updateContents();
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


// Select everything under a given (rubber-band) rectangle.
void qtractorTrackView::selectRect ( const QRect& rectDrag,
	qtractorTrackView::SelectMode selectMode,
	qtractorTrackView::SelectEdit selectEdit )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	QRect rect(rectDrag.normalized());

	// The precise (snapped) selection frame points...
	unsigned long iSelectStart
		= pSession->frameSnap(pSession->frameFromPixel(rect.left()));
	unsigned long iSelectEnd
		= pSession->frameSnap(pSession->frameFromPixel(rect.right()));

	// Special whole-vertical range case...
	QRect rectRange(0, 0, 0, qtractorScrollView::contentsHeight());
	rectRange.setLeft(pSession->pixelFromFrame(iSelectStart));
	rectRange.setRight(pSession->pixelFromFrame(iSelectEnd));
	if (selectMode == SelectRange) {
		rect.setTop(rectRange.top());
		rect.setBottom(rectRange.height());
	}

	// Let's start invalidading things...
	QRect rectUpdate = m_pClipSelect->rect();
	// Reset all selected clips...
	m_pClipSelect->clear();

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
			int y = y1 + 1;
			int h = y2 - y1 - 2;
			for (qtractorClip *pClip = pTrack->clips().first();
					pClip; pClip = pClip->next()) {
				int x = pSession->pixelFromFrame(pClip->clipStart());
				int w = pSession->pixelFromFrame(pClip->clipLength());
				if (x > rect.right())
					break;
				// Test whether the whole clip rectangle
				// intersects the rubber-band range one...
				QRect rectClip(x, y, w, h);
				if (rect.intersects(rectClip)) {
					if (selectMode != SelectClip)
						rectClip = rectRange.intersect(rectClip);
					m_pClipSelect->selectClip(pClip, rectClip, true);
					if (selectMode != SelectClip)
						pClip->setClipSelect(iSelectStart, iSelectEnd);
					rectUpdate = rectUpdate.unite(rectClip);
				}
			}
		}
		pTrack = pTrack->next();
	}

	// Update the screen real estate...
	if (!rectUpdate.isEmpty())
		updateContents(rectUpdate);

	// That's all very nice but we'll also set edit-range positioning...
	if (selectEdit & EditHead)
		setEditHead(iSelectStart);
	if (selectEdit & EditTail)
		setEditTail(iSelectEnd);
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


// Select every clip of a given track.
void qtractorTrackView::selectTrack ( qtractorTrack *pTrackPtr, bool bReset )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int iUpdate = 0;
	if (bReset) {
		m_pClipSelect->clear();
		iUpdate++;
	}

	QRect rectUpdate;
	int y1, y2 = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		y1  = y2;
		y2 += pTrack->zoomHeight();
		if (pTrack == pTrackPtr) {
			int y = y1 + 1;
			int h = y2 - y1 - 2;
			for (qtractorClip *pClip = pTrack->clips().first();
					pClip; pClip = pClip->next()) {
				int x = pSession->pixelFromFrame(pClip->clipStart());
				int w = pSession->pixelFromFrame(pClip->clipLength());
				const bool bSelect = (bReset || !pClip->isClipSelected());
				const QRect rectClip(x, y, w, h);
				m_pClipSelect->selectClip(pClip, rectClip, bSelect);
				rectUpdate = rectUpdate.unite(rectClip);
				iUpdate++;
			}
			break;
		}
		pTrack = pTrack->next();
	}

	if (iUpdate > 0) {
		if (bReset || rectUpdate.isEmpty())
			updateContents();
		else
			updateContents(rectUpdate);
		m_pTracks->selectionChangeNotify();
	}
}


// Select all contents (or not).
void qtractorTrackView::selectAll ( bool bSelect )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	if (bSelect) {
		// Select all clips on all tracks...
		int iUpdate = 0;
		int y1, y2 = 0;
		qtractorTrack *pTrack = pSession->tracks().first();
		while (pTrack) {
			y1  = y2;
			y2 += pTrack->zoomHeight();
			int y = y1 + 1;
			int h = y2 - y1 - 2;
			for (qtractorClip *pClip = pTrack->clips().first();
					pClip; pClip = pClip->next()) {
				int x = pSession->pixelFromFrame(pClip->clipStart());
				int w = pSession->pixelFromFrame(pClip->clipLength());
				const QRect rectClip(x, y, w, h);
				m_pClipSelect->selectClip(pClip, rectClip, bSelect);
				iUpdate++;
			}
			pTrack = pTrack->next();
		}
		// This is most probably an overall update...
		if (iUpdate > 0) {
			updateContents(m_pClipSelect->rect());
			m_pTracks->selectionChangeNotify();
		}
	}
	else
	// Clear all selections...
	if (m_pClipSelect->items().count() > 0) {
		QRect rectUpdate = m_pClipSelect->rect();
		m_pClipSelect->clear();
		if (!rectUpdate.isEmpty())
			updateContents(rectUpdate);
		m_pTracks->selectionChangeNotify();
	}
}


// Draw/hide the whole current clip selection.
void qtractorTrackView::updateClipSelect ( int y, int h ) const
{
	bool bSingleTrack = (m_pClipSelect->singleTrack() != NULL);
	QListIterator<qtractorClipSelect::Item *> iter(m_pClipSelect->items());
	while (iter.hasNext()) {
		qtractorClipSelect::Item *pClipItem	= iter.next();
		if (bSingleTrack) {
			pClipItem->rectClip.setY(y);
			pClipItem->rectClip.setHeight(h);
		}
	}
}

void qtractorTrackView::showClipSelect (void) const
{
	QListIterator<qtractorClipSelect::Item *> iter(m_pClipSelect->items());
	while (iter.hasNext()) {
		qtractorClipSelect::Item *pClipItem = iter.next();
		moveRubberBand(&(pClipItem->rubberBand), pClipItem->rectClip, 3);
	}
}

void qtractorTrackView::hideClipSelect (void) const
{
	QListIterator<qtractorClipSelect::Item *> iter(m_pClipSelect->items());
	while (iter.hasNext()) {
		qtractorRubberBand *pRubberBand = iter.next()->rubberBand;
		if (pRubberBand && pRubberBand->isVisible())
			pRubberBand->hide();
	}
}


// Draw/hide the whole drop rectagle list
void qtractorTrackView::updateDropRects ( int y, int h ) const
{
	QListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		DropItem *pDropItem = iter.next();
		pDropItem->rect.setY(y);
		pDropItem->rect.setHeight(h);
		y += h + 4;
	}
}

void qtractorTrackView::showDropRects (void) const
{
	QListIterator<DropItem *> iter(m_dropItems);
	while (iter.hasNext()) {
		DropItem *pDropItem = iter.next();
		moveRubberBand(&(pDropItem->rubberBand), pDropItem->rect, 3);
	}
}

void qtractorTrackView::hideDropRects (void) const
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
	rect.translate(m_iDraggingX, 0);
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


// Check whether we're up to drag a clip fade-in/out handle.
bool qtractorTrackView::dragFadeStart ( const QPoint& pos )
{
	if (m_pClipDrag == NULL)
		return false;

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return false;

	// Fade-in handle check...
	m_rectHandle.setRect(m_rectDrag.left() + 1
		+ pSession->pixelFromFrame(m_pClipDrag->fadeInLength()),
			m_rectDrag.top() + 1, 8, 8);
	if (m_rectHandle.contains(pos)) {
		m_dragState = DragFadeIn;
		return true;
	}
	
	// Fade-out handle check...
	m_rectHandle.setRect(m_rectDrag.right() - 8
		- pSession->pixelFromFrame(m_pClipDrag->fadeOutLength()),
			m_rectDrag.top() + 1, 8, 8);
	if (m_rectHandle.contains(pos)) {
		m_dragState = DragFadeOut;
		return true;
	}

	return false;
}


// Clip fade-in/out handle drag-moving parts.
void qtractorTrackView::dragFadeMove ( const QPoint& pos )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Always change horizontally wise...
	int dx = (pos.x() - m_posDrag.x());
	if (m_rectHandle.left() + dx < m_rectDrag.left())
		dx = m_rectDrag.left() - m_rectHandle.left();
	else if (m_rectHandle.right() + dx > m_rectDrag.right())
		dx = m_rectDrag.right() - m_rectHandle.right();
	m_iDraggingX = dx;
	moveRubberBand(&m_pRubberBand, m_rectHandle);
	qtractorScrollView::ensureVisible(pos.x(), pos.y(), 24, 24);

	// Prepare to update the whole view area...
	qtractorScrollView::viewport()->update(
		QRect(contentsToViewport(m_rectDrag.topLeft()), m_rectDrag.size()));
}


// Clip fade-in/out handle settler.
void qtractorTrackView::dragFadeDrop ( const QPoint& pos )
{
	if (m_pClipDrag == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	dragFadeMove(pos);

	// We'll build a command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(pMainForm, tr("clip %1").arg(
			m_dragState == DragFadeIn ? tr("fade-in") : tr("fade-out")));

	if (m_dragState == DragFadeIn) {
		pClipCommand->fadeInClip(m_pClipDrag,
				pSession->frameFromPixel(m_rectHandle.left()
					+ m_iDraggingX - m_rectDrag.left()));
	} 
	else
	if (m_dragState == DragFadeOut) {
		pClipCommand->fadeOutClip(m_pClipDrag,
				pSession->frameFromPixel(m_rectDrag.right()
					- m_iDraggingX - m_rectHandle.right()));
	}

	// Reset state for proper redrawing...
	qtractorScrollView::unsetCursor();
	m_dragState = DragNone;

	// Put it in the form of an undoable command...
	pMainForm->commands()->exec(pClipCommand);
}		


// Reset drag/select/move state.
void qtractorTrackView::resetDragState (void)
{
	// To remember what we were doing...
	DragState dragState = m_dragState;

	// Force null state, now.
	m_dragState  = DragNone;
	m_iDraggingX = 0;
	m_pClipDrag  = NULL;

	// If we were moving clips around,
	// just hide selection, of course.
	hideClipSelect();

	// Just hide the rubber-band...
	if (m_pRubberBand)
		m_pRubberBand->hide();

	// If we were dragging fade-slope lines, refresh...
	if (dragState == DragFadeIn || dragState == DragFadeOut) {
		qtractorScrollView::viewport()->update(
			QRect(contentsToViewport(m_rectDrag.topLeft()), m_rectDrag.size()));
	}

	// Should fallback mouse cursor...
	if (dragState != DragNone)
		qtractorScrollView::unsetCursor();

	// No dropping files, whatsoever.
	qDeleteAll(m_dropItems);
	m_dropItems.clear();
	m_dropType = qtractorTrack::None;
}


// Keyboard event handler.
void qtractorTrackView::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackView::keyPressEvent(key=%d)\n", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
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
		} else {
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
		} else {
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
		} else {
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
		} else {
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

	// Make sure we've get focus back...
	qtractorScrollView::setFocus();
}


// Make given frame position visible in view.
void qtractorTrackView::ensureVisibleFrame ( unsigned long iFrame )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		int x0 = qtractorScrollView::contentsX();
		int y  = qtractorScrollView::contentsY();
		int x  = pSession->pixelFromFrame(iFrame);
		int w  = m_pixmap.width();
		int w3 = w - (w >> 3);
		if (x < x0)
			x -= w3;
		else if (x > x0 + w3)
			x += w3;
		qtractorScrollView::ensureVisible(x, y, 0, 0);
		qtractorScrollView::setFocus();
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
	int cx = qtractorScrollView::contentsX();
	int x1 = iPositionX - cx;
	int w  = qtractorScrollView::width();
	int h  = qtractorScrollView::height();
	int wm = (w >> 3);

	// Time-line header extents...
	int h2 = m_pTracks->trackTime()->height();
	int d2 = (h2 >> 1);

	// Restore old position...
	if (iPositionX != x && x1 >= 0 && x1 < w) {
		// Override old view line...
		qtractorScrollView::viewport()->update(QRect(x1, 0, 1, h));
		m_pTracks->trackTime()->viewport()->update(QRect(x1 - d2, d2, h2, d2));
	}

	// New position is in...
	iPositionX = x;

	// Force position to be in view?
	if (bSyncView && (x < cx || x > cx + w - wm)) {
		// Move it...
		qtractorScrollView::setContentsPos(x - wm, qtractorScrollView::contentsY());
	} else if (bSyncView && cx > qtractorScrollView::contentsWidth() - w) {
		 // Maybe we'll need some head-room...
		updateContentsWidth(cx + w);
	} else {
		// Draw the line, by updating the new region...
		x1 = x - cx;
		if (x1 >= 0 && x1 < w)
			qtractorScrollView::viewport()->update(QRect(x1, 0, 1, h));
		// Update the time-line header...
		m_pTracks->trackTime()->viewport()->update(QRect(x1 - d2, d2, h2, d2));
	}
}


// Playhead positioning.
void qtractorTrackView::setPlayHead ( unsigned long iPlayHead, bool bSyncView )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		m_iPlayHead = iPlayHead;
		int iPlayHeadX = pSession->pixelFromFrame(iPlayHead);
		drawPositionX(m_iPlayHeadX, iPlayHeadX, bSyncView);
	}
}

unsigned long qtractorTrackView::playHead (void) const
{
	return m_iPlayHead;
}


// Edit-head positioning
void qtractorTrackView::setEditHead ( unsigned long iEditHead )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		if (iEditHead > pSession->editTail())
			setEditTail(iEditHead);
		pSession->setEditHead(iEditHead);
		int iEditHeadX = pSession->pixelFromFrame(iEditHead);
		drawPositionX(m_iEditHeadX, iEditHeadX);
	}
}


// Edit-tail positioning
void qtractorTrackView::setEditTail ( unsigned long iEditTail )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		if (iEditTail < pSession->editHead())
			setEditHead(iEditTail);
		pSession->setEditTail(iEditTail);
		int iEditTailX = pSession->pixelFromFrame(iEditTail);
		drawPositionX(m_iEditTailX, iEditTailX);
	}
}


// Whether there's any clip currently selected.
bool qtractorTrackView::isClipSelected (void) const
{
	return (m_pClipSelect->items().count() > 0);
}


// Whether there's any clip on clipboard.
bool qtractorTrackView::isClipboardEmpty (void) const
{
	return (m_clipboard.items.count() < 1);
}


// Clear clipboard.
void qtractorTrackView::clearClipboard (void)
{
	m_clipboard.clear();
}


// Clipboard stuffer method.
void qtractorTrackView::ClipBoard::addItem ( qtractorClip *pClip,
	unsigned long iClipStart, unsigned long iClipOffset,
	unsigned long iClipLength )
{
	ClipItem *pClipItem
		= new ClipItem(pClip, iClipStart, iClipOffset, iClipLength);
	if (iClipOffset == pClip->clipOffset())
		pClipItem->fadeInLength = pClip->fadeInLength();
	if (iClipOffset + iClipLength == pClip->clipOffset() + pClip->clipLength())
		pClipItem->fadeOutLength = pClip->fadeOutLength();
	items.append(pClipItem);
}


// Clipboard reset method.
void qtractorTrackView::ClipBoard::clear (void)
{
	qDeleteAll(items);
	items.clear();

	singleTrack = NULL;
}


// Clip selection executive method.
void qtractorTrackView::executeClipSelect ( qtractorTrackView::Command cmd )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Check if anything is really selected...
	if (m_pClipSelect->items().count() < 1)
		return;

	// Reset clipboard...
	bool bClipboard = (cmd == Cut || cmd == Copy);
	if (bClipboard) {
		m_clipboard.clear();
		m_clipboard.singleTrack = m_pClipSelect->singleTrack();
		m_clipboard.rect = m_pClipSelect->rect();
	}

	// We'll build a composite command...
	qtractorClipCommand *pClipCommand = NULL;
	if (cmd == Cut || cmd == Delete) {
		pClipCommand = new qtractorClipCommand(pMainForm,
			tr("%1 clip").arg(cmd == Cut ? tr("cut") : tr("delete")));
	}

	QListIterator<qtractorClipSelect::Item *> iter(m_pClipSelect->items());
	while (iter.hasNext()) {
		qtractorClipSelect::Item *pClipItem	= iter.next();
		qtractorClip  *pClip  = pClipItem->clip;
		qtractorTrack *pTrack = pClip->track();
		// Clip parameters.
		unsigned long iClipStart    = pClip->clipStart();
		unsigned long iClipOffset   = pClip->clipOffset();
		unsigned long iClipLength   = pClip->clipLength();
		unsigned long iClipEnd      = iClipStart + iClipLength;
		// Clip selection points.
		unsigned long iSelectStart  = pClip->clipSelectStart();
		unsigned long iSelectEnd    = pClip->clipSelectEnd();
		unsigned long iSelectOffset = iSelectStart - iClipStart;
		unsigned long iSelectLength = iSelectEnd - iSelectStart;
		// Determine and dispatch selected clip regions...
		if (iSelectStart > iClipStart) {
			if (iSelectEnd < iClipEnd) {
				// -- Middle region...
				if (bClipboard) {
					m_clipboard.addItem(pClip,
						iSelectStart,
						iClipOffset + iSelectOffset,
						iSelectLength);
				}
				if (pClipCommand) {
					// Left-clip...
					pClipCommand->resizeClip(pClip,
						iClipStart,
						iClipOffset,
						iSelectOffset);
					// Right-clip...
					qtractorClip *pClipEx = cloneClip(pClip);
					if (pClipEx) {
						pClipEx->setClipStart(iSelectEnd);
						pClipEx->setClipOffset(iClipOffset
							+ iSelectOffset + iSelectLength);
						pClipEx->setClipLength(iClipEnd - iSelectEnd);
						pClipEx->setFadeOutLength(pClip->fadeOutLength());
						pClipCommand->addClip(pClipEx, pTrack);
					}
				}
				// Done, middle region.
			} else {
				// -- Right region...
				if (bClipboard) {
					m_clipboard.addItem(pClip,
						iSelectStart,
						iClipOffset + iSelectOffset,
						iSelectLength);
				}
				if (pClipCommand) {
					pClipCommand->resizeClip(pClip,
						iClipStart,
						iClipOffset,
						iSelectOffset);
				}
				// Done, right region.
			}
		}
		else
		if (iSelectEnd < iClipEnd) {
			// -- Left region...
			if (bClipboard) {
				m_clipboard.addItem(pClip,
					iClipStart,
					iClipOffset,
					iSelectLength);
			}
			if (pClipCommand) {
				pClipCommand->resizeClip(pClip,
					iSelectEnd,
					iClipOffset + iSelectLength,
					iClipLength - iSelectLength);
			}
			// Done, left region.
		} else {
			// -- Whole clip...
			if (bClipboard) {
				m_clipboard.addItem(pClip,
					iClipStart,
					iClipOffset,
					iClipLength);
			}
			if (pClipCommand) {
				pClipCommand->removeClip(pClip);
			}
			// Done, whole clip.
		}
	}

	// Reset selection on cut or delete;
	// put it in the form of an undoable command...
	if (pClipCommand) {
		m_pClipSelect->clear();
		pMainForm->commands()->exec(pClipCommand);
	}
}


// Paste from clipboard.
void qtractorTrackView::pasteClipSelect (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Check if anythings really on clipboard...
	if (isClipboardEmpty())
		return;

	// Take hand on where we wish to start pasting clips...
	qtractorTrack *pPasteTrack = m_pTracks->currentTrack();
	bool bSingleTrack = (m_clipboard.singleTrack && pPasteTrack
		&& m_clipboard.singleTrack->trackType() == pPasteTrack->trackType());

	// We'll build a composite command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(pMainForm, tr("paste clip"));

	long delta = pSession->editHead()
		- pSession->frameFromPixel(m_clipboard.rect.x());

	QListIterator<ClipItem *> iter(m_clipboard.items);
	while (iter.hasNext()) {
		ClipItem *pClipItem = iter.next();
		qtractorClip *pClip = pClipItem->clip;
		if (!bSingleTrack)
			pPasteTrack = pClip->track();
		unsigned long iClipStart = pClipItem->clipStart;
		if ((long) iClipStart + delta > 0)
			iClipStart += delta;
		else
			iClipStart = 0;
		// Now, its imperative to make a proper copy of those clips...
		qtractorClip *pNewClip = NULL;
		switch (pPasteTrack->trackType()) {
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
		default:
			break;
		}
		// Add the new pasted clip...
		if (pNewClip) {
			pNewClip->setClipStart(iClipStart);
			pNewClip->setClipOffset(pClipItem->clipOffset);
			pNewClip->setClipLength(pClipItem->clipLength);
			pNewClip->setFadeInLength(pClipItem->fadeInLength);
			pNewClip->setFadeOutLength(pClipItem->fadeOutLength);
			pClipCommand->addClip(pNewClip, pPasteTrack);
		}
	}

	// Put it in the form of an undoable command...
	pMainForm->commands()->exec(pClipCommand);
}


// Intra-drag-n-drop clip move method.
void qtractorTrackView::moveClipSelect ( qtractorTrack *pTrack, int dx )
{
	if (pTrack == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Check if anything is really selected...
	if (m_pClipSelect->items().count() < 1)
		return;

	// We can only move clips between tracks of the same type...
	qtractorTrack *pSingleTrack = m_pClipSelect->singleTrack();
	if (pSingleTrack && pSingleTrack->trackType() != pTrack->trackType())
		return;

	// We'll build a composite command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(pMainForm, tr("move clip"));

	QListIterator<qtractorClipSelect::Item *> iter(m_pClipSelect->items());
	while (iter.hasNext()) {
		qtractorClipSelect::Item *pClipItem	= iter.next();
		qtractorClip *pClip = pClipItem->clip;
		if (pSingleTrack == NULL)
			pTrack = pClip->track();
		int x = (pClipItem->rectClip.x() + dx);
		// Clip parameters.
		unsigned long iClipStart    = pClip->clipStart();
		unsigned long iClipOffset   = pClip->clipOffset();
		unsigned long iClipLength   = pClip->clipLength();
		unsigned long iClipEnd      = iClipStart + iClipLength;
		// Clip selection points.
		unsigned long iSelectStart  = pClip->clipSelectStart();
		unsigned long iSelectEnd    = pClip->clipSelectEnd();
		unsigned long iSelectOffset = iSelectStart - iClipStart;
		unsigned long iSelectLength = iSelectEnd - iSelectStart;
		// Determine and keep clip regions...
		if (iSelectStart > iClipStart) {
			// -- Left clip...
			qtractorClip *pClipLeft = cloneClip(pClip);
			pClipLeft->setClipStart(iClipStart);
			pClipLeft->setClipOffset(iClipOffset);
			pClipLeft->setClipLength(iSelectOffset);
			pClipLeft->setFadeInLength(pClip->fadeInLength());
			pClipCommand->addClip(pClipLeft, pClipLeft->track());
			// Done, leftt clip.
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
		// -- Moved clip...
		pClipCommand->moveClip(pClip, pTrack,
			pSession->frameSnap(pSession->frameFromPixel(x > 0 ? x : 0)),
			iClipOffset + iSelectOffset,
			iSelectLength);
	}

	// May reset selection, yep.
	m_pClipSelect->clear();

	// Put it in the form of an undoable command...
	pMainForm->commands()->exec(pClipCommand);
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


// Context menu request slot.
void qtractorTrackView::contentsContextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->editMenu()->exec(pContextMenuEvent->globalPos());
}


// end of qtractorTrackView.cpp
