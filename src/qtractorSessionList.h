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
	void refresh(bool bReset);

	// Clear the underlying model.
	void clear();

	// Forward decls.
	class ItemModel;
	class ItemDelegate;

protected:

	// Draw the colour ribbon on track/clip rows before normal cell painting.
	void drawRow(QPainter *pPainter,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const override;

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
	void refresh(bool bReset);

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

	// Double-click slot.
	void doubleClickedSlot(const QModelIndex&);

	// Bus-menu action slots.
	void busInputsSlot();
	void busOutputsSlot();
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

	// Bus properties dialog summoner.
	void busProperties(qtractorBus *pBus);

private:

	// The contained tree view.
	qtractorSessionListView *m_pListView;

	// Selecting track guard.
	int m_iSelectTrack;

	// Bus-menu interim parameters.
	qtractorBus *m_pBus;
};


#endif  // __qtractorSessionList_h

// end of qtractorSessionList.h
