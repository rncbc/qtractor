// qtractorBusForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorBusForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorEngineCommand.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QMenu>


//----------------------------------------------------------------------
// class qtractorBusListItem -- Custom bus listview item.
//

class qtractorBusListItem : public QTreeWidgetItem
{
public:

	// Constructor.
	qtractorBusListItem(QTreeWidgetItem *pRootItem, qtractorBus *pBus)
		: QTreeWidgetItem(pRootItem), m_pBus(pBus)
	{
		switch (m_pBus->busType()) {
		case qtractorTrack::Audio:
			QTreeWidgetItem::setIcon(0, QIcon(":/icons/trackAudio.png"));
			break;
		case qtractorTrack::Midi:
			QTreeWidgetItem::setIcon(0, QIcon(":/icons/trackMidi.png"));
			break;
		case qtractorTrack::None:
		default:
			break;
		}
		QTreeWidgetItem::setText(0, m_pBus->busName());
	}

	// Bus accessors.
	qtractorBus *bus() const { return m_pBus; }

private:

	// Instance variables.
	qtractorBus *m_pBus;
};



//----------------------------------------------------------------------------
// qtractorBusForm -- UI wrapper form.

// Constructor.
qtractorBusForm::qtractorBusForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Initialize locals.
	m_pBus        = NULL;
	m_pAudioRoot  = NULL;
	m_pMidiRoot   = NULL;
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;
	m_iDirtyTotal = 0;

	QHeaderView *pHeader = m_ui.BusListView->header();
	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

	m_ui.BusListView->setContextMenuPolicy(Qt::CustomContextMenu);

	m_ui.BusTitleTextLabel->setPalette(QPalette(Qt::darkGray));
	m_ui.BusTitleTextLabel->setAutoFillBackground(true);
	
	// (Re)initial contents.
	refreshBuses();

	// Try to restore normal window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.BusListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(selectBus()));
	QObject::connect(m_ui.BusListView,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(contextMenu(const QPoint&)));
	QObject::connect(m_ui.BusNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.BusModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioChannelsSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioAutoConnectCheckBox,
		SIGNAL(clicked()),
		SLOT(changed()));
	QObject::connect(m_ui.MidiPassthruCheckBox,
		SIGNAL(clicked()),
		SLOT(changed()));
	QObject::connect(m_ui.RefreshPushButton,
		SIGNAL(clicked()),
		SLOT(refreshBuses()));
	QObject::connect(m_ui.CreatePushButton,
		SIGNAL(clicked()),
		SLOT(createBus()));
	QObject::connect(m_ui.UpdatePushButton,
		SIGNAL(clicked()),
		SLOT(updateBus()));
	QObject::connect(m_ui.DeletePushButton,
		SIGNAL(clicked()),
		SLOT(deleteBus()));
	QObject::connect(m_ui.ClosePushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qtractorBusForm::~qtractorBusForm (void)
{
}


// Set current bus.
void qtractorBusForm::setBus ( qtractorBus *pBus )
{
	// Get the device view root item...
	QTreeWidgetItem *pRootItem = NULL;
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
	int iChildCount = pRootItem->childCount();
	for (int i = 0; i < iChildCount; i++) {
		QTreeWidgetItem *pItem = pRootItem->child(i);
		// If identities match, select as current device item.
		qtractorBusListItem *pBusItem
			= static_cast<qtractorBusListItem *> (pItem);
		if (pBusItem && pBusItem->bus() == pBus) {
			m_ui.BusListView->setCurrentItem(pItem);
			break;
		}
	}
}


// Current bus accessor.
qtractorBus *qtractorBusForm::bus (void)
{
	return m_pBus;
}


// Current bus accessor.
bool qtractorBusForm::isDirty (void)
{
	return (m_iDirtyTotal > 0);
}


// Show current selected bus.
void qtractorBusForm::showBus ( qtractorBus *pBus )
{
	m_iDirtySetup++;

	// Settle current bus reference...
	m_pBus = pBus;

	// Show bus properties into view pane...
	if (pBus) {
		switch (pBus->busType()) {

		case qtractorTrack::Audio:
		{
			m_ui.BusTitleTextLabel->setText(tr("Audio Bus"));
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (pBus);
			if (pAudioBus) {
				m_ui.AudioChannelsSpinBox->setValue(
					pAudioBus->channels());
				m_ui.AudioAutoConnectCheckBox->setChecked(
					pAudioBus->isAutoConnect());
			}
			break;
		}

		case qtractorTrack::Midi:
		{
			m_ui.BusTitleTextLabel->setText(tr("MIDI Bus"));
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pBus);
			if (pMidiBus) {
				m_ui.MidiPassthruCheckBox->setChecked(
					pMidiBus->isPassthru());
			}
			break;
		}

		case qtractorTrack::None:
		default:
			m_ui.BusTitleTextLabel->setText(tr("Bus"));
			break;
		}

		m_ui.BusNameLineEdit->setText(pBus->busName());
		m_ui.BusModeComboBox->setCurrentIndex(int(pBus->busMode()) - 1);
	}

	// Reset dirty flag...
	m_iDirtyCount = 0;	
	m_iDirtySetup--;
	
	// Done.
	stabilizeForm();
}


// Refresh all buses list and views.
void qtractorBusForm::refreshBuses (void)
{
	//
	// (Re)Load complete bus listing ...
	//
	m_pAudioRoot = NULL;
	m_pMidiRoot  = NULL;
	m_ui.BusListView->clear();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	// Audio buses...
	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine) {
		m_pAudioRoot = new QTreeWidgetItem(m_ui.BusListView);
		m_pAudioRoot->setText(0, ' ' + tr("Audio"));
		m_pAudioRoot->setFlags(	// Audio root item is not selectable...
			m_pAudioRoot->flags() & ~Qt::ItemIsSelectable);
		for (qtractorBus *pBus = pAudioEngine->buses().first();
				pBus; pBus = pBus->next())
			new qtractorBusListItem(m_pAudioRoot, pBus);
#if QT_VERSION >= 0x040201
		m_pAudioRoot->setExpanded(true);
#else
		m_ui.BusListView->setItemExpanded(m_pAudioRoot, true);
#endif
	}

	// MIDI buses...
	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine) {
		m_pMidiRoot = new QTreeWidgetItem(m_ui.BusListView);
		m_pMidiRoot->setText(0, ' ' + tr("MIDI"));
		m_pMidiRoot->setFlags(	// MIDI root item is not selectable...
			m_pMidiRoot->flags() & ~Qt::ItemIsSelectable);
		for (qtractorBus *pBus = pMidiEngine->buses().first();
				pBus; pBus = pBus->next())
			new qtractorBusListItem(m_pMidiRoot, pBus);
#if QT_VERSION >= 0x040201
		m_pMidiRoot->setExpanded(true);
#else
		m_ui.BusListView->setItemExpanded(m_pMidiRoot, true);
#endif
	}

	// Reselect current bus, if any.
	setBus(m_pBus);
}


// Bus selection slot.
void qtractorBusForm::selectBus (void)
{
	// Get current selected item, must not be a root one...
	QTreeWidgetItem *pItem = m_ui.BusListView->currentItem();
	if (pItem == NULL)
		return;
	if (pItem->parent() == NULL)
		return;

	// Just make it in current view...
	qtractorBusListItem *pBusItem
		= static_cast<qtractorBusListItem *> (pItem);
	if (pBusItem == NULL)
		return;

	// Check if we need an update?...
	if (m_pBus && pBusItem->bus() != m_pBus && m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			tr("Apply"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Apply
			updateBus();
			break;
		case 1:     // Discard
			break;;
		default:    // Cancel.
			return;
		}
	}

	// Get new one into view...
	showBus(pBusItem->bus());
}


// Check whether the current view is elligible as a new bus.
bool qtractorBusForm::canCreateBus (void)
{
	if (m_iDirtyCount == 0)
		return false;
	if (m_pBus == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	const QString sBusName = m_ui.BusNameLineEdit->text().simplified();
	if (sBusName.isEmpty())
		return false;

	// Get the device view root item...
	qtractorEngine *pEngine = NULL;
	switch (m_pBus->busType()) {
	case qtractorTrack::Audio:
		pEngine = pSession->audioEngine();
		break;
	case qtractorTrack::Midi:
		pEngine = pSession->midiEngine();
		break;
	default:
		break;
	}
	// Is it still valid?
	if (pEngine == NULL)
		return false;

	// Is there one already?
	return (pEngine->findBus(sBusName) == NULL);
}


// Check whether the current view is elligible for update.
bool qtractorBusForm::canUpdateBus (void)
{
	if (m_iDirtyCount == 0)
		return false;
	if (m_pBus == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	const QString sBusName = m_ui.BusNameLineEdit->text().simplified();
	return (!sBusName.isEmpty());
}


// Check whether the current view is elligible for deletion.
bool qtractorBusForm::canDeleteBus (void)
{
	if (m_pBus == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// The very first bus is never deletable...
	return (m_pBus->prev() != NULL);
}


// Create a new bus from current view.
void qtractorBusForm::createBus (void)
{
	if (m_pBus == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	
	const QString sBusName = m_ui.BusNameLineEdit->text().simplified();
	if (sBusName.isEmpty())
		return;

	qtractorBus::BusMode busMode = qtractorBus::None;
	switch (m_ui.BusModeComboBox->currentIndex()) {
	case 0:
		busMode = qtractorBus::Input;
		break;
	case 1:
		busMode = qtractorBus::Output;
		break;
	case 2:
		busMode = qtractorBus::Duplex;
		break;
	}

	// Make it as an unduable command...
	qtractorCreateBusCommand *pCreateBusCommand
		= new qtractorCreateBusCommand();

	// Set all creational properties...
	qtractorTrack::TrackType busType = m_pBus->busType();
	pCreateBusCommand->setBusType(busType);
	pCreateBusCommand->setBusName(sBusName);
	pCreateBusCommand->setBusMode(busMode);	
	// Specialties for bus types...
	switch (busType) {
	case qtractorTrack::Audio:
		pCreateBusCommand->setChannels(
			m_ui.AudioChannelsSpinBox->value());
		pCreateBusCommand->setAutoConnect(
			m_ui.AudioAutoConnectCheckBox->isChecked());
		break;
	case qtractorTrack::Midi:
		pCreateBusCommand->setPassthru(
			m_ui.MidiPassthruCheckBox->isChecked());
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Execute and refresh form...
	if (pMainForm->commands()->exec(pCreateBusCommand)) {
		m_iDirtyTotal++;
		refreshBuses();
	}
}


// Update current bus in view.
void qtractorBusForm::updateBus (void)
{
	if (m_pBus == NULL)
		return;

	if (m_pBus == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	
	const QString sBusName = m_ui.BusNameLineEdit->text().simplified();
	if (sBusName.isEmpty())
		return;

	qtractorBus::BusMode busMode = qtractorBus::None;
	switch (m_ui.BusModeComboBox->currentIndex()) {
	case 0:
		busMode = qtractorBus::Input;
		break;
	case 1:
		busMode = qtractorBus::Output;
		break;
	case 2:
		busMode = qtractorBus::Duplex;
		break;
	}

	// Make it as an unduable command...
	qtractorUpdateBusCommand *pUpdateBusCommand
		= new qtractorUpdateBusCommand(m_pBus);

	// Set all updated properties...
	qtractorTrack::TrackType busType = m_pBus->busType();
	pUpdateBusCommand->setBusType(busType);
	pUpdateBusCommand->setBusName(sBusName);
	pUpdateBusCommand->setBusMode(busMode);	
	// Specialties for bus types...
	switch (busType) {
	case qtractorTrack::Audio:
		pUpdateBusCommand->setChannels(
			m_ui.AudioChannelsSpinBox->value());
		pUpdateBusCommand->setAutoConnect(
			m_ui.AudioAutoConnectCheckBox->isChecked());
		break;
	case qtractorTrack::Midi:
		pUpdateBusCommand->setPassthru(
			m_ui.MidiPassthruCheckBox->isChecked());
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Execute and refresh form...
	if (pMainForm->commands()->exec(pUpdateBusCommand)) {
		m_iDirtyTotal++;
		refreshBuses();
	}
}


// Delete current bus in view.
void qtractorBusForm::deleteBus (void)
{
	if (m_pBus == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = pMainForm->options();
	if (pOptions && pOptions->bConfirmRemove) {
		// Get some textual type...
		QString sBusType;
		switch (m_pBus->busType()) {
		case qtractorTrack::Audio:
			sBusType = tr("Audio");
			break;
		case qtractorTrack::Midi:
			sBusType = tr("MIDI");
			break;
		default:
			break;
		}
		// Show the warning...
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to remove bus:\n\n"
			"\"%1\" (%2)\n\n"
			"Are you sure?")
			.arg(m_pBus->busName())
			.arg(sBusType),
			tr("OK"), tr("Cancel")) > 0)
			return;
	}

	// Make it as an unduable command...
	qtractorDeleteBusCommand *pDeleteBusCommand
		= new qtractorDeleteBusCommand(m_pBus);

	// Execute and refresh form...
	if (pMainForm->commands()->exec(pDeleteBusCommand)) {
		m_iDirtyTotal++;
		refreshBuses();
	}
}


// Make changes due.
void qtractorBusForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	m_iDirtyCount++;
	stabilizeForm();
}


// Reject settings (Close button slot).
void qtractorBusForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to discard the changes?"),
			tr("Discard"), tr("Cancel"))) {
		case 0:     // Discard
			break;
		default:    // Cancel.
			bReject = false;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Stabilize current form state.
void qtractorBusForm::stabilizeForm (void)
{
	if (m_pBus) {
		m_ui.CommonBusGroup->setEnabled(true);
		m_ui.AudioBusGroup->setVisible(m_pBus->busType() == qtractorTrack::Audio);
		m_ui.MidiBusGroup->setVisible(m_pBus->busType() == qtractorTrack::Midi);
	} else {
		m_ui.CommonBusGroup->setEnabled(false);
		m_ui.AudioBusGroup->setVisible(false);
		m_ui.MidiBusGroup->setVisible(false);
	}

	if (m_pBus && m_pBus->busType() == qtractorTrack::Midi)
		m_ui.MidiPassthruCheckBox->setEnabled(
			m_ui.BusModeComboBox->currentIndex() == 2);

	m_ui.RefreshPushButton->setEnabled(m_iDirtyCount > 0);
	m_ui.CreatePushButton->setEnabled(canCreateBus());
	m_ui.UpdatePushButton->setEnabled(canUpdateBus());
	m_ui.DeletePushButton->setEnabled(canDeleteBus());
}


// Bus list view context menu handler.
void qtractorBusForm::contextMenu ( const QPoint& /*pos*/ )
{
	// Build the device context menu...
	QMenu menu(this);
	QAction *pAction;
	
	pAction = menu.addAction(
		QIcon(":/icons/formCreate.png"),
		tr("&Create"), this, SLOT(createBus()));
	pAction->setEnabled(canCreateBus());

	pAction = menu.addAction(
		QIcon(":/icons/formAccept.png"),
		tr("&Update"), this, SLOT(updateBus()));
	pAction->setEnabled(canUpdateBus());

	pAction = menu.addAction(
		QIcon(":/icons/formRemove.png"),
		tr("&Remove"), this, SLOT(deleteBus()));
	pAction->setEnabled(canDeleteBus());

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/icons/formRefresh.png"),
		tr("&Refresh"), this, SLOT(refreshBuses()));
	pAction->setEnabled(m_iDirtyCount > 0);

//	menu.exec(m_ui.BusListView->mapToGlobal(pos));
	menu.exec(QCursor::pos());
}


// end of qtractorBusForm.cpp
