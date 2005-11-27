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


// Kind of constructor.
void qtractorTrackForm::init (void)
{
	// No settings descriptor initially (the caller will set it).
	m_pInstruments = NULL;
	m_pTrack = NULL;

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
		m_pTrack->setMidiChannel(ChannelSpinBox->value() - 1);
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


// Make changes due to track name.
void qtractorTrackForm::trackNameChanged (void)
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

	m_iDirtyCount++;
	stabilizeForm();
}


// Make changes due to bus name.
void qtractorTrackForm::busNameChanged ( const QString& sBusName )
{
	m_iDirtyCount++;
	stabilizeForm();
}


// Make changes due to MIDI channel.
void qtractorTrackForm::channelChanged ( int iChannel )
{
	m_iDirtyCount++;
	stabilizeForm();
}


// Make changes due to MIDI instrument.
void qtractorTrackForm::instrumentChanged( const QString& sInstrumentName )
{
	m_iDirtyCount++;
	stabilizeForm();
}


// Make changes due to MIDI bank.
void qtractorTrackForm::bankChanged( int iBankIndex )
{
	m_iDirtyCount++;
	stabilizeForm();
}


// Make changes due to MIDI program.
void qtractorTrackForm::programChanged( int iProgramIndex )
{
	m_iDirtyCount++;
	stabilizeForm();
}


// end of qtractorTrackForm.ui.h
