// qtractorTrackForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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
	m_pInstruments = NULL;
	m_pTrack = NULL;

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

	// Initialize dirty control state.
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();
}


// Kind of destructor.
void qtractorTrackForm::destroy (void)
{
}


// Instrument list accessors.
void qtractorTrackForm::setInstruments (
	qtractorInstrumentList *pInstruments )
{
	m_pInstruments = pInstruments;
	
	updateInstruments();
}


qtractorInstrumentList *qtractorTrackForm::instruments (void)
{
	return m_pInstruments;
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorTrackForm::setTrack ( qtractorTrack *pTrack )
{
	// Set reference descriptor.
	m_pTrack = pTrack;

	// Initialize dialog widgets...
	TrackNameTextEdit->setText(m_pTrack->trackName());
	int iTrackType = 0;
	switch (m_pTrack->trackType()) {
		case qtractorTrack::Audio:
			iTrackType = 0;
			break;
		case qtractorTrack::Midi:
			iTrackType = 1;
			break;
		default:
			break;
	}
	TrackTypeGroup->setButton(iTrackType);
	trackTypeChanged(iTrackType);
	if (!m_pTrack->busName().isEmpty())
		BusNameComboBox->setCurrentText(m_pTrack->busName());
	ChannelSpinBox->setValue(m_pTrack->midiChannel() + 1);

	// Make dependant widgets get real...
	updatePrograms(InstrumentComboBox->currentText(),
		m_pTrack->midiBank(), m_pTrack->midiProgram());

	// Update colors...
	updateColorItem(ForegroundColorComboBox, m_pTrack->foreground());
	updateColorItem(BackgroundColorComboBox, m_pTrack->background());

	// Backup clean.
	m_iDirtyCount = 0;

	// Cannot change track type, if track has clips already...
	bool bEnabled = (m_pTrack->clips().count() == 0);
	AudioRadioButton->setEnabled(bEnabled);
	MidiRadioButton->setEnabled(bEnabled);

	// Done.
	stabilizeForm();
}


// Retrieve the editing track, if the case arises.
qtractorTrack *qtractorTrackForm::track (void)
{
	return m_pTrack;
}


// Accept settings (OK button slot).
void qtractorTrackForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Make changes permanent...
		m_pTrack->setTrackName(TrackNameTextEdit->text());
		switch (TrackTypeGroup->id(TrackTypeGroup->selected())) {
		case 0: // Audio track...
			m_pTrack->setTrackType(qtractorTrack::Audio);
			break;
		case 1: // Midi track...
			m_pTrack->setTrackType(qtractorTrack::Midi);
			break;
		}
		m_pTrack->setBusName(BusNameComboBox->currentText());
		// Special case for MIDI settings...
		unsigned short iChannel = ChannelSpinBox->value() - 1;
		m_pTrack->setMidiChannel(iChannel);
		qtractorMidiBus *pMidiBus = midiBus();
		if (pMidiBus) {
			qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
			patch.name = InstrumentComboBox->currentText();
			patch.bank = m_banks[BankComboBox->currentItem()];
			patch.prog = m_progs[ProgComboBox->currentItem()];
			m_pTrack->setMidiBank(patch.bank);
			m_pTrack->setMidiProgram(patch.prog);
		}
		// View colors...
		m_pTrack->setForeground(colorItem(ForegroundColorComboBox));
		m_pTrack->setBackground(colorItem(BackgroundColorComboBox));
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

	if (bReject)
		QDialog::reject();
}


// Stabilize current form state.
void qtractorTrackForm::stabilizeForm (void)
{
	bool bValid = (m_iDirtyCount > 0);
	bValid = bValid && !TrackNameTextEdit->text().isEmpty();
	OkPushButton->setEnabled(bValid);
}


// Retrieve currently assigned MIDI bus, if applicable.
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
	const QString& sBusName = BusNameComboBox->currentText();
	return static_cast<qtractorMidiBus *> (pMidiEngine->findBus(sBusName));
}


// Refresh instrument list.
void qtractorTrackForm::updateInstruments (void)
{
	if (m_pInstruments == NULL)
		return;

	InstrumentComboBox->clear();
	const QPixmap& pixmap = QPixmap::fromMimeSource("itemInstrument.png");
	for (qtractorInstrumentList::Iterator iter = m_pInstruments->begin();
			iter != m_pInstruments->end(); ++iter) {
		InstrumentComboBox->insertItem(pixmap, iter.data().instrumentName());
	}
}


// Refresh channel instrument banks list.
void qtractorTrackForm::updateChannel ( int iChannel )
{
	// Regular channel offset
	if (--iChannel < 0)
	    return;

	// MIDI bus...
	qtractorMidiBus *pMidiBus = midiBus();
	if (pMidiBus == NULL)
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updateChannel(%d)\n", iChannel);
#endif

	// MIDI channel patch...
	const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);

	// Select instrument...
	int iInstrumentIndex = 0;
	QListBoxItem *pItem	= InstrumentComboBox->listBox()->findItem(
		patch.name, Qt::ExactMatch | Qt::CaseSensitive);
	if (pItem)
		iInstrumentIndex = InstrumentComboBox->listBox()->index(pItem);
	InstrumentComboBox->setCurrentItem(iInstrumentIndex);

	// Update instrument, bank and program...
	updateBanks(InstrumentComboBox->currentText(), patch.bank, patch.prog);
}


// Refresh instrument banks list.
void qtractorTrackForm::updateBanks ( const QString& sInstrumentName,
	int iBank, int iProg )
{
	if (m_pTrack == NULL)
		return;
	if (m_pInstruments == NULL)
		return;
	if (sInstrumentName.isEmpty())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updateBanks(\"%s\", %d, %d)\n",
		sInstrumentName.latin1(), iBank, iProg);
#endif

	// Instrument reference...
	qtractorInstrument& instr = (*m_pInstruments)[sInstrumentName];

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
	if (m_pInstruments == NULL)
		return;
	if (sInstrumentName.isEmpty())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackForm::updatePrograms(\"%s\", %d, %d)\n",
		sInstrumentName.latin1(), iBank, iProg);
#endif

	// Instrument reference...
	qtractorInstrument& instr = (*m_pInstruments)[sInstrumentName];

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
			if (it.key() >= 0) {
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
				QString::number(iProgIndex + 1) + " --");
			m_progs[iProgIndex] = iProgIndex;
		}
	}

	// Select program...
	iProgIndex = 0;
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


// Make changes due to track name.
void qtractorTrackForm::changed (void)
{
	m_iDirtyCount++;
	stabilizeForm();
}


// Make changes due to track type.
void qtractorTrackForm::trackTypeChanged ( int iTrackType )
{
	// Make changes due to track type change.
	qtractorEngine *pEngine = NULL;
	switch (iTrackType) {
	case 0: // Audio track...
		pEngine = m_pTrack->session()->audioEngine();
		MidiGroupBox->setEnabled(false);
		break;
	case 1: // Midi track...
		pEngine = m_pTrack->session()->midiEngine();
		MidiGroupBox->setEnabled(true);
		break;
	}

	BusNameComboBox->clear();
	if (pEngine) {
		for (qtractorBus *pBus = pEngine->busses().first();
				pBus; pBus = pBus->next()) {
			BusNameComboBox->insertItem(pBus->busName());
		}
	}

	busNameChanged(BusNameComboBox->currentText());
}


// Make changes due to bus name.
void qtractorTrackForm::busNameChanged ( const QString& /* sBusName */ )
{
	channelChanged(ChannelSpinBox->value());
}


// Make changes due to MIDI channel.
void qtractorTrackForm::channelChanged ( int iChannel )
{
	updateChannel(iChannel);

	progChanged(ProgComboBox->currentItem());
}


// Make changes due to MIDI instrument.
void qtractorTrackForm::instrumentChanged ( const QString& sInstrumentName )
{
	updateBanks(sInstrumentName, m_banks[BankComboBox->currentItem()],
		m_progs[ProgComboBox->currentItem()]);

	progChanged(ProgComboBox->currentItem());
}


// Make changes due to MIDI bank.
void qtractorTrackForm::bankChanged ( int iBankIndex )
{
	updatePrograms(InstrumentComboBox->currentText(), m_banks[iBankIndex],
		m_progs[ProgComboBox->currentItem()]);

	progChanged(ProgComboBox->currentItem());
}


// Make changes due to MIDI program.
void qtractorTrackForm::progChanged( int iProgIndex )
{
	qtractorMidiBus *pMidiBus = midiBus();
	if (pMidiBus == NULL)
	    return;

	// Patch parameters...
	unsigned short iChannel = ChannelSpinBox->value() - 1;
	const QString& sInstrumentName = InstrumentComboBox->currentText();
	int iBank = m_banks[BankComboBox->currentItem()];
	int iProg = m_progs[iProgIndex];
	int iBankSelMethod = 0;
	if (!sInstrumentName.isEmpty())
		iBankSelMethod = (*m_pInstruments)[sInstrumentName].bankSelMethod();

	// Patch it directly...
	pMidiBus->setPatch(iChannel, sInstrumentName, iBank, iProg, iBankSelMethod);
	
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
