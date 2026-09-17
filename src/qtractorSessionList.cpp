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
#include "qtractorPlugin.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"

#include <QHeaderView>

#include <QFileInfo>

#include <QShowEvent>
#include <QCloseEvent>


//----------------------------------------------------------------------------
// qtractorSessionListModel::Node -- Internal tree node.

struct qtractorSessionListModel::Node
{
	// Constructor.
	Node(Node *pParent, int iType,
		const QString& sName, const QString& sDetail,
		void *pData = nullptr, const QIcon& icon0 = QIcon())
		: parent(pParent), type(iType),
		  name(sName), detail(sDetail),
		  data(pData), icon(icon0) {}

	// Destructor -- recursively deletes children.
	~Node() { qDeleteAll(children); }

	// Row index within parent's children list.
	int row() const
	{
		if (parent)
			return parent->children.indexOf(const_cast<Node *> (this));
		else
			return 0;
	}

	Node         *parent;
	int           type;
	QString       name;
	QString       detail;
	void         *data;       // raw pointer to session object (may be null)
	QIcon         icon;
	QList<Node *> children;
};


//----------------------------------------------------------------------------
// qtractorSessionListModel -- QAbstractItemModel implementation.

qtractorSessionListModel::qtractorSessionListModel ( QObject *pParent )
	: QAbstractItemModel(pParent), m_pRoot(nullptr)
{
}


qtractorSessionListModel::~qtractorSessionListModel (void)
{
	if (m_pRoot) delete m_pRoot;
}


// Clear the node tree.
void qtractorSessionListModel::clear (void)
{
	beginResetModel();

	if (m_pRoot) {
		delete m_pRoot;
		m_pRoot = nullptr;
	}

	endResetModel();
}


// Rebuild the node tree from the current session state.
void qtractorSessionListModel::refresh (void)
{
	beginResetModel();

	if (m_pRoot) {
		delete m_pRoot;
		m_pRoot = nullptr;
	}

	buildTree();

	endResetModel();
}


// Build helpers.
//
// Append a Plugins group node under pParent for pPluginList.
qtractorSessionListModel::Node *qtractorSessionListModel::buildPluginsNode (
	Node *pParent, qtractorPluginList *pPluginList )
{
	if (pPluginList == nullptr || pPluginList->count() < 1)
		return nullptr;

	const int nPlugins = pPluginList->count();
	Node *pGroup = new Node(pParent, ItemPluginsGroup,
		tr("Plugins"), QString("[%1]").arg(nPlugins));
	pParent->children.append(pGroup);

	for (qtractorPlugin *pPlugin = pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		const QString sTitle = pPlugin->title();
		QString sDetail;
		const qtractorPluginType *pType = pPlugin->type();
		if (pType) {
			const QString& sHint
				= qtractorPluginType::textFromHint(pType->typeHint());
			sDetail = sHint.isEmpty()
				? pType->name()
				: QString("%1 (%2)").arg(pType->name(), sHint);
		}
		Node *pNode = new Node(pGroup, ItemPlugin,
			sTitle, sDetail,
			static_cast<void *> (pPlugin),
			QIcon::fromTheme("pluginEdit"));
		pGroup->children.append(pNode);
	}

	return pGroup;
}


// Append a Track node (with Clips + Plugins children) under pParent.
qtractorSessionListModel::Node *qtractorSessionListModel::buildTrackNode (
	Node *pParent, qtractorTrack *pTrack )
{
	QString sDetail;
	QIcon trackIcon;
	QIcon clipIcon;
	const QString& sTrackIcon = pTrack->trackIcon();
	if (!sTrackIcon.isEmpty())
		trackIcon = QIcon::fromTheme(sTrackIcon);
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
		sDetail = tr("Audio");
		clipIcon = QIcon::fromTheme("trackAudio");
		if (trackIcon.isNull())
			trackIcon = clipIcon;
		break;
	case qtractorTrack::Midi:
		sDetail = tr("MIDI");
		clipIcon = QIcon::fromTheme("trackMidi");
		if (trackIcon.isNull())
			trackIcon = clipIcon;
		break;
	default:
		break;
	}
	sDetail += ' ';
	sDetail += tr("track");

	QString sFlags;
	if (pTrack->isRecord()) sFlags += 'R';
	if (pTrack->isMute())   sFlags += 'M';
	if (pTrack->isSolo())   sFlags += 'S';
	if (!sFlags.isEmpty())
		sDetail += QString(" [%1]").arg(sFlags);

	Node *pTrackNode = new Node(pParent, ItemTrack,
		pTrack->trackName(), sDetail,
		static_cast<void *> (pTrack), trackIcon);
	pParent->children.append(pTrackNode);

	// Clips child group.
	const int nClips = pTrack->clips().count();
	if (nClips > 0) {
		Node *pClipsGroup = new Node(pTrackNode, ItemClipsGroup,
			tr("Clips"), QString("[%1]").arg(nClips));
		pTrackNode->children.append(pClipsGroup);
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			const QString sName = pClip->clipName();
			Node *pClipNode = new Node(pClipsGroup, ItemClip,
				sName.isEmpty() ? tr("(unnamed)") : sName,
				QFileInfo(pClip->filename()).fileName(),
				static_cast<void *> (pClip), clipIcon);
			pClipsGroup->children.append(pClipNode);
		}
	}

	// Plugins child group.
	buildPluginsNode(pTrackNode, pTrack->pluginList());

	return pTrackNode;
}


// Append a Bus node under pParent, attaching plugins for the given direction.
// busMode: pass qtractorBus::Input or ::Output to show only that side's plugins.
qtractorSessionListModel::Node *qtractorSessionListModel::buildBusNode (
	Node *pParent, qtractorBus *pBus, int busMode )
{
	QString sDetail;
	if (busMode == (int)qtractorBus::Input
		&& (pBus->busMode() & qtractorBus::Input))
		sDetail = tr("Input");
	else
	if (busMode == (int)qtractorBus::Output
		&& (pBus->busMode() & qtractorBus::Output))
		sDetail = tr("Output");
	else
		sDetail = tr("Duplex");
	sDetail += ' ';
	sDetail += tr("bus");

	const QIcon busIcon = QIcon::fromTheme(
		pBus->busType() == qtractorTrack::Audio
			? "trackAudio" : "trackMidi");

	Node *pBusNode = new Node(pParent, ItemBus,
		pBus->busName(), sDetail,
		static_cast<void *> (pBus), busIcon);
	pParent->children.append(pBusNode);

	if (busMode == (int)qtractorBus::Input
		&& (pBus->busMode() & qtractorBus::Input))
		buildPluginsNode(pBusNode, pBus->pluginList_in());
	else
	if (busMode == (int)qtractorBus::Output
		&& (pBus->busMode() & qtractorBus::Output))
		buildPluginsNode(pBusNode, pBus->pluginList_out());

	return pBusNode;
}


// Build the complete node tree.
void qtractorSessionListModel::buildTree (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();

	// m_pRoot is the invisible sentinel; top-level groups are its children.
	m_pRoot = new Node(nullptr, 0, QString(), QString());

	if (pSession == nullptr)
		return;

	// Collect all buses from both engines, split by direction.
	// A duplex bus appears in both Input and Output groups.
	QList<qtractorBus *> inputBuses, outputBuses;
	auto collectBuses = [&](qtractorEngine *pEngine) {
		if (!pEngine) return;
		for (qtractorBus *pBus = pEngine->buses().first();
				pBus; pBus = pBus->next()) {
			const qtractorBus::BusMode m = pBus->busMode();
			if (m & qtractorBus::Input)  inputBuses.append(pBus);
			if (m & qtractorBus::Output) outputBuses.append(pBus);
		}
	};
	collectBuses(pSession->audioEngine());
	collectBuses(pSession->midiEngine());

	// 1. Inputs.
	if (!inputBuses.isEmpty()) {
		Node *pInputBusesGroup = new Node(m_pRoot, ItemInputBusesGroup,
			tr("Inputs"), QString("[%1]").arg(inputBuses.count()));
		m_pRoot->children.append(pInputBusesGroup);
		for (qtractorBus *pBus : inputBuses)
			buildBusNode(pInputBusesGroup, pBus, qtractorBus::Input);
	}

	// 2. Tracks.
	const int nTracks = pSession->tracks().count();
	Node *pTracksGroup = new Node(m_pRoot, ItemTracksGroup,
		tr("Tracks"), QString("[%1]").arg(nTracks));
	m_pRoot->children.append(pTracksGroup);

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next())
		buildTrackNode(pTracksGroup, pTrack);

	// 3. Outputs.
	if (!outputBuses.isEmpty()) {
		Node *pOutputBusesGroup = new Node(m_pRoot, ItemOutputBusesGroup,
			tr("Outputs"), QString("[%1]").arg(outputBuses.count()));
		m_pRoot->children.append(pOutputBusesGroup);
		for (qtractorBus *pBus : outputBuses)
			buildBusNode(pOutputBusesGroup, pBus, qtractorBus::Output);
	}
}


// QAbstractItemModel virtuals.
//
QModelIndex qtractorSessionListModel::index (
	int row, int column, const QModelIndex& parent ) const
{
	if (m_pRoot == nullptr)
		return QModelIndex();

	Node *pParentNode = parent.isValid()
		? static_cast<Node *> (parent.internalPointer())
		: m_pRoot;

	if (row < 0 || row >= pParentNode->children.count())
		return QModelIndex();

	return createIndex(row, column, pParentNode->children.at(row));
}


QModelIndex qtractorSessionListModel::parent ( const QModelIndex& child ) const
{
	if (!child.isValid() || m_pRoot == nullptr)
		return QModelIndex();

	Node *pNode = static_cast<Node *> (child.internalPointer());
	Node *pParent = pNode->parent;

	// The root's children report an invalid parent (root is not an index).
	if (pParent == nullptr || pParent == m_pRoot)
		return QModelIndex();

	return createIndex(pParent->row(), 0, pParent);
}


int qtractorSessionListModel::rowCount ( const QModelIndex& parent ) const
{
	if (m_pRoot == nullptr)
		return 0;

	// Invalid parent means the top level — return the sentinel's child count.
	if (!parent.isValid())
		return m_pRoot->children.count();

	Node *pNode = static_cast<Node *> (parent.internalPointer());
	return pNode->children.count();
}


int qtractorSessionListModel::columnCount ( const QModelIndex& /*parent*/ ) const
{
	return 2; // Name | Detail
}


QVariant qtractorSessionListModel::data (
	const QModelIndex& index, int role ) const
{
	if (!index.isValid() || m_pRoot == nullptr)
		return QVariant();

	Node *pNode = static_cast<Node *> (index.internalPointer());

	switch (role) {
	case Qt::DisplayRole:
		return (index.column() == 0) ? pNode->name : pNode->detail;

	case Qt::ToolTipRole:
		return (index.column() == 0) ? pNode->name : pNode->detail;

	case Qt::UserRole:
		// column 0: item type tag; column 1: raw data pointer.
		if (index.column() == 0)
			return pNode->type;
		else
			return QVariant::fromValue(pNode->data);

	case Qt::DecorationRole:
		if (index.column() == 0 && !pNode->icon.isNull())
			return pNode->icon;
		break;

	case Qt::FontRole:
		if (index.column() == 0 && pNode->parent == m_pRoot) {
			QFont font;
			font.setBold(true);
			return font;
		}
		break;

	default:
		break;
	}

	return QVariant();
}


QVariant qtractorSessionListModel::headerData (
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


Qt::ItemFlags qtractorSessionListModel::flags ( const QModelIndex& index ) const
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


//----------------------------------------------------------------------------
// qtractorSessionListView -- QTreeView wired to qtractorSessionListModel.

qtractorSessionListView::qtractorSessionListView ( QWidget *pParent )
	: QTreeView(pParent), m_pModel(nullptr)
{
	m_pModel = new qtractorSessionListModel(this);
	QTreeView::setModel(m_pModel);

	QTreeView::setUniformRowHeights(true);
	QTreeView::setAllColumnsShowFocus(true);
	QTreeView::setRootIsDecorated(true);
	QTreeView::setSelectionMode(QAbstractItemView::SingleSelection);
	QTreeView::setEditTriggers(QAbstractItemView::NoEditTriggers);
	QTreeView::setAlternatingRowColors(true);

	QHeaderView *pHeaderView = QTreeView::header();
	pHeaderView->setStretchLastSection(true);
//	pHeaderView->hide();
}


qtractorSessionListView::~qtractorSessionListView (void)
{
}


void qtractorSessionListView::refresh (void)
{
	m_pModel->refresh();

	QTreeView::expandToDepth(0);
//	QTreeView::resizeColumnToContents(0);
}


void qtractorSessionListView::clear (void)
{
	m_pModel->clear();
}


//----------------------------------------------------------------------------
// qtractorSessionList -- Session overview dockable window.

qtractorSessionList::qtractorSessionList ( QWidget *pParent )
	: QDockWidget(pParent)
{
	QDockWidget::setObjectName("qtractorSessionList");
	QDockWidget::setWindowTitle(tr("Session"));
	QDockWidget::setWindowIcon(QIcon::fromTheme("document-open"));
	QDockWidget::setMinimumWidth(200);
	QDockWidget::setFeatures(
		QDockWidget::DockWidgetClosable   |
		QDockWidget::DockWidgetMovable    |
		QDockWidget::DockWidgetFloatable);

	m_pTreeView = new qtractorSessionListView(this);
	QDockWidget::setWidget(m_pTreeView);

	QObject::connect(m_pTreeView->selectionModel(),
		SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)),
		SLOT(currentRowChangedSlot(const QModelIndex&, const QModelIndex&)));
}


qtractorSessionList::~qtractorSessionList (void)
{
}


void qtractorSessionList::refresh (void)
{
	m_pTreeView->refresh();
}


void qtractorSessionList::clear (void)
{
	m_pTreeView->clear();
}


void qtractorSessionList::refreshSlot (void)
{
	if (QDockWidget::isVisible())
		m_pTreeView->refresh();
}


void qtractorSessionList::showEvent ( QShowEvent *pShowEvent )
{
	m_pTreeView->refresh();
	QDockWidget::showEvent(pShowEvent);
}


void qtractorSessionList::closeEvent ( QCloseEvent *pCloseEvent )
{
	QDockWidget::closeEvent(pCloseEvent);

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


void qtractorSessionList::currentRowChangedSlot (
	const QModelIndex&, const QModelIndex& )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	QAbstractItemModel *pModel = m_pTreeView->model();
	if (pModel == nullptr)
		return;

	const QModelIndex& index = m_pTreeView->currentIndex();
	if (!index.isValid())
		return;

	qtractorTrack *pTrack = nullptr;
	qtractorClip *pClip = nullptr;

	// Type is stored in UserRole, column 0.
	// Raw pointer is stored in Qt::UserRole on column 1.
	const QModelIndex& col0 = index.sibling(index.row(), 0);
	const QModelIndex& col1 = index.sibling(index.row(), 1);
	const int iType = pModel->data(col0, Qt::UserRole).toInt();

	if (iType == qtractorSessionListModel::ItemTrack) {
		pTrack = static_cast<qtractorTrack *> (
			pModel->data(col1, Qt::UserRole).value<void *> ());
	}
	else
	if (iType == qtractorSessionListModel::ItemClip) {
		pClip = static_cast<qtractorClip *> (
			pModel->data(col1, Qt::UserRole).value<void *> ());
		if (pClip)
			pTrack = pClip->track();
	}

	if (pTrack)
		pMainForm->selectTrackOnTrackList(pTrack);
	if (pClip) {
		pMainForm->selectClipOnTrackView(pClip);
		pMainForm->selectClipFile(pClip);
	}
}


// Track item selection.
void qtractorSessionList::selectTrack ( qtractorTrack *pTrack )
{
	if (pTrack == nullptr)
		return;

	QAbstractItemModel *pModel = m_pTreeView->model();
	if (pModel == nullptr)
		return;

	// Iterate top-level groups to find the ItemTracksGroup.
	const int nGroups = pModel->rowCount();
	for (int iGroup = 0; iGroup < nGroups; ++iGroup) {
		const QModelIndex groupIdx = pModel->index(iGroup, 0);
		if (pModel->data(groupIdx, Qt::UserRole).toInt()
				!= qtractorSessionListModel::ItemTracksGroup)
			continue;
		// Walk the track children.
		const int nTracks = pModel->rowCount(groupIdx);
		for (int iTrack = 0; iTrack < nTracks; ++iTrack) {
			const QModelIndex trackIdx0 = pModel->index(iTrack, 0, groupIdx);
			if (pModel->data(trackIdx0, Qt::UserRole).toInt()
					!= qtractorSessionListModel::ItemTrack)
				continue;
			const QModelIndex trackIdx1 = pModel->index(iTrack, 1, groupIdx);
			void *pData = pModel->data(trackIdx1,
				Qt::UserRole).value<void *> ();
			if (pData != static_cast<void *> (pTrack))
				continue;
			// Found — select without re-entering currentRowChangedSlot.
			const QSignalBlocker blocker(m_pTreeView->selectionModel());
			if (m_pTreeView->isExpanded(groupIdx))
				m_pTreeView->setExpanded(groupIdx, false);
			m_pTreeView->setExpanded(groupIdx, true);
			m_pTreeView->setCurrentIndex(trackIdx0);
			m_pTreeView->scrollTo(trackIdx0);
			return;
		}
		break; // There is only one ItemTracksGroup.
	}
}


// Clip item selection.
void qtractorSessionList::selectClip ( qtractorClip *pClip )
{
	if (pClip == nullptr)
		return;

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == nullptr)
		return;

	QAbstractItemModel *pModel = m_pTreeView->model();
	if (pModel == nullptr)
		return;

	// Iterate top-level groups to find the ItemTracksGroup.
	const int nGroups = pModel->rowCount();
	for (int iGroup = 0; iGroup < nGroups; ++iGroup) {
		const QModelIndex groupIdx = pModel->index(iGroup, 0);
		if (pModel->data(groupIdx, Qt::UserRole).toInt()
				!= qtractorSessionListModel::ItemTracksGroup)
			continue;
		// Walk the track children to find the matching ItemTrack.
		const int nTracks = pModel->rowCount(groupIdx);
		for (int iTrack = 0; iTrack < nTracks; ++iTrack) {
			const QModelIndex trackIdx0 = pModel->index(iTrack, 0, groupIdx);
			if (pModel->data(trackIdx0, Qt::UserRole).toInt()
					!= qtractorSessionListModel::ItemTrack)
				continue;
			const QModelIndex trackIdx1 = pModel->index(iTrack, 1, groupIdx);
			void *pTrackData = pModel->data(trackIdx1,
				Qt::UserRole).value<void *> ();
			if (pTrackData != static_cast<void *> (pTrack))
				continue;
			// Found the track — now search its children for ItemClipsGroup.
			const int nTrackChildren = pModel->rowCount(trackIdx0);
			for (int iChild = 0; iChild < nTrackChildren; ++iChild) {
				const QModelIndex clipsGroupIdx
					= pModel->index(iChild, 0, trackIdx0);
				if (pModel->data(clipsGroupIdx, Qt::UserRole).toInt()
						!= qtractorSessionListModel::ItemClipsGroup)
					continue;
				// Walk the clip children.
				const int nClips = pModel->rowCount(clipsGroupIdx);
				for (int iClip = 0; iClip < nClips; ++iClip) {
					const QModelIndex clipIdx0
						= pModel->index(iClip, 0, clipsGroupIdx);
					if (pModel->data(clipIdx0, Qt::UserRole).toInt()
							!= qtractorSessionListModel::ItemClip)
						continue;
					const QModelIndex clipIdx1
						= pModel->index(iClip, 1, clipsGroupIdx);
					void *pClipData = pModel->data(clipIdx1,
						Qt::UserRole).value<void *> ();
					if (pClipData != static_cast<void *> (pClip))
						continue;
					// Found — select without re-entering currentRowChangedSlot.
					const QSignalBlocker blocker(m_pTreeView->selectionModel());
					if (m_pTreeView->isExpanded(trackIdx0))
						m_pTreeView->setExpanded(trackIdx0, false);
					m_pTreeView->setExpanded(trackIdx0, true);
					if (m_pTreeView->isExpanded(clipsGroupIdx))
						m_pTreeView->setExpanded(clipsGroupIdx, false);
					m_pTreeView->setExpanded(clipsGroupIdx, true);
					m_pTreeView->setCurrentIndex(clipIdx0);
					m_pTreeView->scrollTo(clipIdx0);
					return;
				}
				break; // Only one ItemClipsGroup per track.
			}
			break; // Track found; clip not present in model.
		}
		break; // There is only one ItemTracksGroup.
	}
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
	list.append(m_pTreeView->header()->saveState());

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
			m_pTreeView->header()->restoreState(data);
			break;
		case 2: // anything else...
		default:
			break;
		}
	}

	return (i > 0);
}


// end of qtractorSessionList.cpp
