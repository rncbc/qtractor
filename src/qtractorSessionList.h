// qtractorSessionList.h
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

#ifndef __qtractorSessionList_h
#define __qtractorSessionList_h

#include <QAbstractItemModel>
#include <QDockWidget>
#include <QTreeView>
#include <QHash>


// Forward decls.
class qtractorTrack;
class qtractorClip;
class qtractorBus;


//----------------------------------------------------------------------------
// qtractorSessionListView -- Session hierarchy tree view.

class qtractorSessionListView : public QTreeView
{
public:

	// Constructor.
	qtractorSessionListView(QWidget *pParent = nullptr);

	// Destructor.
	~qtractorSessionListView();

	// Rebuild the underlying model.
	void refresh();

	// Clear the underlying model.
	void clear();

	// Forward decls.
	class ItemModel;

private:

	ItemModel *m_pItemModel;
};


//----------------------------------------------------------------------------
// qtractorSessionList -- Session overview dockable window.

class qtractorSessionList : public QDockWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorSessionList(QWidget *pParent = nullptr);

	// Destructor.
	~qtractorSessionList();

	// Full refresh delegate.
	void refresh();

	// Clear all contents.
	void clear();

	// Track/Clip item selection.
	void selectTrack(qtractorTrack *pTrack);
	void selectClip(qtractorClip *pClip);

	// State saver/loader.
	QByteArray saveState() const;
	bool restoreState(const QByteArray& state);

protected slots:

	// Selection change slot.
	void currentRowChangedSlot(const QModelIndex&, const QModelIndex&);

	// Bus-menu action slots.
	void busConnectionsSlot();
	void busPropertiesSlot();

protected:

	// Reveal full refresh on show.
	void showEvent(QShowEvent *);

	// Notify main-window on close.
	void closeEvent(QCloseEvent *);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

	// Buses context menu builder and executive.
	void busMenu(const QPoint& pos);

private:

	// The contained tree view.
	qtractorSessionListView *m_pListView;

	// Bus-menu interim parameters.
	qtractorBus *m_pBus;
	int m_busMode;
};


#endif  // __qtractorSessionList_h

// end of qtractorSessionList.h
