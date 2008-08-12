// qtractorPluginForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorPluginForm.h"
#include "qtractorPluginCommand.h"

#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"

#include "qtractorSlider.h"

#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include <QMessageBox>
#include <QGridLayout>
#include <QValidator>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>

#include <QFileInfo>
#include <QFileDialog>

#include <math.h>

// This shall hold the default preset name.
static QString g_sDefPreset;


//----------------------------------------------------------------------------
// qtractorPluginForm -- UI wrapper form.

// Constructor.
qtractorPluginForm::qtractorPluginForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	const QFont& font = QWidget::font();
	QWidget::setFont(QFont(font.family(), font.pointSize() - 1));

	m_pPlugin       = NULL;
	m_pGridLayout   = NULL;
	m_iDirtyCount   = 0;
	m_iUpdate       = 0;

    m_ui.PresetComboBox->setValidator(
		new QRegExpValidator(QRegExp("[\\w-]+"), m_ui.PresetComboBox));
	m_ui.PresetComboBox->setInsertPolicy(QComboBox::NoInsert);

	// Have some effective feedback when toggling on/off...
	QIcon icons;
	icons.addPixmap(
		QPixmap(":/icons/itemLedOff.png"), QIcon::Active, QIcon::Off);
	icons.addPixmap(
		QPixmap(":/icons/itemLedOn.png"), QIcon::Active, QIcon::On);
	m_ui.ActivateToolButton->setIcon(icons);

	if (g_sDefPreset.isEmpty())
		g_sDefPreset = tr("(default)");

	// UI signal/slot connections...
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changePresetSlot(const QString&)));
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(loadPresetSlot(const QString&)));
	QObject::connect(m_ui.OpenPresetToolButton,
		SIGNAL(clicked()),
		SLOT(openPresetSlot()));
	QObject::connect(m_ui.SavePresetToolButton,
		SIGNAL(clicked()),
		SLOT(savePresetSlot()));
	QObject::connect(m_ui.DeletePresetToolButton,
		SIGNAL(clicked()),
		SLOT(deletePresetSlot()));
	QObject::connect(m_ui.EditToolButton,
		SIGNAL(toggled(bool)),
		SLOT(editSlot(bool)));
	QObject::connect(m_ui.ActivateToolButton,
		SIGNAL(toggled(bool)),
		SLOT(activateSlot(bool)));

	QObject::connect(this,
		SIGNAL(valueChanged(qtractorPluginParam *, float)),
		SLOT(valueChangeSlot(qtractorPluginParam *, float)));
}


// Destructor.
qtractorPluginForm::~qtractorPluginForm (void)
{
	clear();
}


// Plugin accessors.
void qtractorPluginForm::setPlugin ( qtractorPlugin *pPlugin )
{
	clear();

	// Just in those cases where
	// we've been around before...
	if (m_pGridLayout) {
		delete m_pGridLayout;
		m_pGridLayout = NULL;
	}

	// Set the new reference...
	m_pPlugin = pPlugin;

	if (m_pPlugin == NULL)
		return;

	// Plugin might not have its own editor...
	m_pGridLayout = new QGridLayout();
	m_pGridLayout->setMargin(4);
	m_pGridLayout->setSpacing(4);
#if QT_VERSION >= 0x040300
	m_pGridLayout->setHorizontalSpacing(16);
#endif
	const QList<qtractorPluginParam *>& params = m_pPlugin->params();
	int iRows = params.count();
	bool bEditor = (m_pPlugin->type())->isEditor();
	// FIXME: Couldn't stand more than a hundred widgets?
	// or do we have one dedicated editor GUI?
	if (!bEditor || iRows < 101) {
		int iCols = 1;
		while (iRows > 12 && iCols < 4)
			iRows = (params.count() / ++iCols);
		if (params.count() % iCols)	// Adjust to balance.
			iRows++;
		int iRow = 0;
		int iCol = 0;
		QListIterator<qtractorPluginParam *> iter(params);
		while (iter.hasNext()) {
			qtractorPluginParam *pParam = iter.next();
			qtractorPluginParamWidget *pParamWidget
				= new qtractorPluginParamWidget(pParam, this);
			m_paramWidgets.append(pParamWidget);
			m_pGridLayout->addWidget(pParamWidget, iRow, iCol);
			QObject::connect(pParamWidget,
				SIGNAL(valueChanged(qtractorPluginParam *, float)),
				SLOT(valueChangeSlot(qtractorPluginParam *, float)));
			if (++iRow >= iRows) {
				iRow = 0;
				iCol++;
			}
		}
	}
	layout()->addItem(m_pGridLayout);

	// Show editor button if available?
	m_ui.EditToolButton->setVisible(bEditor);
	if (bEditor)
		toggleEditor(m_pPlugin->isEditorVisible());

	// Set plugin name as title...
	updateCaption();
	adjustSize();

	updateActivated();
	refresh();
	stabilize();
	show();

	// Do we have a dedicated editor GUI?
	// if (bEditor)
	//	m_pPlugin->openEditor(this);
}


void qtractorPluginForm::activateForm (void)
{
	if (!isVisible())
		show();

	raise();
	activateWindow();
}


// Editor widget methods.
void qtractorPluginForm::toggleEditor ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	// Set the toggle button anyway...
	m_ui.EditToolButton->setChecked(bOn);

	m_iUpdate--;
}


// Plugin accessor.
qtractorPlugin *qtractorPluginForm::plugin (void) const
{
	return m_pPlugin;
}


// Plugin preset accessors.
void qtractorPluginForm::setPreset ( const QString& sPreset )
{
	if (!sPreset.isEmpty()) {
		m_iUpdate++;
		m_ui.PresetComboBox->setEditText(sPreset);
		m_iUpdate--;
	}
}

QString qtractorPluginForm::preset (void) const
{
	QString sPreset = m_ui.PresetComboBox->currentText();

	if (sPreset == g_sDefPreset || m_iDirtyCount > 0)
		sPreset.clear();

	return sPreset;
}


// Update form caption.
void qtractorPluginForm::updateCaption (void)
{
	if (m_pPlugin == NULL)
		return;

	QString sCaption = (m_pPlugin->type())->name();

	qtractorPluginList *pPluginList = m_pPlugin->list();
	if (pPluginList && !pPluginList->name().isEmpty())
		sCaption += " - " + pPluginList->name();

	setWindowTitle(sCaption);

	m_pPlugin->setEditorTitle(sCaption);
}


// Update activation state.
void qtractorPluginForm::updateActivated (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;
	m_ui.ActivateToolButton->setChecked(m_pPlugin->isActivated());
	m_iUpdate--;
}


// Update parameter value state.
void qtractorPluginForm::updateParamValue (
	unsigned long iIndex, float fValue )
{
	if (m_pPlugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginForm[%p]::updateParamValue(%lu)", this, iIndex);
#endif

	qtractorPluginParam *pParam = m_pPlugin->findParam(iIndex);
	if (pParam)
		emit valueChanged(pParam, fValue);
}


// Update port widget state.
void qtractorPluginForm::updateParamWidget ( unsigned long iIndex )
{
	if (m_pPlugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginForm[%p]::updateParamWidget(%lu)", this, iIndex);
#endif

	QListIterator<qtractorPluginParamWidget *> iter(m_paramWidgets);
	while (iter.hasNext()) {
		qtractorPluginParamWidget *pParamWidget = iter.next();
		if ((pParamWidget->param())->index() == iIndex) {
			pParamWidget->refresh();
			break;
		}
	}
}


// Preset management slots...
void qtractorPluginForm::changePresetSlot ( const QString& sPreset )
{
	if (m_iUpdate > 0)
		return;

	if (!sPreset.isEmpty() && m_ui.PresetComboBox->findText(sPreset) >= 0)
		m_iDirtyCount++;

	stabilize();
}


void qtractorPluginForm::loadPresetSlot ( const QString& sPreset )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0 || sPreset.isEmpty())
		return;

	// We'll need this, sure.
	qtractorOptions  *pOptions  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	if (sPreset == g_sDefPreset) {
		// Reset to default...
		pMainForm->commands()->exec(
			new qtractorResetPluginCommand(m_pPlugin));
	} else {
		// An existing preset is about to be loaded...
		QSettings& settings = pOptions->settings();
		// Should it be load from know file?...
		if ((m_pPlugin->type())->isConfigure()) {
			settings.beginGroup(m_pPlugin->presetGroup());
			m_pPlugin->loadPreset(settings.value(sPreset).toString());
			settings.endGroup();
			refresh();
		} else {
			//...or make it as usual (parameter list only)...
			settings.beginGroup(m_pPlugin->presetGroup());
			QStringList vlist = settings.value(sPreset).toStringList();
			settings.endGroup();
			if (!vlist.isEmpty()) {
				pMainForm->commands()->exec(
					new qtractorPresetPluginCommand(m_pPlugin, vlist));
			}
		}
	}

	stabilize();
}


void qtractorPluginForm::openPresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;
	if (!(m_pPlugin->type())->isConfigure())
		return;

	if (m_iUpdate > 0)
		return;

	// We'll need this, sure.
	qtractorOptions  *pOptions  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	// We'll assume that there's an external file...
	const QString sExt("qtx");
	// Prompt if file does not currently exist...
	QString sFilename = QFileDialog::getOpenFileName(this,
		tr("Open Preset") + " - " QTRACTOR_TITLE,   // Caption.
		pOptions->sPresetDir,                       // Start here.
		tr("Preset files (*.%1)").arg(sExt));       // Filter files.
	// Have we a filename to load a preset from?
	if (!sFilename.isEmpty()) {
		if (m_pPlugin->loadPreset(sFilename)) {
			// Got it loaded alright...
			QFileInfo fi(sFilename);
			setPreset(fi.baseName()
				.replace((m_pPlugin->type())->label() + '-', QString()));
			pOptions->sPresetDir = fi.absolutePath();
		} else {
			// Failure (maybe wrong plugin)...
			QMessageBox::critical(this,
				tr("Error") + " - " QTRACTOR_TITLE,
				tr("Preset could not be loaded\n"
				"from \"%1\".\n\n"
				"Sorry.").arg(sFilename),
				tr("Cancel"));
		}
	}
	refresh();

	stabilize();
}


void qtractorPluginForm::savePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset = m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// We'll need this, sure.
	qtractorOptions  *pOptions  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	// The current state preset is about to be saved...
	// this is where we'll make it...
	QSettings& settings = pOptions->settings();
	settings.beginGroup(m_pPlugin->presetGroup());
	// Which mode of preset?
	if ((m_pPlugin->type())->isConfigure()) {
		// Sure, we'll have something complex enough
		// to make it save into an external file...
		const QString sExt("qtx");
		QFileInfo fi(QDir(pOptions->sPresetDir),
			(m_pPlugin->type())->label() + '-' + sPreset + '.' + sExt);
		QString sFilename = fi.absoluteFilePath();
		// Prompt if file does not currently exist...
		if (!fi.exists()) {
			sFilename = QFileDialog::getSaveFileName(this,
				tr("Save Preset") + " - " QTRACTOR_TITLE,   // Caption.
				sFilename,                                  // Start here.
				tr("Preset files (*.%1)").arg(sExt));       // Filter files.
		}
		// We've a filename to save the preset
		if (!sFilename.isEmpty()) {
			if (QFileInfo(sFilename).suffix() != sExt)
				sFilename += '.' + sExt;
			if (m_pPlugin->savePreset(sFilename)) {
				settings.setValue(sPreset, sFilename);
				pOptions->sPresetDir = QFileInfo(sFilename).absolutePath();
			}
		}
	}	// Just leave it to simple parameter value list...
	else settings.setValue(sPreset, m_pPlugin->values());
	settings.endGroup();
	refresh();

	stabilize();
}


void qtractorPluginForm::deletePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset =  m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// We'll need this, sure.
	qtractorOptions  *pOptions  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	// A preset entry is about to be deleted;
	// prompt user if he/she's sure about this...
	if (pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to delete preset:\n\n"
			"\"%1\" (%2)\n\n"
			"Are you sure?")
			.arg(sPreset)
			.arg((m_pPlugin->type())->name()),
			tr("OK"), tr("Cancel")) > 0)
			return;
	}
	// Go ahead...
	QSettings& settings = pOptions->settings();
	settings.beginGroup(m_pPlugin->presetGroup());
	if ((m_pPlugin->type())->isConfigure()) {
		const QString& sFilename = settings.value(sPreset).toString();
		if (QFileInfo(sFilename).exists())
			QFile(sFilename).remove();
	}
	settings.remove(sPreset);
	settings.endGroup();
	refresh();

	stabilize();
}


// Editor slot.
void qtractorPluginForm::editSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	if (bOn)
		m_pPlugin->openEditor(this);
	else
		m_pPlugin->closeEditor();

	m_iUpdate--;
}


// Activation slot.
void qtractorPluginForm::activateSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	// Make it a undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorActivatePluginCommand(m_pPlugin, bOn));

	m_iUpdate--;
}


// Something has changed.
void qtractorPluginForm::valueChangeSlot (
	qtractorPluginParam *pParam, float fValue )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginForm[%p]::valueChangeSlot(%p, %g)", this, pParam, fValue);
#endif
	m_iUpdate++;

	// Make it a undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorPluginParamCommand(pParam, fValue));

	m_iUpdate--;

	m_iDirtyCount++;
	stabilize();
}


// Parameter-widget refreshner-loader.
void qtractorPluginForm::refresh (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginForm[%p]::refresh()", this);
#endif
	m_iUpdate++;

	const QString sOldPreset = m_ui.PresetComboBox->currentText();
	m_ui.PresetComboBox->clear();
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			pOptions->settings().beginGroup(m_pPlugin->presetGroup());
			m_ui.PresetComboBox->insertItems(0,
				pOptions->settings().childKeys());
			pOptions->settings().endGroup();
		}
	}
	m_ui.PresetComboBox->addItem(g_sDefPreset);
	m_ui.PresetComboBox->setEditText(sOldPreset);

	QListIterator<qtractorPluginParamWidget *> iter(m_paramWidgets);
	while (iter.hasNext())
		iter.next()->refresh();

	m_pPlugin->idleEditor();

	m_iDirtyCount = 0;
	m_iUpdate--;
}


// Preset control.
void qtractorPluginForm::stabilize (void)
{
	bool bExists  = false;
	bool bEnabled = (m_pPlugin != NULL);
	m_ui.ActivateToolButton->setEnabled(bEnabled);
	if (bEnabled) {
		bEnabled = (
			(m_pPlugin->type())->controlIns() > 0 ||
			(m_pPlugin->type())->isConfigure());
	}

	m_ui.PresetComboBox->setEnabled(bEnabled);
	m_ui.OpenPresetToolButton->setVisible(
		bEnabled && (m_pPlugin->type())->isConfigure());

	if (bEnabled) {
		const QString& sPreset = m_ui.PresetComboBox->currentText();
		bEnabled = (!sPreset.isEmpty() && sPreset != g_sDefPreset);
	    bExists  = (m_ui.PresetComboBox->findText(sPreset) >= 0);
	}

	m_ui.SavePresetToolButton->setEnabled(
		bEnabled && (!bExists || m_iDirtyCount > 0));
	m_ui.DeletePresetToolButton->setEnabled(
		bEnabled && bExists);
}


// Clear up plugin form...
void qtractorPluginForm::clear()
{
	if (m_pPlugin)
		m_pPlugin->closeEditor();

	qDeleteAll(m_paramWidgets);
	m_paramWidgets.clear();
}


//----------------------------------------------------------------------------
// qtractorPluginParamWidget -- Plugin port widget.
//

// Constructor.
qtractorPluginParamWidget::qtractorPluginParamWidget (
	qtractorPluginParam *pParam, QWidget *pParent )
	: QFrame(pParent), m_pParam(pParam), m_iUpdate(0)
{
	m_pGridLayout = new QGridLayout();
	m_pGridLayout->setMargin(0);
	m_pGridLayout->setSpacing(4);

	m_pLabel    = NULL;
	m_pSlider   = NULL;
	m_pSpinBox  = NULL;
	m_pCheckBox = NULL;
	m_pDisplay  = NULL;

	if (m_pParam->isToggled()) {
		m_pCheckBox = new QCheckBox(/*this*/);
		m_pCheckBox->setText(m_pParam->name());
	//	m_pCheckBox->setChecked(m_pParam->value() > 0.1f);
		m_pGridLayout->addWidget(m_pCheckBox, 0, 0);
		QObject::connect(m_pCheckBox,
			SIGNAL(toggled(bool)),
			SLOT(checkBoxToggled(bool)));
	} else if (m_pParam->isInteger()) {
		m_pGridLayout->setColumnMinimumWidth(0, 120);
		m_pLabel = new QLabel(/*this*/);
		m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_pLabel->setText(m_pParam->name() + ':');
		m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 2);
		m_pSpinBox = new QDoubleSpinBox(/*this*/);
		m_pSpinBox->setMaximumWidth(64);
		m_pSpinBox->setDecimals(0);
		m_pSpinBox->setMinimum(m_pParam->minValue());
		m_pSpinBox->setMaximum(m_pParam->maxValue());
		m_pSpinBox->setAlignment(Qt::AlignHCenter);
	//	m_pSpinBox->setValue(int(m_pParam->value()));
		m_pGridLayout->addWidget(m_pSpinBox, 0, 2);
		QObject::connect(m_pSpinBox,
			SIGNAL(valueChanged(const QString&)),
			SLOT(spinBoxValueChanged(const QString&)));
	} else {
		m_pLabel = new QLabel(/*this*/);
		if (m_pParam->isDisplay()) {
			m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		//	m_pLabel->setFixedWidth(72);
			m_pLabel->setMinimumWidth(64);
			m_pLabel->setText(m_pParam->name());
			m_pGridLayout->addWidget(m_pLabel, 0, 0);
		} else {
			m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
			m_pLabel->setText(m_pParam->name() + ':');
			m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 3);
		}
		m_pSlider = new qtractorSlider(Qt::Horizontal/*, this*/);
		m_pSlider->setTickPosition(QSlider::NoTicks);
		m_pSlider->setMinimumWidth(120);
		m_pSlider->setMinimum(0);
		m_pSlider->setMaximum(10000);
		m_pSlider->setPageStep(1000);
		m_pSlider->setSingleStep(100);
		m_pSlider->setDefault(paramToSlider(m_pParam->defaultValue()));
	//	m_pSlider->setValue(paramToSlider(m_pParam->value()));
		if (m_pParam->isDisplay()) {
			m_pGridLayout->addWidget(m_pSlider, 0, 1);
			m_pDisplay = new QLabel(/*this*/);
			m_pDisplay->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			m_pDisplay->setText(m_pParam->display());
		//	m_pDisplay->setFixedWidth(72);
			m_pDisplay->setMinimumWidth(64);
			m_pGridLayout->addWidget(m_pDisplay, 0, 2);
		} else {
			m_pGridLayout->addWidget(m_pSlider, 1, 0, 1, 2);
			int iDecs = paramDecs();
			m_pSpinBox = new QDoubleSpinBox(/*this*/);
			m_pSpinBox->setMaximumWidth(64);
			m_pSpinBox->setDecimals(iDecs);
			m_pSpinBox->setMinimum(m_pParam->minValue());
			m_pSpinBox->setMaximum(m_pParam->maxValue());
			m_pSpinBox->setSingleStep(::powf(10.0f, - float(iDecs)));
		#if QT_VERSION >= 0x040200
			m_pSpinBox->setAccelerated(true);
		#endif
		//	m_pSpinBox->setValue(m_pParam->value());
			m_pGridLayout->addWidget(m_pSpinBox, 1, 2);
			QObject::connect(m_pSpinBox,
				SIGNAL(valueChanged(const QString&)),
				SLOT(spinBoxValueChanged(const QString&)));
		}
		QObject::connect(m_pSlider,
			SIGNAL(valueChanged(int)),
			SLOT(sliderValueChanged(int)));
	}

	QFrame::setLayout(m_pGridLayout);
//	QFrame::setFrameShape(QFrame::StyledPanel);
//	QFrame::setFrameShadow(QFrame::Raised);
	QFrame::setToolTip(m_pParam->name());
}


// Refreshner-loader method.
void qtractorPluginParamWidget::refresh (void)
{
	if (m_iUpdate > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginParamWidget[%p]::refresh()", this);
#endif
	m_iUpdate++;

	float fValue = m_pParam->value();
	if (m_pParam->isToggled()) {
		m_pCheckBox->setChecked(fValue > 0.001f);
	} else if (m_pParam->isInteger()) {
		m_pSpinBox->setValue(int(fValue));
	} else {
		m_pSlider->setValue(paramToSlider(fValue));
		if (m_pSpinBox)
			m_pSpinBox->setValue(fValue);
		if (m_pDisplay)
			m_pDisplay->setText(m_pParam->display());
	}

	m_iUpdate--;
}


// Slider conversion methods.
int qtractorPluginParamWidget::paramToSlider ( float fValue ) const
{
	int iValue = 0;

	if (m_pParam->isLogarithmic() && m_pParam->minValue() > 1E-6f) {
		if (fValue > 1E-6f) {
			float fLogMaxValue = ::logf(m_pParam->maxValue());
			float fLogMinValue = ::logf(m_pParam->minValue());
			iValue = ::lroundf(10000.0f
				* (::logf(fValue) - fLogMinValue)
				/ (fLogMaxValue - fLogMinValue));
		}
	} else {
		float fScale = m_pParam->maxValue() - m_pParam->minValue();
		if (fScale > 1E-6f)
			iValue = int((10000.0f * (fValue - m_pParam->minValue())) / fScale);
	}

	return iValue;
}

float qtractorPluginParamWidget::sliderToParam ( int iValue ) const
{
	float fValue = 0.0f;

	if (m_pParam->isLogarithmic() && m_pParam->minValue() > 1E-6f) {
		float fRatio = m_pParam->maxValue() / m_pParam->minValue();
		fValue = m_pParam->minValue() * ::powf(fRatio, (float(iValue) / 10000.0f));
	} else {
		float fDelta = m_pParam->maxValue() - m_pParam->minValue();
		fValue = m_pParam->minValue() + (float(iValue) * fDelta) / 10000.0f;
	}

	return fValue;
}


// Spin-box decimals helper.
int qtractorPluginParamWidget::paramDecs (void) const
{
	int   iDecs = 0;
	float fDecs = ::log10f(m_pParam->maxValue() - m_pParam->minValue());
	if (fDecs < 0.0f)
		iDecs = 3;
	else if (fDecs < 1.0f)
		iDecs = 2;
	else if (fDecs < 3.0f)
		iDecs = 1;

	return iDecs;

}

// Change slots.
void qtractorPluginParamWidget::checkBoxToggled ( bool bOn )
{
	float fValue = (bOn ? 1.0f : 0.0f);

//	m_pParam->setValue(fValue);
	emit valueChanged(m_pParam, fValue);
}

void qtractorPluginParamWidget::spinBoxValueChanged ( const QString& sText )
{
	if (m_iUpdate > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginParamWidget[%p]::spinBoxValueChanged()", this);
#endif
	m_iUpdate++;

	float fValue = 0.0f;
	if (m_pParam->isInteger()) {
		fValue = float(sText.toInt());
	} else {
		fValue = sText.toFloat();
	}

	//	Don't let be no-changes...
	if (::fabsf(m_pParam->value() - fValue)
		> ::powf(10.0f, - float(paramDecs()))) {
		emit valueChanged(m_pParam, fValue);
		if (m_pSlider)
			m_pSlider->setValue(paramToSlider(fValue));
	}

	m_iUpdate--;
}

void qtractorPluginParamWidget::sliderValueChanged ( int iValue )
{
	if (m_iUpdate > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginParamWidget[%p]::sliderValueChanged()", this);
#endif
	m_iUpdate++;

	float fValue = sliderToParam(iValue);
	//	Don't let be no-changes...
	if (::fabsf(m_pParam->value() - fValue)
		> ::powf(10.0f, - float(paramDecs()))) {
		emit valueChanged(m_pParam, fValue);
		if (m_pSpinBox)
			m_pSpinBox->setValue(fValue);
		if (m_pDisplay)
			m_pDisplay->setText(m_pParam->display());
	}

	m_iUpdate--;
}


// end of qtractorPluginForm.cpp
