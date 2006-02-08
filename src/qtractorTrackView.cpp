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
#include "qtractorTracks.h"
#include "qtractorSession.h"

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

	m_clipboard.setAutoDelete(false);
	m_pClipboardSingleTrack = NULL;

	m_iPlayHead  = 0;
	m_iEditHead  = 0;
	m_iEditTail  = 0;

	m_iPlayHeadX = 0;
	m_iEditHeadX = 0;
	m_iEditTailX = 0;

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

	// Reset any current selection out there...
	m_pClipSelect->clear();
	// Do the contents resize thing...
	QScrollView::resizeContents(QScrollView::contentsWidth(), iContentsHeight);
}


// Update track view content width.
void qtractorTrackView::updateContentsWidth (void)
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorTrackView::updateContentsWidth()\n");
#endif

	int iContentsWidth = 0;
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		iContentsWidth = pSession->pixelFromFrame(pSession->sessionLength())
			+ pSession->pixelFromBeat(2 * pSession->beatsPerBar());
	}

#ifdef CONFIG_DEBUG_0
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
		showClipSelect(m_rectDrag, m_iDraggingX);
	} else if (m_dragState == DragDrop && m_rectDrag.intersects(rect)) {
		QScrollView::repaintContents(m_rectDrag.unite(rect));
		showDragRect(m_rectDrag, m_iDraggingX);
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
		showClipSelect(m_rectDrag, m_iDraggingX);
	} else if (m_dragState == DragDrop) {
		QScrollView::repaintContents();
		showDragRect(m_rectDrag, m_iDraggingX);
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

	// Lines a-head...
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int x;

	// Draw edit-head line...
	x = pSession->pixelFromFrame(editHead());
	if (x >= clipx && x < clipx + clipw) {
		p->setPen(Qt::blue);
		p->drawLine(x, clipy, x, clipy + cliph);
	}

	// Draw edit-tail line...
	x = pSession->pixelFromFrame(editTail());
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
		unsigned short iBeat = pSession->beatFromPixel(cx);
#if 0
		unsigned int iPixelsPerBeat = pSession->pixelFromBeat(1);
		int x = pSession->pixelFromBeat(iBeat);
#else
		unsigned long iFrameFromBeat = pSession->frameFromBeat(iBeat);
		unsigned long iFramesPerBeat = pSession->frameFromBeat(1);
		int x = pSession->pixelFromFrame(iFrameFromBeat);
#endif
		while (x < cx + w) {
			if (x >= cx && pSession->beatIsBar(iBeat)) {
				p.setPen(Qt::lightGray);
				p.drawLine(x - cx, 0, x - cx, y2 - cy - 2);
			}
			if (x > cx) {
				p.setPen(Qt::darkGray);
				p.drawLine(x - cx - 1, 0, x - cx - 1, y2 - cy - 2);
			}
#if 0
			x += iPixelsPerBeat;
#else
			iFrameFromBeat += iFramesPerBeat;
			x = pSession->pixelFromFrame(iFrameFromBeat);
#endif
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

	hideClipSelect(m_rectDrag, m_iDraggingX);

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

	showClipSelect(m_rectDrag, m_iDraggingX);

	// OK, we've move it...
	return pTrack;
}


qtractorTrack *qtractorTrackView::dragDropTrack ( QDropEvent *pDropEvent )
{
	// It must be a valid session...
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return NULL;
	
	const QPoint& pos = pDropEvent->pos();

	// If we're already dragging something,
	// find the current pointer track...
	qtractorTrack *pTrack = NULL;

	if (!m_dropItems.isEmpty()) {
		hideDragRect(m_rectDrag, m_iDraggingX);
		// Which track we're pointing at?
		qtractorTrackViewInfo tvi;
		pTrack = trackAt(pos, &tvi);
		// Must be of same type...
		if (pTrack && pTrack->trackType() != m_dropType)
			return NULL;
		// Adjust vertically to target track...
		m_rectDrag.setY(tvi.trackRect.y() + 1);
		m_rectDrag.setHeight(tvi.trackRect.height() - 2);
		// Always change horizontally wise...
		int  x = m_rectDrag.x();
		int dx = (pos.x() - m_posDrag.x());
		if (x + dx < 0)
			dx = -(x);	// Force to origin (x=0).
		m_iDraggingX = (pSession->pixelSnap(x + dx) - x);
		QScrollView::ensureVisible(pos.x(), pos.y(), 8, 8);
		showDragRect(m_rectDrag, m_iDraggingX);
		// OK, we've move it...
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

	// Nows time to estimate the drag-rectangle width,
	// as we'll take the largest...
	int w = 0;
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
		// First test as a MIDI file...
		if (m_dropType == qtractorTrack::None
			|| m_dropType == qtractorTrack::Midi) {
			qtractorMidiFile file;
			if (file.open(pDropItem->path)) {
				qtractorMidiSequence seq;
				seq.setTicksPerBeat(pSession->ticksPerBeat());
				if (file.readTrack(&seq, pDropItem->channel)) {
					w += pSession->pixelFromTick(seq.duration());
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Midi;
				} else if (m_dropType == qtractorTrack::Midi) {
					m_dropItems.remove(pDropItem);
				}
				file.close();
			} 
			else if (m_dropType == qtractorTrack::Midi) {
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
					w += pSession->pixelFromFrame(iFrames);
					if (m_dropType == qtractorTrack::None)
						m_dropType = qtractorTrack::Audio;
				} else if (m_dropType == qtractorTrack::Audio) {
					m_dropItems.remove(pDropItem);
				}
				delete pFile;
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

	// Nice, now we'll try to set a preview selection rectangle...
	qtractorTrackViewInfo tvi;
	pTrack = trackAt(pos, &tvi);
	m_posDrag.setX(pSession->pixelSnap(pos.x() - 8));
	m_posDrag.setY(tvi.trackRect.y() + 1);
	m_rectDrag.setRect(
		m_posDrag.x(), m_posDrag.y(), w, tvi.trackRect.height() - 2);
	// However, track must be of proper type...
	if (pTrack && pTrack->trackType() != m_dropType)
		return NULL;

	// Finally, show it to the world...
	showDragRect(m_rectDrag, m_iDraggingX);
	// Done.
	return pTrack;
}


// Drag enter event handler.
void qtractorTrackView::contentsDragEnterEvent (
	QDragEnterEvent *pDragEnterEvent )
{
	qtractorTrack *pTrack = dragDropTrack(pDragEnterEvent);
	if (pTrack || !m_dropItems.isEmpty()) {
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
	if (pTrack || !m_dropItems.isEmpty()) {
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
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL) {
		resetDragState();
		return;
	}

	// Add new clips on proper and consecutive track locations...
	unsigned long iClipStart
		= pSession->frameFromPixel(m_rectDrag.x() + m_iDraggingX);

	// Now check whether the drop is intra-track...
	qtractorTrack *pTrack = dragDropTrack(pDropEvent);
	if (pTrack == NULL) {
		// Do we have something tpo drop anyway?
		// if yes, this is a extra-track drop...
		if (!m_dropItems.isEmpty()) {
			// Prepare file list for import...
			QStringList files;
			for (DropItem *pDropItem = m_dropItems.first();
					pDropItem; pDropItem = m_dropItems.next()) {
				files.append(pDropItem->path);
			}
			// Depending on import type...
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
		resetDragState();
		return;
	}

	// We'll build a composite command...
	qtractorAddClipCommand *pAddClipCommand
		= new qtractorAddClipCommand(m_pTracks->mainForm());

	// Nows time to create the clip(s)...
	for (DropItem *pDropItem = m_dropItems.first();
			pDropItem; pDropItem = m_dropItems.next()) {
		switch (pTrack->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
			if (pAudioClip) {
				pAudioClip->setClipStart(iClipStart);
				pAudioClip->open(pDropItem->path);
				pAddClipCommand->addClip(pAudioClip, pTrack, iClipStart);
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
				pAddClipCommand->addClip(pMidiClip, pTrack, iClipStart);
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

	// Put it in the form of an undoable command...
	m_pTracks->mainForm()->commands()->exec(pAddClipCommand);
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
		const QPoint& pos = pMouseEvent->pos();
		// Remember what and where we'll be dragging/selecting...
		m_dragState = DragStart;
		m_posDrag   = pos;
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
		} else {
			// Clear any selection out there?
			if (!bModifier)
				selectAll(false);
			qtractorSession *pSession = m_pTracks->session();
			if (pSession) {
				// Direct snap positioning...
				unsigned long iFrame = pSession->frameSnap(
					pSession->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
				if (pMouseEvent->button() == Qt::LeftButton) {
					if (bModifier) {
						// First, set actual engine position...
						pSession->setPlayHead(iFrame);
						// Playhead positioning...
						setPlayHead(iFrame);
						// Not quite a selection, but for
						// immediate visual feedback...
						m_pTracks->selectionChangeNotify();
					}
				}	// RightButton!...
				else if (!bModifier) {
					// Edittail positioning...
					setEditTail(iFrame);
					// Nothing more...
					m_dragState = DragNone;
				}
			}
		}
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
		hideDragRect(m_rectDrag, 0);
		m_rectDrag.setBottomRight(pos);
		QScrollView::ensureVisible(pos.x(), pos.y(), 8, 8);
		showDragRect(m_rectDrag, 0, 1);
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
				showClipSelect(m_rectDrag, m_iDraggingX);
			} else {
				// We'll start rubber banding...
				m_rectDrag.setTopLeft(m_posDrag);
				m_rectDrag.setBottomRight(pos);
				m_dragState = DragSelect;
				QScrollView::setCursor(QCursor(Qt::CrossCursor));
				showDragRect(m_rectDrag, 0, 1);
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
		// Which mouse state?
		const bool bModifier = (pMouseEvent->state()
			& (Qt::ShiftButton | Qt::ControlButton));
		switch (m_dragState) {
		case DragSelect:
			// Here we're mainly supposed to select a few bunch
			// of clips (all that fall inside the rubber-band...
			selectDragRect(m_rectDrag, !bModifier);
			// That's nice but we'll also set edit-range positioning...
			if (!bModifier) {
				const QRect rect(m_rectDrag.normalize());
				setEditHead(pSession->frameSnap(
					pSession->frameFromPixel(rect.left())));
				setEditTail(pSession->frameSnap(
					pSession->frameFromPixel(rect.right())));
			}
			break;
		case DragMove:
			if (m_pClipSelect->clips().count() > 0) {
				qtractorTrack *pNewTrack = dragMoveTrack(pMouseEvent->pos());
				bool bSingleTrack = (m_pClipSelect->singleTrack() != NULL);
				if (pNewTrack) {
					// We'll build a composite command...
					qtractorMoveClipCommand *pMoveClipCommand
						= new qtractorMoveClipCommand(m_pTracks->mainForm());
					qtractorClipSelect::Item *pClipItem
						= m_pClipSelect->clips().first();
					while (pClipItem) {
						qtractorClip *pClip = pClipItem->clip;
						if (!bSingleTrack)
							pNewTrack = pClip->track();
						int x = (pClipItem->rectClip.x() + m_iDraggingX);
						pMoveClipCommand->addClip(pClip, pNewTrack,
							pSession->frameFromPixel(x < 0 ? 0 : x));
						pClipItem = m_pClipSelect->clips().next();
					}
					m_pClipSelect->clear();
					// Put it in the form of an undoable command...
					m_pTracks->mainForm()->commands()->exec(pMoveClipCommand);
				}
			}
			break;
		case DragStart:
			// Deferred left-button edit-head positioning...
			if (!bModifier) {
				setEditHead(pSession->frameSnap(
					pSession->frameFromPixel(
						m_posDrag.x() > 0 ? m_posDrag.x() : 0)));
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

	// Check if anything is really selected...
	if (m_pClipSelect->clips().count() < 1)
		return;

	// We'll build a composite command...
	qtractorRemoveClipCommand *pRemoveClipCommand
		= new qtractorRemoveClipCommand(m_pTracks->mainForm());

	qtractorClipSelect::Item *pClipItem = m_pClipSelect->clips().first();
	while (pClipItem) {
		pRemoveClipCommand->addClip(
			pClipItem->clip, (pClipItem->clip)->track(), 0);
		pClipItem = m_pClipSelect->clips().next();
	}

	m_pClipSelect->clear();

	// Put it in the form of an undoable command...
	m_pTracks->mainForm()->commands()->exec(pRemoveClipCommand);
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
void qtractorTrackView::showClipSelect ( const QRect& rectDrag, int dx,
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
		showDragRect(rectClip, dx, iThickness);
		pClipItem = m_pClipSelect->clips().next();
	}
}

void qtractorTrackView::hideClipSelect ( const QRect& rectDrag, int dx )
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
		hideDragRect(rectClip, dx);
		pClipItem = m_pClipSelect->clips().next();
	}
}


// Draw/hide the current clip selection.
void qtractorTrackView::showDragRect ( const QRect& rectDrag, int dx,
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

void qtractorTrackView::hideDragRect ( const QRect& rectDrag, int dx )
{
	QRect rect(rectDrag.normalize());

	// Horizontal adjust...
	rect.moveBy(dx, 0);
	QScrollView::repaintContents(rect);
}


// Reset drag/select/move state.
void qtractorTrackView::resetDragState (void)
{
	// Cancel any dragging out there...
	if (m_dragState == DragDrop) {
		hideDragRect(m_rectDrag, m_iDraggingX);
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
		if (iPositionX != x2)
			p.drawPixmap(x1, 0, *m_pPixmap, x1, 0, 1, h);
		// Update the time-line header...
		m_pTracks->trackTime()->updateContents(QRect(x0 + x1 - d2, d2, h2, d2));
	}

	// New position is in...
	iPositionX = x;

	// Force position to be in view?
	if (bSyncView && x < x0) {
		// Move backward....
		QScrollView::setContentsPos(x, QScrollView::contentsY());
	} else if (bSyncView && x > x0 + w - wm) {
		// Move forward....
		if (x0 < QScrollView::contentsWidth() - w)
			x0 += (w - wm);
		else
			x0 = QScrollView::contentsWidth() - w;
		QScrollView::setContentsPos(x0, QScrollView::contentsY());
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
		m_iEditHead = iEditHead;
		if (iEditHead > m_iEditTail)
			setEditTail(iEditHead);
		int iEditHeadX = pSession->pixelFromFrame(iEditHead);
		drawPositionX(m_iEditHeadX, iEditHeadX, m_iEditTailX, Qt::blue);
	}
}

unsigned long qtractorTrackView::editHead (void) const
{
	return m_iEditHead;
}


// Edit-tail positioning
void qtractorTrackView::setEditTail ( unsigned long iEditTail )
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession) {
		m_iEditTail = iEditTail;
		if (iEditTail < m_iEditHead)
			setEditHead(iEditTail);
		int iEditTailX = pSession->pixelFromFrame(iEditTail);
		drawPositionX(m_iEditTailX, iEditTailX, m_iEditHeadX, Qt::blue);
	}
}

unsigned long qtractorTrackView::editTail (void) const
{
	return m_iEditTail;
}


// Whether there's any clip currently selected.
bool qtractorTrackView::isClipSelected (void) const
{
	return (m_pClipSelect->clips().count() > 0);
}


// Whether there's any clip on clipboard.
bool qtractorTrackView::isClipboardEmpty (void) const
{
	return (m_clipboard.count() < 1);
}


// Clear clipboard.
void qtractorTrackView::clearClipboard (void)
{
	m_clipboard.clear();
	m_pClipboardSingleTrack = NULL;
}


// Copy current selected clips to clipboard.
void qtractorTrackView::copyClipSelect (void)
{
	// Reset clipboard...
	m_pClipboardSingleTrack = m_pClipSelect->singleTrack();
	m_clipboard.clear();

	// Copy each selected clip to clipboard...
	qtractorClipSelect::Item *pClipItem = m_pClipSelect->clips().first();
	while (pClipItem) {
		m_clipboard.append(pClipItem->clip);
		pClipItem = m_pClipSelect->clips().next();
	}
}


// Copy current selected clips to clipboard.
void qtractorTrackView::cutClipSelect (void)
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Check if anything is really selected...
	if (m_pClipSelect->clips().count() < 1)
		return;

	// Reset clipboard...
	m_pClipboardSingleTrack = m_pClipSelect->singleTrack();
	m_clipboard.clear();

	// We'll build a composite command...
	qtractorRemoveClipCommand *pRemoveClipCommand
		= new qtractorRemoveClipCommand(m_pTracks->mainForm());
	// Override default command name.
	pRemoveClipCommand->setName(tr("cut clip"));

	qtractorClipSelect::Item *pClipItem = m_pClipSelect->clips().first();
	while (pClipItem) {
		m_clipboard.append(pClipItem->clip);
		pRemoveClipCommand->addClip(
			pClipItem->clip, (pClipItem->clip)->track(), 0);
		pClipItem = m_pClipSelect->clips().next();
	}

	m_pClipSelect->clear();

	// Put it in the form of an undoable command...
	m_pTracks->mainForm()->commands()->exec(pRemoveClipCommand);
}


// Paste from clipboard.
void qtractorTrackView::pasteClipSelect (void)
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	// Check if anythings really on clipboard...
	if (m_clipboard.count() < 1)
		return;

	// Take hand on where we wish to start pasting clips...
	qtractorTrack *pPasteTrack = m_pTracks->currentTrack();
	bool bSingleTrack = (m_pClipboardSingleTrack && pPasteTrack
		&& m_pClipboardSingleTrack->trackType() == pPasteTrack->trackType());

	// We'll build a composite command...
	qtractorAddClipCommand *pAddClipCommand
		= new qtractorAddClipCommand(m_pTracks->mainForm());
	// Override default command name.
	pAddClipCommand->setName(tr("paste clip"));

	qtractorClip *pClip = m_clipboard.first();
	long delta = (pClip ? m_iEditHead - pClip->clipStart() : 0);
	while (pClip) {
		if (!bSingleTrack)
			pPasteTrack = pClip->track();
		unsigned long iClipStart = pClip->clipStart();
		if ((long) iClipStart + delta > 0)
			iClipStart += delta;
		else
			iClipStart = 0;
		// Now, its imperative to make a proper copy of those clips...
		switch (pPasteTrack->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip
				= static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip) {
				pAudioClip = new qtractorAudioClip(*pAudioClip);
				pAudioClip->setClipStart(iClipStart);
				pAddClipCommand->addClip(pAudioClip, pPasteTrack, iClipStart);
			}
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip) {
				pMidiClip = new qtractorMidiClip(*pMidiClip);
				pMidiClip->setClipStart(iClipStart);
				pAddClipCommand->addClip(pMidiClip, pPasteTrack, iClipStart);
			}
			break;
		}
		default:
			break;
		}
		pClip = m_clipboard.next();
	}

	// Put it in the form of an undoable command...
	m_pTracks->mainForm()->commands()->exec(pAddClipCommand);
}


// Context menu request slot.
void qtractorTrackView::contentsContextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	m_pTracks->mainForm()->editMenu->exec(pContextMenuEvent->globalPos());
}


// end of qtractorTrackView.cpp
