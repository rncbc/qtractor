// qtractorTrackView.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorCurve.h"
#include "qtractorTempoCurve.h"
#include "qtractorSpinBox.h"

#include <QPixmap>
#include <QBrush>


// Forward declarations.
class qtractorTracks;
class qtractorClipSelect;
class qtractorCurveSelect;
class qtractorTempoCurveSelect;
class qtractorMidiSequence;
class qtractorSessionCursor;
class qtractorTrackListItem;

class qtractorCurveEditCommand;
class qtractorTempoCurveEditCommand;

class QToolButton;
class QDoubleSpinBox;

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;
class QResizeEvent;
class QKeyEvent;

class QMimeData;
class QCursor;


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

	void trackTempoCurveShow(qtractorSession *pSession, qtractorTrack *pTrack);

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

	// Selection flags
	enum {
		SelectNone   = 0,
		SelectClear  = 1,
		SelectToggle = 2,
		SelectCommit = 4
	};

	// Convert selection flags from keyboard modifiers.
	static int selectFlags(const Qt::KeyboardModifiers modifiers);

	// Clip selection mode accessors.
	void setSelectMode(SelectMode selectMode);
	SelectMode selectMode() const;

	// Select every clip under a given (rubber-band) rectangle.
	void selectClipRect(const QRect& rectDrag,
		SelectMode selectMode, int flags, SelectEdit selectEdit = EditNone);

	// Select every clip of a given track-range.
	void selectClipTrackRange(qtractorTrack *pTrackPtr, bool bReset = true);
	// Select every clip of a given track.
	void selectClipTrack(qtractorTrack *pTrackPtr, bool bReset = true);

	// Select all clip contents.
	void selectClipAll();
	// Invert selection on all tracks and clips.
	void selectClipInvert();

	// Select all clips of given filename and track/channel.
	void selectClipFile(qtractorTrack::TrackType trackType,
		const QString& sFilename, int iTrackChannel, bool bSelect);

	// Select curve nodes under a given (rubber-band) rectangle.
	void selectCurveRect(const QRect& rectDrag,
		SelectMode selectMode, int flags, SelectEdit selectEdit = EditNone);

	// Select every current curve/automation node of a given track-range.
	void selectCurveTrackRange(qtractorTrack *pTrackPtr, bool bReset = true);
	// Select every current curve/automation node of a given track.
	void selectCurveTrack(qtractorTrack *pTrackPtr, bool bReset = true);

	// Select all current track curve/automation nodes.
	void selectCurveAll();
	// Invert selection on current track curve/automation nodes.
	void selectCurveInvert();

	// Contents update overloaded methods.
	void updateRect(const QRect& rect);

	// Whether there's any clip currently editable.
	qtractorClip *currentClip() const;

	// Clip selection accessor.
	qtractorClipSelect *clipSelect() const;

	// Whether there's any clip currently selected.
	bool isClipSelected() const;

	// Whether there's any curve/automation currently selected.
	bool isCurveSelected() const;

	// Whether there's a single track selection.
	qtractorTrack *singleTrackSelected();

	// Clear current selection (no notify).
	void clearSelect(bool bReset = false);

	// Update current selection (no notify).
	void updateSelect();

	// Whether there's anything on clipboard.
	static bool isClipboard();

	// Paste from clipboard (start).
	void pasteClipboard(
		unsigned short iPasteCount = 1, unsigned long iPastePeriod = 0);

	// Retrieve current paste period.
	// (as from current clipboard width)
	unsigned long pastePeriod() const;

	// Clip selection command types.
	enum Command { Cut, Copy, Delete, Split };
	
	// Clip selection executive method.
	void executeClipSelect(
		qtractorTrackView::Command cmd, qtractorClip *pClip = NULL);

	// Intra-drag-n-drop clip move method.
	void moveClipSelect(qtractorTrack *pTrack);

	// Paste from clipboard (execute).
	void pasteClipSelect(qtractorTrack *pTrack);

	// Curve/automation selection executive method.
	void executeCurveSelect(qtractorTrackView::Command cmd);

	// Intra-drag-n-drop curve/automation node move method.
	void moveCurveSelect(const QPoint& pos);

	// Paste from clipboard (execute).
	void pasteCurveSelect(const QPoint& pos);

	// Play-head positioning.
	void setPlayHead(unsigned long iPlayHead, bool bSyncView = false);
	int playHeadX() const;

	// Auto-backwatrd interim play-head positioning.
	void setPlayHeadAutoBackward(unsigned long iPlayHead);
	int playHeadAutoBackwardX() const;

	// Edit-head/tail positioning.
	void setEditHead(unsigned long iEditHead);
	int editHeadX() const;

	void setEditTail(unsigned long iEditTail);
	int editTailX() const;

	// Make given contents position visible in view.
	void ensureVisible(int cx, int cy, int mx, int my);

	// Make given frame position visible in view.
	void ensureVisibleFrame(unsigned long iFrame);

	// Current session cursor accessor.
	qtractorSessionCursor *sessionCursor() const;

	// Clip cloner helper.
	static qtractorClip *cloneClip(qtractorClip *pClip);

	// Multi-item drop mode (whether to span clips horixontally).
	void setDropSpan(bool bDropSpan);
	bool isDropSpan() const;

	// Snap-to-bar zebra mode.
	void setSnapZebra(bool bSnapZebra);
	bool isSnapZebra() const;

	// Snap-to-beat grid mode.
	void setSnapGrid(bool bSnapGrid);
	bool isSnapGrid() const;

	// Floating tool-tips mode.
	void setToolTips(bool bToolTips);
	bool isToolTips() const;

	// Automation curve node editing mode.
	void setCurveEdit(bool bCurveEdit);
	bool isCurveEdit() const;

	// Temporary sync-view/follow-playhead hold state.
	void setSyncViewHoldOn(bool bOn);

	void setSyncViewHold(bool bSyncViewHold);
	bool isSyncViewHold() const;

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the track view
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Track view state info.
	struct TrackViewInfo
	{
		int           trackIndex;   // Track index.
		unsigned long trackStart;   // First track frame on view.
		unsigned long trackEnd;     // Last track frame on view.
		QRect         trackRect;    // The track view rectangle.
	};

	// Get track from given contents vertical position.
	qtractorTrack *trackAt(const QPoint& pos,
		bool bSelectTrack = false, TrackViewInfo *pTrackViewInfo = NULL) const;

	// Get clip from given contents position.
	qtractorClip *clipAt(const QPoint& pos,
		bool bSelectTrack = false, QRect *pClipRect = NULL) const;
	// Get clip from given contents position.
	qtractorClip *clipAtTrack(const QPoint& pos, QRect *pClipRect,
		qtractorTrack *pTrack, TrackViewInfo *pTrackViewInfo) const;

	// Get automation curve node from given contents position.
	qtractorCurve::Node *nodeAtTrack(const QPoint& pos,
		qtractorTrack *pTrack, TrackViewInfo *pTrackViewInfo) const;
	qtractorCurve::Node *nodeAt(const QPoint& pos) const;

	// Get tempo curve time-scale node from given contents position.
	qtractorTimeScale::Node *tempoNodeAtTrack(const QPoint& pos,
		qtractorTrack *pTrack, TrackViewInfo *pTrackViewInfo, bool *atBorder) const;
	qtractorTimeScale::Node *tempoNodeAt(const QPoint& pos) const;

	// Get contents visible rectangle from given track.
	bool trackInfo(qtractorTrack *pTrack,
		TrackViewInfo *pTrackViewInfo) const;
	// Get contents rectangle from given clip.
	bool clipInfo(qtractorClip *pClip, QRect *pClipRect,
		TrackViewInfo *pTrackViewInfo) const;

	// Drag-n-drop event stuffers (for clips).
	qtractorTrack *dragClipMove(const QPoint& pos, bool bKeyStep = false);
	qtractorTrack *dragClipDrop(const QPoint& pos, bool bKeyStep = false,
		const QMimeData *pMimeData = NULL);
	qtractorTrack *dragClipDropEvent(QDropEvent *pDropEvent);
	bool canClipDropEvent(QDropEvent *pDropEvent);

	// Drag-n-drop event stuffers (for curve/automation nodes).
	void dragCurveMove(const QPoint& pos, bool bKeyStep = false);

	// Drag-n-drop event handlers.
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *pDragLeaveEvent);
	void dropEvent(QDropEvent *pDropEvent);

	bool dropClip(const QPoint& pos, const QMimeData *pMimeData = NULL);

	// Handle item selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Handle item/clip editing from mouse.
	void mouseDoubleClickEvent(QMouseEvent *pMouseEvent);

	// Handle zoom with mouse wheel.
	void wheelEvent(QWheelEvent *pWheelEvent);

	// Focus lost event.
	void focusOutEvent(QFocusEvent *pFocusEvent);

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// Clip file(item) selection convenience method.
	void selectClipFile(bool bReset);

	// Clip selection sanity check method.
	bool queryClipSelect(qtractorClip *pClip = NULL);

	// Update whole clip selection.
	void updateClipSelect();

	// Draw/hide the whole current clip selection.
	void showClipSelect() const;
	void hideClipSelect() const;

	// Update whole automation/curve selection.
	void updateCurveSelect();

	// Show selection tooltip...
	void showToolTip(const QRect& rect, int dx) const;

	// Draw/hide the whole drop rectagle list
	void updateClipDropRects(int y, int h) const;
	void showClipDropRects() const;
	void hideClipDropRects() const;

	// Draw/hide a dragging rectangular selection.
	void moveRubberBand(qtractorRubberBand **ppRubberBand,
		const QRect& rectDrag, int thick = 1) const;

	// Check whether we're up to drag something on a track or one of its clips.
	qtractorClip *dragClipStart(const QPoint& pos,
		const Qt::KeyboardModifiers& modifiers,
		bool bSelectTrack = false, QRect *pClipRect = NULL);
	// Check whether we're up to drag a clip fade-in/out or resize handles.
	bool dragClipStartEx(const QPoint& pos,
		const Qt::KeyboardModifiers& modifiers,
		qtractorClip *pClip, const QRect& rectClip);

	// Clip fade-in/out handle drag-move methods.
	void dragClipFadeMove(const QPoint& pos);
	void dragClipFadeDrop(const QPoint& pos);

	// Clip select drag-move and settler method.
	void dragClipSelectMove(const QPoint& pos);

	// Clip resize drag-move methods.
	void dragClipResizeMove(const QPoint& pos);
	void dragClipResizeDrop(const QPoint& pos, bool bTimeStretch = false);

	// Clip resize/repeat handler (over to the left/right).
	void dragClipRepeatLeft(const QPoint& pos);
	void dragClipRepeatRight(const QPoint& pos);

	// Automation curve node drag-move methods.
	void dragCurveNode(const QPoint& pos,
		const Qt::KeyboardModifiers& modifiers);

	// Common tool-tip builder for automation nodes.
	QString nodeToolTip(qtractorCurve *pCurve, qtractorCurve::Node *pNode) const;
	QString nodeToolTip(qtractorTempoCurve *pCurve, qtractorTimeScale::Node *pNode) const;

	// Reset drag/select/move state.
	void resetDragState();

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Keyboard step handler.
	bool keyStep(int iKey, const Qt::KeyboardModifiers& modifiers);

	// Vertical line positioning.
	void drawPositionX(int& iPositionX, int x, bool bSyncView = false);

	// Automation/Tempo/curve node editor methods.
	void openEditCurveNode(qtractorCurve *pCurve, qtractorCurve::Node *pNode);
	void toggleTempoNodeAttached(qtractorTempoCurve *pTempoCurve, qtractorTimeScale::Node *pNode);
	void openEditCurveNode(qtractorTempoCurve *pCurve, qtractorTimeScale::Node *pNode);
	void closeEditCurveNode();

protected slots:

	// To have track view in v-sync with track list.
	void contentsYMovingSlot(int cx, int cy);

	// (Re)create the complete track view pixmap.
	void updatePixmap(int cx, int cy);

	// Drag-reset timer slot.
	void dragTimeout();

	// Automatio/curve node editor slots.
	void editCurveNodeChanged();
	void editCurveNodeFinished();

	// Tempo/curve node editor slots.
	void editTempoCurveNodeChanged();
	void editTempoCurveNodeFinished();

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
		DragNone = 0, DragStart, DragSelect,
		DragClipMove, DragClipStep, DragClipDrop,
		DragClipPaste, DragClipPasteDrop,
		DragClipFadeIn, DragClipFadeOut,
		DragClipSelectLeft, DragClipSelectRight,
		DragClipResizeLeft, DragClipResizeRight,
		DragClipRepeatLeft, DragClipRepeatRight,
		DragCurveMove, DragCurveStep,
		DragCurvePaste, DragCurveNode
	} m_dragState, m_dragCursor;

	qtractorClip *m_pClipDrag;
	QPoint        m_posDrag;
	QRect         m_rectDrag;
	QRect         m_rectHandle;
	int           m_iDragClipX;
	bool          m_bDragTimer;
	QPoint        m_posStep;

	qtractorRubberBand  *m_pRubberBand;
	qtractorClipSelect  *m_pClipSelect;
	qtractorCurveSelect *m_pCurveSelect;
	qtractorTempoCurveSelect *m_pTempoCurveSelect;

	// The clip select mode.
	SelectMode m_selectMode;

	// The multi-item drop mode.
	bool m_bDropSpan;

	// Snap-to-beat/bar grid/zebra view mode.
	bool m_bSnapZebra;
	bool m_bSnapGrid;

	// Floating tool-tips mode.
	bool m_bToolTips;

	// Automation curve node editing mode.
	bool m_bCurveEdit;

	// The local clipboard items.
	struct ClipItem
	{
		// Clipboard item constructor.
		ClipItem(qtractorClip *pClip, const QRect& clipRect,
			unsigned long iClipStart, unsigned long iClipOffset,
			unsigned long iClipLength)
			: clip(pClip), rect(clipRect), clipStart(iClipStart),
				clipOffset(iClipOffset), clipLength(iClipLength),
				fadeInLength(0), fadeOutLength(0) {}
		// Clipboard item members.
		qtractorClip *clip;
		QRect         rect;
		unsigned long clipStart;
		unsigned long clipOffset;
		unsigned long clipLength;
		unsigned long fadeInLength;
		unsigned long fadeOutLength;
	};

	struct NodeItem
	{
		// Clipboard item constructor.
		NodeItem(qtractorCurve::Node *pNode, const QRect& nodeRect,
			unsigned long iFrame, float fValue)
			: node(pNode), rect(nodeRect), frame(iFrame), value(fValue) {}
		NodeItem(qtractorTimeScale::Node *pNode, const QRect& nodeRect,
			unsigned long iFrame, float fValue)
			: tempoNode(pNode), rect(nodeRect), frame(iFrame), value(fValue) {}
		// Clipboard item members.
		qtractorCurve::Node *node;
		qtractorTimeScale::Node *tempoNode;
		QRect rect;
		unsigned long frame;
		float value;
	};

	// The local clipboard stuff (singleton).
	static struct ClipBoard
	{
		// Clipboard constructor.
		ClipBoard() : singleTrack(NULL), frames(0) {}
		// Destructor.
		~ClipBoard() { clear(); }
		// Clipboard stuffer methods.
		void addClip(qtractorClip *pClip, const QRect& clipRect,
			unsigned long iClipStart, unsigned long iClipOffset,
			unsigned long iClipLength);
		void addNode(qtractorCurve::Node *pNode, const QRect& nodeRect,
			unsigned long iFrame, float fValue);
		void addNode(qtractorTimeScale::Node *pNode, const QRect& nodeRect,
			unsigned long iFrame, float fValue);
		// Clipboard reset method.
		void clear();
		// Clipboard members.
		QList<ClipItem *> clips;
		QList<NodeItem *> nodes;
		qtractorTrack    *singleTrack;
		unsigned long     frames;

	} g_clipboard;

	// Playhead and edit shadow pixel positioning.
	int m_iPlayHeadX;
	int m_iEditHeadX;
	int m_iEditTailX;

	int m_iPlayHeadAutoBackwardX;

	// Record rolling update width.
	int m_iLastRecordX;

	// Paste interim parameters.
	unsigned short m_iPasteCount;
	unsigned long  m_iPastePeriod;

	// Single track dragging interim parameters.
	bool m_bDragSingleTrack;
	int  m_iDragSingleTrackY;
	int  m_iDragSingleTrackHeight;

	// Automation curve editing node.
	qtractorCurve       *m_pDragCurve;
	qtractorCurve::Node *m_pDragCurveNode;

	// Tempo curve editing node.
	qtractorTempoCurve       *m_pDragTempoCurve;
	qtractorTimeScale::Node  *m_pDragTimeScaleNode;

	int m_iDragCurveX;

	qtractorCurveEditCommand *m_pCurveEditCommand;

	qtractorTempoCurveEditCommand *m_pTempoCurveEditCommand;

	// Temporary sync-view/follow-playhead hold state.
	bool m_bSyncViewHold;
	int  m_iSyncViewHold;

	// Automation curve node editor widget.
	qtractorCurve       *m_pEditCurve;
	qtractorCurve::Node *m_pEditCurveNode;
	QDoubleSpinBox      *m_pEditCurveNodeSpinBox;
	int                  m_iEditCurveNodeDirty;

	// Tempo curve node editor widget.
	qtractorTempoCurve       *m_pEditTempoCurve;
	qtractorTimeScale::Node  *m_pEditTimeScaleNode;
	qtractorTempoSpinBox *m_pEditTempoCurveNodeSpinBox;
	int                  m_iEditTempoCurveNodeDirty;

	// Optional edge-shadow gradient brushes.
	QBrush m_gradLeft;
	QBrush m_gradRight;
};


#endif  // __qtractorTrackView_h


// end of qtractorTrackView.h
