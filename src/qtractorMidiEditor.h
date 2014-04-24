// qtractorMidiEditor.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiEditor_h
#define __qtractorMidiEditor_h

#include "qtractorMidiCursor.h"
#include "qtractorMidiEditSelect.h"

#include "qtractorMidiEvent.h"

#include <QSplitter>
#include <QHash>
#include <QMap>


// Forward declarations.
class qtractorScrollView;
class qtractorRubberBand;

class qtractorCommandList;

class qtractorMidiEditList;
class qtractorMidiEditTime;
class qtractorMidiEditView;
class qtractorMidiEditEventScale;
class qtractorMidiEditEvent;

class qtractorMidiThumbView;

class qtractorMidiEditCommand;
class qtractorMidiClip;

class qtractorTimeScale;

class QFrame;
class QComboBox;
class QCloseEvent;
class QCursor;


//----------------------------------------------------------------------------
// qtractorMidiEditor -- The main session track listview widget.

class qtractorMidiEditor : public QSplitter
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEditor(QWidget *pParent);
	// Destructor.
	~qtractorMidiEditor();

	// MIDI clip sequence accessors.
	void setMidiClip(qtractorMidiClip *pMidiClip);
	qtractorMidiClip *midiClip() const;

	// MIDI clip properties accessors.
	const QString& filename() const;
	unsigned short trackChannel() const;
	unsigned short format() const;

	qtractorMidiSequence *sequence() const;

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

	// Floating tool-tips mode.
	void setToolTips(bool bToolTips);
	bool isToolTips() const;

	// Edit (creational) mode.
	void setEditMode(bool bEditMode);
	bool isEditMode() const;

	// Edit draw (notes) mode.
	void setEditModeDraw(bool bEditModeDraw);
	bool isEditModeDraw() const;

	// Zoom (view) modes.
	enum { ZoomNone = 0, ZoomHorizontal = 1, ZoomVertical = 2, ZoomAll = 3 };

	void setZoomMode(int iZoomMode);
	int zoomMode() const;

	// Zoom ratio accessors.
	void setHorizontalZoom(unsigned short iHorizontalZoom);
	unsigned short horizontalZoom() const;

	void setVerticalZoom(unsigned short iVerticalZoom);
	unsigned short verticalZoom() const;

	// Local time scale accessors.
	qtractorTimeScale *timeScale() const;
	unsigned long timeOffset() const;

	// The original clip time-scale length/time.
	void setClipLength(unsigned long iClipLength);
	unsigned long clipLength() const;

	// Reset original clip time-scale length/time.
	void resetClipLength();

	// Time-scale offset (in frames) accessors.
	void setOffset(unsigned long iOffset);
	unsigned long offset() const;

	// Time-scale length (in frames) accessors.
	void setLength(unsigned long iLength);
	unsigned long length() const;

	// Child widgets accessors.
	QFrame *editListHeader() const;
	qtractorMidiEditList *editList() const;
	qtractorMidiEditTime *editTime() const;
	qtractorMidiEditView *editView() const;
	qtractorMidiEditEventScale *editEventScale() const;
	qtractorMidiEditEvent *editEvent() const;
	QFrame *editEventFrame() const;

	qtractorMidiThumbView *thumbView() const;

	// Edit-head/tail accessors.
	void setEditHead(unsigned long iEditHead, bool bSyncView = true);
	unsigned long editHead() const;
	int editHeadX() const;

	void setEditTail(unsigned long iEditTail, bool bSyncView = true);
	unsigned long editTail() const;
	int editTailX() const;

	// Play-head positioning.
	void setPlayHead(unsigned long iPlayHead, bool bSyncView = true);
	unsigned long playHead() const;
	int playHeadX() const;

	// Update time-scale to master session.
	void updateTimeScale();

	// Play-head follow-ness.
	void setSyncView(bool bSyncView);
	bool isSyncView() const;

	// Note autition while editing.
	void setSendNotes(bool bSendNotes);
	bool isSendNotes() const;

	// Note event value vs. duration display.
	void setNoteDuration(bool bNoteDuration);
	bool isNoteDuration() const;

	// Note event coloring.
	void setNoteColor(bool bNoteColor);
	bool isNoteColor() const;

	// Note event coloring.
	void setValueColor(bool bValueColor);
	bool isValueColor() const;

	// Snap-to-scale/quantize key accessor.
	void setSnapToScaleKey(int iSnapToScaleKey);
	int snapToScaleKey() const;

	// Snap-to-scale/quantize type accessor.
	void setSnapToScaleType(int iSnapToScaleType);
	int snapToScaleType() const;

	// Alterrnate command action update helper...
	void updateUndoAction(QAction *pAction) const;
	void updateRedoAction(QAction *pAction) const;

	// Command predicate status.
	bool canUndo() const;
	bool canRedo() const;

	// Undo/redo last edit command.
	void undoCommand();
	void redoCommand();

	// Whether there's any items currently selected.
	bool isSelected() const;

	// Whether there's any items on the clipboard.
	static bool isClipboard();

	// Clipboard commands.
	void cutClipboard();
	void copyClipboard();
	void pasteClipboard(
		unsigned short iPasteCount = 1, unsigned long iPastePeriod = 0);

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

	// Add/remove one single event to current selection.
	void selectEvent(qtractorMidiEvent *pEvent, bool bSelect = true);

	// Retrieve current selection.
	QList<qtractorMidiEvent *> selectedEvents() const;

	// Selectable event predicate.
	bool isEventSelectable(qtractorMidiEvent *pEvent) const;

	// Whether there's any events beyond the insertion point (edit-tail).
	bool isInsertable() const;

	// Whether there's any selected range (edit-head/tail).
	bool isSelectable() const;

	// Insert/remove edit range.
	void insertEditRange();
	void removeEditRange();

	// Update/sync integral contents.
	void updateContents();
	
	// Try to center vertically the edit-view...
	void centerContents();

	// Reset event cursors.
	void reset(bool bSelectClear);

	// Clear all contents.
	void clear();

	// To optimize and keep track of current (re)draw
	// position, mostly like an sequence cursor/iterator.
	qtractorMidiEvent *seekEvent(unsigned long iTime);

	// Get event from given contents position.
	qtractorMidiEvent *eventAt(qtractorScrollView *pScrollView,
		const QPoint& pos, QRect *pRect = NULL);

	// Start immediate some drag-edit mode...
	qtractorMidiEvent *dragEditEvent(qtractorScrollView *pScrollView,
		const QPoint& pos, Qt::KeyboardModifiers modifiers);
	
	// Track drag-move-select cursor and mode...
	qtractorMidiEvent *dragMoveEvent (qtractorScrollView *pScrollView,
		const QPoint& pos, Qt::KeyboardModifiers modifiers);

	// Start drag-move-selecting...
	void dragMoveStart(qtractorScrollView *pScrollView,
		const QPoint& pos, Qt::KeyboardModifiers modifiers);

	// Update drag-move-selection...
	void dragMoveUpdate(qtractorScrollView *pScrollView,
		const QPoint& pos, Qt::KeyboardModifiers modifiers);

	// Commit drag-move-selection...
	void dragMoveCommit(qtractorScrollView *pScrollView,
		const QPoint& pos, Qt::KeyboardModifiers modifiers);

	// Trap for help/tool-tip and leave events.
	bool dragMoveFilter(qtractorScrollView *pScrollView,
		QObject *pObject, QEvent *pEvent);

	// Keyboard event handler (common).
	bool keyPress(qtractorScrollView *pScrollView,
		int iKey, Qt::KeyboardModifiers modifiers);

	// Keyboard step handler.
	bool keyStep(int iKey);

	// Lost focus handler.
	void focusOut(qtractorScrollView *pScrollView);

	// Show selection tooltip...
	void showToolTip(qtractorScrollView *pScrollView, const QRect& rect) const;

	// MIDI event tool tip helper.
	QString eventToolTip(qtractorMidiEvent *pEvent,
		long iTimeDelta = 0, int iNoteDelta = 0, int iValueDelta = 0) const;

	// Temporary sync-view/follow-playhead hold state.
	void setSyncViewHoldOn(bool bOn);

	void setSyncViewHold(bool bSyncViewHold);
	bool isSyncViewHold() const;

	// Visualize the event selection drag-move.
	void paintDragState(qtractorScrollView *pScrollView,
		QPainter *pPainter);

	// Reset drag/select/move state.
	void resetDragState(qtractorScrollView *pScrollView);

	// Tool indexes.
	enum Tool {
		Quantize  = 0,
		Transpose = 1,
		Normalize = 2,
		Randomize = 3,
		Resize    = 4,
		Rescale   = 5,
		Timeshift = 6
	};

	// Edit tools form page selector.
	void executeTool(int iToolIndex);

	// Command list accessor.
	qtractorCommandList *commands() const;

	// Note name map accessor.
	const QString noteName(unsigned char note) const;
	// Controller name map accessor.
	const QString& controllerName(unsigned char controller) const;

	// RPN/NRPN map accessors.
	const QMap<unsigned short, QString>& rpnNames() const;
	const QMap<unsigned short, QString>& nrpnNames() const;

	// Control-14 map accessors.
	const QString& control14Name(unsigned char controller) const;

	// Default note name map accessor.
	static const QString defaultNoteName(unsigned char note, bool fDrums = false);
	// Default controller name accessor.
	static const QString& defaultControllerName(unsigned char controller);

	// Default RPN/NRPN map accessors.
	static const QMap<unsigned short, QString>& defaultRpnNames();
	static const QMap<unsigned short, QString>& defaultNrpnNames();

	// Default Control-14 map accessors.
	static const QString& defaultControl14Name(unsigned char controller);

	// Default scale key/type names accessors.
	static const QStringList& scaleKeyNames();
	static const QStringList& scaleTypeNames();

	// Scale quantizer method.	
	static unsigned char snapToScale(
		unsigned char note, int iKey, int iScale);

public slots:

	// Redirect selection/change notification.
	void selectionChangeNotify();
	void contentsChangeNotify();

	// Redirect note on/off;
	void sendNote(int iNote, int iVelocity = 0);

	// Update instrument defined names for current clip/track.
	void updateInstrumentNames();

	// Zoom view slots.
	void zoomIn();
	void zoomOut();
	void zoomReset();

protected:

	// Update instrument default note names (nb. drum key names).
	void updateDefaultDrumNoteNames();
	void updateDefaultControllerNames();
	void updateDefaultRpnNames();
	void updateDefaultNrpnNames();

	// Zoom factor constants.
	enum { ZoomMin = 10, ZoomBase = 100, ZoomMax = 1000, ZoomStep = 10 };

	// Common zoom factor settlers.
	void horizontalZoomStep(int iZoomStep);
	void verticalZoomStep(int iZoomStep);

	// Zoom centering context.
	struct ZoomCenter
	{
		int x, y, item;
		unsigned long frame;
	};

	// Zoom centering prepare and post methods.
	void zoomCenterPre(ZoomCenter& zc) const;
	void zoomCenterPost(const ZoomCenter& zc);

	// Ensuere point visibility depending on view.
	void ensureVisible(qtractorScrollView *pScrollView, const QPoint& pos);

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

	// Update the event selection list.
	void updateDragSelect(qtractorScrollView *pScrollView,
		const QRect& rectSelect, int flags);

	// Compute current drag time snap (in ticks).
	long timeSnap(long iTime) const;

	// Compute current drag time delta (in ticks).
	long timeDelta(qtractorScrollView *pScrollView) const;

	// Compute current drag note delta.
	int noteDelta(qtractorScrollView *pScrollView) const;

	// Compute current drag value delta.
	int valueDelta(qtractorScrollView *pScrollView) const;

	// Apply the event drag-resize (also editing).
	void resizeEvent(qtractorMidiEvent *pEvent,
		long iTimeDelta, int iValueDelta,
		qtractorMidiEditCommand *pEditCommand = NULL);

	// Update event selection rectangle.
	void updateEvent(qtractorMidiEvent *pEvent);

	// Update event visual rectangles.
	void updateEventRects(qtractorMidiEvent *pEvent,
		QRect& rectEvent, QRect& rectView) const;

	// Drag-move current selection.
	void updateDragMove(qtractorScrollView *pScrollView, const QPoint& pos);
	// Finalize the event drag-move.
	void executeDragMove(qtractorScrollView *pScrollView, const QPoint& pos);

	// Drag-rescale current selection.
	void updateDragRescale(qtractorScrollView *pScrollView, const QPoint& pos);
	// Finalize the event drag-rescale.
	void executeDragRescale(qtractorScrollView *pScrollView, const QPoint& pos);

	// Drag-resize current selection (also editing).
	void updateDragResize(qtractorScrollView *pScrollView, const QPoint& pos);
	// Finalize the event drag-resize (also editing).
	void executeDragResize(qtractorScrollView *pScrollView, const QPoint& pos);

	// Finalize the event drag-paste.
	void executeDragPaste(qtractorScrollView *pScrollView, const QPoint& pos);

	// Drag(draw) event value-resize check.
	bool isDragEventResize(Qt::KeyboardModifiers modifiers) const;
	// Drag(draw) event value-resize to current selection...
	void updateDragEventResize(const QPoint& pos);
	// Apply drag(draw) event value-resize to current selection.
	void executeDragEventResize(const QPoint& pos);

	// Vertical line position drawing.
	void drawPositionX(int& iPositionX, int x, bool bSyncView);

	// Specialized drag/time-scale (draft)...
	struct DragTimeScale;

protected slots:

	// Horizontal zoom view slots.
	void horizontalZoomInSlot();
	void horizontalZoomOutSlot();
	void horizontalZoomResetSlot();

	// Vertical zoom view slots.
	void verticalZoomInSlot();
	void verticalZoomOutSlot();
	void verticalZoomResetSlot();

	// Command execution notification slot.
	void updateNotifySlot(unsigned int flags);

signals:

	// Emitted on selection/changes.
	void selectNotifySignal(qtractorMidiEditor *);
	void changeNotifySignal(qtractorMidiEditor *);

	// Send note event signale.
	void sendNoteSignal(int, int);

private:

	// The editing sequence.
	qtractorMidiClip *m_pMidiClip;

	// Event fore/background colors.
	QColor m_foreground;
	QColor m_background;

	// The main child widgets.
	QFrame *m_pEditListHeader;
	qtractorMidiEditList *m_pEditList;
	qtractorMidiEditTime *m_pEditTime;
	qtractorMidiEditView *m_pEditView;
	qtractorMidiEditEventScale *m_pEditEventScale;
	qtractorMidiEditEvent *m_pEditEvent;
	QFrame *m_pEditEventFrame;

	qtractorMidiThumbView *m_pThumbView;

	// The local time scale.
	qtractorTimeScale *m_pTimeScale;

	// The original clip time-scale length/time.
	unsigned long m_iClipLengthTime;

	// The local time-scale offset/length.
	unsigned long m_iOffset;
	unsigned long m_iLength;

	// Event cursors (main time-line).
	qtractorMidiCursor m_cursor;
	qtractorMidiCursor m_cursorAt;

	// The current selection list.
	qtractorMidiEditSelect m_select;

	// Common drag state.
	enum DragState { 
		DragNone = 0,
		DragStart,
		DragSelect,
		DragMove,
		DragRescale,
		DragResize,
		DragEventResize,
		DragPaste,
		DragStep
	} m_dragState, m_dragCursor;

	// Common drag-resize mode.
	enum ResizeMode { 
		ResizeNone = 0,
		ResizeNoteRight,
		ResizeNoteLeft,
		ResizeValue,
		ResizeValue14,
		ResizePitchBend
	} m_resizeMode;

	// The current selecting/dragging stuff.
	qtractorMidiEvent *m_pEventDrag;
	QPoint m_posDrag;
	QRect  m_rectDrag;

	// Differential drag-move position.
	QPoint m_posDelta;

	// Step (keyboard) drag-move position
	QPoint m_posStep;

	// Drag(draw) event-value position.
	QPoint m_posDragEventResize;

	// Which widget holds focus on drag-paste?
	qtractorScrollView *m_pEditPaste;

	// Viewport rubber-banding stuff.
	qtractorRubberBand *m_pRubberBand;

	// Zoom mode flag.
	int m_iZoomMode;

	// Edit mode flags.
	bool m_bEditMode;
	bool m_bEditModeDraw;

	bool m_bEventDragEdit;

	// Snap-to-beat/bar grid/zebra mode.
	bool m_bSnapZebra;
	bool m_bSnapGrid;

	// Floating tool-tips mode.
	bool m_bToolTips;

	// Last useful editing values.
	struct {
		unsigned char  note;
		unsigned short value;
		unsigned long  duration;
		unsigned short pitchBend;
	} m_last;

	// The local edit command list/queue.
	qtractorCommandList *m_pCommands;

	// Local edit-head/tail positioning.
	unsigned long m_iEditHead;
	int           m_iEditHeadX;
	unsigned long m_iEditTail;
	int           m_iEditTailX;

	// Local playhead positioning.
	unsigned long m_iPlayHead;
	int           m_iPlayHeadX;
	bool          m_bSyncView;

	// Note autition while editing.
	bool m_bSendNotes;
	
	// Event value stick vs. duration rectangle.
	bool m_bNoteDuration;

	// Event (note, velocity) coloring.
	bool m_bNoteColor;
	bool m_bValueColor;

	// The local clipboard stuff (singleton).
	static struct ClipBoard
	{
		// Constructor.
		ClipBoard() {}
		// Destructor.
		~ClipBoard() { clear(); }
		// Clipboard clear.
		void clear() { qDeleteAll(items); items.clear(); }
		// Clipboard members.
		QList<qtractorMidiEvent *> items;

		// Singleton declaration.
	}	g_clipboard;

	// Instrument defined names for current clip/track.
	QHash<unsigned char,  QString> m_noteNames;
	QHash<unsigned char,  QString> m_controllerNames;

	QMap<unsigned short, QString> m_rpnNames;
	QMap<unsigned short, QString> m_nrpnNames;

	// Snap-to-scale (aka.in-place scale-quantize) stuff.
	int m_iSnapToScaleKey;
	int m_iSnapToScaleType;

	// Temporary sync-view/follow-playhead hold state.
	bool m_bSyncViewHold;
	int  m_iSyncViewHold;
};


#endif  // __qtractorMidiEditor_h


// end of qtractorMidiEditor.h
