// qtractorTrackList.h
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorScrollView.h"

#include <QHash>
#include <QPixmap>


// Forward declarations.
class qtractorTrack;
class qtractorTracks;
class qtractorTrackList;
class qtractorTrackButton;
class qtractorRubberBand;
class qtractorMidiManager;

class qtractorCurveButton;
class qtractorPluginListView;
class qtractorMeter;

class QHeaderView;

class QResizeEvent;
class QMouseEvent;
class QWheelEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QKeyEvent;


//----------------------------------------------------------------------------
// qtractorTrackListButtons -- Track button layout widget.

class qtractorTrackListButtons : public QWidget
{
public:

	// Constructor.
	qtractorTrackListButtons(
		qtractorTrack *pTrack, QWidget *pParent = 0);

	// Local child widgets accessors.
	qtractorTrackButton *recordButton() const
		{ return m_pRecordButton; }
	qtractorTrackButton *muteButton() const
		{ return m_pMuteButton; }
	qtractorTrackButton *soloButton() const
		{ return m_pSoloButton; }
	qtractorCurveButton *curveButton() const
		{ return m_pCurveButton; }

	// Refresh color (palette) state buttons
	void updateTrackButtons();

private:

	// The local child widgets.
	qtractorTrackButton *m_pRecordButton;
	qtractorTrackButton *m_pMuteButton;
	qtractorTrackButton *m_pSoloButton;
	qtractorCurveButton *m_pCurveButton;
};


//----------------------------------------------------------------------------
// qtractorTrackList -- Track list widget.

class qtractorTrackList : public qtractorScrollView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackList(qtractorTracks *pTracks, QWidget *pParent = 0);
	// Destructor.
	~qtractorTrackList();

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

	// Header view accessor.
	QHeaderView *header() const;

	// Find the list view item from track pointer reference.
	int trackRow(qtractorTrack *pTrack) const;

	// Find track row/column of given viewport point...
	int trackRowAt(const QPoint& pos);
	int trackColumnAt(const QPoint& pos);

	// Find the track pointer reference from list view item row.
	qtractorTrack *track(int iTrack) const;

	// Insert/remove a track item; return actual track of incident.
	int insertTrack(int iTrack, qtractorTrack *pTrack);
	int removeTrack(int iTrack);

	// Manage current track row by index.
	void setCurrentTrackRow(int iTrack);
	int currentTrackRow() const;
	int trackRowCount() const;

	// Current selected track reference.
	void setCurrentTrack(qtractorTrack *pTrack);
	qtractorTrack *currentTrack() const;

	// Update the list view item from track pointer reference.
	void updateTrack(qtractorTrack *pTrack);

	// Update the list view item from MIDI manager pointer reference.
	void updateMidiTrackItem(qtractorMidiManager *pMidiManager);

	// Track-button colors (palette) update.
	void updateTrackButtons();

	// Update all track-items/icons methods.
	void updateItems();
	void updateIcons();

	// Main table cleaner.
	void clear();

	// Update list content height.
	void updateContentsHeight();

	// Rectangular contents update.
	void updateContents(const QRect& rect);
	// Overall contents update.
	void updateContents();

	// Retrieve all current seleceted tracks but one.
	QList<qtractorTrack *> selectedTracks(
		qtractorTrack *pTrackEx = NULL, bool bAllTracks = false) const;

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Draw the time scale.
	void drawContents(QPainter *pPainter, const QRect& rect);

	// Retrive the given track row rectangular (in contents coordinates).
	QRect trackRect(int iTrack) const;

	// Draw table cell.
	void drawCell(QPainter *pPainter, int iRow, int iCol,
		const QRect& rect) const;

	// Context menu request slot.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Handle mouse double-clicks.
	void mouseDoubleClickEvent(QMouseEvent *pMouseEvent);

	// Handle item selection with mouse.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Handle zoom with mouse wheel.
	void wheelEvent(QWheelEvent *pWheelEvent);

	// Drag-n-drop event handlers.
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dropEvent(QDropEvent *pDropEvent);

	// Show and move rubber-band item.
	void moveRubberBand(const QPoint& posDrag);
	void moveRubberBand(const QRect& rectDrag);

	// Make sure the given (track) rectangle is visible.
	bool ensureVisibleRect(const QRect& rect);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Reset drag/select/move state.
	void resetDragState();

	// Update header extents.
	void updateHeader();

	// Selection methods.
	void selectTrack(int iTrack, bool bSelect = true, bool bToggle = false);

	void updateSelect(bool bCommit);
	void clearSelect();

	// Track-list header model pre-decl.
	class HeaderModel;

signals:

	// More like current row has changed.
	void selectionChanged();

protected slots:

	// To have timeline in h-sync with main track view.
	void contentsYMovingSlot(int cx, int cy);

	// (Re)create the time scale pixmap.
	void updatePixmap(int cx, int cy);

	// Check/update header resize.
	void updateHeaderSize(int iCol, int, int iColSize);

	// Update header extents.
	void resetHeaderSize(int iCol);

private:

	// The logical parent binding.
	qtractorTracks *m_pTracks;

	// Local horizontal header.
	QHeaderView *m_pHeader;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// Model cache item.
	struct Item
	{
		// Constructor
		Item(qtractorTrack *pTrack);
		// Destructor.
		~Item();
		// Bank/program names helper.
		bool updateBankProgNames(qtractorMidiManager *pMidiManager,
			const QString& sInstrumentName,
			QString& sBankName, QString& sProgName) const;
		// Item updaters.
		void updateItem(qtractorTrackList *pTrackList);
		void updateIcon(qtractorTrackList *pTrackList);
		// Item members.
		qtractorTrack *track;
		QPixmap        icon;
		QStringList    text;
		unsigned int   flags;
		// Track-list item widgets.
		qtractorTrackListButtons *buttons;
		qtractorPluginListView *plugins;
		QWidget *meters;
	};

	// Model cache item list.
	QList<Item *> m_items;

	// Model cache track map.
	QHash<qtractorTrack *, Item *> m_tracks;

	// Current selection map.
	QHash<int, Item *> m_select;

	// Current selected row.
	int m_iCurrentTrack;

	// The current selecting/dragging item stuff.
	enum DragState {
		DragNone = 0, DragStart, DragMove, DragResize, DragSelect
	} m_dragState;

	// For whether we're resizing or moving an item;
	QPoint m_posDrag;
	int    m_iDragTrack;
	int    m_iDragY;

	qtractorRubberBand *m_pRubberBand;

	// To avoid update reentries.
	int m_iUpdateContents;

	// Track icon pixmap stuff.
	enum { IconAudio = 0, IconMidi = 1, IconCount = 2 };

	QPixmap *m_pPixmap[IconCount];
};


#endif  // __qtractorTrackList_h


// end of qtractorTrackList.h
