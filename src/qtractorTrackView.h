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
	void updateContentsWidth(int iContentsWidth = 0);

	// Contents update overloaded methods.
	void updateContents(const QRect& rect, bool bRefresh = true);
	void updateContents(bool bRefresh = true);
	// Special recording visual feedback.
	void updateContentsRecord(bool bRefresh = false);

	// The current clip selection mode.
	enum SelectMode { SelectClip, SelectRange, SelectRect };
	enum SelectEdit { EditNone = 0, EditHead = 1, EditTail = 2, EditBoth = 3 };

	// Clip selection mode accessors.
	void setSelectMode(SelectMode selectMode);
	SelectMode selectMode() const;

	// Select everything under a given (rubber-band) rectangle.
	void selectRect(const QRect& rectDrag,
		SelectMode selectMode, SelectEdit = EditBoth);
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

	// Clip selection command types.
	enum Command { Cut, Copy, Delete };
	
	// Clip selection executive method.
	void executeClipSelect(qtractorTrackView::Command cmd);

	// Clipboard paste method.
	void pasteClipSelect();

	// Intra-drag-n-drop clip move method.
	void moveClipSelect(qtractorTrack *pTrack, int dx);

	// Play-head positioning.
	void setPlayHead(unsigned long iPlayHead, bool bSyncView = false);
	unsigned long playHead() const;

	// Edit-head/tail positioning.
	void setEditHead(unsigned long iEditHead);
	void setEditTail(unsigned long iEditTail);

	// Make given frame position visible in view.
	void ensureVisibleFrame(unsigned long iFrame);

	// Current session cursor accessor.
	qtractorSessionCursor *sessionCursor() const;

	// Clip cloner helper.
	static qtractorClip *cloneClip(qtractorClip *pClip);

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
	bool canDropTrack(QDropEvent *pDropEvent);

	// Drag-n-drop event handlers.
	void contentsDragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void contentsDragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void contentsDragLeaveEvent(QDragLeaveEvent *pDragLeaveEvent);
	void contentsDropEvent(QDropEvent *pDropEvent);

	// Handle item selection with mouse.
	void contentsMousePressEvent(QMouseEvent *pMouseEvent);
	void contentsMouseMoveEvent(QMouseEvent *pMouseEvent);
	void contentsMouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Clip file(item) selection convenience method.
	void selectClipFile(bool bReset);

	// Draw/hide the whole current clip selection.
	void updateClipSelect(int y, int h);
	void showClipSelect(int iThickness = 3) const;
	void hideClipSelect();

	// Draw/hide the whole drop rectagle list
	void updateDropRects(int y, int h);
	void showDropRects(int iThickness = 3);
	void hideDropRects();

	// Draw/hide a dragging rectangular selection.
	void showDragRect(const QRect& rectDrag, int iThickness) const;
	void hideDragRect(const QRect& rectDrag);

	// Clip fade-in/out handle drag-move methods.
	bool dragFadeStart(const QPoint& pos);
	void dragFadeMove(const QPoint& pos);
	void dragFadeDrop(const QPoint& pos);

	// Show/hide a moving clip fade in/out handle.
	void drawFadeHandle() const;

	// Reset drag/select/move state.
	void resetDragState();

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Vertical line positioning.
	void drawPositionX(int& iPositionX, int x, int x2,
		const QColor& color, bool bSyncView = false);

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
		DropItem(const QString& sPath, int iChannel = -1)
			: path(sPath), channel(iChannel) {}
		// Item members.
		QString path;
		int channel;
		QRect rect;
	};

	QPtrList<qtractorTrackView::DropItem> m_dropItems;
	qtractorTrack::TrackType m_dropType;

	// The current selecting/dragging clip stuff.
	enum DragState {
		DragNone = 0, DragStart,
		DragSelect, DragMove, DragDrop,
		DragFadeIn, DragFadeOut
	} m_dragState;

	qtractorClip *m_pClipDrag;
	QPoint        m_posDrag;
	QRect         m_rectDrag;
	QRect         m_rectHandle;
	int           m_iDraggingX;

	qtractorClipSelect *m_pClipSelect;

	// The clip select mode.
	SelectMode m_selectMode;

	// The local clipboard item.
	struct ClipItem
	{
		// Clipboard item constructor.
		ClipItem(qtractorClip *pClip, unsigned long iClipStart,
			unsigned long iClipOffset, unsigned long iClipLength)
			: clip(pClip), clipStart(iClipStart),
				clipOffset(iClipOffset), clipLength(iClipLength),
				fadeInLength(0), fadeOutLength(0) {}
		// Clipboard item members.
		qtractorClip *clip;
		unsigned long clipStart;
		unsigned long clipOffset;
		unsigned long clipLength;
		unsigned long fadeInLength;
		unsigned long fadeOutLength;
	};

	// The local clipboard stuff.
	struct ClipBoard
	{
		// Clipbaord constructor.
		ClipBoard() : singleTrack(NULL)
			{ items.setAutoDelete(false); }
		// Clipboard stuffer method.
		void addItem(qtractorClip *pClip, unsigned long iClipStart,
			unsigned long iClipOffset, unsigned long iClipLength);
		// Clipboard reset method.
		void clear();
		// Clipboard members.
		QPtrList<ClipItem> items;
		qtractorTrack     *singleTrack;
		QRect              rect;

	} m_clipboard;

	// Playhead and edit frame positioning.
	unsigned long m_iPlayHead;

	// Playhead and edit shadow pixel positioning.
	int m_iPlayHeadX;
	int m_iEditHeadX;
	int m_iEditTailX;

	// Recording state window.
	int m_iLastRecordX;
};


#endif  // __qtractorTrackView_h


// end of qtractorTrackView.h
