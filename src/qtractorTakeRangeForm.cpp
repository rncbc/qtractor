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

#include "qtractorClip.h"

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
	m_pClip = NULL;

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
	}

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.SelectionRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
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


// Setup accessors.
void qtractorTakeRangeForm::setClip ( qtractorClip *pClip )
{
	m_pClip = pClip;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Populate range options...
		if (m_pClip && m_pClip->isClipSelected())
			m_ui.SelectionRangeRadioButton->setChecked(true);
		else
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
}


qtractorClip *qtractorTakeRangeForm::clip (void) const
{
	return m_pClip;
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


int qtractorTakeRangeForm::currentTake (void) const
{
	return m_ui.CurrentTakeComboBox->currentIndex();
}


// Display format has changed.
void qtractorTakeRangeForm::rangeChanged (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (m_ui.SelectionRangeRadioButton->isChecked() && m_pClip) {
		m_ui.TakeStartSpinBox->setValue(m_pClip->clipSelectStart(), false);
		m_ui.TakeEndSpinBox->setValue(m_pClip->clipSelectEnd(), false);
	}
	else
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

	updateCurrentTake();
	stabilizeForm();
}


// Range values have changed.
void qtractorTakeRangeForm::valueChanged (void)
{
	m_ui.CustomRangeRadioButton->setChecked(true);

	updateCurrentTake();
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


// Populate current take list.
void qtractorTakeRangeForm::updateCurrentTake (void)
{
	int iCurrentTake = m_ui.CurrentTakeComboBox->currentIndex();
	m_ui.CurrentTakeComboBox->clear();
	if (m_pClip) {
		int iTakeCount = qtractorClip::TakeInfo(
			m_pClip->clipStart(),
			m_pClip->clipOffset(),
			m_pClip->clipLength(),
			m_ui.TakeStartSpinBox->value(),
			m_ui.TakeEndSpinBox->value()).takeCount();
		for (int iTake = 0; iTake < iTakeCount; ++iTake)
			m_ui.CurrentTakeComboBox->addItem(tr("Take %1").arg(iTake + 1));
		if (iCurrentTake < 0)
			iCurrentTake = 0;
		else
		if (iCurrentTake > iTakeCount - 1)
			iCurrentTake = iTakeCount - 1;
	}
	m_ui.CurrentTakeComboBox->setCurrentIndex(iCurrentTake);
}


// Stabilize current form state.
void qtractorTakeRangeForm::stabilizeForm (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	m_ui.SelectionRangeRadioButton->setEnabled(
		m_pClip && m_pClip->isClipSelected());
	m_ui.LoopRangeRadioButton->setEnabled(pSession->isLooping());
	m_ui.PunchRangeRadioButton->setEnabled(pSession->isPunching());
	m_ui.EditRangeRadioButton->setEnabled(
		pSession->editHead() < pSession->editTail());

	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(
		m_ui.TakeStartSpinBox->value() < m_ui.TakeEndSpinBox->value());
}


// end of qtractorTakeRangeForm.cpp
