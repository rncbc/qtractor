// qtractorTrackView.h
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

#ifndef __qtractorTrackView_h
#define __qtractorTrackView_h

#include "qtractorScrollView.h"
#include "qtractorRubberBand.h"

#include "qtractorTrack.h"

#include <QPixmap>


// Forward declarations.
class qtractorTracks;
class qtractorClipSelect;
class qtractorMidiSequence;
class qtractorSessionCursor;
class qtractorTrackListItem;

class QToolButton;

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;
class QResizeEvent;
class QKeyEvent;


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

class qtractorTrackView : public qtractorScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackView(qtractorTracks *pTracks, QWidget *pParent = 0);
	// Destructor.
	~qtractorTrackView();

	// Track view state reset.
	void clear();

	// Update track view content height.
	void updateContentsHeight();
	// Update track view content width.
	void updateContentsWidth(int iContentsWidth = 0);

	// Contents update overloaded methods.
	void updateContents(const QRect& rect);
	void updateContents();

	// Special recording visual feedback.
	void updateContentsRecord();

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
	void selectTrack(qtractorTrack *pTrackPtr, bool bReset = true);
	// Select all contents.
	void selectAll(bool bSelect = true);

	// Whether there's any clip currently editable.
	qtractorClip *currentClip() const;

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

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the track view
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Get track from given contents vertical position.
	qtractorTrack *trackAt(const QPoint& pos,
		qtractorTrackViewInfo *pTrackViewInfo = NULL) const;
	// Get clip from given contents position.
	qtractorClip *clipAt(const QPoint& pos,	QRect *pClipRect = NULL) const;

	// Get contents visible rectangle from given track.
	bool trackInfo(qtractorTrack *pTrackPtr,
		qtractorTrackViewInfo *pTrackViewInfo) const;
	// Get contents rectangle from given clip.
	bool clipInfo(qtractorClip *pClip, QRect *pClipRect) const;

	// Drag-n-drop event stuffer.
	qtractorTrack *dragMoveTrack(const QPoint& pos);
	qtractorTrack *dragDropTrack(QDropEvent *pDropEvent);
	bool canDropTrack(QDropEvent *pDropEvent);

	// Drag-n-drop event handlers.
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *pDragLeaveEvent);
	void dropEvent(QDropEvent *pDropEvent);

	// Handle item selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Handle item/clip editing from mouse.
	void mouseDoubleClickEvent(QMouseEvent *pMouseEvent);

	// Clip file(item) selection convenience method.
	void selectClipFile(bool bReset);

	// Draw/hide the whole current clip selection.
	void updateClipSelect(int y, int h) const;
	void showClipSelect() const;
	void hideClipSelect() const;

	// Draw/hide the whole drop rectagle list
	void updateDropRects(int y, int h) const;
	void showDropRects() const;
	void hideDropRects() const;

	// Draw/hide a dragging rectangular selection.
	void moveRubberBand(qtractorRubberBand **ppRubberBand,
		const QRect& rectDrag, int thick = 1) const;

	// Clip fade-in/out handle drag-move methods.
	bool dragFadeStart(const QPoint& pos);
	void dragFadeMove(const QPoint& pos);
	void dragFadeDrop(const QPoint& pos);

	// Reset drag/select/move state.
	void resetDragState();

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Vertical line positioning.
	void drawPositionX(int& iPositionX, int x, bool bSyncView = false);

protected slots:

	// To have track view in v-sync with track list.
	void contentsYMovingSlot(int cx, int cy);

	// (Re)create the complete track view pixmap.
	void updatePixmap(int cx, int cy);

	// Drag-reset timer slot.
	void dragTimeout();

private:

	// The logical parent binding.
	qtractorTracks *m_pTracks;

	// Local zoom control widgets.
	QToolButton *m_pHzoomIn;
	QToolButton *m_pHzoomOut;
	QToolButton *m_pVzoomIn;
	QToolButton *m_pVzoomOut;
	QToolButton *m_pXzoomReset;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// To maintain the current track/clip positioning.
	qtractorSessionCursor *m_pSessionCursor;

	// Drag-n-dropping stuff.
	struct DropItem
	{
		// Item constructor.
		DropItem(const QString& sPath, int iChannel = -1)
			: path(sPath), channel(iChannel), rubberBand(NULL) {}
		// Item destructor.
		~DropItem() {
			if (rubberBand) { 
				rubberBand->hide();
				delete rubberBand;
			}
		}
		// Item members.
		QString path;
		int channel;
		QRect rect;
		qtractorRubberBand *rubberBand;
	};

	QList<qtractorTrackView::DropItem *> m_dropItems;
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
	bool          m_bDragTimer;

	qtractorRubberBand *m_pRubberBand;

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
		ClipBoard() : singleTrack(NULL) {}
		// Clipboard stuffer method.
		void addItem(qtractorClip *pClip, unsigned long iClipStart,
			unsigned long iClipOffset, unsigned long iClipLength);
		// Clipboard reset method.
		void clear();
		// Clipboard members.
		QList<ClipItem *> items;
		qtractorTrack    *singleTrack;
		QRect             rect;

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
