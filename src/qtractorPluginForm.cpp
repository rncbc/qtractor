// qtractorPluginForm.cpp
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

#include "qtractorPluginForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorPluginListView.h"
#include "qtractorPluginCommand.h"

#include "qtractorMainForm.h"

#include <QMessageBox>
#include <QGridLayout>
#include <QValidator>
#include <QLineEdit>


// This shall hold the default preset name.
static QString g_sDefPreset;


//----------------------------------------------------------------------------
// qtractorPluginForm -- UI wrapper form.

// Constructor.
qtractorPluginForm::qtractorPluginForm (
	QWidget *pParent, Qt::WFlags wflags ) : QWidget(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pPlugin     = NULL;
	m_pGridLayout = NULL;
	m_iDirtyCount = 0;
	m_iUpdate     = 0;

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
		SIGNAL(textChanged(const QString &)),
		SLOT(changePresetSlot(const QString&)));
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(loadPresetSlot(const QString&)));
	QObject::connect(m_ui.SavePresetToolButton,
		SIGNAL(clicked()),
		SLOT(savePresetSlot()));
	QObject::connect(m_ui.DeletePresetToolButton,
		SIGNAL(clicked()),
		SLOT(deletePresetSlot()));
	QObject::connect(m_ui.ActivateToolButton,
		SIGNAL(toggled(bool)),
		SLOT(activateSlot(bool)));
}


// Destructor.
qtractorPluginForm::~qtractorPluginForm (void)
{
	qDeleteAll(m_portWidgets);
	m_portWidgets.clear();
}


// Plugin accessors.
void qtractorPluginForm::setPlugin ( qtractorPlugin *pPlugin )
{
	m_pPlugin = pPlugin;

	if (m_pPlugin == NULL)
		return;

	const QList<qtractorPluginPort *>& cports = m_pPlugin->cports();
	
	int iRows = cports.count();
	int iCols = 1;

	while (iRows > 12 && iCols < 3) {
		iRows >>= 1;
		iCols++;
	}

	if (m_pGridLayout)
		delete m_pGridLayout;

	qDeleteAll(m_portWidgets);
	m_portWidgets.clear();

	m_pGridLayout = new QGridLayout();

	int iRow = 0;
	int iCol = 0;
	
	QListIterator<qtractorPluginPort *> iter(cports);
	while (iter.hasNext()) {
		qtractorPluginPort *pPort = iter.next();
		qtractorPluginPortWidget *pPortWidget
			= new qtractorPluginPortWidget(pPort, this);
		m_portWidgets.append(pPortWidget);
		m_pGridLayout->addWidget(pPortWidget, iRow, iCol);
		QObject::connect(pPortWidget,
			SIGNAL(valueChanged(qtractorPluginPort *, float)),
			SLOT(valueChangeSlot(qtractorPluginPort *, float)));
		if (++iRow > iRows) {
			iRow = 0;
			iCol++;
		}
	}

	layout()->addItem(m_pGridLayout);

	// Set plugin name as title...
	updateCaption();
	adjustSize();

	updateActivated();
	refresh();
	stabilize();
}


qtractorPlugin *qtractorPluginForm::plugin (void)
{
	return m_pPlugin;
}

// Plugin preset accessors.
void qtractorPluginForm::setPreset ( const QString& sPreset )
{
	if (!sPreset.isEmpty()) {
		m_iUpdate++;
		m_ui.PresetComboBox->lineEdit()->setText(sPreset);
		m_iUpdate--;
	}
}

QString qtractorPluginForm::preset (void)
{
	QString sPreset = m_ui.PresetComboBox->currentText();

	if (sPreset == g_sDefPreset || m_iDirtyCount > 0)
		sPreset = QString::null;

	return sPreset;
}


// Update form caption.
void qtractorPluginForm::updateCaption (void)
{
	if (m_pPlugin == NULL)
		return;

	QString sCaption = m_pPlugin->name();

	qtractorPluginList *pPluginList = m_pPlugin->list();
	if (pPluginList && !pPluginList->name().isEmpty()) {
		sCaption += " - ";
		sCaption += pPluginList->name();
	}

	setWindowTitle(sCaption);
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


// Update port widget state.
void qtractorPluginForm::updatePort ( qtractorPluginPort *pPort )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	QListIterator<qtractorPluginPortWidget *> iter(m_portWidgets);
	while (iter.hasNext()) {
		qtractorPluginPortWidget *pPortWidget = iter.next();
		if (pPortWidget->port() == pPort) {
			pPortWidget->refresh();
			break;
		}
	}

	m_iUpdate--;
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

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	if (sPreset == g_sDefPreset) {
		// Reset to default...
		pMainForm->commands()->exec(
			new qtractorResetPluginCommand(pMainForm, m_pPlugin));
	} else {
		// An existing preset is about to be loaded...
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			QSettings& settings = pOptions->settings();
			// Get the preset entry, if any...
			settings.beginGroup(m_pPlugin->presetGroup());
			QStringList vlist = settings.value(sPreset).toStringList();
			settings.endGroup();
			// Is it there?
			if (!vlist.isEmpty()) {
				pMainForm->commands()->exec(
					new qtractorPresetPluginCommand(pMainForm,
						m_pPlugin, vlist));
			}
		}
	}

	stabilize();
}

void qtractorPluginForm::savePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset = m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// The current state preset is about to be saved...
	QStringList vlist = m_pPlugin->values();
	// Is there any?
	if (!vlist.isEmpty()) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorOptions *pOptions = pMainForm->options();
			if (pOptions) {
				QSettings& settings = pOptions->settings();
				// Set the preset entry...
				settings.beginGroup(m_pPlugin->presetGroup());
				settings.setValue(sPreset, vlist);
				settings.endGroup();
				refresh();
			}
		}
	}

	stabilize();
}


void qtractorPluginForm::deletePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset =  m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// A preset entry is about to be deleted...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			// Prompt user if he/she's sure about this...
			if (pOptions->bConfirmRemove) {
				if (QMessageBox::warning(this,
					tr("Warning") + " - " QTRACTOR_TITLE,
					tr("About to delete preset:\n\n"
					"%1 (%2)\n\n"
					"Are you sure?")
					.arg(sPreset)
					.arg(m_pPlugin->name()),
					tr("OK"), tr("Cancel")) > 0)
					return;
			}
			// Go ahead...
			QSettings& settings = pOptions->settings();
			settings.beginGroup(m_pPlugin->presetGroup());
			settings.remove(sPreset);
			settings.endGroup();
			refresh();
		}
	}

	stabilize();
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
			new qtractorActivatePluginCommand(pMainForm, m_pPlugin, bOn));

	m_iUpdate--;
}


// Something has changed.
void qtractorPluginForm::valueChangeSlot (
	qtractorPluginPort *pPort, float fValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	// Make it a undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorPluginPortCommand(pMainForm, pPort, fValue));

	m_iUpdate--;

	m_iDirtyCount++;
	stabilize();
}


// Port-widget refreshner-loader.
void qtractorPluginForm::refresh (void)
{
	if (m_pPlugin == NULL)
		return;

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
	m_ui.PresetComboBox->lineEdit()->setText(sOldPreset);

	QListIterator<qtractorPluginPortWidget *> iter(m_portWidgets);
	while (iter.hasNext())
		iter.next()->refresh();

	m_iDirtyCount = 0;
	m_iUpdate--;
}


// Preset control.
void qtractorPluginForm::stabilize (void)
{
	bool bExists  = false;
	bool bEnabled = (m_pPlugin != NULL);
	m_ui.ActivateToolButton->setEnabled(bEnabled);
	if (bEnabled)
		bEnabled = (m_pPlugin->cports().count() > 0);
	m_ui.PresetComboBox->setEnabled(bEnabled);

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


// end of qtractorPluginForm.cpp
