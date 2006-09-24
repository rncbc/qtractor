// qtractorOptionsForm.ui.h
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorOptions.h"

#include <qvalidator.h>
#include <qmessagebox.h>
#include <qfontdialog.h>


// Kind of constructor.
void qtractorOptionsForm::init (void)
{
	// No settings descriptor initially (the caller will set it).
	m_pOptions = NULL;

	// Initialize dirty control state.
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();
}


// Kind of destructor.
void qtractorOptionsForm::destroy (void)
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
	if (m_pOptions->sMessagesFont.isEmpty() || !font.fromString(m_pOptions->sMessagesFont))
		font = QFont("Fixed", 8);
	MessagesFontTextLabel->setFont(font);
	MessagesFontTextLabel->setText(font.family() + " " + QString::number(font.pointSize()));

	// Messages limit option.
	MessagesLimitCheckBox->setChecked(m_pOptions->bMessagesLimit);
	MessagesLimitLinesSpinBox->setValue(m_pOptions->iMessagesLimitLines);

	// Other options finally.
	ConfirmRemoveCheckBox->setChecked(m_pOptions->bConfirmRemove);
	StdoutCaptureCheckBox->setChecked(m_pOptions->bStdoutCapture);
	CompletePathCheckBox->setChecked(m_pOptions->bCompletePath);
	PeakAutoRemoveCheckBox->setChecked(m_pOptions->bPeakAutoRemove);
	TransportTimeCheckBox->setChecked(m_pOptions->bTransportTime);
	MaxRecentFilesSpinBox->setValue(m_pOptions->iMaxRecentFiles);
	ResampleTypeComboBox->setCurrentItem(m_pOptions->iResampleType);

#ifndef CONFIG_LIBSAMPLERATE
	ResampleTypeTextLabel->setEnabled(false);
	ResampleTypeComboBox->setEnabled(false);
#endif

	// Donw. Restart clean.
	m_iDirtyCount = 0;
	stabilizeForm();
}


// Retrieve the editing options, if the case arises.
qtractorOptions *qtractorOptionsForm::options (void)
{
	return m_pOptions;
}


// Accept settings (OK button slot).
void qtractorOptionsForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Messages options...
		m_pOptions->sMessagesFont   = MessagesFontTextLabel->font().toString();
		m_pOptions->bMessagesLimit  = MessagesLimitCheckBox->isChecked();
		m_pOptions->iMessagesLimitLines = MessagesLimitLinesSpinBox->value();
		// Other options...
		m_pOptions->bConfirmRemove  = ConfirmRemoveCheckBox->isChecked();
		m_pOptions->bStdoutCapture  = StdoutCaptureCheckBox->isChecked();
		m_pOptions->bCompletePath   = CompletePathCheckBox->isChecked();
		m_pOptions->bPeakAutoRemove = PeakAutoRemoveCheckBox->isChecked();
		m_pOptions->bTransportTime  = TransportTimeCheckBox->isChecked();
		m_pOptions->iMaxRecentFiles = MaxRecentFilesSpinBox->value();
		m_pOptions->iResampleType   = ResampleTypeComboBox->currentItem();
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
	QFont font = QFontDialog::getFont(&bOk, MessagesFontTextLabel->font(), this);
	if (bOk) {
		MessagesFontTextLabel->setFont(font);
		MessagesFontTextLabel->setText(font.family() + " " + QString::number(font.pointSize()));
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
	MessagesLimitLinesSpinBox->setEnabled(MessagesLimitCheckBox->isChecked());

	OkPushButton->setEnabled(m_iDirtyCount > 0);
}


// end of qtractorOptionsForm.ui.h

