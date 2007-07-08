// qtractorOptionsForm.cpp
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

#include "qtractorOptionsForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"

#include "qtractorAudioFile.h"

#include <QFontDialog>
#include <QMessageBox>
#include <QValidator>


//----------------------------------------------------------------------------
// qtractorOptionsForm -- UI wrapper form.

// Constructor.
qtractorOptionsForm::qtractorOptionsForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// No settings descriptor initially (the caller will set it).
	m_pOptions = NULL;

	// Populate the capture file type combo-box.
	m_ui.AudioCaptureTypeComboBox->clear();
	int iFormat = 0;
	const qtractorAudioFileFactory::FileFormats& list
		= qtractorAudioFileFactory::formats();
	QListIterator<qtractorAudioFileFactory::FileFormat *> iter(list);
	while (iter.hasNext()) {
		qtractorAudioFileFactory::FileFormat *pFormat = iter.next();
		if (pFormat->type != qtractorAudioFileFactory::MadFile)
			m_ui.AudioCaptureTypeComboBox->addItem(pFormat->name, iFormat);
		++iFormat;
	}

	// Populate the audio capture sample format combo-box.
	m_ui.AudioCaptureFormatComboBox->clear();
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Signed 16-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Signed 24-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Signed 32-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Float  32-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Float  64-Bit"));

	// Populate the MIDI capture file format combo-box.
	m_ui.MidiCaptureFormatComboBox->clear();
	m_ui.MidiCaptureFormatComboBox->addItem(tr("SMF Format 0"));
	m_ui.MidiCaptureFormatComboBox->addItem(tr("SMF Format 1"));

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
	QObject::connect(m_ui.DisplayFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MaxRecentFilesSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioCaptureTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioCaptureFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioCaptureQualitySpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioResampleTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiCaptureFormatComboBox,
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
	m_ui.MaxRecentFilesSpinBox->setValue(m_pOptions->iMaxRecentFiles);
	m_ui.DisplayFormatComboBox->setCurrentIndex(m_pOptions->iDisplayFormat);

	// Audio options.
	int iIndex  = 0;
	int iFormat = 0;
	const qtractorAudioFileFactory::FileFormats& list
		= qtractorAudioFileFactory::formats();
	QListIterator<qtractorAudioFileFactory::FileFormat *> iter(list);
	while (iter.hasNext()) {
		qtractorAudioFileFactory::FileFormat *pFormat = iter.next();
		if (m_pOptions->sAudioCaptureExt == pFormat->ext
			&& m_pOptions->iAudioCaptureType == pFormat->data) {
			iIndex = m_ui.AudioCaptureTypeComboBox->findData(iFormat);
			break;
		}
		++iFormat;
	}
	m_ui.AudioCaptureTypeComboBox->setCurrentIndex(iIndex);
	m_ui.AudioCaptureFormatComboBox->setCurrentIndex(m_pOptions->iAudioCaptureFormat);
	m_ui.AudioCaptureQualitySpinBox->setValue(m_pOptions->iAudioCaptureQuality);
	m_ui.AudioResampleTypeComboBox->setCurrentIndex(m_pOptions->iAudioResampleType);

#ifndef CONFIG_LIBSAMPLERATE
	m_ui.AudioResampleTypeTextLabel->setEnabled(false);
	m_ui.AudioResampleTypeComboBox->setEnabled(false);
#endif

	// MIDI options.
	m_ui.MidiCaptureFormatComboBox->setCurrentIndex(m_pOptions->iMidiCaptureFormat);

	// Done. Restart clean.
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
		m_pOptions->sMessagesFont   = m_ui.MessagesFontTextLabel->font().toString();
		m_pOptions->bMessagesLimit  = m_ui.MessagesLimitCheckBox->isChecked();
		m_pOptions->iMessagesLimitLines = m_ui.MessagesLimitLinesSpinBox->value();
		// Other options...
		m_pOptions->bConfirmRemove  = m_ui.ConfirmRemoveCheckBox->isChecked();
		m_pOptions->bStdoutCapture  = m_ui.StdoutCaptureCheckBox->isChecked();
		m_pOptions->bCompletePath   = m_ui.CompletePathCheckBox->isChecked();
		m_pOptions->bPeakAutoRemove = m_ui.PeakAutoRemoveCheckBox->isChecked();
		m_pOptions->iMaxRecentFiles = m_ui.MaxRecentFilesSpinBox->value();
		m_pOptions->iDisplayFormat  = m_ui.DisplayFormatComboBox->currentIndex();
		// Audio options...
		int iFormat	= m_ui.AudioCaptureTypeComboBox->itemData(
			m_ui.AudioCaptureTypeComboBox->currentIndex()).toInt();
		const qtractorAudioFileFactory::FileFormat *pFormat
			= qtractorAudioFileFactory::formats().at(iFormat);
		m_pOptions->sAudioCaptureExt     = pFormat->ext;
		m_pOptions->iAudioCaptureType    = pFormat->data;
		m_pOptions->iAudioCaptureFormat  = m_ui.AudioCaptureFormatComboBox->currentIndex();
		m_pOptions->iAudioCaptureQuality = m_ui.AudioCaptureQualitySpinBox->value();
		m_pOptions->iAudioResampleType   = m_ui.AudioResampleTypeComboBox->currentIndex();
		// MIDI options...
		m_pOptions->iMidiCaptureFormat = m_ui.MidiCaptureFormatComboBox->currentIndex();
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

	// Audio options validy check...
	int iIndex  = m_ui.AudioCaptureTypeComboBox->currentIndex();
	int iFormat	= m_ui.AudioCaptureTypeComboBox->itemData(iIndex).toInt();
	const qtractorAudioFileFactory::FileFormat *pFormat
		= qtractorAudioFileFactory::formats().at(iFormat);

	bool bSndFile
		= (pFormat && pFormat->type == qtractorAudioFileFactory::SndFile);
	m_ui.AudioCaptureFormatTextLabel->setEnabled(bSndFile);
	m_ui.AudioCaptureFormatComboBox->setEnabled(bSndFile);

	bool bVorbisFile
		= (pFormat && pFormat->type == qtractorAudioFileFactory::VorbisFile);
	m_ui.AudioCaptureQualityTextLabel->setEnabled(bVorbisFile);
	m_ui.AudioCaptureQualitySpinBox->setEnabled(bVorbisFile);

	bool bValid = (m_iDirtyCount > 0);
	if (bValid) {
		iFormat = m_ui.AudioCaptureFormatComboBox->currentIndex();
		bValid  = qtractorAudioFileFactory::isValidFormat(pFormat, iFormat);
	}

	m_ui.OkPushButton->setEnabled(bValid);
}


// end of qtractorOptionsForm.cpp

