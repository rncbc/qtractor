// qtractorMidiEventListModel.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEventListModel.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiClip.h"

#include "qtractorTimeScale.h"

#include <QHeaderView>


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

	QAbstractItemModel::reset();
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
			default:
				break;
			}
			break;
		case 3: // Value.
			switch (pEvent->type()) {
			case qtractorMidiEvent::NOTEON:
			case qtractorMidiEvent::NOTEOFF:
				return QString::number(pEvent->velocity());
			case qtractorMidiEvent::KEYPRESS:
			case qtractorMidiEvent::CONTROLLER:
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


// end of qtractorMidiEventListModel.h
