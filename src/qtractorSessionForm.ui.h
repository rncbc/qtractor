// qtractorSessionForm.ui.h
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
#include "qtractorSession.h"

#include <qvalidator.h>
#include <qmessagebox.h>

#include <math.h>


// Kind of constructor.
void qtractorSessionForm::init (void)
{
	// No settings descriptor initially (the caller will set it).
	m_pSession = NULL;

	// Setup some specific validators.
	SampleRateComboBox->setValidator(new QIntValidator(SampleRateComboBox));

	// Initialize dirty control state.
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();
}


// Kind of destructor.
void qtractorSessionForm::destroy (void)
{
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorSessionForm::setSession ( qtractorSession *pSession )
{
	// Set reference descriptor.
	m_pSession = pSession;

	// Initialize dialog widgets...
	SessionNameLineEdit->setText(m_pSession->sessionName());
	DescriptionTextEdit->setText(m_pSession->description());
	// Time properties...
	SampleRateComboBox->setCurrentText(QString::number(m_pSession->sampleRate()));
	TempoSpinBox->setValue(int(::ceil(m_pSession->tempo())));
	BeatsPerBarSpinBox->setValue(int(m_pSession->beatsPerBar()));
	TicksPerBeatSpinBox->setValue(int(m_pSession->ticksPerBeat()));
	// View properties...
	SnapPerBeatSpinBox->setValue(int(m_pSession->snapPerBeat()));
	PixelsPerBeatSpinBox->setValue(int(m_pSession->pixelsPerBeat()));
	HorizontalZoomSpinBox->setValue(int(m_pSession->horizontalZoom()));
	VerticalZoomSpinBox->setValue(int(m_pSession->verticalZoom()));
	// Backup clean.
	m_iDirtyCount = 0;

	// Done.
	stabilizeForm();
}


// Retrieve the editing session, if the case arises.
qtractorSession *qtractorSessionForm::session (void)
{
	return m_pSession;
}


// Accept settings (OK button slot).
void qtractorSessionForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Make changes permanent...
		m_pSession->setSessionName(SessionNameLineEdit->text());
		m_pSession->setDescription(DescriptionTextEdit->text());
		// Time properties...
		m_pSession->setSampleRate(SampleRateComboBox->currentText().toUInt());
		m_pSession->setTempo(float(TempoSpinBox->value()));
		m_pSession->setBeatsPerBar(BeatsPerBarSpinBox->value());
		m_pSession->setTicksPerBeat(TicksPerBeatSpinBox->value());
		// View properties...
		m_pSession->setSnapPerBeat(SnapPerBeatSpinBox->value());
		m_pSession->setPixelsPerBeat(PixelsPerBeatSpinBox->value());
		m_pSession->setHorizontalZoom(HorizontalZoomSpinBox->value());
		m_pSession->setVerticalZoom(VerticalZoomSpinBox->value());
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorSessionForm::reject (void)
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


// Dirty up settings.
void qtractorSessionForm::changed (void)
{
	m_iDirtyCount++;
	stabilizeForm();
}


// Stabilize current form state.
void qtractorSessionForm::stabilizeForm (void)
{
	bool bValid = (m_iDirtyCount > 0);
	bValid = bValid && !SessionNameLineEdit->text().isEmpty();
	bValid = bValid && !DescriptionTextEdit->text().isEmpty();
	OkPushButton->setEnabled(bValid);
}


// end of qtractorSessionForm.ui.h
