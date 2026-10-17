// qtractorEditor.h
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

#ifndef __qtractorEditor_h
#define __qtractorEditor_h

#include <QSplitter>


// Forward decls.
class qtractorTimeScale;
class qtractorScrollView;

class qtractorClip;

class qtractorCommandList;
class qtractorCommand;

class qtractorRubberBand;


//----------------------------------------------------------------------------
// qtractorEditor -- Base clip editor widget.

class qtractorEditor : public QSplitter
{
	Q_OBJECT

public:

	// Constructor.
	qtractorEditor(QWidget *pParent);
	// Destructor.
	virtual ~qtractorEditor();

	// Current editing clip accessor.
	virtual qtractorClip *clip() const = 0;

	// Event foreground (outline) color.
	void setForeground(const QColor& fore);
	const QColor& foreground() const;

	// Event background (fill) color.
	void setBackground(const QColor& back);
	const QColor& background() const;

	// Snap-to-bar zebra mode.
	void setSnapZebra(bool bSnapZebra);
	bool isSnapZebra() const;

	// Snap-to-beat grid mode.
	void setSnapGrid(bool bSnapGrid);
	bool isSnapGrid() const;

	// Floating tool-tips view mode.
	void setToolTips(bool bToolTips);
	bool isToolTips() const;

	// Fade in/out controls and view mode.
	void setFadeInOut(bool bFadeInOut);
	bool isFadeInOut() const;

	// Zoom (view) modes.
	enum { ZoomNone = 0, ZoomHorizontal = 1, ZoomVertical = 2, ZoomAll = 3 };

	void setZoomMode(int iZoomMode);
	int zoomMode() const;

	// Zoom ratio accessors.
	virtual void setHorizontalZoom(unsigned short iHorizontalZoom);
	unsigned short horizontalZoom() const;

	virtual void setVerticalZoom(unsigned short iVerticalZoom);
	unsigned short verticalZoom() const;

	// Local time scale accessors.
	qtractorTimeScale *timeScale() const;

	// Time-scale offset (in frames) accessors.
	void setOffset(unsigned long iOffset);
	unsigned long offset() const;

	// Time-scale length (in frames) accessors.
	void setLength(unsigned long iLength);
	unsigned long length() const;

	// Edit-head/tail accessors.
	void setEditHead(unsigned long iEditHead, bool bSyncView = true);
	int editHeadX() const;

	void setEditTail(unsigned long iEditTail, bool bSyncView = true);
	int editTailX() const;

	// Play-head positioning.
	void setPlayHead(unsigned long iPlayHead, bool bSyncView = true);
	int playHeadX() const;

	// Play-head follow-ness.
	void setSyncView(bool bSyncView);
	bool isSyncView() const;

	// Temporary sync-view/follow-playhead hold state.
	void setSyncViewHoldOn(bool bOn);

	void setSyncViewHold(bool bSyncViewHold);
	bool isSyncViewHold() const;

	// Return either snapped pixel/frame,
	// or the passed one if [Alt] key is pressed.
	unsigned int pixelSnap(unsigned int x) const;
	unsigned long frameSnap(unsigned long iFrame) const;

	// Show selection tooltip...
	void showToolTip(
		qtractorScrollView *pScrollView, const QRect& rect) const;

	// Make given frame position visible in view.
	void ensureVisibleFrame(
		qtractorScrollView *pScrollView, unsigned long iFrame);

	// Update time-scale to master session.
	virtual void updateTimeScale() = 0;

	// Command list accessor.
	virtual qtractorCommandList *commands() const = 0;

	// Command executioner...
	bool execute(qtractorCommand *pCommand);

	// Draw the fade in/out slopes and handles.
	void drawFadeInOut(QPainter& painter, int dx, const QRect& clipRect);

	// Redirect selection notification.
	virtual void selectionChangeNotify();

signals:

	// Emitted on selection/changes.
	void selectNotifySignal(QObject *);
	void changeNotifySignal(QObject *);

protected slots:

	// Command execution notification slot.
	void updateNotifySlot(unsigned int flags);

protected:

	// Zoom factor constants.
	enum { ZoomMin = 10, ZoomBase = 100, ZoomMax = 1000, ZoomStep = 10 };

	// Zoom step evaluator.
	int zoomStep() const;

	// Common zoom factor settlers.
	void horizontalZoomStep(int iZoomStep);
	void verticalZoomStep(int iZoomStep);

	// Vertical line position drawing.
	virtual void drawPositionX(int& iPositionX, int x, bool bSyncView) = 0;

	// Update/sync integral contents.
	virtual void updateContents() = 0;

	// Start drag-move-selecting...
	virtual bool dragMoveStart(qtractorScrollView *pScrollView,
		const QPoint& pos, const Qt::KeyboardModifiers& modifiers);

	// Update drag-move-selection...
	virtual bool dragMoveUpdate(qtractorScrollView *pScrollView,
		const QPoint& pos, const Qt::KeyboardModifiers& modifiers);

	// Commit drag-move-selection...
	virtual bool dragMoveCommit(qtractorScrollView *pScrollView,
		const QPoint& pos, const Qt::KeyboardModifiers& modifiers);

	// Visualize the current drag/select/move state.
	virtual void paintDragState(
		qtractorScrollView *pScrollView, QPainter& painter);

	// Reset drag/select/move state.
	virtual void resetDragState(
		qtractorScrollView *pScrollView);

	// Check whether we're up to drag a clip fade-in/out or resize handles.
	bool dragFadeInOutStart(
		qtractorScrollView *pScrollView, const QPoint& pos);

	// Clip fade-in/out handle drag-moving parts.
	void dragFadeInOutMove(
		qtractorScrollView *pScrolView, const QPoint& pos);

	// Clip fade-in/out handle settler.
	void dragFadeInOutDrop(
		qtractorScrollView *pScrollView, const QPoint& pos);

	// Show and move rubber-band item.
	void moveRubberBand(
		qtractorScrollView *pScrollView, const QRect& rectDrag, int thick = 1);

	// Temporary sync-view/follow-playhead hold state.
	int m_iSyncViewHold;

private:

	// Event fore/background colors.
	QColor m_foreground;
	QColor m_background;

	// The local time scale.
	qtractorTimeScale *m_pTimeScale;

	// The local time-scale offset/length.
	unsigned long m_iOffset;
	unsigned long m_iLength;

	// Zoom mode flag.
	int m_iZoomMode;

	// Snap-to-beat/bar grid/zebra mode.
	bool m_bSnapZebra;
	bool m_bSnapGrid;

	// Floating tool-tips view mode.
	bool m_bToolTips;

	// Fade in/out control and view mode.
	bool m_bFadeInOut;

	// Local edit-head/tail positioning.
	int  m_iEditHeadX;
	int  m_iEditTailX;

	// Local playhead positioning.
	int  m_iPlayHeadX;
	bool m_bSyncView;

	// Temporary sync-view/follow-playhead hold state.
	bool m_bSyncViewHold;

	// Common fade-in/out drag state.
	enum DragFadeState {
		DragFadeNone = 0,
		DragFadeStart,
		DragFadeIn,
		DragFadeOut
	} m_dragFadeState, m_dragFadeCursor;

	// The current fade-in/out dragging stuff.
	QPoint m_posFadeStart;
	QRect  m_rectFadeClip;
	QRect  m_rectFadeHandle;
	int    m_iDragFadeX;

	// Viewport rubber-banding stuff.
	qtractorRubberBand *m_pRubberBand;
};


#endif  // __qtractorEditor_h


// end of qtractorEditor.h
