// qtractorPluginForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorOptions.h"
#include "qtractorPluginListView.h"
#include "qtractorPluginCommand.h"

#include "qtractorMainForm.h"

#include <qmessagebox.h>
#include <qvalidator.h>
#include <qlistbox.h>


// This shall hold the default preset name.
static QString g_sDefPreset;

// Kind of constructor.
void qtractorPluginForm::init (void)
{
	m_pPlugin     = NULL;
	m_pGridLayout = NULL;
	m_iDirtyCount = 0;
	m_iUpdate     = 0;

	m_portWidgets.setAutoDelete(true);

    PresetComboBox->setValidator(
		new QRegExpValidator(QRegExp("[\\w-]+"), PresetComboBox));
	PresetComboBox->setInsertionPolicy(QComboBox::NoInsertion);

	// Have some effective feedback when toggling on/off...
	QIconSet icons;
	icons.setPixmap(QPixmap::fromMimeSource("itemLedOff.png"),
		QIconSet::Automatic, QIconSet::Active, QIconSet::Off);
	icons.setPixmap(QPixmap::fromMimeSource("itemLedOn.png"),
		QIconSet::Automatic, QIconSet::Active, QIconSet::On);
	ActivateToolButton->setIconSet(icons);

	if (g_sDefPreset.isEmpty())
		g_sDefPreset = tr("(default)");
}


// Kind of destructor.
void qtractorPluginForm::destroy (void)
{
	m_portWidgets.clear();
}


// Plugin accessors.
void qtractorPluginForm::setPlugin ( qtractorPlugin *pPlugin )
{
	m_pPlugin = pPlugin;

	if (m_pPlugin == NULL)
		return;

	QPtrList<qtractorPluginPort>& cports = m_pPlugin->cports();
	
	int iRows = cports.count();
	int iCols = 1;

	while (iRows > 12 && iCols < 3) {
		iRows >>= 1;
		iCols++;
	}

	if (m_pGridLayout)
		delete m_pGridLayout;

	m_pGridLayout = new QGridLayout(layout(), iRows, iCols, 4);

	int iRow = 0;
	int iCol = 0;
	m_portWidgets.clear();
	for (qtractorPluginPort *pPort = cports.first();
			pPort; pPort = cports.next()) {
		qtractorPluginPortWidget *pPortWidget
			= new qtractorPluginPortWidget(pPort, this);
		m_portWidgets.append(pPortWidget);
		m_pGridLayout->addWidget(pPortWidget, iRow, iCol);
		QObject::connect(
			pPortWidget, SIGNAL(valueChanged(qtractorPluginPort *, float)),
			this, SLOT(valueChangeSlot(qtractorPluginPort *, float)));
		if (++iRow > iRows) {
			iRow = 0;
			iCol++;
		}
	}

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
		PresetComboBox->setCurrentText(sPreset);
		m_iUpdate--;
	}
}

QString qtractorPluginForm::preset (void)
{
	QString sPreset = PresetComboBox->currentText();

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

	setCaption(sCaption);
}


// Update activation state.
void qtractorPluginForm::updateActivated (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;
	ActivateToolButton->setOn(m_pPlugin->isActivated());
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

	for (qtractorPluginPortWidget *pPortWidget = m_portWidgets.first();
			pPortWidget; pPortWidget = m_portWidgets.next()) {
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

	if (!sPreset.isEmpty()
		&& PresetComboBox->listBox()->findItem(sPreset, Qt::ExactMatch) == NULL)
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
			QStringList vlist = settings.readListEntry(sPreset);
			settings.endGroup();
			// Is it there?
			if (!vlist.isEmpty()) {
				pMainForm->commands()->exec(
					new qtractorPresetPluginCommand(pMainForm,
						m_pPlugin, vlist));
			}
		}
	}
#if 0
	else {
		// In case this preset name get sneaked into, due to
		// QComboBox::insertionPolicy() != QComboBox::NoInsertion,
		// it should be removed from the drop-down list...
		QListBox *pListBox = PresetComboBox->listBox();
		QListBoxItem *pItem	= pListBox->findItem(sPreset, Qt::ExactMatch);
		if (pItem)
			pListBox->removeItem(pListBox->index(pItem));
	}
#endif

	stabilize();
}

void qtractorPluginForm::savePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset = PresetComboBox->currentText();
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
				bool bResult = settings.writeEntry(sPreset, vlist);
				settings.endGroup();
				if (bResult)
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

	const QString& sPreset = PresetComboBox->currentText();
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
			bool bResult = settings.removeEntry(sPreset);
			settings.endGroup();
			if (bResult)
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

	const QString sOldPreset = PresetComboBox->currentText();
	PresetComboBox->clear();
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			PresetComboBox->insertStringList(
				pOptions->settings().entryList(m_pPlugin->presetGroup()));
		}
	}
	PresetComboBox->insertItem(g_sDefPreset);
	PresetComboBox->setCurrentText(sOldPreset);

	for (qtractorPluginPortWidget *pPortWidget = m_portWidgets.first();
			pPortWidget; pPortWidget = m_portWidgets.next())
		pPortWidget->refresh();

	m_iDirtyCount = 0;
	m_iUpdate--;
}


// Preset control.
void qtractorPluginForm::stabilize (void)
{
	bool bExists  = false;
	bool bEnabled = (m_pPlugin != NULL);
	ActivateToolButton->setEnabled(bEnabled);
	if (bEnabled)
		bEnabled = (m_pPlugin->cports().count() > 0);
	PresetComboBox->setEnabled(bEnabled);

	if (bEnabled) {
		const QString& sPreset = PresetComboBox->currentText();
		bEnabled = (!sPreset.isEmpty() && sPreset != g_sDefPreset);
	    bExists  = (PresetComboBox->listBox()->findItem(
			sPreset, Qt::ExactMatch) != NULL);
	}

	SavePresetToolButton->setEnabled(
		bEnabled && (!bExists || m_iDirtyCount > 0));
	DeletePresetToolButton->setEnabled(
		bEnabled && bExists);
}


// end of qtractorPluginForm.ui.h
