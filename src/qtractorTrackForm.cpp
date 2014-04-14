// qtractorTrackForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTrackForm.h"

#include "qtractorAbout.h"
#include "qtractorTrack.h"
#include "qtractorSession.h"
#include "qtractorInstrument.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMidiBuffer.h"
#include "qtractorPlugin.h"

#include "qtractorCommand.h"

#include "qtractorBusForm.h"

#include <QColorDialog>
#include <QItemDelegate>
#include <QMessageBox>
#include <QValidator>
#include <QPainter>


//----------------------------------------------------------------------
// class qtractorColorItemDelegate -- Custom color view item delegate.
//

class qtractorColorItemDelegate : public QItemDelegate
{
public:

	// Constructor.
	qtractorColorItemDelegate ( QComboBox *pComboBox )
		: QItemDelegate(pComboBox), m_pComboBox(pComboBox) {}

	// Overridden paint method.
	void paint(QPainter *pPainter, const QStyleOptionViewItem& option,
		const QModelIndex& index) const
	{
		// Item data has the color...
		const QPoint delta(2, 2);
		QRect rect(option.rect);
		rect.setTopLeft(rect.topLeft() + delta);
		rect.setBottomRight(rect.bottomRight() - delta);
		QColor color(m_pComboBox->itemText(index.row()));
		pPainter->save();
		if (option.state & QStyle::State_Selected)
			pPainter->setPen(QPen(option.palette.highlight().color(), 2));
		else
			pPainter->setPen(QPen(option.palette.base().color(), 2));
		pPainter->setBrush(color);
		pPainter->drawRect(rect);
		pPainter->restore();
		if (option.state & QStyle::State_HasFocus)
			QItemDelegate::drawFocus(pPainter, option, option.rect);
	}

private:

	// Item color spec.
	QComboBox *m_pComboBox;
};


//----------------------------------------------------------------------------
// qtractorTrackForm::MidiProgramObserver -- Local dedicated observer.

class qtractorTrackForm::MidiProgramObserver : public qtractorObserver
{
public:

	// Constructor.
	MidiProgramObserver(qtractorTrackForm *pTrackForm, qtractorSubject *pSubject)
		: qtractorObserver(pSubject), m_pTrackForm(pTrackForm) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
	{
		if (bUpdate) {
			const int iValue = int(value());
			const int iBank = (iValue >> 7) & 0x3fff;
			const int iProg = (iValue & 0x7f);
			m_pTrackForm->setMidiProgram(iBank, iProg);
		}
	}

private:

	// Members.
	qtractorTrackForm *m_pTrackForm;
};


//----------------------------------------------------------------------------
// qtractorTrackForm -- UI wrapper form.

// Constructor.
qtractorTrackForm::qtractorTrackForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// No settings descriptor initially (the caller will set it).
	m_pTrack = NULL;

	// Current MIDI output bus to be cached.
	m_pMidiBus = NULL;

	// Set some dialog validators...
	m_ui.BankComboBox->setValidator(new QIntValidator(m_ui.BankComboBox));
	m_ui.ProgComboBox->setValidator(new QIntValidator(m_ui.ProgComboBox));

	// Bank select methods.
	const QIcon& icon = QIcon(":/images/itemProperty.png");
	m_ui.BankSelMethodComboBox->clear();
	m_ui.BankSelMethodComboBox->addItem(icon, tr("Normal"));
	m_ui.BankSelMethodComboBox->addItem(icon, tr("Bank MSB"));
	m_ui.BankSelMethodComboBox->addItem(icon, tr("Bank LSB"));
	m_ui.BankSelMethodComboBox->addItem(icon, tr("Patch"));

	// Custom colors.
	m_ui.ForegroundColorComboBox->setItemDelegate(
		new qtractorColorItemDelegate(m_ui.ForegroundColorComboBox));
	m_ui.BackgroundColorComboBox->setItemDelegate(
		new qtractorColorItemDelegate(m_ui.BackgroundColorComboBox));
	m_ui.ForegroundColorComboBox->clear();
	m_ui.BackgroundColorComboBox->clear();
	for (int i = 1; i < 28; ++i) {
		const QColor& rgbBack = qtractorTrack::trackColor(i);
		const QColor& rgbFore = rgbBack.darker();
		m_ui.ForegroundColorComboBox->addItem(rgbFore.name());
		m_ui.BackgroundColorComboBox->addItem(rgbBack.name());
	}
	
	// To save and keep bus/channel patching consistency.
	m_pOldMidiBus = NULL;
	m_iOldChannel = -1;
	m_sOldInstrumentName.clear();
	m_iOldBankSelMethod = -1;
	m_iOldBank = -1;
	m_iOldProg = -1;

	// No last acceptable command yet.
	m_pLastCommand = NULL;

	// MIDI ban/program observer.
	m_pMidiProgramObserver = NULL;

	// Initialize dirty control state.
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;
	m_iDirtyPatch = 0;

	// UI signal/slot connections...
	QObject::connect(m_ui.TrackNameTextEdit,
		SIGNAL(textChanged()),
		SLOT(changed()));
	QObject::connect(m_ui.AudioRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(trackTypeChanged()));
	QObject::connect(m_ui.MidiRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(trackTypeChanged()));
	QObject::connect(m_ui.InputBusNameComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(inputBusNameChanged(const QString&)));
	QObject::connect(m_ui.OutputBusNameComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(outputBusNameChanged(const QString&)));
	QObject::connect(m_ui.BusNameToolButton,
		SIGNAL(clicked()),
		SLOT(busNameClicked()));
	QObject::connect(m_ui.OmniCheckBox,
		SIGNAL(clicked()),
		SLOT(changed()));
	QObject::connect(m_ui.ChannelSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(channelChanged(int)));
	QObject::connect(m_ui.InstrumentComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(instrumentChanged(const QString&)));
	QObject::connect(m_ui.BankSelMethodComboBox,
		SIGNAL(activated(int)),
		SLOT(bankSelMethodChanged(int)));
	QObject::connect(m_ui.BankComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(bankChanged()));
	QObject::connect(m_ui.ProgComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(progChanged()));
	QObject::connect(m_ui.ForegroundColorComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(foregroundColorChanged(const QString&)));
	QObject::connect(m_ui.ForegroundColorToolButton,
		SIGNAL(clicked()),
		SLOT(selectForegroundColor()));
	QObject::connect(m_ui.BackgroundColorComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(backgroundColorChanged(const QString&)));
	QObject::connect(m_ui.BackgroundColorToolButton,
		SIGNAL(clicked()),
		SLOT(selectBackgroundColor()));

	QObject::connect(m_ui.PluginListView,
		SIGNAL(currentRowChanged(int)),
		SLOT(stabilizeForm()));
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

	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorTrackForm::~qtractorTrackForm (void)
{
	// Free up the MIDI bank/program observer...
	if (m_pMidiProgramObserver)
		delete m_pMidiProgramObserver;
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorTrackForm::setTrack ( qtractorTrack *pTrack )
{
	// Avoid dirty this all up.
	++m_iDirtySetup;

	// Set target track reference descriptor.
	m_pTrack = pTrack;

	// Track properties cloning...
	m_props = m_pTrack->properties();

	// Get reference of the last acceptable command...
	qtractorCommandList *pCommands = (m_pTrack->session())->commands();
	m_pLastCommand = pCommands->lastCommand();

	// Set plugin list...
	m_ui.PluginListView->setPluginList(m_pTrack->pluginList());

	// Initialize dialog widgets...
	m_ui.TrackNameTextEdit->setPlainText(m_props.trackName);
	qtractorEngine *pEngine = NULL;
	switch (m_props.trackType) {
	case qtractorTrack::Audio:
		pEngine = (m_pTrack->session())->audioEngine();
		m_ui.AudioRadioButton->setChecked(true);
		break;
	case qtractorTrack::Midi:
		pEngine = (m_pTrack->session())->midiEngine();
		m_ui.MidiRadioButton->setChecked(true);
		break;
	default:
		break;
	}
	updateTrackType(m_props.trackType);

	if (pEngine && pEngine->findInputBus(m_props.inputBusName))
		m_ui.InputBusNameComboBox->setCurrentIndex(
			m_ui.InputBusNameComboBox->findText(m_props.inputBusName));
	if (pEngine && pEngine->findOutputBus(m_props.outputBusName))
		m_ui.OutputBusNameComboBox->setCurrentIndex(
			m_ui.OutputBusNameComboBox->findText(m_props.outputBusName));

	// Force MIDI output bus recaching.
	m_pMidiBus = midiBus();

	// Make sure this will be remembered for backup.
	m_pOldMidiBus = m_pMidiBus;
	m_iOldChannel = m_props.midiChannel;
//	m_sOldInstrumentName initially blank...
	m_iOldBankSelMethod  = m_props.midiBankSelMethod;
	m_iOldBank = m_props.midiBank;
	m_iOldProg = m_props.midiProg;

	// Already time for instrument cacheing...
	updateInstruments();

	m_ui.OmniCheckBox->setChecked(m_props.midiOmni);
	m_ui.ChannelSpinBox->setValue(m_props.midiChannel + 1);
	updateChannel(m_ui.ChannelSpinBox->value(),
		m_props.midiBankSelMethod, m_props.midiBank, m_props.midiProg);

	// Update colors...
	updateColorItem(m_ui.ForegroundColorComboBox, m_props.foreground);
	updateColorItem(m_ui.BackgroundColorComboBox, m_props.background);

	// Cannot change track type, if track is already chained in session..
	m_ui.AudioRadioButton->setEnabled(m_props.trackType != qtractorTrack::Midi);
	m_ui.MidiRadioButton->setEnabled(m_props.trackType != qtractorTrack::Audio);

	// A bit of parental control...
	QObject::connect(pCommands,
		SIGNAL(updateNotifySignal(unsigned int)),
		SLOT(changed()));

	// Backup clean.
	m_iDirtyCount = 0;
	--m_iDirtySetup;

	// Done.
	stabilizeForm();
}


// Retrieve the editing track, if the case arises.
qtractorTrack *qtractorTrackForm::track (void) const
{
	return m_pTrack;
}


// Retrieve the accepted track properties, if the case arises.
const qtractorTrack::Properties& qtractorTrackForm::properties (void) const
{
	return m_props;
}


// Selected track type determinator.
qtractorTrack::TrackType qtractorTrackForm::trackType (void) const
{
	qtractorTrack::TrackType trackType = qtractorTrack::None;

	if (m_ui.AudioRadioButton->isChecked())
		trackType = qtractorTrack::Audio;
	else
	if (m_ui.MidiRadioButton->isChecked())
		trackType = qtractorTrack::Midi;

	return trackType;
}


// Accept settings (OK button slot).
void qtractorTrackForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Make changes permanent...
		m_props.trackName = m_ui.TrackNameTextEdit->toPlainText();
		m_props.trackType = trackType();
		m_props.inputBusName  = m_ui.InputBusNameComboBox->currentText();
		m_props.outputBusName = m_ui.OutputBusNameComboBox->currentText();
		// Special case for MIDI settings...
		m_props.midiOmni = m_ui.OmniCheckBox->isChecked();
		m_props.midiChannel = (m_ui.ChannelSpinBox->value() - 1);
		m_props.midiBankSelMethod = m_ui.BankSelMethodComboBox->currentIndex();
		m_props.midiBank = midiBank();
		m_props.midiProg = midiProg();
		// View colors...
		m_props.foreground = colorItem(m_ui.ForegroundColorComboBox);
		m_props.background = colorItem(m_ui.BackgroundColorComboBox);
		m_props.background.setAlpha(192);
		// Save default bus names...
		saveDefaultBusNames(m_props.trackType);
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorTrackForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Apply:
			accept();
			return;
		case QMessageBox::Discard:
			break;
		default:    // Cancel.
			bReject = false;
		}
	}

	if (bReject) {
		// Bogus track? Unlikely, but...
		if (m_pTrack) {
			// Backout all commands made this far...
			((m_pTrack->session())->commands())->backout(m_pLastCommand);
			// Try to restore the previously saved patch...
			if (m_pOldMidiBus && m_iDirtyPatch > 0) {
				m_pOldMidiBus->setPatch(m_iOldChannel, m_sOldInstrumentName,
					m_iOldBankSelMethod, m_iOldBank, m_iOldProg, m_pTrack);
			}
		}
		// Reset plugin list, before too late...
		m_ui.PluginListView->setPluginList(NULL);
		// Just go away.
		QDialog::reject();
	}
}


// Stabilize current form state.
void qtractorTrackForm::stabilizeForm (void)
{
	bool bValid = (m_iDirtyCount > 0);
	bValid = bValid && !m_ui.TrackNameTextEdit->toPlainText().isEmpty();
	bValid = bValid && trackType() != qtractorTrack::None;
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);

	// Stabilize current plugin list state.
	int iItem = -1;
	int iItemCount = m_ui.PluginListView->count();

	qtractorPlugin *pPlugin = NULL;
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (
			m_ui.PluginListView->currentItem());
	if (pItem) {
		iItem = m_ui.PluginListView->row(pItem);
		pPlugin = pItem->plugin();
	}

//	m_ui.AddPluginToolButton->setEnabled(true);
	m_ui.RemovePluginToolButton->setEnabled(pPlugin != NULL);

	m_ui.MoveUpPluginToolButton->setEnabled(pItem && iItem > 0);
	m_ui.MoveDownPluginToolButton->setEnabled(pItem && iItem < iItemCount - 1);
}


// Retrieve currently assigned MIDI output-bus, if applicable.
qtractorMidiBus *qtractorTrackForm::midiBus (void) const
{
	if (m_pTrack == NULL)
		return NULL;

	// If it ain't MIDI, bail out...
	if (trackType() != qtractorTrack::Midi)
		return NULL;

	// MIDI engine...
	qtractorMidiEngine *pMidiEngine = m_pTrack->session()->midiEngine();
	if (pMidiEngine == NULL)
		return NULL;

	// MIDI bus...
	const QString& sOutputBusName
		= m_ui.OutputBusNameComboBox->currentText();
	return static_cast<qtractorMidiBus *> (
		pMidiEngine->findOutputBus(sOutputBusName));
}


// Retrieve currently selected MIDI bank number.
int qtractorTrackForm::midiBank (void) const
{
	const QString& sBankText = m_ui.BankComboBox->currentText();
	int iBankIndex = m_ui.BankComboBox->findText(sBankText);
	if (iBankIndex >= 0 && m_banks.contains(iBankIndex)
		&& m_ui.BankComboBox->itemText(iBankIndex) == sBankText)
		return m_banks[iBankIndex];

	return sBankText.toInt();
}


// Retrieve currently selected MIDI program number.
int qtractorTrackForm::midiProg (void) const
{
	const QString& sProgText = m_ui.ProgComboBox->currentText();
	int iProgIndex = m_ui.ProgComboBox->findText(sProgText);
	if (iProgIndex >= 0 && m_progs.contains(iProgIndex)
		&& m_ui.ProgComboBox->itemText(iProgIndex) == sProgText)
		return m_progs[iProgIndex];
	
	return sProgText.toInt();
}


// Refresh instrument list.
void qtractorTrackForm::updateInstruments (void)
{
	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	// Avoid superfluous change notifications...
	++m_iDirtySetup;

	m_ui.InstrumentComboBox->clear();
	m_ui.InstrumentComboBox->addItem(tr("(No instrument)"));
	const QIcon& icon = QIcon(":/images/itemInstrument.png");

	// Take care of MIDI plugin instrument names...
	updateInstrumentsAdd(icon,
		(m_pTrack->pluginList())->midiManager());

	// And, maybe some from the MIDI output bus...
	if (m_pMidiBus && m_pMidiBus->pluginList_out())
		updateInstrumentsAdd(icon,
			(m_pMidiBus->pluginList_out())->midiManager());

	// Regular instrument names...
	qtractorInstrumentList::ConstIterator iter = pInstruments->constBegin();
	const qtractorInstrumentList::ConstIterator& iter_end = pInstruments->constEnd();
	for ( ; iter != iter_end; ++iter)
		m_ui.InstrumentComboBox->addItem(icon, iter.value().instrumentName());

	// Done.
	--m_iDirtySetup;
}


// Refresh instrument list of given MIDI buffer manager.
void qtractorTrackForm::updateInstrumentsAdd (
	const QIcon& icon, qtractorMidiManager *pMidiManager )
{
	if (pMidiManager == NULL)
		return;

	pMidiManager->updateInstruments();

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	qtractorMidiManager::Instruments::ConstIterator iter = list.constBegin();
	const qtractorMidiManager::Instruments::ConstIterator& iter_end = list.constEnd();
	for ( ; iter != iter_end; ++iter)
		m_ui.InstrumentComboBox->addItem(icon, iter.key());
}


// Update track type and buses.
void qtractorTrackForm::updateTrackType ( qtractorTrack::TrackType trackType )
{
	// Avoid superfluos change notifications...
	++m_iDirtySetup;

	// Renew the MIDI bank/program observer.
	if (m_pMidiProgramObserver) {
		delete m_pMidiProgramObserver;
		m_pMidiProgramObserver = NULL;
	}

	// Make changes due to track type change.
	qtractorEngine *pEngine = NULL;
	QIcon icon;
	switch (trackType) {
	case qtractorTrack::Audio:
		pEngine = m_pTrack->session()->audioEngine();
		icon = QIcon(":/images/trackAudio.png");
		m_ui.MidiGroupBox->hide();
		m_ui.MidiGroupBox->setEnabled(false);
		m_ui.InputBusNameComboBox->setEnabled(true);
		m_ui.OutputBusNameComboBox->setEnabled(true);
		break;
	case qtractorTrack::Midi:
		pEngine = m_pTrack->session()->midiEngine();
		icon = QIcon(":/images/trackMidi.png");
		m_ui.MidiGroupBox->show();
		m_ui.MidiGroupBox->setEnabled(true);
		m_ui.InputBusNameComboBox->setEnabled(true);
		m_ui.OutputBusNameComboBox->setEnabled(true);
		if (m_pTrack->pluginList()
			&& (m_pTrack->pluginList())->midiProgramSubject()) {
			m_pMidiProgramObserver = new MidiProgramObserver(this,
				(m_pTrack->pluginList())->midiProgramSubject());
		}
		break;
	case qtractorTrack::None:
	default:
		m_ui.MidiGroupBox->hide();
		m_ui.MidiGroupBox->setEnabled(false);
		m_ui.InputBusNameComboBox->setEnabled(false);
		m_ui.OutputBusNameComboBox->setEnabled(false);
		break;
	}

	m_ui.InputBusNameComboBox->clear();
	m_ui.OutputBusNameComboBox->clear();
	if (pEngine) {
		for (qtractorBus *pBus = pEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Input)
				m_ui.InputBusNameComboBox->addItem(icon, pBus->busName());
			if (pBus->busMode() & qtractorBus::Output)
				m_ui.OutputBusNameComboBox->addItem(icon, pBus->busName());
		}
	}

	// Load default bus names...
	loadDefaultBusNames(trackType);

	// Shake it a little bit first, but
	// make it as tight as possible...
	resize(width() - 1, height() - 1);
	adjustSize();

	// Done.
	--m_iDirtySetup;
}


// Refresh channel instrument banks list.
void qtractorTrackForm::updateChannel ( int iChannel,
	int iBankSelMethod, int iBank, int iProg )
{
	// Regular channel offset
	if (--iChannel < 0)
		return;

	// MIDI bus...
	if (m_pMidiBus == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTrackForm::updateChannel(%d, %d, %d, %d)",
		iChannel, iBankSelMethod, iBank, iProg);
#endif

	// Avoid superfluos change notifications...
	++m_iDirtySetup;

	// MIDI channel patch...
	const qtractorMidiBus::Patch& patch = m_pMidiBus->patch(iChannel);
	QString sInstrumentName = patch.instrumentName;
	if (sInstrumentName.isEmpty())
		sInstrumentName = m_pMidiBus->instrumentName();
	if (iBankSelMethod < 0)
		iBankSelMethod = patch.bankSelMethod;
#if 0
	if (iBank < 0)
		iBank = patch.bank;
	if (iProg < 0)
		iProg = patch.prog;
#endif

	// Select instrument...
	int iInstrumentIndex
		= m_ui.InstrumentComboBox->findText(sInstrumentName);
	if (iInstrumentIndex < 0) {
		iInstrumentIndex = 0;
		sInstrumentName.clear();
	}
	m_ui.InstrumentComboBox->setCurrentIndex(iInstrumentIndex);

	// Go and update the bank and program listings...
	updateBanks(sInstrumentName,
		iBankSelMethod, iBank, iProg);

	// Done.
	--m_iDirtySetup;
}


// Refresh instrument banks list.
void qtractorTrackForm::updateBanks ( const QString& sInstrumentName,
	int iBankSelMethod, int iBank, int iProg )
{
	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

//	if (sInstrumentName.isEmpty())
//		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTrackForm::updateBanks(\"%s\", %d, %d, %d)",
		sInstrumentName.toUtf8().constData(), iBankSelMethod, iBank, iProg);
#endif

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	// Avoid superfluos change notifications...
	++m_iDirtySetup;

	// Default (none) patch bank list...
	int iBankIndex = 0;
	const QIcon& icon = QIcon(":/images/itemPatches.png");
	m_banks.clear();
	m_ui.BankComboBox->clear();
	m_ui.BankComboBox->addItem(icon, tr("(None)"));
	m_banks[iBankIndex++] = -1;

	// Care of MIDI plugin instrument banks...
	bool bMidiManager = updateBanksAdd(icon,
		(m_pTrack->pluginList())->midiManager(),
		sInstrumentName, iBank, iBankIndex);
	if (!bMidiManager && m_pMidiBus && m_pMidiBus->pluginList_out()) {
		bMidiManager = updateBanksAdd(icon,
			(m_pMidiBus->pluginList_out())->midiManager(),
			sInstrumentName, iBank, iBankIndex);
	}

	// Get instrument set alright...
	if (!bMidiManager && pInstruments->contains(sInstrumentName)) {
		// Instrument reference...
		const qtractorInstrument& instr = (*pInstruments)[sInstrumentName];
		// Bank selection method...
		if (iBankSelMethod < 0)
			iBankSelMethod = instr.bankSelMethod();
		// Refresh patch bank mapping...
		const qtractorInstrumentPatches& patches = instr.patches();
		qtractorInstrumentPatches::ConstIterator it = patches.constBegin();
		const qtractorInstrumentPatches::ConstIterator& it_end = patches.constEnd();
		for (; it != it_end; ++it) {
			if (it.key() >= 0) {
				m_ui.BankComboBox->addItem(icon, it.value().name());
				m_banks[iBankIndex++] = it.key();
			}
		}
	#if 0
		// In case bank address is generic...
		if (m_ui.BankComboBox->count() < 2) {
			const qtractorInstrumentData& patch = instr.patch(iBank);
			if (!patch.name().isEmpty()) {
				m_ui.BankComboBox->addItem(icon, patch.name());
				m_banks[iBankIndex] = iBank;
			}
		}
	#endif
		// For proper bank selection...
		iBankIndex = -1;
		if (iBank >= 0) {
			const qtractorInstrumentData& patch = instr.patch(iBank);
			if (!patch.name().isEmpty())
				iBankIndex = m_ui.BankComboBox->findText(patch.name());
		}
	}
	else if (!bMidiManager)
		iBankIndex = -1;

	// Bank selection method...
	if (iBankSelMethod < 0)
		iBankSelMethod = 0;
	m_ui.BankSelMethodComboBox->setCurrentIndex(iBankSelMethod);

	// If there's banks we must choose at least one...
	if (iBank < 0 && m_banks.count() > 1) {
		iBankIndex = 1;
		iBank = m_banks[iBankIndex];
	}

	// Do the proper bank selection...
	if (iBank < 0) {
		m_ui.BankComboBox->setCurrentIndex(0);
	} else if (iBankIndex < 0) {
		m_ui.BankComboBox->setEditText(QString::number(iBank));
	} else {
		m_ui.BankComboBox->setCurrentIndex(iBankIndex);
	}

	// And update the bank and program listing...
	updatePrograms(sInstrumentName, iBank, iProg);

	// Done.
	--m_iDirtySetup;
}


// Refresh bank programs list.
void qtractorTrackForm::updatePrograms (  const QString& sInstrumentName,
	int iBank, int iProg )
{
	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

//	if (sInstrumentName.isEmpty())
//		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTrackForm::updatePrograms(\"%s\", %d, %d)",
		sInstrumentName.toUtf8().constData(), iBank, iProg);
#endif

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	// Avoid superfluos change notifications...
	++m_iDirtySetup;

	// Default (none) patch program list...
	// Refresh patch program mapping...
	int iProgIndex = 0;
	const QIcon& icon = QIcon(":/images/itemChannel.png");
	m_progs.clear();
	m_ui.ProgComboBox->clear();
	m_ui.ProgComboBox->addItem(icon, tr("(None)"));
	m_progs[iProgIndex++] = -1;

	// Take care of MIDI plugin instrument programs...
	bool bMidiManager = updateProgramsAdd(icon,
		(m_pTrack->pluginList())->midiManager(),
		sInstrumentName, iBank, iProg, iProgIndex);
	if (!bMidiManager && m_pMidiBus && m_pMidiBus->pluginList_out()) {
		bMidiManager = updateProgramsAdd(icon,
			(m_pMidiBus->pluginList_out())->midiManager(),
			sInstrumentName, iBank, iProg, iProgIndex);
	}

	// Get instrument set alright...
	if (!bMidiManager && pInstruments->contains(sInstrumentName)) {
		// Instrument reference...
		const qtractorInstrument& instr = (*pInstruments)[sInstrumentName];
		// Bank reference...
		const qtractorInstrumentData& bank = instr.patch(iBank);
		// Enumerate the explicit given program list...
		qtractorInstrumentData::ConstIterator it = bank.constBegin();
		const qtractorInstrumentData::ConstIterator& it_end = bank.constEnd();
		for (; it != it_end; ++it) {
			if (it.key() >= 0 && !it.value().isEmpty()) {
				m_ui.ProgComboBox->addItem(icon, it.value());
				m_progs[iProgIndex++] = it.key();
			}
		}
		// For proper program selection...
		iProgIndex = -1;
		if (iProg >= 0 && bank.contains(iProg))
			iProgIndex = m_ui.ProgComboBox->findText(bank[iProg]);
	}
	else if (!bMidiManager)
		iProgIndex = -1;
#if 0
	// If there's programs we must choose at least one...
	if (iProg < 0 && m_progs.count() > 1) {
		iProgIndex = 1;
		iProg = m_progs[iProgIndex];
	}
#endif
	// In case program address is generic...
	if (m_ui.ProgComboBox->count() < 2) {
		// Just make a generic program list...
		for (int i = 0; i < 128; ++i) {
			m_ui.ProgComboBox->addItem(icon, QString("%1  - -").arg(i + 1));
			m_progs[i + 1] = i;
		}
		if (iProg >= 0)
			iProgIndex = iProg + 1;
	}

	// Do the proper program selection...
	if (iProg < 0) {
		m_ui.ProgComboBox->setCurrentIndex(0);
	} else if (iProgIndex < 0) {
		m_ui.ProgComboBox->setEditText(QString::number(iProg));
	} else {
		m_ui.ProgComboBox->setCurrentIndex(iProgIndex);
	}

	// Done.
	--m_iDirtySetup;
}


// Update instrument banks.
bool qtractorTrackForm::updateBanksAdd (
	const QIcon& icon, qtractorMidiManager *pMidiManager,
	const QString& sInstrumentName, int iBank, int& iBankIndex )
{
	if (pMidiManager == NULL)
		return false;

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	if (!list.contains(sInstrumentName))
		return false;

	// Refresh bank mapping...
	const qtractorMidiManager::Banks& banks = list[sInstrumentName];
	qtractorMidiManager::Banks::ConstIterator iter = banks.constBegin();
	const qtractorMidiManager::Banks::ConstIterator& iter_end = banks.constEnd();
	for ( ; iter != iter_end; ++iter) {
		m_ui.BankComboBox->addItem(icon, iter.value().name);
		m_banks[iBankIndex++] = iter.key();
	}
	// Reset given bank combobox index.
	iBankIndex = -1;
	// For proper bank selection...
	if (banks.contains(iBank)) {
		const qtractorMidiManager::Bank& bank = banks[iBank];
		iBankIndex = m_ui.BankComboBox->findText(bank.name);
	}

	// Mark that we've have something.
	return true;
}


// Update instrument programs.
bool qtractorTrackForm::updateProgramsAdd (
	const QIcon& icon, qtractorMidiManager *pMidiManager,
	const QString& sInstrumentName, int iBank, int iProg, int& iProgIndex )
{
	if (pMidiManager == NULL)
		return false;

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	if (!list.contains(sInstrumentName))
		return false;

	// Bank reference...
	const qtractorMidiManager::Banks& banks
		= list[sInstrumentName];
	if (banks.contains(iBank)) {
		const qtractorMidiManager::Progs& progs = banks[iBank].progs;
		// Refresh program mapping...
		const QString sProg("%1 - %2");
		qtractorMidiManager::Progs::ConstIterator iter = progs.constBegin();
		const qtractorMidiManager::Progs::ConstIterator& iter_end = progs.constEnd();
		for ( ; iter != iter_end; ++iter) {
			m_ui.ProgComboBox->addItem(icon,
				sProg.arg(iter.key()).arg(iter.value()));
			m_progs[iProgIndex++] = iter.key();
		}
		// Reset given program combobox index.
		iProgIndex = -1;
		// For proper program selection...
		if (progs.contains(iProg)) {
			iProgIndex = m_ui.ProgComboBox->findText(
				sProg.arg(iProg).arg(progs[iProg]));
		}
	}
	else iProgIndex = -1;

	// Mark that we've have something.
	return true;
}


// Update and set a color item.
void qtractorTrackForm::updateColorItem ( QComboBox *pComboBox,
	const QColor& color )
{
	// Have some immediate feedback...
	updateColorText(pComboBox, color);

	// Check if already exists...
	int iItem = pComboBox->findText(color.name());
	if (iItem >= 0) {
		pComboBox->setCurrentIndex(iItem);
		return;
	}

	// Nope, we'll add it custom...
	pComboBox->addItem(color.name());
	pComboBox->setCurrentIndex(pComboBox->count() - 1);
}


// Update color item visual text.
void qtractorTrackForm::updateColorText ( QComboBox *pComboBox,
	const QColor& color )
{
	QPalette pal;
//	pal.setColor(QPalette::Window, color);
	pal.setColor(QPalette::Base, color);
	pal.setColor(QPalette::Text,
		color.value() < 0x7f ? color.lighter(200) : color.darker(300));
//	pComboBox->lineEdit()->setPalette(pal);
	pComboBox->setPalette(pal);
}


// Retreieve currently selected color item.
QColor qtractorTrackForm::colorItem ( QComboBox *pComboBox )
{
	return QColor(pComboBox->currentText());
}


// Make more changes due.
void qtractorTrackForm::foregroundColorChanged ( const QString& sText )
{
	if (m_iDirtySetup > 0)
		return;

	updateColorText(m_ui.ForegroundColorComboBox, QColor(sText));

	++m_iDirtyCount;
	stabilizeForm();
}


void qtractorTrackForm::backgroundColorChanged ( const QString& sText )
{
	if (m_iDirtySetup > 0)
		return;

	updateColorText(m_ui.BackgroundColorComboBox, QColor(sText));

	++m_iDirtyCount;
	stabilizeForm();
}


void qtractorTrackForm::pluginListChanged (void)
{
	updateInstruments();
	changed();
}


void qtractorTrackForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	++m_iDirtyCount;
	stabilizeForm();
}


// Make changes due to track type.
void qtractorTrackForm::trackTypeChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

	if (m_pOldMidiBus) {
		// Restore previously current/saved patch...
		m_pOldMidiBus->setPatch(m_iOldChannel, m_sOldInstrumentName,
			m_iOldBankSelMethod, m_iOldBank, m_iOldProg, m_pTrack);
		// Reset restorable patch reference...
		m_pOldMidiBus = NULL;
		m_iOldChannel = -1;
		m_sOldInstrumentName.clear();
		m_iOldBankSelMethod = -1;
		m_iOldBank = -1;
		m_iOldProg = -1;
	}

	updateTrackType(trackType());
//	inputBusNameChanged(m_ui.InputBusNameComboBox->currentText());
	outputBusNameChanged(m_ui.OutputBusNameComboBox->currentText());
}


// Make changes due to input-bus name.
void qtractorTrackForm::inputBusNameChanged ( const QString& /* sBusName */ )
{
	changed();
}


// Make changes due to output-bus name.
void qtractorTrackForm::outputBusNameChanged ( const QString& sBusName )
{
	if (m_iDirtySetup > 0)
		return;

	if (m_pTrack == NULL)
		return;

	// It all depends on the track type we're into...
	qtractorTrack::TrackType trackType = qtractorTrackForm::trackType();

	// (Re)initialize plugin-list audio output bus properly...
	qtractorSession *pSession = m_pTrack->session();
	qtractorAudioEngine *pAudioEngine = NULL;
	if (pSession)
		pAudioEngine = pSession->audioEngine();
	if (pAudioEngine) {
		// Get the audio bus applicable for the plugin list...
		qtractorAudioBus *pAudioBus = NULL;
		if (trackType == qtractorTrack::Audio)
			pAudioBus = static_cast<qtractorAudioBus *> (
				pAudioEngine->findOutputBus(sBusName));
		// FIXME: Master audio bus as reference, still...
		if (pAudioBus == NULL)
			pAudioBus = static_cast<qtractorAudioBus *> (
				pAudioEngine->buses().first());
		// If an audio bus has been found applicable,
		// must set plugin-list channel buffers...
		if (pAudioBus) {
			m_pTrack->pluginList()->setBuffer(pAudioBus->channels(),
				pAudioEngine->bufferSize(), pAudioEngine->sampleRate(),
				trackType == qtractorTrack::Audio
					? qtractorPluginList::AudioTrack
					: qtractorPluginList::MidiTrack);
		}
	}

	// Recache the applicable MIDI output bus ...
	if (trackType == qtractorTrack::Midi) {
		m_pMidiBus = midiBus();
		updateInstruments();
	}

	channelChanged(m_ui.ChannelSpinBox->value());
}


// Manage buses.
void qtractorTrackForm::busNameClicked (void)
{
	if (m_iDirtySetup > 0)
		return;

	// Depending on track type...
	qtractorEngine *pEngine = NULL;
	switch (trackType()) {
	case qtractorTrack::Audio:
		pEngine = m_pTrack->session()->audioEngine();
		break;
	case qtractorTrack::Midi:
		pEngine = m_pTrack->session()->midiEngine();
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Call here the bus management form.
	qtractorBusForm busForm(this);
	// Pre-select bus...
	const QString& sBusName = m_ui.OutputBusNameComboBox->currentText();
	if (pEngine && !sBusName.isEmpty())
		busForm.setBus(pEngine->findBus(sBusName));
	// Go for it...
	busForm.exec();

	// Check if any buses have changed...
	if (busForm.isDirty()) {
		// Try to preserve current selected names...
		const QString sInputBusName  = m_ui.InputBusNameComboBox->currentText();
		const QString sOutputBusName = m_ui.OutputBusNameComboBox->currentText();
		// Update the comboboxes...
		trackTypeChanged();
		// Restore old current selected ones...
		if (pEngine->findInputBus(sInputBusName))
			m_ui.InputBusNameComboBox->setCurrentIndex(
				m_ui.InputBusNameComboBox->findText(sInputBusName));
		if (pEngine->findOutputBus(sOutputBusName))
			m_ui.OutputBusNameComboBox->setCurrentIndex(
				m_ui.OutputBusNameComboBox->findText(sOutputBusName));
	}
}


// Make changes due to MIDI channel.
void qtractorTrackForm::channelChanged ( int iChannel )
{
	if (m_iDirtySetup > 0)
		return;

	// First update channel instrument mapping...
	updateChannel(iChannel,
		-1, // m_ui.BankSelMethodComboBox->currentItem(),
		-1, // midiBank(),
		-1);// midiProgram());

	progChanged();
}


// Make changes due to MIDI instrument.
void qtractorTrackForm::instrumentChanged ( const QString& sInstrumentName )
{
	if (m_iDirtySetup > 0)
		return;

	updateBanks(sInstrumentName,
		-1, // m_ui.BankSelMethodComboBox->currentItem(),
		midiBank(),
		midiProg());

	progChanged();
}


// Make changes due to MIDI bank selection method.
void qtractorTrackForm::bankSelMethodChanged ( int /* iBankSelMethod */ )
{
	if (m_iDirtySetup > 0)
		return;

	progChanged();
}


// Make changes due to MIDI bank.
void qtractorTrackForm::bankChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

	QString sInstrumentName;
	if (m_ui.InstrumentComboBox->currentIndex() > 0)
		sInstrumentName = m_ui.InstrumentComboBox->currentText();

	updatePrograms(sInstrumentName,
		midiBank(),
		midiProg());

	progChanged();
}


// Make changes due to MIDI program.
void qtractorTrackForm::progChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

	// Of course, only applicable on MIDI tracks...
	if (m_pMidiBus) {
		// Patch parameters...
		unsigned short iChannel = m_ui.ChannelSpinBox->value() - 1;
		QString sInstrumentName;
		if (m_ui.InstrumentComboBox->currentIndex() > 0)
			sInstrumentName = m_ui.InstrumentComboBox->currentText();
		int iBankSelMethod = m_ui.BankSelMethodComboBox->currentIndex();
		int iBank = midiBank();
		int iProg = midiProg();
		// Keep old bus/channel patching consistency.
		if (m_pMidiBus != m_pOldMidiBus || iChannel != m_iOldChannel) {
			// Restore previously saved patch...
			if (m_pOldMidiBus) {
				m_pOldMidiBus->setPatch(m_iOldChannel, m_sOldInstrumentName,
					m_iOldBankSelMethod, m_iOldBank, m_iOldProg, m_pTrack);
			}
			// Save current channel patch...
			const qtractorMidiBus::Patch& patch = m_pMidiBus->patch(iChannel);
			if (!patch.instrumentName.isEmpty()) {
				m_pOldMidiBus = m_pMidiBus;
				m_iOldChannel = iChannel;
				m_sOldInstrumentName = patch.instrumentName;
				m_iOldBankSelMethod  = patch.bankSelMethod;
				m_iOldBank = patch.bank;
				m_iOldProg = patch.prog;
			}
		}
		// Patch it directly...
		m_pMidiBus->setPatch(iChannel, sInstrumentName,
			iBankSelMethod, iBank, iProg, m_pTrack);
		// Make it dirty.
		++m_iDirtyPatch;
	}

	// Flag that it changed anyhow!
	changed();
}


// Select custom track foreground color.
void qtractorTrackForm::selectForegroundColor (void)
{
	const QColor& color = QColorDialog::getColor(
		colorItem(m_ui.ForegroundColorComboBox), this,
		tr("Forgeground Color") + " - " QTRACTOR_TITLE);

	if (color.isValid()) {
		updateColorItem(m_ui.ForegroundColorComboBox, color);
		changed();
	}
}


// Select custom track background color.
void qtractorTrackForm::selectBackgroundColor (void)
{
	const QColor& color = QColorDialog::getColor(
		colorItem(m_ui.BackgroundColorComboBox), this,
		tr("Background Color") + " - " QTRACTOR_TITLE);

	if (color.isValid()) {
		updateColorItem(m_ui.BackgroundColorComboBox, color);
		changed();
	}
}


// Plugin list slots.
void qtractorTrackForm::addPlugin (void)
{
	m_ui.PluginListView->addPlugin();
}

void qtractorTrackForm::removePlugin (void)
{
	m_ui.PluginListView->removePlugin();
}

void qtractorTrackForm::moveUpPlugin (void)
{
	m_ui.PluginListView->moveUpPlugin();
}

void qtractorTrackForm::moveDownPlugin (void)
{
	m_ui.PluginListView->moveDownPlugin();
}


// MIDI bank/program settlers.
void qtractorTrackForm::setMidiProgram ( int iBank, int iProg )
{
	QString sInstrumentName;
	if (m_ui.InstrumentComboBox->currentIndex() > 0)
		sInstrumentName = m_ui.InstrumentComboBox->currentText();

	updateBanks(
		sInstrumentName,
		-1, // m_ui.BankSelMethodComboBox->currentIndex()
		iBank,
		iProg
	);

	changed();
}


//----------------------------------------------------------------------------
// Default bus names...

QString qtractorTrackForm::g_sAudioInputBusName;
QString qtractorTrackForm::g_sAudioOutputBusName;

QString qtractorTrackForm::g_sMidiInputBusName;
QString qtractorTrackForm::g_sMidiOutputBusName;


// Load default bus names...
void qtractorTrackForm::loadDefaultBusNames (
	qtractorTrack::TrackType trackType )
{
	QString sInputBusName;
	QString sOutputBusName;

	switch (trackType) {
	case qtractorTrack::Audio:
		sInputBusName  = g_sAudioInputBusName;;
		sOutputBusName = g_sAudioOutputBusName;
		break;
	case qtractorTrack::Midi:
		sInputBusName  = g_sMidiInputBusName;
		sOutputBusName = g_sMidiOutputBusName;
		break;
	default:
		break;
	}

	const int iInputBusIndex
		= m_ui.InputBusNameComboBox->findText(sInputBusName);
	m_ui.InputBusNameComboBox->setCurrentIndex(
		iInputBusIndex < 0 ? 0 : iInputBusIndex);

	const int iOutputBusIndex
		= m_ui.OutputBusNameComboBox->findText(sOutputBusName);
	m_ui.OutputBusNameComboBox->setCurrentIndex(
		iOutputBusIndex < 0 ? 0 : iOutputBusIndex);
}


// Save default bus names...
void qtractorTrackForm::saveDefaultBusNames (
	qtractorTrack::TrackType trackType ) const
{
	const int iInputBusIndex  = m_ui.InputBusNameComboBox->currentIndex();
	const int iOutputBusIndex = m_ui.OutputBusNameComboBox->currentIndex();

	switch (trackType) {
	case qtractorTrack::Audio:
		if (iInputBusIndex > 0)
			g_sAudioInputBusName  = m_ui.InputBusNameComboBox->currentText();
		if (iOutputBusIndex > 0)
			g_sAudioOutputBusName = m_ui.OutputBusNameComboBox->currentText();
		break;
	case qtractorTrack::Midi:
		if (iInputBusIndex > 0)
			g_sMidiInputBusName   = m_ui.InputBusNameComboBox->currentText();
		if (iOutputBusIndex > 0)
			g_sMidiOutputBusName  = m_ui.OutputBusNameComboBox->currentText();
		break;
	default:
		break;
	}
}


// end of qtractorTrackForm.cpp
