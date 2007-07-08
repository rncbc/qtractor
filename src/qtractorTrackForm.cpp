// qtractorTrackForm.cpp
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

#include "qtractorTrackForm.h"

#include "qtractorAbout.h"
#include "qtractorTrack.h"
#include "qtractorSession.h"
#include "qtractorInstrument.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"
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
// qtractorTrackForm -- UI wrapper form.

// Constructor.
qtractorTrackForm::qtractorTrackForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// No settings descriptor initially (the caller will set it).
	m_pTrack = NULL;

	// Set some dialog validators...
	m_ui.BankComboBox->setValidator(new QIntValidator(m_ui.BankComboBox));
	m_ui.ProgComboBox->setValidator(new QIntValidator(m_ui.ProgComboBox));

	// Bank select methods.
	const QIcon& icon = QIcon(":/icons/itemProperty.png");
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
	for (int i = 1; i < 28; i++) {
		const QColor rgbBack = qtractorTrack::trackColor(i);
		const QColor rgbFore = rgbBack.dark();
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

	// Initialize dirty control state.
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	// Already time for instrument cacheing...
	updateInstruments();

	// Try to restore old window positioning.
	adjustSize();

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
		SIGNAL(textChanged(const QString &)),
		SLOT(bankChanged()));
	QObject::connect(m_ui.ProgComboBox,
		SIGNAL(textChanged(const QString &)),
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
	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qtractorTrackForm::~qtractorTrackForm (void)
{
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorTrackForm::setTrack ( qtractorTrack *pTrack )
{
	// Set reference descriptor.
	m_pTrack = pTrack;

	// Avoid dirty this all up.
	m_iDirtySetup++;

	// Track properties cloning...
	m_props = m_pTrack->properties();

	// Initialize dialog widgets...
	m_ui.TrackNameTextEdit->setPlainText(m_props.trackName);
	qtractorEngine *pEngine = NULL;
	switch (m_props.trackType) {
	case qtractorTrack::Audio:
		pEngine = pTrack->session()->audioEngine();
		m_ui.AudioRadioButton->setChecked(true);
		break;
	case qtractorTrack::Midi:
		pEngine = pTrack->session()->midiEngine();
		m_ui.MidiRadioButton->setChecked(true);
		break;
	default:
		break;
	}
	updateTrackType(m_props.trackType);

	if (pEngine && pEngine->findBus(m_props.inputBusName))
		m_ui.InputBusNameComboBox->setCurrentIndex(
			m_ui.InputBusNameComboBox->findText(m_props.inputBusName));
	if (pEngine && pEngine->findBus(m_props.outputBusName))
		m_ui.OutputBusNameComboBox->setCurrentIndex(
			m_ui.OutputBusNameComboBox->findText(m_props.outputBusName));

	m_ui.ChannelSpinBox->setValue(m_props.midiChannel + 1);
	updateChannel(m_ui.ChannelSpinBox->value(),
		m_props.midiBankSelMethod, m_props.midiBank, m_props.midiProgram);

	// Update colors...
	updateColorItem(m_ui.ForegroundColorComboBox, m_props.foreground);
	updateColorItem(m_ui.BackgroundColorComboBox, m_props.background);

	// Cannot change track type, if track is already chained in session..
	m_ui.AudioRadioButton->setEnabled(m_props.trackType != qtractorTrack::Midi);
	m_ui.MidiRadioButton->setEnabled(m_props.trackType != qtractorTrack::Audio);

	// Backup clean.
	m_iDirtyCount = 0;
	m_iDirtySetup--;

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
qtractorTrack::TrackType qtractorTrackForm::trackType (void)
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
		m_props.midiChannel = (m_ui.ChannelSpinBox->value() - 1);
		m_props.midiBankSelMethod = m_ui.BankSelMethodComboBox->currentIndex();
		m_props.midiBank    = midiBank();
		m_props.midiProgram = midiProgram();
		// View colors...
		m_props.foreground = colorItem(m_ui.ForegroundColorComboBox);
		m_props.background = colorItem(m_ui.BackgroundColorComboBox);
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
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			tr("Apply"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Apply...
			accept();
			return;
		case 1:     // Discard
			break;
		default:    // Cancel.
			bReject = false;
		}
	}

	if (bReject) {
		// Try to restore the previously saved patch...
		if (m_pOldMidiBus) {
			m_pOldMidiBus->setPatch(m_iOldChannel, m_sOldInstrumentName,
				m_iOldBankSelMethod, m_iOldBank, m_iOldProg);
		}
		QDialog::reject();
	}
}


// Stabilize current form state.
void qtractorTrackForm::stabilizeForm (void)
{
	bool bValid = (m_iDirtyCount > 0);
	bValid = bValid && !m_ui.TrackNameTextEdit->toPlainText().isEmpty();
	bValid = bValid && trackType() != qtractorTrack::None;
	m_ui.OkPushButton->setEnabled(bValid);
}


// Retrieve currently assigned MIDI output-bus, if applicable.
qtractorMidiBus *qtractorTrackForm::midiBus (void)
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
	const QString& sBusName = m_ui.OutputBusNameComboBox->currentText();
	return static_cast<qtractorMidiBus *> (pMidiEngine->findBus(sBusName));
}


// Retrieve currently selected MIDI bank number.
int qtractorTrackForm::midiBank (void)
{
	int iBankIndex = m_ui.BankComboBox->currentIndex();
	const QString& sBankText = m_ui.BankComboBox->currentText();
	if (m_banks.contains(iBankIndex)
		&& m_ui.BankComboBox->itemText(iBankIndex) == sBankText)
		return m_banks[iBankIndex];

	return sBankText.toInt();
}


// Retrieve currently selected MIDI program number.
int qtractorTrackForm::midiProgram (void)
{
	int iProgIndex = m_ui.ProgComboBox->currentIndex();
	const QString& sProgText = m_ui.ProgComboBox->currentText();
	if (m_progs.contains(iProgIndex)
		&& m_ui.ProgComboBox->itemText(iProgIndex) == sProgText)
		return m_progs[iProgIndex];

	return sProgText.toInt();
}


// Refresh instrument list.
void qtractorTrackForm::updateInstruments (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorInstrumentList *pInstruments = pMainForm->instruments();
	if (pInstruments == NULL)
		return;

	// Avoid superfluos change notifications...
	m_iDirtySetup++;

	m_ui.InstrumentComboBox->clear();
	const QIcon& icon = QIcon(":/icons/itemInstrument.png");
	for (qtractorInstrumentList::Iterator iter = pInstruments->begin();
			iter != pInstruments->end(); ++iter) {
		m_ui.InstrumentComboBox->addItem(icon, iter.value().instrumentName());
	}

	// Done.
	m_iDirtySetup--;
}


// Update track type and buses.
void qtractorTrackForm::updateTrackType ( qtractorTrack::TrackType trackType )
{
	// Avoid superfluos change notifications...
	m_iDirtySetup++;

	// Make changes due to track type change.
	qtractorEngine *pEngine = NULL;
	QIcon icon;
	switch (trackType) {
	case qtractorTrack::Audio:
		pEngine = m_pTrack->session()->audioEngine();
		icon = QIcon(":/icons/trackAudio.png");
		m_ui.MidiGroupBox->setEnabled(false);
		m_ui.InputBusNameComboBox->setEnabled(true);
		m_ui.OutputBusNameComboBox->setEnabled(true);
		break;
	case qtractorTrack::Midi:
		pEngine = m_pTrack->session()->midiEngine();
		icon = QIcon(":/icons/trackMidi.png");
		m_ui.MidiGroupBox->setEnabled(true);
		m_ui.InputBusNameComboBox->setEnabled(true);
		m_ui.OutputBusNameComboBox->setEnabled(true);
		break;
	case qtractorTrack::None:
	default:
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

	// Done.
	m_iDirtySetup--;
}


// Refresh channel instrument banks list.
void qtractorTrackForm::updateChannel ( int iChannel,
	int iBankSelMethod, int iBank, int iProg )
{
	// Regular channel offset
	if (--iChannel < 0)
		return;

	// MIDI bus...
	qtractorMidiBus *pMidiBus = midiBus();
	if (pMidiBus == NULL)
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updateChannel(%d, %d, %d, %d)\n",
		iChannel, iBankSelMethod, iBank, iProg);
#endif

	// Avoid superfluos change notifications...
	m_iDirtySetup++;

	// MIDI channel patch...
	const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
	if (iBankSelMethod < 0)
		iBankSelMethod = patch.bankSelMethod;
#if 0
	if (iBank < 0)
		iBank = patch.bank;
	if (iProg < 0)
		iProg = patch.prog;
#endif

	// Select instrument...
	m_ui.InstrumentComboBox->setCurrentIndex(
		m_ui.InstrumentComboBox->findText(patch.instrumentName));

	// Go and update the bank and program listings...
	updateBanks(m_ui.InstrumentComboBox->currentText(),
		iBankSelMethod, iBank, iProg);

	// Done.
	m_iDirtySetup--;
}


// Refresh instrument banks list.
void qtractorTrackForm::updateBanks ( const QString& sInstrumentName,
	int iBankSelMethod, int iBank, int iProg )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	if (m_pTrack == NULL)
		return;
	if (sInstrumentName.isEmpty())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updateBanks(\"%s\", %d, %d, %d)\n",
		sInstrumentName.toUtf8().constData(), iBankSelMethod, iBank, iProg);
#endif

	qtractorInstrumentList *pInstruments = pMainForm->instruments();
	if (pInstruments == NULL)
		return;

	// Avoid superfluos change notifications...
	m_iDirtySetup++;

	// Instrument reference...
	qtractorInstrument& instr = (*pInstruments)[sInstrumentName];

	// Bank selection method...
	if (iBankSelMethod < 0)
		iBankSelMethod = instr.bankSelMethod();
	m_ui.BankSelMethodComboBox->setCurrentIndex(iBankSelMethod);

	// Refresh patch bank mapping...
	int iBankIndex = 0;
	const QIcon& icon = QIcon(":/icons/itemPatches.png");
	m_banks.clear();
	m_ui.BankComboBox->clear();
	m_ui.BankComboBox->addItem(icon, tr("(None)"));
	m_banks[iBankIndex++] = -1;
	qtractorInstrumentPatches::Iterator it;
	for (it = instr.patches().begin(); it != instr.patches().end(); ++it) {
		if (it.key() >= 0) {
			m_ui.BankComboBox->addItem(icon, it.value().name());
			m_banks[iBankIndex++] = it.key();
		}
	}

#if 0
	// In case bank address is generic...
	if (m_ui.BankComboBox->count() < 1) {
		qtractorInstrumentData& patch = instr.patch(iBank);
		if (!patch.name().isEmpty()) {
			m_ui.BankComboBox->insertItem(pixmap, patch.name());
			m_banks[iBankIndex] = iBank;
		}
	}
#endif

	// Do the proper bank selection...
	if (iBank >= 0) {
		qtractorInstrumentData& bank = instr.patch(iBank);
		iBankIndex = m_ui.BankComboBox->findText(bank.name());
		if (iBankIndex >= 0) {
			m_ui.BankComboBox->setCurrentIndex(iBankIndex);
		} else {
			m_ui.BankComboBox->setEditText(QString::number(iBank));
		}
	} else {
		m_ui.BankComboBox->setCurrentIndex(0);
	}

	// And update the bank and program listing...
	updatePrograms(sInstrumentName, iBank, iProg);

	// Done.
	m_iDirtySetup--;
}


// Refresh bank programs list.
void qtractorTrackForm::updatePrograms (  const QString& sInstrumentName,
	int iBank, int iProg )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	if (m_pTrack == NULL)
		return;
	if (sInstrumentName.isEmpty())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updatePrograms(\"%s\", %d, %d)\n",
		sInstrumentName.toUtf8().constData(), iBank, iProg);
#endif

	qtractorInstrumentList *pInstruments = pMainForm->instruments();
	if (pInstruments == NULL)
		return;

	// Avoid superfluos change notifications...
	m_iDirtySetup++;

// Instrument reference...
	qtractorInstrument& instr = (*pInstruments)[sInstrumentName];

	// Bank reference...
	qtractorInstrumentData& bank = instr.patch(iBank);

	// Refresh patch program mapping...
	int iProgIndex = 0;
	const QIcon& icon = QIcon(":/icons/itemChannel.png");
	m_progs.clear();
	m_ui.ProgComboBox->clear();
	m_ui.ProgComboBox->addItem(icon, tr("(None)"));
	m_progs[iProgIndex++] = -1;
	// Enumerate the explicit given program list...
	qtractorInstrumentData::Iterator it;
	for (it = bank.begin(); it != bank.end(); ++it) {
		if (it.key() >= 0 && !it.value().isEmpty()) {
			m_ui.ProgComboBox->addItem(icon, it.value());
			m_progs[iProgIndex++] = it.key();
		}
	}

#if 0
	// In case program address is generic...
	if (m_ui.ProgComboBox->count() < 1) {
		// Just make a generic program list...
		for (iProgIndex = 0; iProgIndex < 128; iProgIndex++) {
			m_ui.ProgComboBox->insertItem(pixmap,
				QString::number(iProgIndex + 1) + "  - -");
			m_progs[iProgIndex] = iProgIndex;
		}
	}
#endif

	// Select proper program...
	if (iProg >= 0) {
		iProgIndex = -1;
		if (bank.contains(iProg))
			iProgIndex = m_ui.ProgComboBox->findText(bank[iProg]);
		if (iProgIndex >= 0) {
			m_ui.ProgComboBox->setCurrentIndex(iProgIndex);
		} else {
			m_ui.ProgComboBox->setEditText(QString::number(iProg));
		}
	} else {
		m_ui.ProgComboBox->setCurrentIndex(0);
	}

	// Done.
	m_iDirtySetup--;
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
	QPalette pal(color);
	pal.setColor(QPalette::Base, color);
	pComboBox->lineEdit()->setPalette(pal);
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

	m_iDirtyCount++;
	stabilizeForm();
}


void qtractorTrackForm::backgroundColorChanged ( const QString& sText )
{
	if (m_iDirtySetup > 0)
		return;

	updateColorText(m_ui.BackgroundColorComboBox, QColor(sText));

	m_iDirtyCount++;
	stabilizeForm();
}


void qtractorTrackForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	m_iDirtyCount++;
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
			m_iOldBankSelMethod, m_iOldBank, m_iOldProg);
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
void qtractorTrackForm::outputBusNameChanged ( const QString& /* sBusName */ )
{
	if (m_iDirtySetup > 0)
		return;

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
		if (pEngine->findBus(sInputBusName))
			m_ui.InputBusNameComboBox->setCurrentIndex(
				m_ui.InputBusNameComboBox->findText(sInputBusName));
		if (pEngine->findBus(sOutputBusName))
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
		midiProgram());

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

	updatePrograms(m_ui.InstrumentComboBox->currentText(),
		midiBank(), midiProgram());

	progChanged();
}


// Make changes due to MIDI program.
void qtractorTrackForm::progChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

	// Of course, only applicable on MIDI tracks...
	qtractorMidiBus *pMidiBus = midiBus();
	if (pMidiBus) {
		// Patch parameters...
		unsigned short iChannel = m_ui.ChannelSpinBox->value() - 1;
		const QString& sInstrumentName = m_ui.InstrumentComboBox->currentText();
		int iBankSelMethod = m_ui.BankSelMethodComboBox->currentIndex();
		int iBank = midiBank();
		int iProg = midiProgram();
		// Keep old bus/channel patching consistency.
		if (pMidiBus != m_pOldMidiBus || iChannel != m_iOldChannel) {
			// Restore previously saved patch...
			if (m_pOldMidiBus) {
				m_pOldMidiBus->setPatch(m_iOldChannel, m_sOldInstrumentName,
					m_iOldBankSelMethod, m_iOldBank, m_iOldProg);
			}
			// Save current channel patch...
			const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
			if (!patch.instrumentName.isEmpty()) {
				m_pOldMidiBus = pMidiBus;
				m_iOldChannel = iChannel;
				m_sOldInstrumentName = patch.instrumentName;
				m_iOldBankSelMethod  = patch.bankSelMethod;
				m_iOldBank = patch.bank;
				m_iOldProg = patch.prog;
			}
		}
		// Patch it directly...
		pMidiBus->setPatch(iChannel, sInstrumentName,
			iBankSelMethod, iBank, iProg);
	}
	
	// Flag that it changed anyhow!
	changed();
}


// Select custom track foreground color.
void qtractorTrackForm::selectForegroundColor (void)
{
	QColor color = QColorDialog::getColor(
		colorItem(m_ui.ForegroundColorComboBox), this);
	if (color.isValid()) {
		updateColorItem(m_ui.ForegroundColorComboBox, color);
		changed();
	}
}


// Select custom track background color.
void qtractorTrackForm::selectBackgroundColor (void)
{
	QColor color = QColorDialog::getColor(
		colorItem(m_ui.BackgroundColorComboBox), this);
	if (color.isValid()) {
		updateColorItem(m_ui.BackgroundColorComboBox, color);
		changed();
	}
}


// end of qtractorTrackForm.cpp
