// qtractorMidiEventList.h
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiEventList_h
#define __qtractorMidiEventList_h

#include <QTreeView>
#include <QDockWidget>


// Forwards.
class qtractorMidiEditor;
class qtractorMidiSequence;
class qtractorMidiEvent;

class QAbstractItemModel;


//----------------------------------------------------------------------------
// qtractorMidiEventListView -- Custom (tree) list view.

class qtractorMidiEventListView : public QTreeView
{
public:

	// Constructor.
	qtractorMidiEventListView(QWidget *pParent = 0);

	// Destructor.
	~qtractorMidiEventListView();

	// Settlers.
	void setEditor(qtractorMidiEditor *pEditor);
	qtractorMidiEditor *editor() const;

	// Refreshener.
	void refresh();

	// Locators.
	qtractorMidiEvent *eventOfIndex(const QModelIndex& index) const;
	QModelIndex indexOfEvent(qtractorMidiEvent *pEvent) const;

	QModelIndex indexFromTick(unsigned long iTick) const;
	unsigned long tickFromIndex(const QModelIndex& index) const;

	QModelIndex indexFromFrame(unsigned long iFrame) const;
	unsigned long frameFromIndex(const QModelIndex& index) const;

	// Selections.
	void selectEvent(qtractorMidiEvent *pEvent, bool bSelect = true);

protected:

	// Custom editor closing stub.
	void closeEditor(QWidget */*pEditor*/,
		QAbstractItemDelegate::EndEditHint /*hint*/);

private:

	// Forward decls.
	class ItemModel;
	class ItemDelegate;

	// Instance variables.
	ItemModel    *m_pItemModel;
	ItemDelegate *m_pItemDelegate;
};


//----------------------------------------------------------------------------
// qtractorMidiEventListView -- MIDI Event List dockable window.

class qtractorMidiEventList : public QDockWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEventList(QWidget *pParent = 0);

	// Destructor.
	~qtractorMidiEventList();

	// Settlers.
	void setEditor(qtractorMidiEditor *pEditor);
	qtractorMidiEditor *editor() const;

	// Event list view refreshner.
	void refresh();

protected:

	// Full update when show up.
	void showEvent(QShowEvent *);

	// Just about to notify main-window that we're closing.
	void closeEvent(QCloseEvent *);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

protected slots:

	// Internal list view changes slots.
	void currentRowChangedSlot(const QModelIndex&, const QModelIndex&);
	void selectionChangedSlot(const QItemSelection&, const QItemSelection&);

	// External editor notification slots.
	void selectNotifySlot(qtractorMidiEditor *);
	void changeNotifySlot(qtractorMidiEditor *);

private:

	// Instance variables.
	qtractorMidiEventListView *m_pListView;

	// Cross-selection fake-mutex.
	int m_iSelectUpdate;
};


#endif  // __qtractorMidiEventList_h

// end of qtractorMidiEventList.h
