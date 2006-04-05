// qtractorSessionForm.ui.h
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
#include "qtractorSession.h"

#include <qvalidator.h>
#include <qmessagebox.h>
#include <qfiledialog.h>

#include <math.h>


// Kind of constructor.
void qtractorSessionForm::init (void)
{
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
	// Session properties cloning...
	m_props = pSession->properties();

	// Initialize dialog widgets...
	SessionNameLineEdit->setText(m_props.sessionName);
	SessionDirComboBox->setCurrentText(m_props.sessionDir);
	DescriptionTextEdit->setText(m_props.description);
	// Time properties...
	SampleRateComboBox->setCurrentText(QString::number(m_props.sampleRate));
	SampleRateComboBox->setEnabled(!pSession->isActivated());
	TempoSpinBox->setValueFloat(m_props.tempo);
	BeatsPerBarSpinBox->setValue(int(m_props.beatsPerBar));
	TicksPerBeatSpinBox->setValue(int(m_props.ticksPerBeat));
	// View properties...
	SnapPerBeatComboBox->setCurrentItem(
		qtractorSession::indexFromSnap(m_props.snapPerBeat));
	PixelsPerBeatSpinBox->setValue(int(m_props.pixelsPerBeat));
	HorizontalZoomSpinBox->setValue(int(m_props.horizontalZoom));
	VerticalZoomSpinBox->setValue(int(m_props.verticalZoom));
	// Backup clean.
	m_iDirtyCount = 0;

	// Done.
	stabilizeForm();
}


// Retrieve the accepted session properties, if the case arises.
const qtractorSession::Properties& qtractorSessionForm::properties (void)
{
	return m_props;
}


// Accept settings (OK button slot).
void qtractorSessionForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Make changes permanent...
		m_props.sessionName    = SessionNameLineEdit->text();
		m_props.sessionDir     = SessionDirComboBox->currentText();
		m_props.description    = DescriptionTextEdit->text();
		// Time properties...
		m_props.sampleRate     = SampleRateComboBox->currentText().toUInt();
		m_props.tempo          = TempoSpinBox->valueFloat();
		m_props.beatsPerBar    = BeatsPerBarSpinBox->value();
		m_props.ticksPerBeat   = TicksPerBeatSpinBox->value();
		// View properties...
		m_props.snapPerBeat    = qtractorSession::snapFromIndex(
			SnapPerBeatComboBox->currentItem());
		m_props.pixelsPerBeat  = PixelsPerBeatSpinBox->value();
		m_props.horizontalZoom = HorizontalZoomSpinBox->value();
		m_props.verticalZoom   = VerticalZoomSpinBox->value();
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
	bValid = bValid && QDir(SessionDirComboBox->currentText()).exists();
//	bValid = bValid && !DescriptionTextEdit->text().isEmpty();
	OkPushButton->setEnabled(bValid);
}


// Browse for session directory.
void qtractorSessionForm::browseSessionDir (void)
{
    QString sSessionDir = QFileDialog::getExistingDirectory(
		SessionDirComboBox->currentText(),	   // Start here.
		this, 0,                               // Parent and name (none)
		tr("Session Directory:")               // Caption.
    );

    if (!sSessionDir.isEmpty()) {
        SessionDirComboBox->setCurrentText(sSessionDir);
        SessionDirComboBox->setFocus();
        changed();
    }
}


// end of qtractorSessionForm.ui.h
