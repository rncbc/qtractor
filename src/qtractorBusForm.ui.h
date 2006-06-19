// qtractorBusForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include <qpopupmenu.h>


// Bus list view item.
class qtractorBusListItem : public QListViewItem
{
public:

	// Constructor.
	qtractorBusListItem(QListViewItem *pRootItem, qtractorBus *pBus)
		: QListViewItem(pRootItem, pBus->busName()), m_pBus(pBus) {}

	// Bus accessors.
	qtractorBus *bus() const { return m_pBus; }

private:

	// Instance variables.
	qtractorBus *m_pBus;
};


// Kind of constructor.
void qtractorBusForm::init (void)
{
	// Initialize locals.
	m_busType    = qtractorTrack::None;
	m_pAudioRoot = NULL;
	m_pMidiRoot  = NULL;

	// Initial contents.
	refreshBusses();

	// Try to restore normal window positioning.
	adjustSize();
}


// Kind of destructor.
void qtractorBusForm::destroy (void)
{
}


// Set current selected bus.
void qtractorBusForm::setBus ( qtractorBus *pBus )
{
	// Get the device view root item...
	QListViewItem *pRootItem = NULL;
	switch (pBus->busType()) {
	case qtractorTrack::Audio:
		pRootItem = m_pAudioRoot;
		break;
	case qtractorTrack::Midi:
		pRootItem = m_pMidiRoot;
		break;
	default:
		break;
	}

	// Is the root present?
	if (pRootItem == NULL)
		return;

	// For each child, test for identity...
	QListViewItem *pListItem = pRootItem->firstChild();
	while (pListItem) {
		// If identities match, select as current device item.
		qtractorBusListItem *pBusItem
			= static_cast<qtractorBusListItem *> (pListItem);
		if (pBusItem && pBusItem->bus() == pBus) {
			BusListView->setSelected(pBusItem, true);
			break;
		}
		pListItem = pListItem->nextSibling();
	}
}



// Refresh all busses list and views.
void qtractorBusForm::refreshBusses (void)
{
}


// Bus selection slot.
void qtractorBusForm::selectBus (void)
{
}


// Create a new bus from current view.
void qtractorBusForm::createBus (void)
{
}


// Update current bus in view.
void qtractorBusForm::updateBus (void)
{
}


// Delete current bus in view.
void qtractorBusForm::deleteBus (void)
{
}


// Bus list view context menu handler.
void qtractorBusForm::contextMenu ( QListViewItem *pItem, const QPoint& pos, int )
{
	int iItemID;

	// Build the device context menu...
	QPopupMenu* pContextMenu = new QPopupMenu(this);

	bool bEnabled = (pItem != NULL);
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formCreate.png")),
		tr("&Create bus"), this, SLOT(createBus()));
	pContextMenu->setItemEnabled(iItemID, bEnabled);
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formAccept.png")),
		tr("&Delete bus"), this, SLOT(updateBus()));
	pContextMenu->setItemEnabled(iItemID, bEnabled);
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formRemove.png")),
		tr("&Delete bus"), this, SLOT(deleteBus()));
	pContextMenu->setItemEnabled(iItemID, bEnabled);
	pContextMenu->insertSeparator();
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formRefresh.png")),
		tr("&Refresh"), this, SLOT(refreshBusses()));
	pContextMenu->setItemEnabled(iItemID, bEnabled);

	pContextMenu->exec(pos);

	delete pContextMenu;
}


// Stabilize current form state.
void qtractorBusForm::stabilizeForm (void)
{
	QListViewItem *pItem = BusListView->selectedItem();
	bool bEnabled = (pItem != NULL);
	RefreshPushButton->setEnabled(bEnabled);
	CreatePushButton->setEnabled(bEnabled);
	UpdatePushButton->setEnabled(bEnabled);
	DeletePushButton->setEnabled(bEnabled);
}


// end of qtractorBusForm.ui.h
