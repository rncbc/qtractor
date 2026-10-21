// qtractorAudioToolsForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioToolsForm.h"

#include "qtractorAudioEditor.h"
#include "qtractorAudioEditCommand.h"
#include "qtractorAudioClip.h"

#include "qtractorOptions.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QRegularExpressionValidator>

#include <cmath>


// Default preset name (saved/loaded implicitly on OK/open).
static const char *g_sDefPreset = "(default)";


//----------------------------------------------------------------------
// class qtractorAudioToolsForm -- Audio tools dialog.
//

// Constructor.
qtractorAudioToolsForm::qtractorAudioToolsForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let other windows stay accessible).
	QDialog::setWindowModality(Qt::WindowModal);

	m_iDirtyCount = 0;
	m_iUpdate = 0;

	// Preset name validator: printable non-slash characters.
	m_ui.PresetNameComboBox->setValidator(
		new QRegularExpressionValidator(
			QRegularExpression("[^/]+"), m_ui.PresetNameComboBox));

	// Load the default preset.
	refreshPresets();
	loadPreset(g_sDefPreset);

	// Wire up preset bar.
	QObject::connect(m_ui.PresetNameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(presetChanged(const QString&)));
	QObject::connect(m_ui.PresetNameComboBox,
		SIGNAL(activated(int)),
		SLOT(presetActivated(int)));
	QObject::connect(m_ui.PresetSaveToolButton,
		SIGNAL(clicked()),
		SLOT(presetSave()));
	QObject::connect(m_ui.PresetDeleteToolButton,
		SIGNAL(clicked()),
		SLOT(presetDelete()));

	// Wire up tool controls → changed().
	QObject::connect(m_ui.GainCheckBox,
		SIGNAL(toggled(bool)), SLOT(changed()));
	QObject::connect(m_ui.GainSpinBox,
		SIGNAL(valueChanged(double)), SLOT(changed()));
	QObject::connect(m_ui.PanningCheckBox,
		SIGNAL(toggled(bool)), SLOT(changed()));
	QObject::connect(m_ui.PanningSpinBox,
		SIGNAL(valueChanged(double)), SLOT(changed()));
	QObject::connect(m_ui.NormalizeCheckBox,
		SIGNAL(toggled(bool)), SLOT(changed()));
	QObject::connect(m_ui.NormalizeSpinBox,
		SIGNAL(valueChanged(double)), SLOT(changed()));
	QObject::connect(m_ui.TimeStretchCheckBox,
		SIGNAL(toggled(bool)), SLOT(changed()));
	QObject::connect(m_ui.TimeStretchSpinBox,
		SIGNAL(valueChanged(double)), SLOT(changed()));
	QObject::connect(m_ui.PitchShiftCheckBox,
		SIGNAL(toggled(bool)), SLOT(changed()));
	QObject::connect(m_ui.PitchShiftSpinBox,
		SIGNAL(valueChanged(double)), SLOT(changed()));
	QObject::connect(m_ui.ReverseCheckBox,
		SIGNAL(toggled(bool)), SLOT(changed()));

	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorAudioToolsForm::~qtractorAudioToolsForm (void)
{
}


// Select the initial visible tab.
void qtractorAudioToolsForm::setToolIndex ( int iToolIndex )
{
	// Set the proper tool page.
	m_ui.ToolTabWidget->setCurrentIndex(iToolIndex);

	switch (iToolIndex) {
	case qtractorAudioEditor::Gain:
		m_ui.GainCheckBox->setChecked(true);
		break;
	case qtractorAudioEditor::Panning:
		m_ui.PanningCheckBox->setChecked(true);
		break;
	case qtractorAudioEditor::Normalize:
		m_ui.NormalizeCheckBox->setChecked(true);
		break;
	case qtractorAudioEditor::TimeStretch:
		m_ui.TimeStretchCheckBox->setChecked(true);
		break;
	case qtractorAudioEditor::PitchShift:
		m_ui.PitchShiftCheckBox->setChecked(true);
		break;
	case qtractorAudioEditor::Reverse:
		m_ui.ReverseCheckBox->setChecked(true);
		break;
	default:
		break;
	}

	stabilizeForm();
}


// Retrieve the current tab index.
int qtractorAudioToolsForm::toolIndex (void) const
{
	return m_ui.ToolTabWidget->currentIndex();
}


//----------------------------------------------------------------------
// audioEditCommand() -- build the ApplyTool command.
//
// Called after the dialog is accepted.  Reads enabled-tool states,
// converts UI values to internal units, and constructs a single
// qtractorAudioEditCommand(ApplyTool).
//
// Returns nullptr when no tool is enabled.
//
qtractorAudioEditCommand *qtractorAudioToolsForm::audioEditCommand (
	qtractorAudioClip *pAudioClip,
	unsigned long iStart, unsigned long iLength )
{
	if (pAudioClip == nullptr)
		return nullptr;
	if (iLength < 1)
		return nullptr;

	// Collect tool parameters from UI.
	qtractorAudioEditCommand::ToolParams params;

	params.bGain = m_ui.GainCheckBox->isChecked();
	if (params.bGain)
		params.fGain = ::powf(10.0f,
			float(m_ui.GainSpinBox->value()) / 20.0f);

	params.bPanning = m_ui.PanningCheckBox->isChecked();
	if (params.bPanning)
		params.fPanning = 0.01f * float(m_ui.PanningSpinBox->value());

	params.bNormalize = m_ui.NormalizeCheckBox->isChecked();
	if (params.bNormalize)
		params.fNormalize = ::powf(10.0f,
			float(m_ui.NormalizeSpinBox->value()) / 20.0f);

	params.bTimeStretch = m_ui.TimeStretchCheckBox->isChecked();
	if (params.bTimeStretch)
		params.fTimeStretch = 0.01f * float(m_ui.TimeStretchSpinBox->value());

	params.bPitchShift = m_ui.PitchShiftCheckBox->isChecked();
	if (params.bPitchShift)
		params.fPitchShift = ::powf(2.0f,
			float(m_ui.PitchShiftSpinBox->value()) / 12.0f);

	params.bReverse = m_ui.ReverseCheckBox->isChecked();

	// Nothing enabled?
	if (!params.bGain        &&
		!params.bPanning     &&
		!params.bNormalize   &&
		!params.bTimeStretch &&
		!params.bPitchShift  &&
		!params.bReverse)
		return nullptr;

	// Build command name from enabled tools.
	QStringList tools;
	if (params.bGain)
		tools.append(tr("gain"));
	if (params.bPanning)
		tools.append(tr("panning"));
	if (params.bNormalize)
		tools.append(tr("normalize"));
	if (params.bTimeStretch)
		tools.append(tr("time-stretch"));
	if (params.bPitchShift)
		tools.append(tr("pitch-shift"));
	if (params.bReverse)
		tools.append(tr("reverse"));

	return new qtractorAudioEditCommand(
		pAudioClip, tools.join(", "), params, iStart, iLength);
}


//----------------------------------------------------------------------
// Preset management.
//

void qtractorAudioToolsForm::loadPreset ( const QString& sPreset )
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (!pOptions)
		return;

	QList<QVariant> vlist;
	QSettings& settings = pOptions->settings();

	// Get the preset entry...
	settings.beginGroup("/AudioTools");
	if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
		settings.beginGroup('/' + sPreset);

	// Gain tool...
	vlist = settings.value("/Gain").toList();
	if (vlist.count() > 1) {
	//	m_ui.GainCheckBox->setChecked(vlist[0].toBool());
		m_ui.GainSpinBox->setValue(vlist[1].toDouble());
	}

	// Panning tool...
	vlist = settings.value("/Panning").toList();
	if (vlist.count() > 1) {
	//	m_ui.PanningCheckBox->setChecked(vlist[0].toBool());
		m_ui.PanningSpinBox->setValue(vlist[1].toDouble());
	}

	// Normalize tool...
	vlist = settings.value("/Normalize").toList();
	if (vlist.count() > 1) {
	//	m_ui.NormalizeCheckBox->setChecked(vlist[0].toBool());
		m_ui.NormalizeSpinBox->setValue(vlist[1].toDouble());
	}

	// TimeStretch tool...
	vlist = settings.value("/TimeStretch").toList();
	if (vlist.count() > 1) {
	//	m_ui.TimeStretchCheckBox->setChecked(vlist[0].toBool());
		m_ui.TimeStretchSpinBox->setValue(vlist[1].toDouble());
	}

	// PitchShift tool...
	vlist = settings.value("/PitchShift").toList();
	if (vlist.count() > 1) {
	//	m_ui.PitchShiftCheckBox->setChecked(vlist[0].toBool());
		m_ui.PitchShiftSpinBox->setValue(vlist[1].toDouble());
	}

	// All loaded.
	if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
		settings.endGroup();
	settings.endGroup();
}


void qtractorAudioToolsForm::savePreset ( const QString& sPreset )
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	QList<QVariant> vlist;
	QSettings& settings = pOptions->settings();

	// Set preset entry...
	settings.beginGroup("/AudioTools");
	if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
		settings.beginGroup('/' + sPreset);

	// Gain tool...
	vlist.clear();
	vlist.append(m_ui.GainCheckBox->isChecked());
	vlist.append(m_ui.GainSpinBox->value());
	settings.setValue("/Gain", vlist);

	// Panning tool...
	vlist.clear();
	vlist.append(m_ui.PanningCheckBox->isChecked());
	vlist.append(m_ui.PanningSpinBox->value());
	settings.setValue("/Panning", vlist);

	// Normalize tool...
	vlist.clear();
	vlist.append(m_ui.NormalizeCheckBox->isChecked());
	vlist.append(m_ui.NormalizeSpinBox->value());
	settings.setValue("/Normalize", vlist);

	// TimeStretch tool...
	vlist.clear();
	vlist.append(m_ui.TimeStretchCheckBox->isChecked());
	vlist.append(m_ui.TimeStretchSpinBox->value());
	settings.setValue("/TimeStretch", vlist);

	// PitchShift tool...
	vlist.clear();
	vlist.append(m_ui.PitchShiftCheckBox->isChecked());
	vlist.append(m_ui.PitchShiftSpinBox->value());
	settings.setValue("/PitchShift", vlist);

	// Reverse tool...
	vlist.clear();
	vlist.append(m_ui.ReverseCheckBox->isChecked());
	settings.setValue("/Reverse", vlist);

	// All saved.
	if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
		settings.endGroup();
	settings.endGroup();
}


void qtractorAudioToolsForm::refreshPresets (void)
{
	++m_iUpdate;

	const QString sOldPreset = m_ui.PresetNameComboBox->currentText();
	m_ui.PresetNameComboBox->clear();

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup("/AudioTools");
		m_ui.PresetNameComboBox->insertItems(0, settings.childGroups());
		m_ui.PresetNameComboBox->model()->sort(0);
		settings.endGroup();
	}

	m_ui.PresetNameComboBox->addItem(g_sDefPreset);
	m_ui.PresetNameComboBox->setEditText(sOldPreset);

	m_iDirtyCount = 0;
	--m_iUpdate;

	stabilizeForm();
}


//----------------------------------------------------------------------
// Slots.
//

void qtractorAudioToolsForm::presetChanged ( const QString& sPreset )
{
	if (m_iUpdate > 0)
		return;

	if (!sPreset.isEmpty()
		&& m_ui.PresetNameComboBox->findText(sPreset) >= 0)
		++m_iDirtyCount;

	stabilizeForm();
}


void qtractorAudioToolsForm::presetActivated ( int iPreset )
{
	++m_iUpdate;

	loadPreset(m_ui.PresetNameComboBox->itemText(iPreset));

	m_iDirtyCount = 0;
	--m_iUpdate;

	stabilizeForm();
}


void qtractorAudioToolsForm::presetSave (void)
{
	savePreset(m_ui.PresetNameComboBox->currentText());
	refreshPresets();
}


void qtractorAudioToolsForm::presetDelete (void)
{
	if (m_iUpdate > 0)
		return;

	const QString& sPreset = m_ui.PresetNameComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		if (pOptions->bConfirmRemove) {
			if (QMessageBox::warning(this,
				tr("Warning"),
				tr("About to delete preset:\n\n"
				"\"%1\"\n\n"
				"Are you sure?")
				.arg(sPreset),
				QMessageBox::Ok | QMessageBox::Cancel)
				== QMessageBox::Cancel)
				return;
		}
		QSettings& settings = pOptions->settings();
		settings.beginGroup("/AudioTools");
		settings.remove(sPreset);
		settings.endGroup();
		refreshPresets();
	}
}


void qtractorAudioToolsForm::changed (void)
{
	if (m_iUpdate > 0)
		return;

	++m_iDirtyCount;
	stabilizeForm();
}


// Accept settings (OK button slot).
void qtractorAudioToolsForm::accept (void)
{
	// Save as default preset.
	savePreset(g_sDefPreset);

	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorAudioToolsForm::reject (void)
{
	QDialog::reject();
}


// Stabilize current form state.
void qtractorAudioToolsForm::stabilizeForm (void)
{
	// Preset bar state.
	const QString& sPreset = m_ui.PresetNameComboBox->currentText();
	const bool bExists = (m_ui.PresetNameComboBox->findText(sPreset) >= 0);
	const bool bNamed  = (!sPreset.isEmpty() && sPreset != g_sDefPreset);
	m_ui.PresetSaveToolButton->setEnabled(bNamed
		&& (!bExists || m_iDirtyCount > 0));
	m_ui.PresetDeleteToolButton->setEnabled(bNamed && bExists);

	// Enable/disable controls that depend on their checkbox.
	m_ui.GainLabel->setEnabled(m_ui.GainCheckBox->isChecked());
	m_ui.GainSpinBox->setEnabled(m_ui.GainCheckBox->isChecked());

	m_ui.PanningLabel->setEnabled(m_ui.PanningCheckBox->isChecked());
	m_ui.PanningSpinBox->setEnabled(m_ui.PanningCheckBox->isChecked());

	m_ui.NormalizeLabel->setEnabled(m_ui.NormalizeCheckBox->isChecked());
	m_ui.NormalizeSpinBox->setEnabled(m_ui.NormalizeCheckBox->isChecked());

	m_ui.TimeStretchLabel->setEnabled(m_ui.TimeStretchCheckBox->isChecked());
	m_ui.TimeStretchSpinBox->setEnabled(m_ui.TimeStretchCheckBox->isChecked());

	m_ui.PitchShiftLabel->setEnabled(m_ui.PitchShiftCheckBox->isChecked());
	m_ui.PitchShiftSpinBox->setEnabled(m_ui.PitchShiftCheckBox->isChecked());

	// Count enabled tools; disable OK if nothing is active.
	int iEnabled = 0;
	if (m_ui.GainCheckBox->isChecked()
		&& qAbs(m_ui.GainSpinBox->value()) > 0.001)
		++iEnabled;
	if (m_ui.PanningCheckBox->isChecked()
		&& qAbs(m_ui.PanningSpinBox->value()) > 0.001)
		++iEnabled;
	if (m_ui.NormalizeCheckBox->isChecked())
		++iEnabled;
	if (m_ui.TimeStretchCheckBox->isChecked()
		&& qAbs(m_ui.TimeStretchSpinBox->value() - 100.0) > 0.001)
		++iEnabled;
	if (m_ui.PitchShiftCheckBox->isChecked()
		&& qAbs(m_ui.PitchShiftSpinBox->value()) > 0.001)
		++iEnabled;
	if (m_ui.ReverseCheckBox->isChecked())
		++iEnabled;

	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(iEnabled > 0);
}


// end of qtractorAudioToolsForm.cpp
