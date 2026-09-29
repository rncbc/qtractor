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
class qtractorScrollView;
class qtractorRubberBand;

class qtractorCommandList;
class qtractorCommand;

class qtractorAudioEditTime;
class qtractorAudioEditView;
class qtractorAudioEditViewScale;

class qtractorAudioClip;
class qtractorAudioPeak;

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

	// Whether there's any selected range (edit-head/tail).
	bool isSelectable() const;

	// Update/sync integral contents.
	void updateContents();
	
	// Keyboard event handler (common).
	bool keyPress(qtractorScrollView *pScrollView,
		int iKey, const Qt::KeyboardModifiers& modifiers);

	// Keyboard step handler.
	bool keyStep(qtractorScrollView *pScrollView,
		int iKey, const Qt::KeyboardModifiers& modifiers);

	// Zoom centering context.
	struct ZoomCenter
	{
		int x, y;
		unsigned long frame;
	};

	// Zoom centering prepare and post methods.
	void zoomCenterPre(ZoomCenter& zc) const;
	void zoomCenterPost(const ZoomCenter& zc);

	// Make given frame position visible in view.
	void ensureVisibleFrame(qtractorScrollView *pScrollView, unsigned long iFrame);

	// Visualize the event selection drag-move.
	void paintDragState(qtractorScrollView *pScrollView, QPainter *pPainter);

	// Reset drag/select/move state.
	void resetDragState(qtractorScrollView *pScrollView);

	// Command list accessor.
	qtractorCommandList *commands() const;

	// Command executioner...
	bool execute(qtractorCommand *pCommand);

	// Redirect selection notification.
	void selectionChangeNotify();

public slots:

	// Zoom view slots.
	void zoomIn();
	void zoomOut();
	void zoomReset();

protected:

	// Audio-peak live-cycle methods.
	void createAudioPeak();
	void deleteAudioPeak();

	// Ensure point visibility depending on view.
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

	// Update selection list.
	void updateDragSelect(qtractorScrollView *pScrollView,
		const QRect& rectSelect, int flags);

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
	void selectNotifySignal(QObject *);
	void changeNotifySignal(QObject *);

private:

	// The editing sequence.
	qtractorAudioClip *m_pAudioClip;
	qtractorAudioPeak *m_pAudioPeak;

	// The main widget splitters.
	QSplitter *m_pHSplitter;
	QSplitter *m_pVSplitter;

	// The main child widgets.
	qtractorAudioEditTime *m_pEditTime;
	qtractorAudioEditView *m_pEditView;
	qtractorAudioEditViewScale *m_pEditViewScale;
	QFrame *m_pEditViewHeader;
};


#endif  // __qtractorAudioEditor_h


// end of qtractorAudioEditor.h
