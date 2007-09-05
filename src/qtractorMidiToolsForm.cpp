// qtractorMidiToolsForm.cpp
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

#include "qtractorMidiToolsForm.h"

#include "qtractorAbout.h"
#include "qtractorMidiEditor.h"

#include "qtractorMainForm.h"
#include "qtractorSession.h"


#include <QMessageBox>


//----------------------------------------------------------------------------
// qtractorMidiToolsForm -- UI wrapper form.

// Constructor.
qtractorMidiToolsForm::qtractorMidiToolsForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pTimeScale = NULL;

	qtractorSession  *pSession  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pSession = pMainForm->session();
	if (pSession) {
		// Copy from global time-scale instance...
		if (m_pTimeScale)
			delete m_pTimeScale;
		m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());
		m_ui.TransposeTimeSpinBox->setTimeScale(m_pTimeScale);
		m_ui.ResizeDurationSpinBox->setTimeScale(m_pTimeScale);
		// Default quantization value...
		unsigned short iSnapPerBeat = m_pTimeScale->snapPerBeat();
		if (iSnapPerBeat > 0)
			iSnapPerBeat--;
		int iSnapIndex = qtractorTimeScale::indexFromSnap(iSnapPerBeat);
		m_ui.QuantizeTimeComboBox->setCurrentIndex(iSnapIndex);
		m_ui.QuantizeDurationComboBox->setCurrentIndex(iSnapIndex);
	}

	// Choose BBT to be default format here.
	formatChanged(qtractorTimeScale::BBT);

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.QuantizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.TransposeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.NormalizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.RandomizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ResizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));

	QObject::connect(m_ui.QuantizeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.QuantizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.TransposeNoteCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.TransposeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.NormalizePercentRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.NormalizeValueRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.RandomizeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.RandomizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.RandomizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ResizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ResizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));

	QObject::connect(m_ui.TransposeFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.ResizeFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));

	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qtractorMidiToolsForm::~qtractorMidiToolsForm (void)
{
	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
}


// Set (initial) tool page.
void qtractorMidiToolsForm::setToolIndex ( int iToolIndex )
{
	// Set the proper tool page.
	m_ui.ToolTabWidget->setCurrentIndex(iToolIndex);
	switch (iToolIndex) {
	case qtractorMidiEditor::Quantize:
		m_ui.QuantizeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Transpose:
		m_ui.TransposeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Normalize:
		m_ui.NormalizeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Randomize:
		m_ui.RandomizeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Resize:
		m_ui.ResizeCheckBox->setChecked(true);
		break;
	default:
		break;
	}

	// Done.
	stabilizeForm();
}


// Retrieve the current export type, if the case arises.
int qtractorMidiToolsForm::toolIndex (void) const
{
	return m_ui.ToolTabWidget->currentIndex();
}


// Accept settings (OK button slot).
void qtractorMidiToolsForm::accept (void)
{
	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorMidiToolsForm::reject (void)
{
	// Bail out...
	QDialog::reject();
}


// Display format has changed.
void qtractorMidiToolsForm::formatChanged ( int iDisplayFormat )
{
	qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.TransposeFormatComboBox->setCurrentIndex(iDisplayFormat);
	m_ui.ResizeFormatComboBox->setCurrentIndex(iDisplayFormat);

	if (m_pTimeScale) {
		// Set from local time-scale instance...
		m_pTimeScale->setDisplayFormat(displayFormat);
		m_ui.TransposeTimeSpinBox->updateDisplayFormat();
		m_ui.ResizeDurationSpinBox->updateDisplayFormat();
	}

	stabilizeForm();
}


// Stabilize current form state.
void qtractorMidiToolsForm::stabilizeForm (void)
{
	int  iEnabled = 0;
	bool bEnabled;
	bool bEnabled2;

	// Quantize tool...

	bEnabled = m_ui.QuantizeCheckBox->isChecked();

	m_ui.QuantizeTimeCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.QuantizeTimeCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.QuantizeTimeComboBox->setEnabled(bEnabled2);

	m_ui.QuantizeDurationCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.QuantizeDurationCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.QuantizeDurationComboBox->setEnabled(bEnabled2);

	// Transpose tool...

	bEnabled = m_ui.TransposeCheckBox->isChecked();

	m_ui.TransposeNoteCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.TransposeNoteCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.TransposeNoteSpinBox->setEnabled(bEnabled2);

	m_ui.TransposeTimeCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.TransposeTimeCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.TransposeTimeSpinBox->setEnabled(bEnabled2);
	m_ui.TransposeFormatComboBox->setEnabled(bEnabled2);

	// Normalize tool...

	bEnabled = m_ui.NormalizeCheckBox->isChecked();

	m_ui.NormalizePercentRadioButton->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.NormalizePercentRadioButton->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.NormalizePercentSpinBox->setEnabled(bEnabled2);

	m_ui.NormalizeValueRadioButton->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.NormalizeValueRadioButton->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.NormalizeValueSpinBox->setEnabled(bEnabled2);

	// Randomize tool...

	bEnabled = m_ui.RandomizeCheckBox->isChecked();

	m_ui.RandomizeTimeCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeTimeCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.RandomizeTimeSpinBox->setEnabled(bEnabled2);

	m_ui.RandomizeDurationCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeDurationCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.RandomizeDurationSpinBox->setEnabled(bEnabled2);

	m_ui.RandomizeValueCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeValueCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.RandomizeValueSpinBox->setEnabled(bEnabled2);

	// Resize tool...

	bEnabled = m_ui.ResizeCheckBox->isChecked();

	m_ui.ResizeDurationCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.ResizeDurationCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.ResizeDurationSpinBox->setEnabled(bEnabled2);
	m_ui.ResizeFormatComboBox->setEnabled(bEnabled2);

	m_ui.ResizeValueCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.ResizeValueCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.ResizeValueSpinBox->setEnabled(bEnabled2);

	m_ui.OkPushButton->setEnabled(iEnabled > 0);
}


// end of qtractorMidiToolsForm.cpp
