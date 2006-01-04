// qtractorTrackView.h
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

#ifndef __qtractorTrackView_h
#define __qtractorTrackView_h

#include "qtractorTrack.h"

#include <qscrollview.h>

// Forward declarations.
class qtractorTracks;
class qtractorClipSelect;
class qtractorMidiSequence;
class qtractorSessionCursor;
class qtractorTrackListItem;

class QToolButton;


//----------------------------------------------------------------------------
// qtractorTrackViewInfo -- Track view state info.

struct qtractorTrackViewInfo
{
	int           trackIndex;   // Track index.
	unsigned long trackStart;   // First track frame on view.
	unsigned long trackEnd;     // Last track frame on view.
	QRect         trackRect;    // The track view rectangle.
};


//----------------------------------------------------------------------------
// qtractorTrackView -- Track view widget.

class qtractorTrackView : public QScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackView(qtractorTracks *pTracks,
		QWidget *pParent, const char *pszName = 0);
	// Destructor.
	~qtractorTrackView();

	// Update track view content height.
	void updateContentsHeight();
	// Update track view content width.
	void updateContentsWidth();

	// Contents update overloaded methods.
	void updateContents(const QRect& rect);
	void updateContents();

	// Select every clip of a given track.
	void selectTrack(qtractorTrack *pTrack, bool bReset = true);
	// Select all contents.
	void selectAll(bool bSelect = true);

	// Whether there's any clip currently selected.
	bool isClipSelected() const;

	// Whether there's any clip on clipboard.
	bool isClipboardEmpty() const;

	// Clipboard cleaner.
	void clearClipboard();

	// Clipboard methods.
	void copyClipSelect();
	void cutClipSelect();
	void pasteClipSelect();

	// Delete/remove current selection.
	void deleteClipSelect();

	// Playhead positioning.
	void setPlayhead(unsigned long iFrame);
	int playheadX() const;

	// Edithead positioning.
	void setEdithead(unsigned long iFrame);
	int editheadX() const;

	// Current session cursor accessor.
	qtractorSessionCursor *sessionCursor() const;

protected:

	// Scrollbar/tools layout management.
	void setHBarGeometry(QScrollBar& hbar,
		int x, int y, int w, int h);
	void setVBarGeometry(QScrollBar& vbar,
		int x, int y, int w, int h);

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the track view
	void drawContents(QPainter *p,
		int clipx, int clipy, int clipw, int cliph);

	// Get track list item from given contents vertical position.
	qtractorTrackListItem *trackListItemAt(const QPoint& pos,
		qtractorTrackViewInfo *pTrackViewInfo) const;
	// Get track from given contents vertical position.
	qtractorTrack *trackAt(const QPoint& pos,
		qtractorTrackViewInfo *pTrackViewInfo = NULL) const;
	// Get clip from given contents position.
	qtractorClip *clipAt(const QPoint& pos,	QRect *pClipRect = NULL) const;

	// Get contents visible rectangle from given track.
	bool trackInfo(qtractorTrack *pTrack,
		qtractorTrackViewInfo *pTrackViewInfo) const;
	// Get contents rectangle from given clip.
	bool clipInfo(qtractorClip *pClip, QRect *pClipRect) const;

	// Drag-n-drop event stuffer.
	qtractorTrack *dragMoveTrack(const QPoint& pos);
	qtractorTrack *dragDropTrack(QDropEvent *pDropEvent);

	// Drag-n-drop event handlers.
	void contentsDragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void contentsDragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void contentsDragLeaveEvent(QDragLeaveEvent *pDragLeaveEvent);
	void contentsDropEvent(QDropEvent *pDropEvent);

	// Handle item selection with mouse.
	void contentsMousePressEvent(QMouseEvent *pMouseEvent);
	void contentsMouseMoveEvent(QMouseEvent *pMouseEvent);
	void contentsMouseReleaseEvent(QMouseEvent *pMouseEvent);
	
	// Select everything under a given (rubber-band) rectangle.
	void selectDragRect(const QRect& rectDrag, bool bReset = false);

	// Draw/hide the whole current clip selection.
	void drawClipSelect(const QRect& rectDrag, int dx,
		int iThickness = 3) const;

	// Draw/hide the current clip selection.
	void drawDragRect(const QRect& rectDrag, int dx,
		int iThickness = 3) const;

	// Reset drag/select/move state.
	void resetDragState();

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Vertical line positioning.
	void drawPositionX(int& iPositionX,
		unsigned long iFrame, const QColor& color);

protected slots:

	// To have track view in v-sync with track list.
	void contentsMovingSlot(int cx, int cy);

	// Context menu request slot.
	void contentsContextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// (Re)create the complete track view pixmap.
	void updatePixmap(int cx, int cy);

private:

	// The logical parent binding.
	qtractorTracks *m_pTracks;

	// Local zoom control widgets.
	QToolButton *m_pHzoomIn;
	QToolButton *m_pHzoomOut;
	QToolButton *m_pVzoomIn;
	QToolButton *m_pVzoomOut;
	QToolButton *m_pXzoomTool;

	// Local double-buffering pixmap.
	QPixmap *m_pPixmap;

	// To maintain the current track/clip positioning.
	qtractorSessionCursor *m_pSessionCursor;

	// Drag-n-dropping stuff.
	struct DropItem
	{
		// Item constructor.
		DropItem(const QString& sPath, unsigned short iChannel = 0)
			: path(sPath), channel(iChannel) {}
		// Item members.
		QString path;
		unsigned short channel;
	};

	QPtrList<qtractorTrackView::DropItem> m_dropItems;
	qtractorTrack::TrackType m_dropType;

	// The current selecting/dragging clip stuff.
	enum DragState {
		DragNone = 0, DragStart, DragSelect, DragMove, DragDrop
	} m_dragState;

	int           m_iDraggingX;
	QRect         m_rectDrag;
	QPoint        m_posDrag;
	qtractorClip *m_pClipDrag;

	qtractorClipSelect *m_pClipSelect;

	// The local clipboard stuff.
	QPtrList<qtractorClip> m_clipboard;
	qtractorTrack *m_pClipboardSingleTrack;

	// Playhead positioning.
	int m_iPlayheadX;
	// Edithead positioning.
	int m_iEditheadX;
};


#endif  // __qtractorTrackView_h


// end of qtractorTrackView.h
