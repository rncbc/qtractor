// qtractorMidiEventListModel.h
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

#ifndef __qtractorMidiEventListModel_h
#define __qtractorMidiEventListModel_h

#include <QAbstractItemModel>
#include <QItemDelegate>


// Forwards.
class qtractorMidiEditor;
class qtractorMidiEvent;
class qtractorMidiSequence;


//----------------------------------------------------------------------------
// qtractorMidiEventListModel -- List model.

class qtractorMidiEventListModel : public QAbstractItemModel
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEventListModel(qtractorMidiEditor *pEditor, QObject *pParent = 0);

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


#endif  // __qtractorMidiEventListModel_h


// end of qtractorMidiEventListModel.h
