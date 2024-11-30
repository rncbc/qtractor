// qtractorBusForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiManager.h"

#include "qtractorSession.h"

#include "qtractorMidiSysexForm.h"
#include "qtractorMidiSysex.h"

#include "qtractorInstrument.h"
#include "qtractorPlugin.h"

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
	qtractorBusListItem(qtractorBus *pBus)
		: QTreeWidgetItem(), m_pBus(pBus)
	{
		QTreeWidgetItem::setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

		switch (m_pBus->busType()) {
		case qtractorTrack::Audio:
			QTreeWidgetItem::setIcon(0, QIcon::fromTheme("trackAudio"));
			QTreeWidgetItem::setText(1, QString::number(
				static_cast<qtractorAudioBus *> (m_pBus)->channels()));
			break;
		case qtractorTrack::Midi:
			QTreeWidgetItem::setIcon(0, QIcon::fromTheme("trackMidi"));
			QTreeWidgetItem::setText(1, QString::number(16));
			break;
		case qtractorTrack::None:
		default:
			break;
		}
		QTreeWidgetItem::setText(0, m_pBus->busName());
		switch (m_pBus->busMode()) {
		case qtractorBus::Duplex:
			QTreeWidgetItem::setText(2, QObject::tr("Duplex"));
			break;
		case qtractorBus::Output:
			QTreeWidgetItem::setText(2, QObject::tr("Output"));
			break;
		case qtractorBus::Input:
			QTreeWidgetItem::setText(2, QObject::tr("Input"));
			break;
		case qtractorBus::None:
		default:
			QTreeWidgetItem::setText(2, QObject::tr("None"));
			break;
		}
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
qtractorBusForm::qtractorBusForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Initialize locals.
	m_pBus        = nullptr;

	m_pAudioRoot  = nullptr;
	m_pMidiRoot   = nullptr;

	m_iDirtySetup = 0;
	m_iDirtyCount = 0;
	m_iDirtyTotal = 0;

	QHeaderView *pHeader = m_ui.BusListView->header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->resizeSection(0, 140);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	pHeader->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(1, QHeaderView::ResizeToContents);
 	pHeader->setResizeMode(2, QHeaderView::ResizeToContents);
	pHeader->setMovable(false);
#endif

	m_ui.BusListView->setContextMenuPolicy(Qt::CustomContextMenu);

	const QColor& rgbDark = palette().dark().color().darker(150);
	m_ui.BusTitleTextLabel->setPalette(QPalette(rgbDark));
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
	QObject::connect(m_ui.MonitorCheckBox,
		SIGNAL(clicked()),
		SLOT(changed()));
	QObject::connect(m_ui.AudioChannelsSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioAutoConnectCheckBox,
		SIGNAL(clicked()),
		SLOT(changed()));
	QObject::connect(m_ui.MidiInstrumentComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiSysexPushButton,
		SIGNAL(clicked()),
		SLOT(midiSysex()));

	QObject::connect(m_ui.InputPluginListView,
		SIGNAL(currentRowChanged(int)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.InputPluginListView,
		SIGNAL(contentsChanged()),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.AddInputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(addInputPlugin()));
	QObject::connect(m_ui.RemoveInputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(removeInputPlugin()));
	QObject::connect(m_ui.MoveUpInputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(moveUpInputPlugin()));
	QObject::connect(m_ui.MoveDownInputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(moveDownInputPlugin()));

	QObject::connect(m_ui.OutputPluginListView,
		SIGNAL(currentRowChanged(int)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.OutputPluginListView,
		SIGNAL(contentsChanged()),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.AddOutputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(addOutputPlugin()));
	QObject::connect(m_ui.RemoveOutputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(removeOutputPlugin()));
	QObject::connect(m_ui.MoveUpOutputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(moveUpOutputPlugin()));
	QObject::connect(m_ui.MoveDownOutputPluginToolButton,
		SIGNAL(clicked()),
		SLOT(moveDownOutputPlugin()));

	QObject::connect(m_ui.MoveUpPushButton,
		SIGNAL(clicked()),
		SLOT(moveUpBus()));
	QObject::connect(m_ui.MoveDownPushButton,
		SIGNAL(clicked()),
		SLOT(moveDownBus()));
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

	stabilizeForm();
}


// Set current bus.
void qtractorBusForm::setBus ( qtractorBus *pBus )
{
	if (pBus == nullptr)
		return;

	// Get the device view root item...
	QTreeWidgetItem *pRootItem = nullptr;
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
	if (pRootItem == nullptr) {
		stabilizeForm();
		return;
	}

	// For each child, test for identity...
	const int iChildCount = pRootItem->childCount();
	for (int i = 0; i < iChildCount; ++i) {
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
	++m_iDirtySetup;

	// Reset plugin lists...
	resetPluginLists();

	// Settle current bus reference...
	m_pBus = pBus;

	// Update some dependable specifics...
	updateMidiInstruments();
	updateMidiSysex();

	// Show bus properties into view pane...
	if (pBus) {
		QString sBusTitle = pBus->busName();
		if (!sBusTitle.isEmpty())
			sBusTitle += " - ";
		switch (pBus->busType()) {
		case qtractorTrack::Audio:
		{
			sBusTitle += tr("Audio");
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (pBus);
			if (pAudioBus) {
				// Audio bus specifics...
				m_ui.AudioChannelsSpinBox->setValue(
					pAudioBus->channels());
				m_ui.AudioAutoConnectCheckBox->setChecked(
					pAudioBus->isAutoConnect());
				// Set plugin lists...
				if (pAudioBus->busMode() & qtractorBus::Input)
					m_ui.InputPluginListView->setPluginList(
						pAudioBus->pluginList_in());
				if (pAudioBus->busMode() & qtractorBus::Output)
					m_ui.OutputPluginListView->setPluginList(
						pAudioBus->pluginList_out());
			}
			break;
		}
		case qtractorTrack::Midi:
		{
			sBusTitle += tr("MIDI");
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pBus);
			if (pMidiBus) {
				// MIDI bus specifics...
				const int iInstrumentIndex
					= m_ui.MidiInstrumentComboBox->findText(
						pMidiBus->instrumentName());
				m_ui.MidiInstrumentComboBox->setCurrentIndex(
					iInstrumentIndex > 0 ? iInstrumentIndex : 0);
				// Set plugin lists...
				if (pMidiBus->busMode() & qtractorBus::Input)
					m_ui.InputPluginListView->setPluginList(
						pMidiBus->pluginList_in());
				if (pMidiBus->busMode() & qtractorBus::Output)
					m_ui.OutputPluginListView->setPluginList(
						pMidiBus->pluginList_out());
			}
			break;
		}
		case qtractorTrack::None:
		default:
			break;
		}
		if (!sBusTitle.isEmpty())
			sBusTitle += ' ';
		m_ui.BusTitleTextLabel->setText(sBusTitle + tr("Bus"));
		m_ui.BusNameLineEdit->setText(pBus->busName());
		m_ui.BusModeComboBox->setCurrentIndex(int(pBus->busMode()) - 1);
		m_ui.MonitorCheckBox->setChecked(pBus->isMonitor());
	}

	// Reset dirty flag...
	m_iDirtyCount = 0;	
	--m_iDirtySetup;

	// Done.
	stabilizeForm();
}


// Refresh all buses list and views.
void qtractorBusForm::refreshBuses (void)
{
	//
	// (Re)Load complete bus listing ...
	//
	m_pAudioRoot = nullptr;
	m_pMidiRoot  = nullptr;

	m_ui.BusListView->clear();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Audio buses...
	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine) {
		m_pAudioRoot = new QTreeWidgetItem();
		m_pAudioRoot->setText(0, ' ' + tr("Audio"));
		m_pAudioRoot->setFlags(Qt::ItemIsEnabled); // but not selectable...
		QListIterator<qtractorBus *> iter(pAudioEngine->buses2());
		while (iter.hasNext())
			m_pAudioRoot->addChild(new qtractorBusListItem(iter.next()));
		m_ui.BusListView->addTopLevelItem(m_pAudioRoot);
		m_pAudioRoot->setExpanded(true);
	}

	// MIDI buses...
	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine) {
		m_pMidiRoot = new QTreeWidgetItem();
		m_pMidiRoot->setText(0, ' ' + tr("MIDI"));
		m_pMidiRoot->setFlags(Qt::ItemIsEnabled); // but not selectable...
		QListIterator<qtractorBus *> iter(pMidiEngine->buses2());
		while (iter.hasNext())
			m_pMidiRoot->addChild(new qtractorBusListItem(iter.next()));
		m_ui.BusListView->addTopLevelItem(m_pMidiRoot);
		m_pMidiRoot->setExpanded(true);
	}
}


// Bus selection slot.
void qtractorBusForm::selectBus (void)
{
	if (m_iDirtySetup > 0)
		return;

	// Get current selected item, must not be a root one...
	QTreeWidgetItem *pItem = m_ui.BusListView->currentItem();
	if (pItem == nullptr)
		return;
	if (pItem->parent() == nullptr)
		return;

	// Just make it in current view...
	qtractorBusListItem *pBusItem
		= static_cast<qtractorBusListItem *> (pItem);
	if (pBusItem == nullptr)
		return;

	// Check if we need an update?...
	bool bUpdate = false;
	qtractorBus *pBus = pBusItem->bus();
	if (m_pBus && m_pBus != pBus && m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.UpdatePushButton->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning"),
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Apply:
			bUpdate = updateBus(m_pBus);
			// Fall thru...
		case QMessageBox::Discard:
			break;
		default: // Cancel.
			return;
		}
	}

	// Get new one into view...
	showBus(pBus);

	// Reselect as current (only on apply/update)
	if (bUpdate) {
		++m_iDirtyTotal;
		refreshBuses();
		setBus(m_pBus);
	}
}


// Check whether the current view is elligible for action.
unsigned int qtractorBusForm::flags (void) const
{
	unsigned int iFlags = 0;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return iFlags;

	if (m_pBus == nullptr)
		return iFlags;

	qtractorEngine *pEngine = m_pBus->engine();
	if (pEngine == nullptr)
		return iFlags;

	const int iBus2
		= pEngine->buses2().indexOf(m_pBus);
	if (iBus2 < 0)
		return iFlags;

	const int iNumBuses2
		= pEngine->buses2().count();
	if (iBus2 > 0) {
		iFlags |= Delete;
		if (iBus2 > 1)
			iFlags |= MoveUp;
		if (iBus2 < iNumBuses2 - 1)
			iFlags |= MoveDown;
	}
	
	if (m_iDirtyCount == 0)
		return iFlags;

	const QString sBusName
		= m_ui.BusNameLineEdit->text().simplified();
	if (sBusName.isEmpty())
		return iFlags;

	// Is there one already?
	qtractorBus *pBus = pEngine->findBus(sBusName);
	qtractorBus *pBusEx = pEngine->findBusEx(sBusName);
	if (pBus == nullptr && pBusEx == nullptr)
		iFlags |= Create;
	if ((pBus == nullptr  || pBus == m_pBus) && (pBusEx == nullptr)
		&& (iBus2 > 0 || m_ui.BusModeComboBox->currentIndex() == 2))
		iFlags |= Update;

	return iFlags;
}


// Update bus method.
bool qtractorBusForm::updateBus ( qtractorBus *pBus )
{
	if (pBus == nullptr)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	const QString sBusName = m_ui.BusNameLineEdit->text().simplified();
	if (sBusName.isEmpty())
		return false;

	// Reset plugin lists...
	resetPluginLists();

	const qtractorBus::BusMode busMode
		= qtractorBus::BusMode(m_ui.BusModeComboBox->currentIndex() + 1);

	// Make it as an unduable command...
	qtractorUpdateBusCommand *pUpdateBusCommand
		= new qtractorUpdateBusCommand(pBus);

	// Set all updated properties...
	qtractorTrack::TrackType busType = pBus->busType();
	pUpdateBusCommand->setBusType(busType);
	pUpdateBusCommand->setBusName(sBusName);
	pUpdateBusCommand->setBusMode(busMode);
	pUpdateBusCommand->setMonitor(
		((busMode & qtractorBus::Duplex) == qtractorBus::Duplex)
		&& m_ui.MonitorCheckBox->isChecked());

	// Specialties for bus types...
	switch (busType) {
	case qtractorTrack::Audio:
		pUpdateBusCommand->setChannels(
			m_ui.AudioChannelsSpinBox->value());
		pUpdateBusCommand->setAutoConnect(
			m_ui.AudioAutoConnectCheckBox->isChecked());
		break;
	case qtractorTrack::Midi:
		pUpdateBusCommand->setInstrumentName(
			m_ui.MidiInstrumentComboBox->currentIndex() > 0
			? m_ui.MidiInstrumentComboBox->currentText()
			: QString());
		// Fall thru...
	case qtractorTrack::None:
	default:
		break;
	}

	// Execute and refresh form...
	return pSession->execute(pUpdateBusCommand);
}


// Move current bus up towards the list top.
void qtractorBusForm::moveUpBus (void)
{
	if (m_pBus == nullptr)
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession &&
		pSession->execute(new qtractorMoveBusCommand(m_pBus, -1))) {
		++m_iDirtyTotal;
		refreshBuses();
	}

	// Reselect current bus...
	setBus(m_pBus);
}


// Move current bus down towards the list bottom.
void qtractorBusForm::moveDownBus (void)
{
	if (m_pBus == nullptr)
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession &&
		pSession->execute(new qtractorMoveBusCommand(m_pBus, +1))) {
		++m_iDirtyTotal;
		refreshBuses();
	}

	// Reselect current bus...
	setBus(m_pBus);
}


// Create a new bus from current view.
void qtractorBusForm::createBus (void)
{
	if (m_pBus == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;
	
	const QString sBusName = m_ui.BusNameLineEdit->text().simplified();
	if (sBusName.isEmpty())
		return;

	const qtractorBus::BusMode busMode
		= qtractorBus::BusMode(m_ui.BusModeComboBox->currentIndex() + 1);

	// Make it as an unduable command...
	qtractorCreateBusCommand *pCreateBusCommand
		= new qtractorCreateBusCommand(m_pBus);

	// Set all creational properties...
	qtractorTrack::TrackType busType = m_pBus->busType();
	pCreateBusCommand->setBusType(busType);
	pCreateBusCommand->setBusName(sBusName);
	pCreateBusCommand->setBusMode(busMode);	
	pCreateBusCommand->setMonitor(
		(busMode & qtractorBus::Duplex) == qtractorBus::Duplex
		&& m_ui.MonitorCheckBox->isChecked());

	// Specialties for bus types...
	switch (busType) {
	case qtractorTrack::Audio:
		pCreateBusCommand->setChannels(
			m_ui.AudioChannelsSpinBox->value());
		pCreateBusCommand->setAutoConnect(
			m_ui.AudioAutoConnectCheckBox->isChecked());
		break;
	case qtractorTrack::Midi:
		pCreateBusCommand->setInstrumentName(
			m_ui.MidiInstrumentComboBox->currentIndex() > 0
			? m_ui.MidiInstrumentComboBox->currentText()
			: QString());
		// Fall thru...
	case qtractorTrack::None:
	default:
		break;
	}

	// Execute and refresh form...
	if (pSession->execute(pCreateBusCommand)) {
		++m_iDirtyTotal;
		refreshBuses();
	}

	// Get new one into view,
	// usually created after the cuurent one...
	showBus(m_pBus->next());

	// Select the new bus...
	setBus(m_pBus);
}


// Update current bus in view.
void qtractorBusForm::updateBus (void)
{
	// That's it...
	if (updateBus(m_pBus)) {
		++m_iDirtyTotal;
		refreshBuses();
	}

	// Reselect current bus...
	setBus(m_pBus);
}


// Delete current bus in view.
void qtractorBusForm::deleteBus (void)
{
	if (m_pBus == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
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
			tr("Warning"),
			tr("About to remove bus:\n\n"
			"\"%1\" (%2)\n\n"
			"Are you sure?")
			.arg(m_pBus->busName())
			.arg(sBusType),
			QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
			return;
	}

	// Reset plugin lists...
	resetPluginLists();

	// Make it as an unduable command...
	qtractorDeleteBusCommand *pDeleteBusCommand
		= new qtractorDeleteBusCommand(m_pBus);

	// Invalidade current bus...
	m_pBus = nullptr;

	// Execute and refresh form...
	if (pSession->execute(pDeleteBusCommand)) {
		++m_iDirtyTotal;
		refreshBuses();
	}

	// Done.
	stabilizeForm();
}


// Reset (stabilize) plugin lists...
void qtractorBusForm::resetPluginLists (void)
{
	m_ui.InputPluginListView->setPluginList(nullptr);
	m_ui.OutputPluginListView->setPluginList(nullptr);
}


// Make changes due.
void qtractorBusForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	++m_iDirtyCount;
	stabilizeForm();
}


// Reject settings (Close button slot).
void qtractorBusForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning"),
			tr("Some settings have been changed.\n\n"
			"Do you want to discard the changes?"),
			QMessageBox::Discard | QMessageBox::Cancel)) {
		case QMessageBox::Discard:
			break;
		default:    // Cancel.
			bReject = false;
			break;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Stabilize current form state.
void qtractorBusForm::stabilizeForm (void)
{
	const qtractorBus::BusMode busMode
		= qtractorBus::BusMode(m_ui.BusModeComboBox->currentIndex() + 1);

	bool bEnabled = (m_pBus != nullptr);
	m_ui.BusTabWidget->setEnabled(bEnabled);
	m_ui.CommonBusGroup->setEnabled(bEnabled);
	if (m_pBus && m_pBus->busType() == qtractorTrack::Audio) {
		m_ui.AudioBusGroup->setEnabled(true);
		m_ui.AudioBusGroup->setVisible(true);
		m_ui.MidiBusGroup->setEnabled(false);
		m_ui.MidiBusGroup->setVisible(false);
	}
	else
	if (m_pBus && m_pBus->busType() == qtractorTrack::Midi) {
		m_ui.AudioBusGroup->setEnabled(false);
		m_ui.AudioBusGroup->setVisible(false);
		m_ui.MidiBusGroup->setEnabled(true);
		m_ui.MidiBusGroup->setVisible(true);
		bEnabled = (busMode & qtractorBus::Output);
		m_ui.MidiInstrumentComboBox->setEnabled(bEnabled);
		m_ui.MidiSysexPushButton->setEnabled(bEnabled);
		m_ui.MidiSysexTextLabel->setEnabled(bEnabled);
	} else {
		m_ui.AudioBusGroup->setEnabled(false);
		m_ui.AudioBusGroup->setVisible(true);
		m_ui.MidiBusGroup->setEnabled(false);
		m_ui.MidiBusGroup->setVisible(false);
	}

	m_ui.MonitorCheckBox->setEnabled(busMode == qtractorBus::Duplex);

	const unsigned int iFlags = flags();
	m_ui.MoveUpPushButton->setEnabled(iFlags & MoveUp);
	m_ui.MoveDownPushButton->setEnabled(iFlags & MoveDown);
	m_ui.CreatePushButton->setEnabled(iFlags & Create);
	m_ui.UpdatePushButton->setEnabled(iFlags & Update);
	m_ui.DeletePushButton->setEnabled(iFlags & Delete);

	// Stabilize current plugin lists state.
	int iItem, iItemCount;
	qtractorPlugin *pPlugin = nullptr;
	qtractorPluginListItem *pItem = nullptr;

	// Input plugin list...
	bEnabled = (busMode & qtractorBus::Input)
		&& (m_pBus && (m_pBus->busMode() & qtractorBus::Input))
		&& (m_ui.InputPluginListView->pluginList() != nullptr);
	m_ui.BusTabWidget->setTabEnabled(1, bEnabled);
	if (bEnabled) {
		iItemCount = m_ui.InputPluginListView->count();
		iItem = -1;
		pPlugin = nullptr;
		pItem = static_cast<qtractorPluginListItem *> (
			m_ui.InputPluginListView->currentItem());
		if (pItem) {
			iItem = m_ui.InputPluginListView->row(pItem);
			pPlugin = pItem->plugin();
		}
	//	m_ui.AddInputPluginToolButton->setEnabled(true);
		m_ui.RemoveInputPluginToolButton->setEnabled(pPlugin != nullptr);
		m_ui.MoveUpInputPluginToolButton->setEnabled(pItem && iItem > 0);
		m_ui.MoveDownInputPluginToolButton->setEnabled(
			pItem && iItem < iItemCount - 1);
	}

	// Output plugin list...
	bEnabled = (busMode & qtractorBus::Output)
		&& (m_pBus && (m_pBus->busMode() & qtractorBus::Output))
		&& (m_ui.OutputPluginListView->pluginList() != nullptr);
	m_ui.BusTabWidget->setTabEnabled(2, bEnabled);
	if (bEnabled) {
		iItemCount = m_ui.OutputPluginListView->count();
		iItem = -1;
		pPlugin = nullptr;
		pItem = static_cast<qtractorPluginListItem *> (
			m_ui.OutputPluginListView->currentItem());
		if (pItem) {
			iItem = m_ui.OutputPluginListView->row(pItem);
			pPlugin = pItem->plugin();
		}
	//	m_ui.AddOutputPluginToolButton->setEnabled(true);
		m_ui.RemoveOutputPluginToolButton->setEnabled(pPlugin != nullptr);
		m_ui.MoveUpOutputPluginToolButton->setEnabled(pItem && iItem > 0);
		m_ui.MoveDownOutputPluginToolButton->setEnabled(
			pItem && iItem < iItemCount - 1);
	}
}


// Bus list view context menu handler.
void qtractorBusForm::contextMenu ( const QPoint& /*pos*/ )
{
	// Build the device context menu...
	QMenu menu(this);
	QAction *pAction;

	const unsigned int iFlags = flags();

	pAction = menu.addAction(
		QIcon::fromTheme("formCreate"),
		tr("&Create"), this, SLOT(createBus()));
	pAction->setEnabled(iFlags & Create);

	pAction = menu.addAction(
		QIcon::fromTheme("formAccept"),
		tr("&Update"), this, SLOT(updateBus()));
	pAction->setEnabled(iFlags & Update);

	pAction = menu.addAction(
		QIcon::fromTheme("formRemove"),
		tr("&Delete"), this, SLOT(deleteBus()));
	pAction->setEnabled(iFlags & Delete);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon::fromTheme("formMoveUp"),
		tr("Move &Up"), this, SLOT(moveUpBus()));
	pAction->setEnabled(iFlags & MoveUp);

	pAction = menu.addAction(
		QIcon::fromTheme("formMoveDown"),
		tr("Move &Down"), this, SLOT(moveDownBus()));
	pAction->setEnabled(iFlags & MoveDown);

//	menu.exec(m_ui.BusListView->mapToGlobal(pos));
	menu.exec(QCursor::pos());
}


// Show MIDI SysEx bank manager dialog...
void qtractorBusForm::midiSysex (void)
{
	// Care of MIDI output bus...
	if (m_pBus == nullptr)
		return;
	if (m_pBus->busType() != qtractorTrack::Midi)
		return;
	if ((m_pBus->busMode() & qtractorBus::Output) == 0)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pBus);
	if (pMidiBus == nullptr)
		return;
	if (pMidiBus->sysexList() == nullptr)
		return;

	qtractorMidiSysexForm form(this);
	form.setSysexList(pMidiBus->sysexList());
	form.exec();

	updateMidiSysex();
}


// Refresh instrument list.
void qtractorBusForm::updateMidiInstruments (void)
{
	m_ui.MidiInstrumentComboBox->clear();
	m_ui.MidiInstrumentComboBox->addItem(tr("(No instrument)"));

	// Care of MIDI output bus...
	if (m_pBus == nullptr)
		return;
	if (m_pBus->busType() != qtractorTrack::Midi)
		return;
	if ((m_pBus->busMode() & qtractorBus::Output) == 0)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pBus);
	if (pMidiBus == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == nullptr)
		return;

	// Avoid superfluous change notifications...
	++m_iDirtySetup;

	const QIcon& icon = QIcon::fromTheme("itemInstrument");
	if (pMidiBus->pluginList_out()) {
		qtractorMidiManager *pMidiManager
			= (pMidiBus->pluginList_out())->midiManager();
		if (pMidiManager) {
			pMidiManager->updateInstruments();
			const qtractorInstrumentList& instruments
				= pMidiManager->instruments();
			qtractorInstrumentList::ConstIterator iter = instruments.constBegin();
			const qtractorInstrumentList::ConstIterator& iter_end = instruments.constEnd();
			for ( ; iter != iter_end; ++iter)
				m_ui.MidiInstrumentComboBox->addItem(icon, iter.value().instrumentName());
		}
	}

	// Regular instrument names...
	qtractorInstrumentList::ConstIterator iter = pInstruments->constBegin();
	const qtractorInstrumentList::ConstIterator& iter_end = pInstruments->constEnd();
	for ( ; iter != iter_end; ++iter)
		m_ui.MidiInstrumentComboBox->addItem(icon, iter.value().instrumentName());

	// Done.
	--m_iDirtySetup;
}


// Update SysEx status.
void qtractorBusForm::updateMidiSysex (void)
{
	m_ui.MidiSysexTextLabel->clear();

	// Care of MIDI output bus...
	if (m_pBus == nullptr)
		return;
	if (m_pBus->busType() != qtractorTrack::Midi)
		return;
	if ((m_pBus->busMode() & qtractorBus::Output) == 0)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pBus);
	if (pMidiBus == nullptr)
		return;
	if (pMidiBus->sysexList() == nullptr)
		return;

	// Show proper count status...
	const int iSysexCount = (pMidiBus->sysexList())->count();
	switch (iSysexCount) {
	case 0:
		m_ui.MidiSysexTextLabel->setText(tr("(none)"));
		break;
	case 1:
		m_ui.MidiSysexTextLabel->setText(tr("(1 item)"));
		break;
	default:
		m_ui.MidiSysexTextLabel->setText(tr("(%1 items)").arg(iSysexCount));
		break;
	}
}


// Input plugin list slots.
void qtractorBusForm::addInputPlugin (void)
{
	m_ui.InputPluginListView->addPlugin();
}

void qtractorBusForm::removeInputPlugin (void)
{
	m_ui.InputPluginListView->removePlugin();
}

void qtractorBusForm::moveUpInputPlugin (void)
{
	m_ui.InputPluginListView->moveUpPlugin();
}

void qtractorBusForm::moveDownInputPlugin (void)
{
	m_ui.InputPluginListView->moveDownPlugin();
}


// Output plugin list slots.
void qtractorBusForm::addOutputPlugin (void)
{
	m_ui.OutputPluginListView->addPlugin();
}

void qtractorBusForm::removeOutputPlugin (void)
{
	m_ui.OutputPluginListView->removePlugin();
}

void qtractorBusForm::moveUpOutputPlugin (void)
{
	m_ui.OutputPluginListView->moveUpPlugin();
}

void qtractorBusForm::moveDownOutputPlugin (void)
{
	m_ui.OutputPluginListView->moveDownPlugin();
}


// end of qtractorBusForm.cpp
