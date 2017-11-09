// qtractorMidiListView.cpp
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiImportForm.h"
#include "qtractorSession.h"
#include "qtractorMidiImportExtender.h"
#include "qtractorMainForm.h"
#include "qtractorPlugin.h"
#include "qtractorMidiManager.h"
#include "qtractorFiles.h"
#include "qtractorCommand.h"
#include "qtractorInstrument.h"
#include "QPushButton"


//----------------------------------------------------------------------
// class qtractorMidiImportForm -- UI wrapper form.
//


// Constructor.
qtractorMidiImportForm::qtractorMidiImportForm(qtractorMidiImportExtender *pMidiImportExtender,
		QWidget *pParent, Qt::WindowFlags wflags )
		: QDialog(pParent, wflags),
		m_pMidiImportExtender(pMidiImportExtender), m_iDirtySetup(0)
{

	// Get global session object.
	qtractorSession *pSession = qtractorSession::getInstance();
	if (!pSession)
		return;

	// Get plugin list to display.
	qtractorPluginList *pPluginList = m_pMidiImportExtender->pluginListForGui();
	if (!pPluginList)
		return;

	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Get reference of the last command before dialog.
	m_pLastCommand = NULL;
	qtractorCommandList *pCommands = pSession->commands();
	m_pLastCommand = pCommands->lastCommand();

	// Set plugin list...
	m_ui.PluginListView->setPluginList(pPluginList);
	m_ui.PluginListView->refresh();

	// Fill Instrument / bank comboboxes.
	updateInstruments();

	// Get a pointer to settings.
	qtractorMidiImportExtender::exportExtensionsData *pExtendedSettings =
			m_pMidiImportExtender->exportExtensions();
	m_ui.InstInstComboBox->setCurrentText(
				pExtendedSettings->sMidiImportInstInst);
	m_ui.DrumInstComboBox->setCurrentText(
				pExtendedSettings->sMidiImportDrumInst);
	updateBanks(pExtendedSettings->sMidiImportInstInst,
				pExtendedSettings->iMidiImportInstBank, Instruments);
	updateBanks(pExtendedSettings->sMidiImportDrumInst,
				pExtendedSettings->iMidiImportDrumBank, Drums);

	// Select track-name radio option
	m_groupNameRadios = new QButtonGroup(this);
	m_groupNameRadios->addButton(m_ui.NameMidiRadioButton,
								 (int) qtractorMidiImportExtender::Midifile);
	m_groupNameRadios->addButton(m_ui.NameTrackRadioButton,
								 (int) qtractorMidiImportExtender::Track);
	m_groupNameRadios->addButton(m_ui.NamePatchRadioButton,
								 (int) qtractorMidiImportExtender::PatchName);
	QList<QAbstractButton *> radios = m_groupNameRadios->buttons();
	for (int iRadio = 0; iRadio < radios.count(); ++iRadio) {
		if (m_groupNameRadios->id(radios[iRadio]) ==
				(int)pExtendedSettings->eMidiImportTrackNameType) {
			radios[iRadio]->setChecked(true);
			break;
		}
	}

	// UI signal/slot connections...
	QObject::connect(m_ui.PluginListView,
		SIGNAL(currentRowChanged(int)),
		SLOT(stabilizeListButtons()));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(contentsChanged()),
		SLOT(pluginListChanged()));
	QObject::connect(m_ui.AddPluginToolButton,
		SIGNAL(clicked()),
		SLOT(addPlugin()));
	QObject::connect(m_ui.RemovePluginToolButton,
		SIGNAL(clicked()),
		SLOT(removePlugin()));
	QObject::connect(m_ui.MoveUpPluginToolButton,
		SIGNAL(clicked()),
		SLOT(moveUpPlugin()));
	QObject::connect(m_ui.MoveDownPluginToolButton,
		SIGNAL(clicked()),
		SLOT(moveDownPlugin()));

	QObject::connect(m_ui.InstInstComboBox,
		SIGNAL(currentIndexChanged(const QString &)),
		SLOT(changedInstInst(const QString &)));
	QObject::connect(m_ui.DrumInstComboBox,
		SIGNAL(currentIndexChanged(const QString &)),
		SLOT(changedDrumInst(const QString &)));
	QObject::connect(m_ui.InstBankComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.DrumBankComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_groupNameRadios,
		SIGNAL(buttonClicked(QAbstractButton *)),
		SLOT(changed()));

	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));

	stabilizeListButtons();
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}


// Destructor.
qtractorMidiImportForm::~qtractorMidiImportForm()
{
}


// Accept settings (OK button slot).
void qtractorMidiImportForm::accept()
{
	// Tidy up.
	commonCleanup();

	// Get a pointer to settings.
	qtractorMidiImportExtender::exportExtensionsData *pExtendedSettings =
			m_pMidiImportExtender->exportExtensions();

	// Instrument instruments.
	QString strInstrument = m_ui.InstInstComboBox->currentText();
	if (strInstrument == getInstrumentNone())
		// Avoid language specific options.
		strInstrument.clear();
	pExtendedSettings->sMidiImportInstInst = strInstrument;

	// Instrument drums.
	strInstrument = m_ui.DrumInstComboBox->currentText();
	if (strInstrument == getInstrumentNone())
		// Avoid language specific options.
		strInstrument.clear();
	pExtendedSettings->sMidiImportDrumInst = strInstrument;

	// Bank instruments.
	pExtendedSettings->iMidiImportInstBank = -1;
	int iCurrSelection = m_ui.InstBankComboBox->currentIndex();
	if (iCurrSelection > 0 && iCurrSelection < m_instBanks.count())
		pExtendedSettings->iMidiImportInstBank = m_instBanks[iCurrSelection];

	// Bank drums.
	pExtendedSettings->iMidiImportDrumBank = -1;
	iCurrSelection = m_ui.DrumBankComboBox->currentIndex();
	if (iCurrSelection > 0 && iCurrSelection < m_drumBanks.count())
		pExtendedSettings->iMidiImportDrumBank = m_drumBanks[iCurrSelection];

	// Track name type.
	pExtendedSettings->eMidiImportTrackNameType =
			(qtractorMidiImportExtender::TrackNameType)m_groupNameRadios->id(m_groupNameRadios->checkedButton());

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorMidiImportForm::reject()
{
	// Tidy up.
	commonCleanup();

	// Backout all commands made in this dialog-session.
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->commands()->backout(m_pLastCommand);

	// Just go away.
	QDialog::reject();
}


// Stabilize current form state.
void qtractorMidiImportForm::stabilizeListButtons()
{
	// Stabilize current plugin list state.
	const int iPluginItemCount = m_ui.PluginListView->count();
	int iPluginItem = -1;
	qtractorPlugin *pPlugin = NULL;
	qtractorPluginListItem *pPluginItem
		= static_cast<qtractorPluginListItem *> (
			m_ui.PluginListView->currentItem());
	if (pPluginItem) {
		iPluginItem = m_ui.PluginListView->row(pPluginItem);
		pPlugin = pPluginItem->plugin();
	}
	m_ui.RemovePluginToolButton->setEnabled(pPlugin != NULL);
	m_ui.MoveUpPluginToolButton->setEnabled(pPluginItem && iPluginItem > 0);
	m_ui.MoveDownPluginToolButton->setEnabled(pPluginItem && iPluginItem < iPluginItemCount - 1);
}


void qtractorMidiImportForm::pluginListChanged()
{
	updateInstruments();
	stabilizeListButtons();
	changed();
}


// Plugin list: add plugin.
void qtractorMidiImportForm::addPlugin()
{
	m_ui.PluginListView->addPlugin();
	changed();
}


// Plugin list: remove plugin.
void qtractorMidiImportForm::removePlugin()
{
	m_ui.PluginListView->removePlugin();
	changed();
}


// Plugin list: move plugin up.
void qtractorMidiImportForm::moveUpPlugin()
{
	m_ui.PluginListView->moveUpPlugin();
	changed();
}


// Plugin list: move plugin down.
void qtractorMidiImportForm::moveDownPlugin()
{
	m_ui.PluginListView->moveDownPlugin();
	changed();
}


// Instrument for instruments selection changed.
void qtractorMidiImportForm::changedInstInst( const QString &text )
{
	if (m_iDirtySetup > 0)
		return;
	updateBanks(text, -1, Instruments);
	changed();
}


// Instrument for instruments selection changed.
void qtractorMidiImportForm::changedDrumInst( const QString &text )
{
	if (m_iDirtySetup > 0)
		return;
	updateBanks(text, -1, Drums);
	changed();
}


// Settings changed - enable OK
void qtractorMidiImportForm::changed()
{
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}


// Refresh instrument lists
void qtractorMidiImportForm::updateInstruments()
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	++m_iDirtySetup;

	// keep old selections - maybe we can resstore them
	const QString strOldInstInst = m_ui.InstInstComboBox->currentText();
	const QString strOldDrumInst = m_ui.DrumInstComboBox->currentText();

	const QIcon& icon = QIcon(":/images/itemInstrument.png");
	m_ui.InstInstComboBox->clear();
	m_ui.InstInstComboBox->addItem(getInstrumentNone());
	m_ui.DrumInstComboBox->clear();
	m_ui.DrumInstComboBox->addItem(getInstrumentNone());

	// Take care of MIDI plugin instrument names...
	updateInstrumentsAdd(icon,
		m_pMidiImportExtender->pluginListForGui()->midiManager());

	// Regular instrument names...
	qtractorInstrumentList::ConstIterator iter = pInstruments->constBegin();
	const qtractorInstrumentList::ConstIterator& iter_end = pInstruments->constEnd();
	for ( ; iter != iter_end; ++iter) {

		m_ui.InstInstComboBox->addItem(icon, iter.value().instrumentName());
		m_ui.DrumInstComboBox->addItem(icon, iter.value().instrumentName());
	}

	// Try to restore old instrument selections if possible.
	m_ui.InstInstComboBox->setCurrentText(strOldInstInst);
	m_ui.DrumInstComboBox->setCurrentText(strOldDrumInst);

	// In case insrtument has changed: update banks.
	const QString strNewInstInst = m_ui.InstInstComboBox->currentText();
	const QString strNewDrumInst = m_ui.DrumInstComboBox->currentText();
	if (strNewInstInst != strOldInstInst)
		updateBanks(strNewInstInst, -1, Instruments);
	if (strNewDrumInst != strOldDrumInst)
		updateBanks(strNewDrumInst, -1, Drums);

	--m_iDirtySetup;
}


// Refresh instrument lists of given MIDI buffer manager.
void qtractorMidiImportForm::updateInstrumentsAdd( const QIcon &icon,
	qtractorMidiManager *pMidiManager )
{
	if (pMidiManager == NULL)
		return;

	pMidiManager->updateInstruments();

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	qtractorMidiManager::Instruments::ConstIterator iter = list.constBegin();
	const qtractorMidiManager::Instruments::ConstIterator& iter_end = list.constEnd();
	for ( ; iter != iter_end; ++iter) {
		m_ui.InstInstComboBox->addItem(icon, iter.key());
		m_ui.DrumInstComboBox->addItem(icon, iter.key());
	}
}

// Refresh instrument banks lists.
void qtractorMidiImportForm::updateBanks(
		const QString &sInstrumentName, int iBank, enum BankType bankType )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	// Default (none) patch bank list...
	QComboBox *pComboBox = NULL;
	QMap<int, int> *pBankMap = NULL;
	switch (bankType) {
	case Instruments:
		pComboBox = m_ui.InstBankComboBox;
		pBankMap = &m_instBanks;
		break;
	case Drums:
		pComboBox = m_ui.DrumBankComboBox;
		pBankMap = &m_drumBanks;
		break;
	}
	// Should not happen at all...
	if (!pComboBox || !pBankMap)
		return;

	// keep old selections - maybe we can resstore them
	const QString strOldBank = pComboBox->currentText();

	const QIcon& icon = QIcon(":/images/itemPatches.png");
	pComboBox->clear();
	pComboBox->addItem(icon, getBankNone());

	int iBankIndex = 0;
	pBankMap->clear();
	(*pBankMap)[iBankIndex++] = -1;

	// Care of MIDI plugin instrument banks...
	bool bMidiManager = updateBanksAdd(icon,
		m_pMidiImportExtender->pluginListForGui()->midiManager(),
		sInstrumentName, iBank, iBankIndex,
		pComboBox, pBankMap);

	// Get instrument set alright...
	if (!bMidiManager && pInstruments->contains(sInstrumentName)) {
		// Try to reset old bank
		pComboBox->setCurrentText(strOldBank);
		if (pComboBox->currentIndex() > 0)
			// Old bank found: done
			return;

		// Instrument reference...
		const qtractorInstrument& instr
			= pInstruments->value(sInstrumentName);
		// For proper bank selection...
		iBankIndex = -1;
		if (iBank >= 0) {
			const qtractorInstrumentData& patch = instr.patch(iBank);
			if (!patch.name().isEmpty())
				iBankIndex = pComboBox->findText(patch.name());
		}
	}
	else if (!bMidiManager)
		iBankIndex = -1;

	// If there's banks we must choose at least one...
	if (iBank < 0 && pBankMap->count() > 1) {
		iBankIndex = 1;
		iBank = (*pBankMap)[iBankIndex];
	}

	// Do the proper bank selection...
	if (iBank < 0) {
		pComboBox->setCurrentIndex(0);
	} else if (iBankIndex < 0) {
		pComboBox->setCurrentIndex(0);
	} else {
		pComboBox->setCurrentIndex(iBankIndex);
	}
}


// Update instruments banks.
bool qtractorMidiImportForm::updateBanksAdd( const QIcon &icon,
		qtractorMidiManager *pMidiManager,
		const QString &sInstrumentName,
		int iBank,
		int &iBankIndex,
		QComboBox *pComboBox, QMap<int, int> *pBankMap)
{
	if (pMidiManager == NULL)
		return false;

	// It is possible that bank layout has changed - e.g for fluidsynth-dssi
	// loaded a new soundfont. So instead of opening and closing dialog, it
	// is possible to select no instrument and then fluidsyth-dssi to get
	// proper bank layout.
	pMidiManager->updateInstruments();

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	if (!list.contains(sInstrumentName))
		return false;

	// Refresh bank mapping...
	const qtractorMidiManager::Banks& banks = list[sInstrumentName];
	qtractorMidiManager::Banks::ConstIterator iter = banks.constBegin();
	const qtractorMidiManager::Banks::ConstIterator& iter_end = banks.constEnd();
	for ( ; iter != iter_end; ++iter) {
		pComboBox->addItem(icon, iter.value().name);
		(*pBankMap)[iBankIndex++] = iter.key();
	}
	// Reset given bank combobox index.
	iBankIndex = -1;
	// For proper bank selection...
	if (banks.contains(iBank)) {
		const qtractorMidiManager::Bank& bank = banks[iBank];
		iBankIndex = pComboBox->findText(bank.name);
	}

	// Mark that we've have something.
	return true;
}


// Cleanup right before dialog is closed.
void qtractorMidiImportForm::commonCleanup()
{
	// Close all plugin windows before the turn into zombies.
	for (qtractorPlugin *pPlugin = m_pMidiImportExtender->pluginListForGui()->first();
			pPlugin; pPlugin = pPlugin->next()) {
		pPlugin->closeEditor();
		pPlugin->closeForm();
	}

	// Unlink plugin list from view.
	m_ui.PluginListView->setPluginList(NULL);
}


// end of qtractorMidiImportForm.cpp
