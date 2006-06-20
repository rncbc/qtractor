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

#include "qtractorMainForm.h"

#include <qpopupmenu.h>


//----------------------------------------------------------------------
// class qtractorBusListItem -- Custom bus listview item.
//

class qtractorBusListItem : public QListViewItem
{
public:

	// Constructor.
	qtractorBusListItem(QListViewItem *pRootItem, qtractorBus *pBus)
		: QListViewItem(pRootItem, pBus->busName()), m_pBus(pBus)
	{
		switch (m_pBus->busType()) {
		case qtractorTrack::Audio:
			setPixmap(0, QPixmap::fromMimeSource("trackAudio.png"));
			break;
		case qtractorTrack::Midi:
			setPixmap(0, QPixmap::fromMimeSource("trackMidi.png"));
			break;
		default:
			break;
		}
	}

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
	m_pMainForm   = NULL;
	m_pBus        = NULL;
	m_pAudioRoot  = NULL;
	m_pMidiRoot   = NULL;
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;
}


// Kind of destructor.
void qtractorBusForm::destroy (void)
{
}


// Main form accessors.
void qtractorBusForm::setMainForm ( qtractorMainForm *pMainForm )
{
	m_pMainForm = pMainForm;

	// (Re)initial contents.
	refreshBusses();

	// Try to restore normal window positioning.
	adjustSize();
}

qtractorMainForm *qtractorBusForm::mainForm (void)
{
	return m_pMainForm;
}


// Set current bus.
void qtractorBusForm::setBus ( qtractorBus *pBus )
{
	// Get the device view root item...
	QListViewItem *pRootItem = NULL;
	if (pBus) {
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
	}
	// Is the root present?
	if (pRootItem == NULL) {
		stabilizeForm();
		return;
	}

	// For each child, test for identity...
	QListViewItem *pItem = pRootItem->firstChild();
	while (pItem) {
		// If identities match, select as current device item.
		qtractorBusListItem *pBusItem
			= static_cast<qtractorBusListItem *> (pItem);
		if (pBusItem && pBusItem->bus() == pBus) {
			BusListView->setSelected(pItem, true);
			break;
		}
		pItem = pItem->nextSibling();
	}
}


// Current bus accessor.
qtractorBus *qtractorBusForm::bus (void)
{
	return m_pBus;
}


// Show current selected bus.
void qtractorBusForm::showBus ( qtractorBus *pBus )
{
	m_iDirtySetup++;

	// Settle current bus reference...
	m_pBus = pBus;

	// Show bus properties into view pane...
	if (pBus) {
		BusNameTextLabel->setEnabled(true);
		BusNameLineEdit->setEnabled(true);
		BusModeTextLabel->setEnabled(true);
		BusModeComboBox->setEnabled(true);
		BusNameLineEdit->setText(pBus->busName());
		BusModeComboBox->setCurrentItem(int(pBus->busMode()) - 1);
		qtractorAudioBus *pAudioBus = NULL;
		if (pBus->busType() == qtractorTrack::Audio)
			pAudioBus = static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			AudioBusGroup->setEnabled(true);
			AudioChannelsSpinBox->setValue(pAudioBus->channels());
			AudioAutoConnectCheckBox->setChecked(pAudioBus->isAutoConnect());
		} else {
			AudioBusGroup->setEnabled(false);
		}
	} else {
		BusModeTextLabel->setEnabled(false);
		BusModeComboBox->setEnabled(false);
		BusNameTextLabel->setEnabled(false);
		BusNameLineEdit->setEnabled(false);
		AudioBusGroup->setEnabled(false);
	}
	
	m_iDirtySetup--;
	
	// Done.
	stabilizeForm();
}


// Check whether the the current view is ellegible as a new bus.
bool qtractorBusForm::canCreateBus (void)
{
	if (m_pMainForm == NULL)
		return false;
	if (m_pMainForm->session() == NULL)
		return false;

	const QString sBusName = BusNameLineEdit->text().stripWhiteSpace();
	if (sBusName.isEmpty())
		return false;

	// Get the device view root item...
	qtractorEngine *pEngine = NULL;
	if (m_pBus) {
		switch (m_pBus->busType()) {
		case qtractorTrack::Audio:
			pEngine = m_pMainForm->session()->audioEngine();
			break;
		case qtractorTrack::Midi:
			pEngine = m_pMainForm->session()->midiEngine();
			break;
		default:
			break;
		}
	}
	// Is it still valid?
	if (pEngine == NULL)
		return false;

	// Is there one already?
	return (pEngine->findBus(sBusName) == NULL);
}


// Refresh all busses list and views.
void qtractorBusForm::refreshBusses (void)
{
	//
	// (Re)Load complete bus listing ...
	//
	m_pAudioRoot = NULL;
	m_pMidiRoot  = NULL;
	BusListView->clear();
	
	if (m_pMainForm == NULL)
		return;
	if (m_pMainForm->session() == NULL)
		return;

	// Audio busses...
	qtractorAudioEngine *pAudioEngine = m_pMainForm->session()->audioEngine();
	if (pAudioEngine) {
		m_pAudioRoot = new QListViewItem(BusListView, tr("Audio"));
		m_pAudioRoot->setSelectable(false);
		for (qtractorBus *pBus = pAudioEngine->busses().first();
				pBus; pBus = pBus->next())
			new qtractorBusListItem(m_pAudioRoot, pBus);
		m_pAudioRoot->setOpen(true);
	}

	// MIDI busses...
	qtractorMidiEngine *pMidiEngine = m_pMainForm->session()->midiEngine();
	if (pMidiEngine) {
		m_pMidiRoot = new QListViewItem(BusListView, tr("MIDI"));
		m_pMidiRoot->setSelectable(false);
		for (qtractorBus *pBus = pMidiEngine->busses().first();
				pBus; pBus = pBus->next())
			new qtractorBusListItem(m_pMidiRoot, pBus);
		m_pMidiRoot->setOpen(true);
	}
	
	// Reselect current bus, if any.
	setBus(m_pBus);
}


// Bus selection slot.
void qtractorBusForm::selectBus (void)
{
	// Get current selected item, must not be a root one...
	QListViewItem *pItem = BusListView->selectedItem();
	if (pItem == NULL)
		return;
	if (pItem->parent() == NULL)
		return;

	// Just make it in current view...
	qtractorBusListItem *pBusItem
		= static_cast<qtractorBusListItem *> (pItem);
	if (pBusItem)
		showBus(pBusItem->bus());
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


// Make changes due.
void qtractorBusForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	m_iDirtyCount++;
	stabilizeForm();
}


// Stabilize current form state.
void qtractorBusForm::stabilizeForm (void)
{
	QListViewItem *pItem = BusListView->selectedItem();
	bool bEnabled = (pItem != NULL);
	RefreshPushButton->setEnabled(true);
	CreatePushButton->setEnabled(canCreateBus() && m_iDirtyCount > 0);
	UpdatePushButton->setEnabled(bEnabled && m_iDirtyCount > 0);
	DeletePushButton->setEnabled(bEnabled);
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
		tr("&Create"), this, SLOT(createBus()));
	pContextMenu->setItemEnabled(iItemID, canCreateBus() && m_iDirtyCount > 0);
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formAccept.png")),
		tr("&Update"), this, SLOT(updateBus()));
	pContextMenu->setItemEnabled(iItemID, bEnabled && m_iDirtyCount > 0);
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formRemove.png")),
		tr("&Delete"), this, SLOT(deleteBus()));
	pContextMenu->setItemEnabled(iItemID, bEnabled);
	pContextMenu->insertSeparator();
	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formRefresh.png")),
		tr("&Refresh"), this, SLOT(refreshBusses()));
	pContextMenu->setItemEnabled(iItemID, true);

	pContextMenu->exec(pos);

	delete pContextMenu;
}


// end of qtractorBusForm.ui.h
