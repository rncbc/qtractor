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


//----------------------------------------------------------------------------
// qtractorSessionListModel -- Session hierarchy item model.

class qtractorSessionListModel : public QAbstractItemModel
{
	Q_OBJECT

public:

	// Item type tags (stored via Qt::UserRole on column 0).
	enum ItemType {
		ItemTracksGroup = 1,
		ItemInputBusesGroup,
		ItemOutputBusesGroup,
		ItemTrack,
		ItemBus,
		ItemPluginsGroup,
		ItemPlugin,
		ItemClipsGroup,
		ItemClip
	};

	// Constructor.
	explicit qtractorSessionListModel(QObject *pParent = nullptr);

	// Destructor.
	~qtractorSessionListModel();

	// Rebuild the node tree from the current session state.
	void refresh();

	// Clear the node tree.
	void clear();

	// QAbstractItemModel interface.
	QModelIndex index(int row, int column,
		const QModelIndex& parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex& child) const override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	int columnCount(const QModelIndex& parent = QModelIndex()) const override;

	QVariant data(const QModelIndex& index,
		int role = Qt::DisplayRole) const override;

	QVariant headerData(int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;

	Qt::ItemFlags flags(const QModelIndex& index) const override;

private:

	// Forward declaration of the internal node type.
	struct Node;

	// Build helpers.
	void buildTree();
	Node *buildPluginsNode(Node *pParent,
		struct qtractorPluginList *pPluginList);
	Node *buildTrackNode(Node *pParent,
		struct qtractorTrack *pTrack);
	Node *buildBusNode(Node *pParent,
		struct qtractorBus *pBus,
		int busMode = 0);

	// Root node of the tree.
	Node *m_pRoot;
};


//----------------------------------------------------------------------------
// qtractorSessionListView -- Session hierarchy tree view.

class qtractorSessionListView : public QTreeView
{
	Q_OBJECT

public:

	// Constructor.
	explicit qtractorSessionListView(QWidget *pParent = nullptr);

	// Destructor.
	~qtractorSessionListView();

	// Rebuild the underlying model.
	void refresh();

	// Clear the underlying model.
	void clear();

protected slots:

	// Navigate to track on double-click.
	void activatedSlot(const QModelIndex& index);

private:

	qtractorSessionListModel *m_pModel;
};


//----------------------------------------------------------------------------
// qtractorSessionList -- Session overview dockable window.

class qtractorSessionList : public QDockWidget
{
	Q_OBJECT

public:

	// Constructor.
	explicit qtractorSessionList(QWidget *pParent = nullptr);

	// Destructor.
	~qtractorSessionList();

	// Full refresh delegate.
	void refresh();

	// Clear all contents.
	void clear();

public slots:

	// Refresh on demand (e.g. after session load/clear).
	void refreshSlot();

protected:

	// Reveal full refresh on show.
	void showEvent(QShowEvent *pShowEvent);

	// Notify main-window on close.
	void closeEvent(QCloseEvent *pCloseEvent);

private:

	// The contained tree view.
	qtractorSessionListView *m_pTreeView;
};


#endif  // __qtractorSessionList_h

// end of qtractorSessionList.h
