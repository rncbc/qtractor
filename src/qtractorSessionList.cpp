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

#include <QApplication>
#include <QFileInfo>
#include <QHeaderView>
#include <QShowEvent>
#include <QCloseEvent>


//----------------------------------------------------------------------------
// qtractorSessionListModel::Node -- Internal tree node.

struct qtractorSessionListModel::Node
{
	// Constructor.
	Node(Node *pParent, int iType,
		const QString& sName, const QString& sDetail,
		void *pData = nullptr, bool bEnabled = true,
		const QIcon& iicon = QIcon())
		: parent(pParent), type(iType),
		  name(sName), detail(sDetail),
		  data(pData), enabled(bEnabled), icon(iicon) {}

	// Destructor -- recursively deletes children.
	~Node()
		{ qDeleteAll(children); }

	// Row index within parent's children list.
	int row() const
	{
		if (parent)
			return parent->children.indexOf(
				const_cast<Node *>(this));
		return 0;
	}

	Node         *parent;
	int           type;
	QString       name;
	QString       detail;
	void         *data;       // raw pointer to session object (may be null)
	bool          enabled;
	QIcon         icon;
	QList<Node *> children;
};


//----------------------------------------------------------------------------
// qtractorSessionListModel -- helpers.

// Build a display string for a plugin type hint.
static QString pluginTypeHintText ( qtractorPluginType::Hint hint )
{
	switch (hint) {
	case qtractorPluginType::Ladspa:  return QObject::tr("LADSPA");
	case qtractorPluginType::Dssi:    return QObject::tr("DSSI");
	case qtractorPluginType::Vst2:    return QObject::tr("VST2");
	case qtractorPluginType::Vst3:    return QObject::tr("VST3");
	case qtractorPluginType::Clap:    return QObject::tr("CLAP");
	case qtractorPluginType::Lv2:     return QObject::tr("LV2");
	case qtractorPluginType::Insert:  return QObject::tr("Insert");
	case qtractorPluginType::AuxSend: return QObject::tr("Aux Send");
	case qtractorPluginType::Control: return QObject::tr("Control");
	default:                          return QString();
	}
}


//----------------------------------------------------------------------------
// qtractorSessionListModel -- QAbstractItemModel implementation.

qtractorSessionListModel::qtractorSessionListModel ( QObject *pParent )
	: QAbstractItemModel(pParent), m_pRoot(nullptr)
{
}


qtractorSessionListModel::~qtractorSessionListModel ()
{
	delete m_pRoot;
}


// Clear the node tree.
void qtractorSessionListModel::clear ()
{
	beginResetModel();
	delete m_pRoot;
	m_pRoot = nullptr;
	endResetModel();
}


// Rebuild the node tree from the current session state.
void qtractorSessionListModel::refresh ()
{
	beginResetModel();

	delete m_pRoot;
	m_pRoot = nullptr;

	buildTree();

	endResetModel();
}


// Build helpers.

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
			const QString sHint = pluginTypeHintText(pType->typeHint());
			sDetail = sHint.isEmpty()
				? pType->name()
				: QString("%1 (%2)").arg(pType->name(), sHint);
		}
		Node *pNode = new Node(pGroup, ItemPlugin,
			sTitle.isEmpty() ? tr("(noname)") : sTitle,
			sDetail,
			static_cast<void *>(pPlugin),
			pPlugin->isActivated());
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
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
		sDetail = tr("Audio");
		trackIcon = QIcon::fromTheme("trackAudio");
		break;
	case qtractorTrack::Midi:
		sDetail = tr("MIDI");
		trackIcon = QIcon::fromTheme("trackMidi");
		break;
	default:
		sDetail = tr("None");
		break;
	}
	QString sFlags;
	if (pTrack->isMute())   sFlags += 'M';
	if (pTrack->isSolo())   sFlags += 'S';
	if (pTrack->isRecord()) sFlags += 'R';
	if (!sFlags.isEmpty())
		sDetail += QString(" [%1]").arg(sFlags);

	Node *pTrackNode = new Node(pParent, ItemTrack,
		pTrack->trackName(), sDetail,
		static_cast<void *>(pTrack),
		!pTrack->isMute(), trackIcon);
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
				static_cast<void *>(pClip),
				!pClip->isClipMute());
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
	// If no explicit direction given, fall back to the bus's own mode.
	const int effectiveMode = busMode ? busMode : (int)pBus->busMode();

	QString sModeStr;
	if (effectiveMode == (int)qtractorBus::Input)
		sModeStr = tr("Input");
	else if (effectiveMode == (int)qtractorBus::Output)
		sModeStr = tr("Output");
	else
		sModeStr = tr("Duplex");

	const QIcon busIcon = QIcon::fromTheme(
		pBus->busType() == qtractorTrack::Audio
			? "trackAudio" : "trackMidi");

	Node *pBusNode = new Node(pParent, ItemBus,
		pBus->busName(), sModeStr,
		static_cast<void *>(pBus), true, busIcon);
	pParent->children.append(pBusNode);

	if (effectiveMode & (int)qtractorBus::Input)
		buildPluginsNode(pBusNode, pBus->pluginList_in());
	if (effectiveMode & (int)qtractorBus::Output)
		buildPluginsNode(pBusNode, pBus->pluginList_out());

	return pBusNode;
}


// Build the complete node tree.
void qtractorSessionListModel::buildTree ()
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
		Node *pInputGroup = new Node(m_pRoot, ItemInputBusesGroup,
			tr("Inputs"), QString("[%1]").arg(inputBuses.count()));
		m_pRoot->children.append(pInputGroup);
		for (qtractorBus *pBus : inputBuses)
			buildBusNode(pInputGroup, pBus, qtractorBus::Input);
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
		Node *pOutputGroup = new Node(m_pRoot, ItemOutputBusesGroup,
			tr("Outputs"), QString("[%1]").arg(outputBuses.count()));
		m_pRoot->children.append(pOutputGroup);
		for (qtractorBus *pBus : outputBuses)
			buildBusNode(pOutputGroup, pBus, qtractorBus::Output);
	}
}


// QAbstractItemModel virtuals.

QModelIndex qtractorSessionListModel::index (
	int row, int column, const QModelIndex& parent ) const
{
	if (m_pRoot == nullptr)
		return QModelIndex();

	Node *pParentNode = parent.isValid()
		? static_cast<Node *>(parent.internalPointer())
		: m_pRoot;

	if (row < 0 || row >= pParentNode->children.count())
		return QModelIndex();

	return createIndex(row, column, pParentNode->children.at(row));
}


QModelIndex qtractorSessionListModel::parent ( const QModelIndex& child ) const
{
	if (!child.isValid() || m_pRoot == nullptr)
		return QModelIndex();

	Node *pNode = static_cast<Node *>(child.internalPointer());
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

	Node *pNode = static_cast<Node *>(parent.internalPointer());
	return pNode->children.count();
}


int qtractorSessionListModel::columnCount ( const QModelIndex& /*parent*/ ) const
{
	return 2;   // Name | Detail
}


QVariant qtractorSessionListModel::data (
	const QModelIndex& index, int role ) const
{
	if (!index.isValid() || m_pRoot == nullptr)
		return QVariant();

	Node *pNode = static_cast<Node *>(index.internalPointer());

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
		if (pNode->parent == m_pRoot) {
			QFont font;
			font.setBold(true);
			return font;
		}
		break;

	case Qt::ForegroundRole:
		if (!pNode->enabled)
			return QApplication::palette().color(QPalette::Disabled, QPalette::Text);
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
	if (!index.isValid())
		return Qt::NoItemFlags;

	return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}


//----------------------------------------------------------------------------
// qtractorSessionListView -- QTreeView wired to qtractorSessionListModel.

qtractorSessionListView::qtractorSessionListView ( QWidget *pParent )
	: QTreeView(pParent), m_pModel(nullptr)
{
	m_pModel = new qtractorSessionListModel(this);
	QTreeView::setModel(m_pModel);

	QTreeView::header()->setStretchLastSection(true);
	QTreeView::setUniformRowHeights(true);
	QTreeView::setAllColumnsShowFocus(true);
	QTreeView::setRootIsDecorated(true);
	QTreeView::setSelectionMode(QAbstractItemView::SingleSelection);
	QTreeView::setEditTriggers(QAbstractItemView::NoEditTriggers);
	QTreeView::setAlternatingRowColors(true);

	QObject::connect(this, SIGNAL(activated(const QModelIndex&)),
		this, SLOT(activatedSlot(const QModelIndex&)));
}


qtractorSessionListView::~qtractorSessionListView ()
{
}


void qtractorSessionListView::refresh ()
{
	m_pModel->refresh();

	QTreeView::expandAll();
	QTreeView::resizeColumnToContents(0);
}


void qtractorSessionListView::clear ()
{
	m_pModel->clear();
}


// Double-click / Enter on a track item: make it current in the session.
void qtractorSessionListView::activatedSlot ( const QModelIndex& index )
{
	if (!index.isValid())
		return;

	// Type is stored in UserRole, column 0.
	const QModelIndex col0 = index.sibling(index.row(), 0);
	const int iType = m_pModel->data(col0, Qt::UserRole).toInt();

	if (iType == qtractorSessionListModel::ItemTrack) {
		// Raw pointer is stored in Qt::UserRole on column 1.
		const QModelIndex col1 = index.sibling(index.row(), 1);
		qtractorTrack *pTrack = static_cast<qtractorTrack *>(
			m_pModel->data(col1, Qt::UserRole).value<void *>());
		if (pTrack) {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession)
				pSession->setCurrentTrack(pTrack);
			qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
			if (pMainForm)
				pMainForm->stabilizeForm();
		}
	}
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
}


qtractorSessionList::~qtractorSessionList ()
{
}


void qtractorSessionList::refresh ()
{
	m_pTreeView->refresh();
}


void qtractorSessionList::clear ()
{
	m_pTreeView->clear();
}


void qtractorSessionList::refreshSlot ()
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


// end of qtractorSessionList.cpp
