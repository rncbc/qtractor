// qtractorTrackView.cpp
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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorTrackView.h"
#include "qtractorTrackList.h"
#include "qtractorTrackTime.h"
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

#include <qapplication.h>
#include <qdragobject.h>
#include <qpopupmenu.h>
#include <qtooltip.h>
#include <qpainter.h>
#include <qcursor.h>
#include <qpen.h>

//----------------------------------------------------------------------------
// qtractor_alpha_blend -- alpha blending experimentalism.

#include <qbitmap.h>

inline void qtractor_alpha_blend ( QImage& image, int val )
{
	const int a = (256 - val) & 0xff;	
	const int w = image.width();
	const int h = image.height();

	for (int y = 0; y < h; y++) {
		QRgb *pScanLine = reinterpret_cast<QRgb *> (image.scanLine(y));
		for (int x = 0; x < w; x++) {
			const QRgb rgba = *pScanLine;
			*pScanLine++ = qRgba(
				qRed(rgba), qGreen(rgba), qBlue(rgba), a * qAlpha(rgba));
		}
	}
}


//----------------------------------------------------------------------------
// qtractorTrackView -- Track view widget.

// Constructor.
qtractorTrackView::qtractorTrackView ( qtractorTracks *pTracks,
	QWidget *pParent, const char *pszName )
	: QScrollView(pParent, pszName, WStaticContents | WNoAutoErase)
{
	m_pTracks = pTracks;
	m_pPixmap = new QPixmap();

	m_pSessionCursor = NULL;
	
	m_dropItems.setAutoDelete(true);
	m_dropType = qtractorTrack::None;

	m_dragState   = DragNone;
	m_iDraggingX  = 0;
	m_pClipDrag   = NULL;
	m_pClipSelect = new qtractorClipSelect();
	m_selectMode  = SelectClip;

	m_iPlayHead  = 0;

	m_iPlayHeadX = 0;
	m_iEditHeadX = 0;
	m_iEditTailX = 0;

	m_iLastRecordX = 0;

	// Zoom tool widgets
	m_pHzoomIn   = new QToolButton(this);
	m_pHzoomOut  = new QToolButton(this);
	m_pVzoomIn   = new QToolButton(this);
	m_pVzoomOut  = new QToolButton(this);
	m_pXzoomTool = new QToolButton(this);

	const QPixmap& pmZoomIn = QPixmap::fromMimeSource("viewZoomIn.png");
	m_pHzoomIn->setPixmap(pmZoomIn);
	m_pVzoomIn->setPixmap(pmZoomIn);

	const QPixmap& pmZoomOut = QPixmap::fromMimeSource("viewZoomOut.png");
	m_pHzoomOut->setPixmap(pmZoomOut);
	m_pVzoomOut->setPixmap(pmZoomOut);

	m_pXzoomTool->setPixmap(QPixmap::fromMimeSource("viewZoomTool.png"));

	QScrollView::setCornerWidget(m_pXzoomTool);

	m_pHzoomIn->setAutoRepeat(true);
	m_pHzoomOut->setAutoRepeat(true);
	m_pVzoomIn->setAutoRepeat(true);
	m_pVzoomOut->setAutoRepeat(true);

	QToolTip::add(m_pHzoomIn,   tr("Zoom in (horizontal)"));
	QToolTip::add(m_pHzoomOut,  tr("Zoom out (horizontal)"));
	QToolTip::add(m_pVzoomIn,   tr("Zoom in (vertical)"));
	QToolTip::add(m_pVzoomOut,  tr("Zoom out (vertical)"));
	QToolTip::add(m_pXzoomTool, tr("Zoom reset"));

	QObject::connect(m_pHzoomIn, SIGNAL(clicked()),
		m_pTracks, SLOT(horizontalZoomInSlot()));
	QObject::connect(m_pHzoomOut, SIGNAL(clicked()),
		m_pTracks, SLOT(horizontalZoomOutSlot()));
	QObject::connect(m_pVzoomIn, SIGNAL(clicked()),
		m_pTracks, SLOT(verticalZoomInSlot()));
	QObject::connect(m_pVzoomOut, SIGNAL(clicked()),
		m_pTracks, SLOT(verticalZoomOutSlot()));
	QObject::connect(m_pXzoomTool, SIGNAL(clicked()),
		m_pTracks, SLOT(viewZoomToolSlot()));

	QScrollView::viewport()->setBackgroundMode(Qt::NoBackground);

	QScrollView::setHScrollBarMode(QScrollView::AlwaysOn);
	QScrollView::setVScrollBarMode(QScrollView::AlwaysOn);

	QScrollView::viewport()->setFocusPolicy(QWidget::StrongFocus);
//	QScrollView::viewport()->setFocusProxy(this);
	QScrollView::viewport()->setAcceptDrops(true);
	QScrollView::setDragAutoScroll(false);

	const QFont& font = QScrollView::font();
	QScrollView::setFont(QFont(font.family(), font.pointSize() - 2));

	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorTrackView::~qtractorTrackView (void)
{
	clearClipboard();

	if (m_pSessionCursor)
		delete m_pSessionCursor;

	delete m_pClipSelect;
	delete m_pPixmap;
}


// Scrollbar/tools layout management.
void qtractorTrackView::setHBarGeometry ( QScrollBar& hbar,
	int x, int y, int w, int h )
{
	hbar.setGeometry(x, y, w - h * 2, h);
	if (m_pHzoomOut)
		m_pHzoomOut->setGeometry(x + w - h * 2, y, h, h);
	if (m_pHzoomIn)
		m_pHzoomIn->setGeometry(x + w - h, y, h, h);
}

void qtractorTrackView::setVBarGeometry ( QScrollBar& vbar,
	int x, int y, int w, int h )
{
	vbar.setGeometry(x, y, w, h - w * 2);
	if (m_pVzoomIn)
		m_pVzoomIn->setGeometry(x, y + h - w * 2, w, w);
	if (m_pVzoomOut)
		m_pVzoomOut->setGeometry(x, y + h - w, w, w);
}


// Update track view content height.
void qtractorTrackView::updateContentsHeight (void)
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorTrackView::updateContentsHeight()\n");
#endif

	int iContentsHeight = 0;
	QListViewItem *pItem = m_pTracks->trackList()->firstChild();
	while (pItem) {
		iContentsHeight += pItem->totalHeight();
		pItem = pItem->nextSibling();
	}

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, " => iContentsHeight=%d\n", iContentsHeight);
#endif

	// Do the contents resize thing...
	QScrollView::resizeContents(QScrollView::contentsWidth(), iContentsHeight);
}


// Update track view content width.
void qtractorTrackView::updateContentsWidth ( int iContentsWidth )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorTrackView::updateContentsWidth(%d)\n", iContentsWidth);
#endif

	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		if (iContentsWidth < 1) 
			iContentsWidth = pSession->pixelFromFrame(pSession->sessionLength());
		iContentsWidth += pSession->pixelFromBeat(2 * pSession->beatsPerBar());
		m_iEditHeadX = pSession->pixelFromFrame(pSession->editHead());
		m_iEditTailX = pSession->pixelFromFrame(pSession->editTail());
	}

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, " => iContentsWidth=%d\n", iContentsWidth);
#endif

	// Do the contents resize thing...
	QScrollView::resizeContents(iContentsWidth,	QScrollView::contentsHeight());

	// Force an update on the track time line too...
	m_pTracks->trackTime()->resizeContents(
		iContentsWidth + 100, m_pTracks->trackTime()->contentsHeight());
	m_pTracks->trackTime()->updateContents();
}


// Local rectangular contents update.
void qtractorTrackView::updateContents ( const QRect& rect, bool bRefresh )
{
	if (bRefresh)
		updatePixmap(QScrollView::contentsX(), QScrollView::contentsY());

	QScrollView::repaintContents(rect);

	if (m_dragState == DragMove) {
		showClipSelect();
	} else if (m_dragState == DragDrop) {
		showDropRects();
	} else if (m_dragState == DragFadeIn || m_dragState == DragFadeOut) {
		drawFadeHandle();
	}
}


// Overall contents update.
void qtractorTrackView::updateContents ( bool bRefresh )
{
	if (bRefresh)
		updatePixmap(QScrollView::contentsX(), QScrollView::contentsY());

	QScrollView::repaintContents();

	if (m_dragState == DragMove) {
		showClipSelect();
	} else if (m_dragState == DragDrop) {
		showDropRects();
	} else if (m_dragState == DragFadeIn || m_dragState == DragFadeOut) {
		drawFadeHandle();
	}
}


// Special recording visual feedback.
void qtractorTrackView::updateContentsRecord ( bool bRefresh )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int x = QScrollView::contentsX();
	int w = QScrollView::width();

	int iCurrRecordX = m_iPlayHeadX;
	if (iCurrRecordX > x + w)
		iCurrRecordX = x + w;

	if (m_iLastRecordX < iCurrRecordX) {
		if (x < m_iLastRecordX && m_iLastRecordX < x + w)
			x = m_iLastRecordX - 2;
		w = iCurrRecordX - x + 2;
		updateContents(
			QRect(x, QScrollView::contentsY(), w, QScrollView::height()),
			bRefresh);
	}
	else if (m_iLastRecordX > iCurrRecordX)
		updateContents(bRefresh);

	m_iLastRecordX = iCurrRecordX;
}


// Draw the track view.
void qtractorTrackView::drawContents ( QPainter *p,
	int clipx, int clipy, int clipw, int cliph )
{
	// Draw viewport canvas...
	p->drawPixmap(clipx, clipy, *m_pPixmap,
		clipx - QScrollView::contentsX(),
		clipy - QScrollView::contentsY(),
		clipw, cliph);

	// Lines a-head...
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int x;

	// On-the-fly recording clip drawing...
	if (pSession->isRecording() && pSession->isPlaying()) {
		unsigned long iTrackStart = pSession->frameFromPixel(clipx);
		unsigned long iTrackEnd   = pSession->playHead();
		if (iTrackStart < iTrackEnd) {
			int y1 = 0;
			int y2 = 0;
			QListViewItem *pItem = m_pTracks->trackList()->firstChild();
			while (pItem && y2 < clipy + cliph) {
				y1  = y2;
				y2 += pItem->totalHeight();
				if (y2 > clipy) {
					// Dispatch to paint this track...
					qtractorTrack *pTrack = NULL;
					qtractorTrackListItem *pTrackItem
						= static_cast<qtractorTrackListItem *> (pItem);
					if (pTrackItem)
						pTrack = pTrackItem->track();
					if (pTrack && pTrack->clipRecord()) {
						int h = y2 - y1 - 2;
						const QRect trackRect(clipx - 1, y1 + 1, clipw + 2, h);
						qtractorClip *pClipRecord = pTrack->clipRecord(); 
						unsigned long iClipStart  = pClipRecord->clipStart();
						unsigned long iClipOffset = 0;
						if (iClipStart < iTrackStart)
							iClipOffset += iTrackStart - iClipStart;
						x = pSession->pixelFromFrame(iClipStart);
						int w = 0;
						if (iClipStart < iTrackEnd)
							w += pSession->pixelFromFrame(iTrackEnd - iClipStart);
						const QRect& clipRect
							= QRect(x, y1 + 1, w, h).intersect(trackRect);
						if (!clipRect.isEmpty()) {
#if 0
							pClipRecord->drawClip(p, clipRect, iClipOffset);
#else
							w = clipRect.width();
							h = clipRect.height();
							QPixmap pm(w, h);
							QBitmap bm(w, h);
							bm.fill(Qt::color1);
							pm.setMask(bm);
							QPainter painter(&pm);
							painter.setFont(QScrollView::font());
						//	pClipRecord->drawClip(&painter,
						//		QRect(0, 0, w, h), iClipOffset);
							painter.setPen(pTrack->background().dark());
							painter.setBrush(pTrack->background());
							painter.drawRect(0, 0, w, h);
							QImage img = pm.convertToImage();
							qtractor_alpha_blend(img, 120);
							p->drawImage(clipRect.x(), clipRect.y(), img);
#endif
						}
					}
				}
				pItem = pItem->nextSibling();
			}
		}
	}

	// Draw edit-head line...
	x = pSession->pixelFromFrame(pSession->editHead());
	if (x >= clipx && x < clipx + clipw) {
		p->setPen(Qt::blue);
		p->drawLine(x, clipy, x, clipy + cliph);
	}

	// Draw edit-tail line...
	x = pSession->pixelFromFrame(pSession->editTail());
	if (x >= clipx && x < clipx + clipw) {
		p->setPen(Qt::blue);
		p->drawLine(x, clipy, x, clipy + cliph);
	}

	// Draw play-head line...
	x = pSession->pixelFromFrame(playHead());
	if (x >= clipx && x < clipx + clipw) {
		p->setPen(Qt::red);
		p->drawLine(x, clipy, x, clipy + cliph);
	}
}


// Resize event handler.
void qtractorTrackView::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QScrollView::resizeEvent(pResizeEvent);
	updateContents();
}


// (Re)create the complete track view pixmap.
void qtractorTrackView::updatePixmap ( int cx, int cy )
{
#if 0
	QWidget *pViewport = QScrollView::viewport();
	int w = pViewport->width()  + 2;
	int h = pViewport->height() + 2;
#else
	int w = QScrollView::width();
	int h = QScrollView::height();
#endif

	m_pPixmap->resize(w, h);
	m_pPixmap->fill(Qt::darkGray);

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	QPainter p(m_pPixmap);
	p.setViewport(0, 0, w, h);
	p.setWindow(0, 0, w, h);
	p.setFont(QScrollView::font());

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

	// Draw track and horizontal lines...
	int iTrack = 0;
	int x, y1, y2;
	y1 = y2 = 0;
	QListViewItem *pItem = m_pTracks->trackList()->firstChild();
	while (pItem && y2 < cy + h) {
		y1  = y2;
		y2 += pItem->totalHeight();
		if (y2 > cy) {
			// Dispatch to paint this track...
			qtractorTrack *pTrack = NULL;
			qtractorTrackListItem *pTrackItem
				= static_cast<qtractorTrackListItem *> (pItem);
			if (pTrackItem)
				pTrack = pTrackItem->track();
			if (pTrack) {
				if (y1 > cy) {
					p.setPen(Qt::lightGray);
					p.drawLine(0, y1 - cy, w, y1 - cy);
				}
				QRect trackRect(0, y1 - cy + 1, w, y2 - y1 - 2);
				p.fillRect(trackRect, Qt::gray);
				pTrack->drawTrack(&p, trackRect, iTrackStart, iTrackEnd,
					m_pSessionCursor->clip(iTrack));
			}
			p.setPen(Qt::darkGray);
			p.drawLine(0, y2 - cy - 1, w, y2 - cy - 1);
		}
		pItem = pItem->nextSibling();
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
					p.setPen(Qt::lightGray);
					p.drawLine(x, 0, x, y2 - cy - 2);
				}
				if (bBeatIsBar || iPixelsPerBeat > 16) {
					p.setPen(Qt::darkGray);
					p.drawLine(x - 1, 0, x - 1, y2 - cy - 2);
				}
			}
			iFrameFromBeat += iFramesPerBeat;
			x = pSession->pixelFromFrame(iFrameFromBeat) - cx;
			iBeat++;
		}
	}

	// Fill the empty area...
	if (y2 < cy + h) {
		p.setPen(Qt::gray);
		p.drawLine(0, y2 - cy, w, y2 - cy);
	//	p->fillRect(0, y2 - cy + 1, w, h, Qt::darkGray);
	}

	// Draw loop boundaries, if applicable...
	if (pSession->isLooping()) {
		p.setPen(Qt::darkCyan);
		x = pSession->pixelFromFrame(pSession->loopStart()) - cx;
		if (x >= 0 && x < w)
			p.drawLine(x, 0, x, h);
		x = pSession->pixelFromFrame(pSession->loopEnd()) - cx;
		if (x >= 0 && x < w)
			p.drawLine(x, 0, x, h);
	}
}


// To have track view in v-sync with track list.
void qtractorTrackView::contentsMovingSlot ( int /*cx*/, int cy )
{
	if (QScrollView::contentsY() != cy)
		m_pTracks->setContentsPos(this, QScrollView::contentsX(), cy);
}


// Get track list item from given contents vertical position.
qtractorTrackListItem *qtractorTrackView::trackListItemAt (
	const QPoint& pos, qtractorTrackViewInfo *pTrackViewInfo ) const
{
	int y1 = 0;
	int y2 = 0;
	int iTrack = 0;

	QListViewItem *pItem = m_pTracks->trackList()->firstChild();
	while (pItem) {
		y1  = y2;
		y2 += pItem->totalHeight();
		if (y2 > pos.y()) {
			// Force left pane current selection...
			m_pTracks->trackList()->setSelected(pItem, true);
			break;
		}
		pItem = pItem->nextSibling();
		iTrack++;
	}

	if (pTrackViewInfo) {
		qtractorSession *pSession = m_pTracks->session();
		if (pSession == NULL)
			return NULL;
		if (m_pSessionCursor == NULL)
			return NULL;
		int x = QScrollView::contentsX();
		int w = QScrollView::width();   	// View width, not contents.
		if (pItem == NULL) {				// Below all tracks.
			y1 = y2;
			y2 = y1 + (48 * pSession->verticalZoom()) / 100;
		}
		pTrackViewInfo->trackIndex = iTrack;
		pTrackViewInfo->trackStart = m_pSessionCursor->frame();
		pTrackViewInfo->trackEnd   = pTrackViewInfo->trackStart
			+ pSession->frameFromPixel(w);
		pTrackViewInfo->trackRect.setRect(x, y1 + 1, w, y2 - y1 - 2);
	}

	return static_cast<qtractorTrackListItem *> (pItem);
}


// Get track from given contents vertical position.
qtractorTrack *qtractorTrackView::trackAt ( const QPoint& pos,
	qtractorTrackViewInfo *pTrackViewInfo ) const
{
	qtractorTrackListItem *pTrackItem = trackListItemAt(pos, pTrackViewInfo);
	return (pTrackItem ? pTrackItem->track() : NULL);
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
bool qtractorTrackView::trackInfo ( qtractorTrack *pTrack,
	qtractorTrackViewInfo *pTrackViewInfo ) const
{
	int y1, y2 = 0;
	int iTrack = 0;
	qtractorTrackListItem *pTrackItem = NULL;
	QListViewItem *pItem = m_pTracks->trackList()->firstChild();
	while (pItem) {
		y1  = y2;
		y2 += pItem->totalHeight();
		pTrackItem = static_cast<qtractorTrackListItem *> (pItem);
		if (pTrackItem == NULL)
			return false;
		if (pTrackItem->track() == pTrack && m_pSessionCursor) {
			int x = QScrollView::contentsX();
			int w = QScrollView::width();   	// View width, not contents.
			pTrackViewInfo->trackIndex = iTrack;
			pTrackViewInfo->trackStart = m_pSessionCursor->frame();
			pTrackViewInfo->trackEnd   = pTrackViewInfo->trackStart
				+ m_pTracks->session()->frameFromPixel(w);
			pTrackViewInfo->trackRect.setRect(x, y1 + 1, w, y2 - y1 - 2);
			return true;
		}
		pItem = pItem->nextSibling();
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

	hideClipSelect();

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
	QScrollView::ensureVisible(pos.x(), pos.y(), 16, 16);

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
	const QPoint& pos = pDropEvent->pos();
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, &tvi);
	if (!m_dropItems.isEmpty()) {
		hideDropRects();
		// Adjust vertically to target track...
		updateDropRects(tvi.trackRect.y() + 1, tvi.trackRect.height() - 2);
		// Always change horizontally wise...
		int  x = m_rectDrag.x();
		int dx = (pos.x() - m_posDrag.x());
		if (x + dx < 0)
			dx = -(x);	// Force to origin (x=0).
		m_iDraggingX = (pSession->pixelSnap(x + dx) - x);
		QScrollView::ensureVisible(pos.x(), pos.y(), 16, 16);
		showDropRects();
		// OK, we've moved it...
		return pTrack;
	}

	// Let's start from scratch...
	m_dropItems.clear();
	m_dropType = qtractorTrack::None;
	
	// Can it be single track channel (MIDI for sure)?
	if (qtractorFileChannelDrag::canDecode(pDropEvent)) {
		// In the meantime, it can only be only one...
		QString sPath;
		unsigned short iChannel = 0;
		if (qtractorFileChannelDrag::decode(pDropEvent, sPath, &iChannel)) {
			m_dropItems.append(new DropItem(sPath, iChannel));
			m_dropType = qtractorTrack::Midi;
		}
	}	// Can we decode it as Audio/MIDI files?
	else if (QUriDrag::canDecode(pDropEvent)) {
		// Let's see how many files there are...
		QStringList files;
		if (QUriDrag::decodeLocalFiles(pDropEvent, files)) {
			for (QStringList::Iterator iter = files.begin();
					iter != files.end(); ++iter) {
				m_dropItems.append(new DropItem(*iter));
			}
		}
	}


	// Nice, now we'll try to set a preview selection rectangle set...
	m_posDrag.setX(pSession->pixelSnap(pos.x() - 8));
	m_posDrag.setY(tvi.trackRect.y() + 1);
	m_rectDrag.setRect(
		m_posDrag.x(), m_posDrag.y(), 0, tvi.trackRect.height() - 2);

	// Nows time to add those rectangles...
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
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
							m_rectDrag.moveBy(0, m_rectDrag.height() + 4);
						}
					}
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Midi;
				} else if (file.readTrack(&seq, pDropItem->channel)
					&& seq.duration() > 0) {
					m_rectDrag.setWidth(
						pSession->pixelFromTick(seq.duration()));
					pDropItem->rect = m_rectDrag;
					m_rectDrag.moveBy(0, m_rectDrag.height() + 4);
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Midi;
				} else /*if (m_dropType == qtractorTrack::Midi)*/ {
					m_dropItems.remove(pDropItem);
				}
				file.close();
				continue;
			} else if (m_dropType == qtractorTrack::Midi) {
				m_dropItems.remove(pDropItem);
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
					m_rectDrag.moveBy(0, m_rectDrag.height() + 4);
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Audio;
				} else if (m_dropType == qtractorTrack::Audio) {
					m_dropItems.remove(pDropItem);
				}
				delete pFile;
				continue;
			} else if (m_dropType == qtractorTrack::Audio) {
				m_dropItems.remove(pDropItem);
			}
		}
	}

	// Are we still here?
	if (m_dropItems.isEmpty()) {
		m_dropType = qtractorTrack::None;
		return NULL;
	}

	// Ok, sure we're into some drag state...
	m_dragState = DragDrop;
	m_iDraggingX = 0;	

	// Finally, show it to the world...
	showDropRects();

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
void qtractorTrackView::contentsDragEnterEvent (
	QDragEnterEvent *pDragEnterEvent )
{
	if (canDropTrack(pDragEnterEvent)) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


// Drag move event handler.
void qtractorTrackView::contentsDragMoveEvent (
	QDragMoveEvent *pDragMoveEvent )
{
	if (canDropTrack(pDragMoveEvent)) {
		pDragMoveEvent->accept();
	} else {
		pDragMoveEvent->ignore();
	}
}


// Drag move event handler.
void qtractorTrackView::contentsDragLeaveEvent ( QDragLeaveEvent * )
{
	// Maybe we have something currently going on;
	// redraw to hide preview selection rectangle...
	resetDragState();
}


// Drop event handler.
void qtractorTrackView::contentsDropEvent (
	QDropEvent *pDropEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Add new clips on proper and consecutive track locations...
	unsigned long iClipStart // = pSession->frameSnap(
		= pSession->frameFromPixel(m_rectDrag.x() + m_iDraggingX);

	// Now check whether the drop is intra-track...
	qtractorTrack *pTrack = dragDropTrack(pDropEvent);
	if (pTrack == NULL) {
		// Do we have something to drop anyway?
		// if yes, this is a extra-track drop...
		if (!m_dropItems.isEmpty()) {
			// Prepare file list for import...
			QStringList files;
			for (DropItem *pDropItem = m_dropItems.first();
					pDropItem; pDropItem = m_dropItems.next()) {
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
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
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
void qtractorTrackView::contentsMousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	resetDragState();

	// Which mouse state?
	const bool bModifier = (pMouseEvent->state()
		& (Qt::ShiftButton | Qt::ControlButton));
	const QPoint& pos = pMouseEvent->pos();

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
				QScrollView::setCursor(QCursor(Qt::PointingHandCursor));
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

	QScrollView::contentsMousePressEvent(pMouseEvent);

	// Make sure we've get focus back...
	QScrollView::setFocus();
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorTrackView::contentsMouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// Are we already moving/dragging something?
	const QPoint& pos = pMouseEvent->pos();

	switch (m_dragState) {
	case DragMove:
		dragMoveTrack(pos);
		break;
	case DragFadeIn:
	case DragFadeOut:
		dragFadeMove(pos);
		break;
	case DragSelect:
		if (m_selectMode != SelectRange)
			hideDragRect(m_rectDrag);
		m_rectDrag.setBottomRight(pos);
		QScrollView::ensureVisible(pos.x(), pos.y(), 16, 16);
		selectRect(m_rectDrag, m_selectMode);
		if (m_selectMode != SelectRange)
			showDragRect(m_rectDrag, 1);
		break;
	case DragStart:
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			// Check if we're pointing in some fade-in/out handle...
			if (dragFadeStart(m_posDrag)) {
				m_iDraggingX = (pos.x() - m_posDrag.x());
				drawFadeHandle();	// Show.
			} else {
				// We'll start dragging clip/regions alright...
				qtractorClipSelect::Item *pClipItem = NULL;
				if (m_pClipDrag)
					pClipItem = m_pClipSelect->findClip(m_pClipDrag);
				if (pClipItem && pClipItem->rectClip.contains(pos)) {
					int x = m_pTracks->session()->pixelSnap(m_rectDrag.x());
					m_iDraggingX = (x - m_rectDrag.x());
					m_dragState = DragMove;
					QScrollView::setCursor(QCursor(Qt::SizeAllCursor));
					showClipSelect();
				} else {
					// We'll start rubber banding...
					m_rectDrag.setTopLeft(m_posDrag);
					m_rectDrag.setBottomRight(pos);
					m_dragState = DragSelect;
					if (m_selectMode == SelectRange) {
						QScrollView::setCursor(QCursor(Qt::SizeHorCursor));
					} else {
						QScrollView::setCursor(QCursor(Qt::CrossCursor));
						showDragRect(m_rectDrag, 1);
					}
				}
			}
		}
		// Fall thru...
	case DragDrop:
	case DragNone:
	default:
		break;
	}

	QScrollView::contentsMouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorTrackView::contentsMouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QScrollView::contentsMouseReleaseEvent(pMouseEvent);

	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		// Direct snap positioning...
		unsigned long iFrame = pSession->frameSnap(
			pSession->frameFromPixel(m_posDrag.x() > 0 ? m_posDrag.x() : 0));
		// Which mouse state?
		const bool bModifier = (pMouseEvent->state()
			& (Qt::ShiftButton | Qt::ControlButton));
		switch (m_dragState) {
		case DragSelect:
			// Here we're mainly supposed to select a few bunch
			// of clips (all that fall inside the rubber-band...
			selectRect(m_rectDrag, m_selectMode);
			break;
		case DragMove:
			// Let's move them...
			moveClipSelect(dragMoveTrack(pMouseEvent->pos()), m_iDraggingX);
			break;
		case DragFadeIn:
		case DragFadeOut:
			dragFadeDrop(pMouseEvent->pos());
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
					// Not quite a selection, but for
					// immediate visual feedback...
					m_pTracks->selectionChangeNotify();
					// Done with (deferred) play-head positioning.
				} else {
					// Deferred left-button edit-head positioning...
					setEditHead(iFrame);
					// Not a selection, rather just for visual feedback...
					m_pTracks->selectionChangeNotify();
				}
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
	QScrollView::setFocus();
}


// Clip file(item) selection convenience method.
void qtractorTrackView::selectClipFile ( bool bReset )
{
	if (m_pClipDrag == NULL)
		return;

	// Do the slecvtion dance, first...
	const bool bSelect = !m_pClipDrag->isClipSelected();
	if (!bReset) {
		m_pClipSelect->selectClip(m_pClipDrag, m_rectDrag, bSelect);
		updateContents(m_rectDrag);
		m_pTracks->selectionChangeNotify();
	} else if (bSelect) {
		m_pClipSelect->clear();
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

	// The precise (snapped) selection frame points...
	QRect rect(rectDrag.normalize());

	QRect rectRange(0, 0, 0, QScrollView::contentsHeight());
	rectRange.setLeft(pSession->pixelSnap(rect.left()));
	rectRange.setRight(pSession->pixelSnap(rect.right()));

	unsigned long iSelectStart = pSession->frameFromPixel(rectRange.left());
	unsigned long iSelectEnd   = pSession->frameFromPixel(rectRange.right());

	// Special whole-vertical range case...
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
	QListViewItem *pItem = m_pTracks->trackList()->firstChild();
	while (pItem) {
		y1  = y2;
		y2 += pItem->totalHeight();
		if (rect.bottom() < y1)
			break;
		if (y2 >= rect.top()) {
			qtractorTrack *pTrack = NULL;
			qtractorTrackListItem *pTrackItem
				= static_cast<qtractorTrackListItem *> (pItem);
			if (pTrackItem)
				pTrack = pTrackItem->track();
			if (pTrack) {
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
		}
		pItem = pItem->nextSibling();
	}

	// Update the screen real estate...
	if (!rectUpdate.isEmpty())
		updateContents(rectUpdate);

	// That's all very nice but we'll also set edit-range positioning...
	if (selectEdit & EditHead)
		setEditHead(iSelectStart);
	if (selectEdit & EditTail)
		setEditTail(iSelectEnd);

	m_pTracks->selectionChangeNotify();
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
void qtractorTrackView::selectTrack ( qtractorTrack *pTrack, bool bReset )
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
	QListViewItem *pItem = m_pTracks->trackList()->firstChild();
	while (pItem) {
		y1  = y2;
		y2 += pItem->totalHeight();
		qtractorTrackListItem *pTrackItem
			= static_cast<qtractorTrackListItem *> (pItem);
		if (pTrackItem && pTrackItem->track() == pTrack) {
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
		}
		pItem = pItem->nextSibling();
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
		QListViewItem *pItem = m_pTracks->trackList()->firstChild();
		while (pItem) {
			y1  = y2;
			y2 += pItem->totalHeight();
			qtractorTrack *pTrack = NULL;
			qtractorTrackListItem *pTrackItem
				= static_cast<qtractorTrackListItem *> (pItem);
			if (pTrackItem)
				pTrack = pTrackItem->track();
			if (pTrack) {
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
			}
			pItem = pItem->nextSibling();
		}
		// This is most probably an overall update...
		if (iUpdate > 0) {
			updateContents();
			m_pTracks->selectionChangeNotify();
		}
	}
	else
	// Clear all selections...
	if (m_pClipSelect->clips().count() > 0) {
		m_pClipSelect->clear();
		updateContents();
		m_pTracks->selectionChangeNotify();
	}
}


// Draw/hide the whole current clip selection.
void qtractorTrackView::updateClipSelect ( int y, int h )
{
	bool bSingleTrack = (m_pClipSelect->singleTrack() != NULL);
	qtractorClipSelect::Item *pClipItem	= m_pClipSelect->clips().first();
	while (pClipItem) {
		if (bSingleTrack) {
			pClipItem->rectClip.setY(y);
			pClipItem->rectClip.setHeight(h);
		}
		pClipItem = m_pClipSelect->clips().next();
	}
}

void qtractorTrackView::showClipSelect ( int iThickness ) const
{
	qtractorClipSelect::Item *pClipItem	= m_pClipSelect->clips().first();
	while (pClipItem) {
		showDragRect(pClipItem->rectClip, iThickness);
		pClipItem = m_pClipSelect->clips().next();
	}
}

void qtractorTrackView::hideClipSelect (void)
{
	qtractorClipSelect::Item *pClipItem	= m_pClipSelect->clips().first();
	while (pClipItem) {
		hideDragRect(pClipItem->rectClip);
		pClipItem = m_pClipSelect->clips().next();
	}
}


// Draw/hide the whole drop rectagle list
void qtractorTrackView::updateDropRects ( int y, int h )
{
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
		pDropItem->rect.setY(y);
		pDropItem->rect.setHeight(h);
		y += h + 4;
	}
}

void qtractorTrackView::showDropRects ( int iThickness )
{
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
		showDragRect(pDropItem->rect, iThickness);
	}
}

void qtractorTrackView::hideDropRects (void)
{
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
		hideDragRect(pDropItem->rect);
	}
}


// Draw/hide a dragging rectangular selection.
void qtractorTrackView::showDragRect ( const QRect& rectDrag,
	int iThickness ) const
{
	QPainter p(QScrollView::viewport());
	QRect rect(rectDrag.normalize());
	const QPoint delta(1, 1);

	// Horizontal adjust...
	rect.moveBy(m_iDraggingX, 0);
	// Convert rectangle into view coordinates...
	rect.moveTopLeft(QScrollView::contentsToViewport(rect.topLeft()));
	// Make sure the rectangle doesn't get too off view,
	// which it would make it sluggish :)
	if (rect.left() < 0)
		rect.setLeft(-(iThickness + 1));
	if (rect.right() > QScrollView::width())
		rect.setRight(QScrollView::width() + iThickness + 1);

	// Now draw it with given thickness...
	p.drawWinFocusRect(rect, Qt::gray);
	while (--iThickness > 0) {
		rect.setTopLeft(rect.topLeft() + delta);
		rect.setBottomRight(rect.bottomRight() - delta);
		p.drawWinFocusRect(rect, Qt::gray);
	}
}

void qtractorTrackView::hideDragRect ( const QRect& rectDrag )
{
	QRect rect(rectDrag.normalize());

	// Horizontal adjust...
	rect.moveBy(m_iDraggingX, 0);
	QScrollView::repaintContents(rect);
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

	drawFadeHandle();	// Hide.
	// Always change horizontally wise...
	int dx = (pos.x() - m_posDrag.x());
	if (m_rectHandle.left() + dx < m_rectDrag.left())
		dx = m_rectDrag.left() - m_rectHandle.left();
	else if (m_rectHandle.right() + dx > m_rectDrag.right())
		dx = m_rectDrag.right() - m_rectHandle.right();
	m_iDraggingX = dx;
	QScrollView::ensureVisible(pos.x(), pos.y(), 16, 16);
	drawFadeHandle();	// Show.
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
	} else {
		pClipCommand->fadeOutClip(m_pClipDrag,
				pSession->frameFromPixel(m_rectDrag.right()
					- m_iDraggingX - m_rectHandle.right()));
	}

	// Put it in the form of an undoable command...
	pMainForm->commands()->exec(pClipCommand);
}		


// Show/hide a moving clip fade in/out handle.
void qtractorTrackView::drawFadeHandle (void) const
{
	QPainter p(QScrollView::viewport());
	QRect rect(m_rectHandle);

	// Horizontal adjust...
	rect.moveBy(m_iDraggingX, 0);
	// Convert rectangle into view coordinates...
	rect.moveTopLeft(QScrollView::contentsToViewport(rect.topLeft()));

	// Draw the handle rectangle...
	p.setPen(Qt::gray);
	p.setRasterOp(Qt::NotROP);
	p.fillRect(rect, Qt::gray);

	// Draw envelope line...
	QPoint vpos;
	if (m_dragState == DragFadeIn) {
		vpos = QScrollView::contentsToViewport(m_rectDrag.bottomLeft());
		p.drawLine(vpos.x(), vpos.y(), rect.x(), rect.y());
	} 
	else 
	if (m_dragState == DragFadeOut) {
		vpos = QScrollView::contentsToViewport(m_rectDrag.bottomRight());
		p.drawLine(rect.x() + 8, rect.y(), vpos.x(), vpos.y());
	}
}


// Reset drag/select/move state.
void qtractorTrackView::resetDragState (void)
{
	// Cancel any dragging out there...
	if (m_dragState == DragDrop) {
		hideDropRects();
	} else if (m_dragState == DragFadeIn || m_dragState == DragFadeOut) {
		drawFadeHandle();	// Hide.
	} else if (m_dragState == DragSelect || m_dragState == DragMove) {
		QScrollView::updateContents();
	}

	// Should fallback mouse cursor...
	if (m_dragState != DragNone)
		QScrollView::unsetCursor();

	// Force null state.
	m_dragState  = DragNone;
	m_iDraggingX = 0;
	m_pClipDrag  = NULL;

	// No dropping files, whatsoever.
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
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(0, 0);
		} else {
			QScrollView::setContentsPos(
				0, QScrollView::contentsY());
		}
		break;
	case Qt::Key_End:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsWidth(), QScrollView::contentsHeight());
		} else {
			QScrollView::setContentsPos(
				QScrollView::contentsWidth(), QScrollView::contentsY());
		}
		break;
	case Qt::Key_Left:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsX() - QScrollView::width(),
				QScrollView::contentsY());
		} else {
			QScrollView::setContentsPos(
				QScrollView::contentsX() - 16,
				QScrollView::contentsY());
		}
		break;
	case Qt::Key_Right:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsX() + QScrollView::width(),
				QScrollView::contentsY());
		} else {
			QScrollView::setContentsPos(
				QScrollView::contentsX() + 16,
				QScrollView::contentsY());
		}
		break;
	case Qt::Key_Up:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsX(),
				QScrollView::contentsY() - QScrollView::height());
		} else {
			QScrollView::setContentsPos(
				QScrollView::contentsX(),
				QScrollView::contentsY() - 16);
		}
		break;
	case Qt::Key_Down:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsX(),
				QScrollView::contentsY() + QScrollView::height());
		} else {
			QScrollView::setContentsPos(
				QScrollView::contentsX(),
				QScrollView::contentsY() + 16);
		}
		break;
	case Qt::Key_Prior:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsX(), 16);
		} else {
			QScrollView::setContentsPos(
				QScrollView::contentsX(),
				QScrollView::contentsY() - QScrollView::height());
		}
		break;
	case Qt::Key_Next:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsX(), QScrollView::contentsHeight());
		} else {
			QScrollView::setContentsPos(
				QScrollView::contentsX(),
				QScrollView::contentsY() + QScrollView::height());
		}
		break;
	default:
		QScrollView::keyPressEvent(pKeyEvent);
		break;
	}

	// Make sure we've get focus back...
	QScrollView::setFocus();
}


// Make given frame position visible in view.
void qtractorTrackView::ensureVisibleFrame ( unsigned long iFrame )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		int x0 = QScrollView::contentsX();
		int x  = pSession->pixelFromFrame(iFrame);
		int w  = m_pPixmap->width();
		int wm = (w >> 3);
		if (x < x0)
			x -= wm;
		else if (x > x0 + w - wm && iFrame < pSession->sessionLength())
			x += w - wm;
		QScrollView::ensureVisible(x, QScrollView::contentsY(), 0, 0);
		QScrollView::setFocus();
	}
}


// Session cursor accessor.
qtractorSessionCursor *qtractorTrackView::sessionCursor (void) const
{
	return m_pSessionCursor;
}


// Vertical line positioning.
void qtractorTrackView::drawPositionX ( int& iPositionX, int x, int x2,
	const QColor& color, bool bSyncView )
{
	QPainter p(QScrollView::viewport());

	// Update track-view position...
	int x0 = QScrollView::contentsX();
	int x1 = iPositionX - x0;
	int w  = m_pPixmap->width();
	int h  = m_pPixmap->height();
	int wm = (w >> 3);

	// Time-line header extents...
	int h2 = m_pTracks->trackTime()->height();
	int d2 = (h2 >> 1);

	// Restore old position...
	if (x1 >= 0 && x1 < w) {
		// Override old view line...
		if (iPositionX != x2) {
			p.drawPixmap(x1, 0, *m_pPixmap, x1, 0, 1, h);
		//	updateContents(
		//		QRect(x0 + x1, QScrollView::contentsY(), 1, h), false);
		}
		// Update the time-line header...
		m_pTracks->trackTime()->updateContents(QRect(x0 + x1 - d2, d2, h2, d2));
	}

	// New position is in...
	iPositionX = x;

	// Force position to be in view?
	if (bSyncView && (x < x0 || x > x0 + w - wm)) {
		// Move it...
		QScrollView::setContentsPos(x - wm, QScrollView::contentsY());
	} else if (bSyncView && x0 > QScrollView::contentsWidth() - w) {
		 // Maybe we'll need some head-room...
		updateContentsWidth(x0 + w);
	} else {
		// Draw the line...
		x1 = x - x0;
		if (x1 >= 0 && x1 < w) {
			p.setPen(color);
			p.drawLine(x1, 0, x1, h);
		}
		// Update the time-line header...
		m_pTracks->trackTime()->updateContents(QRect(x0 + x1 - d2, d2, h2, d2));
	}
}


// Playhead positioning.
void qtractorTrackView::setPlayHead ( unsigned long iPlayHead, bool bSyncView )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		m_iPlayHead = iPlayHead;
		int iPlayHeadX = pSession->pixelFromFrame(iPlayHead);
		drawPositionX(m_iPlayHeadX, iPlayHeadX, iPlayHeadX, Qt::red, bSyncView);
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
		drawPositionX(m_iEditHeadX, iEditHeadX, m_iEditTailX, Qt::blue);
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
		drawPositionX(m_iEditTailX, iEditTailX, m_iEditHeadX, Qt::blue);
	}
}


// Whether there's any clip currently selected.
bool qtractorTrackView::isClipSelected (void) const
{
	return (m_pClipSelect->clips().count() > 0);
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
	if (m_pClipSelect->clips().count() < 1)
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

	qtractorClipSelect::Item *pClipItem = m_pClipSelect->clips().first();
	while (pClipItem) {
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
		// Next item in selection...
		pClipItem = m_pClipSelect->clips().next();
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

	ClipItem *pClipItem = m_clipboard.items.first();
	while (pClipItem) {
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
		// Carry on with next item...
		pClipItem = m_clipboard.items.next();
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
	if (m_pClipSelect->clips().count() < 1)
		return;

	// We can only move clips between tracks of the same type...
	qtractorTrack *pSingleTrack = m_pClipSelect->singleTrack();
	if (pSingleTrack && pSingleTrack->trackType() != pTrack->trackType())
		return;

	// We'll build a composite command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(pMainForm, tr("move clip"));

	qtractorClipSelect::Item *pClipItem
		= m_pClipSelect->clips().first();
	while (pClipItem) {
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
			pSession->frameFromPixel(x < 0 ? 0 : x),
			iClipOffset + iSelectOffset,
			iSelectLength);
		// Done, move clip.
		pClipItem = m_pClipSelect->clips().next();
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
		pMainForm->editMenu->exec(pContextMenuEvent->globalPos());
}


// end of qtractorTrackView.cpp
