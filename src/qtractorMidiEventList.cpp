// qtractorMidiEventList.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiEventList.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditorForm.h"
#include "qtractorMidiEditCommand.h"
#include "qtractorMidiSequence.h"

#include "qtractorSpinBox.h"

#include <QHeaderView>
#include <QComboBox>
#include <QSpinBox>

#include <QContextMenuEvent>


//----------------------------------------------------------------------------
// qtractorMidiEventListModel -- List model.

// Constructor.
qtractorMidiEventListModel::qtractorMidiEventListModel (
	qtractorMidiEditor *pEditor, QObject *pParent )
	: QAbstractItemModel(pParent), m_pEditor(pEditor),
		m_pSeq(NULL), m_iTimeOffset(0),
		m_pEvent(NULL), m_iEvent(0)
{
	m_headers
		<< tr("Time")
		<< tr("Type")
		<< tr("Name")
		<< tr("Value")
		<< tr("Duration/Data");

	reset();
}


int qtractorMidiEventListModel::rowCount (
	const QModelIndex& /*parent*/ ) const
{
	return (m_pSeq ? m_pSeq->events().count() : 0);
}


int qtractorMidiEventListModel::columnCount (
	const QModelIndex& /*parent*/ ) const
{
	return m_headers.count();
}


QVariant qtractorMidiEventListModel::headerData (
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


QVariant qtractorMidiEventListModel::data (
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
		break;
	}
	return QVariant();
}


Qt::ItemFlags qtractorMidiEventListModel::flags (
	const QModelIndex& index ) const
{
	Qt::ItemFlags ret = Qt::ItemFlags(0);

	qtractorMidiEvent *pEvent = eventOfIndex(index);
	if (pEvent) {
		if (m_pEditor->isEventSelectable(pEvent))
			ret = QAbstractItemModel::flags(index);
		else
			ret = Qt::ItemIsEnabled;
		ret |= Qt::ItemIsEditable;
	}

	return ret;
}


QModelIndex qtractorMidiEventListModel::index (
	int row, int column, const QModelIndex& /*parent*/) const
{
//	qDebug("index(%d, %d)", row, column);
	qtractorMidiEvent *pEvent = eventAt(row);
	if (pEvent)
		return createIndex(row, column, pEvent);
	else
		return QModelIndex();
}


QModelIndex qtractorMidiEventListModel::parent ( const QModelIndex& ) const
{
	return QModelIndex();
}

void qtractorMidiEventListModel::reset (void)
{
//	qDebug("reset()");

	m_pSeq = m_pEditor->sequence();

	m_iTimeOffset = m_pEditor->timeOffset();

	m_pEvent = NULL;
	m_iEvent = 0;

#if QT_VERSION >= 0x050000
	QAbstractItemModel::beginResetModel();
	QAbstractItemModel::endResetModel();
#else
	QAbstractItemModel::reset();
#endif
}


qtractorMidiEvent *qtractorMidiEventListModel::eventAt ( int i ) const
{
	if (m_pSeq == NULL)
		return NULL;

	const int n = m_pSeq->events().count();
	const int m = (n >> 1);

	if (m_pEvent == NULL || i > m_iEvent + m || i < m_iEvent - m) {
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


qtractorMidiEvent *qtractorMidiEventListModel::eventOfIndex (
	const QModelIndex& index ) const
{
	return static_cast<qtractorMidiEvent *> (index.internalPointer());
}


QModelIndex qtractorMidiEventListModel::indexOfEvent (
	qtractorMidiEvent *pEvent ) const
{
	if (pEvent == NULL)
		return QModelIndex();

	unsigned long iTime = pEvent->time();
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


QModelIndex qtractorMidiEventListModel::indexFromTick (
	unsigned long iTick ) const
{
	if (m_pEvent == NULL && m_pSeq) {
		m_iEvent = 0;
		m_pEvent = m_pSeq->events().first();
	}

	if (m_pEvent == NULL)
		return QModelIndex();

	unsigned long iTime = (iTick > m_iTimeOffset ? iTick - m_iTimeOffset : 0);

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

	//qDebug("indexFromTick(%lu) index=%d time=%lu", iTime, m_iEvent, m_pEvent->time());

	return createIndex(m_iEvent, 0, m_pEvent);
}

unsigned long qtractorMidiEventListModel::tickFromIndex (
	const QModelIndex& index ) const
{
	qtractorMidiEvent *pEvent = eventOfIndex(index);
	return m_iTimeOffset + (pEvent ? pEvent->time() : 0);
}


QModelIndex qtractorMidiEventListModel::indexFromFrame (
	unsigned long iFrame ) const
{
	return indexFromTick((m_pEditor->timeScale())->tickFromFrame(iFrame));
}

unsigned long qtractorMidiEventListModel::frameFromIndex (
	const QModelIndex& index ) const
{
	return (m_pEditor->timeScale())->frameFromTick(tickFromIndex(index));
}


qtractorMidiEditor *qtractorMidiEventListModel::editor (void) const
{
	return m_pEditor;
}


QString qtractorMidiEventListModel::itemDisplay (
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
			case qtractorMidiEvent::CONTROLLER:
			case qtractorMidiEvent::REGPARAM:
			case qtractorMidiEvent::NONREGPARAM:
			case qtractorMidiEvent::CONTROL14:
			case qtractorMidiEvent::PGMCHANGE:
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
					sText += QString().sprintf("%02x ", data[i]);
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


QString qtractorMidiEventListModel::itemToolTip (
	const QModelIndex& index ) const
{
	qtractorMidiEvent *pEvent = eventOfIndex(index);
	if (pEvent)
		return m_pEditor->eventToolTip(pEvent);
	else
		return QString();
}


int qtractorMidiEventListModel::columnAlignment( int column ) const
{
	if (column == 0 || column == 3) // Time,Value.
		return int(Qt::AlignRight | Qt::AlignVCenter);
	else
		return int(Qt::AlignLeft  | Qt::AlignVCenter);
}


//----------------------------------------------------------------------------
// qtractorMidiEventItemDelegate -- Custom (tree) list item delegate.

// Constructor.
qtractorMidiEventItemDelegate::qtractorMidiEventItemDelegate (
	QObject *pParent ) : QItemDelegate(pParent)
{
}


// Destructor.
qtractorMidiEventItemDelegate::~qtractorMidiEventItemDelegate (void)
{
}


// Keyboard event hook.
bool qtractorMidiEventItemDelegate::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	QWidget *pEditor = qobject_cast<QWidget*> (pObject);

	if (pEditor) {

		switch (pEvent->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		{
			QKeyEvent *pKeyEvent = static_cast<QKeyEvent *> (pEvent);
			if ((pKeyEvent->modifiers() & Qt::ControlModifier) == 0 &&
				(pKeyEvent->key() == Qt::Key_Up || pKeyEvent->key() == Qt::Key_Down)) {
				pEvent->ignore();
				return true;
			}
			if (pKeyEvent->key() == Qt::Key_Enter ||
				pKeyEvent->key() == Qt::Key_Return) {
				emit commitData(pEditor);
				if (pEditor)
					pEditor->close();
				return true;
			}
			break;
		}

		case QEvent::FocusOut:
		{
			if (!pEditor->isActiveWindow()
				|| (QApplication::focusWidget() != pEditor)) {
				QWidget *pWidget = QApplication::focusWidget();
				while (pWidget) {
					if (pWidget == pEditor)
						return false;
					pWidget = pWidget->parentWidget();
				}
				emit commitData(pEditor);
			}
			return false;
		}

		default:
			break;
		}
	}

	return QItemDelegate::eventFilter(pObject, pEvent);
}


// QItemDelegate Interface...

void qtractorMidiEventItemDelegate::paint ( QPainter *pPainter,
	const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QItemDelegate::paint(pPainter, option, index);
}


QSize qtractorMidiEventItemDelegate::sizeHint (
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	return QItemDelegate::sizeHint(option, index) + QSize(4, 4);
}


QWidget *qtractorMidiEventItemDelegate::createEditor ( QWidget *pParent,
	const QStyleOptionViewItem& /*option*/, const QModelIndex& index ) const
{
	const qtractorMidiEventListModel *pListModel
		= static_cast<const qtractorMidiEventListModel *> (index.model());

	qtractorMidiEvent *pEvent = pListModel->eventOfIndex(index);
	if (pEvent == NULL)
		return NULL;

	qtractorMidiEditor *pMidiEditor = pListModel->editor();
	if (pMidiEditor == NULL)
		return NULL;

	QWidget *pEditor = NULL;

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
			pTimeSpinBox->setTimeScale(
				(pListModel->editor())->timeScale());
			pTimeSpinBox->setDeltaValue(true);
			pEditor = pTimeSpinBox;
		}
		break;
	}

	default:
		break;
	}

	if (pEditor) {
		pEditor->installEventFilter(
			const_cast<qtractorMidiEventItemDelegate *> (this));
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventItemDelegate::createEditor(%p, %d, %d) = %p",
		pParent, index.row(), index.column(), pEditor);
#endif

	return pEditor;
}


void qtractorMidiEventItemDelegate::setEditorData ( QWidget *pEditor,
	const QModelIndex& index ) const
{
	const qtractorMidiEventListModel *pListModel
		= static_cast<const qtractorMidiEventListModel *> (index.model());

	qtractorMidiEvent *pEvent = pListModel->eventOfIndex(index);
	if (pEvent == NULL)
		return;

	qtractorMidiEditor *pMidiEditor = pListModel->editor();
	if (pMidiEditor == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventItemDelegate::setEditorData(%p, %p, %d, %d)",
		pEditor, pListModel, index.row(), index.column());
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


void qtractorMidiEventItemDelegate::setModelData ( QWidget *pEditor,
	QAbstractItemModel *pModel,	const QModelIndex& index ) const
{
	const qtractorMidiEventListModel *pListModel
		= static_cast<const qtractorMidiEventListModel *> (pModel);

	qtractorMidiEvent *pEvent = pListModel->eventOfIndex(index);
	if (pEvent == NULL)
		return;

	qtractorMidiEditor *pMidiEditor = pListModel->editor();
	if (pMidiEditor == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventItemDelegate::setModelData(%p, %p, %d, %d)",
		pEditor, pListModel, index.row(), index.column());
#endif

	qtractorTimeScale *pTimeScale = pMidiEditor->timeScale();

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(pMidiEditor->midiClip(),
			tr("edit %1").arg(pListModel->headerData(
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
			int iNote = pComboBox->currentIndex();
			unsigned long iTime = pEvent->time();
			pEditCommand->moveEvent(pEvent, iNote, iTime);
		}
		break;
	}

	case 3: // Value.
	{
		QSpinBox *pSpinBox = qobject_cast<QSpinBox *> (pEditor);
		if (pSpinBox) {
			int iValue = pSpinBox->value();
			pEditCommand->resizeEventValue(pEvent, iValue);
		}
		break;
	}

	case 4: // Duration/Data.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			unsigned long iTime = pEvent->time();
			unsigned long iDuration
				= pTimeScale->tickFromFrame(pTimeSpinBox->value());
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
		break;
	}

	default:
		break;
	}

	// Do it.
	pMidiEditor->commands()->exec(pEditCommand);
}


void qtractorMidiEventItemDelegate::updateEditorGeometry ( QWidget *pEditor,
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	QItemDelegate::updateEditorGeometry(pEditor, option, index);
}


//----------------------------------------------------------------------------
// qtractorMidiEventListView -- Custom (tree) list view.

// Constructor.
qtractorMidiEventListView::qtractorMidiEventListView ( QWidget *pParent )
	: QTreeView(pParent), m_pListModel(NULL), m_pItemDelegate(NULL)
{
}


// Destructor.
qtractorMidiEventListView::~qtractorMidiEventListView (void)
{
	if (m_pItemDelegate)
		delete m_pItemDelegate;

	if (m_pListModel)
		delete m_pListModel;
}


// Settlers.
void qtractorMidiEventListView::setEditor ( qtractorMidiEditor *pEditor )
{
	if (m_pItemDelegate)
		delete m_pItemDelegate;

	if (m_pListModel)
		delete m_pListModel;

	m_pListModel = new qtractorMidiEventListModel(pEditor);
	m_pItemDelegate = new qtractorMidiEventItemDelegate();

	QTreeView::setModel(m_pListModel);
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
#if QT_VERSION >= 0x050000
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setMovable(false);
#endif
	pHeader->resizeSection(2, 60); // Name
	pHeader->resizeSection(3, 40); // Value
	pHeader->setStretchLastSection(true);
}


qtractorMidiEditor *qtractorMidiEventListView::editor (void) const
{
	return (m_pListModel ? m_pListModel->editor() : NULL);
}


// Refreshner.
void qtractorMidiEventListView::refresh (void)
{
	if (m_pListModel == NULL)
		return;

	QItemSelectionModel *pSelectionModel = QTreeView::selectionModel();

	const QModelIndex& index = pSelectionModel->currentIndex();

	m_pListModel->reset();

	pSelectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
}


// Locators.
qtractorMidiEvent *qtractorMidiEventListView::eventOfIndex (
	const QModelIndex& index) const
{
	return (m_pListModel ? m_pListModel->eventOfIndex(index) : NULL);
}


QModelIndex qtractorMidiEventListView::indexOfEvent (
	qtractorMidiEvent *pEvent ) const
{
	return (m_pListModel ? m_pListModel->indexOfEvent(pEvent) : QModelIndex());
}


QModelIndex qtractorMidiEventListView::indexFromTick (
	unsigned long iTick ) const
{
	return (m_pListModel ? m_pListModel->indexFromTick(iTick) : QModelIndex());
}

unsigned long qtractorMidiEventListView::tickFromIndex (
	const QModelIndex& index ) const
{
	return (m_pListModel ? m_pListModel->tickFromIndex(index) : 0);
}


QModelIndex qtractorMidiEventListView::indexFromFrame (
	unsigned long iFrame ) const
{
	return (m_pListModel ? m_pListModel->indexFromFrame(iFrame) : QModelIndex());
}

unsigned long qtractorMidiEventListView::frameFromIndex (
	const QModelIndex& index ) const
{
	return (m_pListModel ? m_pListModel->frameFromIndex(index) : 0);
}


void qtractorMidiEventListView::selectEvent (
	qtractorMidiEvent *pEvent, bool bSelect )
{
	if (m_pListModel == NULL)
		return;

	const QModelIndex& index = m_pListModel->indexOfEvent(pEvent);
	if (index.isValid()) {
		QItemSelectionModel::SelectionFlags flags = QItemSelectionModel::Rows;
		if (bSelect)
			flags |= QItemSelectionModel::Select;
		else
			flags |= QItemSelectionModel::Deselect;
		QTreeView::selectionModel()->select(index, flags);
	}
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
	QDockWidget::setFeatures(QDockWidget::AllDockWidgetFeatures);
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
		SIGNAL(selectNotifySignal(qtractorMidiEditor*)),
		SLOT(selectNotifySlot(qtractorMidiEditor*)));
	QObject::connect(pEditor,
		SIGNAL(changeNotifySignal(qtractorMidiEditor*)),
		SLOT(changeNotifySlot(qtractorMidiEditor*)));
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
	if (pMidiEditorForm)
		pMidiEditorForm->editMenu()->exec(pContextMenuEvent->globalPos());
}


// Current list view row changed slot.
void qtractorMidiEventList::currentRowChangedSlot (
	const QModelIndex& index, const QModelIndex& /*previous*/ )
{
	qtractorMidiEditor *pEditor = m_pListView->editor();
	if (pEditor == NULL)
		return;

	if (m_iSelectUpdate > 0)
		return;

	++m_iSelectUpdate;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEventList[%p]::currentRowChangedSlot()", this);
#endif

	pEditor->setEditHead(m_pListView->frameFromIndex(index));
	pEditor->selectionChangeNotify();

	--m_iSelectUpdate;
}


// Current list view selection changed slot.
void qtractorMidiEventList::selectionChangedSlot (
	const QItemSelection& selected, const QItemSelection& deselected )
{
	qtractorMidiEditor *pEditor = m_pListView->editor();
	if (pEditor == NULL)
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
