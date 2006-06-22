// qtractorTrackForm.ui.h
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
#include "qtractorTrack.h"
#include "qtractorSession.h"
#include "qtractorInstrument.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"
#include "qtractorBusForm.h"

#include <qmessagebox.h>
#include <qcolordialog.h>
#include <qlistbox.h>
#include <qpainter.h>
#include <qstyle.h>


//----------------------------------------------------------------------
// class qtractorColorItem -- Custom color listbox item.
//

class qtractorColorItem : public QListBoxItem
{
public:

	// Constructor.
	qtractorColorItem ( const QColor& color )
		: QListBoxItem(), m_color(color) { setCustomHighlighting(true); }

	// Color accessors.
	void setColor(const QColor& color) { m_color = color; }
	const QColor& color() const { return m_color; }

protected:

	// Custom paint method.
	void paint(QPainter *pPainter);

	// Default item extents
	int width  (const QListBox*) const { return 32; }
	int height (const QListBox*) const { return 16; }

private:

	// Item color spec.
	QColor m_color;
};


// ListBox item custom highlighting method.
void qtractorColorItem::paint ( QPainter *pPainter )
{
	// Evil trick: find out whether we are painted onto our listbox...
	QListBox *pListBox = listBox();
	int w = pListBox->viewport()->width();
	int h = height(pListBox);

	QRect rect(0, 0, w, h);
	if (isSelected())
		pPainter->eraseRect(rect);

	pPainter->fillRect(1, 1, w - 2, h - 2, m_color);
}


// Kind of constructor.
void qtractorTrackForm::init (void)
{
	// No settings descriptor initially (the caller will set it).
	m_pMainForm = NULL;
	m_pTrack = NULL;

	// Bank select methods.
	const QPixmap& pixmap = QPixmap::fromMimeSource("itemProperty.png");
	BankSelMethodComboBox->clear();
	BankSelMethodComboBox->insertItem(pixmap, tr("Normal"));
	BankSelMethodComboBox->insertItem(pixmap, tr("Bank MSB"));
	BankSelMethodComboBox->insertItem(pixmap, tr("Bank LSB"));
	BankSelMethodComboBox->insertItem(pixmap, tr("Patch"));

	// Custom colors.
	ForegroundColorComboBox->clear();
	BackgroundColorComboBox->clear();
	for (int i = 1; i < 28; i++) {
		const QColor color = qtractorTrack::trackColor(i);
		ForegroundColorComboBox->listBox()->insertItem(
			new qtractorColorItem(color.dark()));
		BackgroundColorComboBox->listBox()->insertItem(
			new qtractorColorItem(color));
	}

	// To save and keep bus/channel patching consistency.
    m_pOldMidiBus = NULL;
    m_iOldChannel = -1;
    m_sOldInstrumentName = QString::null;
    m_iOldBankSelMethod  = -1;
    m_iOldBank = -1;
    m_iOldProg = -1;

	// Initialize dirty control state.
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();
}


// Kind of destructor.
void qtractorTrackForm::destroy (void)
{
}


// Main form accessors.
void qtractorTrackForm::setMainForm ( qtractorMainForm *pMainForm )
{
	m_pMainForm = pMainForm;
	
	updateInstruments();
}


qtractorMainForm *qtractorTrackForm::mainForm (void)
{
	return m_pMainForm;
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorTrackForm::setTrack ( qtractorTrack *pTrack )
{
	// Set reference descriptor.
	m_pTrack = pTrack;

    // Avoid dirty this all up.
    m_iDirtySetup++;

	// Track properties cloning...
	m_props = pTrack->properties();

	// Initialize dialog widgets...
	TrackNameTextEdit->setText(m_props.trackName);
	qtractorEngine *pEngine = NULL;
	int iTrackType = -1;
	switch (m_props.trackType) {
		case qtractorTrack::Audio:
			pEngine = pTrack->session()->audioEngine();
			iTrackType = 0;
			break;
		case qtractorTrack::Midi:
			pEngine = pTrack->session()->midiEngine();
			iTrackType = 1;
			break;
		default:
			break;
	}
	TrackTypeGroup->setButton(iTrackType);
	updateTrackType(iTrackType);

	if (pEngine && pEngine->findBus(m_props.inputBusName))
		InputBusNameComboBox->setCurrentText(m_props.inputBusName);
	if (pEngine && pEngine->findBus(m_props.outputBusName))
		OutputBusNameComboBox->setCurrentText(m_props.outputBusName);

	ChannelSpinBox->setValue(m_props.midiChannel + 1);
	updateChannel(ChannelSpinBox->value(),
		m_props.midiBankSelMethod, m_props.midiBank, m_props.midiProgram);

	// Update colors...
	updateColorItem(ForegroundColorComboBox, m_props.foreground);
	updateColorItem(BackgroundColorComboBox, m_props.background);

	// Cannot change track type, if track has clips already...
	bool bEnabled = (pTrack->clips().count() == 0);
	AudioRadioButton->setEnabled(bEnabled);
	MidiRadioButton->setEnabled(bEnabled);

	// Backup clean.
	m_iDirtyCount = 0;
	m_iDirtySetup--;

	// Done.
	stabilizeForm();
}


// Retrieve the editing track, if the case arises.
qtractorTrack *qtractorTrackForm::track (void)
{
	return m_pTrack;
}


// Retrieve the accepted track properties, if the case arises.
const qtractorTrack::Properties& qtractorTrackForm::properties (void)
{
	return m_props;
}


// Accept settings (OK button slot).
void qtractorTrackForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Make changes permanent...
		m_props.trackName = TrackNameTextEdit->text();
		switch (TrackTypeGroup->id(TrackTypeGroup->selected())) {
		case 0: // Audio track...
			m_props.trackType = qtractorTrack::Audio;
			break;
		case 1: // Midi track...
			m_props.trackType = qtractorTrack::Midi;
			break;
		}
		m_props.inputBusName  = InputBusNameComboBox->currentText();
		m_props.outputBusName = OutputBusNameComboBox->currentText();
		// Special case for MIDI settings...
		m_props.midiChannel = (ChannelSpinBox->value() - 1);
		m_props.midiBankSelMethod = BankSelMethodComboBox->currentItem();
		m_props.midiBank    = m_banks[BankComboBox->currentItem()];
		m_props.midiProgram = m_progs[ProgComboBox->currentItem()];
		// View colors...
		m_props.foreground = colorItem(ForegroundColorComboBox);
		m_props.background = colorItem(BackgroundColorComboBox);
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
	bValid = bValid && !TrackNameTextEdit->text().isEmpty();
	bValid = bValid && TrackTypeGroup->selected();
	OkPushButton->setEnabled(bValid);
}


// Retrieve currently assigned MIDI output-bus, if applicable.
qtractorMidiBus *qtractorTrackForm::midiBus (void)
{
	if (m_pTrack == NULL)
		return NULL;

	// If it ain't MIDI, bail out...
	if (TrackTypeGroup->id(TrackTypeGroup->selected()) != 1)
		return NULL;

	// MIDI engine...
	qtractorMidiEngine *pMidiEngine = m_pTrack->session()->midiEngine();
	if (pMidiEngine == NULL)
		return NULL;

	// MIDI bus...
	const QString& sBusName = OutputBusNameComboBox->currentText();
	return static_cast<qtractorMidiBus *> (pMidiEngine->findBus(sBusName));
}


// Refresh instrument list.
void qtractorTrackForm::updateInstruments (void)
{
	if (m_pMainForm == NULL)
		return;

	qtractorInstrumentList *pInstruments = m_pMainForm->instruments();
	if (pInstruments == NULL)
		return;

	InstrumentComboBox->clear();
	const QPixmap& pixmap = QPixmap::fromMimeSource("itemInstrument.png");
	for (qtractorInstrumentList::Iterator iter = pInstruments->begin();
			iter != pInstruments->end(); ++iter) {
		InstrumentComboBox->insertItem(pixmap, iter.data().instrumentName());
	}
}


// Update track type and busses.
void qtractorTrackForm::updateTrackType ( int iTrackType )
{
	// Make changes due to track type change.
	qtractorEngine *pEngine = NULL;
	QPixmap pixmap;
	switch (iTrackType) {
	case 0: // Audio track...
		pEngine = m_pTrack->session()->audioEngine();
		pixmap = QPixmap::fromMimeSource("trackAudio.png");
		MidiGroupBox->setEnabled(false);
		InputBusNameComboBox->setEnabled(true);
		OutputBusNameComboBox->setEnabled(true);
		break;
	case 1: // Midi track...
		pEngine = m_pTrack->session()->midiEngine();
		pixmap = QPixmap::fromMimeSource("trackMidi.png");
		MidiGroupBox->setEnabled(true);
		InputBusNameComboBox->setEnabled(true);
		OutputBusNameComboBox->setEnabled(true);
		break;
	default:
		MidiGroupBox->setEnabled(false);
		InputBusNameComboBox->setEnabled(false);
		OutputBusNameComboBox->setEnabled(false);
		break;
	}

	InputBusNameComboBox->clear();
	OutputBusNameComboBox->clear();
	if (pEngine) {
		for (qtractorBus *pBus = pEngine->busses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Input)
				InputBusNameComboBox->insertItem(pixmap, pBus->busName());
			if (pBus->busMode() & qtractorBus::Output)
				OutputBusNameComboBox->insertItem(pixmap, pBus->busName());
		}
	}
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

	// MIDI channel patch...
	const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
	if (iBankSelMethod < 0)
		iBankSelMethod = patch.bankSelMethod;
	if (iBank < 0)
		iBank = patch.bank;
	if (iProg < 0)
		iProg = patch.prog;

	// Select instrument...
	int iInstrumentIndex = 0;
	QListBoxItem *pItem = InstrumentComboBox->listBox()->findItem(
		patch.instrumentName, Qt::ExactMatch | Qt::CaseSensitive);
	if (pItem)
		iInstrumentIndex = InstrumentComboBox->listBox()->index(pItem);
	InstrumentComboBox->setCurrentItem(iInstrumentIndex);

	// Go and update the bank and program listings...
	updateBanks(InstrumentComboBox->currentText(),
		iBankSelMethod, iBank, iProg);
}


// Refresh instrument banks list.
void qtractorTrackForm::updateBanks ( const QString& sInstrumentName,
	int iBankSelMethod, int iBank, int iProg )
{
	if (m_pTrack == NULL)
		return;
	if (m_pMainForm == NULL)
		return;
	if (sInstrumentName.isEmpty())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updateBanks(\"%s\", %d, %d, %d)\n",
		sInstrumentName.latin1(), iBankSelMethod, iBank, iProg);
#endif

	qtractorInstrumentList *pInstruments = m_pMainForm->instruments();
	if (pInstruments == NULL)
		return;

	// Instrument reference...
	qtractorInstrument& instr = (*pInstruments)[sInstrumentName];

	// Bank selection method...
	if (iBankSelMethod < 0)
		iBankSelMethod = instr.bankSelMethod();
	BankSelMethodComboBox->setCurrentItem(iBankSelMethod);

	// Refresh patch bank mapping...
	m_banks.clear();
	int iBankIndex = 0;
	const QPixmap& pixmap = QPixmap::fromMimeSource("itemPatches.png");
	BankComboBox->clear();
	qtractorInstrumentPatches::Iterator it;
	for (it = instr.patches().begin(); it != instr.patches().end(); ++it) {
		if (it.key() >= 0) {
			BankComboBox->insertItem(pixmap, it.data().name());
			m_banks[iBankIndex++] = it.key();
		}
	}

	// In case bank address is generic...
	if (BankComboBox->count() < 1) {
		qtractorInstrumentData& patch = instr.patch(iBank);
		if (!patch.name().isEmpty()) {
			BankComboBox->insertItem(pixmap, patch.name());
			m_banks[iBankIndex] = iBank;
		}
	}

	// Do the proper bank selection...
	qtractorInstrumentData& bank = instr.patch(iBank);

	// Select bank...
	iBankIndex = 0;
	QListBoxItem *pItem	= BankComboBox->listBox()->findItem(
		bank.name(), Qt::ExactMatch | Qt::CaseSensitive);
	if (pItem)
		iBankIndex = BankComboBox->listBox()->index(pItem);
	BankComboBox->setCurrentItem(iBankIndex);

	// And update the bank and program listing...
	updatePrograms(sInstrumentName, m_banks[iBankIndex], iProg);
}


// Refresh bank programs list.
void qtractorTrackForm::updatePrograms (  const QString& sInstrumentName,
	int iBank, int iProg )
{
	if (m_pTrack == NULL)
		return;
	if (m_pMainForm == NULL)
		return;
	if (sInstrumentName.isEmpty())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updatePrograms(\"%s\", %d, %d)\n",
		sInstrumentName.latin1(), iBank, iProg);
#endif

	qtractorInstrumentList *pInstruments = m_pMainForm->instruments();
	if (pInstruments == NULL)
		return;

	// Instrument reference...
	qtractorInstrument& instr = (*pInstruments)[sInstrumentName];

	// Bank reference...
	qtractorInstrumentData& bank = instr.patch(iBank);

	// Refresh patch program mapping...
	m_progs.clear();
	int iProgIndex = 0;
	const QPixmap& pixmap = QPixmap::fromMimeSource("itemChannel.png");
	ProgComboBox->clear();
	if (bank.count() > 0) {
		// Enumerate the explicit given program list...
		qtractorInstrumentData::Iterator it;
		for (it = bank.begin(); it != bank.end(); ++it) {
			if (it.key() >= 0 && !it.data().isEmpty()) {
				ProgComboBox->insertItem(pixmap, it.data());
				m_progs[iProgIndex++] = it.key();
			}
		}
	}

	// In case program address is generic...
	if (ProgComboBox->count() < 1) {
		// Just make a generic program list...
		for (iProgIndex = 0; iProgIndex < 128; iProgIndex++) {
			ProgComboBox->insertItem(pixmap,
				QString::number(iProgIndex + 1) + "  - -");
			m_progs[iProgIndex] = iProgIndex;
		}
	}

	// Select program...
	iProgIndex = iProg;
	if (bank.contains(iProg)) {
		QListBoxItem *pItem	= ProgComboBox->listBox()->findItem(
			bank[iProg], Qt::ExactMatch | Qt::CaseSensitive);
		if (pItem)
			iProgIndex = ProgComboBox->listBox()->index(pItem);
	}
	ProgComboBox->setCurrentItem(iProgIndex);
}


// Update and set a color item.
void qtractorTrackForm::updateColorItem ( QComboBox *pComboBox,
	const QColor& color )
{
	// Check if already exists...
	int iItem = 0;
	for ( ; iItem < pComboBox->count(); iItem++) {
		qtractorColorItem *pItem
			= static_cast<qtractorColorItem *> (pComboBox->listBox()->item(iItem));
		if (pItem->color() == color) {
			pComboBox->setCurrentItem(iItem);
			return;
		}
	}
	// Nope, we'll add it custom...
	pComboBox->listBox()->insertItem(new qtractorColorItem(color));
	pComboBox->setCurrentItem(iItem);
}


// Retreieve currently selected color item.
const QColor& qtractorTrackForm::colorItem ( QComboBox *pComboBox )
{
	int iItem = pComboBox->currentItem();
	qtractorColorItem *pItem
		= static_cast<qtractorColorItem *> (pComboBox->listBox()->item(iItem));
	return pItem->color();
}


// Make changes due.
void qtractorTrackForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	m_iDirtyCount++;
	stabilizeForm();
}


// Make changes due to track type.
void qtractorTrackForm::trackTypeChanged ( int iTrackType )
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
		m_sOldInstrumentName = QString::null;
		m_iOldBankSelMethod = -1;
		m_iOldBank = -1;
		m_iOldProg = -1;
	}

	updateTrackType(iTrackType);
//	inputBusNameChanged(InputBusNameComboBox->currentText());
	outputBusNameChanged(OutputBusNameComboBox->currentText());
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

	channelChanged(ChannelSpinBox->value());
}


// Manage busses.
void qtractorTrackForm::busNameClicked (void)
{
	if (m_iDirtySetup > 0)
		return;

	// Depending on track type...
	qtractorEngine *pEngine = NULL;
	int iTrackType = TrackTypeGroup->id(TrackTypeGroup->selected());
	switch (iTrackType) {
	case 0: // Audio track...
		pEngine = m_pTrack->session()->audioEngine();
		break;
	case 1: // Midi track...
		pEngine = m_pTrack->session()->midiEngine();
		break;
	}

	// Call here the bus management form.
	qtractorBusForm busForm(this);
	busForm.setMainForm(m_pMainForm);
	// Pre-select bus...
	const QString& sBusName = OutputBusNameComboBox->currentText();
	if (pEngine && !sBusName.isEmpty())
		busForm.setBus(pEngine->findBus(sBusName));
	// Go for it...
	busForm.exec();

	// Check if any busses have changed...
	if (busForm.isDirty()) {
		// Try to preserve current selected names...
		const QString sInputBusName  = InputBusNameComboBox->currentText();
		const QString sOutputBusName = OutputBusNameComboBox->currentText();
		// Update the comboboxes...
		trackTypeChanged(iTrackType);
		// Restore old current selected ones...
		if (pEngine->findBus(sInputBusName))
			InputBusNameComboBox->setCurrentText(sInputBusName);
		if (pEngine->findBus(sOutputBusName))
			OutputBusNameComboBox->setCurrentText(sOutputBusName);
	}
}


// Make changes due to MIDI channel.
void qtractorTrackForm::channelChanged ( int iChannel )
{
	if (m_iDirtySetup > 0)
		return;

	// First update channel instrument mapping...
	updateChannel(iChannel,
		-1, // BankSelMethodComboBox->currentItem(),
		-1, // m_banks[BankComboBox->currentItem()],
		-1);// m_progs[ProgComboBox->currentItem()]);

	progChanged(ProgComboBox->currentItem());
}


// Make changes due to MIDI instrument.
void qtractorTrackForm::instrumentChanged ( const QString& sInstrumentName )
{
	if (m_iDirtySetup > 0)
		return;

	updateBanks(sInstrumentName,
		-1, // BankSelMethodComboBox->currentItem(),
		m_banks[BankComboBox->currentItem()],
		m_progs[ProgComboBox->currentItem()]);

	progChanged(ProgComboBox->currentItem());
}


// Make changes due to MIDI bank selection method.
void qtractorTrackForm::bankSelMethodChanged ( int /* iBankSelMethod */ )
{
	if (m_iDirtySetup > 0)
		return;

	progChanged(ProgComboBox->currentItem());
}


// Make changes due to MIDI bank.
void qtractorTrackForm::bankChanged ( int iBankIndex )
{
	if (m_iDirtySetup > 0)
		return;

	updatePrograms(InstrumentComboBox->currentText(), m_banks[iBankIndex],
		m_progs[ProgComboBox->currentItem()]);

	progChanged(ProgComboBox->currentItem());
}


// Make changes due to MIDI program.
void qtractorTrackForm::progChanged ( int iProgIndex )
{
	if (m_iDirtySetup > 0)
		return;

	// Of course, only applicable on MIDI tracks...
	qtractorMidiBus *pMidiBus = midiBus();
	if (pMidiBus) {
		// Patch parameters...
		unsigned short iChannel = ChannelSpinBox->value() - 1;
		const QString& sInstrumentName = InstrumentComboBox->currentText();
		int iBankSelMethod = BankSelMethodComboBox->currentItem();
		int iBank = m_banks[BankComboBox->currentItem()];
		int iProg = m_progs[iProgIndex];
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
		colorItem(ForegroundColorComboBox), this);

	if (color.isValid()) {
		updateColorItem(ForegroundColorComboBox, color);
		changed();
	}
}


// Select custom track background color.
void qtractorTrackForm::selectBackgroundColor (void)
{
	QColor color = QColorDialog::getColor(
		colorItem(BackgroundColorComboBox), this);

	if (color.isValid()) {
		updateColorItem(BackgroundColorComboBox, color);
		changed();
	}
}


// end of qtractorTrackForm.ui.h
