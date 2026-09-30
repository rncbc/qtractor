// qtractorSessionList.cpp
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

#include "qtractorAbout.h"
#include "qtractorSessionList.h"

#include "qtractorSession.h"
#include "qtractorTrack.h"
#include "qtractorClip.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"

#include "qtractorMainForm.h"
#include "qtractorConnections.h"
#include "qtractorBusForm.h"
#include "qtractorTracks.h"

#include "qtractorTrackCommand.h"

#include <QHeaderView>
#include <QPainter>
#include <QStyledItemDelegate>

#include <QSet>

#include <QFileInfo>

#include <QContextMenuEvent>

#include <QShowEvent>
#include <QCloseEvent>

// Drag-n-drop stuff.
//
#include <QApplication>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QDrag>
#include <QRubberBand>


//----------------------------------------------------------------------------
// qtractorSessionListView::ItemModel -- Session hierarchy item model.

class qtractorSessionListView::ItemModel : public QAbstractItemModel
{
public:

	// Item type tags (stored via Qt::UserRole on column 0).
	enum ItemType {
		ItemRoot = 0,
		ItemInputs,
		ItemOutputs,
		ItemTracks,
		ItemTrack,
		ItemClip,
		ItemBus
	};

	// Constructor.
	ItemModel(QObject *pParent = nullptr);

	// Destructor.
	~ItemModel();

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

	// Helper acessors (inline).
	ItemType itemType ( const QModelIndex& index ) const
	{
		const QModelIndex& col0 = index.sibling(index.row(), 0);
		return ItemType(data(col0, Qt::UserRole).toInt());
	}

	void *itemPointer ( const QModelIndex& index ) const
	{
		const QModelIndex& col1 = index.sibling(index.row(), 1);
		return data(col1, Qt::UserRole).value<void *> ();
	}

	// Helper locators.
	QModelIndex indexOfGroup(ItemType itype) const;

	qtractorTrack *trackOfIndex(const QModelIndex& index) const;
	QModelIndex indexOfTrack(qtractorTrack *pTrack) const;

	qtractorClip *clipOfIndex(const QModelIndex& index) const;
	QModelIndex indexOfClip(qtractorClip *pClip) const;

	qtractorBus *busOfIndex(const QModelIndex& index) const;
	QModelIndex indexOfBus(qtractorBus *pBus, int busMode) const;
	int busModeOfIndex(const QModelIndex& index) const;

protected:

	// Helper functions.
	QString trackTypeText(qtractorTrack::TrackType trackType) const
	{
		QString sText;
		switch (trackType) {
		case qtractorTrack::Audio:
			sText = tr("Audio");
			break;
		case qtractorTrack::Midi:
			sText = tr("MIDI");
			break;
		default:
			break;
		}
		return sText;
	}

	QIcon trackTypeIcon(qtractorTrack::TrackType trackType) const
	{
		QIcon icon;
		switch (trackType) {
		case qtractorTrack::Audio:
			icon = QIcon::fromTheme("trackAudio");
			break;
		case qtractorTrack::Midi:
			icon = QIcon::fromTheme("trackMidi");
			break;
		default:
			break;
		}
		return icon;
	}

	// Live item data roles.
	QVariant dataDisplayRole(const QModelIndex& index) const;
	QVariant dataDecorationRole(const QModelIndex& index) const;
	QVariant dataUserRole(const QModelIndex& index) const;
	QVariant dataFontRole(const QModelIndex& index) const;
	QVariant dataForegroundRole(const QModelIndex& index) const;

private:

	// Forward declaration of the internal node type.
	struct Node;

	// Build helpers.
	Node *nodeForTrack(Node *pGroupNode, qtractorTrack *pTrack) const;
	Node *nodeForClip(Node *pTrackNode, qtractorClip *pClip) const;
	Node *nodeForBus(Node *pGroupNode, qtractorBus *pBus, int busMode) const;

	// Compute live row for a track/bus/clip node.
	int rowOfTrack(qtractorTrack *pTrack) const;
	int rowOfClip(qtractorTrack *pTrack, qtractorClip *pClip) const;
	int rowOfBus(qtractorBus *pBus, int busMode) const;
	bool rowOfBusPtr(qtractorEngine *pEngine,
		qtractorBus *pBus, int busMode, int& iRow) const;

	// Per-engine bus helpers (call once per engine, audio then MIDI).
	void addBusKeys(qtractorEngine *pEngine, QSet<void *>& keys) const;
	int countBusesByMode(qtractorEngine *pEngine, int busMode) const;
	QModelIndex indexOfBusRow(qtractorEngine *pEngine,
		Node *pGroupNode, int busMode, int row, int& iRow, int column) const;
	QModelIndex indexOfBusPtr(qtractorEngine *pEngine,
		const QModelIndex& groupIdx,
		qtractorBus *pBus, int busMode, int& iRow) const;

	// Root node of the tree (holds exactly the group nodes as its children).
	Node *m_pRoot;

	// Top-level group nodes (root's children).
	Node *m_pInputs;
	Node *m_pTracks;
	Node *m_pOutputs;

	// Pool of live-data nodes keyed by session-object pointer.
	// Entries are created on demand in index() and cleared on refresh/clear.
	mutable QHash<void *, Node *> m_nodePool;
};


//----------------------------------------------------------------------------
// qtractorSessionListView::ItemModel::Node -- Internal tree node.
//
// Used for:
//   - the invisible root sentinel (type==0, parent==nullptr)
//   - the three group nodes: Inputs, Tracks, Outputs (parent==m_pRoot)
//   - live-data nodes for tracks, buses, and clips
//     (created on demand in m_nodePool, parent == group or track node)
//
// Group nodes are owned by m_pRoot via m_pRoot->children.
// Live-data nodes are owned by m_nodePool.
// There are NO Node::children lists — child rows are computed from live
// session data (qtractorSession::tracks(), qtractorTrack::clips(),
// engine::buses()) on every model query.

struct qtractorSessionListView::ItemModel::Node
{
	// Constructor.
	Node(Node *pParent, ItemType itemType, void *pData = nullptr)
		: parent(pParent), type(itemType), data(pData), cachedRow(0) {}

	// Destructor -- recursively deletes children (used only for m_pRoot
	// to clean up the group nodes; live-data nodes are owned by m_nodePool).
	~Node() { qDeleteAll(children); }

	// Row index within parent — for group nodes this is their position
	// among m_pRoot's children; for live-data nodes it is set when the
	// node is created in nodeFor*() and updated lazily by row().
	int row() const
	{
		if (parent && parent->parent == nullptr) {
			// This is a group node: find position among siblings.
			// Group nodes are stored in m_pRoot->children.
			return parent->children.indexOf(const_cast<Node *> (this));
		}
		// For live-data nodes the caller stores the row in cachedRow.
		return cachedRow;
	}

	Node        *parent;
	ItemType     type;
	void        *data;      // raw pointer to session object (may be nullptr)
	int          cachedRow; // position within parent's live list

	// Group nodes use this to hold the 3 child Node pointers
	// (Inputs/Tracks/Outputs as immediate children of m_pRoot).
	// This list is ONLY populated on m_pRoot itself.
	QList<Node *> children;
};


//----------------------------------------------------------------------------
// qtractorSessionListView::ItemModel -- QAbstractItemModel implementation.

qtractorSessionListView::ItemModel::ItemModel ( QObject *pParent )
	: QAbstractItemModel(pParent), m_pRoot(nullptr),
		m_pInputs(nullptr),
		m_pTracks(nullptr),
		m_pOutputs(nullptr)
{
}


qtractorSessionListView::ItemModel::~ItemModel (void)
{
	qDeleteAll(m_nodePool);
	m_nodePool.clear();

	if (m_pRoot) delete m_pRoot;
}


// Clear the node tree.
void qtractorSessionListView::ItemModel::clear (void)
{
	beginResetModel();

	qDeleteAll(m_nodePool);
	m_nodePool.clear();

	if (m_pRoot) {
		delete m_pRoot;
		m_pRoot = nullptr;
	}

	m_pInputs = nullptr;
	m_pTracks = nullptr;
	m_pOutputs = nullptr;

	endResetModel();
}


// Refresh the top-level nodes (Inputs / Tracks / Outputs);
// no sub-node children are created here; all deeper rows
// are read live from the current session state.
void qtractorSessionListView::ItemModel::refresh (void)
{
	// 0. Root is the invisible sentinel; top-level groups are its children.
	if (m_pRoot == nullptr)
		m_pRoot = new Node(nullptr, ItemRoot);

	// Create group-nodes if not there yet..

	// 1. Inputs group.
	if (m_pInputs == nullptr) {
		m_pInputs = new Node(m_pRoot, ItemInputs);
		m_pInputs->cachedRow = m_pRoot->children.count();
		m_pRoot->children.append(m_pInputs);
	}

	// 2. Tracks group.
	if (m_pTracks == nullptr) {
		m_pTracks = new Node(m_pRoot, ItemTracks);
		m_pTracks->cachedRow = m_pRoot->children.count();
		m_pRoot->children.append(m_pTracks);
	}

	// 3. Outputs group.
	if (m_pOutputs == nullptr) {
		m_pOutputs = new Node(m_pRoot, ItemOutputs);
		m_pOutputs->cachedRow = m_pRoot->children.count();
		m_pRoot->children.append(m_pOutputs);
	}

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// ----------------------------------------------------------------
	// Use beginResetModel()/endResetModel() rather than the
	// layoutAboutToBeChanged()/layoutChanged() pair.
	//
	// The layout-change pair causes QAbstractItemView to snapshot every
	// current QPersistentModelIndex (which hold raw Node* pointers) into
	// the view's private state between the two signals.  On layoutChanged()
	// the view iterates those snapshots and calls parent() on each one,
	// dereferencing the Node* internalPointer().  If any of those nodes
	// were freed in between (stale clip/track nodes), that is a
	// heap-use-after-free.  changePersistentIndexList() cannot help here
	// because it only patches the model's own persistent-index table, not
	// the view's snapshot list.
	//
	// beginResetModel()/endResetModel() unconditionally discards all
	// persistent indexes and view state without ever calling parent(),
	// which is exactly correct for a wholesale refresh of pool contents.
	// ----------------------------------------------------------------
	beginResetModel();

	// Build the set of all currently-live keys.
	QSet<void *> keys;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		keys.insert(static_cast<void *> (pTrack));
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			keys.insert(static_cast<void *> (pClip));
		}
	}
	addBusKeys(pSession->audioEngine(), keys);
	addBusKeys(pSession->midiEngine(), keys);

	// Evict stale pool entries (session objects no longer present).
	// Safe to free immediately: beginResetModel() has already invalidated
	// all persistent indexes, so no live Node* references remain.
	QHash<void *, Node *>::Iterator iter = m_nodePool.begin();
	while (iter != m_nodePool.end()) {
		if (!keys.contains(iter.key())) {
			delete iter.value();
			iter = m_nodePool.erase(iter);
		} else {
			++iter;
		}
	}

	endResetModel();
}


//----------------------------------------------------------------------------
// Live-data node helpers -- create or retrieve a Node from m_nodePool for a
// given session object pointer.  The pool is keyed by the session pointer so
// that the same QModelIndex internalPointer is always returned for the same
// object, which is a Qt model requirement.

// Return the pool node for a track, constructing it if needed.
qtractorSessionListView::ItemModel::Node *
qtractorSessionListView::ItemModel::nodeForTrack (
	Node *pGroupNode, qtractorTrack *pTrack ) const
{
	void *key = static_cast<void *> (pTrack);
	Node *pNode = m_nodePool.value(key, nullptr);
	if (pNode)
		return pNode;

	pNode = new Node(pGroupNode, ItemTrack, key);
	pNode->cachedRow = rowOfTrack(pTrack);
	m_nodePool.insert(key, pNode);
	return pNode;
}


// Return the pool node for a clip, constructing it if needed.
qtractorSessionListView::ItemModel::Node *
qtractorSessionListView::ItemModel::nodeForClip (
	Node *pTrackNode, qtractorClip *pClip ) const
{
	void *key = static_cast<void *> (pClip);
	Node *pNode = m_nodePool.value(key, nullptr);
	if (pNode)
		return pNode;

	qtractorTrack *pTrack = pClip->track();

	pNode = new Node(pTrackNode, ItemClip, key);
	pNode->cachedRow = (pTrack ? rowOfClip(pTrack, pClip) : 0);
	m_nodePool.insert(key, pNode);
	return pNode;
}


// Return the pool node for a bus, constructing it if needed.
// A duplex bus may appear in both the Inputs and Outputs groups; use a
// composite key = (pointer | busMode) to get distinct nodes for each side.
qtractorSessionListView::ItemModel::Node *
qtractorSessionListView::ItemModel::nodeForBus (
	Node *pGroupNode, qtractorBus *pBus, int busMode ) const
{
	// Composite key: encode busMode into the low bits of the pointer.
	// qtractorBus objects are heap-allocated and thus at least 4-byte aligned,
	// so the low 2 bits are always 0 in the raw pointer value.
	void *key = reinterpret_cast<void *> (
		reinterpret_cast<quintptr> (pBus)
		| quintptr(busMode & 0x3));
	Node *pNode = m_nodePool.value(key, nullptr);
	if (pNode)
		return pNode;

	pNode = new Node(pGroupNode, ItemBus, key);
	pNode->cachedRow = rowOfBus(pBus, busMode);
	m_nodePool.insert(key, pNode);
	return pNode;
}


//----------------------------------------------------------------------------
// Live row-position helpers -- walk the live session lists to find the
// ordinal position of a given object within its parent's collection.

int qtractorSessionListView::ItemModel::rowOfTrack (
	qtractorTrack *pTrack ) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return 0;

	int iRow = 0;
	for (qtractorTrack *p = pSession->tracks().first(); p; p = p->next()) {
		if (p == pTrack)
			return iRow;
		++iRow;
	}

	return 0;
}


int qtractorSessionListView::ItemModel::rowOfClip (
	qtractorTrack *pTrack, qtractorClip *pClip ) const
{
	int iRow = 0;
	for (qtractorClip *p = pTrack->clips().first(); p; p = p->next()) {
		if (p == pClip)
			return iRow;
		++iRow;
	}

	return 0;
}


// Count how many buses precede pBus in the merged Input or Output list
// (audio engine first, then MIDI engine).
int qtractorSessionListView::ItemModel::rowOfBus (
	qtractorBus *pBus, int busMode ) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return 0;

	int iRow = 0;
	if (rowOfBusPtr(pSession->audioEngine(), pBus, busMode, iRow))
		return iRow;
	rowOfBusPtr(pSession->midiEngine(), pBus, busMode, iRow);
	return iRow;
}


bool qtractorSessionListView::ItemModel::rowOfBusPtr (
	qtractorEngine *pEngine, qtractorBus *pBus, int busMode, int& iRow ) const
{
	if (pEngine == nullptr)
		return false;

	const qtractorBus::BusMode bm
		= qtractorBus::BusMode(busMode);

	for (qtractorBus *p = pEngine->buses().first(); p; p = p->next()) {
		if (!(p->busMode() & bm))
			continue;
		if (p == pBus)
			return true;
		++iRow;
	}

	return false;
}


// Per-engine bus helpers -- each operates on a single engine; callers
// invoke them twice (audio engine first, then MIDI engine).
//
// Insert composite bus keys (pointer | busMode) for one engine into keys.
void qtractorSessionListView::ItemModel::addBusKeys (
	qtractorEngine *pEngine , QSet<void *>& keys ) const
{
	if (pEngine == nullptr)
		return;

	for (qtractorBus *pBus = pEngine->buses().first();
			pBus; pBus = pBus->next()) {
		const qtractorBus::BusMode busMode = pBus->busMode();
		if (busMode & qtractorBus::Input)
			keys.insert(reinterpret_cast<void *> (
				reinterpret_cast<quintptr> (pBus)
				| quintptr(qtractorBus::Input  & 0x3)));
		if (busMode & qtractorBus::Output)
			keys.insert(reinterpret_cast<void *> (
				reinterpret_cast<quintptr> (pBus)
				| quintptr(qtractorBus::Output & 0x3)));
	}
}


// Count buses of the given mode in one engine.
int qtractorSessionListView::ItemModel::countBusesByMode (
	qtractorEngine *pEngine, int busMode ) const
{
	if (pEngine == nullptr)
		return 0;

	const qtractorBus::BusMode bm
		= qtractorBus::BusMode(busMode);

	int n = 0;
	for (qtractorBus *p = pEngine->buses().first(); p; p = p->next())
		if (p->busMode() & bm) ++n;

	return n;
}


// Search one engine for the bus at ordinal position row (within the merged
// list).  iRow is the running counter across successive engine calls; it is
// updated in place so the caller can pass it unchanged into the next call.
QModelIndex
qtractorSessionListView::ItemModel::indexOfBusRow (
	qtractorEngine *pEngine, Node *pGroupNode,
	int busMode, int row, int& iRow, int column ) const
{
	if (pEngine == nullptr)
		return QModelIndex();

	const qtractorBus::BusMode bm
		= qtractorBus::BusMode(busMode);

	for (qtractorBus *pBus = pEngine->buses().first();
			pBus; pBus = pBus->next()) {
		if (!(pBus->busMode() & bm))
			continue;
		if (iRow == row) {
			Node *pNode = nodeForBus(pGroupNode, pBus, busMode);
			return createIndex(row, column, pNode);
		}
		++iRow;
	}

	return QModelIndex();
}


// Search one engine for the specific bus pointer (within the merged list).
// iRow is the running counter across successive engine calls; it is
// updated in place so the caller can pass it unchanged into the next call.
QModelIndex
qtractorSessionListView::ItemModel::indexOfBusPtr (
	qtractorEngine *pEngine, const QModelIndex& groupIdx,
	qtractorBus *pBus, int busMode, int& iRow ) const
{
	if (pEngine == nullptr)
		return QModelIndex();

	const qtractorBus::BusMode bm
		= qtractorBus::BusMode(busMode);

	for (qtractorBus *p = pEngine->buses().first(); p; p = p->next()) {
		if (!(p->busMode() & bm))
			continue;
		if (p == pBus)
			return index(iRow, 0, groupIdx);
		++iRow;
	}

	return QModelIndex();
}


// QAbstractItemModel virtuals.
//
QModelIndex
qtractorSessionListView::ItemModel::index (
	int row, int column, const QModelIndex& parent ) const
{
	if (m_pRoot == nullptr)
		return QModelIndex();

	// Top level: parent is invalid, children are the group nodes.
	if (!parent.isValid()) {
		if (row < 0 || row >= m_pRoot->children.count())
			return QModelIndex();
		return createIndex(row, column, m_pRoot->children.at(row));
	}

	Node *pParentNode = static_cast<Node *> (parent.internalPointer());

	// Group node -> children are tracks or buses read from live session data.
	if (pParentNode->parent == m_pRoot) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession == nullptr)
			return QModelIndex();
		switch (pParentNode->type) {
		case ItemTracks: {
			// Walk tracks to the requested row.
			int iRow = 0;
			for (qtractorTrack *pTrack = pSession->tracks().first();
					pTrack; pTrack = pTrack->next()) {
				if (iRow == row) {
					Node *pNode = nodeForTrack(pParentNode, pTrack);
					return createIndex(row, column, pNode);
				}
				++iRow;
			}
			return QModelIndex();
		}
		case ItemInputs:
		case ItemOutputs: {
			const qtractorBus::BusMode bm
				= (pParentNode->type == ItemInputs)
				? qtractorBus::Input
				: qtractorBus::Output;
			int iRow = 0;
			const QModelIndex& idx
				= indexOfBusRow(pSession->audioEngine(),
					pParentNode, bm, row, iRow, column);
			if (idx.isValid())
				return idx;
			return indexOfBusRow(pSession->midiEngine(),
				pParentNode, bm, row, iRow, column);
		}
		default:
			return QModelIndex();
		}
	}

	// Track node -> children are clips read from the live track clip list.
	if (pParentNode->type == ItemTrack) {
		qtractorTrack *pTrack
			= static_cast<qtractorTrack *> (pParentNode->data);
		if (pTrack == nullptr)
			return QModelIndex();
		int iRow = 0;
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			if (iRow == row) {
				Node *pNode = nodeForClip(pParentNode, pClip);
				return createIndex(row, column, pNode);
			}
			++iRow;
		}
		return QModelIndex();
	}

	return QModelIndex();
}


QModelIndex
qtractorSessionListView::ItemModel::parent (
	const QModelIndex& child ) const
{
	if (!child.isValid() || m_pRoot == nullptr)
		return QModelIndex();

	// Children of the root (group nodes) have no visible parent.
	Node *pChildNode = static_cast<Node *> (child.internalPointer());
	if (pChildNode == nullptr)
		return QModelIndex();

	Node *pParentNode = pChildNode->parent;
	if (pParentNode == nullptr || pParentNode == m_pRoot)
		return QModelIndex();

	// The parent is itself a group or track node.
	return createIndex(pParentNode->cachedRow, 0, pParentNode);
}


int qtractorSessionListView::ItemModel::rowCount (
	const QModelIndex& parent ) const
{
	if (m_pRoot == nullptr)
		return 0;

	// Invalid parent -> top-level groups.
	if (!parent.isValid())
		return m_pRoot->children.count();

	Node *pNode = static_cast<Node *> (parent.internalPointer());

	// Group nodes: count from live session data.
	if (pNode->parent == m_pRoot) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession == nullptr)
			return 0;
		switch (pNode->type) {
		case ItemTracks:
			return pSession->tracks().count();
		case ItemInputs:
		case ItemOutputs: {
			const qtractorBus::BusMode bm
				= (pNode->type == ItemInputs)
				? qtractorBus::Input
				: qtractorBus::Output;
			return countBusesByMode(pSession->audioEngine(), bm)
				+  countBusesByMode(pSession->midiEngine(),  bm);
		}
		default:
			return 0;
		}
	}

	// Track nodes: count clips from the live track.
	if (pNode->type == ItemTrack) {
		qtractorTrack *pTrack = static_cast<qtractorTrack *> (pNode->data);
		return (pTrack ? pTrack->clips().count() : 0);
	}

	// Clips and buses have no children.
	return 0;
}


int qtractorSessionListView::ItemModel::columnCount (
	const QModelIndex& /*parent*/ ) const
{
	return 2; // Name | Detail
}


QVariant
qtractorSessionListView::ItemModel::data (
	const QModelIndex& index, int role ) const
{
	if (!index.isValid() || m_pRoot == nullptr)
		return QVariant();

	switch (role) {
	case Qt::DisplayRole:
	case Qt::ToolTipRole:
		return dataDisplayRole(index);
	case Qt::DecorationRole:
		return dataDecorationRole(index);
	case Qt::UserRole:
		return dataUserRole(index);
	case Qt::FontRole:
		return dataFontRole(index);
	case Qt::ForegroundRole:
		return dataForegroundRole(index);
	default:
		break;
	}

	return QVariant();
}


QVariant
qtractorSessionListView::ItemModel::headerData (
	int section, Qt::Orientation orientation, int role ) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		switch (section) {
			case 0: return tr("Name");
			case 1: return tr("Detail");
			default: break;
		}
	}
	return QVariant();
}


Qt::ItemFlags
qtractorSessionListView::ItemModel::flags (
	const QModelIndex& index ) const
{
	Qt::ItemFlags ret = Qt::NoItemFlags;

	if (index.isValid()) {
		ret |= Qt::ItemIsEnabled;
		const QModelIndex& col0 = index.sibling(index.row(), 0);
		const int iType = data(col0, Qt::UserRole).toInt();
		if (iType == ItemTrack || iType == ItemClip)
			ret |= Qt::ItemIsSelectable;
	}

	return ret;
}


// Helper locators.
//

QModelIndex
qtractorSessionListView::ItemModel::indexOfGroup (
	ItemType groupType ) const
{
	const int iGroupCount = rowCount();
	for (int iGroup = 0; iGroup < iGroupCount; ++iGroup) {
		const QModelIndex& groupIdx = index(iGroup, 0);
		if (itemType(groupIdx) == groupType)
			return groupIdx;
	}

	return QModelIndex();
}


qtractorTrack *
qtractorSessionListView::ItemModel::trackOfIndex (
	const QModelIndex& index ) const
{
	if (itemType(index) == ItemTrack)
		return static_cast<qtractorTrack *> (itemPointer(index));
	else
		return nullptr;
}


QModelIndex
qtractorSessionListView::ItemModel::indexOfTrack (
	qtractorTrack *pTrack ) const
{
	if (pTrack == nullptr)
		return QModelIndex();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return QModelIndex();

	const QModelIndex& groupIdx = indexOfGroup(ItemTracks);
	if (!groupIdx.isValid())
		return QModelIndex();

	int iRow = 0;
	for (qtractorTrack *p = pSession->tracks().first(); p; p = p->next()) {
		if (p == pTrack)
			return index(iRow, 0, groupIdx);
		++iRow;
	}

	return QModelIndex();
}


qtractorClip *
qtractorSessionListView::ItemModel::clipOfIndex (
	const QModelIndex& index ) const
{
	if (itemType(index) == ItemClip)
		return static_cast<qtractorClip *> (itemPointer(index));
	else
		return nullptr;
}


QModelIndex
qtractorSessionListView::ItemModel::indexOfClip (
	qtractorClip *pClip ) const
{
	if (pClip == nullptr)
		return QModelIndex();

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == nullptr)
		return QModelIndex();

	const QModelIndex& trackIdx = indexOfTrack(pTrack);
	if (!trackIdx.isValid())
		return QModelIndex();

	int iRow = 0;
	for (qtractorClip *p = pTrack->clips().first(); p; p = p->next()) {
		if (p == pClip)
			return index(iRow, 0, trackIdx);
		++iRow;
	}

	return QModelIndex();
}


qtractorBus *
qtractorSessionListView::ItemModel::busOfIndex (
	const QModelIndex& index ) const
{
	if (itemType(index) == ItemBus) {
		// The data pointer for a bus node is the composite
		// key (pointer|busMode); recover the real pointer
		// by masking off the low 2 bits.
		void *key = itemPointer(index);
		return reinterpret_cast<qtractorBus *> (
			reinterpret_cast<quintptr> (key) & ~quintptr(0x3));
	} else {
		return nullptr;
	}
}


QModelIndex
qtractorSessionListView::ItemModel::indexOfBus (
	qtractorBus *pBus, int busMode ) const
{
	if (pBus == nullptr)
		return QModelIndex();

	ItemType groupType;
	switch (qtractorBus::BusMode(busMode)) {
	case qtractorBus::Input:
		groupType = ItemInputs;
		break;
	case qtractorBus::Output:
		groupType = ItemOutputs;
		break;
	default:
		return QModelIndex();
	}

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return QModelIndex();

	const QModelIndex& groupIdx = indexOfGroup(groupType);
	if (!groupIdx.isValid())
		return QModelIndex();

	const qtractorBus::BusMode bm
		= qtractorBus::BusMode(busMode);

	int iRow = 0;
	const QModelIndex& idx
		= indexOfBusPtr(pSession->audioEngine(), groupIdx, pBus, bm, iRow);
	if (idx.isValid())
		return idx;
	return indexOfBusPtr(pSession->midiEngine(), groupIdx, pBus, bm, iRow);
}


int qtractorSessionListView::ItemModel::busModeOfIndex (
	const QModelIndex& index ) const
{
	if (itemType(index) == ItemBus) {
		// The data pointer for a bus node is the composite
		// key (pointer|busMode); recover the real pointer
		// by masking off the low 2 bits.
		void *key = itemPointer(index);
		return reinterpret_cast<quintptr> (key) & quintptr(0x3);
	} else {
		return 0;
	}
}


// Live item data roles.
//
QVariant
qtractorSessionListView::ItemModel::dataDisplayRole (
	const QModelIndex& index ) const
{
	Node *pNode = static_cast<Node *> (index.internalPointer());

	QString sText;

	switch (pNode->type) {
	case ItemInputs:
		if (index.column() == 0) {
			sText = tr("Inputs");
		} else {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				const int iInputsCount
					= countBusesByMode(pSession->audioEngine(), qtractorBus::Input)
					+ countBusesByMode(pSession->midiEngine(),  qtractorBus::Input);
				sText = QString("[%1]").arg(iInputsCount);
			}
		}
		break;
	case ItemTracks:
		if (index.column() == 0) {
			sText = tr("Tracks");
		} else {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession)
				sText = QString("[%1]").arg(pSession->tracks().count());
		}
		break;
	case ItemOutputs:
		if (index.column() == 0) {
			sText = tr("Outputs");
		} else {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				const int iOutputsCount
					= countBusesByMode(pSession->audioEngine(), qtractorBus::Output)
					+ countBusesByMode(pSession->midiEngine(),  qtractorBus::Output);
				sText = QString("[%1]").arg(iOutputsCount);
			}
		}
		break;
	case ItemTrack: {
		qtractorTrack *pTrack = static_cast<qtractorTrack *> (pNode->data);
		if (pTrack) {
			if (index.column() == 0) {
				sText = pTrack->shortTrackName();
			} else {
				sText  = trackTypeText(pTrack->trackType());
				sText += ' ';
				sText += tr("track");
				sText += ' ';
				sText += QString("[%1]").arg(pTrack->clips().count());
			}
		}
		break;
	}
	case ItemClip: {
		qtractorClip *pClip = static_cast<qtractorClip *> (pNode->data);
		if (pClip) {
			if (index.column() == 0) {
				sText = pClip->clipName();
			} else {
				qtractorTrack *pTrack = pClip->track();
				if (pTrack)
					sText = trackTypeText(pTrack->trackType());
				sText += ' ';
				sText += tr("clip");
			}
		}
		break;
	}
	case ItemBus: {
		qtractorBus *pBus = reinterpret_cast<qtractorBus *> (
			reinterpret_cast<quintptr> (pNode->data) & ~quintptr(0x3));
		if (pBus) {
			if (index.column() == 0) {
				sText = pBus->busName();
			} else {
				unsigned short iChannels = 0;
				switch (pBus->busType()) {
				case qtractorTrack::Audio: {
					qtractorAudioBus *pAudioBus
						= static_cast<qtractorAudioBus *> (pBus);
					if (pAudioBus)
						iChannels = pAudioBus->channels();
					break;
				}
				case qtractorTrack::Midi: {
					qtractorMidiBus *pMidiBus
						= static_cast<qtractorMidiBus *> (pBus);
					if (pMidiBus)
						iChannels = 16;
					break;
				}
				default:
					break;
				}
				sText = trackTypeText(pBus->busType());
				sText += ' ';
				sText += tr("bus");
				sText += ' ';
				sText += QString("(%1)").arg(iChannels);
			}
		}
		break;
	}
	default:
		break;
	}

	return sText;
}


QVariant
qtractorSessionListView::ItemModel::dataDecorationRole (
	const QModelIndex& index ) const
{
	QIcon icon;

	if (index.column() != 0)
		return icon;

	Node *pNode = static_cast<Node *> (index.internalPointer());

	switch (pNode->type) {
	case ItemTrack: {
		qtractorTrack *pTrack = static_cast<qtractorTrack *> (pNode->data);
		if (pTrack) {
			const QString& sTrackIcon = pTrack->trackIcon();
			if (!sTrackIcon.isEmpty())
				icon = QIcon::fromTheme(sTrackIcon);
			if (icon.isNull())
				icon = trackTypeIcon(pTrack->trackType());
		}
		break;
	}
	case ItemBus: {
		qtractorBus *pBus = reinterpret_cast<qtractorBus *> (
			reinterpret_cast<quintptr> (pNode->data) & ~quintptr(0x3));
		if (pBus)
			icon = trackTypeIcon(pBus->busType());
		break;
	}
	default:
		break;
	}

	return icon;
}


QVariant
qtractorSessionListView::ItemModel::dataUserRole (
	const QModelIndex& index ) const
{
	Node *pNode = static_cast<Node *> (index.internalPointer());

	if (index.column() == 0)
		return pNode->type;
	else
		return QVariant::fromValue(pNode->data);

	return QVariant();
}


QVariant
qtractorSessionListView::ItemModel::dataFontRole (
	const QModelIndex& index ) const
{
	Node *pNode = static_cast<Node *> (index.internalPointer());

	if (index.column() == 0 && pNode->type == ItemTrack) {
		QFont font;
		font.setBold(true);
		return font;
	}

	return QVariant();
}


QVariant
qtractorSessionListView::ItemModel::dataForegroundRole (
	const QModelIndex& index ) const
{
	Node *pNode = static_cast<Node *> (index.internalPointer());

	bool bGrayed = false;

	qtractorTrack *pTrack = nullptr;
	switch (pNode->type) {
	case ItemInputs:
	case ItemTracks:
	case ItemOutputs:
		bGrayed = true;
		break;
	case ItemTrack:
		pTrack = static_cast<qtractorTrack *> (pNode->data);
		break;
	case ItemClip: {
		qtractorClip *pClip = static_cast<qtractorClip *> (pNode->data);
		if (pClip) {
			pTrack = pClip->track();
			bGrayed = pClip->isClipMute();
		}
		break;
	}
	case ItemBus:
	default:
		break;
	}

	if (pTrack && (pTrack->isMute()
		|| (!pTrack->isSolo() && (pTrack->session())->soloTracks())))
		bGrayed = true;
	if (bGrayed)
		return QPalette().color(QPalette::Disabled, QPalette::Text);

	return QVariant();
}


//----------------------------------------------------------------------------
// qtractorSessionListView::ItemDelegate -- shifts Detail-column text right
// by the meter ribbon width for ItemTrack and ItemClip rows.

class qtractorSessionListView::ItemDelegate : public QStyledItemDelegate
{
public:

	ItemDelegate(QObject *pParent = nullptr)
		: QStyledItemDelegate(pParent) {}

	void paint(QPainter *pPainter,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const override
	{
		if (index.column() == 1) {
			const QModelIndex& col0
				= index.sibling(index.row(), 0);
			const int iType
				= col0.data(Qt::UserRole).toInt();
			if (iType == ItemModel::ItemTrack ||
				iType == ItemModel::ItemClip  ||
				iType == ItemModel::ItemBus) {
				QStyleOptionViewItem opt(option);
				opt.rect.adjust(6, 0, 0, 0);
				QStyledItemDelegate::paint(pPainter, opt, index);
				return;
			}
		}
		QStyledItemDelegate::paint(pPainter, option, index);
	}
};


//----------------------------------------------------------------------------
// qtractorSessionListView -- QTreeView wired to qtractorSessionListModel.

qtractorSessionListView::qtractorSessionListView ( QWidget *pParent )
	: QTreeView(pParent), m_pItemModel(nullptr)
{
	m_pItemModel = new ItemModel(this);
	QTreeView::setModel(m_pItemModel);

	QTreeView::setUniformRowHeights(true);
	QTreeView::setAllColumnsShowFocus(true);
	QTreeView::setRootIsDecorated(true);
	QTreeView::setSelectionMode(QAbstractItemView::SingleSelection);
	QTreeView::setEditTriggers(QAbstractItemView::NoEditTriggers);
	QTreeView::setAlternatingRowColors(true);

	QTreeView::setItemDelegateForColumn(1, new ItemDelegate(this));

	QHeaderView *pHeaderView = QTreeView::header();
	pHeaderView->setStretchLastSection(true);
//	pHeaderView->hide();

	// Drag-n-drop stuff.
	//
	m_pRubberBand = nullptr;

	QTreeView::setAcceptDrops(true);
	QTreeView::setAutoScroll(true);
}


qtractorSessionListView::~qtractorSessionListView (void)
{
}


// Draw a vertical colour ribbon on the leftmost edge of each
// ItemTrack and ItemClip row, then let the base class paint the cells.
void qtractorSessionListView::drawRow (
	QPainter *pPainter,
	const QStyleOptionViewItem& option,
	const QModelIndex& index ) const
{
	QTreeView::drawRow(pPainter, option, index);

	// Resolve item type, track pointer, and bus type.
	const int iType = index.data(Qt::UserRole).toInt();

	qtractorTrack *pTrack = nullptr;
	qtractorTrack::TrackType busTrackType = qtractorTrack::None;
	if (iType == ItemModel::ItemTrack) {
		const QModelIndex& col1 = index.sibling(index.row(), 1);
		pTrack = static_cast<qtractorTrack *> (
			col1.data(Qt::UserRole).value<void *> ());
	} else if (iType == ItemModel::ItemClip) {
		const QModelIndex& col1 = index.sibling(index.row(), 1);
		qtractorClip *pClip = static_cast<qtractorClip *> (
			col1.data(Qt::UserRole).value<void *> ());
		if (pClip)
			pTrack = pClip->track();
	} else if (iType == ItemModel::ItemBus) {
		const QModelIndex& col1 = index.sibling(index.row(), 1);
		void *key = col1.data(Qt::UserRole).value<void *> ();
		qtractorBus *pBus = reinterpret_cast<qtractorBus *> (
			reinterpret_cast<quintptr> (key) & ~quintptr(0x3));
		if (pBus)
			busTrackType = pBus->busType();
	}

	if (pTrack == nullptr && busTrackType == qtractorTrack::None)
		return;

	pPainter->save();
	pPainter->setClipping(false);

	// option.rect spans the full row width; its left() is the
	// true leftmost pixel of the row in viewport coordinates.
	// Use the tree's indentation step as the ribbon width — it
	// matches the root-decoration (branch indicator) column width.
	if (pTrack) {
		const QRect ribbonRect(
			option.rect.left(),
			option.rect.top() + (iType == ItemModel::ItemTrack ? 1 : 0),
			QTreeView::indentation(),
			option.rect.height());
		QColor fg = pTrack->foreground().lighter();
		pPainter->fillRect(ribbonRect, fg);
		// For ItemTrack rows, overlay the 1-based track number centred
		// in the ribbon, using the track's background() as text colour.
		if (iType == ItemModel::ItemTrack) {
			QFont font = pPainter->font();
			font.setPointSize(font.pointSize() - 3);
			pPainter->setFont(font);
			const QColor bg = pTrack->background().lighter();
			if (qAbs(bg.value() - fg.value()) < 0x33)
				fg.setHsv(fg.hue(), fg.saturation(), (255 - fg.value()), 200);
			pPainter->setPen(bg);
			pPainter->drawText(ribbonRect, Qt::AlignCenter,
				QString::number(index.row() + 1));
		}
	}

	// Draw a meter-colour ribbon on the left edge of the second column
	// (Detail) for ItemTrack, ItemClip, and ItemBus rows.
	const QModelIndex& col1 = index.sibling(index.row(), 1);
	const QRect& rect = QTreeView::visualRect(col1);
	if (rect.isValid()) {
		const qtractorTrack::TrackType trackType
			= pTrack ? pTrack->trackType() : busTrackType;
		QColor ribbonColor;
		switch (trackType) {
		case qtractorTrack::Audio:
			ribbonColor = qtractorAudioMeter::color(qtractorAudioMeter::Color10dB);
			break;
		case qtractorTrack::Midi:
			ribbonColor = qtractorMidiMeter::color(qtractorMidiMeter::ColorOver);
			break;
		default:
			break;
		}
		if (ribbonColor.isValid()) {
			const QRect ribbonRect(
				rect.left() + 1, rect.top(), 4, rect.height() - 1);
			pPainter->fillRect(ribbonRect, ribbonColor);
		}
	}

	pPainter->restore();
}


void qtractorSessionListView::refresh ( bool bReset )
{
	QModelIndex index = currentIndex();
	qtractorClip *pClip = m_pItemModel->clipOfIndex(index);

	// Save expand state for every currently-expanded index before the
	// model reset, keyed by Node* so we can restore by the same pointer
	// after the reset (permanent group nodes and surviving pool nodes
	// keep their Node* across refresh()).
	QSet<void *> expandedNodes;
	if (!bReset) {
		// Walk all visible indexes and record those that are expanded.
		const int iGroupCount = m_pItemModel->rowCount();
		for (int iGroup = 0; iGroup < iGroupCount; ++iGroup) {
			const QModelIndex& groupIdx = m_pItemModel->index(iGroup, 0);
			if (QTreeView::isExpanded(groupIdx))
				expandedNodes.insert(groupIdx.internalPointer());
			const int iChildCount = m_pItemModel->rowCount(groupIdx);
			for (int iChild = 0; iChild < iChildCount; ++iChild) {
				const QModelIndex& childIdx
					= m_pItemModel->index(iChild, 0, groupIdx);
				if (QTreeView::isExpanded(childIdx))
					expandedNodes.insert(childIdx.internalPointer());
			}
		}
	}

	m_pItemModel->refresh();

	if (bReset) {
		QTreeView::expandToDepth(0);
	//	QTreeView::resizeColumnToContents(0);
	}
	else
	if (!expandedNodes.isEmpty()) {
		// Restore expand state: re-expand any node whose Node* was
		// saved above and is still valid after the refresh.
		const int iGroupCount = m_pItemModel->rowCount();
		for (int iGroup = 0; iGroup < iGroupCount; ++iGroup) {
			const QModelIndex& groupIdx = m_pItemModel->index(iGroup, 0);
			if (expandedNodes.contains(groupIdx.internalPointer()))
				QTreeView::setExpanded(groupIdx, true);
			const int iChildCount = m_pItemModel->rowCount(groupIdx);
			for (int iChild = 0; iChild < iChildCount; ++iChild) {
				const QModelIndex& childIdx
					= m_pItemModel->index(iChild, 0, groupIdx);
				if (expandedNodes.contains(childIdx.internalPointer()))
					QTreeView::setExpanded(childIdx, true);
			}
		}
	}

	if (pClip) {
		index = m_pItemModel->indexOfClip(pClip);
		if (index.isValid())
			setCurrentIndex(index);
	}
}


void qtractorSessionListView::clear (void)
{
	m_pItemModel->clear();
}


// Drag-n-drop stuff.
//
void qtractorSessionListView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	dragLeaveEvent(nullptr);

	if (pMouseEvent->button() == Qt::LeftButton) {
		m_posDrag = pMouseEvent->pos();
		const QModelIndex& index
			= QTreeView::indexAt(m_posDrag);
		m_dragItem = index.sibling(index.row(), 0);
	}

	QTreeView::mousePressEvent(pMouseEvent);
}


void qtractorSessionListView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	QTreeView::mouseMoveEvent(pMouseEvent);

	if (m_dragItem.isValid()
		&& (pMouseEvent->buttons() & Qt::LeftButton)
		&& ((pMouseEvent->pos() - m_posDrag).manhattanLength()
			>= QApplication::startDragDistance())) {
		// Start dragging if it's the right type...
		if (m_pItemModel->itemType(m_dragItem) == ItemModel::ItemTrack) {
			// We'll start dragging something alright...
			QMimeData *pMimeData = new QMimeData();
			pMimeData->setText(
				m_dragItem.data(Qt::DisplayRole).toString());
			QDrag *pDrag = new QDrag(this);
			pDrag->setMimeData(pMimeData);
			pDrag->setPixmap(
				m_dragItem.data(Qt::DecorationRole).value<QIcon>().pixmap(16));
			pDrag->setHotSpot(QPoint(-4, -4));
			pDrag->exec(Qt::MoveAction);
		}
		// We've dragged and maybe dropped it by now...
		dragLeaveEvent(nullptr);
		m_dragItem = QModelIndex();
	}
}


void qtractorSessionListView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QTreeView::mouseReleaseEvent(pMouseEvent);

	dragLeaveEvent(nullptr);
	m_dragItem = QModelIndex();
}


void qtractorSessionListView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
	// Always accept the drag-enter event,
	// so let we deal with it during move later...
	pDragEnterEvent->accept();
}


void qtractorSessionListView::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	if (!canDropEvent(pDragMoveEvent)) {
		pDragMoveEvent->ignore();
		return;
	}

	const QModelIndex& index = dragDropItem(
	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		pDragMoveEvent->position().toPoint());
	#else
		pDragMoveEvent->pos());
	#endif
	if (index.isValid()) {
		if (!pDragMoveEvent->isAccepted()) {
			pDragMoveEvent->setDropAction(Qt::MoveAction);
			pDragMoveEvent->accept();
		}
	} else {
		pDragMoveEvent->ignore();
	}
}


void qtractorSessionListView::dragLeaveEvent ( QDragLeaveEvent */*pDragLeaveEvent*/ )
{
	if (m_pRubberBand)
		delete m_pRubberBand;
	m_pRubberBand = nullptr;

	m_dropItem = QModelIndex();
}


void qtractorSessionListView::dropEvent ( QDropEvent *pDropEvent )
{
	const QPoint& pos
	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		= pDropEvent->position().toPoint();
	#else
		= pDropEvent->pos();
	#endif
	dropItem(dragDropItem(pos));

	dragLeaveEvent(nullptr);
	m_dragItem = QModelIndex();
}


bool qtractorSessionListView::canDropEvent ( QDropEvent *pDropEvent )
{
	if (!m_dragItem.isValid())
		return false;

	if (m_pItemModel->itemType(m_dragItem) != ItemModel::ItemTrack)
		return false;

	if (pDropEvent->source() != this)
		return false;

	return (pDropEvent->mimeData())->hasText();
}


QModelIndex qtractorSessionListView::dragDropItem ( const QPoint& pos )
{
	if (!m_dragItem.isValid())
		return QModelIndex();

	QModelIndex index = QTreeView::indexAt(pos);
	index = index.sibling(index.row(), 0);
	if (!index.isValid() || index == m_dragItem)
		return m_dropItem;

	const ItemModel::ItemType itemType
		= m_pItemModel->itemType(index);
	if (itemType != ItemModel::ItemClip  &&
		itemType != ItemModel::ItemTrack &&
		itemType != ItemModel::ItemTracks)
		index = QModelIndex();

	moveRubberBand(index);

	m_dropItem = index;

	return index;
}


void qtractorSessionListView::dropItem ( const QModelIndex& index )
{
	if (!m_dragItem.isValid())
		return;

	if (!index.isValid() || index == m_dragItem)
		return;

	qtractorTrack *pTrack = m_pItemModel->trackOfIndex(m_dragItem);
	if (pTrack == nullptr)
		return;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return;

	qtractorTrack *pTrackNext = m_pItemModel->trackOfIndex(index);
	if (pTrackNext)
		pTrackNext = pTrackNext->next();
	else
		pTrackNext = pSession->tracks().first();

	if (pTrack == pTrackNext)
		return;

	const bool bExpanded
		= QTreeView::isExpanded(m_dragItem);

	pSession->execute(new qtractorMoveTrackCommand(pTrack, pTrackNext));

	const QModelIndex& trackIdx = m_pItemModel->indexOfTrack(pTrack);
	if (trackIdx.isValid()) {
		if (bExpanded)
			QTreeView::setExpanded(trackIdx, true);
		QTreeView::setCurrentIndex(trackIdx);
	}
}


// Draw a dragging separator line.
void qtractorSessionListView::moveRubberBand ( const QModelIndex& index )
{
	if (!m_dragItem.isValid())
		return;

	// Is there any item upon we might drop anything?
	if (!index.isValid()) {
		if (m_pRubberBand)
			m_pRubberBand->hide();
		return;
	}

	// Find the drop point below this item...
	const ItemModel::ItemType itemType
		= m_pItemModel->itemType(index);
	QModelIndex belowItem = index;
	switch (itemType) {
	case ItemModel::ItemClip:
		belowItem = index.parent();
		// Fall thru...
	case ItemModel::ItemTrack:
		if (QTreeView::isExpanded(belowItem)) {
			const int iClipCount = m_pItemModel->rowCount(belowItem);
			if (iClipCount > 0)
				belowItem = m_pItemModel->index(iClipCount - 1, 0, belowItem);
		}
		// Fall thru...
	default:
		break;
	}

	ensureVisibleItem(belowItem);

	// Create the rubber-band if there's none...
	if (m_pRubberBand == nullptr)
		m_pRubberBand = new QRubberBand(QRubberBand::Line, QTreeView::viewport());

	// Just move it...
	QRect rect = QTreeView::visualRect(belowItem);
	switch (m_pItemModel->itemType(belowItem)) {
	case ItemModel::ItemClip:
		rect.setX(rect.x() - QTreeView::indentation());
		// Fall thru...
	case ItemModel::ItemTrack:
		rect.setX(rect.x() - QTreeView::indentation());
		// Fall-thu...
	default:
		break;
	}
	rect.setTop(rect.bottom() + 1);
	rect.setHeight(3);
	m_pRubberBand->setGeometry(rect);

	// Ah, and make it visible, of course...
	if (!m_pRubberBand->isVisible())
		m_pRubberBand->show();
}


// Ensure given item is brought to viewport visibility...
void qtractorSessionListView::ensureVisibleItem ( const QModelIndex& index )
{
	const QModelIndex& indexAbove
		= QTreeView::indexAbove(index);
	if (indexAbove.isValid())
		QTreeView::scrollTo(indexAbove);

	QTreeView::scrollTo(index);
}


//----------------------------------------------------------------------------
// qtractorSessionList -- Session overview dockable window.

qtractorSessionList::qtractorSessionList ( QWidget *pParent )
	: QDockWidget(pParent), m_pListView(nullptr),
		m_iSelectTrack(0), m_pBus(nullptr)
{
	QDockWidget::setObjectName("qtractorSessionList");
	QDockWidget::setWindowTitle(tr("Session"));
	QDockWidget::setWindowIcon(QIcon::fromTheme("document-open"));
	QDockWidget::setMinimumWidth(200);
	QDockWidget::setFeatures(
		QDockWidget::DockWidgetClosable   |
		QDockWidget::DockWidgetMovable    |
		QDockWidget::DockWidgetFloatable);

	m_pListView = new qtractorSessionListView(this);
	QDockWidget::setWidget(m_pListView);

	QObject::connect(m_pListView->selectionModel(),
		SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)),
		SLOT(currentRowChangedSlot(const QModelIndex&, const QModelIndex&)));
	QObject::connect(m_pListView,
		SIGNAL(doubleClicked(const QModelIndex&)),
		SLOT(doubleClickedSlot(const QModelIndex&)));
}


qtractorSessionList::~qtractorSessionList (void)
{
}


void qtractorSessionList::refresh ( bool bReset )
{
	m_pListView->refresh(bReset);
}


void qtractorSessionList::clear (void)
{
	m_pListView->clear();
}


void qtractorSessionList::showEvent ( QShowEvent *pShowEvent )
{
	m_pListView->refresh(false);

	QDockWidget::showEvent(pShowEvent);
}


void qtractorSessionList::closeEvent ( QCloseEvent *pCloseEvent )
{
	QDockWidget::closeEvent(pCloseEvent);

	m_pListView->clear();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Context menu request event handler.
void qtractorSessionList::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorSessionListView::ItemModel *pItemModel
		= static_cast<qtractorSessionListView::ItemModel *> (
			m_pListView->model());
	if (pItemModel == nullptr)
		return;

	const QModelIndex& index
		= m_pListView->currentIndex();
	if (!index.isValid())
		return;

	switch (pItemModel->itemType(index)) {
	case qtractorSessionListView::ItemModel::ItemInputs:
		m_pBus = nullptr;
		busMenu(pContextMenuEvent->globalPos());
		break;
	case qtractorSessionListView::ItemModel::ItemOutputs:
		m_pBus = nullptr;
		busMenu(pContextMenuEvent->globalPos());
		break;
	case qtractorSessionListView::ItemModel::ItemBus: {
		m_pBus = pItemModel->busOfIndex(index);
		busMenu(pContextMenuEvent->globalPos());
		break;
	}
	case qtractorSessionListView::ItemModel::ItemClip:
		pMainForm->clipMenu()->exec(pContextMenuEvent->globalPos());
		break;
	case qtractorSessionListView::ItemModel::ItemTracks:
	case qtractorSessionListView::ItemModel::ItemTrack:
	default:
		pMainForm->trackMenu()->exec(pContextMenuEvent->globalPos());
		break;
	}
}


// Buses context menu builder and executive.
void qtractorSessionList::busMenu ( const QPoint& pos )
{
	QMenu menu(this);

	if (m_pBus) {
		const qtractorTrack::TrackType busType
			= m_pBus->busType();
		const qtractorBus::BusMode busMode
			= m_pBus->busMode();
		if (busMode & qtractorBus::Input) {
			QIcon iconInputs;
			if (busType == qtractorTrack::Audio)
				iconInputs = QIcon::fromTheme("itemAudioPortIn");
			else
			if (busType == qtractorTrack::Midi)
				iconInputs = QIcon::fromTheme("itemMidiPortIn");
			menu.addAction(iconInputs, tr("&Inputs"),
				this, SLOT(busInputsSlot()));
		}
		if (busMode & qtractorBus::Output) {
			QIcon iconOutputs;
			if (busType == qtractorTrack::Audio)
				iconOutputs = QIcon::fromTheme("itemAudioPortOut");
			else
			if (busType == qtractorTrack::Midi)
				iconOutputs = QIcon::fromTheme("itemMidiPortOut");
			menu.addAction(iconOutputs, tr("&Outputs"),
				this, SLOT(busOutputsSlot()));
		}
		menu.addSeparator();
	}

	menu.addAction(tr("&Buses..."), this, SLOT(busPropertiesSlot()));

	menu.exec(pos);

	// We're done; reset interim variables.
	m_pBus = nullptr;
}


// Bus properties dialog summoner.
void qtractorSessionList::busProperties ( qtractorBus *pBus )
{
	qtractorBusForm busForm(this);
	busForm.setBus(pBus);
	busForm.exec();
}


// Bus-menu action slots.
void qtractorSessionList::busInputsSlot (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->connections()->showBus(m_pBus, qtractorBus::Input);
}


void qtractorSessionList::busOutputsSlot (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->connections()->showBus(m_pBus, qtractorBus::Output);
}


void qtractorSessionList::busPropertiesSlot (void)
{
	busProperties(m_pBus);
}


// Selection change slot.
void qtractorSessionList::currentRowChangedSlot (
	const QModelIndex& index, const QModelIndex& )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorSessionListView::ItemModel *pItemModel
		= static_cast<qtractorSessionListView::ItemModel *> (
			m_pListView->model());
	if (pItemModel == nullptr)
		return;

	qtractorClip *pClip = nullptr;
	qtractorTrack *pTrack = pItemModel->trackOfIndex(index);
	if (pTrack == nullptr) {
		pClip = pItemModel->clipOfIndex(index);
		if (pClip)
			pTrack = pClip->track();
	}

	if (pTrack) {
		++m_iSelectTrack;
		pMainForm->selectTrackOnTrackList(pTrack);
		--m_iSelectTrack;
	}

	if (pClip) {
		pMainForm->selectClipOnTrackView(pClip);
		pMainForm->selectClipFile(pClip);
	}
}


// Double-click slot.
void qtractorSessionList::doubleClickedSlot (
	const QModelIndex& index )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	qtractorSessionListView::ItemModel *pItemModel
		= static_cast<qtractorSessionListView::ItemModel *> (
			m_pListView->model());
	if (pItemModel == nullptr)
		return;

	switch (pItemModel->itemType(index)) {
	case qtractorSessionListView::ItemModel::ItemTrack:
		pTracks->editTrack(pItemModel->trackOfIndex(index));
		break;
	case qtractorSessionListView::ItemModel::ItemClip:
		pTracks->editClip(pItemModel->clipOfIndex(index));
		break;
	case qtractorSessionListView::ItemModel::ItemBus:
		busProperties(pItemModel->busOfIndex(index));
		break;
	default:
		break;
	}
}


// Track item selection.
void qtractorSessionList::selectTrack ( qtractorTrack *pTrack )
{
	if (m_iSelectTrack > 0)
		return;

	if (pTrack == nullptr)
		return;

	qtractorSessionListView::ItemModel *pItemModel
		= static_cast<qtractorSessionListView::ItemModel *> (
			m_pListView->model());
	if (pItemModel == nullptr)
		return;

	const QModelIndex& groupIdx
		= pItemModel->indexOfGroup(qtractorSessionListView::ItemModel::ItemTracks);
	if (!groupIdx.isValid())
		return;

	const QModelIndex& trackIdx = pItemModel->indexOfTrack(pTrack);
	if (!trackIdx.isValid())
		return;

	// Found — select without re-entering currentRowChangedSlot.
	const QSignalBlocker blocker(m_pListView->selectionModel());
	if (m_pListView->isExpanded(groupIdx))
		m_pListView->setExpanded(groupIdx, false);
	m_pListView->setExpanded(groupIdx, true);
	m_pListView->setCurrentIndex(trackIdx);
	m_pListView->scrollTo(trackIdx);
}


// Clip item selection.
void qtractorSessionList::selectClip ( qtractorClip *pClip )
{
	if (pClip == nullptr)
		return;

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == nullptr)
		return;

	qtractorSessionListView::ItemModel *pItemModel
		= static_cast<qtractorSessionListView::ItemModel *> (
			m_pListView->model());
	if (pItemModel == nullptr)
		return;

	const QModelIndex& trackIdx = pItemModel->indexOfTrack(pTrack);
	if (!trackIdx.isValid())
		return;

	const QModelIndex& clipIdx = pItemModel->indexOfClip(pClip);
	if (!clipIdx.isValid())
		return;

	// Found — select without re-entering currentRowChangedSlot.
	const QSignalBlocker blocker(m_pListView->selectionModel());
	if (m_pListView->isExpanded(trackIdx))
		m_pListView->setExpanded(trackIdx, false);
	m_pListView->setExpanded(trackIdx, true);
	m_pListView->setCurrentIndex(clipIdx);
	m_pListView->scrollTo(clipIdx);
}


// State saver.
QByteArray qtractorSessionList::saveState (void) const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
	QList<QByteArray> list;
#else
	QByteArrayList list;
#endif

	// 1. header-view state...
	list.append(m_pListView->header()->saveState());

	// 2. anything else...

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
	QByteArray state;
	QListIterator<QByteArray> iter(list);
	while (iter.hasNext()) {
		state.append(iter.next());
		state.append(':');
	}
	return state;
#else
	return list.join(':');
#endif
}


// State loader.
bool qtractorSessionList::restoreState ( const QByteArray& state )
{
	int i = 0;
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
	QListIterator<QByteArray> iter(state.split(':'));
#else
	QByteArrayListIterator iter(state.split(':'));
#endif
	while (iter.hasNext()) {
		const QByteArray& data = iter.next();
		switch (++i) {
		case 1: // header-view state...
			m_pListView->header()->restoreState(data);
			break;
		case 2: // anything else...
		default:
			break;
		}
	}

	return (i > 0);
}


// end of qtractorSessionList.cpp
