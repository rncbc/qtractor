// qtractorAudioEditor.h
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

#ifndef __qtractorAudioEditor_h
#define __qtractorAudioEditor_h

#include "qtractorEditor.h"

#include <QHash>
#include <QMap>


// Forward declarations.
class qtractorAudioEditTime;
class qtractorAudioEditView;
class qtractorAudioEditViewScale;

class qtractorAudioClip;
class qtractorAudioPeak;
class qtractorAudioPeakFactory;

class qtractorTrack;

class QFrame;
class QComboBox;
class QCloseEvent;
class QCursor;


//----------------------------------------------------------------------------
// qtractorAudioEditor -- The main session track listview widget.

class qtractorAudioEditor : public qtractorEditor
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioEditor(QWidget *pParent);
	// Destructor.
	~qtractorAudioEditor();

	// Current editing clip accessor.
	qtractorClip *clip() const;

	// Audio clip sequence accessors.
	void setAudioClip(qtractorAudioClip *pAudioClip);
	qtractorAudioClip *audioClip() const;
	qtractorAudioPeak *audioPeak() const;

	// Audio clip properties accessors.
	const QString& filename() const;
	unsigned short channels() const;

	// Child widgets accessors.
	qtractorAudioEditTime *editTime() const;
	qtractorAudioEditView *editView() const;

	// Zoom ratio accessors.
	void setHorizontalZoom(unsigned short iHorizontalZoom);
	void setVerticalZoom(unsigned short iVerticalZoom);

	// Update time-scale to master session.
	void updateTimeScale();

	// Command predicate status.
	bool canUndo() const;
	bool canRedo() const;

	// Undo/redo last edit command.
	void undoCommand();
	void redoCommand();

	// Whether there's any items currently selected.
	bool isSelected() const
		{ return (m_rectSelect.width() > 1); }

	// Whether there's any items on the clipboard.
	static bool isClipboard();

	// Clipboard commands.
	void cutClipboard();
	void copyClipboard();
	void pasteClipboard(
		unsigned short iPasteCount = 1,
		unsigned long iPastePeriod = 0);

	// Retrieve current paste period.
	// (as from current clipboard width)
	unsigned long pastePeriod() const;

	// Execute event removal.
	void deleteSelect();

	// Select all/none contents.
	void selectAll(qtractorScrollView *pScrollView,
		bool bSelect = true, bool bToggle = false);

	// Select range view contents.
	void selectRange(qtractorScrollView *pScrollView,
		bool bToggle = false, bool bCommit = false);

	// Select everything between a given view rectangle.
	void selectRect(qtractorScrollView *pScrollView,
		const QRect& rect, bool bToggle = false, bool bCommit = false);

	// Whether there's any selected range (edit-head/tail).
	bool isSelectable() const;

	// Update/sync integral contents.
	void updateContents();

	// Start drag-move-selecting...
	bool dragMoveStart(qtractorScrollView *pScrollView,
		const QPoint& pos, const Qt::KeyboardModifiers& modifiers);

	// Update drag-move-selection...
	bool dragMoveUpdate(qtractorScrollView *pScrollView,
		const QPoint& pos, const Qt::KeyboardModifiers& modifiers);

	// Commit drag-move-selection...
	bool dragMoveCommit(qtractorScrollView *pScrollView,
		const QPoint& pos, const Qt::KeyboardModifiers& modifiers);

	// Keyboard event handler (common).
	bool keyPress(qtractorScrollView *pScrollView,
		int iKey, const Qt::KeyboardModifiers& modifiers);

	// Keyboard step handler.
	bool keyStep(qtractorScrollView *pScrollView,
		int iKey, const Qt::KeyboardModifiers& modifiers);

	// Zoom centering context.
	struct ZoomCenter
	{
		ZoomCenter() : x(0), y(0), frame(0) {}

		int x, y;
		unsigned long frame;
	};

	// Zoom centering prepare and post methods.
	void zoomCenterPre(ZoomCenter& zc) const;
	void zoomCenterPost(const ZoomCenter& zc);

	// Visualize the event selection drag-move.
	void paintDragState(qtractorScrollView *pScrollView, QPainter& painter);

	// Reset drag/select/move state.
	void resetDragState(qtractorScrollView *pScrollView);

	// Audio-peak factory accessor (singleton)
	static qtractorAudioPeakFactory *audioPeakFactory()
		{ return g_pAudioPeakFactory; }

	// Command list accessor.
	qtractorCommandList *commands() const;

	// Current selection sccessors (in frames)
	unsigned long selectStart() const;
	unsigned long selectEnd() const;

public slots:

	// Zoom view slots.
	void zoomIn();
	void zoomOut();
	void zoomReset();

protected:

	// Audio-peak live-cycle methods.
	void createAudioPeak();
	void deleteAudioPeak();

	// Selection flags
	enum { 
		SelectNone   = 0,
		SelectClear  = 1,
		SelectToggle = 2,
		SelectCommit = 4
	};

	// Clear all selection.
	void clearSelect();

	// Update all selection rectangular areas.
	void updateSelect(bool bSelectReset);

	// Update selection list.
	void updateDragSelect(qtractorScrollView *pScrollView,
		const QRect& rectSelect, int flags);

	// Vertical line position drawing.
	void drawPositionX(int& iPositionX, int x, bool bSyncView);

	// Selection resize drag-move methods.
	bool dragResizeStart(qtractorScrollView *pScrollView, const QPoint& pos);
	void dragResizeUpdate(qtractorScrollView *pScrollView, const QPoint& pos);
	void dragResizeCommit(qtractorScrollView *pScrollView, const QPoint& pos);

protected slots:

	// Horizontal zoom view slots.
	void horizontalZoomInSlot();
	void horizontalZoomOutSlot();
	void horizontalZoomResetSlot();

	// Vertical zoom view slots.
	void verticalZoomInSlot();
	void verticalZoomOutSlot();
	void verticalZoomResetSlot();

	// Audio peak ready slot.
	void audioPeakEventSlot();

private:

	// The editing sequence.
	qtractorAudioClip *m_pAudioClip;
	qtractorAudioPeak *m_pAudioPeak;

	// The main child widgets.
	qtractorAudioEditTime *m_pEditTime;
	qtractorAudioEditView *m_pEditView;
	qtractorAudioEditViewScale *m_pEditViewScale;
	QFrame *m_pEditViewHeader;

	// Common drag state.
	enum DragState {
		DragNone = 0,
		DragStart,
		DragSelect,
		DragResizeLeft,
		DragResizeRight
	} m_dragState, m_dragCursor;

	// The current selecting/dragging stuff.
	QPoint m_posDrag;
	QRect m_rectDrag;

	// Current selection (in pixels).
	QRect m_rectSelect;

	static qtractorAudioPeakFactory *g_pAudioPeakFactory;
	static unsigned int              g_iAudioPeakRefCount;
};


#endif  // __qtractorAudioEditor_h


// end of qtractorAudioEditor.h
