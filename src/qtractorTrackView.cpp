// qtractorTrackView.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTracks.h"
#include "qtractorSession.h"

#include "qtractorAudioClip.h"
#include "qtractorAudioFile.h"
#include "qtractorMidiClip.h"
#include "qtractorMidiFile.h"
#include "qtractorSessionCursor.h"
#include "qtractorFileListView.h"
#include "qtractorClipSelect.h"

#include "qtractorMainForm.h"

#include <qapplication.h>
#include <qdragobject.h>
#include <qpopupmenu.h>
#include <qtooltip.h>
#include <qpainter.h>
#include <qcursor.h>
#include <qpen.h>


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

	m_iPlayheadX = 0;

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

	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorTrackView::~qtractorTrackView (void)
{
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
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackView::updateContentsHeight()\n");
#endif

	int iContentsHeight = 0;
	QListViewItem *pItem = m_pTracks->trackList()->firstChild();
	while (pItem) {
		iContentsHeight += pItem->totalHeight();
		pItem = pItem->nextSibling();
	}

#ifdef CONFIG_DEBUG
	fprintf(stderr, " => iContentsHeight=%d\n", iContentsHeight);
#endif

	// Reset any current selection out there...
	m_pClipSelect->clear();
	// Do the contents resize thing...
	QScrollView::resizeContents(QScrollView::contentsWidth(), iContentsHeight);
}


// Update track view content width.
void qtractorTrackView::updateContentsWidth (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackView::updateContentsWidth()\n");
#endif

	int iContentsWidth = 0;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		iContentsWidth = pSession->pixelFromFrame(pSession->sessionLength())
			+ pSession->pixelFromBeat(4 * pSession->beatsPerBar());
	}

#ifdef CONFIG_DEBUG
	fprintf(stderr, " => iContentsWidth=%d\n", iContentsWidth);
#endif

	// Reset any current selection out there...
	m_pClipSelect->clear();
	// Do the contents resize thing...
	QScrollView::resizeContents(iContentsWidth,	QScrollView::contentsHeight());

	// Force an update on the track time line too...
	m_pTracks->trackTime()->resizeContents(
		iContentsWidth + 100, m_pTracks->trackTime()->contentsHeight());
	m_pTracks->trackTime()->updateContents();
}


// Local rectangular contents update.
void qtractorTrackView::updateContents ( const QRect& rect )
{
	updatePixmap(QScrollView::contentsX(), QScrollView::contentsY());
	if (m_dragState == DragMove) {
		QScrollView::repaintContents();
		drawClipSelect(m_rectDrag, m_iDraggingX);	// Show.
	} else if (m_dragState == DragDrop && m_rectDrag.intersects(rect)) {
		QScrollView::repaintContents(m_rectDrag.unite(rect));
		drawDragRect(m_rectDrag, m_iDraggingX);	// Show.
	} else {
		QScrollView::updateContents(rect);
	}
}


// Overall contents update.
void qtractorTrackView::updateContents (void)
{
	updatePixmap(QScrollView::contentsX(), QScrollView::contentsY());
	if (m_dragState == DragMove) {
		QScrollView::repaintContents();
		drawClipSelect(m_rectDrag, m_iDraggingX);	// Show.
	} else if (m_dragState == DragDrop) {
		QScrollView::repaintContents();
		drawDragRect(m_rectDrag, m_iDraggingX);	// Show.
	} else {
		QScrollView::updateContents();
	}
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

	// Draw playhead line...
	if (m_iPlayheadX >= clipx && m_iPlayheadX < clipx + clipw) {
		p->setPen(Qt::red);
		p->drawLine(m_iPlayheadX, clipy, m_iPlayheadX, clipy + cliph);
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
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackView::updatePixmap(cx=%d, cy=%d)\n", cx, cy);
#endif

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

	QPainter p(m_pPixmap);
	p.setViewport(0, 0, w, h);
	p.setWindow(0, 0, w, h);

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

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
	int y1, y2;
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
		int iBeat = pSession->beatFromPixel(cx);
		int x = pSession->pixelFromBeat(iBeat);
		int iBeatWidth = pSession->pixelFromBeat(1);
		while (x < cx + w) {
			if (x >= cx && pSession->beatIsBar(iBeat)) {
				p.setPen(Qt::lightGray);
				p.drawLine(x - cx, 0, x - cx, y2 - cy - 2);
			}
			if (x > cx) {
				p.setPen(Qt::darkGray);
				p.drawLine(x - cx - 1, 0, x - cx - 1, y2 - cy - 2);
			}
			x += iBeatWidth;
			iBeat++;
		}
	}

	// Finally fill the empty area...
	if (y2 < cy + h) {
		p.setPen(Qt::gray);
		p.drawLine(0, y2 - cy, w, y2 - cy);
	//	p->fillRect(0, y2 - cy + 1, w, h, Qt::darkGray);
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

	if (pItem && pTrackViewInfo) {
		qtractorSession *pSession = m_pTracks->session();
		if (pSession == NULL)
			return NULL;
		if (m_pSessionCursor == NULL)
			return NULL;
		int x = QScrollView::contentsX();
		int w = QScrollView::width();   	// View width, not contents.
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
	qtractorTrackViewInfo tvi;
	qtractorTrack *pTrack = trackAt(pos, &tvi);
	if (pTrack) {
		drawClipSelect(m_rectDrag, m_iDraggingX);	// Hide.
		// May change vertically, if we've only one track selected,
		// and only between same track type...
		qtractorTrack *pSingleTrack = m_pClipSelect->singleTrack();
		if (pSingleTrack
			&& pSingleTrack->trackType() == pTrack->trackType()) {
			m_rectDrag.setY(tvi.trackRect.y() + 1);
			m_rectDrag.setHeight(tvi.trackRect.height() - 2);
		}
		// Always change horizontally wise...
		int  x = m_rectDrag.x();
		int dx = (pos.x() - m_posDrag.x());
		if (x + dx < 0)
			dx = -(x);	// Force to origin (x=0).
		m_iDraggingX = (m_pTracks->session()->pixelSnap(x + dx) - x);
		QScrollView::ensureVisible(pos.x(), pos.y(), 8, 8);
		drawClipSelect(m_rectDrag, m_iDraggingX);	// Show.
	}

	return pTrack;
}


qtractorTrack *qtractorTrackView::dragDropTrack ( QDropEvent *pDropEvent )
{
	const QPoint& pos = pDropEvent->pos();
	
	// If we're already dragging something,
	// find the current pointer track...
	qtractorTrack *pTrack = NULL;

	if (!m_dropItems.isEmpty()) {
		qtractorTrackViewInfo tvi;
		pTrack = trackAt(pos, &tvi);
		if (pTrack == NULL)
			return NULL;
		// Must be of same type...
		if (pTrack->trackType() != m_dropType)
			return NULL;
		drawDragRect(m_rectDrag, m_iDraggingX);	// Hide.
		// Adjust vertically to target track...
		m_rectDrag.setY(tvi.trackRect.y() + 1);
		m_rectDrag.setHeight(tvi.trackRect.height() - 2);
		// Always change horizontally wise...
		int  x = m_rectDrag.x();
		int dx = (pos.x() - m_posDrag.x());
		if (x + dx < 0)
			dx = -(x);	// Force to origin (x=0).
		m_iDraggingX = (m_pTracks->session()->pixelSnap(x + dx) - x);
		QScrollView::ensureVisible(pos.x(), pos.y(), 8, 8);
		drawDragRect(m_rectDrag, m_iDraggingX);	// Show.
		// OK, we've move it...
		return pTrack;
	}

	// It must be a valid track...
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return NULL;

	qtractorTrackViewInfo tvi;
	pTrack = trackAt(pos, &tvi);
	if (pTrack == NULL)
		return NULL;

	// Test decoding the dragged in object...
	switch (pTrack->trackType()) {
	// Drop Audio files in Audio tracks...
	case qtractorTrack::Audio: {
		// Can we decode it as audio files?
		if (!QUriDrag::canDecode(pDropEvent))
			return NULL;
		// Let's see how many files there are...
		QStringList files;
		if (!QUriDrag::decodeLocalFiles(pDropEvent, files))
			return NULL;
		for (QStringList::Iterator iter = files.begin();
				iter != files.end(); ++iter) {
			m_dropItems.append(new DropItem(*iter));
		}
		break;
	}
	// Drop MIDI sequences in MIDI tracks...
	case qtractorTrack::Midi: {
		// Must be a MIDI track...
		if (!qtractorFileChannelDrag::canDecode(pDropEvent))
			return NULL;
		// In the meantime, it can only be only one...
		QString sPath;
		unsigned short iChannel = 0;
		if (!qtractorFileChannelDrag::decode(pDropEvent, sPath, &iChannel))
			return NULL;
		m_dropItems.append(new DropItem(sPath, iChannel));
		break;
	}
	// No others are allowed, anyway...
	case qtractorTrack::None:
	default:
		return NULL;
	}

	// Nice, now we'll try to check if those are actually
	// audio files and get a preview selection rectangle...

	m_posDrag.setX(pSession->pixelSnap(pos.x() - 8));
	m_posDrag.setY(tvi.trackRect.y() + 1);
	m_rectDrag.setRect(
		m_posDrag.x(), m_posDrag.y(), 0, tvi.trackRect.height() - 2);
	// Nows time to estimate the drag-rectangle width...
	// we'll take the largest...
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
		switch (pTrack->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioFile *pFile
				= qtractorAudioFileFactory::createAudioFile(pDropItem->path);
			if (pFile) {
				if (pFile->open(pDropItem->path)) {
					int w = pSession->pixelFromFrame(pFile->frames());
					m_rectDrag.setWidth(m_rectDrag.width() + w);
				} else {
					m_dropItems.remove(pDropItem);
				}
				delete pFile;
			} else {
				m_dropItems.remove(pDropItem);
			}
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiFile file;
			if (file.open(pDropItem->path)) {
				qtractorMidiSequence seq;
				seq.setTicksPerBeat(pSession->ticksPerBeat());
				if (file.readTrack(&seq, pDropItem->channel)) {
					int w = pSession->pixelFromTick(seq.duration());
					m_rectDrag.setWidth(m_rectDrag.width() + w);
				} else {
					m_dropItems.remove(pDropItem);
				}
			} else {
				m_dropItems.remove(pDropItem);
			}
			break;
		}
		case qtractorTrack::None:
		default:
			break;
		}
	}

	// Maybe we have none anymore...
	if (m_dropItems.isEmpty())
		return NULL;

	// Set the according item type...
	m_dropType = pTrack->trackType();

	// We can draw initial preview rectangle...
	m_dragState = DragDrop;
	m_iDraggingX = 0;
	drawDragRect(m_rectDrag, m_iDraggingX);	// Show.
	// Done.
	return pTrack;
}


// Drag enter event handler.
void qtractorTrackView::contentsDragEnterEvent (
	QDragEnterEvent *pDragEnterEvent )
{
	qtractorTrack *pTrack = dragDropTrack(pDragEnterEvent);
	if (pTrack) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


// Drag move event handler.
void qtractorTrackView::contentsDragMoveEvent (
	QDragMoveEvent *pDragMoveEvent )
{
	qtractorTrack *pTrack = dragDropTrack(pDragMoveEvent);
	if (pTrack) {
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
	qtractorTrack *pTrack = dragDropTrack(pDropEvent);
	if (pTrack == NULL) {
		resetDragState();
		return;
	}

	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL) {
		resetDragState();
		return;
	}

	// Add new clips on proper and consecutive track locations...
	unsigned long iClipStart
		= pSession->frameFromPixel(m_rectDrag.x() + m_iDraggingX);

	// Nows time to create the clip(s)...
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
		switch (pTrack->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
			if (pAudioClip) {
				pAudioClip->setClipStart(iClipStart);
				pAudioClip->open(pDropItem->path);
				pTrack->addClip(pAudioClip);
				iClipStart += pAudioClip->clipLength();
				// Don't forget to add this one to local repository.
				m_pTracks->mainForm()->addAudioFile(pDropItem->path);
			}
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
			if (pMidiClip) {
				pMidiClip->setClipStart(iClipStart);
				pMidiClip->open(pDropItem->path, pDropItem->channel);
				if (pTrack->clips().count() < 1) {
					pTrack->setMidiChannel(pMidiClip->channel());
					pTrack->setMidiBank(pMidiClip->bank());
					pTrack->setMidiProgram(pMidiClip->program());
				}
				pTrack->addClip(pMidiClip);
				iClipStart += pMidiClip->clipLength();
				// Don't forget to add this one to local repository.
				m_pTracks->mainForm()->addMidiFile(pDropItem->path);
			}
			break;
		}
		case qtractorTrack::None:
		default:
			break;
		}
	}

	// Clean up.
	resetDragState();

	// Refresh view.
	pSession->updateTrack(pTrack);
	if (m_pTracks->session()->updateSessionLength())
		updateContentsWidth();
	updateContents();

	// Don't let this be unnoticed.
	m_pTracks->contentsChangeNotify();
}


// Handle item selection/dragging -- mouse button press.
void qtractorTrackView::contentsMousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Force null state.
	resetDragState();

	if (pMouseEvent->button() == Qt::LeftButton
		|| pMouseEvent->button() == Qt::RightButton) {
		// Which mouse state?
		const bool bModifier = (pMouseEvent->state()
			& (Qt::ShiftButton | Qt::ControlButton));	
		// Remember what and where we'll be dragging/selecting...
		m_dragState = DragStart;
		m_posDrag   = pMouseEvent->pos();
		m_pClipDrag = clipAt(m_posDrag, &m_rectDrag);
		// Should it be selected(toggled)?
		if (m_pClipDrag) {
			QScrollView::setCursor(QCursor(Qt::PointingHandCursor));
			const bool bSelect = !m_pClipDrag->isClipSelected();
			if (bModifier) {
				m_pClipSelect->selectClip(m_pClipDrag, m_rectDrag, bSelect);
				updateContents(m_rectDrag.normalize());
				m_pTracks->selectionChangeNotify();
			} else if (bSelect) {
				m_pClipSelect->clear();
				m_pClipSelect->selectClip(m_pClipDrag, m_rectDrag, true);
				updateContents();
				m_pTracks->selectionChangeNotify();
			}
		}   // Clear any selection out there?
		else if (!bModifier)
			selectAll(false);
	}

	QScrollView::contentsMousePressEvent(pMouseEvent);
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
	case DragSelect:
		drawDragRect(m_rectDrag, 0, 1);	// Hide.
		m_rectDrag.setBottomRight(pos);
		QScrollView::ensureVisible(pos.x(), pos.y(), 8, 8);
		drawDragRect(m_rectDrag, 0, 1);	// Show.
		break;
	case DragStart:
		if ((m_posDrag - pos).manhattanLength()
				> QApplication::startDragDistance()) {
			// We'll start dragging alright...
			if (m_pClipSelect->clips().count() > 0) {
				int x = m_pTracks->session()->pixelSnap(m_rectDrag.x());
				m_iDraggingX = (x - m_rectDrag.x());
				m_dragState = DragMove;
				QScrollView::setCursor(QCursor(Qt::SizeAllCursor));
				drawClipSelect(m_rectDrag, m_iDraggingX);	// Show.
			} else {
				// We'll start rubber banding...
				m_rectDrag.setTopLeft(m_posDrag);
				m_rectDrag.setBottomRight(pos);
				m_dragState = DragSelect;
				QScrollView::setCursor(QCursor(Qt::CrossCursor));
				drawDragRect(m_rectDrag, 0, 1);	// Show.
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

	switch (m_dragState) {
	case DragSelect:
		// Here we're supposed to select a few bunch
		// of clips (all that fall inside the rubber-band...
		selectDragRect(m_rectDrag, (pMouseEvent->state()
			& (Qt::ShiftButton | Qt::ControlButton)) == 0);
		break;
	case DragMove:
		if (m_pClipSelect->clips().count() > 0) {
			qtractorTrack *pNewTrack = dragMoveTrack(pMouseEvent->pos());
			qtractorTrack *pSingleTrack = m_pClipSelect->singleTrack();
			qtractorSession *pSession = m_pTracks->session();
			if (pNewTrack && pSession) {
				qtractorClipSelect::Item *pClipItem
					= m_pClipSelect->clips().first();
				while (pClipItem) {
					qtractorClip  *pClip = pClipItem->pClip;
					qtractorTrack *pOldTrack = pClip->track();
					if (pOldTrack) {
						pOldTrack->unlinkClip(pClip);
						if (pSingleTrack == NULL)
							pNewTrack = pOldTrack;
						else if (pOldTrack != pNewTrack)
							pSession->updateTrack(pOldTrack);
					}
					int x = (pClipItem->rectClip.x() + m_iDraggingX);
					if (x < 0) x = 0;
					pClip->setClipStart(pSession->frameFromPixel(x));
					pNewTrack->addClip(pClip);
					pSession->updateTrack(pNewTrack);
					pClipItem = m_pClipSelect->clips().next();
				}
				m_pClipSelect->clear();
				if (pSession->updateSessionLength())
					updateContentsWidth();
				updateContents();
				m_pTracks->contentsChangeNotify();
			}
		}
		// Fall thru...
	case DragStart:
	case DragDrop:
	case DragNone:
	default:
		break;
	}

	// Force null state.
	resetDragState();
}


// Select everything under a given (rubber-band) rectangle.
void qtractorTrackView::selectDragRect ( const QRect& rectDrag,
	bool bReset )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int iUpdate = 0;
	if (bReset) {
		m_pClipSelect->clear();
		iUpdate++;
	}

	const QRect rect(rectDrag.normalize());

	QRect rectUpdate;
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
					const bool bSelect = (bReset || !pClip->isClipSelected());
					const QRect rectClip(x, y, w, h);
					if (rect.contains(rectClip) || rect.intersects(rectClip)) {
						m_pClipSelect->selectClip(pClip, rectClip, bSelect);
						rectUpdate = rectUpdate.unite(rectClip);
						iUpdate++;
					}
				}
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


// Delete/remove current selection.
void qtractorTrackView::deleteClipSelect (void)
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int iUpdate = 0;
	QRect rectUpdate;
	qtractorClipSelect::Item *pClipItem
		= m_pClipSelect->clips().first();
	while (pClipItem) {
		qtractorClip  *pClip  = pClipItem->pClip;
		qtractorTrack *pTrack = pClip->track();
		if (pTrack) {
			pTrack->removeClip(pClip);
			pSession->updateTrack(pTrack);
		}
		rectUpdate = rectUpdate.unite(pClipItem->rectClip);
		iUpdate++;
		pClipItem = m_pClipSelect->clips().next();
	}

	if (iUpdate > 0) {
		m_pClipSelect->clear();
		updateContents(rectUpdate);
		m_pTracks->contentsChangeNotify();
	}
}


// Select every clip of a given track.
void qtractorTrackView::selectTrack ( qtractorTrack *pTrack,
	bool bReset )
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
	// clear all selections...
	if (m_pClipSelect->clips().count() > 0) {
		m_pClipSelect->clear();
		updateContents();
		m_pTracks->selectionChangeNotify();
	}
}


// Draw/hide the whole current clip selection.
void qtractorTrackView::drawClipSelect ( const QRect& rectDrag, int dx,
	int iThickness ) const
{
	bool bSingleTrack = (m_pClipSelect->singleTrack() != NULL);
	qtractorClipSelect::Item *pClipItem
		= m_pClipSelect->clips().first();
	while (pClipItem) {
		QRect rectClip(pClipItem->rectClip);
		if (bSingleTrack) {
			rectClip.setY(rectDrag.y());
			rectClip.setHeight(rectDrag.height());
		}
		drawDragRect(rectClip, dx, iThickness);
		pClipItem = m_pClipSelect->clips().next();
	}
}


// Draw/hide the current clip selection.
void qtractorTrackView::drawDragRect ( const QRect& rectDrag, int dx,
	int iThickness ) const
{
	QPainter p(QScrollView::viewport());
	QRect rect(rectDrag.normalize());
	const QPoint delta(1, 1);

	// Horizontal adjust...
	rect.moveBy(dx, 0);
	// Convert rectangle into view coordinates...
	rect.moveTopLeft(QScrollView::contentsToViewport(rect.topLeft()));
	// Make sure the rectangle doesn't get too off view,
	// which it would make it sluggish :)
	if (rect.left() < 0)
		rect.setLeft(-(iThickness + 1));
	if (rect.right() > QScrollView::width())
		rect.setRight(QScrollView::width() + iThickness + 1);
	// Now draw it with given thickness...
	p.drawWinFocusRect(rect);
	while (--iThickness > 0) {
		rect.setTopLeft(rect.topLeft() + delta);
		rect.setBottomRight(rect.bottomRight() - delta);
		p.drawWinFocusRect(rect);
	}
}


// Reset drag/select/move state.
void qtractorTrackView::resetDragState (void)
{
	// Cancel any dragging out there...
	if (m_dragState == DragDrop) {
		drawDragRect(m_rectDrag, m_iDraggingX, 1);	// Hide.
		QScrollView::updateContents(m_rectDrag.normalize());
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
				QScrollView::contentsX() - 8,
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
				QScrollView::contentsX() + 8,
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
				QScrollView::contentsY() - 8);
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
				QScrollView::contentsY() + 8);
		}
		break;
	case Qt::Key_Prior:
		if (pKeyEvent->state() & Qt::ControlButton) {
			QScrollView::setContentsPos(
				QScrollView::contentsX(), 0);
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
}


// Session cursor accessor.
qtractorSessionCursor *qtractorTrackView::sessionCursor (void) const
{
	return m_pSessionCursor;
}


// Playhead positioning.
void qtractorTrackView::setPlayhead ( unsigned long iFrame )
{
	if (m_pTracks->session() == NULL)
		return;

	// Update playhead position...
	QPainter p(QScrollView::viewport());
	int x1 = m_pTracks->session()->pixelFromFrame(iFrame);
	int x0 = QScrollView::contentsX();
	int x  = m_iPlayheadX - x0;
	int w  = m_pPixmap->width();
	int h  = m_pPixmap->height();
	int wm = (w >> 3);
	if (x >= 0 && x < w)
		p.drawPixmap(x, 0, *m_pPixmap, x, 0, 1, h);
	if (x1 < x0) {
		// Move backward....
		QScrollView::setContentsPos(x1, QScrollView::contentsY());
	} else if (x1 > x0 + w - wm) {
		// Move forward....
		if (x0 < QScrollView::contentsWidth() - w)
			x0 += (w - wm);
		else
			x0 = QScrollView::contentsWidth() - w;
		QScrollView::setContentsPos(x0, QScrollView::contentsY());
	} else {
		// New position is in...
		x = x1 - x0;
		if (x >= 0 && x < w) {
			p.setPen(Qt::red);
			p.drawLine(x, 0, x, h);
		}
		m_iPlayheadX = x1;
	}
}


int qtractorTrackView::playheadX (void) const
{
	return m_iPlayheadX;
}


// Whether there's any clip currently selected.
bool qtractorTrackView::isClipSelected (void) const
{
	return (m_pClipSelect->clips().count() > 0);
}


// Context menu request slot.
void qtractorTrackView::contentsContextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	m_pTracks->mainForm()->editMenu->exec(pContextMenuEvent->globalPos());
}


// end of qtractorTrackView.cpp
