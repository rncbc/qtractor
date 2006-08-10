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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorPluginListView.h"

#include "qtractorOptions.h"
#include "qtractorMainForm.h"

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
		QObject::connect(pPortWidget, SIGNAL(valueChanged(float)),
			this, SLOT(changeSlot()));
		if (++iRow > iRows) {
			iRow = 0;
			iCol++;
		}
	}

	// Set plugin name as title...
	setCaption(m_pPlugin->name());
	adjustSize();

	updateActivated();
	refresh();
	stabilize();
}


qtractorPlugin *qtractorPluginForm::plugin (void)
{
	return m_pPlugin;
}


// Update activation state.
void qtractorPluginForm::updateActivated (void)
{
	if (m_iUpdate > 0)
		return;

	if (m_pPlugin == NULL)
		return;

	m_iUpdate++;
	ActivateToolButton->setOn(m_pPlugin->isActivated());
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
	if (m_iUpdate > 0 || sPreset.isEmpty())
		return;

	if (m_pPlugin == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorOptions *pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	if (sPreset == g_sDefPreset) {
		// Reset to default...
		m_pPlugin->reset();
		refresh();
	} else if (m_pPlugin->loadPreset(pOptions->settings(), sPreset)) {
		// An existing preset has just been loaded...
		refresh();
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
	const QString& sPreset = PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	if (m_pPlugin == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorOptions *pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	if (m_pPlugin->savePreset(pOptions->settings(), sPreset))
		refresh();

	stabilize();
}

void qtractorPluginForm::deletePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorOptions *pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	const QString& sPreset = PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	if (m_pPlugin->deletePreset(pOptions->settings(), sPreset))
		refresh();

	stabilize();
}


// Activation slot.
void qtractorPluginForm::activateSlot ( bool bOn )
{
	if (m_iUpdate > 0)
		return;

	if (m_pPlugin == NULL)
		return;

	m_iUpdate++;
	m_pPlugin->setActivated(bOn);
	m_iUpdate--;
}


// Something has changed.
void qtractorPluginForm::changeSlot (void)
{
	if (m_iUpdate > 0)
		return;

	m_iDirtyCount++;
	stabilize();
}


// Port-widget refreshner-loader.
void qtractorPluginForm::refresh (void)
{
	if (m_pPlugin == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorOptions *pOptions = pMainForm->options();
	if (pOptions == NULL)
		return;

	m_iUpdate++;

	const QString sOldPreset = PresetComboBox->currentText();
	PresetComboBox->clear();
	PresetComboBox->insertStringList(
		m_pPlugin->presetList(pOptions->settings()));
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
