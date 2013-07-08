// qtractorTrackList.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <QAbstractListModel>

#include <QPixmap>


// Forward declarations.
class qtractorTrack;
class qtractorTracks;
class qtractorTrackList;
class qtractorTrackButton;
class qtractorRubberBand;
class qtractorMidiManager;

class qtractorCurveButton;

class QHeaderView;

class QResizeEvent;
class QMouseEvent;
class QKeyEvent;


//----------------------------------------------------------------------------
// qtractorTrackItemWidget -- Track button layout widget.

class qtractorTrackItemWidget : public QWidget
{
//	Q_OBJECT

public:

	// Constructor.
	qtractorTrackItemWidget(
		qtractorTrackList *pTrackList, qtractorTrack *pTrack);

	// Local child widgets accessors.
	qtractorCurveButton *curveButton() const
		{ return m_pCurveButton; }
	qtractorTrackButton *recordButton() const
		{ return m_pRecordButton; }
	qtractorTrackButton *muteButton() const
		{ return m_pMuteButton; }
	qtractorTrackButton *soloButton() const
		{ return m_pSoloButton; }

private:

	// The local child widgets.
	qtractorCurveButton *m_pCurveButton;
	qtractorTrackButton *m_pRecordButton;
	qtractorTrackButton *m_pMuteButton;
	qtractorTrackButton *m_pSoloButton;
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

	// Main table cleaner.
	void clear();

	// Update list content height.
	void updateContentsHeight();

	// Rectangular contents update.
	void updateContents(const QRect& rect);
	// Overall contents update.
	void updateContents();

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

	// Show and move rubber-band item.
	void moveRubberBand(const QPoint& posDrag);

	// Make sure the given (track) rectangle is visible.
	bool ensureVisibleRect(const QRect& rect);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *pKeyEvent);

	// Reset drag/select/move state.
	void resetDragState();

signals:

	// More like current row has changed.
	void selectionChanged();

protected slots:

	// To have timeline in h-sync with main track view.
	void contentsYMovingSlot(int cx, int cy);

	// (Re)create the time scale pixmap.
	void updatePixmap(int cx, int cy);

	// Update header extents.
	void updateHeader();

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
		Item(qtractorTrackList *pTrackList, qtractorTrack *pTrack);
		// Destructor.
		~Item();
		// Bank/program names helper.
		bool updateBankProgram (qtractorMidiManager *pMidiManager,
			const QString& sInstrument,
			QString& sBank, QString& sProgram ) const;
		// Item updater.
		void update(qtractorTrackList *pTrackList);
		// Item members.
		qtractorTrack *track;
		QStringList    text;
		// Track-list item widget.
		qtractorTrackItemWidget *widget;
	};

	// Model cache item list.
	QList<Item *> m_items;

	// Current selected row.
	int m_iCurrentTrack;

	// The current selecting/dragging item stuff.
	enum DragState {
		DragNone = 0, DragStart, DragMove, DragResize
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


//----------------------------------------------------------------------------
// qtractorTrackListHeaderModel -- Track-list header model.

class qtractorTrackListHeaderModel : public QAbstractListModel
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackListHeaderModel(QObject *pParent = 0);

	QVariant headerData(int section, Qt::Orientation orient, int role) const;

	int rowCount(const QModelIndex&) const
		{ return 0; }

	int columnCount(const QModelIndex&) const
		{ return m_headerText.count(); }

	QVariant data(const QModelIndex&, int) const
		{ return QVariant(); }

private:

	// Model variables.
	QStringList m_headerText;
};


#endif  // __qtractorTrackList_h


// end of qtractorTrackList.h
