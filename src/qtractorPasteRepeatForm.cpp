// qtractorPasteRepeatForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorPasteRepeatForm.h"

#include "qtractorAbout.h"
#include "qtractorTimeScale.h"
#include "qtractorSession.h"
#include "qtractorOptions.h"

#include <QMessageBox>
#include <QPushButton>


//----------------------------------------------------------------------------
// qtractorPasteRepeatForm -- UI wrapper form.

// Constructor.
qtractorPasteRepeatForm::qtractorPasteRepeatForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Initialize dirty control state.
	m_pTimeScale  = NULL;
	m_iDirtyCount = 0;

	// Copy from global time-scale instance...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());
		m_ui.RepeatPeriodSpinBox->setTimeScale(m_pTimeScale);
		m_ui.RepeatPeriodSpinBox->setDeltaValue(true, pSession->playHead());
	}

	// Initialize conveniency options...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		m_ui.RepeatCountSpinBox->setValue(pOptions->iPasteRepeatCount);
		m_ui.RepeatPeriodCheckBox->setChecked(pOptions->bPasteRepeatPeriod);
	}

	// Choose BBT to be default format here.
	formatChanged(qtractorTimeScale::BBT);

	// Try to set minimal window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.RepeatCountSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.RepeatPeriodCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RepeatPeriodSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.RepeatPeriodSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.RepeatFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorPasteRepeatForm::~qtractorPasteRepeatForm (void)
{
	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
}


// Access accepted the accepted values.
void qtractorPasteRepeatForm::setRepeatCount ( unsigned short iRepeatCount )
{
	m_ui.RepeatCountSpinBox->setValue(iRepeatCount);
}

unsigned short qtractorPasteRepeatForm::repeatCount (void) const
{
	return m_ui.RepeatCountSpinBox->value();
}


void qtractorPasteRepeatForm::setRepeatPeriod ( unsigned long iRepeatPeriod )
{
	m_ui.RepeatPeriodSpinBox->setValue(iRepeatPeriod, false);
}

unsigned long qtractorPasteRepeatForm::repeatPeriod (void) const
{
	return (m_ui.RepeatPeriodCheckBox->isChecked()
		? m_ui.RepeatPeriodSpinBox->value() : 0);
}


// Accept settings (OK button slot).
void qtractorPasteRepeatForm::accept (void)
{
	// Save settings...
	if (m_iDirtyCount > 0) {
		// Save conveniency options...
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions) {
			pOptions->iPasteRepeatCount   =  repeatCount();
			pOptions->bPasteRepeatPeriod  = (repeatPeriod() > 0);
		}
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorPasteRepeatForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Apply:
			accept();
			return;
		case QMessageBox::Discard:
			break;
		default:    // Cancel.
			bReject = false;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Dirty up settings.
void qtractorPasteRepeatForm::changed (void)
{
	++m_iDirtyCount;
	stabilizeForm();
}


// Display format has changed.
void qtractorPasteRepeatForm::formatChanged ( int iDisplayFormat )
{
	bool bBlockSignals = m_ui.RepeatFormatComboBox->blockSignals(true);
	m_ui.RepeatFormatComboBox->setCurrentIndex(iDisplayFormat);

	qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.RepeatPeriodSpinBox->setDisplayFormat(displayFormat);

	if (m_pTimeScale)
		m_pTimeScale->setDisplayFormat(displayFormat);

	m_ui.RepeatFormatComboBox->blockSignals(bBlockSignals);

	stabilizeForm();
}


// Stabilize current form state.
void qtractorPasteRepeatForm::stabilizeForm (void)
{
	bool bEnabled = m_ui.RepeatPeriodCheckBox->isChecked();
	m_ui.RepeatPeriodSpinBox->setEnabled(bEnabled);
	m_ui.RepeatFormatComboBox->setEnabled(bEnabled);

	m_ui.DialogButtonBox->button(
		QDialogButtonBox::Ok)->setEnabled(m_pTimeScale != NULL);
}


// end of qtractorPasteRepeatForm.cpp
