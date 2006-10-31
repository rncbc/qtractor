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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __qtractorTrackList_h
#define __qtractorTrackList_h

#include <QTableView>


// Forward declarations.
class qtractorTracks;
class qtractorTrack;

class qtractorTrackList;
class qtractorTrackListItem;
class qtractorTrackButton;
class qtractorInstrumentList;
class qtractorMonitor;

class qtractorTrackListModel;

class QResizeEvent;
class QMouseEvent;
class QKeyEvent;

class QRubberBand;


//----------------------------------------------------------------------------
// qtractorTrackList -- Track list widget.

class qtractorTrackList : public QTableView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackList(qtractorTracks *pTracks, QWidget *pParent = 0);

	// Track list view column indexes.
	enum ColumnIndex {
		Number     = 0,
		Name       = 1,
		Bus        = 2,
		Channel    = 3,
		Patch      = 4,
		Instrument = 5
	};

	// Main tracks widget accessor.
	qtractorTracks *tracks() const;

	// Find the list view item from track pointer reference.
	int trackRow(qtractorTrack *pTrack) const;

	// Find the track pointer reference from list view item row.
	qtractorTrack *track(int iTrack) const;

	// Insert/remove/select a track item.
	void insertTrack(int iTrack, qtractorTrack *pTrack);
	void removeTrack(int iTrack);
	void selectTrack(int iTrack);

	// Retrieves current selected track reference.
	qtractorTrack *currentTrack() const;

	// Update the list view item from track pointer reference.
	void updateTrack(qtractorTrack *pTrack);

	// Main table cleaner.
	void clear();

	// Base item height (in pixels).
	enum { ItemHeightMin = 24, ItemHeightBase = 48 };

	// Zoom all tracks item height.
	void updateZoomHeight();

protected:

	// Trap size changes.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Context menu request slot.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Handle item height resizing and track move with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Show and move rubber-band item.
	void moveRubberBand(const QPoint& posDrag);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Reset drag/select/move state.
	void resetDragState();

signals:

	// More like current row has changed.
	void selectionChanged();

protected slots:

	// Trap current index changes.
	void currentChanged(const QModelIndex& curr, const QModelIndex& prev);

	// To have dircet access to track properties.
	void doubleClickedSlot(const QModelIndex& index);

	// Vertical offset position change slot.
	void contentsYChangedSlot();

	// To have track list in v-sync with main track view.
	void contentsYMovingSlot(int cx, int cy);

private:

	// The logical parent binding.
	qtractorTracks *m_pTracks;

	// Our own track-list model.
	qtractorTrackListModel  *m_pListModel;

	// The current selecting/dragging item stuff.
	enum DragState {
		DragNone = 0, DragStart, DragMove, DragResize
	} m_dragState;

	// For whether we're resizing or moving an item;
	QPoint m_posDrag;
	int    m_iDragTrack;
	int    m_iDragY;

	QRubberBand *m_pRubberBand;

	// Avoid vertical change recursion.
	int m_iContentsYMoving;
};


#endif  // __qtractorTrackList_h


// end of qtractorTrackList.h
