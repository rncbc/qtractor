// qtractorMidiToolsForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiClip.h"

#include "qtractorMidiEditCommand.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"


#include <QMessageBox>

// This shall hold the default preset name.
static QString g_sDefPreset;


//----------------------------------------------------------------------------
// qtractorMidiToolsForm -- UI wrapper form.

// Constructor.
qtractorMidiToolsForm::qtractorMidiToolsForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	m_pTimeScale = NULL;

	m_iDirtyCount = 0;
	m_iUpdate = 0;

    m_ui.PresetNameComboBox->setValidator(
		new QRegExpValidator(QRegExp("[\\w-]+"), m_ui.PresetNameComboBox));
	m_ui.PresetNameComboBox->setInsertPolicy(QComboBox::NoInsert);

	if (g_sDefPreset.isEmpty())
		g_sDefPreset = tr("(default)");

	// Reinitialize random seed.
	::srand(::time(NULL));

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Copy from global time-scale instance...
		if (m_pTimeScale)
			delete m_pTimeScale;
		m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());
		m_ui.TransposeTimeSpinBox->setTimeScale(m_pTimeScale);
		m_ui.ResizeDurationSpinBox->setTimeScale(m_pTimeScale);
		// Fill-up snap-per-beat items...
		QStringList items = qtractorTimeScale::snapItems(1);
		m_ui.QuantizeTimeComboBox->clear();
		m_ui.QuantizeTimeComboBox->insertItems(0, items);
		m_ui.QuantizeDurationComboBox->clear();
		m_ui.QuantizeDurationComboBox->insertItems(0, items);
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

	// Load initial preset names;
	loadPreset(g_sDefPreset);
	refreshPresets();
	
	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.PresetNameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(presetChanged(const QString&)));
	QObject::connect(m_ui.PresetNameComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(presetActivated(const QString&)));
	QObject::connect(m_ui.PresetSaveToolButton,
		SIGNAL(clicked()),
		SLOT(presetSave()));
	QObject::connect(m_ui.PresetDeleteToolButton,
		SIGNAL(clicked()),
		SLOT(presetDelete()));

	QObject::connect(m_ui.QuantizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.QuantizeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeTimeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeDurationComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));

	QObject::connect(m_ui.TransposeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.TransposeNoteCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeNoteSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeTimeSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));

	QObject::connect(m_ui.NormalizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.NormalizePercentCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.NormalizePercentSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.NormalizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.NormalizeValueSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));

	QObject::connect(m_ui.RandomizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.RandomizeNoteCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeNoteSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeTimeSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeDurationSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeValueSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));

	QObject::connect(m_ui.ResizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ResizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeDurationSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.ResizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeValueSpinBox,
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


// Preset management methods...
void qtractorMidiToolsForm::loadPreset ( const QString& sPreset )
{
	// An existing preset is about to be loaded...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QVariant> vlist;
		QSettings& settings = pOptions->settings();
		// Get the preset entry...
		settings.beginGroup("/MidiTools");
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.beginGroup('/' + sPreset);
		// Quantize tool...
		vlist = settings.value("/Quantize").toList();
		if (vlist.count() > 4) {
		//	m_ui.QuantizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.QuantizeTimeCheckBox->setChecked(vlist[1].toBool());
			m_ui.QuantizeTimeComboBox->setCurrentIndex(vlist[2].toInt());
			m_ui.QuantizeDurationCheckBox->setChecked(vlist[3].toBool());
			m_ui.QuantizeDurationComboBox->setCurrentIndex(vlist[4].toInt());
		}
		// Transpose tool...
		vlist = settings.value("/Transpose").toList();
		if (vlist.count() > 4) {
		//	m_ui.TransposeCheckBox->setChecked(vlist[0].toBool());
			m_ui.TransposeNoteCheckBox->setChecked(vlist[1].toBool());
			m_ui.TransposeNoteSpinBox->setValue(vlist[2].toInt());
			m_ui.TransposeTimeCheckBox->setChecked(vlist[3].toBool());
			m_ui.TransposeTimeSpinBox->setValue(vlist[4].toUInt());
		}
		// Normalize tool...
		vlist = settings.value("/Normalize").toList();
		if (vlist.count() > 4) {
		//	m_ui.NormalizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.NormalizePercentCheckBox->setChecked(vlist[1].toBool());
			m_ui.NormalizePercentSpinBox->setValue(vlist[2].toInt());
			m_ui.NormalizeValueCheckBox->setChecked(vlist[3].toBool());
			m_ui.NormalizeValueSpinBox->setValue(vlist[4].toInt());
		}
		// Randomize tool...
		vlist = settings.value("/Randomize").toList();
		if (vlist.count() > 8) {
		//	m_ui.RandomizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.RandomizeNoteCheckBox->setChecked(vlist[1].toBool());
			m_ui.RandomizeNoteSpinBox->setValue(vlist[2].toInt());
			m_ui.RandomizeTimeCheckBox->setChecked(vlist[3].toBool());
			m_ui.RandomizeTimeSpinBox->setValue(vlist[4].toInt());
			m_ui.RandomizeDurationCheckBox->setChecked(vlist[5].toBool());
			m_ui.RandomizeDurationSpinBox->setValue(vlist[6].toInt());
			m_ui.RandomizeValueCheckBox->setChecked(vlist[7].toBool());
			m_ui.RandomizeValueSpinBox->setValue(vlist[8].toInt());
		}
		// Resize tool...
		vlist = settings.value("/Resize").toList();
		if (vlist.count() > 4) {
		//	m_ui.ResizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.ResizeValueCheckBox->setChecked(vlist[1].toBool());
			m_ui.ResizeValueSpinBox->setValue(vlist[2].toInt());
			m_ui.ResizeDurationCheckBox->setChecked(vlist[3].toBool());
			m_ui.ResizeDurationSpinBox->setValue(vlist[4].toUInt());
		}
		// All loaded.
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.endGroup();
		settings.endGroup();
	}
}


void qtractorMidiToolsForm::savePreset ( const QString& sPreset )
{
	// The current state preset is about to be saved...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QVariant> vlist;
		QSettings& settings = pOptions->settings();
		// Set preset entry...
		settings.beginGroup("/MidiTools");
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.beginGroup('/' + sPreset);
		// Quantize tool...
		vlist.clear();
		vlist.append(m_ui.QuantizeCheckBox->isChecked());
		vlist.append(m_ui.QuantizeTimeCheckBox->isChecked());
		vlist.append(m_ui.QuantizeTimeComboBox->currentIndex());
		vlist.append(m_ui.QuantizeDurationCheckBox->isChecked());
		vlist.append(m_ui.QuantizeDurationComboBox->currentIndex());
		settings.setValue("/Quantize", vlist);
		// Transpose tool...
		vlist.clear();
		vlist.append(m_ui.TransposeCheckBox->isChecked());
		vlist.append(m_ui.TransposeNoteCheckBox->isChecked());
		vlist.append(m_ui.TransposeNoteSpinBox->value());
		vlist.append(m_ui.TransposeTimeCheckBox->isChecked());
		vlist.append((unsigned int) m_ui.TransposeTimeSpinBox->value());
		settings.setValue("/Transpose", vlist);
		// Normalize tool...
		vlist.clear();
		vlist.append(m_ui.NormalizeCheckBox->isChecked());
		vlist.append(m_ui.NormalizePercentCheckBox->isChecked());
		vlist.append(m_ui.NormalizePercentSpinBox->value());
		vlist.append(m_ui.NormalizeValueCheckBox->isChecked());
		vlist.append(m_ui.NormalizeValueSpinBox->value());
		settings.setValue("/Normalize", vlist);
		// Randomize tool...
		vlist.clear();
		vlist.append(m_ui.RandomizeCheckBox->isChecked());
		vlist.append(m_ui.RandomizeNoteCheckBox->isChecked());
		vlist.append(m_ui.RandomizeNoteSpinBox->value());
		vlist.append(m_ui.RandomizeTimeCheckBox->isChecked());
		vlist.append(m_ui.RandomizeTimeSpinBox->value());
		vlist.append(m_ui.RandomizeDurationCheckBox->isChecked());
		vlist.append(m_ui.RandomizeDurationSpinBox->value());
		vlist.append(m_ui.RandomizeValueCheckBox->isChecked());
		vlist.append(m_ui.RandomizeValueSpinBox->value());
		settings.setValue("/Randomize", vlist);
		// Resize tool...
		vlist.clear();
		vlist.append(m_ui.ResizeCheckBox->isChecked());
		vlist.append(m_ui.ResizeValueCheckBox->isChecked());
		vlist.append(m_ui.ResizeValueSpinBox->value());
		vlist.append(m_ui.ResizeDurationCheckBox->isChecked());
		vlist.append((unsigned int) m_ui.ResizeDurationSpinBox->value());
		settings.setValue("/Resize", vlist);
		// All saved.
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.endGroup();
		settings.endGroup();
	}
}


// Preset list loader.
void qtractorMidiToolsForm::refreshPresets (void)
{
	m_iUpdate++;

	const QString sOldPreset = m_ui.PresetNameComboBox->currentText();
	m_ui.PresetNameComboBox->clear();
	
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup("/MidiTools");
		m_ui.PresetNameComboBox->insertItems(0, settings.childGroups());
		settings.endGroup();
	}
	m_ui.PresetNameComboBox->addItem(g_sDefPreset);
	m_ui.PresetNameComboBox->setEditText(sOldPreset);

	m_iDirtyCount = 0;
	m_iUpdate--;

	stabilizeForm();
}


// Preset management slots...
void qtractorMidiToolsForm::presetChanged ( const QString& sPreset )
{
	if (m_iUpdate > 0)
		return;

	if (!sPreset.isEmpty()
		&& m_ui.PresetNameComboBox->findText(sPreset) >= 0)
		m_iDirtyCount++;

	stabilizeForm();
}


void qtractorMidiToolsForm::presetActivated ( const QString& sPreset )
{
	m_iUpdate++;

	loadPreset(sPreset);

	m_iDirtyCount = 0;
	m_iUpdate--;
	
	stabilizeForm();
}


void qtractorMidiToolsForm::presetSave (void)
{
	savePreset(m_ui.PresetNameComboBox->currentText());

	refreshPresets();
}


void qtractorMidiToolsForm::presetDelete (void)
{
	if (m_iUpdate > 0)
		return;

	const QString& sPreset = m_ui.PresetNameComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// A preset entry is about to be deleted...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		// Prompt user if he/she's sure about this...
		if (pOptions->bConfirmRemove) {
			if (QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("About to delete preset:\n\n"
				"\"%1\"\n\n"
				"Are you sure?")
				.arg(sPreset),
				QMessageBox::Ok | QMessageBox::Cancel)
				== QMessageBox::Cancel)
				return;
		}
		// Go ahead...
		QSettings& settings = pOptions->settings();
		settings.beginGroup("/MidiTools");
		settings.remove(sPreset);
		settings.endGroup();
		refreshPresets();
	}
}


// Create edit command based on given selection.
qtractorMidiEditCommand *qtractorMidiToolsForm::editCommand (
	qtractorMidiClip *pMidiClip, qtractorMidiEditSelect *pSelect,
	unsigned long iTimeOffset )
{
	// Create command, it will be handed over...
	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(pMidiClip, tr("none"));

	// Set composite command title.
	QStringList tools;
	if (m_ui.QuantizeCheckBox->isChecked())
		tools.append(tr("quantize"));
	if (m_ui.TransposeCheckBox->isChecked())
		tools.append(tr("transpose"));
	if (m_ui.NormalizeCheckBox->isChecked())
		tools.append(tr("normalize"));
	if (m_ui.RandomizeCheckBox->isChecked())
		tools.append(tr("randomize"));
	if (m_ui.ResizeCheckBox->isChecked())
		tools.append(tr("resize"));
	pEditCommand->setName(tools.join(", "));

	QListIterator<qtractorMidiEditSelect::Item *> iter(pSelect->items());

	// First scan pass for the normalize tool:
	// find maximum value from the selection...
	int iMaxValue = 0;
	if (m_ui.NormalizeCheckBox->isChecked()) {
		// Make it through one time...
		while (iter.hasNext()) {
			qtractorMidiEvent *pEvent = iter.next()->event;
			bool bPitchBend = (pEvent->type() == qtractorMidiEvent::PITCHBEND);
			int iValue = (bPitchBend ? pEvent->pitchBend() : pEvent->value());
			if (iMaxValue < iValue)
				iMaxValue = iValue;
		}
		// Get it back to front...
		iter.toFront();
	}

	qtractorTimeScale::Cursor cursor(m_pTimeScale);

	while (iter.hasNext()) {
		qtractorMidiEvent *pEvent = iter.next()->event;
		long iTime = pEvent->time();
		long iDuration = pEvent->duration();
		bool bPitchBend = (pEvent->type() == qtractorMidiEvent::PITCHBEND);
		int iValue = (bPitchBend ? pEvent->pitchBend() : pEvent->value());
		qtractorTimeScale::Node *pNode = cursor.seekTick(iTime + iTimeOffset);
		// Quantize tool...
		if (m_ui.QuantizeCheckBox->isChecked()) {
			tools.append(tr("quantize"));
			if (m_ui.QuantizeTimeCheckBox->isChecked()) {
				unsigned short p = qtractorTimeScale::snapFromIndex(
					m_ui.QuantizeTimeComboBox->currentIndex() + 1);
				unsigned long q = pNode->ticksPerBeat / p;
				iTime = q * ((iTime + (q >> 1)) / q);
			}
			if (m_ui.QuantizeDurationCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				unsigned short p = qtractorTimeScale::snapFromIndex(
					m_ui.QuantizeDurationComboBox->currentIndex() + 1);
				unsigned long q = pNode->ticksPerBeat / p;
				iDuration = q * ((iDuration + q - 1) / q);
			}
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
		// Transpose tool...
		if (m_ui.TransposeCheckBox->isChecked()) {
			tools.append(tr("transpose"));
			int iNote = int(pEvent->note());
			if (m_ui.TransposeNoteCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				iNote += m_ui.TransposeNoteSpinBox->value();
				if (iNote < 0)
					iNote = 0;
				else
				if (iNote > 127)
					iNote = 127;
			}
			if (m_ui.TransposeTimeCheckBox->isChecked()) {
				iTime += pNode->tickFromFrame(
					pNode->frameFromTick(iTime + iTimeOffset)
						+ m_ui.TransposeTimeSpinBox->value())
							- (iTime + iTimeOffset);
				if (iTime < 0)
					iTime = 0;
			}
			pEditCommand->moveEvent(pEvent, iNote, iTime);
		}
		// Normalize tool...
		if (m_ui.NormalizeCheckBox->isChecked()) {
			tools.append(tr("normalize"));
			int p, q = iMaxValue;
			if (m_ui.NormalizeValueCheckBox->isChecked())
				p = m_ui.NormalizeValueSpinBox->value();
			else
				p = (bPitchBend ? 8192 : 128);
			if (m_ui.NormalizePercentCheckBox->isChecked()) {
				p *= m_ui.NormalizePercentSpinBox->value();
				q *= 100;
			}
			if (q > 0) {
				iValue = (iValue * p) / q;
				if (bPitchBend) {
					if (iValue > +8191)
						iValue = +8191;
					else
					if (iValue < -8191)
						iValue = -8191;
				} else {
					if (iValue > 127)
						iValue = 127;
					else
					if (iValue < 0)
						iValue = 0;
				}
			}
			pEditCommand->resizeEventValue(pEvent, iValue);
		}
		// Randomize tool...
		if (m_ui.RandomizeCheckBox->isChecked()) {
			tools.append(tr("randomize"));
			int p, q;
			if (m_ui.RandomizeNoteCheckBox->isChecked()) {
				int iNote = int(pEvent->note());
				p = m_ui.RandomizeNoteSpinBox->value();
				q = 127;
				if (p > 0) {
					iNote += (p * (q - (::rand() % (q << 1)))) / 100;
					if (iNote > 127)
						iNote = 127;
					else
					if (iNote < 0)
						iNote = 0;
					pEditCommand->moveEvent(pEvent, iNote, iTime);
				}
			}
			if (m_ui.RandomizeTimeCheckBox->isChecked()) {
				p = m_ui.RandomizeTimeSpinBox->value();
				q = pNode->ticksPerBeat;
				if (p > 0) {
					iTime += (p * (q - (::rand() % (q << 1)))) / 100;
					if (iTime < 0)
						iTime = 0;
					pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
				}
			}
			if (m_ui.RandomizeDurationCheckBox->isChecked()) {
				p = m_ui.RandomizeDurationSpinBox->value();
				q = pNode->ticksPerBeat;
				if (p > 0) {
					iDuration += (p * (q - (::rand() % (q << 1)))) / 100;
					if (iDuration < 0)
						iDuration = 0;
					pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
				}
			}
			if (m_ui.RandomizeValueCheckBox->isChecked()) {
				p = m_ui.RandomizeValueSpinBox->value();
				q = (bPitchBend ? 8192 : 128);
				if (p > 0) {
					iValue += (p * (q - (::rand() % (q << 1)))) / 100;
					if (bPitchBend) {
						if (iValue > +8191)
							iValue = +8191;
						else
						if (iValue < -8191)
							iValue = -8191;
					} else {
						if (iValue > 127)
							iValue = 127;
						else
						if (iValue < 0)
							iValue = 0;
					}
					pEditCommand->resizeEventValue(pEvent, iValue);
				}
			}
		}
		// Resize tool...
		if (m_ui.ResizeCheckBox->isChecked()) {
			tools.append(tr("resize"));
			if (m_ui.ResizeDurationCheckBox->isChecked()) {
				iDuration = pNode->tickFromFrame(
					pNode->frameFromTick(iTime + iTimeOffset)
						+ m_ui.ResizeDurationSpinBox->value())
							- (iTime + iTimeOffset);
				pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
			}
			if (m_ui.ResizeValueCheckBox->isChecked()) {
				iValue = m_ui.ResizeValueSpinBox->value();
				if (bPitchBend) iValue <<= 6; // WTF?
				pEditCommand->resizeEventValue(pEvent, iValue);
			}
		}
	}

	// Done.
	return pEditCommand;
}


// Common change slot.
void qtractorMidiToolsForm::changed (void)
{
	if (m_iUpdate > 0)
		return;

	m_iDirtyCount++;
	stabilizeForm();
}


// Accept settings (OK button slot).
void qtractorMidiToolsForm::accept (void)
{
	// Save as default preset...
	savePreset(g_sDefPreset);

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

	// Preset status...
	const QString& sPreset = m_ui.PresetNameComboBox->currentText();
	bool bExists = (m_ui.PresetNameComboBox->findText(sPreset) >= 0);
	bEnabled = (!sPreset.isEmpty() && sPreset != g_sDefPreset);
	m_ui.PresetSaveToolButton->setEnabled(bEnabled
		&& (!bExists || m_iDirtyCount > 0));
	m_ui.PresetDeleteToolButton->setEnabled(bEnabled && bExists);

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

	m_ui.NormalizePercentCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.NormalizePercentCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.NormalizePercentSpinBox->setEnabled(bEnabled2);

	m_ui.NormalizeValueCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.NormalizeValueCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.NormalizeValueSpinBox->setEnabled(bEnabled2);

	// Randomize tool...

	bEnabled = m_ui.RandomizeCheckBox->isChecked();

	m_ui.RandomizeNoteCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeNoteCheckBox->isChecked();
	if (bEnabled2)
		iEnabled++;
	m_ui.RandomizeNoteSpinBox->setEnabled(bEnabled2);

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
