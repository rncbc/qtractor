// qtractorTakeRangeForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTakeRangeForm.h"

#include "qtractorAbout.h"
#include "qtractorSession.h"

#include <QMessageBox>
#include <QPushButton>


//----------------------------------------------------------------------------
// qtractorTakeRangeForm -- UI wrapper form.

// Constructor.
qtractorTakeRangeForm::qtractorTakeRangeForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::ApplicationModal);

	// Initialize dirty control state.
	m_pTimeScale = NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());
		m_ui.TakeStartSpinBox->setTimeScale(m_pTimeScale);
		m_ui.TakeEndSpinBox->setTimeScale(m_pTimeScale);
		// Set proper time scales display format...
		switch (m_pTimeScale->displayFormat()) {
		case qtractorTimeScale::BBT:
			m_ui.BbtRadioButton->setChecked(true);
			break;
		case qtractorTimeScale::Time:
			m_ui.TimeRadioButton->setChecked(true);
			break;
		case qtractorTimeScale::Frames:
		default:
			m_ui.FramesRadioButton->setChecked(true);
			break;
		}
		// Populate range options...
		if (pSession->isLooping())
			m_ui.LoopRangeRadioButton->setChecked(true);
		else
		if (pSession->isPunching())
			m_ui.PunchRangeRadioButton->setChecked(true);
		else
		if (pSession->editHead() < pSession->editTail())
			m_ui.EditRangeRadioButton->setChecked(true);
		else
			m_ui.CustomRangeRadioButton->setChecked(true);
		// Populate range values...
		rangeChanged();
	}

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.LoopRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.PunchRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.EditRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.CustomRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.TimeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(formatChanged()));
	QObject::connect(m_ui.BbtRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(formatChanged()));
	QObject::connect(m_ui.TakeStartSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(valueChanged()));
	QObject::connect(m_ui.TakeEndSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(valueChanged()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));

	// Done.
	stabilizeForm();
}


// Destructor.
qtractorTakeRangeForm::~qtractorTakeRangeForm (void)
{
	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
}


// Retrieve the current take-range, if the case arises.
unsigned long qtractorTakeRangeForm::takeStart (void) const
{
	return m_ui.TakeStartSpinBox->value();
}

unsigned long qtractorTakeRangeForm::takeEnd (void) const
{
	return m_ui.TakeEndSpinBox->value();
}


// Display format has changed.
void qtractorTakeRangeForm::rangeChanged (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (m_ui.LoopRangeRadioButton->isChecked()) {
		m_ui.TakeStartSpinBox->setValue(pSession->loopStart(), false);
		m_ui.TakeEndSpinBox->setValue(pSession->loopEnd(), false);
	}
	else
	if (m_ui.PunchRangeRadioButton->isChecked()) {
		m_ui.TakeStartSpinBox->setValue(pSession->punchIn(), false);
		m_ui.TakeEndSpinBox->setValue(pSession->punchOut(), false);
	}
	else
	if (m_ui.EditRangeRadioButton->isChecked()) {
		m_ui.TakeStartSpinBox->setValue(pSession->editHead(), false);
		m_ui.TakeEndSpinBox->setValue(pSession->editTail(), false);
	}

	stabilizeForm();
}


// Range values have changed.
void qtractorTakeRangeForm::valueChanged (void)
{
	m_ui.CustomRangeRadioButton->setChecked(true);

	stabilizeForm();
}


// Display format has changed.
void qtractorTakeRangeForm::formatChanged (void)
{
	qtractorTimeScale::DisplayFormat displayFormat = qtractorTimeScale::Frames;

	if (m_ui.TimeRadioButton->isChecked())
		displayFormat = qtractorTimeScale::Time;
	else
	if (m_ui.BbtRadioButton->isChecked())
		displayFormat= qtractorTimeScale::BBT;

	if (m_pTimeScale) {
		// Set from local time-scale instance...
		m_pTimeScale->setDisplayFormat(displayFormat);
		m_ui.TakeStartSpinBox->updateDisplayFormat();
		m_ui.TakeEndSpinBox->updateDisplayFormat();
	}

	stabilizeForm();
}


// Stabilize current form state.
void qtractorTakeRangeForm::stabilizeForm (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	m_ui.LoopRangeRadioButton->setEnabled(pSession->isLooping());
	m_ui.PunchRangeRadioButton->setEnabled(pSession->isPunching());
	m_ui.EditRangeRadioButton->setEnabled(
		pSession->editHead() < pSession->editTail());

	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(
		m_ui.TakeStartSpinBox->value() < m_ui.TakeEndSpinBox->value());
}


// end of qtractorTakeRangeForm.cpp
