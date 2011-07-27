// qtractorMidiEventListView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEventListView.h"
#include "qtractorMidiEventListModel.h"
#include "qtractorMidiEventItemDelegate.h"

#include <QHeaderView>


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
//	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setDefaultSectionSize(80);
	pHeader->resizeSection(2, 60); // Name
	pHeader->resizeSection(3, 40); // Value
//	pHeader->setDefaultAlignment(Qt::AlignLeft);
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


// end of qtractorMidiEventListView.h
