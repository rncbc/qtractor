// qtractorMidiEventList.cpp
//
/****************************************************************************
   Copyright (C) 2005-2023, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEventList.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditorForm.h"
#include "qtractorMidiEditCommand.h"
#include "qtractorMidiEditView.h"
#include "qtractorMidiSequence.h"

#include "qtractorSpinBox.h"

#include <QAbstractItemModel>
#include <QItemDelegate>

#include <QHeaderView>
#include <QComboBox>
#include <QSpinBox>

#include <QContextMenuEvent>


//----------------------------------------------------------------------------
// qtractorMidiEventListView::ItemModel -- List model.

class qtractorMidiEventListView::ItemModel : public QAbstractItemModel
{
public:

	// Constructor.
	ItemModel(qtractorMidiEditor *pEditor, QObject *pParent = nullptr);

	// Concretizers (virtual).
	int rowCount(const QModelIndex& parent = QModelIndex()) const;
	int columnCount(const QModelIndex& parent = QModelIndex()) const;

	QVariant headerData(int section, Qt::Orientation orient, int role) const;
	QVariant data(const QModelIndex& index, int role) const;

	Qt::ItemFlags flags(const QModelIndex& index ) const;

	QModelIndex index(int row, int column,
		const QModelIndex& parent = QModelIndex()) const;

	QModelIndex parent(const QModelIndex&) const;

	void reset();

	// Specifics.
	qtractorMidiEvent *eventOfIndex(const QModelIndex& index) const;
	QModelIndex indexOfEvent(qtractorMidiEvent *pEvent) const;

	QModelIndex indexFromTick(unsigned long iTick) const;
	unsigned long tickFromIndex(const QModelIndex& index) const;

	QModelIndex indexFromFrame(unsigned long iFrame) const;
	unsigned long frameFromIndex(const QModelIndex& index) const;

	qtractorMidiEditor *editor() const;

protected:

	qtractorMidiEvent *eventAt(int i) const;

	QString itemDisplay(const QModelIndex& index) const;
	QString itemToolTip(const QModelIndex& index) const;

	int columnAlignment(int column) const;

private:

	// Model variables.
	QStringList m_headers;

	qtractorMidiEditor   *m_pEditor;
	qtractorMidiSequence *m_pSeq;

	unsigned long m_iTimeOffset;

	mutable qtractorMidiEvent *m_pEvent;
	mutable int m_iEvent;
};


//----------------------------------------------------------------------------
// qtractorMidiEventListView::ItemDelegate -- Custom (tree) list item delegate.

class qtractorMidiEventListView::ItemDelegate : public QItemDelegate
{
public:

	// Constructor.
	ItemDelegate(QObject *pParent = nullptr);

	// QItemDelegate Interface...

	void paint(QPainter *pPainter,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;

	QSize sizeHint(
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;

	QWidget *createEditor(QWidget *pParent,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;

	void setEditorData(QWidget *pEditor,
		const QModelIndex& index) const;

	void setModelData(QWidget *pEditor,
		QAbstractItemModel *pModel,
		const QModelIndex& index) const;
};


//----------------------------------------------------------------------------
// qtractorMidiEventListView::ItemModel -- List model.

// Constructor.
qtractorMidiEventListView::ItemModel::ItemModel (
	qtractorMidiEditor *pEditor, QObject *pParent )
	: QAbstractItemModel(pParent), m_pEditor(pEditor),
		m_pSeq(nullptr), m_iTimeOffset(0),
		m_pEvent(nullptr), m_iEvent(0)
{
	m_headers
		<< tr("Time")
		<< tr("Type")
		<< tr("Name")
		<< tr("Value")
		<< tr("Duration/Data");

	reset();
}


int qtractorMidiEventListView::ItemModel::rowCount (
	const QModelIndex& /*parent*/ ) const
{
	return (m_pSeq ? m_pSeq->events().count() : 0);
}


int qtractorMidiEventListView::ItemModel::columnCount (
	const QModelIndex& /*parent*/ ) const
{
	return m_headers.count();
}


QVariant qtractorMidiEventListView::ItemModel::headerData (
	int section, Qt::Orientation orient, int role ) const
{
//	qDebug("headerData(%d, %d, %d)", section, int(orient), int(role));
	if (orient == Qt::Horizontal) {
		switch (role) {
		case Qt::DisplayRole:
		#if 0
			if (section == 0) {
				switch ((m_pEditor->timeScale())->displayFormat()) {
				case qtractorTimeScale::Frames:
					return tr("Frame");
				case qtractorTimeScale::Time:
					return tr("Time");
				case qtractorTimeScale::BBT:
					return tr("BBT");
				}
			}
		#endif
			return m_headers.at(section);
		case Qt::TextAlignmentRole:
			return columnAlignment(section);
		default:
			break;
		}
	}
	return QVariant();
}


QVariant qtractorMidiEventListView::ItemModel::data (
	const QModelIndex& index, int role ) const
{
//	qDebug("data(%d, %d, %d)", index.row(), index.column(), int(role));
	switch (role) {
	case Qt::DisplayRole:
		return itemDisplay(index);
	case Qt::TextAlignmentRole:
		return columnAlignment(index.column());
	case Qt::ToolTipRole:
		return itemToolTip(index);
	default:
		return QVariant();
	}
}


Qt::ItemFlags qtractorMidiEventListView::ItemModel::flags (
	const QModelIndex& index ) const
{
	const Qt::ItemFlags flags = QAbstractItemModel::flags(index);
	return flags | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}


QModelIndex qtractorMidiEventListView::ItemModel::index (
	int row, int column, const QModelIndex& /*parent*/) const
{
//	qDebug("index(%d, %d)", row, column);
	qtractorMidiEvent *pEvent = eventAt(row);
	if (pEvent)
		return createIndex(row, column, pEvent);
	else
		return QModelIndex();
}


QModelIndex qtractorMidiEventListView::ItemModel::parent ( const QModelIndex& ) const
{
	return QModelIndex();
}

void qtractorMidiEventListView::ItemModel::reset (void)
{
//	qDebug("reset()");

	m_pSeq = m_pEditor->sequence();

	m_iTimeOffset = m_pEditor->timeOffset();

	m_pEvent = nullptr;
	m_iEvent = 0;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	QAbstractItemModel::beginResetModel();
	QAbstractItemModel::endResetModel();
#else
	QAbstractItemModel::reset();
#endif
}


qtractorMidiEvent *qtractorMidiEventListView::ItemModel::eventAt ( int i ) const
{
	if (m_pSeq == nullptr)
		return nullptr;

	const int n = m_pSeq->events().count();
	const int m = (n >> 1);

	if (m_pEvent == nullptr || i > m_iEvent + m || i < m_iEvent - m) {
		if (i > m) {
			m_iEvent = n - 1;
			m_pEvent = m_pSeq->events().last();
		} else {
			m_iEvent = 0;
			m_pEvent = m_pSeq->events().first();
		}
	}

	if (i > m_iEvent) {
		while (m_pEvent && m_pEvent->next() && i > m_iEvent) {
			m_pEvent = m_pEvent->next();
			++m_iEvent;
		}
	}
	else
	if (i < m_iEvent) {
		while (m_pEvent && m_pEvent->prev() && i < m_iEvent) {
			m_pEvent = m_pEvent->prev();
			--m_iEvent;
		}
	}

	return m_pEvent;
}


qtractorMidiEvent *qtractorMidiEventListView::ItemModel::eventOfIndex (
	const QModelIndex& index ) const
{
	return static_cast<qtractorMidiEvent *> (index.internalPointer());
}


QModelIndex qtractorMidiEventListView::ItemModel::indexOfEvent (
	qtractorMidiEvent *pEvent ) const
{
	if (pEvent == nullptr)
		return QModelIndex();

	const unsigned long iTime = pEvent->time();
	if (indexFromTick(m_iTimeOffset + iTime).isValid()) {
		while (m_pEvent != pEvent && m_pEvent->next()
			&& m_pEvent->time() == iTime) {
			m_pEvent = m_pEvent->next();
			++m_iEvent;
		}
	}

	return (m_pEvent == pEvent
		? createIndex(m_iEvent, 0, m_pEvent) : QModelIndex());
}


QModelIndex qtractorMidiEventListView::ItemModel::indexFromTick (
	unsigned long iTick ) const
{
	if (m_pEvent == nullptr && m_pSeq) {
		m_iEvent = 0;
		m_pEvent = m_pSeq->events().first();
	}

	if (m_pEvent == nullptr)
		return QModelIndex();

	const unsigned long iTime
		= (iTick > m_iTimeOffset ? iTick - m_iTimeOffset : 0);

	if (m_pEvent->time() >= iTime) {
		while (m_pEvent && m_pEvent->prev()
			&& (m_pEvent->prev())->time() >= iTime) {
			m_pEvent = m_pEvent->prev();
			--m_iEvent;
		}
	}
	else
	while (m_pEvent && m_pEvent->next() && iTime > m_pEvent->time()) {
		m_pEvent = m_pEvent->next();
		++m_iEvent;
	}

	return createIndex(m_iEvent, 0, m_pEvent);
}


unsigned long qtractorMidiEventListView::ItemModel::tickFromIndex (
	const QModelIndex& index ) const
{
	qtractorMidiEvent *pEvent = eventOfIndex(index);
	return m_iTimeOffset + (pEvent ? pEvent->time() : 0);
}


QModelIndex qtractorMidiEventListView::ItemModel::indexFromFrame (
	unsigned long iFrame ) const
{
	return indexFromTick((m_pEditor->timeScale())->tickFromFrame(iFrame));
}


unsigned long qtractorMidiEventListView::ItemModel::frameFromIndex (
	const QModelIndex& index ) const
{
	return (m_pEditor->timeScale())->frameFromTick(tickFromIndex(index));
}


qtractorMidiEditor *qtractorMidiEventListView::ItemModel::editor (void) const
{
	return m_pEditor;
}


QString qtractorMidiEventListView::ItemModel::itemDisplay (
	const QModelIndex& index ) const
{
//	qDebug("itemDisplay(%d, %d)", index.row(), index.column());
	const QString sDashes(2, '-');

	qtractorMidiEvent *pEvent = eventOfIndex(index);
	if (pEvent) {
		switch (index.column()) {
		case 0: // Time.
			return (m_pEditor->timeScale())->textFromTick(
				m_iTimeOffset + pEvent->time());
		case 1: // Type.
			switch (pEvent->type()) {
			case qtractorMidiEvent::NOTEON:
				return tr("Note On (%1)").arg(pEvent->note());
			case qtractorMidiEvent::NOTEOFF:
				return tr("Note Off (%1)").arg(pEvent->note());
			case qtractorMidiEvent::KEYPRESS:
				return tr("Key Press (%1)").arg(pEvent->note());
			case qtractorMidiEvent::CONTROLLER:
				return tr("Controller (%1)").arg(pEvent->controller());
			case qtractorMidiEvent::CONTROL14:
				return tr("Control 14 (%1)").arg(pEvent->controller());
			case qtractorMidiEvent::REGPARAM:
				return tr("RPN (%1)").arg(pEvent->param());
			case qtractorMidiEvent::NONREGPARAM:
				return tr("NRPN (%1)").arg(pEvent->param());
			case qtractorMidiEvent::PGMCHANGE:
				return tr("Pgm Change");
			case qtractorMidiEvent::CHANPRESS:
				return tr("Chan Press");
			case qtractorMidiEvent::PITCHBEND:
				return tr("Pitch Bend");
			case qtractorMidiEvent::SYSEX:
				return tr("SysEx");
			case qtractorMidiEvent::META:
				return tr("Meta (%1)").arg(int(pEvent->type()));
			default:
				break;
			}
			return tr("Unknown (%1)").arg(int(pEvent->type()));
		case 2: // Name.
			switch (pEvent->type()) {
			case qtractorMidiEvent::NOTEON:
			case qtractorMidiEvent::NOTEOFF:
			case qtractorMidiEvent::KEYPRESS:
				return m_pEditor->noteName(pEvent->note());
			case qtractorMidiEvent::PGMCHANGE:
				return m_pEditor->programName(pEvent->param());
			case qtractorMidiEvent::CONTROLLER:
				return m_pEditor->controllerName(pEvent->controller());
			case qtractorMidiEvent::REGPARAM:
				return m_pEditor->rpnNames().value(pEvent->param());
			case qtractorMidiEvent::NONREGPARAM:
				return m_pEditor->nrpnNames().value(pEvent->param());
			case qtractorMidiEvent::CONTROL14:
				return m_pEditor->control14Name(pEvent->controller());
			default:
				break;
			}
			break;
		case 3: // Value.
			switch (pEvent->type()) {
			case qtractorMidiEvent::NOTEON:
			case qtractorMidiEvent::NOTEOFF:
			case qtractorMidiEvent::KEYPRESS:
				return QString::number(pEvent->velocity());
			case qtractorMidiEvent::PGMCHANGE:
				return QString::number(pEvent->param());
			case qtractorMidiEvent::CONTROLLER:
			case qtractorMidiEvent::REGPARAM:
			case qtractorMidiEvent::NONREGPARAM:
			case qtractorMidiEvent::CONTROL14:
			case qtractorMidiEvent::CHANPRESS:
				return QString::number(pEvent->value());
			case qtractorMidiEvent::PITCHBEND:
				return QString::number(pEvent->pitchBend());
			case qtractorMidiEvent::SYSEX:
				return QString::number(pEvent->sysex_len());
			default:
				break;
			}
			break;
		case 4: // Duration/Data
			switch (pEvent->type()) {
			case qtractorMidiEvent::NOTEON:
				return (m_pEditor->timeScale())->textFromTick(
					m_iTimeOffset + pEvent->time(), true, pEvent->duration());
			case qtractorMidiEvent::SYSEX:
			{
				QString sText;
				unsigned char *data = pEvent->sysex();
				unsigned short len  = pEvent->sysex_len();
				sText += '{';
				sText += ' ';
				for (unsigned short i = 0; i < len; ++i)
				#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
					sText += QString().sprintf("%02x ", data[i]);
				#else
					sText += QString::asprintf("%02x ", data[i]);
				#endif
				sText += '}';
				return sText;
			}
			default:
				break;
			}
			break;
		}
	}

	return sDashes;
}


QString qtractorMidiEventListView::ItemModel::itemToolTip (
	const QModelIndex& index ) const
{
	qtractorMidiEvent *pEvent = eventOfIndex(index);
	if (pEvent)
		return m_pEditor->eventToolTip(pEvent);
	else
		return QString();
}


int qtractorMidiEventListView::ItemModel::columnAlignment( int column ) const
{
	switch (column) {
	case 0: // Time.
		return int(Qt::AlignHCenter | Qt::AlignVCenter);
	case 3: // Value.
		return int(Qt::AlignRight | Qt::AlignVCenter);
	default:
		return int(Qt::AlignLeft | Qt::AlignVCenter);
	}
}


//----------------------------------------------------------------------------
// qtractorMidiEventListView::ItemDelegate -- Custom (tree) list item delegate.

// Constructor.
qtractorMidiEventListView::ItemDelegate::ItemDelegate (
	QObject *pParent ) : QItemDelegate(pParent)
{
}


// QItemDelegate Interface...

void qtractorMidiEventListView::ItemDelegate::paint ( QPainter *pPainter,
	const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QItemDelegate::paint(pPainter, option, index);
}


QSize qtractorMidiEventListView::ItemDelegate::sizeHint (
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	return QItemDelegate::sizeHint(option, index) + QSize(4, 4);
}


QWidget *qtractorMidiEventListView::ItemDelegate::createEditor ( QWidget *pParent,
	const QStyleOptionViewItem& /*option*/, const QModelIndex& index ) const
{
	const ItemModel *pItemModel
		= static_cast<const ItemModel *> (index.model());

	qtractorMidiEvent *pEvent = pItemModel->eventOfIndex(index);
	if (pEvent == nullptr)
		return nullptr;

	qtractorMidiEditor *pMidiEditor = pItemModel->editor();
	if (pMidiEditor == nullptr)
		return nullptr;

	QWidget *pEditor = nullptr;

	switch (index.column()) {
	case 0: // Time.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= new qtractorTimeSpinBox(pParent);
		pTimeSpinBox->setTimeScale(pMidiEditor->timeScale());
		pEditor = pTimeSpinBox;
		break;
	}

	case 2: // Name.
	{
		if (pEvent->type() == qtractorMidiEvent::NOTEON ||
			pEvent->type() == qtractorMidiEvent::KEYPRESS) {
			QComboBox *pComboBox = new QComboBox(pParent);
			for (unsigned char note = 0; note < 128; ++note)
				pComboBox->addItem(pMidiEditor->noteName(note));
			pEditor = pComboBox;
		}
		break;
	}

	case 3: // Value.
	{
		QSpinBox *pSpinBox = new QSpinBox(pParent);
		const qtractorMidiEvent::EventType etype = pEvent->type();
		if (etype == qtractorMidiEvent::PITCHBEND) {
			pSpinBox->setMinimum(-8192);
			pSpinBox->setMaximum(+8192);
		}
		else
		if (etype == qtractorMidiEvent::REGPARAM    ||
			etype == qtractorMidiEvent::NONREGPARAM ||
			etype == qtractorMidiEvent::CONTROL14) {
			pSpinBox->setMinimum(0);
			pSpinBox->setMaximum(16383);
		} else {
			pSpinBox->setMinimum(0);
			pSpinBox->setMaximum(127);
		}
		pEditor = pSpinBox;
		break;
	}

	case 4: // Duration/Data.
	{
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			qtractorTimeSpinBox *pTimeSpinBox
				= new qtractorTimeSpinBox(pParent);
			pTimeSpinBox->setTimeScale(pMidiEditor->timeScale());
			pTimeSpinBox->setDeltaValue(true);
			pEditor = pTimeSpinBox;
		}
		break;
	}

	default:
		break;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventListView::ItemDelegate::createEditor(%p, %d, %d) = %p",
		pParent, index.row(), index.column(), pEditor);
#endif

	return pEditor;
}


void qtractorMidiEventListView::ItemDelegate::setEditorData ( QWidget *pEditor,
	const QModelIndex& index ) const
{
	const ItemModel *pItemModel
		= static_cast<const ItemModel *> (index.model());

	qtractorMidiEvent *pEvent = pItemModel->eventOfIndex(index);
	if (pEvent == nullptr)
		return;

	qtractorMidiEditor *pMidiEditor = pItemModel->editor();
	if (pMidiEditor == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventListView::ItemDelegate::setEditorData(%p, %p, %d, %d)",
		pEditor, pItemModel, index.row(), index.column());
#endif

	qtractorTimeScale *pTimeScale = pMidiEditor->timeScale();

	switch (index.column()) {
	case 0: // Time.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			pTimeSpinBox->setValue(pTimeScale->frameFromTick(
				pMidiEditor->timeOffset() + pEvent->time()));
		}
		break;
	}

	case 2: // Name.
	{
		QComboBox *pComboBox = qobject_cast<QComboBox *> (pEditor);
		if (pComboBox)
			pComboBox->setCurrentIndex(int(pEvent->note()));
		break;
	}

	case 3: // Value.
	{
		QSpinBox *pSpinBox = qobject_cast<QSpinBox *> (pEditor);
		if (pSpinBox) {
			if (pEvent->type() == qtractorMidiEvent::PITCHBEND)
				pSpinBox->setValue(pEvent->pitchBend());
			else
			if (pEvent->type() == qtractorMidiEvent::PGMCHANGE)
				pSpinBox->setValue(pEvent->param());
			else
				pSpinBox->setValue(pEvent->value());
		}
		break;
	}

	case 4: // Duration/Data.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			pTimeSpinBox->setValue(
				pTimeScale->frameFromTick(pEvent->duration()));
		}
		break;
	}

	default:
		break;
	}
}


void qtractorMidiEventListView::ItemDelegate::setModelData ( QWidget *pEditor,
	QAbstractItemModel *pModel,	const QModelIndex& index ) const
{
	const ItemModel *pItemModel
		= static_cast<const ItemModel *> (pModel);

	qtractorMidiEvent *pEvent = pItemModel->eventOfIndex(index);
	if (pEvent == nullptr)
		return;

	qtractorMidiEditor *pMidiEditor = pItemModel->editor();
	if (pMidiEditor == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventListView::ItemDelegate::setModelData(%p, %p, %d, %d)",
		pEditor, pItemModel, index.row(), index.column());
#endif

	qtractorTimeScale *pTimeScale = pMidiEditor->timeScale();

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(pMidiEditor->midiClip(),
			tr("edit %1").arg(pItemModel->headerData(
				index.column(), Qt::Horizontal, Qt::DisplayRole)
					.toString().toLower()));

	switch (index.column()) {
	case 0: // Time.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			unsigned long iTime
				= pTimeScale->tickFromFrame(pTimeSpinBox->valueFromText());
			if (iTime  > pMidiEditor->timeOffset())
				iTime -= pMidiEditor->timeOffset();
			else
				iTime = 0;
			unsigned long iDuration = 0;
			if (pEvent->type() == qtractorMidiEvent::NOTEON)
				iDuration = pEvent->duration();
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
		break;
	}

	case 2: // Name.
	{
		QComboBox *pComboBox = qobject_cast<QComboBox *> (pEditor);
		if (pComboBox) {
			const int iNote = pComboBox->currentIndex();
			const unsigned long iTime = pEvent->time();
			pEditCommand->moveEvent(pEvent, iNote, iTime);
		}
		break;
	}

	case 3: // Value.
	{
		QSpinBox *pSpinBox = qobject_cast<QSpinBox *> (pEditor);
		if (pSpinBox) {
			const int iValue = pSpinBox->value();
			pEditCommand->resizeEventValue(pEvent, iValue);
		}
		break;
	}

	case 4: // Duration/Data.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			const unsigned long iTime = pEvent->time();
			const unsigned long iDuration
				= pTimeScale->tickFromFrame(pTimeSpinBox->value());
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
		break;
	}

	default:
		break;
	}

	// Do it.
	pMidiEditor->execute(pEditCommand);
}


//----------------------------------------------------------------------------
// qtractorMidiEventListView -- Custom (tree) list view.

// Constructor.
qtractorMidiEventListView::qtractorMidiEventListView ( QWidget *pParent )
	: QTreeView(pParent), m_pItemModel(nullptr), m_pItemDelegate(nullptr)
{
}


// Destructor.
qtractorMidiEventListView::~qtractorMidiEventListView (void)
{
	if (m_pItemDelegate)
		delete m_pItemDelegate;

	if (m_pItemModel)
		delete m_pItemModel;
}


// Settlers.
void qtractorMidiEventListView::setEditor ( qtractorMidiEditor *pEditor )
{
	if (m_pItemDelegate)
		delete m_pItemDelegate;

	if (m_pItemModel)
		delete m_pItemModel;

	m_pItemModel = new ItemModel(pEditor);
	m_pItemDelegate = new ItemDelegate();

	QTreeView::setModel(m_pItemModel);
	QTreeView::setItemDelegate(m_pItemDelegate);

	QTreeView::setSelectionMode(QAbstractItemView::ExtendedSelection);
	QTreeView::setRootIsDecorated(false);
	QTreeView::setUniformRowHeights(true);
	QTreeView::setItemsExpandable(false);
	QTreeView::setAllColumnsShowFocus(true);
	QTreeView::setAlternatingRowColors(true);

	QHeaderView *pHeader = QTreeView::header();
//	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setDefaultSectionSize(80);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	pHeader->setSectionsMovable(false);
#else
	pHeader->setMovable(false);
#endif
	pHeader->resizeSection(2, 60); // Name
	pHeader->resizeSection(3, 60); // Value
	pHeader->setStretchLastSection(true);
}


qtractorMidiEditor *qtractorMidiEventListView::editor (void) const
{
	return (m_pItemModel ? m_pItemModel->editor() : nullptr);
}


// Refreshner.
void qtractorMidiEventListView::refresh (void)
{
	if (m_pItemModel == nullptr)
		return;

	QItemSelectionModel *pSelectionModel = QTreeView::selectionModel();

	const QModelIndex& index = pSelectionModel->currentIndex();

	m_pItemModel->reset();

	pSelectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
}


// Locators.
qtractorMidiEvent *qtractorMidiEventListView::eventOfIndex (
	const QModelIndex& index) const
{
	return (m_pItemModel ? m_pItemModel->eventOfIndex(index) : nullptr);
}


QModelIndex qtractorMidiEventListView::indexOfEvent (
	qtractorMidiEvent *pEvent ) const
{
	return (m_pItemModel ? m_pItemModel->indexOfEvent(pEvent) : QModelIndex());
}


QModelIndex qtractorMidiEventListView::indexFromTick (
	unsigned long iTick ) const
{
	return (m_pItemModel ? m_pItemModel->indexFromTick(iTick) : QModelIndex());
}


unsigned long qtractorMidiEventListView::tickFromIndex (
	const QModelIndex& index ) const
{
	return (m_pItemModel ? m_pItemModel->tickFromIndex(index) : 0);
}


QModelIndex qtractorMidiEventListView::indexFromFrame (
	unsigned long iFrame ) const
{
	return (m_pItemModel ? m_pItemModel->indexFromFrame(iFrame) : QModelIndex());
}


unsigned long qtractorMidiEventListView::frameFromIndex (
	const QModelIndex& index ) const
{
	return (m_pItemModel ? m_pItemModel->frameFromIndex(index) : 0);
}


void qtractorMidiEventListView::selectEvent (
	qtractorMidiEvent *pEvent, bool bSelect )
{
	if (m_pItemModel == nullptr)
		return;

	const QModelIndex& index = m_pItemModel->indexOfEvent(pEvent);
	if (index.isValid()) {
		QItemSelectionModel::SelectionFlags flags = QItemSelectionModel::Rows;
		if (bSelect)
			flags |= QItemSelectionModel::Select;
		else
			flags |= QItemSelectionModel::Deselect;
		QTreeView::selectionModel()->select(index, flags);
	}
}


// Custom editor closing stub.
void qtractorMidiEventListView::closeEditor (
	QWidget */*pEditor*/, QAbstractItemDelegate::EndEditHint /*hint*/ )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEventListView[%p]::closeEditor(%p, 0x%04x)", this, pEditor, uint(hint));
#endif

	// FIXME: Avoid spitting out QAbstractItemView::closeEditor
	// called with an editor that does not belong to this view...
//	QTreeView::closeEditor(pEditor, hint);
}


//----------------------------------------------------------------------------
// qtractorMidiEventList -- MIDI Event List dockable window.

// Constructor.
qtractorMidiEventList::qtractorMidiEventList ( QWidget *pParent )
	: QDockWidget(pParent)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	QDockWidget::setObjectName("qtractorMidiEventList");

	// Create local list view.
	m_pListView = new qtractorMidiEventListView();

	// Cross-selection fake-mutex.
	m_iSelectUpdate = 0;

	const QFont& font = QDockWidget::font();
	m_pListView->setFont(QFont(font.family(), font.pointSize() - 1));

	// Prepare the dockable window stuff.
	QDockWidget::setWidget(m_pListView);
//	QDockWidget::setFeatures(QDockWidget::AllDockWidgetFeatures);
	QDockWidget::setAllowedAreas(
		Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	// Some specialties to this kind of dock window...
	QDockWidget::setMinimumWidth(280);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Events");
	QDockWidget::setWindowTitle(sCaption);
	QDockWidget::setWindowIcon(QIcon(":/images/viewEvents.png"));
	QDockWidget::setToolTip(sCaption);
}


// Destructor.
qtractorMidiEventList::~qtractorMidiEventList (void)
{
	// No need to delete child widgets, Qt does it all for us.
}



// Full update when show up.
void qtractorMidiEventList::showEvent ( QShowEvent *pShowEvent )
{
	QDockWidget::showEvent(pShowEvent);

	refresh();
}


// Just about to notify main-window that we're closing.
void qtractorMidiEventList::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
	QDockWidget::hide();

	qtractorMidiEditorForm *pMidiEditorForm
		= static_cast<qtractorMidiEditorForm *> (QDockWidget::parentWidget());
	if (pMidiEditorForm)
		pMidiEditorForm->stabilizeForm();
}


// Settlers.
void qtractorMidiEventList::setEditor ( qtractorMidiEditor *pEditor )
{
	m_pListView->setEditor(pEditor);

	// Set internal list view change connections.
	QObject::connect(m_pListView->selectionModel(),
		SIGNAL(currentRowChanged(const QModelIndex&,const QModelIndex&)),
		SLOT(currentRowChangedSlot(const QModelIndex&,const QModelIndex&)));
	QObject::connect(m_pListView->selectionModel(),
		SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)),
		SLOT(selectionChangedSlot(const QItemSelection&,const QItemSelection&)));

	// Set MIDI editor change connections.
	QObject::connect(pEditor,
		SIGNAL(selectNotifySignal(qtractorMidiEditor *)),
		SLOT(selectNotifySlot(qtractorMidiEditor *)));
	QObject::connect(pEditor,
		SIGNAL(changeNotifySignal(qtractorMidiEditor *)),
		SLOT(changeNotifySlot(qtractorMidiEditor *)));
}


qtractorMidiEditor *qtractorMidiEventList::editor (void) const
{
	return m_pListView->editor();
}


// Event list view refreshner.
void qtractorMidiEventList::refresh (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEventList[%p]::refresh()", this);
#endif

	m_pListView->refresh();

	selectNotifySlot(m_pListView->editor());
}


// Context menu request event handler.
void qtractorMidiEventList::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	qtractorMidiEditorForm *pMidiEditorForm
		= static_cast<qtractorMidiEditorForm *> (QDockWidget::parentWidget());
	if (pMidiEditorForm) {
		pMidiEditorForm->stabilizeForm();
		pMidiEditorForm->editMenu()->exec(pContextMenuEvent->globalPos());
	}
}


// Current list view row changed slot.
void qtractorMidiEventList::currentRowChangedSlot (
	const QModelIndex& index, const QModelIndex& /*previous*/ )
{
	qtractorMidiEditor *pEditor = m_pListView->editor();
	if (pEditor == nullptr)
		return;

	if (m_iSelectUpdate > 0)
		return;

	++m_iSelectUpdate;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEventList[%p]::currentRowChangedSlot()", this);
#endif

//	pEditor->setEditHead(m_pListView->frameFromIndex(index));
	pEditor->ensureVisibleFrame(pEditor->editView(),
		m_pListView->frameFromIndex(index));
	pEditor->selectionChangeNotify();

	--m_iSelectUpdate;
}


// Current list view selection changed slot.
void qtractorMidiEventList::selectionChangedSlot (
	const QItemSelection& selected, const QItemSelection& deselected )
{
	qtractorMidiEditor *pEditor = m_pListView->editor();
	if (pEditor == nullptr)
		return;

	if (m_iSelectUpdate > 0)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEventList[%p]::selectionChangedSlot()", this);
#endif

	++m_iSelectUpdate;

	m_pListView->setUpdatesEnabled(false);
	QListIterator<QModelIndex> iter1(selected.indexes());
	while (iter1.hasNext()) {
		const QModelIndex& index = iter1.next();
		if (index.column() == 0)
			pEditor->selectEvent(m_pListView->eventOfIndex(index), true);
	}
	QListIterator<QModelIndex> iter2(deselected.indexes());
	while (iter2.hasNext()) {
		const QModelIndex& index = iter2.next();
		if (index.column() == 0)
			pEditor->selectEvent(m_pListView->eventOfIndex(index), false);
	}
	m_pListView->setUpdatesEnabled(true);

	pEditor->selectionChangeNotify();

	--m_iSelectUpdate;
}


// MIDI editor selection changed slot.
void qtractorMidiEventList::selectNotifySlot (
	qtractorMidiEditor *pEditor )
{
	if (!isVisible())
		return;

	if (m_iSelectUpdate > 0)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEventList[%p]::selectNotifySlot()", this);
#endif

	++m_iSelectUpdate;

	m_pListView->clearSelection();

	const QList<qtractorMidiEvent *>& list = pEditor->selectedEvents();
	if (list.count() > 0) {
		m_pListView->setCurrentIndex(
			m_pListView->indexOfEvent(list.first()));
		QListIterator<qtractorMidiEvent *> iter(list);
		while (iter.hasNext())
			m_pListView->selectEvent(iter.next(), true);
	}

	--m_iSelectUpdate;
}


// MIDI editor selection changed slot.
void qtractorMidiEventList::changeNotifySlot ( qtractorMidiEditor * )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEventList[%p]::changeNotifySlot()", this);
#endif

	refresh();
}


// end of qtractorMidiEventList.cpp
