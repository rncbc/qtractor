// qtractorSessionForm.cpp
//
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorSessionForm.h"


#include "qtractorAbout.h"
#include "qtractorSession.h"

#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QValidator>
#include <QLineEdit>

#include <math.h>


//----------------------------------------------------------------------------
// qtractorSessionForm -- UI wrapper form.

// Constructor.
qtractorSessionForm::qtractorSessionForm (
	QWidget *pParent, Qt::WFlags wflags ) : QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Initialize conveniency options...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions)
			pOptions->loadComboBoxHistory(m_ui.SessionDirComboBox);
	}

	// Setup some specific validators.
	m_ui.SampleRateComboBox->setValidator(
		new QIntValidator(m_ui.SampleRateComboBox));

	// Initialize dirty control state.
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.SessionNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionDirComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionDirToolButton,
		SIGNAL(clicked()),
		SLOT(browseSessionDir()));
	QObject::connect(m_ui.DescriptionTextEdit,
		SIGNAL(textChanged()),
		SLOT(changed()));
	QObject::connect(m_ui.SampleRateComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.BeatsPerBarSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TicksPerBeatSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SnapPerBeatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.PixelsPerBeatSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.HorizontalZoomSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.VerticalZoomSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qtractorSessionForm::~qtractorSessionForm (void)
{
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorSessionForm::setSession ( qtractorSession *pSession )
{
	// Session properties cloning...
	m_props = pSession->properties();

	// Initialize dialog widgets...
	m_ui.SessionNameLineEdit->setText(m_props.sessionName);
	m_ui.SessionDirComboBox->lineEdit()->setText(m_props.sessionDir);
	m_ui.DescriptionTextEdit->setPlainText(m_props.description);
	// Time properties...
	m_ui.SampleRateComboBox->lineEdit()->setText(
		QString::number(m_props.sampleRate));
	m_ui.SampleRateTextLabel->setEnabled(!pSession->isActivated());
	m_ui.SampleRateComboBox->setEnabled(!pSession->isActivated());
	m_ui.TempoSpinBox->setValue(m_props.tempo);
	m_ui.BeatsPerBarSpinBox->setValue(int(m_props.beatsPerBar));
	m_ui.TicksPerBeatSpinBox->setValue(int(m_props.ticksPerBeat));
	// View properties...
	m_ui.SnapPerBeatComboBox->setCurrentIndex(
		qtractorSession::indexFromSnap(m_props.snapPerBeat));
	m_ui.PixelsPerBeatSpinBox->setValue(int(m_props.pixelsPerBeat));
	m_ui.HorizontalZoomSpinBox->setValue(int(m_props.horizontalZoom));
	m_ui.VerticalZoomSpinBox->setValue(int(m_props.verticalZoom));

	// Start editing session name, if empty...
	if (m_props.sessionName.isEmpty())
		m_ui.SessionNameLineEdit->setFocus();

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
		m_props.sessionName    = m_ui.SessionNameLineEdit->text();
		m_props.sessionDir     = m_ui.SessionDirComboBox->currentText();
		m_props.description    = m_ui.DescriptionTextEdit->toPlainText();
		// Time properties...
		m_props.sampleRate     = m_ui.SampleRateComboBox->currentText().toUInt();
		m_props.tempo          = m_ui.TempoSpinBox->value();
		m_props.beatsPerBar    = m_ui.BeatsPerBarSpinBox->value();
		m_props.ticksPerBeat   = m_ui.TicksPerBeatSpinBox->value();
		// View properties...
		m_props.snapPerBeat    = qtractorSession::snapFromIndex(
			m_ui.SnapPerBeatComboBox->currentIndex());
		m_props.pixelsPerBeat  = m_ui.PixelsPerBeatSpinBox->value();
		m_props.horizontalZoom = m_ui.HorizontalZoomSpinBox->value();
		m_props.verticalZoom   = m_ui.VerticalZoomSpinBox->value();
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Save other conveniency options...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions)
			pOptions->saveComboBoxHistory(m_ui.SessionDirComboBox);
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
	bValid = bValid && !m_ui.SessionNameLineEdit->text().isEmpty();
	bValid = bValid && QDir(m_ui.SessionDirComboBox->currentText()).exists();
//	bValid = bValid && !m_ui.DescriptionTextEdit->text().isEmpty();
	m_ui.OkPushButton->setEnabled(bValid);
}


// Browse for session directory.
void qtractorSessionForm::browseSessionDir (void)
{
    QString sSessionDir = QFileDialog::getExistingDirectory(
		this,                                  // Parent.
		tr("Session Directory:"),              // Caption.
		m_ui.SessionDirComboBox->currentText() // Start here.
    );

    if (!sSessionDir.isEmpty()) {
        m_ui.SessionDirComboBox->lineEdit()->setText(sSessionDir);
        m_ui.SessionDirComboBox->setFocus();
        changed();
    }
}


// end of qtractorSessionForm.cpp
