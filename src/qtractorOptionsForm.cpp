// qtractorOptionsForm.cpp
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

#include "qtractorOptionsForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"

#include <QFontDialog>
#include <QMessageBox>
#include <QValidator>


//----------------------------------------------------------------------------
// qtractorOptionsForm -- UI wrapper form.

// Constructor.
qtractorOptionsForm::qtractorOptionsForm (
	QWidget *pParent, Qt::WFlags wflags ) : QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// No settings descriptor initially (the caller will set it).
	m_pOptions = NULL;

	// Initialize dirty control state.
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
	QObject::connect(m_ui.MessagesFontPushButton,
		SIGNAL(clicked()),
		SLOT(chooseMessagesFont()));
	QObject::connect(m_ui.MessagesLimitCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MessagesLimitLinesSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ConfirmRemoveCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.StdoutCaptureCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.CompletePathCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.PeakAutoRemoveCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TransportTimeCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MaxRecentFilesSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResampleTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
}


// Destructor.
qtractorOptionsForm::~qtractorOptionsForm (void)
{
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorOptionsForm::setOptions ( qtractorOptions *pOptions )
{
	// Set reference descriptor.
	m_pOptions = pOptions;

	// Load Display options...
	QFont font;
	// Messages font.
	if (m_pOptions->sMessagesFont.isEmpty()
		|| !font.fromString(m_pOptions->sMessagesFont))
		font = QFont("Monospace", 8);
	QPalette pal(m_ui.MessagesFontTextLabel->palette());
	pal.setColor(m_ui.MessagesFontTextLabel->backgroundRole(), Qt::white);
	m_ui.MessagesFontTextLabel->setPalette(pal);
	m_ui.MessagesFontTextLabel->setFont(font);
	m_ui.MessagesFontTextLabel->setText(
		font.family() + " " + QString::number(font.pointSize()));

	// Messages limit option.
	m_ui.MessagesLimitCheckBox->setChecked(m_pOptions->bMessagesLimit);
	m_ui.MessagesLimitLinesSpinBox->setValue(m_pOptions->iMessagesLimitLines);

	// Other options finally.
	m_ui.ConfirmRemoveCheckBox->setChecked(m_pOptions->bConfirmRemove);
	m_ui.StdoutCaptureCheckBox->setChecked(m_pOptions->bStdoutCapture);
	m_ui.CompletePathCheckBox->setChecked(m_pOptions->bCompletePath);
	m_ui.PeakAutoRemoveCheckBox->setChecked(m_pOptions->bPeakAutoRemove);
	m_ui.TransportTimeCheckBox->setChecked(m_pOptions->bTransportTime);
	m_ui.MaxRecentFilesSpinBox->setValue(m_pOptions->iMaxRecentFiles);
	m_ui.ResampleTypeComboBox->setCurrentIndex(m_pOptions->iResampleType);

#ifndef CONFIG_LIBSAMPLERATE
	m_ui.ResampleTypeTextLabel->setEnabled(false);
	m_ui.ResampleTypeComboBox->setEnabled(false);
#endif

	// Donw. Restart clean.
	m_iDirtyCount = 0;
	stabilizeForm();
}


// Retrieve the editing options, if the case arises.
qtractorOptions *qtractorOptionsForm::options (void) const
{
	return m_pOptions;
}


// Accept settings (OK button slot).
void qtractorOptionsForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Messages options...
		m_pOptions->sMessagesFont = m_ui.MessagesFontTextLabel->font().toString();
		m_pOptions->bMessagesLimit = m_ui.MessagesLimitCheckBox->isChecked();
		m_pOptions->iMessagesLimitLines = m_ui.MessagesLimitLinesSpinBox->value();
		// Other options...
		m_pOptions->bConfirmRemove  = m_ui.ConfirmRemoveCheckBox->isChecked();
		m_pOptions->bStdoutCapture  = m_ui.StdoutCaptureCheckBox->isChecked();
		m_pOptions->bCompletePath   = m_ui.CompletePathCheckBox->isChecked();
		m_pOptions->bPeakAutoRemove = m_ui.PeakAutoRemoveCheckBox->isChecked();
		m_pOptions->bTransportTime  = m_ui.TransportTimeCheckBox->isChecked();
		m_pOptions->iMaxRecentFiles = m_ui.MaxRecentFilesSpinBox->value();
		m_pOptions->iResampleType   = m_ui.ResampleTypeComboBox->currentIndex();
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorOptionsForm::reject (void)
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


// The messages font selection dialog.
void qtractorOptionsForm::chooseMessagesFont (void)
{
	bool  bOk  = false;
	QFont font = QFontDialog::getFont(&bOk,
		m_ui.MessagesFontTextLabel->font(), this);
	if (bOk) {
		m_ui.MessagesFontTextLabel->setFont(font);
		m_ui.MessagesFontTextLabel->setText(
			font.family() + " " + QString::number(font.pointSize()));
		changed();
	}
}


// Dirty up settings.
void qtractorOptionsForm::changed (void)
{
	m_iDirtyCount++;
	stabilizeForm();
}


// Stabilize current form state.
void qtractorOptionsForm::stabilizeForm (void)
{
	m_ui.MessagesLimitLinesSpinBox->setEnabled(
		m_ui.MessagesLimitCheckBox->isChecked());

	m_ui.OkPushButton->setEnabled(m_iDirtyCount > 0);
}


// end of qtractorOptionsForm.cpp

