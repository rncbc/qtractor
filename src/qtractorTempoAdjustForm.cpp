// qtractorTempoAdjustForm.cpp
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

#include "qtractorTempoAdjustForm.h"

#include "qtractorAbout.h"

#include "qtractorSession.h"

#include "qtractorMainForm.h"

#include <QMessageBox>
#include <QLineEdit>
#include <QTime>


//----------------------------------------------------------------------------
// qtractorTempoAdjustForm -- UI wrapper form.

// Constructor.
qtractorTempoAdjustForm::qtractorTempoAdjustForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pTempoTap = new QTime();

	// Initialize local time scale.
	m_pTimeScale = new qtractorTimeScale();
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		m_pTimeScale->copy(*pSession->timeScale());

	m_ui.RangeStartSpinBox->setTimeScale(m_pTimeScale);
	m_ui.RangeLengthSpinBox->setTimeScale(m_pTimeScale);

	m_ui.TempoSpinBox->setTempo(m_pTimeScale->tempo(), false);
	m_ui.TempoSpinBox->setBeatsPerBar(m_pTimeScale->beatsPerBar(), false);
	m_ui.TempoSpinBox->setBeatDivisor(m_pTimeScale->beatDivisor(), true);

	m_iTempoTap = 0;
	m_fTempoTap = 0.0f;

	// Set proper time scales display format...
	m_ui.FormatComboBox->setCurrentIndex(int(m_pTimeScale->displayFormat()));

	// Initialize dirty control state (nope).
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	// Try to set minimal window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(tempoChanged()));
	QObject::connect(m_ui.RangeStartSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(rangeStartChanged(unsigned long)));
	QObject::connect(m_ui.RangeStartSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.RangeLengthSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(rangeLengthChanged(unsigned long)));
	QObject::connect(m_ui.RangeLengthSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.RangeBeatsSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AdjustPushButton,
		SIGNAL(clicked()),
		SLOT(adjust()));
	QObject::connect(m_ui.FormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.TempoPushButton,
		SIGNAL(clicked()),
		SLOT(tempoTap()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorTempoAdjustForm::~qtractorTempoAdjustForm (void)
{
	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
	if (m_pTempoTap)
		delete m_pTempoTap;
}


// Range accessors.
void qtractorTempoAdjustForm::setRangeStart ( unsigned long iRangeStart )
{
	++m_iDirtySetup;
	m_ui.RangeStartSpinBox->setValue(iRangeStart, true);
	m_ui.RangeLengthSpinBox->setDeltaValue(true, iRangeStart);
	--m_iDirtySetup;
}

unsigned long qtractorTempoAdjustForm::rangeStart (void) const
{
	return m_ui.RangeStartSpinBox->value();
}

void qtractorTempoAdjustForm::setRangeLength ( unsigned long iRangeLength )
{
	++m_iDirtySetup;
	m_ui.RangeLengthSpinBox->setValue(iRangeLength, true);
	unsigned int iRangeBeats = m_pTimeScale->beatFromFrame(iRangeLength);
	unsigned long q = m_pTimeScale->beatsPerBar();
	iRangeBeats = q * ((iRangeBeats + (q >> 1)) / q);
	m_ui.RangeBeatsSpinBox->setValue(iRangeBeats);
	--m_iDirtySetup;
}

unsigned long qtractorTempoAdjustForm::rangeLength (void) const
{
	return m_ui.RangeLengthSpinBox->value();
}

unsigned short qtractorTempoAdjustForm::rangeBeats (void) const
{
	return m_ui.RangeBeatsSpinBox->value();
}


// Accept settings (OK button slot).
void qtractorTempoAdjustForm::accept (void)
{
	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorTempoAdjustForm::reject (void)
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
void qtractorTempoAdjustForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	++m_iDirtyCount;
	stabilizeForm();
}


// Tempo signature has changed.
void qtractorTempoAdjustForm::tempoChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::tempoChanged()");
#endif

	m_iTempoTap = 0;
	m_fTempoTap = 0.0f;
	
	changed();
}


// Adjust delta-value spin-boxes to new anchor frame.
void qtractorTempoAdjustForm::rangeStartChanged ( unsigned long iRangeStart )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::rangeStartChanged(%lu)", iRangeStart);
#endif

	m_ui.RangeLengthSpinBox->setDeltaValue(true, iRangeStart);

	selectChanged();
}


void qtractorTempoAdjustForm::rangeLengthChanged ( unsigned long iRangeLength )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::rangeLengthChanged(%lu)", iRangeLength);
#endif

	int iRangeBeatsMax // It follows from max. tempo = 300bpm.
		= int(5.0f * float(iRangeLength) / float(m_pTimeScale->sampleRate()));
	m_ui.RangeBeatsSpinBox->setMaximum(iRangeBeatsMax);

	selectChanged();
}


// Adjust as instructed.
void qtractorTempoAdjustForm::adjust (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::adjust()");
#endif

	unsigned short iRangeBeats = m_ui.RangeBeatsSpinBox->value();
	if (iRangeBeats < 1)
		return;

	unsigned long iRangeLength = m_ui.RangeLengthSpinBox->value();
	unsigned long iBeatLength  = iRangeLength / iRangeBeats;
	
	float fTempo = 60.0f * float(m_pTimeScale->sampleRate()) / float(iBeatLength);
	m_ui.TempoSpinBox->setTempo(fTempo, false);

//	m_ui.RangeLengthSpinBox->setValue(iRangeBeats * iBeatLength, false);
	selectChanged();
}


// Adjust current selection edit/tail.
void qtractorTempoAdjustForm::selectChanged (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::selectChanged()");
#endif

	unsigned long iRangeStart  = m_ui.RangeStartSpinBox->value();
	unsigned long iRangeLength = m_ui.RangeLengthSpinBox->value();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		pSession->setEditHead(iRangeStart);
		pSession->setEditTail(iRangeStart + iRangeLength);
	}

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->selectionNotifySlot(NULL);

	changed();
}


// Display format has changed.
void qtractorTempoAdjustForm::formatChanged ( int iDisplayFormat )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::formatChanged()");
#endif

	bool bBlockSignals = m_ui.FormatComboBox->blockSignals(true);
	m_ui.FormatComboBox->setCurrentIndex(iDisplayFormat);

	qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.RangeStartSpinBox->setDisplayFormat(displayFormat);
	m_ui.RangeLengthSpinBox->setDisplayFormat(displayFormat);

	if (m_pTimeScale)
		m_pTimeScale->setDisplayFormat(displayFormat);

	m_ui.FormatComboBox->blockSignals(bBlockSignals);

	stabilizeForm();
}


// Stabilize current form state.
void qtractorTempoAdjustForm::stabilizeForm (void)
{
	bool bValid = (m_iDirtyCount > 0);
	unsigned long iRangeLength = m_ui.RangeLengthSpinBox->value();
	unsigned short iRangeBeats = m_ui.RangeBeatsSpinBox->value();
	bValid = bValid && (iRangeLength > 0);
	bValid = bValid && (iRangeBeats > 0);
	m_ui.AdjustPushButton->setEnabled(bValid);
//	m_ui.OkPushButton->setEnabled(bValid);
}


// Tempo tap click.
void qtractorTempoAdjustForm::tempoTap (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::tempoTap()");
#endif

	int iTimeTap = m_pTempoTap->restart();
	if (iTimeTap > 200 && iTimeTap < 2000) { // Magic!
		m_fTempoTap += (60000.0f / float(iTimeTap));
		if (++m_iTempoTap > 2) {
			m_fTempoTap /= float(m_iTempoTap);
			m_iTempoTap  = 1; // Median-like averaging...
			m_ui.TempoSpinBox->setTempo(int(m_fTempoTap), false);
		}
	} else {
		m_iTempoTap = 0;
		m_fTempoTap = 0.0f;
	}
}


// Accepted results accessors.
float qtractorTempoAdjustForm::tempo (void) const
{
	return m_ui.TempoSpinBox->tempo();
}

unsigned short qtractorTempoAdjustForm::beatsPerBar (void) const
{
	return m_ui.TempoSpinBox->beatsPerBar();
}

unsigned short qtractorTempoAdjustForm::beatDivisor (void) const
{
	return m_ui.TempoSpinBox->beatDivisor();
}


// end of qtractorTempoAdjustForm.cpp
