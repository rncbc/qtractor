// qtractorMidiEventList.cpp
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
#include "qtractorMidiEventList.h"
#include "qtractorMidiEventListView.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditorForm.h"

#include <QContextMenuEvent>


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


// end of qtractorMidiEventList.h
