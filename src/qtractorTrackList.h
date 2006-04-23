// qtractorTrackList.h
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

#ifndef __qtractorTrackList_h
#define __qtractorTrackList_h

#include <qlistview.h>
#include <qtoolbutton.h>

// Forward declarations.
class qtractorTracks;
class qtractorTrack;

class qtractorTrackList;
class qtractorTrackListItem;
class qtractorInstrumentList;
class qtractorMonitor;


//----------------------------------------------------------------------------
// qtractorTrackListToolButton -- Tracks list item tool button.

class qtractorTrackListToolButton : public QToolButton
{
	Q_OBJECT

public:

	// Tool button specific types:
	enum ToolType { Record, Mute, Solo };

	// Constructor.
	qtractorTrackListToolButton(qtractorTrackListItem *pItem,
		ToolType toolType);

	// Specific accessors.
	qtractorTrackListItem *item() const { return m_pItem; }
	ToolType toolType() const { return m_toolType; }

protected slots:

	// Special toggle slot.
	void toggledSlot(bool bOn);

private:

	// Instance variables.
	qtractorTrackListItem *m_pItem;
	ToolType m_toolType;

	// Special background colors.
	QColor m_rgbOn;
	QColor m_rgbOff;
};



//----------------------------------------------------------------------------
// qtractorTrackListItem -- Tracks list item.

class qtractorTrackListItem : public QListViewItem
{
public:

	// Constructors.
	qtractorTrackListItem(qtractorTrackList *pTrackList,
		qtractorTrack *pTrack);
	qtractorTrackListItem(qtractorTrackList *pTrackList,
		qtractorTrack *pTrack, QListViewItem *pItemAfter);

	// Destructor.
	~qtractorTrackListItem();

	// Track list brainless accessor.
	qtractorTrackList *trackList() const;

	// Track container accessor.
	qtractorTrack *track() const;

	// Overriden to set extra text info.
	void setText(int iColumn, const QString& sText);

	// Set track item height.
	void setItemHeight(int iItemHeight);
	// Zoom item track's height.
	void zoomItemHeight(unsigned short iVerticalZoom);

	// Overriden view item setup.
	virtual void setup();

	// Overrriden cell painter.
	virtual void paintCell(QPainter *p, const QColorGroup& cg,
		int column, int width, int align);

protected:

	// Common item initializer.
	void initItem(qtractorTrackList *pTrackList, qtractorTrack *pTrack);
	void updateItem(bool bShow);

private:

	// The track reference.
	qtractorTrack *m_pTrack;

	qtractorTrackListToolButton *m_pRecordButton;
	qtractorTrackListToolButton *m_pMuteButton;
	qtractorTrackListToolButton *m_pSoloButton;
};


//----------------------------------------------------------------------------
// qtractorTrackList -- Track list widget.

class qtractorTrackList : public QListView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackList(qtractorTracks *pTracks,
		QWidget *pParent, const char *pszName = 0);

	// Track list view column indexes.
	enum ColumnIndex {
		Number     = 0,
		Name       = 1,
		Bus        = 2,
		Channel    = 3,
		Patch      = 4,
		Instrument = 5
	};

	// Find the list view item from track pointer reference.
	qtractorTrackListItem *trackItem(qtractorTrack *pTrack);

	// Renumber track list items.
	void renumberTrackItems(QListViewItem *pItem = NULL);

	// Zoom all tracks item height.
	void zoomItemHeight(int iVerticalZoom);

	// Instrument list accessor helper.
	qtractorInstrumentList *instruments() const;

	// Notify whole setup that we changed something.
	void contentsChangeNotify();

protected:

	// Handle item height resizing with mouse.
	// Trap this widget width for remembering later.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Overriden to catch early attributes (e.g. header height)
	void polish();

	void contentsMousePressEvent(QMouseEvent *pMouseEvent);
	void contentsMouseMoveEvent(QMouseEvent *pMouseEvent);
	void contentsMouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Draw a dragging separator line.
	void drawDragLine(const QPoint& posDrag, int iThickness = 3) const;

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Reset drag/select/move state.
	void resetDragState();

protected slots:

	// To have track list in v-sync with main track view.
	void contentsMovingSlot(int cx, int cy);

	// Context menu request slot.
	void contextMenuSlot(QListViewItem *pItem, const QPoint& pos, int col);

	// Simple click handler.
	void clickedSlot(QListViewItem *pItem, const QPoint& pos, int col);

signals:

	// Emitted on early polish.
	void polishNotifySignal();

private:

	// The logical parent binding.
	qtractorTracks *m_pTracks;

	// The current selecting/dragging item stuff.
	enum DragState {
		DragNone = 0, DragStart, DragMove, DragResize
	} m_dragState;

	// Flag whether we're resizing an item;
	QPoint         m_posDrag;
	QListViewItem *m_pItemDrag;
	int            m_iItemDragY;
};


#endif  // __qtractorTrackList_h


// end of qtractorTrackList.h
